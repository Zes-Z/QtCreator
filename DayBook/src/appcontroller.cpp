#include "appcontroller.h"
#include "models.h"
#include "database.h"

#include <QClipboard>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHash>
#include <QLocale>
#include <QPair>
#include <QRegularExpression>
#include <QStringList>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStyleHints>
#include <QUrl>
#include <QVariant>
#include <QtMath>
#include <algorithm>
#include <utility>

namespace {
QString groupedCents(qint64 cents)
{
    const bool negative = cents < 0;
    QString digits = QString::number(qAbs(cents));
    while (digits.size() < 3)
        digits.prepend('0');
    const QString fraction = digits.right(2);
    QString integer = digits.left(digits.size() - 2);
    QString grouped;
    int counter = 0;
    for (int i = integer.size() - 1; i >= 0; --i) {
        grouped.prepend(integer.at(i));
        if (++counter % 3 == 0 && i > 0)
            grouped.prepend(',');
    }
    return (negative ? "-" : "") + grouped + "." + fraction;
}

QString weekdayShort(const QDate &date)
{
    static const char *names[] = {"周一", "周二", "周三", "周四", "周五", "周六", "周日"};
    if (!date.isValid())
        return {};
    return QString::fromUtf8(names[qBound(1, date.dayOfWeek(), 7) - 1]);
}

QDateTime parseOccurred(const QString &date, const QString &time)
{
    QDate d = QDate::fromString(date, "yyyy-MM-dd");
    if (!d.isValid())
        d = QDate::currentDate();
    QTime t = QTime::fromString(time, "HH:mm");
    if (!t.isValid())
        t = QTime::fromString(time, "HH:mm:ss");
    if (!t.isValid())
        t = QTime::currentTime();
    return QDateTime(d, t);
}

QString localPathFromInput(const QString &input)
{
    const QUrl url(input);
    if (url.isLocalFile())
        return url.toLocalFile();
    return input;
}

QString symbolForCurrency(const QString &code)
{
    if (code == "USD")
        return QStringLiteral("$");
    if (code == "EUR")
        return QStringLiteral("€");
    if (code == "JPY")
        return QStringLiteral("¥");
    if (code == "GBP")
        return QStringLiteral("£");
    if (code == "HKD")
        return QStringLiteral("HK$");
    return QStringLiteral("¥");
}

bool isKnownCurrency(const QString &code)
{
    return code == "CNY" || code == "USD" || code == "EUR" || code == "JPY"
        || code == "GBP" || code == "HKD";
}

const QStringList &navKeys()
{
    static const QStringList keys = {"overview", "transactions", "insights", "accounts", "budgets"};
    return keys;
}

const QStringList &summaryKeys()
{
    static const QStringList keys = {"cashflow", "budget", "reserve"};
    return keys;
}

QStringList normalizeOrder(const QStringList &requested, const QStringList &allowed,
                           const QStringList &fallback)
{
    QStringList result;
    for (const QString &key : requested) {
        if (allowed.contains(key) && !result.contains(key))
            result.append(key);
    }
    for (const QString &key : fallback) {
        if (allowed.contains(key) && !result.contains(key))
            result.append(key);
    }
    for (const QString &key : allowed) {
        if (!result.contains(key))
            result.append(key);
    }
    return result;
}
} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_settings()
{
}

AppController::~AppController() = default;

bool AppController::initialize(QString *error)
{
    if (!Database::instance().init(error))
        return false;

    m_themeMode = m_settings.value("appearance/theme", "system").toString();
    if (m_themeMode != "light" && m_themeMode != "dark" && m_themeMode != "system")
        m_themeMode = "system";
    m_accentName = m_settings.value("appearance/accent", "coral").toString();
    if (m_accentName != "coral" && m_accentName != "sage" && m_accentName != "azure"
        && m_accentName != "amber" && m_accentName != "plum" && m_accentName != "pollen"
        && m_accentName != "custom")
        m_accentName = "coral";
    m_customAccent = m_settings.value("appearance/customAccent", "#E85D75").toString();
    if (!QColor(m_customAccent).isValid())
        m_customAccent = "#E85D75";

    if (qEnvironmentVariableIsSet("DAYBOOK_THEME")) {
        const QString envTheme = qEnvironmentVariable("DAYBOOK_THEME");
        if (envTheme == "light" || envTheme == "dark" || envTheme == "system")
            m_themeMode = envTheme;
    }
    if (qEnvironmentVariableIsSet("DAYBOOK_CUSTOM_ACCENT")) {
        const QColor envCustom(qEnvironmentVariable("DAYBOOK_CUSTOM_ACCENT"));
        if (envCustom.isValid())
            m_customAccent = envCustom.name(QColor::HexRgb);
    }
    if (qEnvironmentVariableIsSet("DAYBOOK_ACCENT")) {
        const QString envAccent = qEnvironmentVariable("DAYBOOK_ACCENT");
        if (envAccent == "coral" || envAccent == "sage" || envAccent == "azure"
            || envAccent == "amber" || envAccent == "plum" || envAccent == "pollen"
            || envAccent == "custom")
            m_accentName = envAccent;
    }
    m_currencyCode = m_settings.value("appearance/currency", "CNY").toString();
    if (!isKnownCurrency(m_currencyCode))
        m_currencyCode = "CNY";
    if (qEnvironmentVariableIsSet("DAYBOOK_CURRENCY")) {
        const QString envCurrency = qEnvironmentVariable("DAYBOOK_CURRENCY").toUpper();
        if (isKnownCurrency(envCurrency))
            m_currencyCode = envCurrency;
    }

    m_navOrder = normalizeOrder(m_settings.value("ui/navOrder", navKeys()).toStringList(),
                                navKeys(), navKeys());
    m_summaryOrder = normalizeOrder(m_settings.value("ui/summaryOrder", summaryKeys()).toStringList(),
                                    summaryKeys(), summaryKeys());

    m_month = QDate::currentDate().toString("yyyy-MM");

    const int fontId = QFontDatabase::addApplicationFont(":/assets/fonts/DayBookIcons.ttf");
    if (fontId >= 0) {
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty())
            m_iconFontFamily = families.constFirst();
    }

    const QStringList families = QFontDatabase::families();
    auto pickFamily = [&families](const QStringList &candidates, const QString &fallback) {
        for (const QString &candidate : candidates) {
            if (families.contains(candidate))
                return candidate;
        }
        return fallback;
    };
#ifdef Q_OS_MACOS
    m_fontFamily = pickFamily({"PingFang SC", "Hiragino Sans GB", "STHeiti"}, "PingFang SC");
    m_numberFamily = pickFamily({"SF Pro Display", "SF Pro Text", "Helvetica Neue"}, m_fontFamily);
#elif defined(Q_OS_WIN)
    m_fontFamily = pickFamily({"Microsoft YaHei UI", "Microsoft YaHei", "Segoe UI"}, "Microsoft YaHei UI");
    m_numberFamily = pickFamily({"Segoe UI Variable Display", "Segoe UI", "Bahnschrift"}, m_fontFamily);
#else
    m_fontFamily = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    m_numberFamily = m_fontFamily;
#endif
    if (m_fontFamily.isEmpty())
        m_fontFamily = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    m_defaultFontFamily = m_fontFamily;

    auto addFontOption = [this, &families](const QString &key, const QString &label,
                                           const QStringList &candidates) {
        const QString family = [&]() {
            for (const QString &candidate : candidates) {
                if (families.contains(candidate))
                    return candidate;
            }
            return QString();
        }();
        if (family.isEmpty() || family == m_defaultFontFamily)
            return;
        for (const QVariant &existing : std::as_const(m_fontOptions)) {
            if (existing.toMap().value("family").toString() == family)
                return;
        }
        m_fontOptions.append(QVariantMap{
            {"key", key}, {"label", label}, {"family", family}
        });
    };

    m_fontOptions.append(QVariantMap{
        {"key", "system"}, {"label", "系统"}, {"family", m_defaultFontFamily}
    });
#ifdef Q_OS_MACOS
    addFontOption("serif", "宋体", {"Songti SC", "STSong", "SimSun"});
    addFontOption("rounded", "圆体", {"Yuanti SC", "YouYuan"});
    addFontOption("kai", "楷体", {"Kaiti SC", "STKaiti", "KaiTi"});
#elif defined(Q_OS_WIN)
    addFontOption("serif", "宋体", {"SimSun", "NSimSun"});
    addFontOption("rounded", "圆体", {"YouYuan", "Microsoft YaHei UI"});
    addFontOption("kai", "楷体", {"KaiTi", "STKaiti"});
#endif

    m_fontChoice = m_settings.value("appearance/font", "system").toString();
    bool fontChoiceFound = false;
    for (const QVariant &option : std::as_const(m_fontOptions)) {
        const QVariantMap map = option.toMap();
        if (map.value("key").toString() == m_fontChoice) {
            m_fontFamily = map.value("family").toString();
            fontChoiceFound = true;
            break;
        }
    }
    if (!fontChoiceFound) {
        m_fontChoice = "system";
        m_fontFamily = m_defaultFontFamily;
    }
    if (qEnvironmentVariableIsSet("DAYBOOK_FONT")) {
        const QString envFont = qEnvironmentVariable("DAYBOOK_FONT");
        for (const QVariant &option : std::as_const(m_fontOptions)) {
            const QVariantMap map = option.toMap();
            if (map.value("key").toString() == envFont) {
                m_fontChoice = envFont;
                m_fontFamily = map.value("family").toString();
                break;
            }
        }
    }

    m_transactions = new TransactionModel(this);
    m_accountBills = new TransactionModel(this);
    m_accounts = new AccountModel(this);
    m_categories = new CategoryModel(this);
    m_budgets = new BudgetModel(this);

    if (QStyleHints *hints = QGuiApplication::styleHints()) {
        connect(hints, &QStyleHints::colorSchemeChanged, this, [this]() {
            emit systemDarkChanged();
            emit appearanceChanged();
        });
    }

    refreshAll();
    return true;
}

bool AppController::systemDark() const
{
    if (QStyleHints *hints = QGuiApplication::styleHints())
        return hints->colorScheme() == Qt::ColorScheme::Dark;
    return false;
}

bool AppController::isDark() const
{
    return m_themeMode == "dark" || (m_themeMode == "system" && systemDark());
}

QString AppController::monthLabel() const
{
    const QDate date = monthStart();
    return QStringLiteral("%1年%2月").arg(date.year()).arg(date.month());
}

QString AppController::monthShortLabel() const
{
    return QStringLiteral("%1月").arg(monthStart().month());
}

bool AppController::isCurrentMonth() const
{
    return m_month == QDate::currentDate().toString("yyyy-MM");
}

QDate AppController::monthStart() const
{
    const QDate date = QDate::fromString(m_month + "-01", "yyyy-MM-dd");
    return date.isValid() ? date : QDate::currentDate();
}

void AppController::setCurrentMonth(const QString &month)
{
    QDate parsed = QDate::fromString(month + "-01", "yyyy-MM-dd");
    if (!parsed.isValid())
        return;
    const QString normalized = parsed.toString("yyyy-MM");
    if (normalized == m_month)
        return;
    m_month = normalized;
    emit currentMonthChanged();
    refreshAll();
}

void AppController::shiftMonth(int delta)
{
    setCurrentMonth(monthStart().addMonths(delta).toString("yyyy-MM"));
}

void AppController::goToToday()
{
    setCurrentMonth(QDate::currentDate().toString("yyyy-MM"));
}

void AppController::setThemeMode(const QString &mode)
{
    if (mode != "light" && mode != "dark" && mode != "system")
        return;
    if (m_themeMode == mode)
        return;
    m_themeMode = mode;
    m_settings.setValue("appearance/theme", mode);
    emit themeModeChanged();
    emit appearanceChanged();
}

void AppController::setAccentName(const QString &name)
{
    if (name != "coral" && name != "sage" && name != "azure" && name != "amber"
        && name != "plum" && name != "pollen" && name != "custom")
        return;
    if (m_accentName == name)
        return;
    m_accentName = name;
    m_settings.setValue("appearance/accent", name);
    emit accentNameChanged();
}

void AppController::setFontChoice(const QString &key)
{
    for (const QVariant &option : std::as_const(m_fontOptions)) {
        const QVariantMap map = option.toMap();
        if (map.value("key").toString() != key)
            continue;
        const QString family = map.value("family").toString();
        if (family.isEmpty() || (m_fontChoice == key && m_fontFamily == family))
            return;
        m_fontChoice = key;
        m_fontFamily = family;
        m_settings.setValue("appearance/font", key);
        emit fontChanged();
        return;
    }
}

void AppController::setCustomAccent(const QColor &color)
{
    if (!color.isValid())
        return;
    const QString hex = color.name(QColor::HexRgb);
    m_customAccent = hex;
    m_settings.setValue("appearance/customAccent", hex);
    emit customAccentChanged();
    if (m_accentName != "custom") {
        m_accentName = "custom";
        m_settings.setValue("appearance/accent", m_accentName);
        emit accentNameChanged();
    }
}

QString AppController::currencySymbol() const
{
    return symbolForCurrency(m_currencyCode);
}

void AppController::setCurrencyCode(const QString &code)
{
    const QString upper = code.trimmed().toUpper();
    if (!isKnownCurrency(upper) || m_currencyCode == upper)
        return;
    m_currencyCode = upper;
    m_settings.setValue("appearance/currency", m_currencyCode);
    emit currencyChanged();
    emit summaryChanged();
}

void AppController::setNavOrder(const QStringList &order)
{
    const QStringList normalized = normalizeOrder(order, navKeys(), navKeys());
    if (m_navOrder == normalized)
        return;
    m_navOrder = normalized;
    m_settings.setValue("ui/navOrder", m_navOrder);
    emit uiOrderChanged();
}

void AppController::setSummaryOrder(const QStringList &order)
{
    const QStringList normalized = normalizeOrder(order, summaryKeys(), summaryKeys());
    if (m_summaryOrder == normalized)
        return;
    m_summaryOrder = normalized;
    m_settings.setValue("ui/summaryOrder", m_summaryOrder);
    emit uiOrderChanged();
}

void AppController::moveNavItem(int from, int to)
{
    if (from < 0 || to < 0 || from >= m_navOrder.size() || to >= m_navOrder.size()
        || from == to)
        return;
    m_navOrder.move(from, to);
    m_settings.setValue("ui/navOrder", m_navOrder);
    emit uiOrderChanged();
}

void AppController::moveSummaryItem(int from, int to)
{
    if (from < 0 || to < 0 || from >= m_summaryOrder.size() || to >= m_summaryOrder.size()
        || from == to)
        return;
    m_summaryOrder.move(from, to);
    m_settings.setValue("ui/summaryOrder", m_summaryOrder);
    emit uiOrderChanged();
}

void AppController::setLastError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

int AppController::monthBudgetPercent() const
{
    return m_monthBudget > 0.0 ? int(qRound(m_monthBudgetSpent / m_monthBudget * 100.0)) : 0;
}

QString AppController::dataPath() const
{
    return Database::instance().path();
}

QString AppController::version() const
{
    return QStringLiteral("1.0.0");
}

QString AppController::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

bool AppController::isMac() const
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

QString AppController::formatAmountInput(const QString &text) const
{
    QString raw = text.trimmed();
    const bool negative = raw.startsWith('-');
    if (negative)
        raw.remove(0, 1);
    raw.remove(',');
    const int dot = raw.indexOf('.');
    QString integer = dot >= 0 ? raw.left(dot) : raw;
    QString fraction = dot >= 0 ? raw.mid(dot + 1) : QString();
    integer.remove(QRegularExpression("[^0-9]"));
    fraction.remove(QRegularExpression("[^0-9]")).truncate(2);
    if (integer.isEmpty() && dot < 0)
        return QString();
    if (integer.isEmpty())
        integer = QStringLiteral("0");
    QString grouped;
    for (int i = 0; i < integer.size(); ++i) {
        if (i > 0 && (integer.size() - i) % 3 == 0)
            grouped += QLatin1Char(',');
        grouped += integer.at(i);
    }
    if (negative && grouped != QStringLiteral("0"))
        grouped.prepend(QLatin1Char('-'));
    if (dot >= 0)
        grouped += QLatin1Char('.') + fraction;
    return grouped;
}

QString AppController::money(double value) const
{
    const QString formatted = groupedCents(qRound64(value * 100.0));
    if (formatted.startsWith('-'))
        return QStringLiteral("-") + currencySymbol() + formatted.mid(1);
    return currencySymbol() + formatted;
}

QString AppController::moneyPlain(double value) const
{
    return groupedCents(qRound64(value * 100.0));
}

QString AppController::signedMoney(double value, bool income) const
{
    return (income ? QStringLiteral("+") : QStringLiteral("-"))
           + currencySymbol() + groupedCents(qRound64(qAbs(value) * 100.0));
}

QString AppController::compactNumber(double value) const
{
    const double a = qAbs(value);
    const QString sign = value < 0 ? "-" : "";
    if (a >= 10000.0)
        return sign + QString::number(a / 10000.0, 'f', a >= 100000.0 ? 0 : 1) + "万";
    if (a >= 1000.0)
        return sign + QString::number(a / 1000.0, 'f', 1) + "k";
    return sign + QString::number(a, 'f', 0);
}

QString AppController::percentLabel(double value) const
{
    const double a = qAbs(value);
    if (a < 0.05)
        return QStringLiteral("0%");
    return (value > 0 ? "+" : "-") + QString::number(a, 'f', a < 10 ? 1 : 0) + "%";
}

AppController::MonthTotals AppController::totalsForMonth(const QString &month) const
{
    MonthTotals totals;
    QSqlQuery q(Database::instance().db());
    q.prepare("SELECT kind, SUM(amount), COUNT(*) FROM transactions"
              " WHERE substr(occurred_at,1,7)=:month GROUP BY kind");
    q.bindValue(":month", month);
    if (q.exec()) {
        while (q.next()) {
            const double sum = q.value(1).toDouble();
            const QString kind = q.value(0).toString();
            if (kind == "income")
                totals.income += sum;
            else if (kind == "expense")
                totals.expense += sum;
            totals.count += q.value(2).toInt();
        }
    }
    return totals;
}

void AppController::refreshAll()
{
    if (!m_transactions)
        return;
    m_transactions->setMonth(m_month);
    m_accountBills->setMonth(m_month);
    m_accounts->setMonth(m_month);
    m_categories->setMonth(m_month);
    m_budgets->setMonth(m_month);
    m_transactions->reload();
    m_accountBills->reload();
    m_accounts->reload();
    m_categories->reload();
    m_budgets->reload();
    refreshSummary();
    emit dataChanged();
}

void AppController::setAccountBillsAccount(int accountId)
{
    if (!m_accountBills)
        return;
    m_accountBills->clearFilters();
    m_accountBills->setMonth(m_month);
    m_accountBills->setAccountFilter(accountId);
}

void AppController::refreshSummary()
{
    const MonthTotals current = totalsForMonth(m_month);
    const MonthTotals previous = totalsForMonth(monthStart().addMonths(-1).toString("yyyy-MM"));

    m_monthIncome = current.income;
    m_monthExpense = current.expense;
    m_monthTransactionCount = current.count;
    m_lastMonthExpense = previous.expense;
    m_lastMonthIncome = previous.income;
    m_expenseChange = previous.expense > 0
            ? 100.0 * (current.expense - previous.expense) / previous.expense
            : 0.0;
    m_incomeChange = previous.income > 0
            ? 100.0 * (current.income - previous.income) / previous.income
            : 0.0;

    const QDate start = monthStart();
    int elapsedDays = start.daysInMonth();
    if (isCurrentMonth())
        elapsedDays = qMin(QDate::currentDate().day(), elapsedDays);
    m_dailyAverage = elapsedDays > 0 ? m_monthExpense / elapsedDays : 0.0;

    // Net worth includes archived accounts: archiving hides an account from
    // the account grid, but its history still belongs to the ledger.
    m_totalAssets = 0.0;
    m_totalLiabilities = 0.0;
    {
        QSqlQuery q(Database::instance().db());
        q.exec("SELECT a.initial"
               " + COALESCE((SELECT SUM(CASE WHEN t.kind='income' THEN t.amount"
               "   WHEN t.kind='expense' THEN -t.amount END)"
               "   FROM transactions t WHERE t.account_id=a.id AND t.kind IN ('income','expense')), 0)"
               " + COALESCE((SELECT SUM(CASE WHEN t.account_id=a.id THEN -t.amount"
               "   WHEN t.transfer_account_id=a.id THEN t.amount END)"
               "   FROM transactions t WHERE t.kind='transfer'"
               "   AND (t.account_id=a.id OR t.transfer_account_id=a.id)), 0)"
               " FROM accounts a");
        while (q.next()) {
            const double balance = q.value(0).toDouble();
            if (balance >= 0)
                m_totalAssets += balance;
            else
                m_totalLiabilities += -balance;
        }
    }
    m_netWorth = m_totalAssets - m_totalLiabilities;

    {
        QSqlQuery q(Database::instance().db());
        q.prepare("SELECT COALESCE(SUM(amount),0) FROM budgets WHERE month=:month");
        q.bindValue(":month", m_month);
        m_monthBudget = (q.exec() && q.next()) ? q.value(0).toDouble() : 0.0;
    }
    {
        QSqlQuery q(Database::instance().db());
        q.prepare("SELECT COALESCE(SUM(t.amount),0) FROM transactions t"
                  " WHERE t.kind='expense' AND substr(t.occurred_at,1,7)=:month"
                  " AND t.category_id IN (SELECT category_id FROM budgets WHERE month=:month)");
        q.bindValue(":month", m_month);
        m_monthBudgetSpent = (q.exec() && q.next()) ? q.value(0).toDouble() : 0.0;
    }

    // Recent transactions
    m_recentTransactions.clear();
    {
        QSqlQuery q(Database::instance().db());
        q.prepare("SELECT t.id, t.kind, t.amount, c.name, c.icon, c.color,"
                  " a.name, t.note, t.occurred_at"
                  " FROM transactions t JOIN categories c ON c.id=t.category_id"
                  " JOIN accounts a ON a.id=t.account_id"
                  " WHERE t.kind IN ('expense','income')"
                  " ORDER BY t.occurred_at DESC, t.id DESC LIMIT 6");
        if (q.exec()) {
            while (q.next()) {
                const QString occurred = q.value(8).toString();
                const QString date = occurred.left(10);
                const QDate d = QDate::fromString(date, "yyyy-MM-dd");
                QVariantMap item;
                item["id"] = q.value(0).toInt();
                item["kind"] = q.value(1).toString();
                item["amount"] = q.value(2).toDouble();
                item["categoryName"] = q.value(3).toString();
                item["categoryIcon"] = q.value(4).toString();
                item["categoryColor"] = q.value(5).toString();
                item["accountName"] = q.value(6).toString();
                item["note"] = q.value(7).toString();
                item["date"] = date;
                item["time"] = occurred.mid(11, 5);
                item["dayLabel"] = relativeDay(date);
                item["dayWeek"] = weekdayShort(d);
                m_recentTransactions.append(item);
            }
        }
    }

    // Category breakdown (expense)
    m_categoryBreakdown.clear();
    {
        QSqlQuery q(Database::instance().db());
        q.prepare("SELECT c.id, c.name, c.icon, c.color, SUM(t.amount), COUNT(*)"
                  " FROM transactions t JOIN categories c ON c.id=t.category_id"
                  " WHERE t.kind='expense' AND substr(t.occurred_at,1,7)=:month"
                  " GROUP BY c.id ORDER BY SUM(t.amount) DESC");
        q.bindValue(":month", m_month);
        if (q.exec()) {
            const double total = current.expense;
            while (q.next()) {
                const double amount = q.value(4).toDouble();
                QVariantMap item;
                item["categoryId"] = q.value(0).toInt();
                item["name"] = q.value(1).toString();
                item["icon"] = q.value(2).toString();
                item["color"] = q.value(3).toString();
                item["amount"] = amount;
                item["count"] = q.value(5).toInt();
                item["percent"] = total > 0 ? 100.0 * q.value(4).toDouble() / total : 0.0;
                m_categoryBreakdown.append(item);
            }
        }
    }

    // Category breakdown (income)
    m_incomeBreakdown.clear();
    {
        QSqlQuery q(Database::instance().db());
        q.prepare("SELECT c.id, c.name, c.icon, c.color, SUM(t.amount), COUNT(*)"
                  " FROM transactions t JOIN categories c ON c.id=t.category_id"
                  " WHERE t.kind='income' AND substr(t.occurred_at,1,7)=:month"
                  " GROUP BY c.id ORDER BY SUM(t.amount) DESC");
        q.bindValue(":month", m_month);
        if (q.exec()) {
            const double total = current.income;
            while (q.next()) {
                const double amount = q.value(4).toDouble();
                QVariantMap item;
                item["categoryId"] = q.value(0).toInt();
                item["name"] = q.value(1).toString();
                item["icon"] = q.value(2).toString();
                item["color"] = q.value(3).toString();
                item["amount"] = amount;
                item["count"] = q.value(5).toInt();
                item["percent"] = total > 0 ? 100.0 * q.value(4).toDouble() / total : 0.0;
                m_incomeBreakdown.append(item);
            }
        }
    }

    // Daily expense series for the selected month
    m_dailySeries.clear();
    {
        const QDate startDate = monthStart();
        int days = startDate.daysInMonth();
        if (isCurrentMonth())
            days = qMin(QDate::currentDate().day(), days);
        QHash<int, double> byDay;
        QSqlQuery q(Database::instance().db());
        q.prepare("SELECT CAST(substr(occurred_at,9,2) AS INTEGER), SUM(amount)"
                  " FROM transactions WHERE kind='expense' AND substr(occurred_at,1,7)=:month"
                  " GROUP BY 1");
        q.bindValue(":month", m_month);
        if (q.exec()) {
            while (q.next())
                byDay.insert(q.value(0).toInt(), q.value(1).toDouble());
        }
        double running = 0.0;
        for (int day = 1; day <= days; ++day) {
            const double value = byDay.value(day, 0.0);
            running += value;
            QVariantMap item;
            item["day"] = day;
            item["label"] = QString::number(day);
            item["amount"] = value;
            item["cumulative"] = running;
            item["key"] = QStringLiteral("%1-%2").arg(m_month).arg(day, 2, 10, QChar('0'));
            m_dailySeries.append(item);
        }
    }

    // Current quarter to date, used by the quarter range of the trend chart.
    m_quarterSeries.clear();
    {
        const QDate today = QDate::currentDate();
        const int quarterStartMonth = ((today.month() - 1) / 3) * 3 + 1;
        const QDate startDate(today.year(), quarterStartMonth, 1);
        QHash<QString, double> byDate;
        QSqlQuery q(Database::instance().db());
        q.prepare("SELECT substr(occurred_at,1,10), SUM(amount)"
                  " FROM transactions WHERE kind='expense' AND substr(occurred_at,1,10)>=:from"
                  " GROUP BY 1");
        q.bindValue(":from", startDate.toString("yyyy-MM-dd"));
        if (q.exec()) {
            while (q.next())
                byDate.insert(q.value(0).toString(), q.value(1).toDouble());
        }
        for (QDate d = startDate; d <= today; d = d.addDays(1)) {
            const QString key = d.toString("yyyy-MM-dd");
            QVariantMap item;
            item["key"] = key;
            item["label"] = QStringLiteral("%1/%2").arg(d.month()).arg(d.day(), 2, 10, QChar('0'));
            item["amount"] = byDate.value(key, 0.0);
            m_quarterSeries.append(item);
        }
    }

    // Last 12 months trend
    m_monthlyTrend.clear();
    {
        const QDate startMonth = monthStart();
        QStringList months;
        for (int i = 11; i >= 0; --i)
            months.append(startMonth.addMonths(-i).toString("yyyy-MM"));
        QHash<QString, QPair<double, double>> values;
        QSqlQuery q(Database::instance().db());
        q.prepare("SELECT substr(occurred_at,1,7), kind, SUM(amount)"
                  " FROM transactions WHERE substr(occurred_at,1,7) >= :from"
                  " AND kind IN ('income','expense')"
                  " GROUP BY 1, 2");
        q.bindValue(":from", months.constFirst());
        if (q.exec()) {
            while (q.next()) {
                const QString key = q.value(0).toString();
                auto &pair = values[key];
                if (q.value(1).toString() == "income")
                    pair.first = q.value(2).toDouble();
                else
                    pair.second = q.value(2).toDouble();
            }
        }
        for (const QString &key : std::as_const(months)) {
            const QDate d = QDate::fromString(key + "-01", "yyyy-MM-dd");
            const auto pair = values.value(key);
            QVariantMap item;
            item["key"] = key;
            item["label"] = QStringLiteral("%1月").arg(d.month());
            item["shortLabel"] = QStringLiteral("%1/%2").arg(d.month()).arg(QString::number(d.year()).right(2));
            item["income"] = pair.first;
            item["expense"] = pair.second;
            m_monthlyTrend.append(item);
        }
    }

    // Weekday spending
    m_weekdayStats.clear();
    {
        QHash<int, double> byWeekday;
        QSqlQuery q(Database::instance().db());
        q.prepare("SELECT CAST(strftime('%w', occurred_at) AS INTEGER), SUM(amount)"
                  " FROM transactions WHERE kind='expense' AND substr(occurred_at,1,7)=:month"
                  " GROUP BY 1");
        q.bindValue(":month", m_month);
        if (q.exec()) {
            while (q.next())
                byWeekday.insert(q.value(0).toInt(), q.value(1).toDouble());
        }
        static const char *names[] = {"周一", "周二", "周三", "周四", "周五", "周六", "周日"};
        for (int i = 0; i < 7; ++i) {
            QVariantMap item;
            const int sqliteIndex = (i + 1) % 7; // Monday-first -> sqlite Sunday-first
            item["label"] = QString::fromUtf8(names[i]);
            item["index"] = i;
            item["amount"] = byWeekday.value(sqliteIndex, 0.0);
            m_weekdayStats.append(item);
        }
    }

    // Category options (for pickers)
    m_allCategoryOptions.clear();
    {
        QSqlQuery q(Database::instance().db());
        q.prepare("SELECT c.id, c.name, c.kind, c.icon, c.color,"
                  " COALESCE((SELECT amount FROM budgets b WHERE b.category_id=c.id AND b.month=:month),0),"
                  " COALESCE((SELECT SUM(t.amount) FROM transactions t"
                  "   WHERE t.category_id=c.id AND substr(t.occurred_at,1,7)=:month),0)"
                  " FROM categories c ORDER BY c.sort, c.id");
        q.bindValue(":month", m_month);
        if (q.exec()) {
            while (q.next()) {
                QVariantMap item;
                item["id"] = q.value(0).toInt();
                item["name"] = q.value(1).toString();
                item["kind"] = q.value(2).toString();
                item["icon"] = q.value(3).toString();
                item["color"] = q.value(4).toString();
                item["budget"] = q.value(5).toDouble();
                item["spent"] = q.value(6).toDouble();
                item["hasBudget"] = q.value(5).toDouble() > 0.0;
                m_allCategoryOptions.append(item);
            }
        }
    }

    m_budgetHighlights.clear();
    if (m_budgets) {
        QVector<QVariantMap> highlights;
        for (int i = 0; i < m_budgets->rowCount(); ++i)
            highlights.append(m_budgets->get(i));
        std::sort(highlights.begin(), highlights.end(), [](const QVariantMap &a, const QVariantMap &b) {
            const int pa = a.value("percent").toInt();
            const int pb = b.value("percent").toInt();
            if (pa != pb)
                return pa > pb;
            return a.value("spent").toDouble() > b.value("spent").toDouble();
        });
        const int count = qMin(4, highlights.size());
        for (int i = 0; i < count; ++i)
            m_budgetHighlights.append(highlights.at(i));
    }

    m_isDemoData = Database::instance().isDemoSeeded() && Database::instance().transactionCount() > 0;
    emit summaryChanged();
}

QVariantMap AppController::transactionById(int id) const
{
    QVariantMap map;
    QSqlQuery q(Database::instance().db());
    q.prepare("SELECT kind, amount, category_id, account_id, note, occurred_at,"
              " transfer_account_id FROM transactions WHERE id=:id");
    q.bindValue(":id", id);
    if (q.exec() && q.next()) {
        const QString occurred = q.value(5).toString();
        map["id"] = id;
        map["kind"] = q.value(0).toString();
        map["amount"] = q.value(1).toDouble();
        map["categoryId"] = q.value(2).toInt();
        map["accountId"] = q.value(3).toInt();
        map["note"] = q.value(4).toString();
        map["date"] = occurred.left(10);
        map["time"] = occurred.mid(11, 5);
        map["transferAccountId"] = q.value(6).toInt();
    }
    return map;
}

int AppController::saveTransaction(const QVariantMap &data)
{
    setLastError({});
    const QString kind = data.value("kind", "expense").toString();
    const double amount = data.value("amount").toDouble();
    const int accountId = data.value("accountId").toInt();
    const int transferAccountId = data.value("transferAccountId").toInt();
    QString note = data.value("note").toString().trimmed().left(120);
    if (note.isNull())
        note = QStringLiteral("");
    const int id = data.value("id").toInt();

    if (kind != "expense" && kind != "income" && kind != "transfer") {
        setLastError(QStringLiteral("请选择收支类型"));
        return -1;
    }
    if (amount <= 0.0 || amount > 99999999.0) {
        setLastError(QStringLiteral("请输入有效的金额"));
        return -1;
    }
    if (accountId <= 0) {
        setLastError(QStringLiteral("请选择一个账户"));
        return -1;
    }
    if (kind == "transfer" && (transferAccountId <= 0 || transferAccountId == accountId)) {
        setLastError(QStringLiteral("请选择不同的转出和转入账户"));
        return -1;
    }

    QSqlDatabase db = Database::instance().db();
    int categoryId = data.value("categoryId").toInt();
    if (kind == "transfer") {
        QSqlQuery q(db);
        q.prepare("SELECT id FROM categories WHERE kind='transfer' LIMIT 1");
        if (!q.exec() || !q.next()) {
            setLastError(QStringLiteral("账户转移分类不存在"));
            return -1;
        }
        categoryId = q.value(0).toInt();
    } else {
        if (categoryId <= 0) {
            setLastError(QStringLiteral("请选择分类"));
            return -1;
        }
        QSqlQuery q(db);
        q.prepare("SELECT kind FROM categories WHERE id=:id");
        q.bindValue(":id", categoryId);
        if (!q.exec() || !q.next() || q.value(0).toString() != kind) {
            setLastError(QStringLiteral("分类与收支类型不匹配"));
            return -1;
        }
    }

    auto accountExists = [&db](int accountIdValue) {
        QSqlQuery q(db);
        q.prepare("SELECT COUNT(*) FROM accounts WHERE id=:id AND archived=0");
        q.bindValue(":id", accountIdValue);
        return q.exec() && q.next() && q.value(0).toInt() > 0;
    };
    if (!accountExists(accountId)) {
        setLastError(QStringLiteral("账户不存在或已归档"));
        return -1;
    }
    if (kind == "transfer" && !accountExists(transferAccountId)) {
        setLastError(QStringLiteral("转入账户不存在或已归档"));
        return -1;
    }

    const QDateTime occurred = parseOccurred(data.value("date").toString(), data.value("time").toString());
    const QString occurredText = occurred.toString("yyyy-MM-dd HH:mm:ss");

    QSqlQuery q(db);
    if (id > 0) {
        q.prepare("UPDATE transactions SET kind=:kind, amount=:amount, account_id=:account,"
                  " transfer_account_id=:transfer, category_id=:category, note=:note,"
                  " occurred_at=:occurred WHERE id=:id");
        q.bindValue(":id", id);
    } else {
        q.prepare("INSERT INTO transactions(kind, amount, account_id, transfer_account_id,"
                  " category_id, note, occurred_at, created_at)"
                  " VALUES(:kind,:amount,:account,:transfer,:category,:note,:occurred,:created)");
        q.bindValue(":created", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    }
    q.bindValue(":kind", kind);
    q.bindValue(":amount", amount);
    q.bindValue(":account", accountId);
    q.bindValue(":transfer", kind == "transfer" ? transferAccountId : 0);
    q.bindValue(":category", categoryId);
    q.bindValue(":note", note);
    q.bindValue(":occurred", occurredText);

    if (!q.exec()) {
        setLastError(QStringLiteral("保存失败：%1").arg(q.lastError().text()));
        return -1;
    }
    const int resultId = id > 0 ? id : q.lastInsertId().toInt();

    const QString txMonth = occurredText.left(7);
    if (txMonth != m_month) {
        m_month = txMonth;
        emit currentMonthChanged();
    }
    refreshAll();
    return resultId;
}

bool AppController::deleteTransaction(int id)
{
    setLastError({});
    QSqlQuery q(Database::instance().db());
    q.prepare("DELETE FROM transactions WHERE id=:id");
    q.bindValue(":id", id);
    if (!q.exec()) {
        setLastError(QStringLiteral("删除失败：%1").arg(q.lastError().text()));
        return false;
    }
    refreshAll();
    return true;
}

QVariantList AppController::categoryOptions(const QString &kind) const
{
    QVariantList list;
    QSqlQuery q(Database::instance().db());
    q.prepare("SELECT id, name, icon, color FROM categories WHERE kind=:kind ORDER BY sort, id");
    q.bindValue(":kind", kind);
    if (q.exec()) {
        while (q.next()) {
            QVariantMap item;
            item["id"] = q.value(0).toInt();
            item["name"] = q.value(1).toString();
            item["icon"] = q.value(2).toString();
            item["color"] = q.value(3).toString();
            list.append(item);
        }
    }
    return list;
}

QVariantList AppController::accountOptions() const
{
    QVariantList list;
    QSqlQuery q(Database::instance().db());
    q.prepare("SELECT a.id, a.name, a.type, a.icon, a.color, a.initial, a.note,"
              " a.initial"
              " + COALESCE((SELECT SUM(CASE WHEN t.kind='income' THEN t.amount"
              "   WHEN t.kind='expense' THEN -t.amount END)"
              "   FROM transactions t WHERE t.account_id=a.id AND t.kind IN ('income','expense')),0)"
              " + COALESCE((SELECT SUM(CASE WHEN t.account_id=a.id THEN -t.amount"
              "   WHEN t.transfer_account_id=a.id THEN t.amount END)"
              "   FROM transactions t WHERE t.kind='transfer'"
              "   AND (t.account_id=a.id OR t.transfer_account_id=a.id)),0) AS balance"
              " FROM accounts a WHERE a.archived=0 ORDER BY a.sort, a.id");
    if (q.exec()) {
        while (q.next()) {
            QVariantMap item;
            item["id"] = q.value(0).toInt();
            item["name"] = q.value(1).toString();
            item["type"] = q.value(2).toString();
            item["icon"] = q.value(3).toString();
            item["color"] = q.value(4).toString();
            item["initial"] = q.value(5).toDouble();
            item["note"] = q.value(6).toString();
            item["balance"] = q.value(7).toDouble();
            list.append(item);
        }
    }
    return list;
}

QVariantList AppController::budgetCategoryOptions() const
{
    QVariantList list;
    for (const QVariant &v : m_allCategoryOptions) {
        const QVariantMap item = v.toMap();
        if (item.value("kind").toString() == "expense")
            list.append(item);
    }
    return list;
}

int AppController::saveAccount(const QVariantMap &data)
{
    setLastError({});
    const int id = data.value("id").toInt();
    const QString name = data.value("name").toString().trimmed().left(40);
    QString type = data.value("type", "cash").toString();
    if (name.isEmpty()) {
        setLastError(QStringLiteral("请输入账户名称"));
        return -1;
    }
    if (type != "cash" && type != "wallet" && type != "bank" && type != "credit")
        type = "cash";
    const QString icon = data.value("icon", "wallet").toString();
    const QString color = data.value("color", "#E8A94E").toString();
    const double initial = data.value("initial").toDouble();
    QString note = data.value("note").toString().trimmed().left(120);
    if (note.isNull())
        note = QStringLiteral("");

    QSqlQuery q(Database::instance().db());
    if (id > 0) {
        q.prepare("UPDATE accounts SET name=:name, type=:type, icon=:icon, color=:color,"
                  " initial=:initial, note=:note WHERE id=:id");
        q.bindValue(":id", id);
    } else {
        QSqlQuery sortQuery(Database::instance().db());
        int sort = 0;
        if (sortQuery.exec("SELECT COALESCE(MAX(sort),-1)+1 FROM accounts") && sortQuery.next())
            sort = sortQuery.value(0).toInt();
        q.prepare("INSERT INTO accounts(name, type, icon, color, initial, note, archived, sort, created_at)"
                  " VALUES(:name,:type,:icon,:color,:initial,:note,0,:sort,:created)");
        q.bindValue(":sort", sort);
        q.bindValue(":created", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    }
    q.bindValue(":name", name);
    q.bindValue(":type", type);
    q.bindValue(":icon", icon);
    q.bindValue(":color", color);
    q.bindValue(":initial", initial);
    q.bindValue(":note", note);

    if (!q.exec()) {
        setLastError(QStringLiteral("保存账户失败：%1").arg(q.lastError().text()));
        return -1;
    }
    const int resultId = id > 0 ? id : q.lastInsertId().toInt();
    refreshAll();
    return resultId;
}

bool AppController::accountInUse(int id) const
{
    QSqlQuery q(Database::instance().db());
    q.prepare("SELECT COUNT(*) FROM transactions WHERE account_id=:id OR transfer_account_id=:id");
    q.bindValue(":id", id);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

bool AppController::deleteAccount(int id)
{
    setLastError({});
    QSqlQuery countQuery(Database::instance().db());
    if (countQuery.exec("SELECT COUNT(*) FROM accounts WHERE archived=0") && countQuery.next()
        && countQuery.value(0).toInt() <= 1) {
        setLastError(QStringLiteral("至少需要保留一个账户"));
        return false;
    }
    QSqlQuery q(Database::instance().db());
    if (accountInUse(id)) {
        q.prepare("UPDATE accounts SET archived=1 WHERE id=:id");
    } else {
        q.prepare("DELETE FROM accounts WHERE id=:id");
    }
    q.bindValue(":id", id);
    if (!q.exec()) {
        setLastError(QStringLiteral("删除账户失败：%1").arg(q.lastError().text()));
        return false;
    }
    refreshAll();
    return true;
}

bool AppController::saveBudget(int categoryId, double amount)
{
    setLastError({});
    if (categoryId <= 0 || amount <= 0.0) {
        setLastError(QStringLiteral("请输入有效的预算金额"));
        return false;
    }
    QSqlQuery q(Database::instance().db());
    q.prepare("INSERT INTO budgets(category_id, month, amount) VALUES(:category,:month,:amount)"
              " ON CONFLICT(category_id, month) DO UPDATE SET amount=excluded.amount");
    q.bindValue(":category", categoryId);
    q.bindValue(":month", m_month);
    q.bindValue(":amount", amount);
    if (!q.exec()) {
        setLastError(QStringLiteral("保存预算失败：%1").arg(q.lastError().text()));
        return false;
    }
    refreshAll();
    return true;
}

bool AppController::deleteBudget(int categoryId)
{
    setLastError({});
    QSqlQuery q(Database::instance().db());
    q.prepare("DELETE FROM budgets WHERE category_id=:category AND month=:month");
    q.bindValue(":category", categoryId);
    q.bindValue(":month", m_month);
    if (!q.exec()) {
        setLastError(QStringLiteral("删除预算失败：%1").arg(q.lastError().text()));
        return false;
    }
    refreshAll();
    return true;
}

bool AppController::saveCustomBudget(const QString &name, double amount,
                                     const QString &icon, const QString &color)
{
    setLastError({});
    const QString trimmed = name.trimmed().left(40);
    if (trimmed.isEmpty()) {
        setLastError(QStringLiteral("给自定义预算对象取个名字吧"));
        return false;
    }
    if (amount <= 0.0 || amount > 99999999.0) {
        setLastError(QStringLiteral("请输入有效的预算金额"));
        return false;
    }

    QSqlDatabase db = Database::instance().db();
    int categoryId = 0;
    {
        QSqlQuery find(db);
        find.prepare("SELECT id FROM categories WHERE kind='expense' AND name=:name LIMIT 1");
        find.bindValue(":name", trimmed);
        if (find.exec() && find.next())
            categoryId = find.value(0).toInt();
    }

    if (categoryId <= 0) {
        QSqlQuery insert(db);
        insert.prepare("INSERT INTO categories(name, kind, icon, color, sort)"
                       " VALUES(:name, 'expense', :icon, :color,"
                       "        (SELECT COALESCE(MAX(sort), 0) + 1 FROM categories))");
        insert.bindValue(":name", trimmed);
        insert.bindValue(":icon", icon.isEmpty() ? QStringLiteral("more_horiz") : icon);
        insert.bindValue(":color", color.isEmpty() ? QStringLiteral("#918A81") : color);
        if (!insert.exec()) {
            setLastError(QStringLiteral("创建预算对象失败：%1").arg(insert.lastError().text()));
            return false;
        }
        categoryId = insert.lastInsertId().toInt();
    }

    QSqlQuery q(db);
    q.prepare("INSERT INTO budgets(category_id, month, amount) VALUES(:category,:month,:amount)"
              " ON CONFLICT(category_id, month) DO UPDATE SET amount=excluded.amount");
    q.bindValue(":category", categoryId);
    q.bindValue(":month", m_month);
    q.bindValue(":amount", amount);
    if (!q.exec()) {
        setLastError(QStringLiteral("保存预算失败：%1").arg(q.lastError().text()));
        return false;
    }
    refreshAll();
    return true;
}

QString AppController::suggestedExportName() const
{
    return QStringLiteral("DayBook-%1.csv").arg(m_month);
}

QString AppController::exportCsvTo(const QString &path)
{
    setLastError({});
    QString localPath = localPathFromInput(path);
    QString error;
    if (!Database::instance().exportCsv(localPath, &error)) {
        setLastError(QStringLiteral("导出失败：%1").arg(error));
        return {};
    }
    if (!localPath.endsWith(".csv", Qt::CaseInsensitive))
        localPath += ".csv";
    return localPath;
}

QString AppController::suggestedBackupName() const
{
    return QStringLiteral("daybook-backup-%1.db").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmm"));
}

QString AppController::backupTo(const QString &path)
{
    setLastError({});
    QString localPath = localPathFromInput(path);
    if (localPath.isEmpty()) {
        setLastError(QStringLiteral("请选择备份位置"));
        return {};
    }
    if (!localPath.endsWith(".db", Qt::CaseInsensitive))
        localPath += ".db";
    QString error;
    if (!Database::instance().backup(localPath, &error)) {
        setLastError(QStringLiteral("备份失败：%1").arg(error));
        return {};
    }
    return localPath;
}

void AppController::openDataFolder()
{
    const QString dir = Database::instance().dataDir();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void AppController::copyText(const QString &text) const
{
    QGuiApplication::clipboard()->setText(text);
}

bool AppController::clearAllData()
{
    setLastError({});
    QString error;
    if (!Database::instance().clearFinancialData(&error)) {
        setLastError(QStringLiteral("清空失败：%1").arg(error));
        return false;
    }
    refreshAll();
    return true;
}

bool AppController::loadDemoData()
{
    setLastError({});
    QString error;
    if (!Database::instance().seedDemoData(&error)) {
        setLastError(QStringLiteral("生成示例数据失败：%1").arg(error));
        return false;
    }
    const QString todayMonth = QDate::currentDate().toString("yyyy-MM");
    if (m_month != todayMonth) {
        m_month = todayMonth;
        emit currentMonthChanged();
    }
    refreshAll();
    return true;
}

QString AppController::todayText() const
{
    const QDate today = QDate::currentDate();
    return QStringLiteral("%1月%2日 %3").arg(today.month()).arg(today.day()).arg(weekdayShort(today));
}

QString AppController::relativeDay(const QString &date) const
{
    const QDate d = QDate::fromString(date, "yyyy-MM-dd");
    if (!d.isValid())
        return date;
    const QDate today = QDate::currentDate();
    if (d == today)
        return QStringLiteral("今天");
    if (d == today.addDays(-1))
        return QStringLiteral("昨天");
    if (d == today.addDays(-2))
        return QStringLiteral("前天");
    if (d.year() != today.year())
        return QStringLiteral("%1年%2月%3日").arg(d.year()).arg(d.month()).arg(d.day());
    return QStringLiteral("%1月%2日").arg(d.month()).arg(d.day());
}
