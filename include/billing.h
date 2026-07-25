#ifndef BILLING_H
#define BILLING_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class Billing;
}
QT_END_NAMESPACE

class Billing : public QMainWindow
{
    Q_OBJECT

public:
    explicit Billing(QWidget *parent = nullptr);
    ~Billing();

private slots:
    void onBarcodeScanned();   // fired when Enter is pressed in barcodeInput
    void generateBill();       // "Generate Bill" button
    void backToDashboard();    // "Back to Dashboard" button

private:
    Ui::Billing *ui;

    void addProductToBill(const QString &code);
    void recalcRowPrice(int row);
    void recalcTotal();
    void setupNameCompleter();

    enum BillColumn {
        ColSku = 0,
        ColName,
        ColUnit,
        ColUnitPrice,
        ColStock,
        ColQty,
        ColPrice,
        ColRemove
    };
};

#endif