#include "../include/stockservice.h"
#include <QSqlQuery>

InventoryStats StockService::computeStats()
{
    InventoryStats stats;
    {
        QSqlQuery q("SELECT COUNT(*) FROM products "
                    "WHERE (is_deleted IS NULL OR is_deleted = 0)");
        if (q.next())
            stats.total = q.value(0).toInt();
    }
    {
        QSqlQuery q;
        q.prepare("SELECT COUNT(*) FROM products "
                  "WHERE (is_deleted IS NULL OR is_deleted = 0) "
                  "AND stock > 0 AND stock <= :t");
        q.bindValue(":t", LOW_STOCK_THRESHOLD);
        if (q.exec() && q.next())
            stats.low = q.value(0).toInt();
    }
    {
        QSqlQuery q;
        q.prepare("SELECT COUNT(*) FROM products "
                  "WHERE (is_deleted IS NULL OR is_deleted = 0) "
                  "AND stock > :t");
        q.bindValue(":t", HIGH_STOCK_THRESHOLD);
        if (q.exec() && q.next())
            stats.high = q.value(0).toInt();
    }
    {
        QSqlQuery q("SELECT COUNT(*) FROM products "
                    "WHERE (is_deleted IS NULL OR is_deleted = 0) AND stock <= 0");
        if (q.next())
            stats.out = q.value(0).toInt();
    }
    return stats;
}

QString StockService::statusForStock(int stock)
{
    if (stock <= 0)                      return "Out Of Stock";
    if (stock <= LOW_STOCK_THRESHOLD)    return "Low Stock";
    if (stock > HIGH_STOCK_THRESHOLD)    return "High Stock";
    return "Normal";
}