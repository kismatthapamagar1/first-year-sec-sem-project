#include "../include/inventory.h"
#include "../ui/ui_inventory.h"
#include "../ui/ui_staffdashboard.h"
#include "../include/stockservice.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QComboBox>
#include <QMouseEvent>
#include <QDebug>
#include <QShowEvent>

inventory::inventory(QWidget *parent)
    : BackBase<QMainWindow>(parent)
    , ui(new Ui::inventory)
{
    ui->setupUi(this);

    // Column headers, matching the "products" table column order
    const QStringList headers = {
        "ID", "Product Name", "Category", "Unit", "Price",
        "Stock", "Expiry Date", "Status", "Supplier", "SKU"
    };
    ui->inventoryTable->setColumnCount(headers.size());
    ui->inventoryTable->setHorizontalHeaderLabels(headers);
    ui->inventoryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->inventoryTable->verticalHeader()->setVisible(false);

    // Sort dropdown
    connect(ui->sortInventory,
            &QComboBox::currentIndexChanged,
            this,
            &inventory::handleSortChanged);

    // Prev / Next buttons
    connect(ui->btnPrev, &QPushButton::clicked, this, &inventory::goToPreviousPage);
    connect(ui->btnNext, &QPushButton::clicked, this, &inventory::goToNextPage);

    // Back to Dashboard — inherited from BackBase<QMainWindow>. Closing
    // this window is enough: AdminDashboard is already open underneath it
    // (see admindashboard.cpp's handleInventory_clicked()).
    wireBackButton(ui->btnBackToDashboard);

    // Make stat boxes clickable for filtering
    ui->cardTotal->installEventFilter(this);
    ui->cardLowStock->installEventFilter(this);
    ui->cardHighStock->installEventFilter(this);
    ui->cardOutOfStock->installEventFilter(this);

    updateStatCards();              // counts, always based on full table
    loadInventoryData(currentPage); // table rows, based on filter + page
}

inventory::~inventory()
{
    delete ui;
}

// Always counts ALL products in the database, regardless of currentFilter/page.
// This keeps the 4 stat cards stable no matter what filter is applied.
void inventory::updateStatCards()
{
    InventoryStats stats = StockService::computeStats();

    ui->lblTotalValue->setText(QString::number(stats.total));
    ui->lblLowStockValue->setText(QString::number(stats.low));
    ui->lblHighStockValue->setText(QString::number(stats.high));
    ui->lblOutOfStockValue->setText(QString::number(stats.out));
}

// Loads only the TABLE rows, respecting currentFilter and pagination.
// Does NOT touch the stat card numbers.
// Status is computed LIVE from stock (via StockService::statusForStock),
// never read from a stored "status" column — that's what caused the
// stat-card vs table mismatch.
void inventory::loadInventoryData(int page)
{
    currentPage = page;
    int offset = (currentPage - 1) * pageSize;

    QSqlQuery query;
    QString sql = "SELECT id, product_name, category, unit, price, stock, "
                  "expiry_date, supplier, sku FROM products "
                  "WHERE (is_deleted IS NULL OR is_deleted = 0) ";

    if (currentFilter == "Low Stock")
        sql += "AND stock > 0 AND stock <= :low ";
    else if (currentFilter == "High Stock")
        sql += "AND stock > :high ";
    else if (currentFilter == "Out Of Stock")
        sql += "AND stock <= 0 ";
    // currentFilter == "" (Total Products) -> no extra condition

    sql += "ORDER BY id LIMIT :limit OFFSET :offset";
    query.prepare(sql);

    if (currentFilter == "Low Stock")
        query.bindValue(":low", StockService::LOW_STOCK_THRESHOLD);
    else if (currentFilter == "High Stock")
        query.bindValue(":high", StockService::HIGH_STOCK_THRESHOLD);

    query.bindValue(":limit", pageSize);
    query.bindValue(":offset", offset);

    if (!query.exec()) {
        qWarning() << "Inventory: failed to load products -" << query.lastError().text();
        return;
    }

    ui->inventoryTable->setRowCount(0);
    int row = 0;

    // Query column order: 0=id, 1=product_name, 2=category, 3=unit, 4=price,
    // 5=stock, 6=expiry_date, 7=supplier, 8=sku
    // Table column order: 0=ID,1=Name,2=Category,3=Unit,4=Price,5=Stock,
    // 6=Expiry,7=Status,8=Supplier,9=SKU
    while (query.next()) {
        ui->inventoryTable->insertRow(row);

        int stock = query.value(5).toInt();
        QString liveStatus = StockService::statusForStock(stock);

        for (int col = 0; col < 10; ++col) {
            auto *item = new QTableWidgetItem();

            if (col == 5) {
                // Store Stock as a real number so sorting works correctly
                item->setData(Qt::DisplayRole, stock);
            } else if (col == 7) {
                item->setText(liveStatus);                  // Status (computed, not from DB)
            } else if (col == 8) {
                item->setText(query.value(7).toString());   // Supplier
            } else if (col == 9) {
                item->setText(query.value(8).toString());   // SKU
            } else {
                item->setText(query.value(col).toString());
            }

            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui->inventoryTable->setItem(row, col, item);
        }

        ++row;
    }
}

void inventory::filterByStatus(const QString &status)
{
    currentFilter = status;
    loadInventoryData(1); // reset to page 1 whenever the filter changes
    // Note: updateStatCards() is NOT called here on purpose -
    // stat card numbers must stay fixed regardless of filter.
}

bool inventory::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        if (watched == ui->cardLowStock) {
            filterByStatus("Low Stock");
            return true;
        } else if (watched == ui->cardHighStock) {
            filterByStatus("High Stock");
            return true;
        } else if (watched == ui->cardOutOfStock) {
            filterByStatus("Out Of Stock");
            return true;
        } else if (watched == ui->cardTotal) {
            filterByStatus(""); // clicking "Total Products" clears the filter
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void inventory::handleSortChanged(int index)
{
    switch (index) {
    case 1: // Name (A-Z)
        ui->inventoryTable->sortItems(1, Qt::AscendingOrder);
        break;
    case 2: // Stock (Low to High)
        ui->inventoryTable->sortItems(5, Qt::AscendingOrder);
        break;
    case 3: // Expiry Date
        ui->inventoryTable->sortItems(6, Qt::AscendingOrder);
        break;
    default:
        break; // "Sort Inventory" placeholder - no-op
    }
}

void inventory::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    // Every time this page becomes visible again (e.g. switching back
    // from the Product page after adding/editing/deleting a product),
    // refresh both the stat cards and the table so nothing is stale.
    updateStatCards();
    loadInventoryData(currentPage);
}

void inventory::goToNextPage()
{
    loadInventoryData(currentPage + 1);
}

void inventory::goToPreviousPage()
{
    if (currentPage > 1)
        loadInventoryData(currentPage - 1);
}