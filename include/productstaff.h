#ifndef PRODUCTSTAFF_H
#define PRODUCTSTAFF_H

#include "productbase.h"
#include <QDialog>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QDateEdit;
class QPushButton;
class QCheckBox;
namespace Ui { class ProductStaff; }

// ─────────────────────────────────────────────
//  Add / Edit Dialog — builds/edits a ProductRecord
// ─────────────────────────────────────────────
class ProductStaffDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ProductStaffDialog(QWidget *parent = nullptr,
                                const ProductRecord &product = ProductRecord());
    ~ProductStaffDialog() override;
    ProductRecord getProduct() const;

private slots:
    void onGenerateSku();
    void onAccept();
    void onNoExpiryToggled(bool checked);

private:
    void setupUi();
    void populateFields(const ProductRecord &p);
    QString generateSku();
    bool validateInputs();

    bool m_editMode = false;
    int  m_editId   = 0;

    QLineEdit   *txtName     = nullptr;
    QComboBox   *cmbCategory = nullptr;
    QComboBox   *cmbUnit     = nullptr;
    QLineEdit   *txtPrice    = nullptr;
    QSpinBox    *spnStock    = nullptr;
    QDateEdit   *deExpiry    = nullptr;
    QCheckBox   *chkNoExpiry = nullptr;
    QComboBox   *cmbStatus   = nullptr;
    QLineEdit   *txtSupplier = nullptr;
    QLineEdit   *txtSku      = nullptr;
    QPushButton *btnGenSku   = nullptr;
    QPushButton *btnSave     = nullptr;
    QPushButton *btnCancel   = nullptr;
};

// ─────────────────────────────────────────────
//  ProductStaff  —  Staff page (view + ADD + edit + delete)
//  Inherits shared fetch/table/search/filter/pagination/delete
//  logic from ProductBase, and additionally owns:
//    • "Add Product" (staff-only capability)
//    • "Back to Dashboard" — closes this page; StaffDashboard is
//      listening for this window's destroyed() signal and re-shows
//      itself (refreshed) the moment that happens. This is what
//      makes ProductStaff behave like a proper "page" instead of an
//      overlapping child widget.
//    • The full expiry-warning system (checkbox filter +
//      colour-coded ⚠ / ⛔ labels on the Expiry column), which
//      used to live on the admin Product page and has moved here.
// ─────────────────────────────────────────────
class ProductStaff : public ProductBase
{
    Q_OBJECT
public:
    explicit ProductStaff(QWidget *parent = nullptr);
    ~ProductStaff() override;

    static QStringList s_units;

protected:
    // ── ProductBase widget accessors ───────────────────────────
    QTableWidget* tableWidget()    const override;
    QLineEdit*    searchBox()      const override;
    QComboBox*    categoryFilter() const override;
    QPushButton*  clearButton()    const override;
    QPushButton*  prevPageButton() const override;
    QPushButton*  nextPageButton() const override;
    QLabel*       pageInfoLabel()  const override;
    QLabel*       statusBarLabel() const override;
    QLabel*       totalLabel()     const override;

    void addActionButtons(int row, const ProductRecord &p) override;

    // ── Staff-only extras: Add button + expiry-soon checkbox. The
    //    checkbox itself is built by ProductBase::setupExpiringSoonFilter()
    //    — formatExpiryText()/decorateExpiryCell()/expiryWarningWindowDays()
    //    all now live there too, shared with the Product admin page.
    void setupExtraUi() override;

private slots:
    void onAddProduct();
    void onEditProduct();
    void onTableDoubleClicked(int row, int column);
    void openRecycleBin();

private:
    bool saveProduct(const ProductRecord &p);
    bool updateProductInDb(const ProductRecord &p);

    // autoRemoveExpiredProducts() now lives on ProductBase (shared with
    // Product's admin page too) — see productbase.h. Both pages call the
    // same inherited implementation. Same for the expiry-warning window,
    // the checkbox, and onExpiringSoonToggled() — all moved to
    // ProductBase so Product and ProductStaff share one implementation.

    Ui::ProductStaff *ui;
};
#endif // PRODUCTSTAFF_H