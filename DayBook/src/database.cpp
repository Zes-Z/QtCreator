#include "database.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QTextStream>
#include <QRandomGenerator>
#include <QTime>
#include <QDateTime>
#include <QVariant>
#include <QDebug>
#include <algorithm>
#include <array>

namespace {
QString sqlQuote(QString value)
{
    value.replace(QStringLiteral("'"), QStringLiteral("''"));
    return QStringLiteral("'%1'").arg(value);
}

struct AccountSeed {
    const char *name;
    const char *type;
    const char *icon;
    const char *color;
    qint64 initialCents;
};

struct CategorySeed {
    const char *name;
    const char *kind;
    const char *icon;
    const char *color;
    int sort;
};

const std::array<AccountSeed, 5> kAccounts = {{
    {"现金钱包", "cash", "wallet", "#E8A94E", 120000},
    {"微信支付", "wallet", "chat", "#3FBF7F", 1950000},
    {"支付宝", "wallet", "account_balance", "#4F8EF7", 1500000},
    {"招商银行", "bank", "credit_card", "#C1503C", 880000},
    {"信用卡", "credit", "credit_card", "#8B7CF6", -286000},
}};

const std::array<CategorySeed, 14> kCategories = {{
    {"餐饮", "expense", "restaurant", "#F26D50", 0},
    {"交通", "expense", "directions_bus", "#4F8EF7", 1},
    {"购物", "expense", "shopping_bag", "#E5A13C", 2},
    {"居住", "expense", "home", "#8B7CF6", 3},
    {"娱乐", "expense", "movie", "#EC7FA9", 4},
    {"医疗", "expense", "health_and_safety", "#34B58A", 5},
    {"学习", "expense", "school", "#4C9EEB", 6},
    {"人情", "expense", "redeem", "#E2739A", 7},
    {"其他", "expense", "more_horiz", "#918A81", 8},
    {"工资", "income", "payments", "#2FAE83", 9},
    {"奖金", "income", "emoji_events", "#E5A13C", 10},
    {"理财", "income", "trending_up", "#4F8EF7", 11},
    {"其他收入", "income", "add_circle", "#918A81", 12},
    {"退款", "income", "currency_exchange", "#34B58A", 13},
}};

struct ExpenseSpec {
    const char *category;
    const char *account;
    int minYuan;
    int maxYuan;
    const char *notes[6];
};

const ExpenseSpec kCommon[] = {
    {"餐饮", "微信支付", 15, 38, {"早餐 · 豆浆油条", "咖啡", "奶茶", "面包", "水果", "豆浆"}},
    {"餐饮", "微信支付", 25, 72, {"午餐 · 公司楼下", "午餐 · 牛肉面", "午餐 · 轻食", "午餐 · 盖饭", "午餐 · 小馆", "午餐 · 沙拉"}},
    {"餐饮", "支付宝", 36, 168, {"晚餐 · 小馆", "晚餐 · 火锅", "晚餐 · 烧烤", "晚餐 · 日料", "晚餐 · 家常菜", "晚餐 · 面馆"}},
    {"餐饮", "支付宝", 18, 68, {"周末早午餐", "咖啡", "甜品", "夜宵", "水果", "下午茶"}},
    {"交通", "微信支付", 3, 12, {"地铁通勤", "公交", "共享单车", "地铁", "通勤", "换乘"}},
    {"交通", "支付宝", 18, 68, {"打车回家", "打车上班", "网约车", "高铁票", "停车费", "机场巴士"}},
    {"购物", "支付宝", 30, 260, {"超市采购", "日用品", "家居小物", "护肤", "衣服", "数码配件"}},
    {"购物", "信用卡", 99, 680, {"冬装", "运动鞋", "耳机配件", "厨房用品", "书籍", "香薰"}},
    {"娱乐", "微信支付", 25, 99, {"电影票", "视频会员", "桌游", "展览", "咖啡店坐坐", "演出"}},
    {"娱乐", "信用卡", 80, 360, {"朋友聚会", "演出门票", "短途旅行", "音乐会", "展览", "桌游"}},
    {"学习", "支付宝", 30, 199, {"电子书", "课程", "文具", "订阅", "讲座", "工具书"}},
    {"医疗", "支付宝", 18, 260, {"药店", "挂号", "维生素", "体检", "牙科", "眼药水"}},
    {"人情", "微信支付", 100, 600, {"朋友生日", "红包", "聚餐 AA", "伴手礼", "探望", "婚礼"}},
    {"其他", "微信支付", 20, 120, {"快递", "理发", "洗衣", "修鞋", "小费", "杂项"}},
};

int pickIndex(QRandomGenerator &rng, int count)
{
    return count > 0 ? int(rng.bounded(quint32(count))) : 0;
}

QString dueTime(QRandomGenerator &rng, const QDate &date, int earliestHour, int latestHour)
{
    int hour = earliestHour + int(rng.bounded(quint32(std::max(1, latestHour - earliestHour + 1))));
    int minute = int(rng.bounded(60));
    QTime t(hour, minute);
    if (date == QDate::currentDate()) {
        const QTime now = QTime::currentTime();
        if (t > now) {
            t = now.addSecs(-int(rng.bounded(7200)));
            if (t.hour() < 7)
                t = QTime(7, 30);
        }
    }
    return t.toString("HH:mm");
}
} // namespace

Database &Database::instance()
{
    static Database db;
    return db;
}

QString Database::dataDir() const
{
    const QString override = qEnvironmentVariable("DAYBOOK_DATA_DIR");
    if (!override.isEmpty())
        return override;
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/DayBook";
}

bool Database::init(QString *error)
{
    QDir().mkpath(dataDir());
    m_path = dataDir() + "/daybook.db";

    if (QSqlDatabase::contains("daybook")) {
        m_db = QSqlDatabase::database("daybook");
    } else {
        m_db = QSqlDatabase::addDatabase("QSQLITE", "daybook");
        m_db.setDatabaseName(m_path);
    }

    if (!m_db.isOpen() && !m_db.open()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }

    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA foreign_keys=ON");
    pragma.exec("PRAGMA synchronous=NORMAL");
    pragma.exec("PRAGMA busy_timeout=5000");

    if (!createSchema(error))
        return false;
    if (!ensureDefaults(error))
        return false;

    // A tasteful demo dataset on the very first launch, so the dashboard
    // is alive from the first second. Clearing the data later must not
    // bring the demo back on the next start.
    const bool firstRun = meta("first_run_done", "0") != "1";
    if (firstRun) {
        if (transactionCount() == 0 && !isDemoSeeded()) {
            if (!seedDemoData(error))
                return false;
        }
        setMeta("first_run_done", "1");
    }
    return true;
}

bool Database::createSchema(QString *error)
{
    const QStringList statements = {
        "CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS accounts ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " name TEXT NOT NULL,"
        " type TEXT NOT NULL DEFAULT 'cash',"
        " icon TEXT NOT NULL DEFAULT 'wallet',"
        " color TEXT NOT NULL DEFAULT '#F26D50',"
        " initial REAL NOT NULL DEFAULT 0,"
        " note TEXT NOT NULL DEFAULT '',"
        " archived INTEGER NOT NULL DEFAULT 0,"
        " sort INTEGER NOT NULL DEFAULT 0,"
        " created_at TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS categories ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " name TEXT NOT NULL,"
        " kind TEXT NOT NULL DEFAULT 'expense',"
        " icon TEXT NOT NULL DEFAULT 'category',"
        " color TEXT NOT NULL DEFAULT '#918A81',"
        " sort INTEGER NOT NULL DEFAULT 0)",
        "CREATE TABLE IF NOT EXISTS transactions ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " kind TEXT NOT NULL,"
        " amount REAL NOT NULL,"
        " account_id INTEGER NOT NULL REFERENCES accounts(id),"
        " transfer_account_id INTEGER NOT NULL DEFAULT 0,"
        " category_id INTEGER NOT NULL REFERENCES categories(id),"
        " note TEXT NOT NULL DEFAULT '',"
        " occurred_at TEXT NOT NULL,"
        " created_at TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS budgets ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " category_id INTEGER NOT NULL REFERENCES categories(id),"
        " month TEXT NOT NULL,"
        " amount REAL NOT NULL,"
        " UNIQUE(category_id, month))",
        "CREATE INDEX IF NOT EXISTS idx_tx_occurred ON transactions(occurred_at)",
        "CREATE INDEX IF NOT EXISTS idx_tx_category ON transactions(category_id)",
        "CREATE INDEX IF NOT EXISTS idx_tx_account ON transactions(account_id)",
        "CREATE INDEX IF NOT EXISTS idx_tx_month ON transactions(substr(occurred_at, 1, 7))",
    };

    for (const QString &sql : statements) {
        QSqlQuery q(m_db);
        if (!q.exec(sql)) {
            if (error)
                *error = q.lastError().text() + "\n" + sql;
            return false;
        }
    }

    // Existing databases predate transfer transactions. SQLite has no
    // IF NOT EXISTS for columns, so inspect the table first.
    bool hasTransferColumn = false;
    QSqlQuery columns(m_db);
    if (columns.exec("PRAGMA table_info(transactions)")) {
        while (columns.next()) {
            if (columns.value(1).toString() == "transfer_account_id") {
                hasTransferColumn = true;
                break;
            }
        }
    }
    if (!hasTransferColumn) {
        QSqlQuery alter(m_db);
        if (!alter.exec("ALTER TABLE transactions ADD COLUMN transfer_account_id INTEGER NOT NULL DEFAULT 0")) {
            if (error)
                *error = alter.lastError().text();
            return false;
        }
    }

    // Version 1 stored money as integer cents (amount_cents). Move existing
    // databases to true decimal columns and divide the old values by 100.
    auto migrateMoneyColumn = [this, error](const QString &table,
                                            const QString &oldColumn,
                                            const QString &newColumn) {
        QSqlQuery info(m_db);
        bool hasOld = false;
        bool hasNew = false;
        if (info.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
            while (info.next()) {
                const QString name = info.value(1).toString();
                if (name == oldColumn)
                    hasOld = true;
                if (name == newColumn)
                    hasNew = true;
            }
        }
        if (!hasOld)
            return true;
        if (!hasNew) {
            QSqlQuery add(m_db);
            if (!add.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 REAL NOT NULL DEFAULT 0")
                          .arg(table, newColumn))) {
                if (error)
                    *error = add.lastError().text();
                return false;
            }
        }
        QSqlQuery copy(m_db);
        if (!copy.exec(QStringLiteral("UPDATE %1 SET %2 = %3 / 100.0")
                       .arg(table, newColumn, oldColumn))) {
            if (error)
                *error = copy.lastError().text();
            return false;
        }
        QSqlQuery drop(m_db);
        if (!drop.exec(QStringLiteral("ALTER TABLE %1 DROP COLUMN %2").arg(table, oldColumn))) {
            if (error)
                *error = drop.lastError().text();
            return false;
        }
        return true;
    };

    if (!migrateMoneyColumn("accounts", "initial_cents", "initial")
        || !migrateMoneyColumn("transactions", "amount_cents", "amount")
        || !migrateMoneyColumn("budgets", "amount_cents", "amount")) {
        return false;
    }
    return true;
}

bool Database::ensureDefaults(QString *error)
{
    QSqlQuery countQuery(m_db);
    countQuery.exec("SELECT COUNT(*) FROM categories");
    const int categoryCount = countQuery.next() ? countQuery.value(0).toInt() : 0;

    if (categoryCount == 0) {
        if (!m_db.transaction()) {
            if (error)
                *error = m_db.lastError().text();
            return false;
        }
        for (const auto &c : kCategories) {
            QSqlQuery q(m_db);
            q.prepare("INSERT INTO categories(name, kind, icon, color, sort) VALUES(?,?,?,?,?)");
            q.addBindValue(QString::fromUtf8(c.name));
            q.addBindValue(QString::fromUtf8(c.kind));
            q.addBindValue(QString::fromUtf8(c.icon));
            q.addBindValue(QString::fromUtf8(c.color));
            q.addBindValue(c.sort);
            if (!q.exec()) {
                m_db.rollback();
                if (error)
                    *error = q.lastError().text();
                return false;
            }
        }
        if (!m_db.commit()) {
            if (error)
                *error = m_db.lastError().text();
            return false;
        }
    }

    QSqlQuery transferQuery(m_db);
    transferQuery.prepare("SELECT id FROM categories WHERE kind='transfer' LIMIT 1");
    if (transferQuery.exec() && !transferQuery.next()) {
        QSqlQuery insert(m_db);
        insert.prepare("INSERT INTO categories(name, kind, icon, color, sort)"
                       " VALUES('账户转移', 'transfer', 'sync', '#8C8377', 0)");
        if (!insert.exec()) {
            if (error)
                *error = insert.lastError().text();
            return false;
        }
    }

    QSqlQuery commQuery(m_db);
    commQuery.prepare("SELECT id FROM categories WHERE name='通讯' AND kind='expense' LIMIT 1");
    if (commQuery.exec() && !commQuery.next()) {
        QSqlQuery insert(m_db);
        insert.prepare("INSERT INTO categories(name, kind, icon, color, sort)"
                       " VALUES('通讯', 'expense', 'phone_iphone', '#5B9BD5', 7)");
        if (!insert.exec()) {
            if (error)
                *error = insert.lastError().text();
            return false;
        }
    }

    countQuery.exec("SELECT COUNT(*) FROM accounts");
    const int accountCount = countQuery.next() ? countQuery.value(0).toInt() : 0;
    if (accountCount == 0) {
        if (!m_db.transaction()) {
            if (error)
                *error = m_db.lastError().text();
            return false;
        }
        int sort = 0;
        for (const auto &a : kAccounts) {
            QSqlQuery q(m_db);
            q.prepare("INSERT INTO accounts(name, type, icon, color, initial, note, sort, created_at)"
                      " VALUES(?,?,?,?,?,?,?,?)");
            q.addBindValue(QString::fromUtf8(a.name));
            q.addBindValue(QString::fromUtf8(a.type));
            q.addBindValue(QString::fromUtf8(a.icon));
            q.addBindValue(QString::fromUtf8(a.color));
            q.addBindValue(a.initialCents / 100.0);
            q.addBindValue(QStringLiteral(""));
            q.addBindValue(sort++);
            q.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
            if (!q.exec()) {
                m_db.rollback();
                if (error)
                    *error = q.lastError().text();
                return false;
            }
        }
        if (!m_db.commit()) {
            if (error)
                *error = m_db.lastError().text();
            return false;
        }
    }
    return true;
}

bool Database::setMeta(const QString &key, const QString &value)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO meta(key, value) VALUES(?,?)"
              " ON CONFLICT(key) DO UPDATE SET value=excluded.value");
    q.addBindValue(key);
    q.addBindValue(value);
    return q.exec();
}

QString Database::meta(const QString &key, const QString &fallback) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM meta WHERE key=?");
    q.addBindValue(key);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return fallback;
}

int Database::categoryId(const QString &name, const QString &kind) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM categories WHERE name=? AND kind=? LIMIT 1");
    q.addBindValue(name);
    q.addBindValue(kind);
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}

int Database::accountId(const QString &name) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM accounts WHERE name=? LIMIT 1");
    q.addBindValue(name);
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}

bool Database::hasTransactions() const
{
    return transactionCount() > 0;
}

int Database::transactionCount() const
{
    QSqlQuery q(m_db);
    if (q.exec("SELECT COUNT(*) FROM transactions") && q.next())
        return q.value(0).toInt();
    return 0;
}

bool Database::isDemoSeeded() const
{
    return meta("demo_seeded", "0") == "1";
}

bool Database::clearFinancialData(QString *error)
{
    if (!m_db.transaction()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }
    QSqlQuery q(m_db);
    if (!q.exec("DELETE FROM budgets") || !q.exec("DELETE FROM transactions")) {
        m_db.rollback();
        if (error)
            *error = q.lastError().text();
        return false;
    }
    if (!setMeta("demo_seeded", "0")) {
        m_db.rollback();
        if (error)
            *error = m_db.lastError().text();
        return false;
    }
    if (!m_db.commit()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }
    return true;
}

bool Database::seedDemoData(QString *error)
{
    if (!clearFinancialData(error))
        return false;

    QRandomGenerator rng(0xDA7B00C5u);
    const QDate today = QDate::currentDate();

    // Cache category / account ids.
    QHash<QString, int> catIds;
    for (const auto &c : kCategories) {
        const QString name = QString::fromUtf8(c.name);
        catIds[name + "|" + QString::fromUtf8(c.kind)] = categoryId(name, QString::fromUtf8(c.kind));
    }
    QHash<QString, int> accIds;
    for (const auto &a : kAccounts)
        accIds[QString::fromUtf8(a.name)] = accountId(QString::fromUtf8(a.name));

    auto addTransaction = [&](const QString &kind, const QString &cat, const QString &account,
                              qint64 cents, const QDate &date, const QString &time,
                              const QString &note) -> bool {
        if (!date.isValid() || date > today)
            return true;
        const int cid = catIds.value(cat + "|" + kind, 0);
        const int aid = accIds.value(account, 0);
        if (cid <= 0 || aid <= 0) {
            qWarning() << "Missing category/account for demo" << kind << cat << account;
            return false;
        }
        QSqlQuery q(m_db);
        q.prepare("INSERT INTO transactions(kind, amount, account_id, category_id, note, occurred_at, created_at)"
                  " VALUES(?,?,?,?,?,?,?)");
        q.addBindValue(kind);
        q.addBindValue(cents / 100.0);
        q.addBindValue(aid);
        q.addBindValue(cid);
        q.addBindValue(note);
        q.addBindValue(date.toString("yyyy-MM-dd") + " " + time + ":00");
        q.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
        if (!q.exec()) {
            qWarning() << "Demo transaction failed:" << q.lastError().text()
                       << "kind" << kind << "category" << cat << "account" << account
                       << "cents" << cents << "date" << date << "noteNull" << note.isNull()
                       << "note" << note;
            if (error)
                *error = q.lastError().text();
            return false;
        }
        return true;
    };

    auto pickAccount = [&](const char *suggested) -> QString {
        const QString s = QString::fromUtf8(suggested);
        if (accIds.value(s, 0) > 0)
            return s;
        return "微信支付";
    };

    if (!m_db.transaction()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }

    bool ok = true;

    for (int back = 5; back >= 0 && ok; --back) {
        const QDate anyDay = today.addMonths(-back);
        const QDate start(anyDay.year(), anyDay.month(), 1);
        const QDate end = (back == 0) ? today : start.addMonths(1).addDays(-1);
        if (!end.isValid() || end < start)
            continue;

        // Fixed monthly items
        if (start <= end) {
            ok = ok && addTransaction("expense", "居住", "招商银行", 350000, start, "09:20", "房租");
            ok = ok && addTransaction("expense", "居住", "招商银行", 26800 + qint64(rng.bounded(8000)), start.addDays(2), "20:15", "电费 · 燃气费");
            ok = ok && addTransaction("expense", "居住", "支付宝", 9900, start.addDays(3), "10:02", "宽带续费");
            ok = ok && addTransaction("expense", "居住", "微信支付", 5900, start.addDays(7), "08:40", "手机话费");
            ok = ok && addTransaction("expense", "娱乐", "支付宝", 2500, start.addDays(4), "21:10", "视频会员");
            if (back % 2 == 0)
                ok = ok && addTransaction("expense", "娱乐", "微信支付", 19900, start.addDays(5), "19:30", "健身月卡");
        }
        if (QDate(start.year(), start.month(), 10) <= end) {
            ok = ok && addTransaction("income", "工资", "招商银行", 1850000, QDate(start.year(), start.month(), 10), "09:12", "月度工资");
        }

        // Everyday expenses
        for (int day = 1; day <= end.day() && ok; ++day) {
            const QDate date(start.year(), start.month(), day);
            const int weekday = date.dayOfWeek();
            const bool weekend = weekday >= 6;

            auto chance = [&](int percent) { return int(rng.bounded(100)) < percent; };

            if (chance(82)) {
                const auto &spec = kCommon[0];
                const int yuan = spec.minYuan + int(rng.bounded(spec.maxYuan - spec.minYuan + 1));
                ok = ok && addTransaction("expense", spec.category, pickAccount(spec.account),
                                          qint64(yuan) * 100, date, dueTime(rng, date, 7, 9),
                                          QString::fromUtf8(spec.notes[pickIndex(rng, 6)]));
            }
            if (chance(weekend ? 70 : 55)) {
                const auto &spec = kCommon[1];
                const int yuan = spec.minYuan + int(rng.bounded(spec.maxYuan - spec.minYuan + 1));
                ok = ok && addTransaction("expense", spec.category, pickAccount(spec.account),
                                          qint64(yuan) * 100, date, dueTime(rng, date, 11, 13),
                                          QString::fromUtf8(spec.notes[pickIndex(rng, 6)]));
            }
            if (chance(weekend ? 72 : 42)) {
                const auto &spec = kCommon[2];
                const int yuan = spec.minYuan + int(rng.bounded(spec.maxYuan - spec.minYuan + 1));
                const QString account = chance(58) ? "微信支付" : "支付宝";
                ok = ok && addTransaction("expense", spec.category, pickAccount(account.toUtf8().constData()),
                                          qint64(yuan) * 100, date, dueTime(rng, date, 18, 21),
                                          QString::fromUtf8(spec.notes[pickIndex(rng, 6)]));
            }
            if (chance(weekend ? 18 : 38)) {
                const auto &spec = kCommon[3];
                const int yuan = spec.minYuan + int(rng.bounded(spec.maxYuan - spec.minYuan + 1));
                ok = ok && addTransaction("expense", spec.category, pickAccount(spec.account),
                                          qint64(yuan) * 100, date, dueTime(rng, date, 8, 22),
                                          QString::fromUtf8(spec.notes[pickIndex(rng, 6)]));
            }
            if (chance(42)) {
                const auto &spec = kCommon[chance(50) ? 4 : 5];
                const int yuan = spec.minYuan + int(rng.bounded(spec.maxYuan - spec.minYuan + 1));
                ok = ok && addTransaction("expense", spec.category, pickAccount(spec.account),
                                          qint64(yuan) * 100, date, dueTime(rng, date, 8, 21),
                                          QString::fromUtf8(spec.notes[pickIndex(rng, 6)]));
            }
            if (chance(weekend ? 22 : 9)) {
                const auto &spec = kCommon[6 + pickIndex(rng, 2)];
                const int yuan = spec.minYuan + int(rng.bounded(spec.maxYuan - spec.minYuan + 1));
                ok = ok && addTransaction("expense", spec.category, pickAccount(spec.account),
                                          qint64(yuan) * 100, date, dueTime(rng, date, 12, 22),
                                          QString::fromUtf8(spec.notes[pickIndex(rng, 6)]));
            }
            if (chance(weekend ? 16 : 8)) {
                const auto &spec = kCommon[8 + pickIndex(rng, 2)];
                const int yuan = spec.minYuan + int(rng.bounded(spec.maxYuan - spec.minYuan + 1));
                ok = ok && addTransaction("expense", spec.category, pickAccount(spec.account),
                                          qint64(yuan) * 100, date, dueTime(rng, date, 14, 22),
                                          QString::fromUtf8(spec.notes[pickIndex(rng, 6)]));
            }
            if (chance(5)) {
                const auto &spec = kCommon[10 + pickIndex(rng, 2)];
                const int yuan = spec.minYuan + int(rng.bounded(spec.maxYuan - spec.minYuan + 1));
                ok = ok && addTransaction("expense", spec.category, pickAccount(spec.account),
                                          qint64(yuan) * 100, date, dueTime(rng, date, 10, 20),
                                          QString::fromUtf8(spec.notes[pickIndex(rng, 6)]));
            }
            if (chance(3)) {
                const auto &spec = kCommon[12 + pickIndex(rng, 2)];
                const int yuan = spec.minYuan + int(rng.bounded(spec.maxYuan - spec.minYuan + 1));
                ok = ok && addTransaction("expense", spec.category, pickAccount(spec.account),
                                          qint64(yuan) * 100, date, dueTime(rng, date, 10, 21),
                                          QString::fromUtf8(spec.notes[pickIndex(rng, 6)]));
            }
        }

        // A few memorable one-off items
        if (back == 1 && ok) {
            const QDate d = QDate(start.year(), start.month(), qMin(12, end.day()));
            ok = ok && addTransaction("expense", "人情", "微信支付", 80000, d, "18:20", "朋友婚礼");
            ok = ok && addTransaction("income", "奖金", "招商银行", 300000, qMin(QDate(start.year(), start.month(), 15), end), "10:05", "季度奖金");
        }
        if (back == 2 && ok) {
            const QDate d = QDate(start.year(), start.month(), qMin(18, end.day()));
            ok = ok && addTransaction("expense", "购物", "信用卡", 129900, d, "20:18", "降噪耳机");
            ok = ok && addTransaction("income", "理财", "招商银行", 12860, qMin(QDate(start.year(), start.month(), 20), end), "15:40", "基金收益");
        }
        if (back == 3 && ok) {
            const QDate d = QDate(start.year(), start.month(), qMin(22, end.day()));
            ok = ok && addTransaction("expense", "娱乐", "信用卡", 68000, d, "19:45", "演唱会门票");
            ok = ok && addTransaction("expense", "学习", "支付宝", 29900, qMin(QDate(start.year(), start.month(), 16), end), "21:00", "线上课程");
        }
        if (back == 4 && ok) {
            const QDate d = QDate(start.year(), start.month(), qMin(9, end.day()));
            ok = ok && addTransaction("expense", "购物", "信用卡", 89900, d, "13:30", "机械键盘");
            ok = ok && addTransaction("income", "其他收入", "支付宝", 150000, qMin(QDate(start.year(), start.month(), 24), end), "17:00", "周末兼职");
        }
        if (back == 5 && ok) {
            const QDate d = QDate(start.year(), start.month(), qMin(6, end.day()));
            ok = ok && addTransaction("expense", "医疗", "支付宝", 42000, d, "09:10", "年度体检");
            ok = ok && addTransaction("income", "奖金", "招商银行", 200000, qMin(QDate(start.year(), start.month(), 28), end), "11:30", "项目奖励");
        }
    }

    // Current month budgets
    struct BudgetSeed { const char *category; int amount; };
    const BudgetSeed budgets[] = {
        {"餐饮", 180000}, {"交通", 50000}, {"购物", 150000},
        {"娱乐", 70000}, {"学习", 40000}, {"居住", 380000},
    };
    const QString month = today.toString("yyyy-MM");
    for (const auto &b : budgets) {
        const int cid = catIds.value(QString::fromUtf8(b.category) + "|expense", 0);
        if (cid <= 0)
            continue;
        QSqlQuery q(m_db);
        q.prepare("INSERT INTO budgets(category_id, month, amount) VALUES(?,?,?)"
                  " ON CONFLICT(category_id, month) DO UPDATE SET amount=excluded.amount");
        q.addBindValue(cid);
        q.addBindValue(month);
        q.addBindValue(b.amount / 100.0);
        if (!q.exec()) {
            ok = false;
            if (error)
                *error = q.lastError().text();
            break;
        }
    }

    if (!ok) {
        m_db.rollback();
        return false;
    }
    setMeta("demo_seeded", "1");
    setMeta("demo_generated_at", QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!m_db.commit()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }
    return true;
}

bool Database::exportCsv(const QString &filePath, QString *error) const
{
    QString path = filePath;
    if (!path.endsWith(".csv", Qt::CaseInsensitive))
        path += ".csv";

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << QChar(0xFEFF);
    out << "日期,时间,类型,分类,账户,金额,备注\n";

    QSqlQuery q(m_db);
    q.exec("SELECT t.occurred_at, t.kind, c.name, a.name, t.amount, t.note"
           " FROM transactions t JOIN categories c ON c.id=t.category_id"
           " JOIN accounts a ON a.id=t.account_id"
           " ORDER BY t.occurred_at DESC, t.id DESC");
    auto escape = [](QString value) {
        value.replace('"', "\"\"");
        if (value.contains(',') || value.contains('"') || value.contains('\n'))
            value = "\"" + value + "\"";
        return value;
    };
    while (q.next()) {
        const QString occurred = q.value(0).toString();
        const QString date = occurred.left(10);
        const QString time = occurred.mid(11, 5);
        const QString kind = q.value(1).toString() == "income" ? "收入" : "支出";
        const double amount = q.value(4).toDouble();
        out << escape(date) << ',' << escape(time) << ',' << escape(kind) << ','
            << escape(q.value(2).toString()) << ',' << escape(q.value(3).toString()) << ','
            << QString::number(amount, 'f', 2) << ',' << escape(q.value(5).toString()) << '\n';
    }
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool Database::backup(const QString &filePath, QString *error) const
{
    QFileInfo info(filePath);
    QDir().mkpath(info.absolutePath());
    QFile::remove(filePath);

    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("VACUUM INTO %1").arg(sqlQuote(filePath)))) {
        if (error)
            *error = q.lastError().text();
        return false;
    }
    return true;
}
