#include "models.h"
#include "database.h"

#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QHash>
#include <QLocale>
#include <QtMath>
#include <utility>

namespace {
QString typeLabel(const QString &type)
{
    if (type == "cash")
        return QStringLiteral("现金");
    if (type == "wallet")
        return QStringLiteral("电子钱包");
    if (type == "bank")
        return QStringLiteral("储蓄卡");
    if (type == "credit")
        return QStringLiteral("信用卡");
    return QStringLiteral("其他");
}

QString currentMonthKey()
{
    return QDate::currentDate().toString("yyyy-MM");
}
} // namespace

TransactionModel::TransactionModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_month = currentMonthKey();
    reload();
}

int TransactionModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QString TransactionModel::relativeDayLabel(const QDate &date)
{
    const QDate today = QDate::currentDate();
    if (date == today)
        return QStringLiteral("今天");
    if (date == today.addDays(-1))
        return QStringLiteral("昨天");
    if (date == today.addDays(-2))
        return QStringLiteral("前天");
    if (date.year() != today.year())
        return QStringLiteral("%1年%2月%3日").arg(date.year()).arg(date.month()).arg(date.day());
    return QStringLiteral("%1月%2日").arg(date.month()).arg(date.day());
}

QString TransactionModel::weekdayLabel(const QDate &date)
{
    static const char *names[] = {"周一", "周二", "周三", "周四", "周五", "周六", "周日"};
    return QString::fromUtf8(names[qBound(1, date.dayOfWeek(), 7) - 1]);
}

QVariant TransactionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());

    if (r.header) {
        switch (role) {
        case IsHeaderRole: return true;
        case DayKeyRole: return r.dayKey;
        case DayLabelRole: return r.dayLabel;
        case DayWeekRole: return r.dayWeek;
        case DayExpenseRole: return r.dayExpense;
        case DayIncomeRole: return r.dayIncome;
        case DayCountRole: return r.dayCount;
        case AmountRole: return 0.0;
        case IdRole: return 0;
        case KindRole: return QStringLiteral("expense");
        case CategoryIdRole: return 0;
        case CategoryNameRole: return QString();
        case CategoryIconRole: return QStringLiteral("category");
        case CategoryColorRole: return QStringLiteral("#918A81");
        case AccountIdRole: return 0;
        case AccountNameRole: return QString();
        case TransferAccountIdRole: return 0;
        case TransferAccountNameRole: return QString();
        case NoteRole: return QString();
        case DateRole: return r.date;
        case TimeRole: return QString();
        case OccurredRole: return QString();
        default: return {};
        }
    }

    switch (role) {
    case IsHeaderRole: return false;
    case IdRole: return r.id;
    case KindRole: return r.kind;
    case AmountRole: return r.amount;
    case CategoryIdRole: return r.categoryId;
    case CategoryNameRole: return r.categoryName;
    case CategoryIconRole: return r.categoryIcon;
    case CategoryColorRole: return r.categoryColor;
    case AccountIdRole: return r.accountId;
    case AccountNameRole: return r.accountName;
    case TransferAccountIdRole: return r.transferAccountId;
    case TransferAccountNameRole: return r.transferAccountName;
    case NoteRole: return r.note;
    case DateRole: return r.date;
    case TimeRole: return r.time;
    case DayKeyRole: return r.dayKey;
    case DayLabelRole: return r.dayLabel;
    case DayWeekRole: return r.dayWeek;
    case DayExpenseRole: return r.dayExpense;
    case DayIncomeRole: return r.dayIncome;
    case DayCountRole: return r.dayCount;
    case OccurredRole: return r.occurred;
    default: return {};
    }
}

QHash<int, QByteArray> TransactionModel::roleNames() const
{
    return {
        {IsHeaderRole, "isHeader"},
        {IdRole, "txId"},
        {KindRole, "kind"},
        {AmountRole, "amount"},
        {CategoryIdRole, "categoryId"},
        {CategoryNameRole, "categoryName"},
        {CategoryIconRole, "categoryIcon"},
        {CategoryColorRole, "categoryColor"},
        {AccountIdRole, "accountId"},
        {AccountNameRole, "accountName"},
        {TransferAccountIdRole, "transferAccountId"},
        {TransferAccountNameRole, "transferAccountName"},
        {NoteRole, "note"},
        {DateRole, "date"},
        {TimeRole, "time"},
        {DayKeyRole, "dayKey"},
        {DayLabelRole, "dayLabel"},
        {DayWeekRole, "dayWeek"},
        {DayExpenseRole, "dayExpense"},
        {DayIncomeRole, "dayIncome"},
        {DayCountRole, "dayCount"},
        {OccurredRole, "occurred"},
    };
}

QVariantMap TransactionModel::get(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= m_rows.size())
        return map;
    const Row &r = m_rows.at(row);
    map["isHeader"] = r.header;
    map["id"] = r.id;
    map["kind"] = r.kind;
    map["amount"] = r.amount;
    map["categoryId"] = r.categoryId;
    map["categoryName"] = r.categoryName;
    map["categoryIcon"] = r.categoryIcon;
    map["categoryColor"] = r.categoryColor;
    map["accountId"] = r.accountId;
    map["accountName"] = r.accountName;
    map["transferAccountId"] = r.transferAccountId;
    map["transferAccountName"] = r.transferAccountName;
    map["note"] = r.note;
    map["date"] = r.date;
    map["time"] = r.time;
    map["occurred"] = r.occurred;
    return map;
}

bool TransactionModel::hasActiveFilters() const
{
    return m_kindFilter != "all" || m_categoryFilter != 0 || m_accountFilter != 0 || !m_search.isEmpty();
}

void TransactionModel::setMonth(const QString &month)
{
    if (m_month == month)
        return;
    m_month = month;
    emit monthChanged();
    reload();
}

void TransactionModel::setKindFilter(const QString &kind)
{
    const QString value = (kind == "income" || kind == "expense") ? kind : "all";
    if (m_kindFilter == value)
        return;
    m_kindFilter = value;

    // A category chosen for one side of the ledger no longer makes sense when
    // the user switches to the other side. Clear it instead of showing an
    // inexplicably empty list.
    if (m_categoryFilter > 0 && value != "all") {
        QSqlQuery q(Database::instance().db());
        q.prepare("SELECT kind FROM categories WHERE id=:id");
        q.bindValue(":id", m_categoryFilter);
        if (!q.exec() || !q.next() || q.value(0).toString() != value)
            m_categoryFilter = 0;
    }

    emit filtersChanged();
    reload();
}

void TransactionModel::setCategoryFilter(int id)
{
    if (m_categoryFilter == id)
        return;
    m_categoryFilter = id;
    emit filtersChanged();
    reload();
}

void TransactionModel::setAccountFilter(int id)
{
    if (m_accountFilter == id)
        return;
    m_accountFilter = id;
    emit filtersChanged();
    reload();
}

void TransactionModel::setSearch(const QString &text)
{
    if (m_search == text)
        return;
    m_search = text;
    emit filtersChanged();
    reload();
}

void TransactionModel::clearFilters()
{
    if (!hasActiveFilters())
        return;
    m_kindFilter = "all";
    m_categoryFilter = 0;
    m_accountFilter = 0;
    m_search.clear();
    emit filtersChanged();
    reload();
}

void TransactionModel::reload()
{
    QSqlDatabase db = Database::instance().db();
    QVector<Row> rows;
    double expenseAmount = 0.0;
    double incomeAmount = 0.0;

    QString sql = "SELECT t.id, t.kind, t.amount, t.account_id, t.category_id, t.note,"
                  " t.occurred_at, c.name, c.icon, c.color, a.name, t.transfer_account_id, ta.name"
                  " FROM transactions t"
                  " JOIN categories c ON c.id = t.category_id"
                  " JOIN accounts a ON a.id = t.account_id"
                  " LEFT JOIN accounts ta ON ta.id = t.transfer_account_id"
                  " WHERE 1=1";
    if (!m_month.isEmpty())
        sql += " AND substr(t.occurred_at, 1, 7) = :month";
    if (m_kindFilter == "income" || m_kindFilter == "expense")
        sql += " AND t.kind = :kind";
    if (m_categoryFilter > 0)
        sql += " AND t.category_id = :category";
    if (m_accountFilter > 0)
        sql += " AND (t.account_id = :account OR t.transfer_account_id = :account)";
    if (!m_search.trimmed().isEmpty())
        sql += " AND (t.note LIKE :search OR c.name LIKE :search OR a.name LIKE :search"
               " OR ta.name LIKE :search OR printf('%.2f', t.amount) LIKE :search)";
    sql += " ORDER BY t.occurred_at DESC, t.id DESC";

    QSqlQuery q(db);
    q.prepare(sql);
    if (!m_month.isEmpty())
        q.bindValue(":month", m_month);
    if (m_kindFilter == "income" || m_kindFilter == "expense")
        q.bindValue(":kind", m_kindFilter);
    if (m_categoryFilter > 0)
        q.bindValue(":category", m_categoryFilter);
    if (m_accountFilter > 0)
        q.bindValue(":account", m_accountFilter);
    if (!m_search.trimmed().isEmpty())
        q.bindValue(":search", "%" + m_search.trimmed() + "%");

    if (!q.exec()) {
        qWarning() << "Transaction query failed:" << q.lastError().text();
    } else {
        QString currentDay;
        int headerRow = -1;
        double dayExpense = 0.0;
        double dayIncome = 0.0;
        int dayCount = 0;

        while (q.next()) {
            Row r;
            r.id = q.value(0).toInt();
            r.kind = q.value(1).toString();
            r.amount = q.value(2).toDouble();
            r.accountId = q.value(3).toInt();
            r.categoryId = q.value(4).toInt();
            r.note = q.value(5).toString();
            r.occurred = q.value(6).toString();
            r.date = r.occurred.left(10);
            r.time = r.occurred.mid(11, 5);
            r.categoryName = q.value(7).toString();
            r.categoryIcon = q.value(8).toString();
            r.categoryColor = q.value(9).toString();
            r.accountName = q.value(10).toString();
            r.transferAccountId = q.value(11).toInt();
            r.transferAccountName = q.value(12).toString();
            r.dayKey = r.date;

            const QDate date = QDate::fromString(r.date, "yyyy-MM-dd");
            r.dayLabel = relativeDayLabel(date);
            r.dayWeek = weekdayLabel(date);

            if (r.kind == "income")
                incomeAmount += r.amount;
            else if (r.kind == "expense")
                expenseAmount += r.amount;

            if (r.date != currentDay) {
                if (headerRow >= 0) {
                    rows[headerRow].dayExpense = dayExpense;
                    rows[headerRow].dayIncome = dayIncome;
                    rows[headerRow].dayCount = dayCount;
                }
                dayExpense = 0;
                dayIncome = 0;
                dayCount = 0;
                currentDay = r.date;

                Row header;
                header.header = true;
                header.date = r.date;
                header.dayKey = r.dayKey;
                header.dayLabel = r.dayLabel;
                header.dayWeek = r.dayWeek;
                rows.append(header);
                headerRow = rows.size() - 1;
            }

            if (r.kind == "income")
                dayIncome += r.amount;
            else if (r.kind == "expense")
                dayExpense += r.amount;
            ++dayCount;
            rows.append(r);
        }
        if (headerRow >= 0) {
            rows[headerRow].dayExpense = dayExpense;
            rows[headerRow].dayIncome = dayIncome;
            rows[headerRow].dayCount = dayCount;
        }
    }

    beginResetModel();
    m_rows = rows;
    m_transactionCount = 0;
    for (const Row &r : std::as_const(m_rows)) {
        if (!r.header)
            ++m_transactionCount;
    }
    m_totalExpense = expenseAmount;
    m_totalIncome = incomeAmount;
    endResetModel();

    emit countChanged();
    emit summaryChanged();
}

// ---------------------------------------------------------------------------

AccountModel::AccountModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_month = currentMonthKey();
    reload();
}

int AccountModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant AccountModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case IdRole: return r.id;
    case NameRole: return r.name;
    case TypeRole: return r.type;
    case TypeLabelRole: return r.typeLabel;
    case IconRole: return r.icon;
    case ColorRole: return r.color;
    case InitialRole: return r.initial;
    case BalanceRole: return r.balance;
    case NoteRole: return r.note;
    case MonthExpenseRole: return r.monthExpense;
    default: return {};
    }
}

QHash<int, QByteArray> AccountModel::roleNames() const
{
    return {
        {IdRole, "accountId"},
        {NameRole, "name"},
        {TypeRole, "accountType"},
        {TypeLabelRole, "typeLabel"},
        {IconRole, "icon"},
        {ColorRole, "color"},
        {InitialRole, "initial"},
        {BalanceRole, "balance"},
        {NoteRole, "note"},
        {MonthExpenseRole, "monthExpense"},
    };
}

QVariantMap AccountModel::get(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= m_rows.size())
        return map;
    const Row &r = m_rows.at(row);
    map["id"] = r.id;
    map["name"] = r.name;
    map["type"] = r.type;
    map["typeLabel"] = r.typeLabel;
    map["icon"] = r.icon;
    map["color"] = r.color;
    map["initial"] = r.initial;
    map["balance"] = r.balance;
    map["note"] = r.note;
    map["monthExpense"] = r.monthExpense;
    return map;
}

void AccountModel::setMonth(const QString &month)
{
    if (m_month == month)
        return;
    m_month = month;
    emit monthChanged();
    reload();
}

void AccountModel::reload()
{
    QVector<Row> rows;
    double total = 0.0;
    double assets = 0.0;
    double liabilities = 0.0;

    QSqlQuery q(Database::instance().db());
    q.prepare("SELECT a.id, a.name, a.type, a.icon, a.color, a.initial, a.note,"
              " a.initial"
              " + COALESCE((SELECT SUM(CASE WHEN t.kind='income' THEN t.amount"
              "   WHEN t.kind='expense' THEN -t.amount END)"
              "   FROM transactions t WHERE t.account_id=a.id AND t.kind IN ('income','expense')), 0)"
              " + COALESCE((SELECT SUM(CASE WHEN t.account_id=a.id THEN -t.amount"
              "   WHEN t.transfer_account_id=a.id THEN t.amount END)"
              "   FROM transactions t WHERE t.kind='transfer'"
              "   AND (t.account_id=a.id OR t.transfer_account_id=a.id)), 0) AS balance,"
              " COALESCE(("
              "   SELECT SUM(t.amount) FROM transactions t"
              "   WHERE t.account_id=a.id AND t.kind='expense' AND substr(t.occurred_at,1,7)=:month), 0) AS month_expense"
              " FROM accounts a WHERE a.archived=0 ORDER BY a.sort, a.id");
    q.bindValue(":month", m_month);
    if (!q.exec()) {
        qWarning() << "Account query failed:" << q.lastError().text();
    } else {
        while (q.next()) {
            Row r;
            r.id = q.value(0).toInt();
            r.name = q.value(1).toString();
            r.type = q.value(2).toString();
            r.typeLabel = typeLabel(r.type);
            r.icon = q.value(3).toString();
            r.color = q.value(4).toString();
            r.initial = q.value(5).toDouble();
            r.note = q.value(6).toString();
            r.balance = q.value(7).toDouble();
            r.monthExpense = q.value(8).toDouble();
            rows.append(r);

            const double balance = r.balance;
            total += balance;
            if (balance >= 0)
                assets += balance;
            else
                liabilities += -balance;
        }
    }

    beginResetModel();
    m_rows = rows;
    m_totalBalance = total;
    m_totalAssets = assets;
    m_totalLiabilities = liabilities;
    endResetModel();
    emit countChanged();
    emit summaryChanged();
}

void AccountModel::moveAccountRow(int from, int to)
{
    if (from < 0 || to < 0 || from >= m_rows.size() || to >= m_rows.size() || from == to)
        return;
    beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to);
    m_rows.move(from, to);
    endMoveRows();
}

// ---------------------------------------------------------------------------

CategoryModel::CategoryModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_month = currentMonthKey();
    reload();
}

int CategoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant CategoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case IdRole: return r.id;
    case NameRole: return r.name;
    case KindRole: return r.kind;
    case IconRole: return r.icon;
    case ColorRole: return r.color;
    case SortRole: return r.sort;
    case SpentRole: return r.spent;
    case BudgetRole: return r.budget;
    case TxCountRole: return r.txCount;
    default: return {};
    }
}

QHash<int, QByteArray> CategoryModel::roleNames() const
{
    return {
        {IdRole, "categoryId"},
        {NameRole, "name"},
        {KindRole, "kind"},
        {IconRole, "icon"},
        {ColorRole, "color"},
        {SortRole, "sort"},
        {SpentRole, "spent"},
        {BudgetRole, "budget"},
        {TxCountRole, "txCount"},
    };
}

QVariantMap CategoryModel::get(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= m_rows.size())
        return map;
    const Row &r = m_rows.at(row);
    map["id"] = r.id;
    map["name"] = r.name;
    map["kind"] = r.kind;
    map["icon"] = r.icon;
    map["color"] = r.color;
    map["spent"] = r.spent;
    map["budget"] = r.budget;
    map["txCount"] = r.txCount;
    return map;
}

void CategoryModel::setMonth(const QString &month)
{
    if (m_month == month)
        return;
    m_month = month;
    emit monthChanged();
    reload();
}

void CategoryModel::setKindFilter(const QString &kind)
{
    const QString value = (kind == "expense" || kind == "income") ? kind : QString();
    if (m_kindFilter == value)
        return;
    m_kindFilter = value;
    emit kindChanged();
    reload();
}

void CategoryModel::reload()
{
    QVector<Row> rows;
    QString sql = "SELECT c.id, c.name, c.kind, c.icon, c.color, c.sort,"
                  " COALESCE((SELECT SUM(t.amount) FROM transactions t"
                  "   WHERE t.category_id=c.id AND substr(t.occurred_at,1,7)=:month), 0),"
                  " COALESCE((SELECT b.amount FROM budgets b"
                  "   WHERE b.category_id=c.id AND b.month=:month), 0),"
                  " (SELECT COUNT(*) FROM transactions t WHERE t.category_id=c.id)"
                  " FROM categories c";
    if (!m_kindFilter.isEmpty())
        sql += " WHERE c.kind=:kind";
    sql += " ORDER BY c.sort, c.id";

    QSqlQuery q(Database::instance().db());
    q.prepare(sql);
    q.bindValue(":month", m_month);
    if (!m_kindFilter.isEmpty())
        q.bindValue(":kind", m_kindFilter);
    if (!q.exec()) {
        qWarning() << "Category query failed:" << q.lastError().text();
    } else {
        while (q.next()) {
            Row r;
            r.id = q.value(0).toInt();
            r.name = q.value(1).toString();
            r.kind = q.value(2).toString();
            r.icon = q.value(3).toString();
            r.color = q.value(4).toString();
            r.sort = q.value(5).toInt();
            r.spent = q.value(6).toDouble();
            r.budget = q.value(7).toDouble();
            r.txCount = q.value(8).toInt();
            rows.append(r);
        }
    }
    beginResetModel();
    m_rows = rows;
    endResetModel();
    emit countChanged();
}

// ---------------------------------------------------------------------------

BudgetModel::BudgetModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_month = currentMonthKey();
    reload();
}

int BudgetModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant BudgetModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case CategoryIdRole: return r.categoryId;
    case NameRole: return r.name;
    case IconRole: return r.icon;
    case ColorRole: return r.color;
    case BudgetRole: return r.budget;
    case SpentRole: return r.spent;
    case RemainingRole: return (r.budget - r.spent);
    case PercentRole: return r.budget > 0 ? int(qRound(100.0 * r.spent / r.budget)) : 0;
    case OverRole: return r.spent > r.budget;
    case NoteRole: return r.note;
    default: return {};
    }
}

QHash<int, QByteArray> BudgetModel::roleNames() const
{
    return {
        {CategoryIdRole, "categoryId"},
        {NameRole, "name"},
        {IconRole, "icon"},
        {ColorRole, "color"},
        {BudgetRole, "budget"},
        {SpentRole, "spent"},
        {RemainingRole, "remaining"},
        {PercentRole, "percent"},
        {OverRole, "over"},
        {NoteRole, "note"},
    };
}

QVariantMap BudgetModel::get(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= m_rows.size())
        return map;
    const Row &r = m_rows.at(row);
    map["categoryId"] = r.categoryId;
    map["name"] = r.name;
    map["icon"] = r.icon;
    map["color"] = r.color;
    map["budget"] = r.budget;
    map["spent"] = r.spent;
    map["remaining"] = (r.budget - r.spent);
    map["percent"] = r.budget > 0 ? int(qRound(100.0 * r.spent / r.budget)) : 0;
    map["over"] = r.spent > r.budget;
    return map;
}

int BudgetModel::totalPercent() const
{
    return m_totalBudget > 0.0 ? int(qRound(m_totalSpent / m_totalBudget * 100.0)) : 0;
}

void BudgetModel::setMonth(const QString &month)
{
    if (m_month == month)
        return;
    m_month = month;
    emit monthChanged();
    reload();
}

void BudgetModel::reload()
{
    QVector<Row> rows;
    double totalBudget = 0.0;
    double totalSpent = 0.0;

    QSqlQuery q(Database::instance().db());
    q.prepare("SELECT b.category_id, c.name, c.icon, c.color, b.amount,"
              " COALESCE((SELECT SUM(t.amount) FROM transactions t"
              "   WHERE t.category_id=b.category_id AND t.kind='expense'"
              "   AND substr(t.occurred_at,1,7)=b.month), 0) AS spent"
              " FROM budgets b JOIN categories c ON c.id=b.category_id"
              " WHERE b.month=:month ORDER BY c.sort, c.id");
    q.bindValue(":month", m_month);
    if (!q.exec()) {
        qWarning() << "Budget query failed:" << q.lastError().text();
    } else {
        while (q.next()) {
            Row r;
            r.categoryId = q.value(0).toInt();
            r.name = q.value(1).toString();
            r.icon = q.value(2).toString();
            r.color = q.value(3).toString();
            r.budget = q.value(4).toDouble();
            r.spent = q.value(5).toDouble();
            rows.append(r);
            totalBudget += r.budget;
            totalSpent += r.spent;
        }
    }
    beginResetModel();
    m_rows = rows;
    m_totalBudget = totalBudget;
    m_totalSpent = totalSpent;
    endResetModel();
    emit countChanged();
    emit summaryChanged();
}
