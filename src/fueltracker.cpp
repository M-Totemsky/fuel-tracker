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

FuelTracker::FuelTracker(QObject *parent)
    : QObject(parent)
{
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
}

QString FuelTracker::unit() const { return m_unit; }
QString FuelTracker::currency() const { return m_currency; }

QStringList FuelTracker::units() const
{
    return { QStringLiteral("Liter"), QStringLiteral("Gallonen") };
}

QStringList FuelTracker::currencies() const
{
    return { QStringLiteral("EUR"), QStringLiteral("USD"), QStringLiteral("GBP") };
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
    if (!(currency == QStringLiteral("EUR")
          || currency == QStringLiteral("USD")
          || currency == QStringLiteral("GBP"))) return;
    m_currency = currency;
    saveSettings();
    emit settingsChanged();
    emit dataChanged();
}

static QString currencySymbol(const QString &currency)
{
    if (currency == QLatin1String("USD")) return QStringLiteral("$");
    if (currency == QLatin1String("GBP")) return QStringLiteral("£");
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
        m_lastSummary = tr("Noch keine Daten");
        m_lastConsumption = 0.0;
        m_lastCostPerKm = 0.0;
        return;
    }

    double km2 = v.value(0).toDouble();
    double lit2 = v.value(1).toDouble();
    double price2 = v.value(2).toDouble();

    if (!v.next()) {
        m_lastSummary = tr("Weitere Tankung nötig");
        m_lastConsumption = 0.0;
        m_lastCostPerKm = 0.0;
        return;
    }

    double km1 = v.value(0).toDouble();
    double diffKm = km2 - km1;
    if (diffKm <= 0 || lit2 <= 0) {
        m_lastSummary = tr("Weitere Tankung nötig");
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
        "SELECT date, km, liters, price_per_liter, full_tank FROM entries "
        "ORDER BY id DESC"));
    while (q.next()) {
        double liters = q.value(2).toDouble();
        double pricePerLiter = q.value(3).toDouble();
        QVariantMap m;
        m.insert(QStringLiteral("date"), q.value(0).toString());
        m.insert(QStringLiteral("km"), q.value(1).toDouble());
        if (m_unit == QStringLiteral("Gallonen")) {
            m.insert(QStringLiteral("amount"), liters / LITER_PER_GALLON);
            m.insert(QStringLiteral("amountUnit"), QStringLiteral("gal"));
            m.insert(QStringLiteral("price"), pricePerLiter * LITER_PER_GALLON);
        } else {
            m.insert(QStringLiteral("amount"), liters);
            m.insert(QStringLiteral("amountUnit"), QStringLiteral("l"));
            m.insert(QStringLiteral("price"), pricePerLiter);
        }
        m.insert(QStringLiteral("fullTank"), q.value(4).toBool());
        list.append(m);
    }
    return list;
}

void FuelTracker::clearAll()
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("DELETE FROM entries"));
    recompute();
    emit dataChanged();
}