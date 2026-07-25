#ifndef PRODUCTRECYCLEBIN_H
#define PRODUCTRECYCLEBIN_H

#include "productbase.h"

namespace Ui { class ProductRecycleBin; }

// ─────────────────────────────────────────────
//  ProductRecycleBin  —  Recycle Bin page (Restore / Delete Permanently)
//  Same inheritance pattern as Product / ProductStaff / FrontProduct:
//  all fetch/table/search/filter/pagination logic is reused as-is from
//  ProductBase. The only two things this subclass changes are:
//    • showDeletedOnly() → true, so the shared fetch/count queries
//      list soft-deleted products instead of active ones.
//    • addActionButtons() → "♻ Restore" and "🗑 Delete Permanently"
//      instead of the usual Update Stock/Add/Edit + Delete buttons.
//  formatExpiryText() is also overridden to show when each product
//  was deleted instead of its expiry date (the ui labels that column
//  "Deleted On" instead of "Expiry" to match).
// ─────────────────────────────────────────────
class ProductRecycleBin : public ProductBase
{
    Q_OBJECT
public:
    explicit ProductRecycleBin(QWidget *parent = nullptr);
    ~ProductRecycleBin() override;

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

    // ── This page lists only soft-deleted products ─────────────
    bool showDeletedOnly() const override { return true; }

    // ── Row actions: Restore + Delete Permanently ───────────────
    void addActionButtons(int row, const ProductRecord &p) override;

    // ── Repurpose the "Expiry" column to show the deletion time ─
    QString formatExpiryText(const ProductRecord &p, int daysLeft) const override;

private:
    Ui::ProductRecycleBin *ui;
};
#endif // PRODUCTRECYCLEBIN_H