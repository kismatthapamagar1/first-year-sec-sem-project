// ═══════════════════════════════════════════════════════════════════
//  Sajilo Bazar – Product Recycle Bin
//  productrecyclebin.cpp
//  Reads bazar.db → products table, filtered to is_deleted = 1 rows
//  only (via ProductBase::showDeletedOnly()). Almost all logic
//  (fetch, table rendering, search, filter, pagination) lives in
//  ProductBase — this file only wires up its own Designer widgets
//  and provides the Restore / Delete Permanently row actions.
// ═══════════════════════════════════════════════════════════════════

#include "../include/productrecyclebin.h"
#include "../ui/ui_productrecyclebin.h"

#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QDateTime>

ProductRecycleBin::ProductRecycleBin(QWidget *parent)
    : ProductBase(parent)
    , ui(new Ui::ProductRecycleBin)
{
    ui->setupUi(this);

    // Common wiring (table columns, search/filter/pagination signals,
    // initial load) happens here, once this object's own widgets exist.
    initializeCommonUi();
}

ProductRecycleBin::~ProductRecycleBin()
{
    delete ui;
}

// ── ProductBase widget accessors ───────────────────────────────────
QTableWidget* ProductRecycleBin::tableWidget()    const { return ui->tblProducts; }
QLineEdit*    ProductRecycleBin::searchBox()      const { return ui->txtSearch; }
QComboBox*    ProductRecycleBin::categoryFilter() const { return ui->cmbFilterCategory; }
QPushButton*  ProductRecycleBin::clearButton()    const { return ui->btnClearSearch; }
QPushButton*  ProductRecycleBin::prevPageButton() const { return ui->btnPrevPage; }
QPushButton*  ProductRecycleBin::nextPageButton() const { return ui->btnNextPage; }
QLabel*       ProductRecycleBin::pageInfoLabel()  const { return ui->lblPageInfo; }
QLabel*       ProductRecycleBin::statusBarLabel() const { return ui->lblStatusBar; }
QLabel*       ProductRecycleBin::totalLabel()     const { return ui->lblTotalProducts; }

// ── Row actions: Restore + Delete Permanently (both slots live in
//    ProductBase since the DB access and confirmation dialogs are
//    identical no matter which subclass triggers them) ──────────────
void ProductRecycleBin::addActionButtons(int row, const ProductRecord &p)
{
    auto *cell   = new QWidget;
    auto *layout = new QHBoxLayout(cell);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    auto *btnRestore = new QPushButton("♻ Restore");
    btnRestore->setProperty("productId", p.id());
    btnRestore->setStyleSheet(
        "QPushButton{background:#EAF7EF;color:#1E8E3E;border:1px solid #A9DFBF;"
        "border-radius:4px;padding:3px 10px;font-size:12px;}"
        "QPushButton:hover{background:#D5F2E0;}");
    connect(btnRestore, &QPushButton::clicked, this, &ProductRecycleBin::onRestoreProduct);

    auto *btnDel = new QPushButton("🗑 Delete Permanently");
    btnDel->setProperty("productId", p.id());
    btnDel->setStyleSheet(
        "QPushButton{background:#FDEDEC;color:#C0392B;border:1px solid #F5B7B1;"
        "border-radius:4px;padding:3px 10px;font-size:12px;}"
        "QPushButton:hover{background:#FADBD8;}");
    connect(btnDel, &QPushButton::clicked, this, &ProductRecycleBin::onPermanentDeleteProduct);

    layout->addStretch();
    layout->addWidget(btnRestore);
    layout->addWidget(btnDel);
    layout->addStretch();

    ui->tblProducts->setCellWidget(row, 10, cell);
    ui->tblProducts->setRowHeight(row, 42);
}

// ── The 7th column is labelled "Deleted On" in this page's .ui, so show
//    the deletion timestamp here instead of the (irrelevant) expiry date.
QString ProductRecycleBin::formatExpiryText(const ProductRecord &p, int /*daysLeft*/) const
{
    if (p.deletedAt().isEmpty())
        return QStringLiteral("—");

    QDateTime dt = QDateTime::fromString(p.deletedAt(), "yyyy-MM-dd hh:mm:ss");
    return dt.isValid() ? dt.toString("dd-MMM-yyyy  hh:mm") : p.deletedAt();
}