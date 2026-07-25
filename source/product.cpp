// ═══════════════════════════════════════════════════════════════════
//  Sajilo Bazar – Product Module (Admin: view + update stock + delete)
//  product.cpp
//  Reads bazar.db → products table.
//  Almost all logic (fetch, table rendering, search, filter,
//  pagination, delete) lives in ProductBase — this file only wires
//  up its own Designer widgets and adds "Update Stock".
// ═══════════════════════════════════════════════════════════════════

#include "../include/product.h"
#include "../ui/ui_product.h"
#include "../include/productrecyclebin.h"

#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QInputDialog>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>

Product::Product(QWidget *parent)
    : ProductBase(parent)
    , ui(new Ui::Product)
{
    ui->setupUi(this);

    // Auto-remove already-expired products (moves them to the Recycle
    // Bin) BEFORE the table's first load, same as ProductStaff — so the
    // admin page never shows a product that expired yesterday still
    // sitting in the active list. Products without an expiry date are
    // never touched by this (see ProductBase::autoRemoveExpiredProducts).
    autoRemoveExpiredProducts();

    // Common wiring (table columns, search/filter/pagination signals,
    // initial load) happens here, once this object's own widgets exist.
    initializeCommonUi();

    // Back to Dashboard — wireBackButton()/goBackToDashboard() come from
    // BackBase<QWidget>, via ProductBase (see productbase.h). Closing
    // this window is enough: AdminDashboard is already open underneath
    // it (see admindashboard.cpp's handleProducts_clicked()).
    wireBackButton(ui->btnBackToDashboard);

    connect(ui->btnRecycleBin, &QPushButton::clicked, this, &Product::openRecycleBin);
}

Product::~Product()
{
    delete ui;
}

void Product::openRecycleBin()
{
    // Same ProductBase-derived page pattern, filtered to soft-deleted
    // rows only (see ProductRecycleBin::showDeletedOnly()). Refresh this
    // page's own table when the Recycle Bin window closes, in case
    // anything was restored — a restored product should reappear here.
    auto *bin = new ProductRecycleBin();
    bin->setAttribute(Qt::WA_DeleteOnClose);
    connect(bin, &QObject::destroyed, this, &Product::loadProducts);
    bin->show();
}

// ── Turns on the same "⚠ Expiring Soon" checkbox + colour-coded
//    warnings that ProductStaff has, via the shared ProductBase helper.
void Product::setupExtraUi()
{
    setupExpiringSoonFilter();
}

// ── ProductBase widget accessors ───────────────────────────────────
QTableWidget* Product::tableWidget()    const { return ui->tblProducts; }
QLineEdit*    Product::searchBox()      const { return ui->txtSearch; }
QComboBox*    Product::categoryFilter() const { return ui->cmbFilterCategory; }
QPushButton*  Product::clearButton()    const { return ui->btnClearSearch; }
QPushButton*  Product::prevPageButton() const { return ui->btnPrevPage; }
QPushButton*  Product::nextPageButton() const { return ui->btnNextPage; }
QLabel*       Product::pageInfoLabel()  const { return ui->lblPageInfo; }
QLabel*       Product::statusBarLabel() const { return ui->lblStatusBar; }
QLabel*       Product::totalLabel()     const { return ui->lblTotalProducts; }

// ── Row actions: Update Stock + Delete (Delete's slot lives in base) ──
void Product::addActionButtons(int row, const ProductRecord &p)
{
    auto *cell   = new QWidget;
    auto *layout = new QHBoxLayout(cell);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    auto *btnStock = new QPushButton("📦 Update Stock");
    btnStock->setProperty("productId", p.id());
    btnStock->setStyleSheet(
        "QPushButton{background:#EAF7EF;color:#1E8E3E;border:1px solid #A9DFBF;"
        "border-radius:4px;padding:3px 10px;font-size:12px;}"
        "QPushButton:hover{background:#D5F2E0;}");
    connect(btnStock, &QPushButton::clicked, this, &Product::onUpdateStock);

    auto *btnDel = new QPushButton("🗑 Delete");
    btnDel->setProperty("productId", p.id());
    btnDel->setStyleSheet(
        "QPushButton{background:#FDEDEC;color:#C0392B;border:1px solid #F5B7B1;"
        "border-radius:4px;padding:3px 10px;font-size:12px;}"
        "QPushButton:hover{background:#FADBD8;}");
    connect(btnDel, &QPushButton::clicked, this, &Product::onDeleteProduct);

    layout->addStretch();
    layout->addWidget(btnStock);
    layout->addWidget(btnDel);
    layout->addStretch();

    ui->tblProducts->setCellWidget(row, 10, cell);
    ui->tblProducts->setRowHeight(row, 42);
}

// ── DB: stock update (kept local to Product; ProductStaff doesn't need it) ──
bool Product::updateStock(int id, int newStock)
{
    QSqlQuery q;
    q.prepare("UPDATE products SET stock = :stock WHERE id = :id");
    q.bindValue(":stock", newStock);
    q.bindValue(":id", id);
    if (!q.exec()) {
        QMessageBox::critical(this, "Database Error", q.lastError().text());
        return false;
    }
    return true;
}

void Product::onUpdateStock()
{
    auto *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    int id = btn->property("productId").toInt();
    ProductRecord p = fetchById(id);

    bool ok = false;
    int newStock = QInputDialog::getInt(
        this, "Update Stock",
        QString("New stock quantity for \"%1\" (SKU: %2):").arg(p.name(), p.sku()),
        p.stock(), 0, 1000000, 1, &ok);

    if (ok && newStock != p.stock() && updateStock(id, newStock)) {
        statusBarLabel()->setText(
            QString("📦  Stock for '%1' updated to %2.").arg(p.name()).arg(newStock));
        loadProducts();
    }
}