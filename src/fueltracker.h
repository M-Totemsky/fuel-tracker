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
    Q_PROPERTY(double totalCost READ totalCost NOTIFY dataChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY dataChanged)
    Q_PROPERTY(QString unit READ unit NOTIFY settingsChanged)
    Q_PROPERTY(QString currency READ currency NOTIFY settingsChanged)
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
    Q_PROPERTY(QVariantMap strings READ strings NOTIFY languageChanged)
    Q_PROPERTY(QStringList units READ units CONSTANT)
    Q_PROPERTY(QStringList currencies READ currencies CONSTANT)
    Q_PROPERTY(QStringList languages READ languages CONSTANT)
    Q_PROPERTY(QStringList unitOptions READ unitOptions NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList vehicles READ vehicles NOTIFY vehiclesChanged)
    Q_PROPERTY(QString activeVehicleName READ activeVehicleName NOTIFY vehiclesChanged)
    Q_PROPERTY(bool isElectric READ isElectric NOTIFY vehiclesChanged)
    Q_PROPERTY(QStringList fuelTypeOptions READ fuelTypeOptions NOTIFY languageChanged)

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
    Q_INVOKABLE QString distanceUnit() const;     // "km" (Liter) | "mi" (Gallonen)
    double totalCost() const;          // Summe aller Ausgaben
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
                              double amount,     // Liter/Gallonen oder kWh je Fahrzeug
                              double pricePerUnit, // Preis je Liter/Gallone/kWh
                              bool fullTank);
    Q_INVOKABLE bool deleteEntry(int id);
    Q_INVOKABLE bool clearAll();

    QVariantList entries() const;   // Q_PROPERTY: Liste für QML

    QString activeVehicleName() const;
    bool isElectric() const;                    // aktives Fahrzeug ist Elektro
    QVariantList vehicles() const;              // Liste für QML (id/name/fuelType)
    QStringList fuelTypes() const;              // Rohwerte (petrol/diesel/lpg/electric)
    QStringList fuelTypeOptions() const;        // Antriebs-Arten in aktueller Sprache
    QString fuelTypeLabel(const QString &code) const;
    Q_INVOKABLE void setActiveVehicle(int id);
    Q_INVOKABLE bool vehicleNameExists(const QString &name) const;
    Q_INVOKABLE int addVehicle(const QString &name, int fuelTypeIndex);
    // addVehicle: >0 Fahrzeug-Id | -1 leer/DB-Fehler | -2 Name schon vergeben
    Q_INVOKABLE bool renameVehicle(int id, const QString &name);
    Q_INVOKABLE bool deleteVehicle(int id);

    Q_INVOKABLE QString createBackup();      // DB-Sicherung nach ~/Dokumente
    Q_INVOKABLE QStringList backupFiles();   // vorhandene Sicherungen (neueste zuerst)
    Q_INVOKABLE bool restoreBackup(const QString &backupPath);
    Q_INVOKABLE bool exportCsv();            // CSV aller Fahrzeuge nach ~/Dokumente

signals:
    void dataChanged();
    void settingsChanged();
    void languageChanged();
    void vehiclesChanged();

private:
    void openDatabase();
    void loadSettings();
    void saveSettings();
    void recompute();
    void ensureDefaultVehicle();
    void loadActiveVehicle();
    void setActiveVehicleId(int id);
    int firstVehicleId() const;

    QString documentsDir() const;      // ~/Dokumente (erzeugt falls nötig)
    QString stamp(bool withSeconds) const;
    bool validDbFile(const QString &path) const;

    QSqlDatabase m_db;
    double m_lastConsumption = 0.0;
    double m_lastCostPerKm = 0.0;
    int m_entryCount = 0;
    QString m_lastSummary;
    QString m_unit = QStringLiteral("Liter");
    QString m_currency = QStringLiteral("EUR");
    QString m_language = QStringLiteral("Deutsch");
    double m_totalCost = 0.0;
    int m_activeVehicleId = 0;
    QString m_activeVehicleName;
    QString m_fuelType = QStringLiteral("petrol");
};

#endif // FUELTRACKER_H