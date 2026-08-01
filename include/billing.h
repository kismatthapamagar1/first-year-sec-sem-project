#ifndef BILLING_H
#define BILLING_H

#include <QMainWindow>
#include <QString>

namespace Ui {
class Billing;
}

class Billing : public QMainWindow
{
    Q_OBJECT

public:
    // Requires the logged-in frontdesk user's identity so that
    // backToDashboard() can reconstruct the correct, personalized
    // dashboard instead of a blank/anonymous one.
    explicit Billing(int staffId, const QString &staffName, QWidget *parent = nullptr);
    ~Billing();

    enum Column {
        ColSku = 0,
        ColName,
        ColUnit,
        ColUnitPrice,
        ColStock,
        ColQty,
        ColPrice,
        ColRemove
    };

private slots:
    void onBarcodeScanned();
    void generateBill();
    void backToDashboard();

private:
    Ui::Billing *ui;

    int     m_staffId;
    QString m_staffName;

    void setupNameCompleter();
    void addProductToBill(const QString &code);
    void recalcRowPrice(int row);
    void recalcTotal();
};

#endif // BILLING_H