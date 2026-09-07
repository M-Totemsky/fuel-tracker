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
    { "JPY", "¥" },
};

static bool isValidCurrency(const QString &code)
{
    for (auto &c : kCurrencies) {
        if (code == QString::fromUtf8(c.code)) return true;
    }
    return false;
}

// Übersetzungstabelle: Neue Texte hier ergänzen (de/en)
struct StringEntry { const char *key; const char *de; const char *en; const char *ja; };
static const StringEntry kStrings[] = {
    { "date", "Datum", "Date", "日付" },
    { "today", "Heute", "Today", "今日" },
    { "odometerKm", "km-Stand", "Odometer (km)", "走行距離(km)" },
    { "odometerMi", "Meilenstand", "Odometer (mi)", "走行距離(mi)" },
    { "fullTank", "Voll getankt", "Full tank", "満タン" },
    { "fullCharge", "Voll geladen", "Full charge", "満充電" },
    { "saveEntry", "Eintrag speichern", "Save entry", "記録を保存" },
    { "history", "Verlauf", "History", "履歴" },
    { "noEntries", "Noch keine Einträge", "No entries yet", "まだ記録がありません" },
    { "deleteAll", "Alle Einträge löschen", "Delete all entries", "すべての記録を削除" },
    { "noData", "Noch keine Daten", "No data yet", "データがありません" },
    { "needMore", "Weitere Tankung nötig", "Another full tank needed", "次の満タンが必要です" },
    { "unitLiter", "Liter", "Liters", "リットル" },
    { "unitGallon", "Gallonen", "Gallons", "ガロン" },
    { "totalSpend", "Gesamtausgaben", "Total spent", "総費用" },
    { "settings", "Einstellungen", "Settings", "設定" },
    { "unitName", "Einheit", "Unit", "単位" },
    { "currencyName", "Währung", "Currency", "通貨" },
    { "languageName", "Sprache", "Language", "言語" },
    { "close", "Schließen", "Close", "閉じる" },
    { "vehicle", "Fahrzeug", "Vehicle", "車両" },
    { "vehicles", "Fahrzeuge", "Vehicles", "車両" },
    { "name", "Name", "Name", "名前" },
    { "addVehicle", "Fahrzeug hinzufügen", "Add vehicle", "車両を追加" },
    { "renameVehicle", "Umbenennen", "Rename", "名前を変更" },
    { "deleteVehicle", "Löschen", "Delete", "削除" },
    { "fuelType", "Antrieb", "Fuel type", "動力" },
    { "defaultVehicle", "Fahrzeug 1", "Vehicle 1", "車両 1" },
    { "confirmDeleteVehicle", "Fahrzeug und alle zugehörigen Einträge löschen?", "Delete vehicle and all its entries?", "車両とそのすべての記録を削除しますか？" },
    { "cancel", "Abbrechen", "Cancel", "キャンセル" },
    { "yes", "Ja", "Yes", "はい" },
    { "kwhUnit", "kWh", "kWh", "kWh" },
    { "unitKm", "Kilometer", "Kilometers", "キロメートル" },
    { "unitMi", "Meilen", "Miles", "マイル" },
};

// Antriebs-Tabelle: Neue Antriebsart hier ergänzen
static const struct { const char *code; const char *de; const char *en; const char *ja; } kFuelTypes[] = {
    { "petrol",   "Benzin",  "Petrol", "ガソリン" },
    { "diesel",   "Diesel",  "Diesel", "ディーゼル" },
    { "lpg",   "LPG",  "LPG", "LPG" },
    { "electric",   "Elektro",  "Electric", "電気" },
};

static bool isValidFuelType(const QString &code)
{
    for (auto &f : kFuelTypes) {
        if (code == QString::fromUtf8(f.code)) return true;
    }
    return false;
}

FuelTracker::FuelTracker(QObject *parent)
    : QObject(parent)
{
    if (QLocale::system().language() == QLocale::English) {
        m_language = QStringLiteral("English");
    } else if (QLocale::system().language() == QLocale::Japanese) {
        m_language = QStringLiteral("日本語");
    }
    openDatabase();
    loadSettings();
    ensureDefaultVehicle();
    loadActiveVehicle();
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
        "  full_tank INTEGER NOT NULL DEFAULT 1,"
        "  vehicle_id INTEGER NOT NULL DEFAULT 1"
        ")"));
    if (q.lastError().isValid()) {
        qWarning() << "CREATE TABLE entries failed:" << q.lastError().text();
        return;
    }

    // Migration: ältere Datenbanken ohne vehicle_id-Spalte erhalten den Wert 1
    // (DEFAULT → gehört dem ersten, automatisch angelegten Fahrzeug).
    QSqlQuery col(m_db);
    col.exec(QStringLiteral("PRAGMA table_info(entries)"));
    bool hasVehicleId = false;
    while (col.next()) {
        if (col.value(1).toString() == QStringLiteral("vehicle_id")) hasVehicleId = true;
    }
    if (!hasVehicleId) {
        QSqlQuery al(m_db);
        al.exec(QStringLiteral("ALTER TABLE entries ADD COLUMN vehicle_id INTEGER NOT NULL DEFAULT 1"));
        if (al.lastError().isValid()) {
            qWarning() << "ALTER TABLE entries failed:" << al.lastError().text();
        }
    }

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS vehicles ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  fuel_type TEXT NOT NULL DEFAULT 'petrol'"
        ")"));
    if (q.lastError().isValid()) {
        qWarning() << "CREATE TABLE vehicles failed:" << q.lastError().text();
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
        else if (key == QStringLiteral("active_vehicle")) m_activeVehicleId = value.toInt();
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
    q.addBindValue(QStringLiteral("active_vehicle"));
    q.addBindValue(QString::number(m_activeVehicleId));
    q.exec();
}

void FuelTracker::ensureDefaultVehicle()
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT COUNT(*) FROM vehicles"));
    int count = 0;
    if (q.next()) count = q.value(0).toInt();
    if (count > 0) return;

    QSqlQuery ins(m_db);
    ins.prepare(QStringLiteral(
        "INSERT INTO vehicles (name, fuel_type) VALUES (?, ?)"));
    ins.addBindValue(ls(QStringLiteral("defaultVehicle")));
    ins.addBindValue(QStringLiteral("petrol"));
    ins.exec();
}

int FuelTracker::firstVehicleId() const
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT id FROM vehicles ORDER BY id LIMIT 1"));
    if (q.next()) return q.value(0).toInt();
    return 0;
}

void FuelTracker::loadActiveVehicle()
{
    if (m_activeVehicleId <= 0) m_activeVehicleId = firstVehicleId();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id FROM vehicles WHERE id = ?"));
    q.addBindValue(m_activeVehicleId);
    q.exec();
    if (!q.next()) {
        m_activeVehicleId = firstVehicleId();
    }
    setActiveVehicleId(m_activeVehicleId);
}

void FuelTracker::setActiveVehicleId(int id)
{
    m_activeVehicleId = id;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT name, fuel_type FROM vehicles WHERE id = ?"));
    q.addBindValue(id);
    q.exec();
    if (q.next()) {
        m_activeVehicleName = q.value(0).toString();
        m_fuelType = q.value(1).toString();
    } else {
        m_activeVehicleName.clear();
        m_fuelType = QStringLiteral("petrol");
    }
}

QString FuelTracker::activeVehicleName() const { return m_activeVehicleName; }
bool FuelTracker::isElectric() const { return m_fuelType == QStringLiteral("electric"); }

QString FuelTracker::unit() const { return m_unit; }
QString FuelTracker::currency() const { return m_currency; }
QString FuelTracker::language() const { return m_language; }

QString FuelTracker::ls(const QString &key) const
{
    const bool en = m_language == QStringLiteral("English");
    const bool ja = m_language == QStringLiteral("日本語");
    for (auto &s : kStrings) {
        if (key == QString::fromUtf8(s.key)) {
            if (ja) return QString::fromUtf8(s.ja);
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
    // Elektro: keine Mengeneinheit (immer kWh) -> nur Distanz waehlbar
    if (isElectric()) {
        return { ls(QStringLiteral("unitKm")), ls(QStringLiteral("unitMi")) };
    }
    return { ls(QStringLiteral("unitLiter")), ls(QStringLiteral("unitGallon")) };
}

QStringList FuelTracker::units() const
{
    return { QStringLiteral("Liter"), QStringLiteral("Gallonen") };
}

QStringList FuelTracker::fuelTypes() const
{
    QStringList list;
    for (auto &f : kFuelTypes) list << QString::fromUtf8(f.code);
    return list;
}

QStringList FuelTracker::fuelTypeOptions() const
{
    QStringList list;
    for (auto &f : kFuelTypes) list << fuelTypeLabel(QString::fromUtf8(f.code));
    return list;
}

QString FuelTracker::fuelTypeLabel(const QString &code) const
{
    const bool en = m_language == QStringLiteral("English");
    const bool ja = m_language == QStringLiteral("日本語");
    for (auto &f : kFuelTypes) {
        if (code == QString::fromUtf8(f.code)) {
            if (ja) return QString::fromUtf8(f.ja);
            return QString::fromUtf8(en ? f.en : f.de);
        }
    }
    return code;
}

QVariantList FuelTracker::vehicles() const
{
    QVariantList list;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT id, name, fuel_type FROM vehicles ORDER BY id"));
    while (q.next()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), q.value(0).toInt());
        m.insert(QStringLiteral("name"), q.value(1).toString());
        m.insert(QStringLiteral("fuelType"), q.value(2).toString());
        m.insert(QStringLiteral("fuelTypeLabel"), fuelTypeLabel(q.value(2).toString()));
        m.insert(QStringLiteral("isActive"), q.value(0).toInt() == m_activeVehicleId);
        list.append(m);
    }
    return list;
}

void FuelTracker::setActiveVehicle(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id FROM vehicles WHERE id = ?"));
    q.addBindValue(id);
    q.exec();
    if (!q.next()) return;

    setActiveVehicleId(id);
    saveSettings();
    recompute();
    emit settingsChanged();
    emit vehiclesChanged();
    emit dataChanged();
}

int FuelTracker::addVehicle(const QString &name, int fuelTypeIndex)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return -1;
    QStringList types = fuelTypes();
    // Absicherung: ungültiger Index (z. B. -1) → erstes Element
    if (fuelTypeIndex < 0 || fuelTypeIndex >= types.size()) fuelTypeIndex = 0;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO vehicles (name, fuel_type) VALUES (?, ?)"));
    q.addBindValue(trimmed);
    q.addBindValue(types.at(fuelTypeIndex));
    if (!q.exec()) return -1;

    emit vehiclesChanged();
    return q.lastInsertId().toInt();
}

bool FuelTracker::renameVehicle(int id, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return false;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE vehicles SET name = ? WHERE id = ?"));
    q.addBindValue(trimmed);
    q.addBindValue(id);
    if (!q.exec()) return false;

    if (id == m_activeVehicleId) {
        setActiveVehicleId(id);
        emit dataChanged();
    }
    emit vehiclesChanged();
    return true;
}

bool FuelTracker::deleteVehicle(int id)
{
    QSqlQuery del(m_db);
    del.prepare(QStringLiteral("DELETE FROM entries WHERE vehicle_id = ?"));
    del.addBindValue(id);
    del.exec();
    del.prepare(QStringLiteral("DELETE FROM vehicles WHERE id = ?"));
    del.addBindValue(id);
    if (!del.exec()) return false;

    // Kein Fahrzeug mehr übrig → automatisch wieder das Default-Fahrzeug anlegen,
    // damit die App immer ein gültiges Fahrzeug hat.
    ensureDefaultVehicle();

    if (id == m_activeVehicleId) {
        setActiveVehicleId(firstVehicleId());
        saveSettings();
    }
    recompute();
    emit settingsChanged();
    emit vehiclesChanged();
    emit dataChanged();
    return true;
}

QStringList FuelTracker::currencies() const
{
    QStringList list;
    for (auto &c : kCurrencies) list << QString::fromUtf8(c.code);
    return list;
}

QStringList FuelTracker::languages() const
{
    return { QStringLiteral("Deutsch"), QStringLiteral("English"), QStringLiteral("日本語") };
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
    if (!(language == QStringLiteral("Deutsch") || language == QStringLiteral("English")
      || language == QStringLiteral("日本語"))) return;
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
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM entries WHERE vehicle_id = ?"));
    q.addBindValue(m_activeVehicleId);
    q.exec();
    if (q.next()) {
        m_entryCount = q.value(0).toInt();
    }

    q.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(liters * price_per_liter), 0) FROM entries WHERE vehicle_id = ?"));
    q.addBindValue(m_activeVehicleId);
    q.exec();
    if (q.next()) {
        m_totalCost = q.value(0).toDouble();
    }

    const QLocale loc = (m_language == QStringLiteral("English")) ? QLocale::English
                  : (m_language == QStringLiteral("日本語")) ? QLocale::Japanese
                  : QLocale::German;

    // Voll→Voll: neuester voller Eintrag minus vorheriger voller Eintrag.
    QSqlQuery v(m_db);
    v.prepare(QStringLiteral(
        "SELECT km, liters, price_per_liter FROM entries "
        "WHERE full_tank = 1 AND vehicle_id = ? ORDER BY id DESC LIMIT 2"));
    v.addBindValue(m_activeVehicleId);
    v.exec();
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

    QString sym = currencySymbol(m_currency);
    const bool gallons = m_unit == QStringLiteral("Gallonen");
    const double dist = gallons ? diffKm / KM_PER_MILE : diffKm;   // km oder mi

    // Basis: l/100km (Verbrenner) bzw. kWh/100km (Elektro) — Anzeige nach Einheit
    double per100 = lit2 / diffKm * 100.0;          // je 100 km
    QString consumptionUnit;
    if (isElectric()) {
        consumptionUnit = QStringLiteral("kWh/100%1").arg(gallons ? QStringLiteral("mi") : QStringLiteral("km"));
        m_lastConsumption = lit2 / dist * 100.0;    // kWh/100(mi|km)
        m_lastCostPerKm = (lit2 * price2) / dist;
        m_lastSummary = QStringLiteral("%1 %2 · %3 %4/%5")
                            .arg(loc.toString(m_lastConsumption, 'f', 1))
                            .arg(consumptionUnit)
                            .arg(loc.toString(m_lastCostPerKm, 'f', 2))
                            .arg(sym)
                            .arg(gallons ? QStringLiteral("mi") : QStringLiteral("km"));
        return;
    }

    if (gallons) {
        // Gallonen-Verbrenner: mpg (Miles per Gallon), Kosten je Meile
        double gallons2 = lit2 / LITER_PER_GALLON;
        double mpg = gallons2 > 0 ? dist / gallons2 : 0.0;
        m_lastConsumption = mpg;
        m_lastCostPerKm = (lit2 * price2) / dist;   // Währung/mi
        m_lastSummary = QStringLiteral("%1 mpg · %2 %3/%4")
                            .arg(loc.toString(mpg, 'f', 1))
                            .arg(loc.toString(m_lastCostPerKm, 'f', 2))
                            .arg(sym)
                            .arg(QStringLiteral("mi"));
    } else {
        m_lastConsumption = per100;
        m_lastCostPerKm = (lit2 * price2) / diffKm;  // Währung/km
        m_lastSummary = QStringLiteral("%1 l/100km · %2 %3/%4")
                            .arg(loc.toString(per100, 'f', 1))
                            .arg(loc.toString(m_lastCostPerKm, 'f', 2))
                            .arg(sym)
                            .arg(QStringLiteral("km"));
    }
}

double FuelTracker::totalCost() const { return m_totalCost; }

QString FuelTracker::distanceUnit() const
{
    return m_unit == QStringLiteral("Gallonen") ? QStringLiteral("mi")
                                                : QStringLiteral("km");
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

    // Verbrenner: normalisiere in Liter als Speicherbasis.
    // Elektro: kWh wird direkt gespeichert (keine Gallonen-Umrechnung).
    const bool gallons = m_unit == QStringLiteral("Gallonen") && !isElectric();
    double liters = gallons ? amount * LITER_PER_GALLON : amount;
    double pricePerLiter = gallons ? pricePerUnit / LITER_PER_GALLON : pricePerUnit;
    // Distanz: Bei Gallonen gibt der Nutzer Meilen ein → intern km
    double kmStored = (m_unit == QStringLiteral("Gallonen"))
                      ? km * KM_PER_MILE
                      : km;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO entries (date, km, liters, price_per_liter, full_tank, vehicle_id) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    q.addBindValue(date.toString(Qt::ISODateWithMs));
    q.addBindValue(kmStored);
    q.addBindValue(liters);
    q.addBindValue(pricePerLiter);
    q.addBindValue(fullTank ? 1 : 0);
    q.addBindValue(m_activeVehicleId);
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
    q.prepare(QStringLiteral(
        "SELECT id, date, km, liters, price_per_liter, full_tank FROM entries "
        "WHERE vehicle_id = ? ORDER BY id DESC"));
    q.addBindValue(m_activeVehicleId);
    q.exec();
    const bool gallons = m_unit == QStringLiteral("Gallonen");
    while (q.next()) {
        double kmStored = q.value(2).toDouble();
        double amount = q.value(3).toDouble();
        double priceStored = q.value(4).toDouble();
        QVariantMap m;
        m.insert(QStringLiteral("id"), q.value(0).toInt());
        m.insert(QStringLiteral("date"), q.value(1).toString());
        if (isElectric()) {
            m.insert(QStringLiteral("amount"), amount);
            m.insert(QStringLiteral("amountUnit"), QStringLiteral("kWh"));
            m.insert(QStringLiteral("price"), priceStored);
        } else if (gallons) {
            m.insert(QStringLiteral("amount"), amount / LITER_PER_GALLON);
            m.insert(QStringLiteral("amountUnit"), QStringLiteral("gal"));
            m.insert(QStringLiteral("price"), priceStored * LITER_PER_GALLON);
        } else {
            m.insert(QStringLiteral("amount"), amount);
            m.insert(QStringLiteral("amountUnit"), QStringLiteral("l"));
            m.insert(QStringLiteral("price"), priceStored);
        }
        if (gallons && !isElectric()) {
            m.insert(QStringLiteral("km"), kmStored / KM_PER_MILE);
            m.insert(QStringLiteral("distUnit"), QStringLiteral("mi"));
        } else {
            m.insert(QStringLiteral("km"), kmStored);
            m.insert(QStringLiteral("distUnit"), QStringLiteral("km"));
        }
        m.insert(QStringLiteral("cost"), amount * priceStored);
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
    q.prepare(QStringLiteral("DELETE FROM entries WHERE vehicle_id = ?"));
    q.addBindValue(m_activeVehicleId);
    q.exec();
    recompute();
    emit dataChanged();
}