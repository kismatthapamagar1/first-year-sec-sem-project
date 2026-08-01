#include "../include/frontdesk.h"
#include "../ui/ui_frontdesk.h"
#include "../include/billing.h"
#include "../include/frontproduct.h"
#include "../include/frontprofile.h"
#include "../include/stockservice.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDate>
#include <QDateTime>
#include <QFrame>
#include <QLabel>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QPainter>
#include <QPainterPath>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSizePolicy>

frontdesk::frontdesk(int staffId, const QString &staffName, QWidget *parent)
    : ClassLogout(parent)
    , ui(new Ui::frontdesk)
    , m_staffId(staffId)
    , m_staffName(staffName)
{
    ui->setupUi(this);

    connect(ui->btnBilling,     &QPushButton::clicked, this, &frontdesk::openBillingWindow);
    connect(ui->btnViewProducts, &QPushButton::clicked, this, &frontdesk::openViewProductsWindow);

    connect(ui->btnProfile,
            &QPushButton::clicked,
            this,
            &frontdesk::handleProfileClicked);

    connect(ui->btnLogout,
            &QPushButton::clicked,
            this,
            &frontdesk::handleLogout_clicked);

    setupProductTable();
    loadCurrentUserInfo();
    loadDashboardStats();
}

frontdesk::~frontdesk()
{
    delete ui;
}

void frontdesk::openViewProductsWindow()
{
    FrontProduct *viewProductsPage = new FrontProduct();
    viewProductsPage->setAttribute(Qt::WA_DeleteOnClose);
    viewProductsPage->show();
}

void frontdesk::openBillingWindow()
{
    // FIX: was `new Billing()` — the default constructor — which carries
    // no staffId/staffName at all. Since this window is closed (and, being
    // WA_DeleteOnClose, destroyed) right after, Billing::backToDashboard()
    // had nothing to reconstruct the dashboard from and produced a blank
    // "no user logged in" dashboard. Now the current user's identity is
    // passed straight into Billing so it can hand it back later.
    Billing *billingWindow = new Billing(m_staffId, m_staffName);
    billingWindow->setAttribute(Qt::WA_DeleteOnClose);
    billingWindow->show();
    this->close();
}

void frontdesk::loadCurrentUserInfo()
{
    if (m_staffId < 0) {
        qWarning() << "frontdesk: no staffId supplied, cannot personalize dashboard";
        ui->welcomeLabel->setText(m_staffName.isEmpty()
                                      ? QStringLiteral("Welcome back, Front Desk!")
                                      : QStringLiteral("Welcome back, %1!").arg(m_staffName));
        return;
    }

    QSqlQuery q;
    q.prepare("SELECT first_name, last_name, username, picture "
              "FROM information WHERE id = :id");
    q.bindValue(":id", m_staffId);

    QString firstName;
    QString lastName;
    QString picture;

    if (q.exec() && q.next()) {
        firstName  = q.value(0).toString();
        lastName   = q.value(1).toString();
        m_username = q.value(2).toString();
        picture    = q.value(3).toString();

        const QString freshFullName = QStringLiteral("%1 %2").arg(firstName, lastName).trimmed();
        if (!freshFullName.isEmpty())
            m_staffName = freshFullName;
    } else {
        qWarning() << "frontdesk: could not load user info for staffId"
                   << m_staffId << "-" << q.lastError().text();
    }

    ui->welcomeLabel->setText(
        firstName.isEmpty() ? QStringLiteral("Welcome back, %1!").arg(
                                  m_staffName.isEmpty() ? QStringLiteral("Front Desk") : m_staffName)
                            : QStringLiteral("Welcome back, %1!").arg(firstName));

    QPixmap avatar;
    bool loaded = false;

    if (!picture.isEmpty()) {
        const QString diskPath =
            QStringLiteral("C:/Users/ASUS/Desktop/Sajilo-Bazarr/resources/staff_photos/") + picture;

        if (QFile::exists(diskPath))
            loaded = avatar.load(diskPath);

        if (!loaded)
            loaded = avatar.load(":/staff_photos/" + picture);
    }

    if (!loaded) {
        QString initials;
        if (!firstName.isEmpty()) initials += firstName.at(0).toUpper();
        if (!lastName.isEmpty())  initials += lastName.at(0).toUpper();
        if (initials.isEmpty())   initials = "?";

        avatar = QPixmap(AVATAR_DIAMETER, AVATAR_DIAMETER);
        avatar.fill(Qt::transparent);

        QPainter p(&avatar);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#660033"));
        p.drawEllipse(0, 0, AVATAR_DIAMETER, AVATAR_DIAMETER);

        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(AVATAR_DIAMETER / 2);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(avatar.rect(), Qt::AlignCenter, initials);
    }

    ui->btnProfile->setIcon(QIcon(makeCircularPixmap(avatar, AVATAR_DIAMETER)));
    ui->btnProfile->setIconSize(QSize(AVATAR_DIAMETER, AVATAR_DIAMETER));
}

QPixmap frontdesk::makeCircularPixmap(const QPixmap &source, int diameter)
{
    QPixmap scaled = source.scaled(diameter, diameter,
                                   Qt::KeepAspectRatioByExpanding,
                                   Qt::SmoothTransformation);

    QPixmap circular(diameter, diameter);
    circular.fill(Qt::transparent);

    QPainter painter(&circular);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath clipPath;
    clipPath.addEllipse(0, 0, diameter, diameter);
    painter.setClipPath(clipPath);

    const int x = (diameter - scaled.width())  / 2;
    const int y = (diameter - scaled.height()) / 2;
    painter.drawPixmap(x, y, scaled);

    return circular;
}

void frontdesk::handleProfileClicked()
{
    FrontProfile *profileWindow = new FrontProfile(m_username, this);
    profileWindow->setAttribute(Qt::WA_DeleteOnClose);
    profileWindow->exec();
}

// ---------------------------------------------------------------------
// Builds a single stat card (title + big value) as a QFrame whose
// internal layout (QVBoxLayout) positions its own labels. No setGeometry
// anywhere — sizing is left entirely to whatever layout this card gets
// added to by the caller, which is what keeps it in sync with the rest
// of the page.
// ---------------------------------------------------------------------
QFrame *frontdesk::createStatCard(QWidget *parent, const QString &title,
                                  const QString &value, const QString &accentColor)
{
    QFrame *card = new QFrame(parent);
    card->setMinimumHeight(76);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    card->setStyleSheet(QString(
                            "QFrame { background-color: #ffffff; border: 1px solid #e5e7eb; "
                            "border-left: 4px solid %1; border-radius: 4px; }"
                            ).arg(accentColor));

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 14, 12, 14);
    cardLayout->setSpacing(4);

    QLabel *titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet(
        "QLabel { color: #6b7280; font-size: 12px; font-weight: 500; background: transparent; border: none; }");
    titleLabel->setWordWrap(true);

    QLabel *valueLabel = new QLabel(value, card);
    valueLabel->setObjectName("statValueLabel");
    valueLabel->setStyleSheet(
        "QLabel { color: #1a2a4a; font-size: 24px; font-weight: 700; background: transparent; border: none; }");

    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(valueLabel);
    cardLayout->addStretch();

    return card;
}

// ---------------------------------------------------------------------
// Creates the 4 stat cards as ONE row widget and inserts that row
// directly into verticalLayout_main — the same layout that already
// owns titleLabel and productTable. This replaces the old Expanding
// spacer (mainTopSpacer, originally item index 1: 0=titleLabel,
// 1=mainTopSpacer, 2=productTable) with the card row plus a small
// FIXED spacer, so the gap above the table is always exactly 16px
// instead of whatever leftover space the old Expanding spacer soaked up.
// ---------------------------------------------------------------------
void frontdesk::loadDashboardStats()
{
    QWidget *cardRow = new QWidget(ui->mainFrame);
    QHBoxLayout *cardRowLayout = new QHBoxLayout(cardRow);
    cardRowLayout->setContentsMargins(0, 0, 0, 0);
    cardRowLayout->setSpacing(16);

    QFrame *cardItems    = createStatCard(cardRow, "Items Sold Today", "0", "#3b82f6");
    QFrame *cardSales    = createStatCard(cardRow, "Sales Total Today", "Rs. 0", "#f59e0b");
    QFrame *cardProducts = createStatCard(cardRow, "Unique Products Sold", "0", "#8b5cf6");
    QFrame *cardLowStock = createStatCard(cardRow, "Low Stock Items", "0", "#ef4444");

    cardRowLayout->addWidget(cardItems,    1);
    cardRowLayout->addWidget(cardSales,    1);
    cardRowLayout->addWidget(cardProducts, 1);
    cardRowLayout->addWidget(cardLowStock, 1);

    // Remove the old Expanding spacer that used to sit between the
    // title and the table (this is what caused the uneven gap).
    QLayoutItem *oldSpacer = ui->verticalLayout_main->takeAt(1);
    delete oldSpacer;

    ui->verticalLayout_main->insertWidget(1, cardRow);
    ui->verticalLayout_main->insertSpacerItem(
        2, new QSpacerItem(20, 16, QSizePolicy::Minimum, QSizePolicy::Fixed));

    lblItemsSoldValue    = cardItems->findChild<QLabel *>("statValueLabel");
    lblSalesTotalValue   = cardSales->findChild<QLabel *>("statValueLabel");
    lblProductsSoldValue = cardProducts->findChild<QLabel *>("statValueLabel");
    lblLowStockValue     = cardLowStock->findChild<QLabel *>("statValueLabel");

    const QString today = QDate::currentDate().toString("yyyy-MM-dd");

    // --- Items sold today + sales total today ---
    {
        QSqlQuery q;
        q.prepare(
            "SELECT COALESCE(SUM(quantity_sold), 0), "
            "       COALESCE(SUM(quantity_sold * unit_price), 0), "
            "       COUNT(DISTINCT product_id) "
            "FROM sales "
            "WHERE date(sale_date) = date(:today)");
        q.bindValue(":today", today);

        if (q.exec() && q.next()) {
            double itemsSold    = q.value(0).toDouble();
            double salesTotal   = q.value(1).toDouble();
            int    productsSold = q.value(2).toInt();

            if (lblItemsSoldValue)
                lblItemsSoldValue->setText(QString::number(itemsSold, 'f', 0));
            if (lblSalesTotalValue)
                lblSalesTotalValue->setText(QString("Rs. %1").arg(salesTotal, 0, 'f', 2));
            if (lblProductsSoldValue)
                lblProductsSoldValue->setText(QString::number(productsSold));
        } else {
            qDebug() << "Dashboard: sales stats query failed:" << q.lastError().text();
        }
    }

    // --- Low stock count (single source of truth: StockService) ---
    {
        InventoryStats stats = StockService::computeStats();
        if (lblLowStockValue)
            lblLowStockValue->setText(QString::number(stats.low));
    }

    loadRecentSales();
}

void frontdesk::setupProductTable()
{
    ui->productTable->setColumnCount(4);
    ui->productTable->setHorizontalHeaderLabels(
        {"Product", "Qty", "Amount", "Date / Time"});
    ui->productTable->horizontalHeader()->setStretchLastSection(true);
    ui->productTable->verticalHeader()->setVisible(false);
    ui->productTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->productTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->productTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->productTable->setAlternatingRowColors(true);
}

void frontdesk::loadRecentSales()
{
    QSqlQuery q;
    q.prepare(
        "SELECT product_name, quantity_sold, (quantity_sold * unit_price), sale_date "
        "FROM sales "
        "ORDER BY sale_date DESC "
        "LIMIT 10");

    if (!q.exec()) {
        qDebug() << "Dashboard: recent sales query failed:" << q.lastError().text();
        ui->productTable->setRowCount(0);
        return;
    }

    ui->productTable->setRowCount(0);
    int row = 0;
    while (q.next()) {
        ui->productTable->insertRow(row);

        QString productName = q.value(0).toString();
        double  qty          = q.value(1).toDouble();
        double  amount       = q.value(2).toDouble();
        QString saleDate     = q.value(3).toString();

        ui->productTable->setItem(row, 0, new QTableWidgetItem(productName));
        ui->productTable->setItem(row, 1, new QTableWidgetItem(QString::number(qty, 'f', 0)));
        ui->productTable->setItem(row, 2, new QTableWidgetItem(QString("Rs. %1").arg(amount, 0, 'f', 2)));
        ui->productTable->setItem(row, 3, new QTableWidgetItem(saleDate));

        ++row;
    }

    if (row == 0) {
        ui->productTable->setRowCount(1);
        QTableWidgetItem *empty = new QTableWidgetItem("No sales recorded yet");
        ui->productTable->setItem(0, 0, empty);
        ui->productTable->setSpan(0, 0, 1, 4);
    }
}