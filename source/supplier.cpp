#include "../include/supplier.h"
#include "../ui/ui_supplier.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QHeaderView>

supplier::supplier(QWidget *parent)
    : BackBase<QWidget>(parent)
    , ui(new Ui::supplier)
{
    ui->setupUi(this);
    setWindowTitle("Sajilo-Bazar Supplier Management");

    // Back to Dashboard — inherited from BackBase<QWidget>. Closing this
    // window is enough: AdminDashboard is already open underneath it
    // (see admindashboard.cpp's handleSuppliers_clicked()).
    wireBackButton(ui->btnBackToDashboard);

    // Connect toolbar buttons
    connect(ui->addSupplierBtn, &QPushButton::clicked,
            this, &supplier::on_addSupplierBtn_clicked);

    // Live search — filter rows as the user types
    connect(ui->searchEdit, &QLineEdit::textChanged,
            this, &supplier::onSearchTextChanged);

    loadSupplierTable();
}

supplier::~supplier()
{
    delete ui;
}

// ─────────────────────────────────────────────────────────────────────────────
//  loadSupplierTable
//  Clears and repopulates supplierTable from the `suppliers` table.
// ─────────────────────────────────────────────────────────────────────────────
void supplier::loadSupplierTable()
{
    QSqlDatabase db = QSqlDatabase::database(); // default connection opened in main()
    if (!db.isOpen()) {
        QMessageBox::critical(this, tr("Database Error"),
                              tr("No open database connection."));
        return;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, supplier_name, email, phone, product, supplied_categories "
        "FROM suppliers ORDER BY id ASC"));

    if (!query.exec()) {
        QMessageBox::critical(this, tr("Database Error"),
                              tr("Failed to load suppliers: %1")
                                  .arg(query.lastError().text()));
        return;
    }

    ui->supplierTable->setRowCount(0);

    int row = 0;
    while (query.next()) {
        const int     id         = query.value(0).toInt();
        const QString name       = query.value(1).toString();
        const QString email      = query.value(2).toString();
        const QString phone      = query.value(3).toString();
        const QString product    = query.value(4).toString();
        const QString category   = query.value(5).toString();

        ui->supplierTable->insertRow(row);

        auto *idItem = new QTableWidgetItem(QString::number(id));
        idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
        ui->supplierTable->setItem(row, 0, idItem);

        auto setCell = [&](int col, const QString &text) {
            auto *item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui->supplierTable->setItem(row, col, item);
        };

        setCell(1, name);
        setCell(2, email);
        setCell(3, phone);
        setCell(4, product);
        setCell(5, category);

        ui->supplierTable->setCellWidget(row, 6, buildActionsWidget(id));

        ++row;
    }

    ui->supplierTable->resizeRowsToContents();

    // Re-apply any active search filter after reload
    onSearchTextChanged(ui->searchEdit->text());
}

// ─────────────────────────────────────────────────────────────────────────────
//  buildActionsWidget
//  Returns a small widget with Edit and Delete buttons for a table row.
// ─────────────────────────────────────────────────────────────────────────────
QWidget *supplier::buildActionsWidget(int supplierId)
{
    auto *container = new QWidget(ui->supplierTable);
    auto *layout    = new QHBoxLayout(container);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    const QString baseStyle = QStringLiteral(
        "QPushButton {"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 4px 10px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "  color: white;"
        "}");

    auto *editBtn = new QPushButton(tr("Edit"), container);
    editBtn->setStyleSheet(baseStyle + QStringLiteral(
                               "QPushButton { background-color: #2D4A7A; }"
                               "QPushButton:hover { background-color: #1B2A4A; }"));
    connect(editBtn, &QPushButton::clicked,
            this, [this, supplierId]() { editSupplier(supplierId); });

    auto *deleteBtn = new QPushButton(tr("Delete"), container);
    deleteBtn->setStyleSheet(baseStyle + QStringLiteral(
                                 "QPushButton { background-color: #DC2626; }"
                                 "QPushButton:hover { background-color: #991B1B; }"));
    connect(deleteBtn, &QPushButton::clicked,
            this, [this, supplierId]() { deleteSupplier(supplierId); });

    layout->addWidget(editBtn);
    layout->addWidget(deleteBtn);
    layout->addStretch();

    container->setLayout(layout);
    return container;
}

// ─────────────────────────────────────────────────────────────────────────────
//  onSearchTextChanged
//  Hides rows that do not contain the search text in any visible text column.
// ─────────────────────────────────────────────────────────────────────────────
void supplier::onSearchTextChanged(const QString &text)
{
    const QString needle = text.trimmed().toLower();

    for (int row = 0; row < ui->supplierTable->rowCount(); ++row) {
        bool match = needle.isEmpty();

        if (!match) {
            // Search across all text columns (0–5)
            for (int col = 0; col <= 5 && !match; ++col) {
                const QTableWidgetItem *item = ui->supplierTable->item(row, col);
                if (item && item->text().toLower().contains(needle))
                    match = true;
            }
        }

        ui->supplierTable->setRowHidden(row, !match);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  on_addSupplierBtn_clicked
//  Opens a dialog to collect supplier details, then inserts a new row.
// ─────────────────────────────────────────────────────────────────────────────
void supplier::on_addSupplierBtn_clicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add Supplier"));

    auto *form = new QFormLayout(&dialog);

    auto *nameEdit     = new QLineEdit(&dialog);
    auto *emailEdit    = new QLineEdit(&dialog);
    auto *phoneEdit    = new QLineEdit(&dialog);
    auto *productEdit  = new QLineEdit(&dialog);
    auto *categoryEdit = new QLineEdit(&dialog);

    form->addRow(tr("Supplier Name:"),     nameEdit);
    form->addRow(tr("Email:"),             emailEdit);
    form->addRow(tr("Phone:"),             phoneEdit);
    form->addRow(tr("Supplied Product:"),  productEdit);
    form->addRow(tr("Supplied Category:"), categoryEdit);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString name     = nameEdit->text().trimmed();
    const QString email    = emailEdit->text().trimmed();
    const QString phone    = phoneEdit->text().trimmed();
    const QString product  = productEdit->text().trimmed();
    const QString category = categoryEdit->text().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Information"),
                             tr("Supplier name is required."));
        return;
    }

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO suppliers (supplier_name, email, phone, product, supplied_categories) "
        "VALUES (:name, :email, :phone, :product, :category)"));
    query.bindValue(QStringLiteral(":name"),     name);
    query.bindValue(QStringLiteral(":email"),    email);
    query.bindValue(QStringLiteral(":phone"),    phone);
    query.bindValue(QStringLiteral(":product"),  product);
    query.bindValue(QStringLiteral(":category"), category);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("Database Error"),
                              tr("Failed to add supplier: %1")
                                  .arg(query.lastError().text()));
        return;
    }

    loadSupplierTable();
}

// ─────────────────────────────────────────────────────────────────────────────
//  editSupplier
//  Fetches the current values, opens a pre-filled dialog, and updates the row.
// ─────────────────────────────────────────────────────────────────────────────
void supplier::editSupplier(int supplierId)
{
    QSqlDatabase db = QSqlDatabase::database();

    QSqlQuery fetch(db);
    fetch.prepare(QStringLiteral(
        "SELECT supplier_name, email, phone, product, supplied_categories "
        "FROM suppliers WHERE id = :id"));
    fetch.bindValue(QStringLiteral(":id"), supplierId);

    if (!fetch.exec() || !fetch.next()) {
        QMessageBox::critical(this, tr("Database Error"),
                              tr("Failed to load supplier record: %1")
                                  .arg(fetch.lastError().text()));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Edit Supplier"));

    auto *form = new QFormLayout(&dialog);

    auto *nameEdit     = new QLineEdit(fetch.value(0).toString(), &dialog);
    auto *emailEdit    = new QLineEdit(fetch.value(1).toString(), &dialog);
    auto *phoneEdit    = new QLineEdit(fetch.value(2).toString(), &dialog);
    auto *productEdit  = new QLineEdit(fetch.value(3).toString(), &dialog);
    auto *categoryEdit = new QLineEdit(fetch.value(4).toString(), &dialog);

    form->addRow(tr("Supplier Name:"),     nameEdit);
    form->addRow(tr("Email:"),             emailEdit);
    form->addRow(tr("Phone:"),             phoneEdit);
    form->addRow(tr("Supplied Product:"),  productEdit);
    form->addRow(tr("Supplied Category:"), categoryEdit);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString name     = nameEdit->text().trimmed();
    const QString email    = emailEdit->text().trimmed();
    const QString phone    = phoneEdit->text().trimmed();
    const QString product  = productEdit->text().trimmed();
    const QString category = categoryEdit->text().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Information"),
                             tr("Supplier name is required."));
        return;
    }

    QSqlQuery update(db);
    update.prepare(QStringLiteral(
        "UPDATE suppliers SET supplier_name = :name, email = :email, phone = :phone, "
        "product = :product, supplied_categories = :category WHERE id = :id"));
    update.bindValue(QStringLiteral(":name"),     name);
    update.bindValue(QStringLiteral(":email"),    email);
    update.bindValue(QStringLiteral(":phone"),    phone);
    update.bindValue(QStringLiteral(":product"),  product);
    update.bindValue(QStringLiteral(":category"), category);
    update.bindValue(QStringLiteral(":id"),       supplierId);

    if (!update.exec()) {
        QMessageBox::critical(this, tr("Database Error"),
                              tr("Failed to update supplier: %1")
                                  .arg(update.lastError().text()));
        return;
    }

    loadSupplierTable();
}

// ─────────────────────────────────────────────────────────────────────────────
//  deleteSupplier
//  Asks for confirmation, then hard-deletes the row from the database.
// ─────────────────────────────────────────────────────────────────────────────
void supplier::deleteSupplier(int supplierId)
{
    const auto reply = QMessageBox::question(
        this, tr("Confirm Delete"),
        tr("Are you sure you want to delete this supplier? This cannot be undone."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM suppliers WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), supplierId);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("Database Error"),
                              tr("Failed to delete supplier: %1")
                                  .arg(query.lastError().text()));
        return;
    }

    loadSupplierTable();
}