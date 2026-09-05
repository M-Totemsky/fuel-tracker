#include "fueltracker.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QVariantMap>
#include <QJsonObject>

FuelTracker::FuelTracker(QObject *parent)
    : QObject(parent)
{
    openDatabase();
    recompute();
}

void FuelTracker::openDatabase()
{
    // Bevorzugt in $XDG_DATA_HOME (Click-apparmor erlaubt ~/.local/share/fuel-tracker/**),
    // Fallback auf QStandardPaths, falls XDG nicht gesetzt ist.
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
        qWarning() << "CREATE TABLE failed:" << q.lastError().text();
    }
}

void FuelTracker::recompute()
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT COUNT(*) FROM entries"));
    if (q.next()) {
        m_entryCount = q.value(0).toInt();
    }

    // Voll→Voll: neuester voller Eintrag minus vorheriger voller Eintrag.
    QSqlQuery v(m_db);
    v.exec(QStringLiteral(
        "SELECT km, liters, price_per_liter, date FROM entries "
        "WHERE full_tank = 1 ORDER BY id DESC LIMIT 2"));
    if (v.next()) {
        double km2 = v.value(0).toDouble();
        double lit2 = v.value(1).toDouble();
        double price2 = v.value(2).toDouble();
        QString date2 = v.value(3).toString();

        if (v.next()) {
            double km1 = v.value(0).toDouble();
            double lit1 = v.value(1).toDouble();
            double price1 = v.value(2).toDouble();
            Q_UNUSED(lit1)
            Q_UNUSED(price1)

            double diffKm = km2 - km1;
            if (diffKm > 0 && lit2 > 0) {
                m_lastConsumption = lit2 / diffKm * 100.0;
            } else {
                m_lastConsumption = 0.0;
            }
            double fuelCost = lit2 * price2;
            if (diffKm > 0) {
                m_lastCostPerKm = fuelCost / diffKm;
            } else {
                m_lastCostPerKm = 0.0;
            }

            m_lastSummary = QStringLiteral("%1 l/100km · %2 €/km")
                                .arg(m_lastConsumption, 0, 'f', 1)
                                .arg(m_lastCostPerKm, 0, 'f', 2);
            Q_UNUSED(date2)
        } else {
            m_lastSummary = tr("Weitere Tankung nötig");
        }
    } else {
        m_lastSummary = tr("Noch keine Daten");
    }
}

double FuelTracker::lastConsumption() const { return m_lastConsumption; }
double FuelTracker::lastCostPerKm() const { return m_lastCostPerKm; }
QString FuelTracker::lastEntrySummary() const { return m_lastSummary; }
int FuelTracker::entryCount() const { return m_entryCount; }

bool FuelTracker::addEntry(const QDateTime &date,
                           double km,
                           double liters,
                           double pricePerLiter,
                           bool fullTank)
{
    if (liters <= 0 || km < 0 || pricePerLiter < 0) return false;

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
        QVariantMap m;
        m.insert(QStringLiteral("date"), q.value(0).toString());
        m.insert(QStringLiteral("km"), q.value(1).toDouble());
        m.insert(QStringLiteral("liters"), q.value(2).toDouble());
        m.insert(QStringLiteral("price"), q.value(3).toDouble());
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