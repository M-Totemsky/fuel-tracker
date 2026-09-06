#ifndef FUELTRACKER_H
#define FUELTRACKER_H

#include <QObject>
#include <QDateTime>
#include <QSqlDatabase>
#include <QVariantList>
#include <QStringList>

class FuelTracker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double lastConsumption READ lastConsumption NOTIFY dataChanged)
    Q_PROPERTY(double lastCostPerKm READ lastCostPerKm NOTIFY dataChanged)
    Q_PROPERTY(QString lastEntrySummary READ lastEntrySummary NOTIFY dataChanged)
    Q_PROPERTY(int entryCount READ entryCount NOTIFY dataChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY dataChanged)
    Q_PROPERTY(QString unit READ unit NOTIFY settingsChanged)
    Q_PROPERTY(QString currency READ currency NOTIFY settingsChanged)
    Q_PROPERTY(QStringList units READ units CONSTANT)
    Q_PROPERTY(QStringList currencies READ currencies CONSTANT)

public:
    explicit FuelTracker(QObject *parent = nullptr);

    double lastConsumption() const;       // aktuelle Einheit
    double lastCostPerKm() const;         // €/km (Nachkommastellen)
    QString lastEntrySummary() const;
    int entryCount() const;

    QString unit() const;                 // "Liter" | "Gallonen"
    QString currency() const;             // "EUR" | "USD" | "GBP"
    QStringList units() const;
    QStringList currencies() const;

    Q_INVOKABLE void setUnit(const QString &unit);
    Q_INVOKABLE void setCurrency(const QString &currency);
    Q_INVOKABLE QString currencySymbol(const QString &currency) const;
    Q_INVOKABLE bool addEntry(const QDateTime &date,
                              double km,
                              double amount,     // Liter oder Gallonen je nach unit
                              double pricePerUnit, // Preis je Liter/Gallone
                              bool fullTank);
    Q_INVOKABLE void clearAll();

    QVariantList entries() const;   // Q_PROPERTY: Liste für QML

signals:
    void dataChanged();
    void settingsChanged();

private:
    void openDatabase();
    void loadSettings();
    void saveSettings();
    void recompute();

    QSqlDatabase m_db;
    double m_lastConsumption = 0.0;
    double m_lastCostPerKm = 0.0;
    int m_entryCount = 0;
    QString m_lastSummary;
    QString m_unit = QStringLiteral("Liter");
    QString m_currency = QStringLiteral("EUR");
};

#endif // FUELTRACKER_H