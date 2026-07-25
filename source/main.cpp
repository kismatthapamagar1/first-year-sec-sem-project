#include <QApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVector>
#include <QDebug>
#include "../include/login.h"
#include "../include/reportdb.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Database seed / schema (runs once at startup)
// ─────────────────────────────────────────────────────────────────────────────

namespace {
const char *const kConnectionName = "bazar_init_connection";

const QVector<QString> kStatements = {
    //categories
    QStringLiteral(R"sql(
    CREATE TABLE IF NOT EXISTS categories (
        id            INTEGER PRIMARY KEY AUTOINCREMENT,
        category_name TEXT,
        description   TEXT
    );
    )sql"),

    QStringLiteral(R"sql(
    INSERT OR IGNORE INTO categories (id, category_name, description) VALUES
    (1,  'Groceries',  'Daily grocery products and essential kitchen items'),
    (2,  'Vegetables', 'Fresh vegetables supplied daily'),
    (3,  'Fruits',     'Fresh seasonal fruits and imported fruits'),
    (4,  'Dairy',      'Milk products and dairy related items'),
    (5,  'Cosmetics',  'Beauty and personal care products'),
    (6,  'Cleaning',   'Cleaning and sanitation products'),
    (7,  'Household',  'Daily usable household products'),
    (8,  'Snacks',     'Quick snack and packaged food items'),
    (9,  'Beverages',  'Cold drinks, tea, coffee and juices'),
    (10, 'Bakery',     'Fresh bakery products'),
    (11, 'Stationery', 'Office and school stationery items');
    )sql"),

    // ── information (users / staff) ──────────────────────────────
    QStringLiteral(R"sql(
    CREATE TABLE IF NOT EXISTS information (
        id         INTEGER PRIMARY KEY AUTOINCREMENT,
        first_name TEXT,
        last_name  TEXT,
        username   TEXT UNIQUE,
        role       TEXT NOT NULL CHECK(role IN ('admin','staff','frontdesk')),
        email      TEXT,
        phone      TEXT,
        picture    TEXT,
        password   TEXT,
        status     TEXT DEFAULT 'enabled' CHECK(status IN ('enabled','disabled','expired'))
    );
    )sql"),

    QStringLiteral(R"sql(
    INSERT OR IGNORE INTO information
        (id, first_name, last_name, username, role, email, phone, picture, password, status)
    VALUES
    (1,  'Anushka',      'Sigdel',     'Anushka_Sigdel_Admin',           'admin',     'anushkasigdel50@gmail.com', '9761468969', 'anushka.jpg',  '202cb962ac59075b964b07152d234b70', 'enabled'),
    (2,  'Suyeshna',     'Shrestha',   'Suyeshna_Shrestha_Staff',        'staff',     'suyeshna@gmail.com',        '9801111111', 'suyeshna.jpg', '202cb962ac59075b964b07152d234b70', 'enabled'),
    (3,  'Kritika',      'Gurung',     'Kritika_Gurung_Staff',           'staff',     'kritika@gmail.com',         '9802222222', 'kritika.jpg',  '202cb962ac59075b964b07152d234b70', 'enabled'),
    (4,  'Milan',        'Tamang',     'Milan_Tamang_Staff',             'staff',     'milan@gmail.com',           '9803333333', 'milan.jpg',    '202cb962ac59075b964b07152d234b70', 'enabled'),
    (5,  'Ritika',       'Karki',      'Ritika_Karki_Staff',             'staff',     'ritika@gmail.com',          '9804444444', 'ritika.jpg',   '202cb962ac59075b964b07152d234b70', 'enabled'),
    (6,  'Roshan',       'Basnet',     'Roshan_Basnet_Staff',            'staff',     'roshan@gmail.com',          '9805555555', 'roshan.jpg',   '202cb962ac59075b964b07152d234b70', 'enabled'),
    (7,  'Prisha',       'Adhikari',   'Prisha_Adhikari_Frontdesk',      'frontdesk', 'prisha@gmail.com',          '9811111111', 'prisha.jpg',   '202cb962ac59075b964b07152d234b70', 'enabled'),
    (8,  'Aayush',       'Thapa',      'Aayush_Thapa_Frontdesk',         'frontdesk', 'aayush@gmail.com',          '9812222222', 'aayush.jpg',   '202cb962ac59075b964b07152d234b70', 'enabled'),
    (9,  'Shreekrishna', 'Mishra',     'shreekrishna_mishra_staff',      'staff',     'skjfksj@gmail.com',         '9849803850', NULL,           '202cb962ac59075b964b07152d234b70', 'enabled'),
    (10, 'Anushka',      'Sigdel',     'anushka_sigdel_frontdesk',       'frontdesk', 'gjhvjhg@gmail.com',         '646858758',  NULL,           '202cb962ac59075b964b07152d234b70', 'enabled'),
    (11, 'Kismat',       'ThapaMagar', 'kismat_thapamagar_staff',        'staff',     'skjfksj@gmail.com',         '9849803850', NULL,           '202cb962ac59075b964b07152d234b70', 'enabled');
    )sql"),

    // ── products ─────────────────────────────────────────────────
    QStringLiteral(R"sql(
    CREATE TABLE IF NOT EXISTS products (
        id           INTEGER PRIMARY KEY AUTOINCREMENT,
        product_name TEXT,
        category     TEXT,
        unit         TEXT,
        price        REAL,
        stock        INTEGER,
        expiry_date  TEXT,
        status       TEXT,
        supplier     TEXT,
        sku          TEXT
    );
    )sql"),

    // NOTE on expiry_date below: perishable rows use date('now', '+N days')
    // so the seed data is always realistic relative to whenever the app is
    // first run (a handful land inside the default 5-day warning window,
    // one is already overdue, the rest sit further out) - static dates would
    // silently drift into the past and make every perishable item vanish
    // into the Recycle Bin (via autoRemoveExpiredProducts) before anyone
    // ever saw a warning. Genuinely non-perishable categories (Household,
    // Stationery, Music) get expiry_date = NULL instead: ProductBase never
    // matches an empty expiry_date in its "expiring soon" filter or its
    // auto-expire query, so these items are excluded from the whole expiry
    // system, exactly like a staff member ticking "Does not expire" would
    // exclude them by hand.
    QStringLiteral(R"sql(
    INSERT OR IGNORE INTO products
        (id, product_name, category, unit, price, stock, expiry_date, status, supplier, sku)
    VALUES
    (1,  'Rice',         'Grains',     'kg',     90.00,  5,   '2027-01-10',                 'Low Stock',  'Nepal Agro',        'SKU001'),
    (2,  'Sugar',        'Groceries',  'kg',     120.00, 80,  '2027-05-01',                 'In Stock',   'Sweet Suppliers',   'SKU002'),
    (3,  'Salt',         'Groceries',  'kg',     25.00,  200, '2028-03-15',                 'High Stock', 'Everest Salt',      'SKU003'),
    (4,  'Potato',       'Vegetables', 'kg',     60.00,  30,  date('now', '+2 days'),       'Low Stock',  'Fresh Farm',        'SKU004'),
    (5,  'Tomato',       'Vegetables', 'kg',     80.00,  45,  date('now', '+1 days'),       'In Stock',   'Fresh Farm',        'SKU005'),
    (6,  'Onion',        'Vegetables', 'kg',     70.00,  25,  date('now', '+16 days'),      'Low Stock',  'Veggie Nepal',      'SKU006'),
    (7,  'Milk',         'Dairy',      'litre',  110.00, 60,  date('now'),                  'In Stock',   'Dairy Nepal',       'SKU007'),
    (8,  'Cheese',       'Dairy',      'pieces', 350.00, 15,  date('now', '+21 days'),      'Low Stock',  'Dairy Nepal',       'SKU008'),
    (9,  'Butter',       'Dairy',      'pieces', 250.00, 40,  date('now', '+7 days'),       'In Stock',   'Dairy Nepal',       'SKU009'),
    (10, 'Yogurt',       'Dairy',      'pieces', 90.00,  65,  date('now', '+4 days'),       'In Stock',   'Fresh Dairy',       'SKU010'),
    (11, 'Toothpaste',   'Cosmetics',  'pieces', 140.00, 90,  '2028-01-01',                 'High Stock', 'Colgate Nepal',     'SKU011'),
    (12, 'Soap',         'Cosmetics',  'pieces', 60.00,  150, '2028-01-01',                 'High Stock', 'Clean Nepal',       'SKU012'),
    (13, 'Shampoo',      'Cosmetics',  'bottle', 450.00, 70,  '2027-10-10',                 'In Stock',   'Beauty Nepal',      'SKU013'),
    (14, 'Face Wash',    'Cosmetics',  'pieces', 320.00, 20,  '2027-11-12',                 'Low Stock',  'Glow Nepal',        'SKU014'),
    (15, 'Body Lotion',  'Cosmetics',  'bottle', 500.00, 55,  '2027-12-01',                 'In Stock',   'Beauty Nepal',      'SKU015'),
    (16, 'Dish Wash',    'Cleaning',   'bottle', 180.00, 35,  '2027-09-09',                 'In Stock',   'Sparkle Suppliers', 'SKU016'),
    (17, 'Detergent',    'Cleaning',   'kg',     220.00, 95,  '2027-07-07',                 'High Stock', 'Sparkle Suppliers', 'SKU017'),
    (18, 'Floor Cleaner','Cleaning',   'bottle', 300.00, 45,  '2027-09-01',                 'In Stock',   'Clean Nepal',       'SKU018'),
    (19, 'Glass Cleaner','Cleaning',   'bottle', 280.00, 15,  '2027-12-12',                 'Low Stock',  'Sparkle Suppliers', 'SKU019'),
    (21, 'Bucket',       'Household',  'pieces', 250.00, 60,  NULL,                         'In Stock',   'Home Nepal',        'SKU021'),
    (22, 'Plate',        'Household',  'pieces', 120.00, 85,  NULL,                         'High Stock', 'Kitchen Nepal',     'SKU022'),
    (23, 'Cup',          'Household',  'pieces', 100.00, 100, NULL,                         'High Stock', 'Kitchen Nepal',     'SKU023'),
    (24, 'Biscuit',      'Snacks',     'pieces', 30.00,  170, date('now', '+3 days'),       'High Stock', 'Snack Nepal',       'SKU024'),
    (25, 'Chips',        'Snacks',     'pieces', 50.00,  130, date('now', '+23 days'),      'High Stock', 'Snack Nepal',       'SKU025'),
    (26, 'Noodles',      'Snacks',     'pieces', 25.00,  220, '2027-01-01',                 'High Stock', 'Wai Wai',           'SKU026'),
    (27, 'Juice',        'Beverages',  'bottle', 120.00, 65,  '2026-10-01',                 'In Stock',   'Juice Nepal',       'SKU027'),
    (28, 'Coke',         'Beverages',  'bottle', 110.00, 140, '2027-01-01',                 'High Stock', 'Coca Cola Nepal',   'SKU028'),
    (29, 'Fanta',        'Beverages',  'bottle', 110.00, 120, '2027-01-01',                 'High Stock', 'Coca Cola Nepal',   'SKU029'),
    (30, 'Sprite',       'Beverages',  'bottle', 110.00, 115, '2027-01-01',                 'High Stock', 'Coca Cola Nepal',   'SKU030'),
    (31, 'Tea',          'Beverages',  'box',    355.00, 40,  '2027-04-04',                 'In Stock',   'Tea Nepal',         'SKU031'),
    (32, 'Coffee',       'Beverages',  'box',    550.00, 28,  '2027-06-06',                 'Low Stock',  'Coffee House',      'SKU032'),
    (33, 'Bread',        'Bakery',     'pieces', 55.00,  22,  date('now', '+1 days'),       'Low Stock',  'Bakery Nepal',      'SKU033'),
    (34, 'Cake',         'Bakery',     'pieces', 850.00, 8,   date('now', '+5 days'),       'Low Stock',  'Bakery Nepal',      'SKU034'),
    (35, 'Eggs',         'Dairy',      'dozen',  220.00, 55,  date('now', '+5 days'),       'In Stock',   'Poultry Nepal',     'SKU035'),
    (36, 'Chicken',      'Meat',       'kg',     480.00, 35,  date('now', '+1 days'),       'In Stock',   'Fresh Meat',        'SKU036'),
    (37, 'Fish',         'Meat',       'kg',     620.00, 14,  date('now', '+2 days'),       'Low Stock',  'Fresh Meat',        'SKU037'),
    (38, 'Apple',        'Fruits',     'kg',     180.00, 50,  date('now', '+16 days'),      'In Stock',   'Fruit Nepal',       'SKU038'),
    (39, 'Banana',       'Fruits',     'dozen',  140.00, 45,  date('now', '+3 days'),       'In Stock',   'Fruit Nepal',       'SKU039'),
    (40, 'Orange',       'Fruits',     'kg',     220.00, 38,  date('now', '+11 days'),      'In Stock',   'Fruit Nepal',       'SKU040'),
    (41, 'Watermelon',   'Fruits',     'pieces', 350.00, 10,  date('now', '+1 days'),       'Low Stock',  'Fruit Nepal',       'SKU041'),
    (42, 'Cooking Oil',  'Groceries',  'litre',  340.00, 90,  '2027-07-07',                 'High Stock', 'Oil Nepal',         'SKU042'),
    (43, 'Pulses',       'Groceries',  'kg',     160.00, 75,  '2027-09-09',                 'In Stock',   'Agro Nepal',        'SKU043'),
    (44, 'Flour',        'Groceries',  'kg',     85.00,  95,  '2027-10-10',                 'High Stock', 'Agro Nepal',        'SKU044'),
    (45, 'Corn Flakes',  'Breakfast',  'box',    420.00, 25,  '2027-08-08',                 'Low Stock',  'Breakfast Nepal',   'SKU045'),
    (46, 'Oats',         'Breakfast',  'box',    380.00, 32,  '2027-07-07',                 'In Stock',   'Breakfast Nepal',   'SKU046'),
    (47, 'Notebook',     'Stationery', 'pieces', 80.00,  110, NULL,                         'High Stock', 'Stationery Nepal',  'SKU047'),
    (48, 'Pen',          'Stationery', 'pieces', 20.00,  250, NULL,                         'High Stock', 'Stationery Nepal',  'SKU048'),
    (49, 'Marker',       'Stationery', 'pieces', 60.00,  70,  NULL,                         'In Stock',   'Stationery Nepal',  'SKU049'),
    (50, 'Stapler',      'Stationery', 'pieces', 180.00, 18,  NULL,                         'Low Stock',  'Stationery Nepal',  'SKU050'),
    (51, 'Guitar',       'Music',      'piece',  5000.00,25,  NULL,                         'In Stock',   'Farm',              'M3000');
    )sql"),

    // ── products: Recycle Bin support ───────────────────────────
    // is_deleted = 0/1 flags a soft-deleted (recycled) product;
    // deleted_at records when it was deleted (manually or via
    // auto-expiry). Kept as ALTER TABLE (like pending_requests.picture
    // above) so anyone with an existing bazar1.db picks these up too —
    // the "duplicate column name" case on later launches is already
    // handled as non-fatal below.
    QStringLiteral(R"sql(ALTER TABLE products ADD COLUMN is_deleted INTEGER DEFAULT 0;)sql"),
    QStringLiteral(R"sql(ALTER TABLE products ADD COLUMN deleted_at TEXT;)sql"),

    // ── suppliers ────────────────────────────────────────────────
    QStringLiteral(R"sql(
    CREATE TABLE IF NOT EXISTS suppliers (
        id                  INTEGER PRIMARY KEY AUTOINCREMENT,
        supplier_name       TEXT,
        email               TEXT,
        phone               TEXT,
        product             TEXT,
        supplied_categories TEXT
    );
    )sql"),

    QStringLiteral(R"sql(
    INSERT OR IGNORE INTO suppliers
        (id, supplier_name, email, phone, product, supplied_categories)
    VALUES
    (1, 'Fresh Farm',    'freshfarm@gmail.com', '9801111000', 'Vegetables', 'Vegetables'),
    (2, 'Dairy Nepal',   'dairy@gmail.com',     '9801111001', 'Milk',       'Dairy'),
    (3, 'Snack Nepal',   'snack@gmail.com',     '9801111002', 'Chips',      'Snacks'),
    (4, 'Fruit Nepal',   'fruit@gmail.com',     '9801111003', 'Apple',      'Fruits'),
    (5, 'Kitchen Nepal', 'kitchen@gmail.com',   '9801111004', 'Plates',     'Household');
    )sql"),

    // ── tasks ────────────────────────────────────────────────────
    QStringLiteral(R"sql(
    CREATE TABLE IF NOT EXISTS tasks (
        id               INTEGER PRIMARY KEY AUTOINCREMENT,
        staff_id         INTEGER,
        staff_name       TEXT,
        task_title       TEXT,
        task_description TEXT,
        deadline         TEXT,
        priority         TEXT,
        category         TEXT,
        status           TEXT,
        created_at       TEXT
    );
    )sql"),

    QStringLiteral(R"sql(ALTER TABLE tasks ADD COLUMN category TEXT;)sql"),
    QStringLiteral(R"sql(ALTER TABLE tasks ADD COLUMN created_at TEXT;)sql"),
    QStringLiteral(R"sql(ALTER TABLE tasks ADD COLUMN staff_id INTEGER;)sql"),

    // ── attendance (check-in / check-out log, Staff Performance page) ─
    // One row per staff/frontdesk member per calendar day. Written by
    // AttendanceManager::recordCheckIn/recordCheckOut (see login.cpp,
    // which calls these around the dashboard window's lifetime) and read
    // back by AttendanceManager::computeStats (used in staffperform.cpp)
    // to derive Present / Irregular / Absent day counts.
    QStringLiteral(R"sql(
    CREATE TABLE IF NOT EXISTS attendance (
        id             INTEGER PRIMARY KEY AUTOINCREMENT,
        staff_id       INTEGER NOT NULL,   -- FK -> information.id
        date           TEXT NOT NULL,      -- 'yyyy-MM-dd'
        check_in_time  TEXT,               -- 'HH:mm:ss'
        check_out_time TEXT,               -- 'HH:mm:ss', NULL until the dashboard window closes
        UNIQUE(staff_id, date)
    );
    )sql"),

    // ── pending_requests (self-registration queue) ───────────────
    QStringLiteral(R"sql(
    CREATE TABLE IF NOT EXISTS pending_requests (
        id           INTEGER PRIMARY KEY AUTOINCREMENT,
        first_name   TEXT NOT NULL,
        last_name    TEXT NOT NULL,
        username     TEXT NOT NULL UNIQUE,
        role         TEXT NOT NULL CHECK(role IN ('staff','frontdesk')),
        email        TEXT NOT NULL,
        phone        TEXT NOT NULL,
        picture      TEXT,
        password     TEXT NOT NULL,
        requested_at TEXT NOT NULL
    );
    )sql"),

    // Same bare-filename convention as information.picture (e.g.
    // "anushka.jpg"), resolved against the profile_pictures/ folder that
    // Register::copyPictureToStorage() writes into. Kept as its own ALTER
    // TABLE (like tasks.category/created_at/staff_id above) so it's added
    // for anyone with an existing bazar1.db from before this column
    // existed -- the "duplicate column name" case is already handled as
    // non-fatal below.
    QStringLiteral(R"sql(ALTER TABLE pending_requests ADD COLUMN picture TEXT;)sql"),

    QStringLiteral(R"sql(
    INSERT OR IGNORE INTO sqlite_sequence (name, seq) VALUES
        ('categories',       11),
        ('information',      14),
        ('products',         51),
        ('suppliers',         5),
        ('tasks',             0),
        ('pending_requests',  0),
        ('attendance',        0);
    )sql"),
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  initBazarDatabase
// ─────────────────────────────────────────────────────────────────────────────
static bool initBazarDatabase(const QString &dbPath, QString *errorMessage)
{
    if (QSqlDatabase::contains(kConnectionName))
        QSqlDatabase::removeDatabase(kConnectionName);

    bool    allOk = true;
    QString firstError;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnectionName);
        db.setDatabaseName(dbPath);

        if (!db.open()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Cannot open database '%1': %2")
                                    .arg(dbPath, db.lastError().text());
            qWarning() << "Cannot open database" << dbPath << ":" << db.lastError().text();
            allOk = false;
        } else {
            qInfo() << "Opened/created" << dbPath << "- building schema and data...";

            for (int i = 0; i < kStatements.size(); ++i) {
                QSqlQuery query(db);
                if (!query.exec(kStatements[i])) {
                    // ALTER TABLE ... ADD COLUMN statements are expected to
                    // fail once the column already exists ("duplicate
                    // column name") on subsequent app launches - that's
                    // fine, so don't treat it as a fatal schema error.
                    const QString errText = query.lastError().text();
                    if (errText.contains(QStringLiteral("duplicate column name"), Qt::CaseInsensitive))
                        continue;

                    allOk = false;
                    const QString msg = QStringLiteral("[Statement %1] SQL error: %2")
                                            .arg(i)
                                            .arg(errText);
                    qWarning().noquote() << msg;
                    if (firstError.isEmpty())
                        firstError = msg;
                }
            }
        }

        db.close();
    }

    QSqlDatabase::removeDatabase(kConnectionName);

    if (!allOk && errorMessage && errorMessage->isEmpty())
        *errorMessage = firstError;

    return allOk;
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(QStringLiteral(":/resources/logo.png")));

    // 1. Run schema / seed SQL via a temporary named connection.
    QString errorMessage;
    if (!initBazarDatabase(QStringLiteral("bazar1.db"), &errorMessage)) {
        qCritical().noquote() << "Database initialization failed:" << errorMessage;
    }

    // 2. Open the DEFAULT (unnamed) connection that every QSqlQuery in
    //    the app will use (QSqlDatabase::database() with no argument).
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(QStringLiteral("bazar1.db"));

    if (!db.open()) {
        qCritical() << "Cannot open bazar1.db:" << db.lastError().text();
        return 1;
    }

    // 2b. Create/verify report-related tables (sales, buying_price column)
    //     on the default connection.
    initReportTables();

    // 3. Show the login window.
    Login w;
    w.show();

    return a.exec();
}