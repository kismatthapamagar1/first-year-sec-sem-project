#include "../include/billing.h"
#include "../ui/ui_billing.h"
#include "../include/frontdesk.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QHeaderView>
#include <QTextDocument>
#include <QPdfWriter>
#include <QStandardPaths>
#include <QDateTime>
#include <QCompleter>
#include <QSet>

Billing::Billing(int staffId, const QString &staffName, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Billing)
    , m_staffId(staffId)
    , m_staffName(staffName)
{
    ui->setupUi(this);

    // billTable cosmetics
    ui->billTable->horizontalHeader()->setStretchLastSection(false);
    ui->billTable->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    ui->billTable->setColumnWidth(ColSku, 80);
    ui->billTable->setColumnWidth(ColUnit, 70);
    ui->billTable->setColumnWidth(ColUnitPrice, 100);
    ui->billTable->setColumnWidth(ColStock, 80);
    ui->billTable->setColumnWidth(ColQty, 100);
    ui->billTable->setColumnWidth(ColPrice, 110);
    ui->billTable->setColumnWidth(ColRemove, 100);
    ui->billTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Every column above is a fixed width. On a maximized/full-screen window
    // that leaves a big empty gap after the last column instead of the table
    // filling the available space. Let the Product column (the one column
    // whose content length varies the most) stretch to soak up whatever room
    // is left, while the rest stay pinned at their fixed widths.
    ui->billTable->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    ui->billTable->horizontalHeader()->setSectionResizeMode(ColSku, QHeaderView::Fixed);
    ui->billTable->horizontalHeader()->setSectionResizeMode(ColUnit, QHeaderView::Fixed);
    ui->billTable->horizontalHeader()->setSectionResizeMode(ColUnitPrice, QHeaderView::Fixed);
    ui->billTable->horizontalHeader()->setSectionResizeMode(ColStock, QHeaderView::Fixed);
    ui->billTable->horizontalHeader()->setSectionResizeMode(ColQty, QHeaderView::Fixed);
    ui->billTable->horizontalHeader()->setSectionResizeMode(ColPrice, QHeaderView::Fixed);
    ui->billTable->horizontalHeader()->setSectionResizeMode(ColRemove, QHeaderView::Fixed);

    // Barcode scanning: works identically whether the input comes from a
    // real USB scanner OR a phone running a "keyboard wedge" scanner app,
    // because both just type digits + Enter into whichever field has focus.
    connect(ui->barcodeInput, &QLineEdit::returnPressed, this, &Billing::onBarcodeScanned);
    connect(ui->btnGenerateBill, &QPushButton::clicked, this, &Billing::generateBill);
    connect(ui->btnBackToDashboard, &QPushButton::clicked, this, &Billing::backToDashboard);

    // Let the same field double as a product-name search box: as the
    // cashier types, matching product names pop up so they don't need to
    // know the SKU/barcode by heart. Picking a suggestion (or typing the
    // full name and pressing Enter) resolves to that product just like a
    // scanned barcode would (see addProductToBill()).
    setupNameCompleter();

    ui->barcodeInput->setPlaceholderText(tr("Scan barcode, or type SKU / product name..."));
    ui->barcodeInput->setFocus();
}

void Billing::setupNameCompleter()
{
    QStringList names;
    QSqlQuery q("SELECT product_name FROM products "
                "WHERE (is_deleted IS NULL OR is_deleted = 0) "
                "  AND product_name IS NOT NULL AND TRIM(product_name) <> '' "
                "ORDER BY product_name ASC");
    while (q.next())
        names << q.value(0).toString();

    auto *completer = new QCompleter(names, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);   // matches anywhere in the name, not just the start
    completer->setCompletionMode(QCompleter::PopupCompletion);
    ui->barcodeInput->setCompleter(completer);
}

Billing::~Billing()
{
    delete ui;
}

// ─────────────────────────────────────────────────────────────────────────
//  Navigation
// ─────────────────────────────────────────────────────────────────────────
void Billing::backToDashboard()
{
    // FIX: frontdesk::openBillingWindow() calls this->close() on the
    // dashboard, and since frontdesk is created with Qt::WA_DeleteOnClose,
    // that dashboard object is genuinely destroyed, not just hidden — so
    // there's nothing left to "return to." This used to call the default
    // frontdesk() constructor, which has no staffId/staffName at all,
    // producing a blank dashboard that looked like nobody was logged in.
    // Now Billing carries the same staffId/staffName it was constructed
    // with (see frontdesk::openBillingWindow()) and passes them straight
    // through, so the dashboard that reappears belongs to the same user.
    frontdesk *dashboard = new frontdesk(m_staffId, m_staffName);
    dashboard->setAttribute(Qt::WA_DeleteOnClose);
    dashboard->show();

    this->close();
}

// ─────────────────────────────────────────────────────────────────────────
//  Barcode handling
// ─────────────────────────────────────────────────────────────────────────
void Billing::onBarcodeScanned()
{
    const QString code = ui->barcodeInput->text().trimmed();
    ui->barcodeInput->clear();
    if (code.isEmpty())
        return;

    addProductToBill(code);
    ui->barcodeInput->setFocus();
}

void Billing::addProductToBill(const QString &code)
{
    // If this product is already on the bill, just bump its quantity by 1
    // instead of adding a duplicate row (handles re-scanning the same
    // barcode, or re-searching/re-selecting the same product by name).
    for (int r = 0; r < ui->billTable->rowCount(); ++r) {
        const bool matchesSku  = ui->billTable->item(r, ColSku)->text().compare(code, Qt::CaseInsensitive) == 0;
        const bool matchesName = ui->billTable->item(r, ColName)->text().compare(code, Qt::CaseInsensitive) == 0;
        if (matchesSku || matchesName) {
            auto *spin = qobject_cast<QDoubleSpinBox *>(ui->billTable->cellWidget(r, ColQty));
            if (spin)
                spin->setValue(spin->value() + 1);
            return;
        }
    }

    QSqlQuery query;
    query.prepare("SELECT id, product_name, unit, price, stock, sku FROM products "
                  "WHERE (sku = :code OR id = :code OR product_name = :code COLLATE NOCASE) "
                  "  AND (is_deleted IS NULL OR is_deleted = 0) LIMIT 1");
    query.bindValue(":code", code);

    if (!query.exec() || !query.next()) {
        QMessageBox::warning(this, tr("Product Not Found"),
                             tr("No product matches code:\n%1").arg(code));
        return;
    }

    const QString  name    = query.value("product_name").toString();
    const QString  unit    = query.value("unit").toString();
    const double   price   = query.value("price").toDouble();
    const int      stock   = query.value("stock").toInt();
    const QString  sku     = query.value("sku").toString();

    if (stock <= 0) {
        QMessageBox::warning(this, tr("Out of Stock"),
                             tr("%1 is currently out of stock.").arg(name));
        return;
    }

    const int row = ui->billTable->rowCount();
    ui->billTable->insertRow(row);
    ui->billTable->setRowHeight(row, 34);

    auto *skuItem = new QTableWidgetItem(sku);
    skuItem->setData(Qt::UserRole, query.value("id").toInt());  // stash product_id
    ui->billTable->setItem(row, ColSku, skuItem);
    ui->billTable->setItem(row, ColName, new QTableWidgetItem(name));
    ui->billTable->setItem(row, ColUnit, new QTableWidgetItem(unit));


    auto *priceItem = new QTableWidgetItem(QString::number(price, 'f', 2));
    priceItem->setData(Qt::UserRole, price); // stash raw unit price for calculations
    ui->billTable->setItem(row, ColUnitPrice, priceItem);

    auto *stockItem = new QTableWidgetItem(QString::number(stock));
    stockItem->setData(Qt::UserRole, stock);
    ui->billTable->setItem(row, ColStock, stockItem);

    // Quantity spin box: only units that are genuinely sold in fractional
    // amounts (weight/volume/length — "kg", "litre", etc.) get decimals,
    // so 2.5 kg is possible. Discrete/countable units (pieces, dozen,
    // box, bottle...) are locked to whole numbers — you can't sell half
    // a bottle or a third of a dozen.
    static const QSet<QString> fractionalUnits = {"kg", "g", "litre", "ml", "meter"};
    const bool allowFraction = fractionalUnits.contains(unit.trimmed().toLower());

    auto *qtySpin = new QDoubleSpinBox;
    qtySpin->setStyleSheet(

        "QDoubleSpinBox {"
        "   background-color: #ffffff; color: #4a1626;"
        "   border: 1px solid #660033; border-radius: 3px;"
        "   padding-right: 18px;"
        "}"
        "QDoubleSpinBox::up-button {"
        "   subcontrol-origin: border; subcontrol-position: top right;"
        "   width: 18px; height: 11px;"
        "   border-left: 1px solid #660033; border-bottom: 1px solid #660033;"
        "   border-top-right-radius: 3px;"
        "   background-color: #660033;"
        "}"
        "QDoubleSpinBox::down-button {"
        "   subcontrol-origin: border; subcontrol-position: bottom right;"
        "   width: 18px; height: 11px;"
        "   border-left: 1px solid #660033;"
        "   border-bottom-right-radius: 3px;"
        "   background-color: #660033;"
        "}"
        "QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover {"
        "   background-color: #8a1a4d;"
        "}"
        "QDoubleSpinBox::up-button:pressed, QDoubleSpinBox::down-button:pressed {"
        "   background-color: #4a1626;"
        "}"
        "QDoubleSpinBox::up-arrow {"
        "   width: 0; height: 0;"
        "   border-left: 4px solid transparent;"
        "   border-right: 4px solid transparent;"
        "   border-bottom: 5px solid white;"
        "}"
        "QDoubleSpinBox::down-arrow {"
        "   width: 0; height: 0;"
        "   border-left: 4px solid transparent;"
        "   border-right: 4px solid transparent;"
        "   border-top: 5px solid white;"
        "}"
        );
        "QDoubleSpinBox { background-color: #ffffff; color: #4a1626; "
        "border: 1px solid #660033; border-radius: 3px; }");
    qtySpin->setMinimumHeight(28);

    if (allowFraction) {
        qtySpin->setDecimals(2);
        qtySpin->setSingleStep(0.10);
        qtySpin->setMinimum(0.01);
        qtySpin->setValue(1.0);
    } else {
        qtySpin->setDecimals(0);
        qtySpin->setSingleStep(1);
        qtySpin->setMinimum(1);
        qtySpin->setValue(1);
    }
    qtySpin->setMaximum(stock);
    ui->billTable->setCellWidget(row, ColQty, qtySpin);
    connect(qtySpin, &QDoubleSpinBox::valueChanged, this, [this, qtySpin]() {
        for (int r = 0; r < ui->billTable->rowCount(); ++r) {
            if (ui->billTable->cellWidget(r, ColQty) == qtySpin) {
                recalcRowPrice(r);
                break;
            }
        }
    });

    ui->billTable->setItem(row, ColPrice, new QTableWidgetItem(QString::number(price, 'f', 2)));

    auto *removeBtn = new QPushButton(tr("Remove"));
    removeBtn->setStyleSheet("QPushButton { background-color: #c0392b; color: white; border: none; "
                             "border-radius: 3px; padding: 4px; } QPushButton:hover { background-color: #e74c3c; }");
    ui->billTable->setCellWidget(row, ColRemove, removeBtn);
    connect(removeBtn, &QPushButton::clicked, this, [this, removeBtn]() {
        for (int r = 0; r < ui->billTable->rowCount(); ++r) {
            if (ui->billTable->cellWidget(r, ColRemove) == removeBtn) {
                ui->billTable->removeRow(r);
                recalcTotal();
                break;
            }
        }
    });

    recalcRowPrice(row);
}

void Billing::recalcRowPrice(int row)
{
    auto *spin = qobject_cast<QDoubleSpinBox *>(ui->billTable->cellWidget(row, ColQty));
    if (!spin)
        return;

    const double unitPrice = ui->billTable->item(row, ColUnitPrice)->data(Qt::UserRole).toDouble();
    const double qty       = spin->value();
    const double linePrice = unitPrice * qty;

    ui->billTable->item(row, ColPrice)->setText(QString::number(linePrice, 'f', 2));
    recalcTotal();
}

void Billing::recalcTotal()
{
    double total = 0.0;
    for (int r = 0; r < ui->billTable->rowCount(); ++r)
        total += ui->billTable->item(r, ColPrice)->text().toDouble();

    ui->totalLabel->setText(tr("Total: Rs. %1").arg(QString::number(total, 'f', 2)));
}

// ─────────────────────────────────────────────────────────────────────────
//  Generate bill: write a PDF invoice, decrement stock, clear the table
//
//  BUG FIX (earlier): this used to contain TWO separate loops that both
//  ran "UPDATE products SET stock = stock - :qty", one after the other,
//  so every sale silently deducted stock TWICE. The whole block is now a
//  single pass — decrement stock, record the sale — done once per row,
//  wrapped in one transaction so a failure partway through rolls back
//  cleanly instead of leaving some rows updated and others not. The stock
//  update also floors at 0 (MAX(stock - :qty, 0)) as a safety net.
// ─────────────────────────────────────────────────────────────────────────
void Billing::generateBill()
{
    const int rowCount = ui->billTable->rowCount();
    if (rowCount == 0) {
        QMessageBox::information(this, tr("Empty Bill"), tr("Scan at least one product first."));
        return;
    }

    QSqlDatabase::database().transaction();

    // 1. Decrement stock and record each sale — exactly once per line item.
    for (int r = 0; r < rowCount; ++r) {
        const QString sku = ui->billTable->item(r, ColSku)->text();
        const int productId = ui->billTable->item(r, ColSku)->data(Qt::UserRole).toInt();
        auto *spin         = qobject_cast<QDoubleSpinBox *>(ui->billTable->cellWidget(r, ColQty));
        const double qty   = spin ? spin->value() : 0.0;
        const double unitPrice = ui->billTable->item(r, ColUnitPrice)->data(Qt::UserRole).toDouble();
        const QString name = ui->billTable->item(r, ColName)->text();

        QSqlQuery update;
        update.prepare("UPDATE products SET stock = MAX(stock - :qty, 0) WHERE sku = :sku");
        update.bindValue(":qty", qty);
        update.bindValue(":sku", sku);

        if (!update.exec()) {
            QSqlDatabase::database().rollback();
            QMessageBox::critical(this, tr("Database Error"),
                                  tr("Could not update stock for %1:\n%2")
                                      .arg(sku, update.lastError().text()));
            return;
        }

        // Record this sale so Reports (Revenue/Profit) can read it later.
        QSqlQuery insertSale;
        insertSale.prepare(R"sql(
            INSERT INTO sales (product_id, sku, product_name, quantity_sold, unit_price, sale_date)
            VALUES (:pid, :sku, :name, :qty, :price, :date)
        )sql");
        insertSale.bindValue(":pid",   productId);
        insertSale.bindValue(":sku",   sku);
        insertSale.bindValue(":name",  name);
        insertSale.bindValue(":qty",   qty);
        insertSale.bindValue(":price", unitPrice);
        insertSale.bindValue(":date",  QDateTime::currentDateTime().toString("yyyy-MM-dd"));

        if (!insertSale.exec()) {
            QSqlDatabase::database().rollback();
            QMessageBox::critical(this, tr("Database Error"),
                                  tr("Could not record sale for %1:\n%2")
                                      .arg(sku, insertSale.lastError().text()));
            return;
        }
    }

    if (!QSqlDatabase::database().commit()) {
        QMessageBox::critical(this, tr("Database Error"),
                              tr("Could not finalize the sale:\n%1")
                                  .arg(QSqlDatabase::database().lastError().text()));
        return;
    }

    // 2. Build a simple HTML invoice and print it straight to PDF.
    QString html = "<h2 style='color:#1a2a4a;'>Sajilo Bazar - Bill</h2>";
    html += QString("<p>%1</p>").arg(QDateTime::currentDateTime().toString("dd MMM yyyy, hh:mm ap"));
    html += "<table width='100%' cellspacing='0' cellpadding='4' border='1'>";
    html += "<tr style='background-color:#1a2a4a; color:white;'>"
            "<th>SKU</th><th>Product</th><th>Unit</th><th>Qty</th><th>Unit Price</th><th>Price</th></tr>";

    for (int r = 0; r < rowCount; ++r) {
        auto *spin = qobject_cast<QDoubleSpinBox *>(ui->billTable->cellWidget(r, ColQty));
        html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td>%6</td></tr>")
                    .arg(ui->billTable->item(r, ColSku)->text(),
                         ui->billTable->item(r, ColName)->text(),
                         ui->billTable->item(r, ColUnit)->text(),
                         QString::number(spin ? spin->value() : 0.0, 'f', 2),
                         ui->billTable->item(r, ColUnitPrice)->text(),
                         ui->billTable->item(r, ColPrice)->text());
    }
    html += "</table>";
    html += QString("<h3 style='text-align:right;'>%1</h3>").arg(ui->totalLabel->text());

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    const QString fileName = QString("%1/Bill_%2.pdf")
                                 .arg(dir, QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    QPdfWriter writer(fileName);
    writer.setPageSize(QPageSize(QPageSize::A5));
    writer.setPageMargins(QMarginsF(15, 15, 15, 15));

    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&writer);

    // 3. Reset the bill for the next customer.
    ui->billTable->setRowCount(0);
    recalcTotal();

    QMessageBox::information(this, tr("Bill Generated"),
                             tr("Bill saved to:\n%1\n\nStock has been updated.").arg(fileName));
}
