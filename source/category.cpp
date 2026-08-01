#include "../include/category.h"
#include "../ui/ui_category.h"
#include "../include/addcategory.h"
#include <QDebug>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox> // Added to support confirmation boxes
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDialog>       // For the inline edit dialog
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>

// ── Column indices ─────────────────────────────────────────────────────────
// 0: ID  1: CATEGORY NAME  2: DESCRIPTION  3: ACTIONS
// (matches the <column> order declared in category.ui)
static constexpr int COL_ID      = 0;
static constexpr int COL_NAME    = 1;
static constexpr int COL_DESC    = 2;
static constexpr int COL_ACTIONS = 3;

category::category(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::category)
{
    ui->setupUi(this);
    setWindowTitle("Sajilo-Bazar Categories Management");

    // NOTE: Column count, header labels, row height, sorting, colors and
    // all other static appearance now live entirely in category.ui. The
    // one exception Qt Designer genuinely cannot express for a QTableWidget
    // is per-column resize MODE (Stretch/Fixed/Interactive) - that is why
    // this one block remains here, mirroring the identical pattern already
    // used in staffperform.cpp for the same reason.
    auto *header = ui->categoryTableWidget->horizontalHeader();
    header->setSectionResizeMode(COL_ID,      QHeaderView::Fixed);
    header->setSectionResizeMode(COL_NAME,    QHeaderView::Stretch);
    header->setSectionResizeMode(COL_DESC,    QHeaderView::Stretch);
    header->setSectionResizeMode(COL_ACTIONS, QHeaderView::Fixed);
    header->resizeSection(COL_ID,      60);
    header->resizeSection(COL_ACTIONS, 170);


    connect(ui->backToDashboardBtn, &QPushButton::clicked, this, &category::backToDashboardRequested);

    loadCategoriesTable();
}

category::~category()
{
    delete ui;
}

void category::loadCategoriesTable()
{
    QSqlDatabase db = QSqlDatabase::database(); // default connection opened in main()
    if (!db.isOpen()) {
        QMessageBox::critical(this, "Database Error", "No open database connection.");
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, category_name, description FROM categories ORDER BY id ASC");

    if (!query.exec()) {
        QMessageBox::critical(this, "Database Error",
                              "Failed to load categories: " + query.lastError().text());
        return;
    }

    // Sorting interferes with row insertion indices while we populate, so
    // disable it during the fill and re-enable once everything is in place.
    ui->categoryTableWidget->setSortingEnabled(false);
    ui->categoryTableWidget->setRowCount(0);

    int row = 0;
    while (query.next()) {
        const int id          = query.value(0).toInt();
        const QString name    = query.value(1).toString();
        const QString desc    = query.value(2).toString();

        ui->categoryTableWidget->insertRow(row);

        // Column 0: ID (real database id, stored in UserRole for delete/edit lookups)
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(id));
        idItem->setTextAlignment(Qt::AlignCenter);
        idItem->setData(Qt::UserRole, id);
        ui->categoryTableWidget->setItem(row, COL_ID, idItem);

        // Column 1: Category Name
        ui->categoryTableWidget->setItem(row, COL_NAME, new QTableWidgetItem(name));

        // Column 2: Description
        ui->categoryTableWidget->setItem(row, COL_DESC, new QTableWidgetItem(desc));

        // Column 3: Actions Container (Edit + Delete Buttons)
        // Colors/hover/radius for these buttons come entirely from
        // category.ui's QPushButton[actionRole="edit"/"delete"] rules -
        // this code only assigns WHAT each button is, same convention as
        // staffperform.cpp's buildActionsWidget.
        QWidget *actionContainer = new QWidget();
        QHBoxLayout *layout = new QHBoxLayout(actionContainer);
        layout->setContentsMargins(6, 2, 6, 2);
        layout->setSpacing(6);

        // --- Edit Button ---
        // IMPORTANT: the category id is attached directly to the button via
        // setProperty(), and the click handler reads it back via sender().
        // This is what makes the row resolution correct regardless of
        // sorting, scrolling, or row position — pos()-based lookups are
        // unreliable because pos() is relative to the button's own parent
        // (the small action-container widget), not the table viewport.
        QPushButton *editBtn = new QPushButton("Edit");
        editBtn->setProperty("categoryId", id);
        editBtn->setProperty("actionRole", "edit");
        editBtn->setCursor(Qt::PointingHandCursor);
        connect(editBtn, &QPushButton::clicked, this, &category::editCategory);
        layout->addWidget(editBtn);

        // --- Delete Button ---
        // Same fix applied here: id stored via setProperty(), resolved via
        // sender() inside deleteCategory().
        QPushButton *deleteBtn = new QPushButton("Delete");
        deleteBtn->setProperty("categoryId", id);
        deleteBtn->setProperty("actionRole", "delete");
        deleteBtn->setCursor(Qt::PointingHandCursor);
        connect(deleteBtn, &QPushButton::clicked, this, &category::deleteCategory);
        layout->addWidget(deleteBtn);

        actionContainer->setLayout(layout);
        ui->categoryTableWidget->setCellWidget(row, COL_ACTIONS, actionContainer);

        ++row;
    }

    ui->categoryTableWidget->setSortingEnabled(true);
}

void category::editCategory()
{
    // Identify which row this came from via the button that sent the
    // signal, not via screen-position math (see comment in
    // loadCategoriesTable() for why that was unreliable).
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn)
        return;

    const int categoryId = btn->property("categoryId").toInt();
    if (categoryId <= 0)
        return;

    // Re-fetch the current name/description straight from the database
    // rather than trusting whatever text happens to be sitting in the
    // table widget at click time. This keeps the edit dialog correct even
    // if the table is ever showing stale data for any reason.
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery fetch(db);
    fetch.prepare("SELECT category_name, description FROM categories WHERE id = :id");
    fetch.bindValue(":id", categoryId);

    if (!fetch.exec() || !fetch.next()) {
        QMessageBox::critical(this, "Database Error",
                              "Failed to load category: " + fetch.lastError().text());
        return;
    }

    const QString currentName = fetch.value(0).toString();
    const QString currentDesc = fetch.value(1).toString();

    // Build a small modal dialog for editing name and description
    QDialog *editDialog = new QDialog(this);
    editDialog->setWindowTitle("Edit Category");
    editDialog->setMinimumWidth(360);
    editDialog->setStyleSheet(
        "QDialog {"
        "   background-color: #ffffff;"
        "   border-radius: 8px;"
        "}"
        "QLabel {"
        "   color: #1e293b;"
        "   font-size: 13px;"
        "   font-weight: 600;"
        "   border: none;"
        "}"
        "QLineEdit {"
        "   border: 1px solid #cbd5e1;"
        "   border-radius: 6px;"
        "   padding: 6px 10px;"
        "   font-size: 13px;"
        "   color: #334155;"
        "   background-color: #f8fafc;"
        "}"
        "QLineEdit:focus {"
        "   border: 1px solid #660033;"
        "   background-color: #ffffff;"
        "}"
        "QPushButton {"
        "   background-color: #f1f5f9;"
        "   color: #334155;"
        "   border: 1px solid #cbd5e1;"
        "   border-radius: 6px;"
        "   padding: 6px 18px;"
        "   font-weight: bold;"
        "   font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #e2e8f0;"
        "}"
        /* Style the Save/OK button in the shared maroon accent */
        "QPushButton[text=\"Save\"] {"
        "   background-color: #660033;"
        "   color: white;"
        "   border: none;"
        "}"
        "QPushButton[text=\"Save\"]:hover {"
        "   background-color: #4d0026;"
        "}"
        );

    QVBoxLayout *mainLayout = new QVBoxLayout(editDialog);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QFormLayout *form = new QFormLayout();
    form->setSpacing(10);

    QLineEdit *nameEdit = new QLineEdit(currentName, editDialog);
    nameEdit->setPlaceholderText("Enter category name");

    QLineEdit *descEdit = new QLineEdit(currentDesc, editDialog);
    descEdit->setPlaceholderText("Enter description");

    form->addRow("Category Name:", nameEdit);
    form->addRow("Description:", descEdit);
    mainLayout->addLayout(form);

    QDialogButtonBox *btnBox = new QDialogButtonBox(editDialog);
    QPushButton *saveBtn   = btnBox->addButton("Save",   QDialogButtonBox::AcceptRole);
    QPushButton *cancelBtn = btnBox->addButton("Cancel", QDialogButtonBox::RejectRole);
    Q_UNUSED(cancelBtn);
    mainLayout->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, editDialog, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, editDialog, &QDialog::reject);
    Q_UNUSED(saveBtn);

    if (editDialog->exec() == QDialog::Accepted) {
        const QString newName = nameEdit->text().trimmed();
        const QString newDesc = descEdit->text().trimmed();

        if (newName.isEmpty()) {
            QMessageBox::warning(this, "Validation", "Category name cannot be empty.");
            delete editDialog;
            return;
        }

        QSqlQuery updateQuery(db);
        updateQuery.prepare(
            "UPDATE categories SET category_name = :name, description = :desc WHERE id = :id"
            );
        updateQuery.bindValue(":name", newName);
        updateQuery.bindValue(":desc", newDesc);
        updateQuery.bindValue(":id",   categoryId);

        if (!updateQuery.exec()) {
            QMessageBox::critical(this, "Database Error",
                                  "Failed to update category: " + updateQuery.lastError().text());
        } else {
            // Any page that reads categories from the database (e.g. the
            // Product page's category dropdown/filter) will pick up this
            // change automatically the next time it's opened, since those
            // pages query the categories table fresh on open rather than
            // relying on a hardcoded list.
            loadCategoriesTable();
        }
    }

    delete editDialog;
}

void category::deleteCategory()
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn)
        return;

    const int categoryId = btn->property("categoryId").toInt();
    if (categoryId <= 0)
        return;

    // 1. Create the message box instance manually
    QMessageBox msgBox(QMessageBox::Question,
                       "Delete Confirmation",
                       "Are you sure you want to delete this?",
                       QMessageBox::Yes | QMessageBox::No,
                       this);

    // 2. Apply custom CSS styling to the dialog window components
    msgBox.setStyleSheet(
        "QMessageBox {"
        "   background-color: #ffffff;"
        "   border: 1px solid #f0dde1;"
        "   border-radius: 8px;"
        "}"
        "QLabel {"
        "   color: #1e293b;"
        "   font-size: 14px;"
        "   font-weight: 500;"
        "   padding-left: 10px;"
        "}"
        "QPushButton {"
        "   background-color: #f1f5f9;"
        "   color: #334155;"
        "   border: 1px solid #cbd5e1;"
        "   border-radius: 5px;"
        "   padding: 6px 14px;"
        "   font-weight: bold;"
        "   min-width: 65px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #e2e8f0;"
        "}"
        "QPushButton[text=\"&Yes\"], QPushButton[text=\"Yes\"] {"
        "   background-color: #8B0000;"
        "   color: white;"
        "   border: none;"
        "}"
        "QPushButton[text=\"&Yes\"]:hover, QPushButton[text=\"Yes\"]:hover {"
        "   background-color: #660000;"
        "}"
        );

    // 3. Execute the styled box and capture the answer
    const int reply = msgBox.exec();
    if (reply != QMessageBox::Yes)
        return;

    // 4. Confirmed — delete from the database and refresh the table
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery deleteQuery(db);
    deleteQuery.prepare("DELETE FROM categories WHERE id = :id");
    deleteQuery.bindValue(":id", categoryId);

    if (!deleteQuery.exec()) {
        QMessageBox::critical(this, "Database Error",
                              "Failed to delete category: " + deleteQuery.lastError().text());
        return;
    }

    loadCategoriesTable();
}

void category::on_addCategoryBtn_clicked()
{
    addcategory *popup = new addcategory(this);
    popup->setWindowModality(Qt::WindowModal);

    if (popup->exec() == QDialog::Accepted) {
        QString name = popup->getCategoryName();
        QString desc = popup->getDescription();

        QSqlDatabase db = QSqlDatabase::database();
        QSqlQuery insertQuery(db);
        insertQuery.prepare("INSERT INTO categories (category_name, description) VALUES (:name, :desc)");
        insertQuery.bindValue(":name", name);
        insertQuery.bindValue(":desc", desc);

        if (!insertQuery.exec()) {
            QMessageBox::critical(this, "Database Error",
                                  "Failed to add category: " + insertQuery.lastError().text());
        } else {
            // This INSERT is the database modification you described:
            // adding a category here writes straight into the shared
            // "categories" table. The Product page doesn't need to be told
            // about it directly — it queries that same table fresh every
            // time it's opened (see Product::loadCategoriesFromDb()), so
            // the new category (e.g. "Sweets") will simply appear in its
            // Add/Edit dropdown and its "All Categories" filter the next
            // time that page is opened.
            loadCategoriesTable();
        }
    }

    delete popup;
}