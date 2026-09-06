#include "fueltracker.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QVariantMap>
#include <QLocale>

// Umrechnungsfaktoren
static constexpr double LITER_PER_GALLON = 3.785411784;   // US-Gallone
static constexpr double KM_PER_MILE     = 1.609344;

// Währungs-Tabelle: Neue Währung hier ergänzen, dann ist sie überall verfügbar
struct CurrencyInfo { const char *code; const char *symbol; };
static const CurrencyInfo kCurrencies[] = {
    { "EUR", "€" },
    { "USD", "$" },
    { "GBP", "£" },
    { "ZAR", "R" },
};

static bool isValidCurrency(const QString &code)
{
    for (auto &c : kCurrencies) {
        if (code == QString::fromUtf8(c.code)) return true;
    }
    return false;
}

// Übersetzungstabelle: Neue Texte hier ergänzen (de/en)
struct StringEntry { const char *key; const char *de; const char *en; };
static const StringEntry kStrings[] = {
    { "date",       "Datum",                   "Date" },
    { "today",      "Heute",                   "Today" },
    { "odometer",   "km-Stand",                "Odometer (km)" },
    { "fullTank",   "Voll getankt",            "Full tank" },
    { "saveEntry",  "Eintrag speichern",       "Save entry" },
    { "history",    "Verlauf",                 "History" },
    { "noEntries",  "Noch keine Einträge",     "No entries yet" },
    { "deleteAll",  "Alle Einträge löschen",   "Delete all entries" },
    { "noData",     "Noch keine Daten",        "No data yet" },
    { "needMore",   "Weitere Tankung nötig",   "Another full tank needed" },
    { "unitLiter",  "Liter",                   "Liters" },
    { "unitGallon", "Gallonen",                "Gallons" },
    { "settings",   "Einstellungen",           "Settings" },
    { "unitName",   "Einheit",                 "Unit" },
    { "currencyName", "Währung",               "Currency" },
    { "languageName", "Sprache",               "Language" },
    { "close",      "Schließen",               "Close" },
};

FuelTracker::FuelTracker(QObject *parent)
    : QObject(parent)
{
    if (QLocale::system().language() == QLocale::English) {
        m_language = QStringLiteral("English");
    }
    openDatabase();
    loadSettings();
    recompute();
}

void FuelTracker::openDatabase()
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QByteArray xdg = qgetenv("XDG_DATA_HOME");
    if (!xdg.isEmpty()) {
        dataDir = QString::fromLocal8Bit(xdg) + QStringLiteral("/fuel-tracker");
    }
    QDir().mkpath(dataDir);

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("fuelconn"));
    m_db.setDatabaseName(dataDir + QStringLiteral("/fuel-tracker.sqlite"));
    if (!m_db.open()) {
        qWarning() << "DB open failed:" << m_db.lastError().text();
        return;
    }

    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS entries ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  date TEXT NOT NULL,"
        "  km REAL NOT NULL,"
        "  liters REAL NOT NULL,"
        "  price_per_liter REAL NOT NULL,"
        "  full_tank INTEGER NOT NULL DEFAULT 1"
        ")"));
    if (q.lastError().isValid()) {
        qWarning() << "CREATE TABLE entries failed:" << q.lastError().text();
        return;
    }

    // Einmalige Reparatur: In 0.1.3/0.1.4 waren km und Liter in addEntry
    // vertauscht (km-Stand wurden als Liter gespeichert und umgekehrt).
    // Betroffene Zeilen erkennen wir an unrealistisch hohem Literwert.
    q.exec(QStringLiteral(
        "UPDATE entries SET km = liters, liters = km "
        "WHERE liters > 200 AND km < 200"));
    if (q.lastError().isValid()) {
        qWarning() << "Data repair failed:" << q.lastError().text();
    }

    // Settings-Tabelle für Einheit/Währung
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ")"));
    if (q.lastError().isValid()) {
        qWarning() << "CREATE TABLE settings failed:" << q.lastError().text();
    }
}

void FuelTracker::loadSettings()
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT key, value FROM settings"));
    while (q.next()) {
        const QString key = q.value(0).toString();
        const QString value = q.value(1).toString();
        if (key == QStringLiteral("unit")) m_unit = value;
        else if (key == QStringLiteral("currency")) m_currency = value;
        else if (key == QStringLiteral("language")) m_language = value;
    }
}

void FuelTracker::saveSettings()
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"));
    q.addBindValue(QStringLiteral("unit"));
    q.addBindValue(m_unit);
    q.exec();
    q.addBindValue(QStringLiteral("currency"));
    q.addBindValue(m_currency);
    q.exec();
    q.addBindValue(QStringLiteral("language"));
    q.addBindValue(m_language);
    q.exec();
}

QString FuelTracker::unit() const { return m_unit; }
QString FuelTracker::currency() const { return m_currency; }
QString FuelTracker::language() const { return m_language; }

QString FuelTracker::ls(const QString &key) const
{
    const bool en = m_language == QStringLiteral("English");
    for (auto &s : kStrings) {
        if (key == QString::fromUtf8(s.key)) {
            return QString::fromUtf8(en ? s.en : s.de);
        }
    }
    return key;
}

QVariantMap FuelTracker::strings() const
{
    QVariantMap map;
    for (auto &s : kStrings) {
        map.insert(QString::fromUtf8(s.key), ls(QString::fromUtf8(s.key)));
    }
    return map;
}

QString FuelTracker::unitLabel() const
{
    return m_unit == QStringLiteral("Gallonen") ? ls(QStringLiteral("unitGallon"))
                                                : ls(QStringLiteral("unitLiter"));
}

QStringList FuelTracker::unitOptions() const
{
    QStringList list;
    list << ls(QStringLiteral("unitLiter")) << ls(QStringLiteral("unitGallon"));
    return list;
}

QStringList FuelTracker::units() const
{
    return { QStringLiteral("Liter"), QStringLiteral("Gallonen") };
}

QStringList FuelTracker::currencies() const
{
    QStringList list;
    for (auto &c : kCurrencies) list << QString::fromUtf8(c.code);
    return list;
}

QStringList FuelTracker::languages() const
{
    return { QStringLiteral("Deutsch"), QStringLiteral("English") };
}

void FuelTracker::setUnit(const QString &unit)
{
    if (m_unit == unit) return;
    if (!(unit == QStringLiteral("Liter") || unit == QStringLiteral("Gallonen"))) return;
    m_unit = unit;
    saveSettings();
    emit settingsChanged();
    emit dataChanged();
}

void FuelTracker::setCurrency(const QString &currency)
{
    if (m_currency == currency) return;
    if (!isValidCurrency(currency)) return;
    m_currency = currency;
    saveSettings();
    emit settingsChanged();
    emit dataChanged();
}

void FuelTracker::setLanguage(const QString &language)
{
    if (m_language == language) return;
    if (!(language == QStringLiteral("Deutsch") || language == QStringLiteral("English"))) return;
    m_language = language;
    saveSettings();
    emit languageChanged();
    emit settingsChanged();
    recompute();
    emit dataChanged();
}

QString FuelTracker::currencySymbol(const QString &currency) const
{
    for (auto &c : kCurrencies) {
        if (currency == QString::fromUtf8(c.code)) return QString::fromUtf8(c.symbol);
    }
    return QStringLiteral("€");
}

void FuelTracker::recompute()
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT COUNT(*) FROM entries"));
    if (q.next()) {
        m_entryCount = q.value(0).toInt();
    }

    // Voll→Voll: neuester voller Eintrag minus vorheriger voller Eintrag.
    QSqlQuery v(m_db);
    v.exec(QStringLiteral(
        "SELECT km, liters, price_per_liter, date FROM entries "
        "WHERE full_tank = 1 ORDER BY id DESC LIMIT 2"));
    if (!v.next()) {
        m_lastSummary = ls(QStringLiteral("noData"));
        m_lastConsumption = 0.0;
        m_lastCostPerKm = 0.0;
        return;
    }

    double km2 = v.value(0).toDouble();
    double lit2 = v.value(1).toDouble();
    double price2 = v.value(2).toDouble();

    if (!v.next()) {
        m_lastSummary = ls(QStringLiteral("needMore"));
        m_lastConsumption = 0.0;
        m_lastCostPerKm = 0.0;
        return;
    }

    double km1 = v.value(0).toDouble();
    double diffKm = km2 - km1;
    if (diffKm <= 0 || lit2 <= 0) {
        m_lastSummary = ls(QStringLiteral("needMore"));
        m_lastConsumption = 0.0;
        m_lastCostPerKm = 0.0;
        return;
    }

    // Basis: l/100km
    double l100km = lit2 / diffKm * 100.0;

    // Anzeige je nach Einheit: Liter → l/100km; Gallonen → mpg (US)
    QString sym = currencySymbol(m_currency);
    if (m_unit == QStringLiteral("Liter")) {
        m_lastConsumption = l100km;
        m_lastCostPerKm = (lit2 * price2) / diffKm;               // €/km
        m_lastSummary = QStringLiteral("%1 l/100km · %2 %3/km")
                            .arg(l100km, 0, 'f', 1)
                            .arg(m_lastCostPerKm, 0, 'f', 2)
                            .arg(sym);
    } else {
        // Gallonen: Verbrauch als mpg (Miles per Gallon)
        double gallons = lit2 / LITER_PER_GALLON;
        double miles = diffKm / KM_PER_MILE;
        double mpg = gallons > 0 ? miles / gallons : 0.0;
        m_lastConsumption = mpg;
        m_lastCostPerKm = (lit2 * price2) / diffKm;              // Währung/km
        m_lastSummary = QStringLiteral("%1 mpg · %2 %3/km")
                            .arg(mpg, 0, 'f', 1)
                            .arg(m_lastCostPerKm, 0, 'f', 2)
                            .arg(sym);
    }
}

double FuelTracker::lastConsumption() const { return m_lastConsumption; }
double FuelTracker::lastCostPerKm() const { return m_lastCostPerKm; }
QString FuelTracker::lastEntrySummary() const { return m_lastSummary; }
int FuelTracker::entryCount() const { return m_entryCount; }

bool FuelTracker::addEntry(const QDateTime &date,
                           double km,
                           double amount,
                           double pricePerUnit,
                           bool fullTank)
{
    if (amount <= 0 || km < 0 || pricePerUnit < 0) return false;

    // Normalisiere in Liter als Speicherbasis
    double liters = (m_unit == QStringLiteral("Gallonen"))
                    ? amount * LITER_PER_GALLON
                    : amount;
    double pricePerLiter = (m_unit == QStringLiteral("Gallonen"))
                           ? pricePerUnit / LITER_PER_GALLON
                           : pricePerUnit;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO entries (date, km, liters, price_per_liter, full_tank) "
        "VALUES (?, ?, ?, ?, ?)"));
    q.addBindValue(date.toString(Qt::ISODateWithMs));
    q.addBindValue(km);
    q.addBindValue(liters);
    q.addBindValue(pricePerLiter);
    q.addBindValue(fullTank ? 1 : 0);
    if (!q.exec()) {
        qWarning() << "INSERT failed:" << q.lastError().text();
        return false;
    }

    recompute();
    emit dataChanged();
    return true;
}

QVariantList FuelTracker::entries() const
{
    QVariantList list;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT id, date, km, liters, price_per_liter, full_tank FROM entries "
        "ORDER BY id DESC"));
    while (q.next()) {
        double liters = q.value(3).toDouble();
        double pricePerLiter = q.value(4).toDouble();
        QVariantMap m;
        m.insert(QStringLiteral("id"), q.value(0).toInt());
        m.insert(QStringLiteral("date"), q.value(1).toString());
        m.insert(QStringLiteral("km"), q.value(2).toDouble());
        if (m_unit == QStringLiteral("Gallonen")) {
            m.insert(QStringLiteral("amount"), liters / LITER_PER_GALLON);
            m.insert(QStringLiteral("amountUnit"), QStringLiteral("gal"));
            m.insert(QStringLiteral("price"), pricePerLiter * LITER_PER_GALLON);
        } else {
            m.insert(QStringLiteral("amount"), liters);
            m.insert(QStringLiteral("amountUnit"), QStringLiteral("l"));
            m.insert(QStringLiteral("price"), pricePerLiter);
        }
        m.insert(QStringLiteral("fullTank"), q.value(5).toBool());
        list.append(m);
    }
    return list;
}

bool FuelTracker::deleteEntry(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM entries WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        qWarning() << "DELETE failed:" << q.lastError().text();
        return false;
    }
    recompute();
    emit dataChanged();
    return true;
}

void FuelTracker::clearAll()
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("DELETE FROM entries"));
    recompute();
    emit dataChanged();
}