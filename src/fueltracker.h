#ifndef FUELTRACKER_H
#define FUELTRACKER_H

#include <QObject>
#include <QDateTime>
#include <QSqlDatabase>
#include <QVariantList>

class FuelTracker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double lastConsumption READ lastConsumption NOTIFY dataChanged)
    Q_PROPERTY(double lastCostPerKm READ lastCostPerKm NOTIFY dataChanged)
    Q_PROPERTY(QString lastEntrySummary READ lastEntrySummary NOTIFY dataChanged)
    Q_PROPERTY(int entryCount READ entryCount NOTIFY dataChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY dataChanged)

public:
    explicit FuelTracker(QObject *parent = nullptr);

    double lastConsumption() const;       // l/100km
    double lastCostPerKm() const;         // €/km (Nachkommastellen)
    QString lastEntrySummary() const;     // z. B. "5,4 l/100km · 0,12 €/km"
    int entryCount() const;

    Q_INVOKABLE bool addEntry(const QDateTime &date,
                              double km,
                              double liters,
                              double pricePerLiter,
                              bool fullTank);
    Q_INVOKABLE void clearAll();

    QVariantList entries() const;   // Q_PROPERTY: Liste für QML

signals:
    void dataChanged();

private:
    void openDatabase();
    void recompute();

    QSqlDatabase m_db;
    double m_lastConsumption = 0.0;
    double m_lastCostPerKm = 0.0;
    int m_entryCount = 0;
    QString m_lastSummary;
};

#endif // FUELTRACKER_H