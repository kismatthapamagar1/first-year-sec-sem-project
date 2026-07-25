#ifndef PRODUCTBASE_H
#define PRODUCTBASE_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QList>
#include "../include/backbase.h"

class QTableWidget;
class QTableWidgetItem;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QCheckBox;

// ═══════════════════════════════════════════════════════════════════
//  ProductRecord  —  ENCAPSULATION
//  Single shared DTO for the "products" table. All fields are
//  private; the outside world only ever touches them through the
//  getters/setters below. Both Product and ProductStaff (and the
//  add/edit dialog) work with this exact same class, so there is
//  only one definition of "what a product is" in the whole program.
// ═══════════════════════════════════════════════════════════════════
class ProductRecord
{
public:
    ProductRecord() = default;

    int     id()        const { return m_id; }
    QString name()       const { return m_name; }
    QString category()   const { return m_category; }
    QString unit()       const { return m_unit; }
    double  price()      const { return m_price; }
    int     stock()      const { return m_stock; }
    QString expiryDate() const { return m_expiryDate; }
    QString status()     const { return m_status; }
    QString supplier()   const { return m_supplier; }
    QString sku()        const { return m_sku; }

    // ── Recycle Bin fields ──────────────────────────────────────
    bool    isDeleted()  const { return m_isDeleted; }
    QString deletedAt()  const { return m_deletedAt; }

    void setId(int v)                  { m_id = v; }
    void setName(const QString &v)     { m_name = v; }
    void setCategory(const QString &v) { m_category = v; }
    void setUnit(const QString &v)     { m_unit = v; }
    void setPrice(double v)            { m_price = v; }
    void setStock(int v)               { m_stock = v; }
    void setExpiryDate(const QString &v) { m_expiryDate = v; }
    void setStatus(const QString &v)   { m_status = v; }
    void setSupplier(const QString &v) { m_supplier = v; }
    void setSku(const QString &v)      { m_sku = v; }
    void setIsDeleted(bool v)          { m_isDeleted = v; }
    void setDeletedAt(const QString &v) { m_deletedAt = v; }

    // Days remaining until expiry (negative = already expired).
    // Returns INT_MIN when there is no usable expiry date.
    int daysUntilExpiry() const;

private:
    int     m_id = 0;
    QString m_name;
    QString m_category;
    QString m_unit;
    double  m_price = 0.0;
    int     m_stock = 0;
    QString m_expiryDate;   // "yyyy-MM-dd"
    QString m_status;
    QString m_supplier;
    QString m_sku;
    bool    m_isDeleted = false;
    QString m_deletedAt;    // "yyyy-MM-dd hh:mm:ss", empty if not deleted
};

// ═══════════════════════════════════════════════════════════════════
//  ProductBase  —  INHERITANCE
//  Abstract base widget holding everything the Product (admin) page
//  and the ProductStaff page have in common: reading from the DB,
//  drawing the table, search box, category filter and pagination.
//
//  Now extends BackBase<QWidget> rather than QWidget directly, so both
//  Product and ProductStaff automatically pick up wireBackButton() /
//  goBackToDashboard() through this one shared base — see backbase.h
//  for why that's a template. This is the only inheritance change; the
//  rest of the class is unchanged from before.
//
//  A concrete subclass must:
//    1. "plug in" its own Designer widgets by implementing the
//       protected pure-virtual accessors below (tableWidget(),
//       searchBox(), ... ) so this class can drive them.
//    2. implement addActionButtons(), since Product offers
//       "Update Stock" while ProductStaff offers "Edit".
//    3. call initializeCommonUi() at the END of its own constructor
//       (i.e. after ui->setupUi(this) has created its widgets), and
//       separately call wireBackButton(ui->btnBackToDashboard) once
//       that button exists, to enable Back to Dashboard.
//       This two-step construction is required because virtual
//       functions cannot be safely dispatched to a derived class
//       from inside the base class's own constructor.
//
//  The expiry-warning system (colour-coded ⚠ / ⛔ labels + the
//  "Expiring Soon" checkbox) lives here as real, shared logic — any
//  subclass that calls setupExpiringSoonFilter() from its own
//  setupExtraUi() gets the checkbox + filtering; the colour-coding
//  itself (formatExpiryText/decorateExpiryCell) is always on, since a
//  plain expiry date benefits from it everywhere it's shown.
// ═══════════════════════════════════════════════════════════════════
class ProductBase : public BackBase<QWidget>
{
    Q_OBJECT
public:
    explicit ProductBase(QWidget *parent = nullptr);
    ~ProductBase() override = default;

    // Reads distinct category names live from the "categories" table so
    // any change made on the Category page is picked up immediately.
    static QStringList loadCategoriesFromDb();

protected:
    // ── Widgets the subclass must expose (bound to its own .ui) ───────
    virtual QTableWidget* tableWidget()    const = 0;
    virtual QLineEdit*    searchBox()      const = 0;
    virtual QComboBox*    categoryFilter() const = 0;
    virtual QPushButton*  clearButton()    const = 0;
    virtual QPushButton*  prevPageButton() const = 0;
    virtual QPushButton*  nextPageButton() const = 0;
    virtual QLabel*       pageInfoLabel()  const = 0;
    virtual QLabel*       statusBarLabel() const = 0;
    virtual QLabel*       totalLabel()     const = 0;

    // ── Row-action buttons differ per page → each subclass builds them ──
    virtual void addActionButtons(int row, const ProductRecord &p) = 0;

    // ── Recycle Bin: OFF by default, so Product/ProductStaff/FrontProduct
    //    only ever list active (non-deleted) products. ProductRecycleBin
    //    overrides this to true so the exact same fetch/count queries
    //    list soft-deleted rows instead — no query duplication needed.
    virtual bool showDeletedOnly() const { return false; }

    // ── Expiry-warning system: real shared logic, not just a hook.
    //    expiringSoonFilterActive() reflects whichever subclass has
    //    called setupExpiringSoonFilter() (see below) — stays "off"
    //    (false) for any page that never calls it, e.g. FrontProduct
    //    and ProductRecycleBin. formatExpiryText()/decorateExpiryCell()
    //    show the ⚠/⛔ colour-coding unconditionally, since a plain
    //    expiry date benefits from it on every page that has one.
    virtual bool    expiringSoonFilterActive() const;
    virtual int     expiryWarningWindowDays()  const { return 5; }
    virtual QString formatExpiryText(const ProductRecord &p, int daysLeft) const;
    virtual void    decorateExpiryCell(QTableWidgetItem *item, int daysLeft) const;

    // Builds the "⚠ Expiring Soon (≤N days)" checkbox next to the
    // category filter and wires it to onExpiringSoonToggled(). Call
    // this from a subclass's setupExtraUi() to turn the filter on for
    // that page (Product and ProductStaff both do; FrontProduct and
    // ProductRecycleBin don't, so they simply never show the checkbox).
    void setupExpiringSoonFilter();

    // ── Optional extension point for subclass-only widgets/signals ────
    // (e.g. ProductStaff's "Add Product" button, the expiry checkbox)
    virtual void setupExtraUi() {}
    virtual void connectExtraSignals() {}

    // ── Call once, at the end of the subclass constructor ─────────────
    void initializeCommonUi();

    // ── Shared building blocks, reusable by subclasses ─────────────────
    void    loadProducts();
    void    populateCategoryFilter();
    QString currentCategoryFilter() const;

    QList<ProductRecord> fetchProducts(const QString &search, const QString &category,
                                        int limit, int offset,
                                        bool expiringSoonOnly) const;
    int  countProducts(const QString &search, const QString &category,
                        bool expiringSoonOnly) const;

    // ── Recycle Bin DB operations ───────────────────────────────
    // Delete no longer removes the row: it soft-deletes (is_deleted = 1,
    // deleted_at = now) so the product moves to the Recycle Bin and can
    // still be restored. Only permanentDeleteProductFromDb() actually
    // removes the row from the table.
    bool softDeleteProductFromDb(int id) const;
    bool restoreProductFromDb(int id) const;
    bool permanentDeleteProductFromDb(int id) const;

    // Soft-deletes (moves to Recycle Bin) every active product whose
    // expiry_date is a valid, already-passed date. Products with no
    // expiry_date set (NULL/empty) are never matched by this query, so
    // they never auto-expire — only delete/restore ever touches them.
    // Shared by Product and ProductStaff so both stay in sync with the
    // same Recycle Bin. Returns the number of products auto-removed.
    int autoRemoveExpiredProducts() const;

    static ProductRecord fetchById(int id);

    void setRowData(int row, const ProductRecord &p);
    void updateStatusBar(int shownCount);
    void updatePagerControls();

    // Pagination state — protected (encapsulated: not exposed publicly),
    // shared by both subclasses so paging behaves identically everywhere.
    static const int PAGE_SIZE = 10;
    int m_currentPage = 0;   // zero-indexed
    int m_totalCount  = 0;

    // Owned by whichever subclass calls setupExpiringSoonFilter(); stays
    // nullptr (and expiringSoonFilterActive() stays false) otherwise.
    QCheckBox *m_chkExpiringSoon = nullptr;

protected slots:
    void onSearchChanged(const QString &text);
    void onFilterCategoryChanged(int index);
    void onClearSearch();
    void onNextPage();
    void onPrevPage();
    void onDeleteProduct();          // active list → soft-delete (send to Recycle Bin)
    void onRestoreProduct();         // Recycle Bin → active list
    void onPermanentDeleteProduct(); // Recycle Bin → gone for good
    void onExpiringSoonToggled(bool checked);
};

#endif // PRODUCTBASE_H
