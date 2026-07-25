#ifndef PRODUCT_H
#define PRODUCT_H

#include "productbase.h"

namespace Ui { class Product; }

// ─────────────────────────────────────────────
//  Product  —  Admin page (view + update stock + delete)
//  Inherits everything shared (fetch, table, search, filter,
//  pagination, delete/restore, expiry-warning colours) from
//  ProductBase, and now also turns on the "Expiring Soon" checkbox
//  (via setupExtraUi()) so the admin page has the same expiry system
//  as ProductStaff. Does NOT provide "Add Product" — that stays
//  staff-only.
//
//  Back-to-Dashboard: no inheritance change needed here — ProductBase
//  itself now extends BackBase<QWidget> (see backbase.h), so
//  wireBackButton()/goBackToDashboard() are already available to
//  Product via that chain. See product.cpp's constructor.
// ─────────────────────────────────────────────
class Product : public ProductBase
{
    Q_OBJECT
public:
    explicit Product(QWidget *parent = nullptr);
    ~Product() override;

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
    // formatExpiryText()/decorateExpiryCell() are left at ProductBase's
    // shared implementation on purpose — same colour-coding as Staff.
    void setupExtraUi() override;   // turns on the "Expiring Soon" checkbox

private slots:
    void onUpdateStock();
    void openRecycleBin();

private:
    bool updateStock(int id, int newStock);

    Ui::Product *ui;
};
#endif // PRODUCT_H