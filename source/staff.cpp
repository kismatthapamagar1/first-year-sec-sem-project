#include "../include/staff.h"
#include "../ui/ui_staff.h"
#include "../include/taskmanagement.h" // TODO: confirm this path/filename matches your project layout
#include "../include/staffperform.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QCryptographicHash>
#include <QColor>

namespace {

// Passwords are stored as MD5 hex digests (consistent with the seed data in main.cpp).
// Swap for a stronger scheme if the login layer is ever upgraded.
QString hashPassword(const QString &plain)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(plain.toUtf8(), QCryptographicHash::Md5).toHex());
}

// Pretty-print the raw role value from the DB into a display label.
QString roleLabel(const QString &role)
{
    if (role.compare(QLatin1String("frontdesk"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("Front Desk");
    if (role.compare(QLatin1String("staff"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("Staff");
    // Fallback: capitalise first letter
    QString r = role;
    if (!r.isEmpty())
        r[0] = r[0].toUpper();
    return r;
}

// Pretty-print the raw status value from the DB into a display label.
// Valid raw values: 'enabled', 'disabled', 'expired'.
QString statusLabel(const QString &status)
{
    if (status.compare(QLatin1String("disabled"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("Disabled");
    if (status.compare(QLatin1String("expired"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("Expired");
    return QStringLiteral("Enabled");
}

} // namespace

// ─── Column indices ────────────────────────────────────────────────────────
// 0: ID  1: NAME  2: EMAIL  3: PHONE NO  4: STATUS  5: USERNAME  6: ROLE  7: ACTIONS
static constexpr int COL_ID       = 0;
static constexpr int COL_NAME     = 1;
static constexpr int COL_EMAIL    = 2;
static constexpr int COL_PHONE    = 3;
static constexpr int COL_STATUS   = 4;
static constexpr int COL_USERNAME = 5;
static constexpr int COL_ROLE     = 6;
static constexpr int COL_ACTIONS  = 7;
static constexpr int COL_COUNT    = 8;

staff::staff(QWidget *parent)
    : BackBase<QWidget>(parent)
    , ui(new Ui::staff)
{
    ui->setupUi(this);
    setWindowTitle("Sajilo-Bazar Staff Management");


    // Back to Dashboard — inherited from BackBase<QWidget>. Closing this
    // window is enough: AdminDashboard is already open underneath it
    // (see admindashboard.cpp's handleStaff_clicked()).
    wireBackButton(ui->btnBackToDashboard);

    // ── Table setup (mirrors inventory.cpp's approach) ───────────────────
    const QStringList headers = {
        QStringLiteral("ID"),
        QStringLiteral("NAME"),
        QStringLiteral("EMAIL"),
        QStringLiteral("PHONE NO"),
        QStringLiteral("STATUS"),
        QStringLiteral("USERNAME"),
        QStringLiteral("ROLE"),
        QStringLiteral("ACTIONS")
    };
    ui->staffTable_3->setColumnCount(COL_COUNT);
    ui->staffTable_3->setHorizontalHeaderLabels(headers);
    ui->staffTable_3->verticalHeader()->setVisible(false);

    // ── Column sizing ──────────────────────────────────────────────────────
    // A single global QHeaderView::Stretch (the old approach) spreads width
    // evenly across all 8 columns, so the narrow ID column ends up as wide
    // as NAME/EMAIL while ACTIONS (which needs to fit its buttons) gets
    // squeezed. Give each column a resize mode that matches what it
    // actually needs to hold, so ACTIONS has enough room to stay readable.
    auto *header = ui->staffTable_3->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionResizeMode(COL_ID,       QHeaderView::Fixed);
    header->setSectionResizeMode(COL_NAME,     QHeaderView::Stretch);
    header->setSectionResizeMode(COL_EMAIL,    QHeaderView::Stretch);
    header->setSectionResizeMode(COL_PHONE,    QHeaderView::Interactive);
    header->setSectionResizeMode(COL_STATUS,   QHeaderView::Fixed);
    header->setSectionResizeMode(COL_USERNAME, QHeaderView::Interactive);
    header->setSectionResizeMode(COL_ROLE,     QHeaderView::Fixed);
    header->setSectionResizeMode(COL_ACTIONS,  QHeaderView::Fixed);

    header->resizeSection(COL_ID,       48);
    header->resizeSection(COL_PHONE,    110);
    header->resizeSection(COL_STATUS,   90);
    header->resizeSection(COL_USERNAME, 130);
    header->resizeSection(COL_ROLE,     90);
    // Only 3 buttons now (Edit / Disable-Enable / Expire-Unexpire) since
    // Task moved to the top nav bar, so ACTIONS needs less width than before.
    header->resizeSection(COL_ACTIONS,  220);
    loadStaffTable();
}

staff::~staff()
{
    delete ui;
}

void staff::loadStaffTable()
{
    QSqlDatabase db = QSqlDatabase::database(); // default connection opened in main()
    if (!db.isOpen()) {
        QMessageBox::critical(this, tr("Database Error"), tr("No open database connection."));
        return;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, first_name, last_name, email, phone, status, username, role "
        "FROM information "
        "WHERE role = 'staff' OR role = 'frontdesk' "
        "ORDER BY id ASC"));

    if (!query.exec()) {
        QMessageBox::critical(this, tr("Database Error"),
                              tr("Failed to load staff: %1").arg(query.lastError().text()));
        return;
    }

    ui->staffTable_3->setRowCount(0);

    int row = 0;
    while (query.next()) {
        const int     id       = query.value(0).toInt();
        const QString name     = query.value(1).toString()
                             + QLatin1Char(' ')
                             + query.value(2).toString();
        const QString email    = query.value(3).toString();
        const QString phone    = query.value(4).toString();
        const QString status   = query.value(5).toString(); // 'enabled' | 'disabled' | 'expired'
        const QString username = query.value(6).toString();
        const QString role     = query.value(7).toString();

        const bool disabled = (status == QLatin1String("disabled"));
        const bool expired  = (status == QLatin1String("expired"));

        ui->staffTable_3->insertRow(row);

        // Helper: create a non-editable cell.
        // NOTE: QTableWidgetItem is not a QWidget, so it has no stylesheet
        // support — Qt only exposes per-item foreground color via the API
        // below (setForeground). This one piece can't be moved into the
        // .ui file; every other visual (button colors, hovers, fonts,
        // borders, padding) lives entirely in staff.ui.
        auto setCell = [&](int col, const QString &text) {
            auto *item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            if (disabled)
                item->setForeground(QColor(QStringLiteral("#94a3b8"))); // muted grey
            else if (expired)
                item->setForeground(QColor(QStringLiteral("#dc2626"))); // muted red
            ui->staffTable_3->setItem(row, col, item);
        };

        setCell(COL_ID,       QString::number(id));
        setCell(COL_NAME,     name.trimmed());
        setCell(COL_EMAIL,    email);
        setCell(COL_PHONE,    phone);
        setCell(COL_STATUS,   statusLabel(status));
        setCell(COL_USERNAME, username);
        setCell(COL_ROLE,     roleLabel(role));

        ui->staffTable_3->setCellWidget(row, COL_ACTIONS, buildActionsWidget(id, status));

        ++row;
    }

    ui->staffTable_3->resizeRowsToContents();
}

// ─── Actions cell ────────────────────────────────────────────────────────────
// Task button removed entirely — TaskManagement is now opened from the
// top-nav "Tasks" button (see on_navTasksBtn_3_clicked), not per staff row.
// Expire now toggles to "Unexpire" once a row is expired, instead of
// permanently disabling the button.

QWidget *staff::buildActionsWidget(int staffId, const QString &status)
{
    const bool disabled = (status == QLatin1String("disabled"));
    const bool expired  = (status == QLatin1String("expired"));

    auto *container = new QWidget(ui->staffTable_3);
    auto *layout    = new QHBoxLayout(container);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);

    // Every button below only sets WHAT it is via the "actionRole" dynamic
    // property. staff.ui's stylesheet matches on that property (e.g.
    // QPushButton[actionRole="edit"]) to apply colors, radius, padding,
    // font size, and hover states. No colors or fonts are set here.

    // Edit
    auto *editBtn = new QPushButton(tr("Edit"), container);
    editBtn->setProperty("actionRole", QStringLiteral("edit"));
    editBtn->setCursor(Qt::PointingHandCursor);
    connect(editBtn, &QPushButton::clicked, this, [this, staffId]() { editStaff(staffId); });

    // Disable / Enable — toggles based on current status.
    auto *disableBtn = new QPushButton(disabled ? tr("Enable") : tr("Disable"), container);
    disableBtn->setProperty("actionRole", disabled ? QStringLiteral("enable")
                                                   : QStringLiteral("disable"));
    disableBtn->setCursor(Qt::PointingHandCursor);
    connect(disableBtn, &QPushButton::clicked, this, [this, staffId]() { disableStaff(staffId); });

    // Expire / Unexpire — toggles based on current status. No longer gets
    // permanently disabled once expired; instead flips to "Unexpire".
    auto *expireBtn = new QPushButton(expired ? tr("Unexpire") : tr("Expire"), container);
    expireBtn->setProperty("actionRole", expired ? QStringLiteral("unexpire")
                                                 : QStringLiteral("expire"));
    expireBtn->setCursor(Qt::PointingHandCursor);
    connect(expireBtn, &QPushButton::clicked, this, [this, staffId]() { expireStaff(staffId); });

    layout->addWidget(editBtn);
    layout->addWidget(disableBtn);
    layout->addWidget(expireBtn);
    layout->addStretch();

    container->setLayout(layout);
    return container;
}



// ─── Edit Staff ────────────────────────────────────────────────────────────

void staff::editStaff(int staffId)
{
    QSqlDatabase db = QSqlDatabase::database();

    QSqlQuery fetch(db);
    fetch.prepare(QStringLiteral(
        "SELECT first_name, last_name, email, phone, username, role "
        "FROM information WHERE id = :id"));
    fetch.bindValue(QStringLiteral(":id"), staffId);

    if (!fetch.exec() || !fetch.next()) {
        QMessageBox::critical(this, tr("Database Error"),
                              tr("Failed to load staff record: %1").arg(fetch.lastError().text()));
        return;
    }

    const QString currentRole = fetch.value(5).toString();

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Edit Staff"));

    auto *form = new QFormLayout(&dialog);

    auto *firstNameEdit = new QLineEdit(fetch.value(0).toString(), &dialog);
    auto *lastNameEdit  = new QLineEdit(fetch.value(1).toString(), &dialog);
    auto *emailEdit     = new QLineEdit(fetch.value(2).toString(), &dialog);
    auto *phoneEdit     = new QLineEdit(fetch.value(3).toString(), &dialog);
    auto *usernameEdit  = new QLineEdit(fetch.value(4).toString(), &dialog);
    auto *passwordEdit  = new QLineEdit(&dialog);
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setPlaceholderText(tr("Leave blank to keep current password"));

    auto *roleCombo = new QComboBox(&dialog);
    roleCombo->addItem(tr("Staff"),      QStringLiteral("staff"));
    roleCombo->addItem(tr("Front Desk"), QStringLiteral("frontdesk"));
    // Pre-select the current role
    const int roleIdx = roleCombo->findData(currentRole);
    if (roleIdx >= 0)
        roleCombo->setCurrentIndex(roleIdx);

    form->addRow(tr("First Name:"),   firstNameEdit);
    form->addRow(tr("Last Name:"),    lastNameEdit);
    form->addRow(tr("Email:"),        emailEdit);
    form->addRow(tr("Phone:"),        phoneEdit);
    form->addRow(tr("Username:"),     usernameEdit);
    form->addRow(tr("New Password:"), passwordEdit);
    form->addRow(tr("Role:"),         roleCombo);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString firstName = firstNameEdit->text().trimmed();
    const QString lastName  = lastNameEdit->text().trimmed();
    const QString email     = emailEdit->text().trimmed();
    const QString phone     = phoneEdit->text().trimmed();
    const QString username  = usernameEdit->text().trimmed();
    const QString password  = passwordEdit->text();
    const QString role      = roleCombo->currentData().toString();

    if (firstName.isEmpty() || username.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Information"),
                             tr("First name and username are required."));
        return;
    }

    QSqlQuery update(db);
    if (password.isEmpty()) {
        update.prepare(QStringLiteral(
            "UPDATE information "
            "SET first_name = :first_name, last_name = :last_name, "
            "    email = :email, phone = :phone, username = :username, role = :role "
            "WHERE id = :id"));
    } else {
        update.prepare(QStringLiteral(
            "UPDATE information "
            "SET first_name = :first_name, last_name = :last_name, "
            "    email = :email, phone = :phone, username = :username, role = :role, "
            "    password = :password "
            "WHERE id = :id"));
        update.bindValue(QStringLiteral(":password"), hashPassword(password));
    }
    update.bindValue(QStringLiteral(":first_name"), firstName);
    update.bindValue(QStringLiteral(":last_name"),  lastName);
    update.bindValue(QStringLiteral(":email"),      email);
    update.bindValue(QStringLiteral(":phone"),      phone);
    update.bindValue(QStringLiteral(":username"),   username);
    update.bindValue(QStringLiteral(":role"),       role);
    update.bindValue(QStringLiteral(":id"),         staffId);

    if (!update.exec()) {
        QMessageBox::critical(this, tr("Database Error"),
                              tr("Failed to update staff: %1").arg(update.lastError().text()));
        return;
    }

    loadStaffTable();
}

// ─── Disable / Enable Staff ────────────────────────────────────────────────

void staff::disableStaff(int staffId)
{
    QSqlDatabase db = QSqlDatabase::database();

    QSqlQuery fetch(db);
    fetch.prepare(QStringLiteral("SELECT status FROM information WHERE id = :id"));
    fetch.bindValue(QStringLiteral(":id"), staffId);

    if (!fetch.exec() || !fetch.next()) {
        QMessageBox::critical(this, tr("Database Error"),
                              tr("Failed to load staff status: %1").arg(fetch.lastError().text()));
        return;
    }

    const bool    currentlyDisabled = (fetch.value(0).toString() == QLatin1String("disabled"));
    const QString newStatus = currentlyDisabled
                                  ? QStringLiteral("enabled")
                                  : QStringLiteral("disabled");

    const auto reply = QMessageBox::question(
        this,
        currentlyDisabled ? tr("Confirm Enable") : tr("Confirm Disable"),
        currentlyDisabled
            ? tr("Re-enable this staff member's account?")
            : tr("Disable this staff member's account? They will not be treated as active staff."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    QSqlQuery update(db);
    update.prepare(QStringLiteral("UPDATE information SET status = :status WHERE id = :id"));
    update.bindValue(QStringLiteral(":status"), newStatus);
    update.bindValue(QStringLiteral(":id"),     staffId);

    if (!update.exec()) {
        QMessageBox::critical(this, tr("Database Error"),
                              tr("Failed to update status: %1").arg(update.lastError().text()));
        return;
    }

    loadStaffTable();
}

// ─── Expire / Unexpire Staff ────────────────────────────────────────────────
// Toggles between 'expired' and 'enabled'. Previously Expire permanently
// disabled itself once a row was expired, with no way back short of editing
// the DB directly. Now the ACTIONS cell just flips to "Unexpire".

void staff::expireStaff(int staffId)
{
    QSqlDatabase db = QSqlDatabase::database();

    QSqlQuery fetch(db);
    fetch.prepare(QStringLiteral("SELECT status FROM information WHERE id = :id"));
    fetch.bindValue(QStringLiteral(":id"), staffId);

    if (!fetch.exec() || !fetch.next()) {
        QMessageBox::critical(this, tr("Database Error"),
                              tr("Failed to load staff status: %1").arg(fetch.lastError().text()));
        return;
    }

    const bool currentlyExpired = (fetch.value(0).toString() == QLatin1String("expired"));
    const QString newStatus = currentlyExpired
                                  ? QStringLiteral("enabled")
                                  : QStringLiteral("expired");

    const auto reply = QMessageBox::question(
        this,
        currentlyExpired ? tr("Confirm Unexpire") : tr("Confirm Expire"),
        currentlyExpired
            ? tr("Restore this staff member's account to active (enabled)?")
            : tr("Mark this staff member's account as expired? This is typically used when their "
                 "contract or engagement period has ended."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    QSqlQuery update(db);
    update.prepare(QStringLiteral("UPDATE information SET status = :status WHERE id = :id"));
    update.bindValue(QStringLiteral(":status"), newStatus);
    update.bindValue(QStringLiteral(":id"),     staffId);

    if (!update.exec()) {
        QMessageBox::critical(this, tr("Database Error"),
                              tr("Failed to update status: %1").arg(update.lastError().text()));
        return;
    }

    loadStaffTable();
}

// ─── Tasks (top nav) ─────────────────────────────────────────────────────────
// Moved out of the per-row ACTIONS cell entirely. Now a page-level entry
// point living alongside Staff / Category / Supplier in the top nav bar.

void staff::on_navTasksBtn_3_clicked()
{
    // TODO: confirm taskmanagement's real constructor. This assumes it looks
    // like staff's own constructor: explicit taskmanagement(QWidget *parent = nullptr).
    auto *taskWin = new TaskManagement(); // no parent -> opens as its own top-level window
    taskWin->setAttribute(Qt::WA_DeleteOnClose); // auto-cleans up when the user closes it
    taskWin->show();
}

// ─── Staff Performance (top nav) ────────────────────────────────────────────
// "navSupplierBtn_3" is a leftover object name from before this button was
// re-labelled "Staff Performance" in staff.ui — kept as-is here since
// renaming it would mean regenerating ui_staff.h too.

void staff::on_navSupplierBtn_3_clicked()
{
    auto *perfWin = new StaffPerform(); // no parent -> opens as its own top-level window
    perfWin->setAttribute(Qt::WA_DeleteOnClose);
    connect(perfWin, &StaffPerform::backToDashboardRequested, perfWin, &StaffPerform::close);
    perfWin->show();
}