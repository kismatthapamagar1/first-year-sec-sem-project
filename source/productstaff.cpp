// ═══════════════════════════════════════════════════════════════════
//  Sajilo Bazar – Product Staff Module
//  productstaff.cpp
//  Reads/writes bazar.db → products table.
//  Shared fetch/table/search/filter/pagination/delete logic lives in
//  ProductBase. This file adds what is unique to staff: "Add Product",
//  "Back to Dashboard", and the full expiry-warning system (checkbox
//  filter + colour-coded ⚠ / ⛔ labels), which now lives here instead
//  of on the admin page.
// ═══════════════════════════════════════════════════════════════════

#include "../include/productstaff.h"
#include "../ui/ui_productstaff.h"
#include "../include/productrecyclebin.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QFrame>
#include <QDate>
#include <QMap>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QSqlQuery>
#include <QSqlError>
#include <QColor>
#include <QDebug>
#include <QFont>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDateEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QMessageBox>
#include <climits>

// ═══════════════════════════════════════════════════════════════════
//  Luxury theme palette (Burgundy / Garnet) — used for the bits of UI
//  that are built in C++ rather than styled purely from the .ui file
//  (the dialog, row action buttons, expiry-cell decoration).
// ═══════════════════════════════════════════════════════════════════
namespace Theme {
constexpr const char *Burgundy    = "#660033";
constexpr const char *BurgundyDk  = "#4D0026";
constexpr const char *BurgundyBg  = "#F3E6EC"; // light tint for hover/edit states
constexpr const char *Garnet      = "#8B0000";
constexpr const char *GarnetBg    = "#FBEAE8";
constexpr const char *WarningTxt  = "#856404"; // kept semantic: amber = "expiring soon"
constexpr const char *WarningBg   = "#FFF3CD";
}

// ── Static data ───────────────────────────────────────────────────
QStringList ProductStaff::s_units = {
    "pcs", "kg", "g", "litre", "ml",
    "dozen", "box", "packet", "bottle", "can",
    "meter", "pair", "set", "piece", "pieces"
};

// ═══════════════════════════════════════════════════════════════════
//  PRODUCT STAFF DIALOG  –  Add / Edit
// ═══════════════════════════════════════════════════════════════════
ProductStaffDialog::ProductStaffDialog(QWidget *parent, const ProductRecord &product)
    : QDialog(parent)
{
    m_editMode = (product.id() > 0);
    m_editId   = product.id();

    setWindowTitle(m_editMode ? "Edit Product" : "Add New Product");
    setMinimumWidth(500);
    setModal(true);

    setupUi();

    if (m_editMode)
        populateFields(product);
    else
        onGenerateSku();
}

ProductStaffDialog::~ProductStaffDialog() {}

void ProductStaffDialog::setupUi()
{
    auto *lblTitle = new QLabel(
        m_editMode ? "✏  Edit Product" : "➕  Add New Product", this);
    lblTitle->setStyleSheet(
        QString("font-size:16px; font-weight:bold; color:%1; padding:6px 0;")
            .arg(Theme::Burgundy));

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("color:#E6DEE1;");

    // ── Fields ──────────────────────────────────────────────────
    txtName = new QLineEdit(this);
    txtName->setPlaceholderText("Enter product name");

    cmbCategory = new QComboBox(this);
    for (const auto &c : ProductStaff::loadCategoriesFromDb())
        cmbCategory->addItem(c);

    cmbUnit = new QComboBox(this);
    cmbUnit->setEditable(true);
    cmbUnit->addItems(ProductStaff::s_units);
    cmbUnit->setCurrentIndex(-1);
    if (cmbUnit->lineEdit())
        cmbUnit->lineEdit()->setPlaceholderText("e.g. kg, pcs, litre");

    txtPrice = new QLineEdit(this);
    txtPrice->setPlaceholderText("0.00");
    txtPrice->setValidator(new QRegularExpressionValidator(
        QRegularExpression(R"(^\d{0,10}(\.\d{0,2})?$)"), txtPrice));

    spnStock = new QSpinBox(this);
    spnStock->setRange(0, 999999);
    spnStock->setSuffix("  units");

    deExpiry = new QDateEdit(this);
    deExpiry->setCalendarPopup(true);
    deExpiry->setDisplayFormat("yyyy-MM-dd");
    deExpiry->setDate(QDate::currentDate().addYears(1));
    deExpiry->setMinimumDate(QDate(2000, 1, 1));

    // Not every product expires — stationery, hardware, electronics,
    // household durables, etc. Checking this disables/clears the date
    // field and the product is saved with no expiry_date at all, so it
    // never shows a warning and never gets auto-removed by
    // ProductBase::autoRemoveExpiredProducts() (that query only ever
    // touches products that actually HAVE an expiry_date).
    chkNoExpiry = new QCheckBox("Does not expire", this);
    connect(chkNoExpiry, &QCheckBox::toggled, this, &ProductStaffDialog::onNoExpiryToggled);

    auto *expiryRow = new QHBoxLayout;
    expiryRow->addWidget(deExpiry);
    expiryRow->addWidget(chkNoExpiry);

    cmbStatus = new QComboBox(this);
    cmbStatus->addItems({"In Stock", "Low Stock", "High Stock", "Out Of Stock"});
    txtSupplier = new QLineEdit(this);
    txtSupplier->setPlaceholderText("Supplier name");

    txtSku = new QLineEdit(this);
    txtSku->setPlaceholderText("e.g. SKU001");
    txtSku->setMaxLength(50);

    btnGenSku = new QPushButton("⟳ Generate", this);
    btnGenSku->setFixedWidth(100);
    btnGenSku->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %2;"
                "border-radius:4px;padding:5px;}"
                "QPushButton:hover{background:#E6CCDA;}")
            .arg(Theme::BurgundyBg, Theme::Burgundy));
    connect(btnGenSku, &QPushButton::clicked, this, &ProductStaffDialog::onGenerateSku);

    auto *skuRow = new QHBoxLayout;
    skuRow->addWidget(txtSku);
    skuRow->addWidget(btnGenSku);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setSpacing(12);
    form->setContentsMargins(20, 8, 20, 8);

    auto makeLabel = [](const QString &txt) {
        auto *l = new QLabel(txt);
        l->setStyleSheet("font-weight:600; color:#2C2C2C;");
        return l;
    };

    form->addRow(makeLabel("Product Name *"), txtName);
    form->addRow(makeLabel("Category *"),     cmbCategory);
    form->addRow(makeLabel("Unit"),           cmbUnit);
    form->addRow(makeLabel("Price (Rs.) *"),  txtPrice);
    form->addRow(makeLabel("Stock *"),        spnStock);
    form->addRow(makeLabel("Expiry Date"),    expiryRow);
    form->addRow(makeLabel("Status"),         cmbStatus);
    form->addRow(makeLabel("Supplier"),       txtSupplier);
    form->addRow(makeLabel("SKU"),            skuRow);

    btnSave   = new QPushButton(
        m_editMode ? "💾  Save Changes" : "✔  Add Product", this);
    btnCancel = new QPushButton("✕  Cancel", this);

    btnSave->setStyleSheet(
        QString("QPushButton{background:%1;color:white;border:none;border-radius:6px;"
                "padding:8px 22px;font-weight:bold;}"
                "QPushButton:hover{background:%2;}")
            .arg(Theme::Burgundy, Theme::BurgundyDk));
    btnCancel->setStyleSheet(
        "QPushButton{background:#EFE9EB;color:#444;border:none;border-radius:6px;"
        "padding:8px 22px;}"
        "QPushButton:hover{background:#DDD1D6;}");

    connect(btnSave,   &QPushButton::clicked, this, &ProductStaffDialog::onAccept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(btnCancel);
    btnRow->addWidget(btnSave);

    auto *master = new QVBoxLayout(this);
    master->setSpacing(8);
    master->setContentsMargins(0, 12, 0, 12);
    master->addWidget(lblTitle, 0, Qt::AlignCenter);
    master->addWidget(separator);
    master->addLayout(form);
    master->addSpacing(8);
    master->addLayout(btnRow);
    master->addSpacing(4);

    setStyleSheet(QString(R"(
        QDialog { background:#FFFFFF; }
        QLineEdit, QComboBox, QSpinBox, QDateEdit {
            background:#FFFFFF; border:1px solid #D8CBD0;
            border-radius:5px; padding:5px 8px; min-height:28px;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDateEdit:focus {
            border:1px solid %1;
        }
        QLineEdit:read-only {
            background:#F2EDEF; color:#8A7E82;
        }
    )").arg(Theme::Burgundy));
}

void ProductStaffDialog::populateFields(const ProductRecord &p)
{
    txtName->setText(p.name());
    txtName->setReadOnly(true);
    txtName->setCursor(Qt::ArrowCursor);
    txtName->setToolTip("Product name cannot be changed once created.");

    int catIdx = cmbCategory->findText(p.category());
    if (catIdx < 0 && !p.category().isEmpty()) {
        cmbCategory->addItem(p.category());
        catIdx = cmbCategory->count() - 1;
    }
    cmbCategory->setCurrentIndex(catIdx >= 0 ? catIdx : 0);

    int unitIdx = cmbUnit->findText(p.unit(), Qt::MatchFixedString);
    if (unitIdx < 0 && !p.unit().isEmpty()) {
        cmbUnit->addItem(p.unit());
        unitIdx = cmbUnit->count() - 1;
    }
    cmbUnit->setCurrentIndex(unitIdx);
    if (unitIdx < 0)
        cmbUnit->setCurrentText(p.unit());

    txtPrice->setText(QString::number(p.price(), 'f', 2));
    spnStock->setValue(p.stock());

    if (!p.expiryDate().isEmpty()) {
        QDate d = QDate::fromString(p.expiryDate(), "yyyy-MM-dd");
        if (d.isValid()) deExpiry->setDate(d);
        chkNoExpiry->setChecked(false);
    } else {
        chkNoExpiry->setChecked(true);   // onNoExpiryToggled() disables deExpiry
    }

    int stIdx = cmbStatus->findText(p.status());
    cmbStatus->setCurrentIndex(stIdx >= 0 ? stIdx : 0);

    txtSupplier->setText(p.supplier());

    txtSku->setText(p.sku());
    txtSku->setReadOnly(true);
    btnGenSku->setEnabled(false);
}

QString ProductStaffDialog::generateSku()
{
    // First 2 letters of the product name, uppercased (e.g. "Shirt" -> "SH").
    // Falls back to padding with "X" if the name is empty or has fewer than
    // 2 letter characters typed so far, so Generate never produces a
    // malformed SKU.
    QString name = txtName ? txtName->text().trimmed() : QString();

    QString lettersOnly;
    for (const QChar &c : name) {
        if (c.isLetter())
            lettersOnly += c.toUpper();
        if (lettersOnly.length() == 2)
            break;
    }
    while (lettersOnly.length() < 2)
        lettersOnly += "X";

    quint32 num = QRandomGenerator::global()->bounded(100u, 999u);
    return QString("SKU-%1-%2").arg(lettersOnly).arg(num, 3, 10, QChar('0'));
}

void ProductStaffDialog::onGenerateSku()
{
    txtSku->setText(generateSku());
}

void ProductStaffDialog::onNoExpiryToggled(bool checked)
{
    deExpiry->setDisabled(checked);
}

bool ProductStaffDialog::validateInputs()
{
    if (txtName->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation", "Product Name is required.");
        txtName->setFocus();
        return false;
    }
    if (txtPrice->text().trimmed().isEmpty() ||
        txtPrice->text().toDouble() < 0) {
        QMessageBox::warning(this, "Validation", "Please enter a valid Price.");
        txtPrice->setFocus();
        return false;
    }
    return true;
}

void ProductStaffDialog::onAccept()
{
    if (validateInputs())
        accept();
}

ProductRecord ProductStaffDialog::getProduct() const
{
    ProductRecord p;
    p.setId(m_editId);
    p.setName(txtName->text().trimmed());
    p.setCategory(cmbCategory->currentText());
    p.setUnit(cmbUnit->currentText().trimmed());
    p.setPrice(txtPrice->text().toDouble());
    p.setStock(spnStock->value());
    p.setExpiryDate(chkNoExpiry->isChecked() ? QString() : deExpiry->date().toString("yyyy-MM-dd"));
    p.setStatus(cmbStatus->currentText());
    p.setSupplier(txtSupplier->text().trimmed());
    p.setSku(txtSku->text().trimmed());
    return p;
}

// ═══════════════════════════════════════════════════════════════════
//  PRODUCT STAFF WIDGET  –  Main Screen
// ═══════════════════════════════════════════════════════════════════
ProductStaff::ProductStaff(QWidget *parent)
    : ProductBase(parent)
    , ui(new Ui::ProductStaff)
{
    ui->setupUi(this);

    connect(ui->btnAddProduct, &QPushButton::clicked,
            this,              &ProductStaff::onAddProduct);
    connect(ui->tblProducts,   &QTableWidget::cellDoubleClicked,
            this,              &ProductStaff::onTableDoubleClicked);
    connect(ui->btnRecycleBin, &QPushButton::clicked,
            this,              &ProductStaff::openRecycleBin);

    // "Back to Dashboard" simply closes this window. Because
    // StaffDashboard constructs this page with no parent and
    // WA_DeleteOnClose, and listens for its destroyed() signal,
    // closing here is all it takes for the dashboard to reappear
    // (refreshed) on its own — no direct coupling needed.
    connect(ui->btnBackDashboard, &QPushButton::clicked,
            this,                 &QWidget::close);

    // Auto-remove already-expired products (moves them to the Recycle Bin)
    // BEFORE the table's first load, so the page never shows a product
    // that expired yesterday still sitting in the active list.
    autoRemoveExpiredProducts();

    // Common wiring (columns, search/filter/pagination, initial load)
    // + setupExtraUi() below (expiry checkbox, via ProductBase).
    initializeCommonUi();
}

ProductStaff::~ProductStaff()
{
    delete ui;
}

// ── ProductBase widget accessors ───────────────────────────────────
QTableWidget* ProductStaff::tableWidget()    const { return ui->tblProducts; }
QLineEdit*    ProductStaff::searchBox()      const { return ui->txtSearch; }
QComboBox*    ProductStaff::categoryFilter() const { return ui->cmbFilterCategory; }
QPushButton*  ProductStaff::clearButton()    const { return ui->btnClearSearch; }
QPushButton*  ProductStaff::prevPageButton() const { return ui->btnPrevPage; }
QPushButton*  ProductStaff::nextPageButton() const { return ui->btnNextPage; }
QLabel*       ProductStaff::pageInfoLabel()  const { return ui->lblPageInfo; }
QLabel*       ProductStaff::statusBarLabel() const { return ui->lblStatusBar; }
QLabel*       ProductStaff::totalLabel()     const { return ui->lblTotalProducts; }

// ── Staff-only extras: expiry-soon checkbox, built by the shared
//    ProductBase helper (Product's admin page calls the same helper) ──
void ProductStaff::setupExtraUi()
{
    setupExpiringSoonFilter();
}

// formatExpiryText()/decorateExpiryCell()/expiryWarningWindowDays()/
// expiringSoonFilterActive() all now live on ProductBase, shared with
// the Product admin page — see productbase.cpp.

// ── Row actions: Edit + Delete (Delete's slot lives in base) ───────
void ProductStaff::addActionButtons(int row, const ProductRecord &p)
{
    auto *cell   = new QWidget;
    auto *layout = new QHBoxLayout(cell);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    auto *btnEdit = new QPushButton("✏ Edit");
    auto *btnDel  = new QPushButton("🗑 Delete");

    btnEdit->setProperty("productId", p.id());
    btnDel ->setProperty("productId", p.id());

    btnEdit->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %2;"
                "border-radius:4px;padding:3px 10px;font-size:12px;}"
                "QPushButton:hover{background:#E6CCDA;}")
            .arg(Theme::BurgundyBg, Theme::Burgundy));
    btnDel->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid #E8B4AE;"
                "border-radius:4px;padding:3px 10px;font-size:12px;}"
                "QPushButton:hover{background:#F5D2CD;}")
            .arg(Theme::GarnetBg, Theme::Garnet));

    connect(btnEdit, &QPushButton::clicked, this, &ProductStaff::onEditProduct);
    connect(btnDel,  &QPushButton::clicked, this, &ProductStaff::onDeleteProduct);

    layout->addStretch();
    layout->addWidget(btnEdit);
    layout->addWidget(btnDel);
    layout->addStretch();

    ui->tblProducts->setCellWidget(row, 10, cell);
    ui->tblProducts->setRowHeight(row, 42);
}

// ─────────────────────────────────────────────────────────────────
//  DB OPERATIONS specific to staff (INSERT / UPDATE — Add/Edit)
// ─────────────────────────────────────────────────────────────────
bool ProductStaff::saveProduct(const ProductRecord &p)
{
    QSqlQuery q;
    q.prepare(R"(
        INSERT INTO products
            (product_name, category, unit, price, stock, expiry_date, status, supplier, sku)
        VALUES
            (:name, :cat, :unit, :price, :stock, :exp, :status, :supplier, :sku)
    )");
    q.bindValue(":name",     p.name());
    q.bindValue(":cat",      p.category());
    q.bindValue(":unit",     p.unit());
    q.bindValue(":price",    p.price());
    q.bindValue(":stock",    p.stock());
    q.bindValue(":exp",      p.expiryDate());
    q.bindValue(":status",   p.status());
    q.bindValue(":supplier", p.supplier());
    q.bindValue(":sku",      p.sku());

    if (!q.exec()) {
        QMessageBox::critical(this, "Database Error", q.lastError().text());
        return false;
    }
    return true;
}

bool ProductStaff::updateProductInDb(const ProductRecord &p)
{
    // product_name is intentionally NOT updated: the Edit dialog locks
    // the name field, so we exclude it from the UPDATE for safety too.
    QSqlQuery q;
    q.prepare(R"(
        UPDATE products
        SET category     = :cat,
            unit         = :unit,
            price        = :price,
            stock        = :stock,
            expiry_date  = :exp,
            status       = :status,
            supplier     = :supplier,
            sku          = :sku
        WHERE id = :id
    )");
    q.bindValue(":cat",      p.category());
    q.bindValue(":unit",     p.unit());
    q.bindValue(":price",    p.price());
    q.bindValue(":stock",    p.stock());
    q.bindValue(":exp",      p.expiryDate());
    q.bindValue(":status",   p.status());
    q.bindValue(":supplier", p.supplier());
    q.bindValue(":sku",      p.sku());
    q.bindValue(":id",       p.id());

    if (!q.exec()) {
        QMessageBox::critical(this, "Database Error", q.lastError().text());
        return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────
//  SLOTS specific to staff
// ─────────────────────────────────────────────────────────────────
void ProductStaff::openRecycleBin()
{
    // Same pattern as Product::openRecycleBin() — a standalone window,
    // refreshing this page's table when it closes in case something
    // was restored.
    auto *bin = new ProductRecycleBin();
    bin->setAttribute(Qt::WA_DeleteOnClose);
    connect(bin, &QObject::destroyed, this, &ProductStaff::loadProducts);
    bin->show();
}

void ProductStaff::onAddProduct()
{
    ProductStaffDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        const ProductRecord p = dlg.getProduct();
        if (saveProduct(p)) {
            statusBarLabel()->setText(
                QString("✔  '%1' added successfully.").arg(p.name()));
            loadProducts();
        }
    }
}

void ProductStaff::onEditProduct()
{
    auto *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    int id = btn->property("productId").toInt();
    ProductRecord p = fetchById(id);
    if (p.id() <= 0) {
        QMessageBox::warning(this, "Not Found", "Product record not found.");
        return;
    }

    ProductStaffDialog dlg(this, p);
    if (dlg.exec() == QDialog::Accepted) {
        ProductRecord updated = dlg.getProduct();
        updated.setName(p.name());   // name is locked; keep original
        if (updateProductInDb(updated)) {
            statusBarLabel()->setText(
                QString("✔  '%1' updated successfully.").arg(updated.name()));
            loadProducts();
        }
    }
}

void ProductStaff::onTableDoubleClicked(int row, int /*column*/)
{
    auto *item = ui->tblProducts->item(row, 0);
    int id = item ? item->data(Qt::UserRole).toInt() : -1;
    if (id <= 0) return;

    ProductRecord p = fetchById(id);
    if (p.id() <= 0) return;

    ProductStaffDialog dlg(this, p);
    if (dlg.exec() == QDialog::Accepted) {
        ProductRecord updated = dlg.getProduct();
        updated.setName(p.name());   // name is locked; keep original
        if (updateProductInDb(updated)) {
            statusBarLabel()->setText(
                QString("✔  '%1' updated.").arg(updated.name()));
            loadProducts();
        }
    }
}