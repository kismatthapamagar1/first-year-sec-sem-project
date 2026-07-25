#include "../include/staffdashboard.h"
#include "../ui/ui_staffdashboard.h"
#include "../include/productstaff.h"
#include "../include/profile.h"
#include "../include/mytasks.h"
#include "../include/stockservice.h"

#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QFile>
#include <QFont>
#include <QIcon>

// ═══════════════════════════════════════════════════════════════════
//  Luxury theme palette (Burgundy / Garnet)
//  Kept as named constants here so every hard-coded colour used from
//  C++ (avatar, stat cards, table rows) stays in lock-step with the
//  palette baked into staffdashboard.ui's stylesheet.
// ═══════════════════════════════════════════════════════════════════
namespace Theme {
constexpr const char *Burgundy      = "#660033"; // primary brand colour
constexpr const char *BurgundyDark  = "#4D0026"; // hover / pressed
constexpr const char *Garnet        = "#8B0000"; // secondary / danger accent
constexpr const char *Gold          = "#C9A227"; // luxury highlight accent
constexpr const char *Plum          = "#5B2C4D"; // 4th stat-card accent
constexpr const char *DangerBg      = "#FBEAE8";
constexpr const char *WarningText   = "#856404"; // kept semantic (amber = low stock)
constexpr const char *WarningBg     = "#FFF3CD";
}

StaffDashboard::StaffDashboard(int staffId, const QString &staffName, QWidget *parent)
    : ClassLogout(parent)
    , ui(new Ui::StaffDashboard)
    , m_staffId(staffId)
    , m_staffName(staffName)
{
    ui->setupUi(this);

    // Products button
    connect(ui->btnProducts,
            &QPushButton::clicked,
            this,
            &StaffDashboard::handleProductsClicked);

    // Logout button
    connect(ui->btnLogout,
            &QPushButton::clicked,
            this,
            &StaffDashboard::handleLogout_clicked);

    // Re-check stats whenever staff clicks back into Dashboard
    connect(ui->btnDashboard,
            &QPushButton::clicked,
            this,
            &StaffDashboard::refreshDashboardStats);

    // Profile avatar (top-right) -> opens Profile page
    connect(ui->btnProfile,
            &QPushButton::clicked,
            this,
            &StaffDashboard::handleProfileClicked);

    // Sidebar "View Tasks" -> opens MyTasks, filtered by this staff member's id
    connect(ui->btnViewTasks,
            &QPushButton::clicked,
            this,
            &StaffDashboard::handleViewTasksClicked);

    setupDashboardContent();
    loadCurrentUserInfo();
    refreshDashboardStats();
}

StaffDashboard::~StaffDashboard()
{
    delete ui;
}

// ─────────────────────────────────────────────────────────────────
//  Builds the stat cards + low-stock table inside contentFrame's
//  existing QVBoxLayout. Called once from the constructor.
// ─────────────────────────────────────────────────────────────────
void StaffDashboard::setupDashboardContent()
{
    QVBoxLayout *contentLayout =
        qobject_cast<QVBoxLayout *>(ui->contentFrame->layout());
    if (!contentLayout) {
        qWarning() << "StaffDashboard: contentFrame has no QVBoxLayout!";
        return;
    }

    // ── Row of stat cards ───────────────────────────────────────
    auto *cardsRow = new QGridLayout();
    cardsRow->setSpacing(16);

    auto makeCard = [this](const QString &title, QLabel *&valueLabelOut,
                           const QString &accentColor) -> QFrame* {
        auto *card = new QFrame;
        card->setStyleSheet(QString(
                                "QFrame {"
                                "  background-color: white;"
                                "  border-radius: 8px;"
                                "  border-left: 4px solid %1;"
                                "}").arg(accentColor));
        card->setMinimumHeight(90);

        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 12, 16, 12);

        auto *titleLbl = new QLabel(title);
        titleLbl->setStyleSheet("color: #8A7E82; font-size: 12px; border: none;");

        valueLabelOut = new QLabel("0");
        valueLabelOut->setStyleSheet(
            QString("color: %1; font-size: 24px; font-weight: bold; border: none;")
                .arg(Theme::Burgundy));

        cardLayout->addWidget(titleLbl);
        cardLayout->addWidget(valueLabelOut);
        return card;
    };

    QFrame *cardTotal   = makeCard("Total Products",  lblTotalProducts,   Theme::Burgundy);
    QFrame *cardLow     = makeCard("Low Stock",        lblLowStockCount,   Theme::Gold);
    QFrame *cardOut     = makeCard("Out of Stock",     lblOutOfStockCount, Theme::Garnet);
    QFrame *cardTasks   = makeCard("Pending Tasks",    lblPendingTasks,    Theme::Plum);

    cardsRow->addWidget(cardTotal, 0, 0);
    cardsRow->addWidget(cardLow,   0, 1);
    cardsRow->addWidget(cardOut,   0, 2);
    cardsRow->addWidget(cardTasks, 0, 3);

    contentLayout->addLayout(cardsRow);

    // ── Low stock table heading ─────────────────────────────────
    auto *lowStockHeading = new QLabel("Low Stock Items");
    lowStockHeading->setStyleSheet(
        QString("color: %1; font-size: 14px; font-weight: bold; margin-top: 10px;")
            .arg(Theme::Burgundy));
    contentLayout->addWidget(lowStockHeading);

    // ── Low stock table ──────────────────────────────────────────
    tblLowStock = new QTableWidget;
    tblLowStock->setColumnCount(5);
    tblLowStock->setHorizontalHeaderLabels(
        {"Product", "Category", "Stock", "Unit", "Status"});
    tblLowStock->verticalHeader()->setVisible(false);
    tblLowStock->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tblLowStock->setSelectionBehavior(QAbstractItemView::SelectRows);
    tblLowStock->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tblLowStock->setColumnWidth(1, 120);
    tblLowStock->setColumnWidth(2, 70);
    tblLowStock->setColumnWidth(3, 70);
    tblLowStock->setColumnWidth(4, 110);
    tblLowStock->setStyleSheet(
        "QTableWidget { background-color: white; border-radius: 6px; "
        "  gridline-color: #EEE3E7; selection-background-color: #F3E6EC; "
        "  selection-color: #660033; }"
        "QHeaderView::section { background-color: #660033; color: white; "
        "  font-weight: bold; padding: 6px 8px; border: none; }");

    contentLayout->addWidget(tblLowStock);
    contentLayout->addStretch();
}

// ─────────────────────────────────────────────────────────────────
//  Pulls this staff member's username/first_name/last_name/picture
//  from 'information' BY ID (m_staffId), and applies them to the
//  topbar (welcome text + circular avatar button). Falls back to an
//  initials avatar if no photo file can be found. m_username is
//  cached here purely so handleProfileClicked() has something to
//  hand to Profile — Profile itself still looks everything else up
//  by username internally.
// ─────────────────────────────────────────────────────────────────
void StaffDashboard::loadCurrentUserInfo()
{
    if (m_staffId < 0) {
        qWarning() << "StaffDashboard: no staffId supplied, cannot personalize dashboard";
        ui->welcomeLabel->setText(m_staffName.isEmpty()
                                      ? QStringLiteral("Welcome back, Staff!")
                                      : QStringLiteral("Welcome back, %1!").arg(m_staffName));
        return;
    }

    QSqlQuery q;
    q.prepare("SELECT first_name, last_name, username, picture "
              "FROM information WHERE id = :id");
    q.bindValue(":id", m_staffId);

    QString firstName;
    QString lastName;
    QString picture;

    if (q.exec() && q.next()) {
        firstName  = q.value(0).toString();
        lastName   = q.value(1).toString();
        m_username = q.value(2).toString();
        picture    = q.value(3).toString();

        // Keep m_staffName authoritative for anything shown after this
        // (welcome text below, and MyTasks' header the next time View
        // Tasks is clicked) — DB values are fresher than whatever Login
        // captured at sign-in time.
        const QString freshFullName = QStringLiteral("%1 %2").arg(firstName, lastName).trimmed();
        if (!freshFullName.isEmpty())
            m_staffName = freshFullName;
    } else {
        qWarning() << "StaffDashboard: could not load user info for staffId"
                   << m_staffId << "-" << q.lastError().text();
    }

    ui->welcomeLabel->setText(
        firstName.isEmpty() ? QStringLiteral("Welcome back, %1!").arg(
                                  m_staffName.isEmpty() ? QStringLiteral("Staff") : m_staffName)
                            : QStringLiteral("Welcome back, %1!").arg(firstName));

    // ── Avatar ───────────────────────────────────────────────────
    QPixmap avatar;
    bool loaded = false;

    if (!picture.isEmpty()) {
        // Must match Profile/FrontProfile/Register's copyPictureToStorage()
        // save location exactly -- this used to read from
        // applicationDirPath()-relative resources/staff_photos/, which is
        // wherever the .exe happens to run from and doesn't match the
        // fixed save path below, so newly saved/approved photos never
        // showed up in the topbar icon even though the file was on disk.
        const QString diskPath =
            QStringLiteral("C:/Users/ASUS/Desktop/Sajilo-Bazarr/resources/staff_photos/") + picture;

        if (QFile::exists(diskPath))
            loaded = avatar.load(diskPath);

        if (!loaded)
            loaded = avatar.load(":/staff_photos/" + picture); // Qt resource fallback
    }

    if (!loaded) {
        // No photo available -> draw a simple initials avatar instead.
        QString initials;
        if (!firstName.isEmpty()) initials += firstName.at(0).toUpper();
        if (!lastName.isEmpty())  initials += lastName.at(0).toUpper();
        if (initials.isEmpty())   initials = "?";

        avatar = QPixmap(AVATAR_DIAMETER, AVATAR_DIAMETER);
        avatar.fill(Qt::transparent);

        QPainter p(&avatar);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(Theme::Burgundy));
        p.drawEllipse(0, 0, AVATAR_DIAMETER, AVATAR_DIAMETER);

        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(AVATAR_DIAMETER / 2);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(avatar.rect(), Qt::AlignCenter, initials);
    }

    ui->btnProfile->setIcon(QIcon(makeCircularPixmap(avatar, AVATAR_DIAMETER)));
    ui->btnProfile->setIconSize(QSize(AVATAR_DIAMETER, AVATAR_DIAMETER));
}

QPixmap StaffDashboard::makeCircularPixmap(const QPixmap &source, int diameter)
{
    QPixmap scaled = source.scaled(diameter, diameter,
                                   Qt::KeepAspectRatioByExpanding,
                                   Qt::SmoothTransformation);

    QPixmap circular(diameter, diameter);
    circular.fill(Qt::transparent);

    QPainter painter(&circular);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath clipPath;
    clipPath.addEllipse(0, 0, diameter, diameter);
    painter.setClipPath(clipPath);

    const int x = (diameter - scaled.width())  / 2;
    const int y = (diameter - scaled.height()) / 2;
    painter.drawPixmap(x, y, scaled);

    return circular;
}

// ─────────────────────────────────────────────────────────────────
//  Pulls fresh numbers from the DB and repopulates the dashboard.
//  Call this any time stock might have changed (e.g. after coming
//  back from the Update Stock page).
// ─────────────────────────────────────────────────────────────────
void StaffDashboard::refreshDashboardStats()
{
    // Stock stats — computed the same way everywhere via StockService,
    // which now excludes soft-deleted (Recycle Bin) products, matching
    // ProductBase::countProducts() used by the Products page.
    InventoryStats stats = StockService::computeStats();
    lblTotalProducts->setText(QString::number(stats.total));
    lblLowStockCount->setText(QString::number(stats.low));
    lblOutOfStockCount->setText(QString::number(stats.out));
    // If staff dashboard ever adds a High Stock card, use stats.high here too

    // Pending tasks — filtered by staff_id, same rule MyTasks uses, so
    // this card always matches what View Tasks shows.
    {
        QSqlQuery q;
        q.prepare("SELECT COUNT(*) FROM tasks WHERE staff_id = :id AND status = 'Pending'");
        q.bindValue(":id", m_staffId);
        if (q.exec() && q.next())
            lblPendingTasks->setText(QString::number(q.value(0).toInt()));
        else
            lblPendingTasks->setText("0");
    }

    // Populate low-stock table
    tblLowStock->setRowCount(0);

    QSqlQuery q;
    q.prepare(
        "SELECT product_name, category, stock, unit, status "
        "FROM products "
        "WHERE (is_deleted IS NULL OR is_deleted = 0) AND stock > 0 AND stock <= :t "
        "ORDER BY stock ASC");
    q.bindValue(":t", LOW_STOCK_THRESHOLD);

    if (!q.exec()) {
        qWarning() << "StaffDashboard: low stock query failed -"
                   << q.lastError().text();
        return;
    }

    int row = 0;
    while (q.next()) {
        tblLowStock->insertRow(row);

        auto *itemName = new QTableWidgetItem(q.value(0).toString());
        auto *itemCat  = new QTableWidgetItem(q.value(1).toString());

        int stock = q.value(2).toInt();
        auto *itemStock = new QTableWidgetItem(QString::number(stock));
        itemStock->setTextAlignment(Qt::AlignCenter);
        if (stock <= 0) {
            itemStock->setForeground(QColor(Theme::Garnet));
        } else {
            itemStock->setForeground(QColor("#B8860B")); // warm amber-gold, low (not out) stock
        }

        auto *itemUnit = new QTableWidgetItem(q.value(3).toString());
        itemUnit->setTextAlignment(Qt::AlignCenter);

        // Status is now derived live from stock (same rule ProductBase
        // uses via StockService::statusForStock), instead of trusting
        // the DB's possibly-stale 'status' column — this is what was
        // producing "Cup: -100 stock / High Stock" before.
        const QString liveStatus = StockService::statusForStock(stock);
        auto *itemStatus = new QTableWidgetItem(liveStatus);
        itemStatus->setTextAlignment(Qt::AlignCenter);
        if (stock <= 0) {
            itemStatus->setForeground(QColor(Theme::Garnet));
            itemStatus->setBackground(QColor(Theme::DangerBg));
        } else {
            itemStatus->setForeground(QColor(Theme::WarningText));
            itemStatus->setBackground(QColor(Theme::WarningBg));
        }

        tblLowStock->setItem(row, 0, itemName);
        tblLowStock->setItem(row, 1, itemCat);
        tblLowStock->setItem(row, 2, itemStock);
        tblLowStock->setItem(row, 3, itemUnit);
        tblLowStock->setItem(row, 4, itemStatus);
        ++row;
    }
}

// ─────────────────────────────────────────────────────────────────
//  Opens the Products page.
// ─────────────────────────────────────────────────────────────────
void StaffDashboard::handleProductsClicked()
{
    this->hide();

    ProductStaff *productPage = new ProductStaff(); // no parent -> real top-level window
    productPage->setAttribute(Qt::WA_DeleteOnClose);

    connect(productPage, &QObject::destroyed, this, [this]() {
        refreshDashboardStats(); // stock may have changed while we were away
        this->show();
    });

    productPage->show();
}

void StaffDashboard::handleProfileClicked()
{
    Profile *profileWindow = new Profile(m_username, this);
    profileWindow->setAttribute(Qt::WA_DeleteOnClose);
    profileWindow->exec();
}

// ─────────────────────────────────────────────────────────────────
//  Opens the My Tasks page.
// ─────────────────────────────────────────────────────────────────
void StaffDashboard::handleViewTasksClicked()
{
    this->hide();

    MyTasks *tasksPage = new MyTasks(m_staffId, m_staffName); // no parent -> real top-level window
    tasksPage->setAttribute(Qt::WA_DeleteOnClose);

    connect(tasksPage, &QObject::destroyed, this, [this]() {
        refreshDashboardStats(); // pending task count may have changed while we were away
        this->show();
    });

    tasksPage->showMaximized();
}