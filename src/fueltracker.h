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
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
    Q_PROPERTY(QVariantMap strings READ strings NOTIFY languageChanged)
    Q_PROPERTY(QStringList units READ units CONSTANT)
    Q_PROPERTY(QStringList currencies READ currencies CONSTANT)
    Q_PROPERTY(QStringList languages READ languages CONSTANT)
    Q_PROPERTY(QStringList unitOptions READ unitOptions CONSTANT)

public:
    explicit FuelTracker(QObject *parent = nullptr);

    double lastConsumption() const;       // aktuelle Einheit
    double lastCostPerKm() const;         // €/km (Nachkommastellen)
    QString lastEntrySummary() const;
    int entryCount() const;

    QString unit() const;                 // "Liter" | "Gallonen" (intern)
    QString currency() const;             // "EUR" | "USD" | "GBP" | "ZAR"
    QString language() const;             // "Deutsch" | "English"
    Q_INVOKABLE QString unitLabel() const;      // Einheit in aktueller Sprache
    Q_INVOKABLE QString ls(const QString &key) const; // übersetzter Text (de/en)
    QStringList unitOptions() const;      // Einheiten als Liste in aktueller Sprache
    QStringList languages() const;        // verfügbare Sprachen
    QVariantMap strings() const;          // alle Übersetzungen (Key→Text)
    QStringList units() const;
    QStringList currencies() const;

    Q_INVOKABLE void setUnit(const QString &unit);
    Q_INVOKABLE void setCurrency(const QString &currency);
    Q_INVOKABLE void setLanguage(const QString &language);
    Q_INVOKABLE QString currencySymbol(const QString &currency) const;
    Q_INVOKABLE bool addEntry(const QDateTime &date,
                              double km,
                              double amount,     // Liter oder Gallonen je nach unit
                              double pricePerUnit, // Preis je Liter/Gallone
                              bool fullTank);
    Q_INVOKABLE bool deleteEntry(int id);
    Q_INVOKABLE void clearAll();

    QVariantList entries() const;   // Q_PROPERTY: Liste für QML

signals:
    void dataChanged();
    void settingsChanged();
    void languageChanged();

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
    QString m_language = QStringLiteral("Deutsch");
};

#endif // FUELTRACKER_H