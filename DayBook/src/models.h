#pragma once

#include <QAbstractListModel>
#include <QDate>
#include <QString>
#include <QVector>

class TransactionModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ transactionCount NOTIFY countChanged)
    Q_PROPERTY(int transactionCount READ transactionCount NOTIFY countChanged)
    Q_PROPERTY(double totalExpense READ totalExpense NOTIFY summaryChanged)
    Q_PROPERTY(double totalIncome READ totalIncome NOTIFY summaryChanged)
    Q_PROPERTY(QString month READ month WRITE setMonth NOTIFY monthChanged)
    Q_PROPERTY(QString kindFilter READ kindFilter WRITE setKindFilter NOTIFY filtersChanged)
    Q_PROPERTY(int categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY filtersChanged)
    Q_PROPERTY(int accountFilter READ accountFilter WRITE setAccountFilter NOTIFY filtersChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY filtersChanged)
    Q_PROPERTY(bool hasActiveFilters READ hasActiveFilters NOTIFY filtersChanged)

public:
    explicit TransactionModel(QObject *parent = nullptr);

    enum Roles {
        IsHeaderRole = Qt::UserRole + 1,
        IdRole,
        KindRole,
        AmountRole,
        CategoryIdRole,
        CategoryNameRole,
        CategoryIconRole,
        CategoryColorRole,
        AccountIdRole,
        AccountNameRole,
        TransferAccountIdRole,
        TransferAccountNameRole,
        NoteRole,
        DateRole,
        TimeRole,
        DayKeyRole,
        DayLabelRole,
        DayWeekRole,
        DayExpenseRole,
        DayIncomeRole,
        DayCountRole,
        OccurredRole,
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap get(int row) const;

    int transactionCount() const { return m_transactionCount; }
    double totalExpense() const { return m_totalExpense; }
    double totalIncome() const { return m_totalIncome; }
    QString month() const { return m_month; }
    QString kindFilter() const { return m_kindFilter; }
    int categoryFilter() const { return m_categoryFilter; }
    int accountFilter() const { return m_accountFilter; }
    QString search() const { return m_search; }
    bool hasActiveFilters() const;

    Q_INVOKABLE void setMonth(const QString &month);
    Q_INVOKABLE void setKindFilter(const QString &kind);
    Q_INVOKABLE void setCategoryFilter(int id);
    Q_INVOKABLE void setAccountFilter(int id);
    Q_INVOKABLE void setSearch(const QString &text);
    Q_INVOKABLE void clearFilters();

public slots:
    void reload();

signals:
    void countChanged();
    void summaryChanged();
    void monthChanged();
    void filtersChanged();

private:
    struct Row {
        bool header = false;
        int id = 0;
        QString kind;
        double amount = 0.0;
        int categoryId = 0;
        int accountId = 0;
        int transferAccountId = 0;
        QString categoryName;
        QString categoryIcon;
        QString categoryColor;
        QString accountName;
        QString transferAccountName;
        QString note;
        QString date;
        QString time;
        QString dayKey;
        QString dayLabel;
        QString dayWeek;
        double dayExpense = 0.0;
        double dayIncome = 0.0;
        int dayCount = 0;
        QString occurred;
    };

    static QString relativeDayLabel(const QDate &date);
    static QString weekdayLabel(const QDate &date);

    QVector<Row> m_rows;
    int m_transactionCount = 0;
    double m_totalExpense = 0.0;
    double m_totalIncome = 0.0;
    QString m_month;
    QString m_kindFilter = "all";
    int m_categoryFilter = 0;
    int m_accountFilter = 0;
    QString m_search;
};

class AccountModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(double totalBalance READ totalBalance NOTIFY summaryChanged)
    Q_PROPERTY(double totalAssets READ totalAssets NOTIFY summaryChanged)
    Q_PROPERTY(double totalLiabilities READ totalLiabilities NOTIFY summaryChanged)
    Q_PROPERTY(QString month READ month WRITE setMonth NOTIFY monthChanged)

public:
    explicit AccountModel(QObject *parent = nullptr);

    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        TypeLabelRole,
        IconRole,
        ColorRole,
        InitialRole,
        BalanceRole,
        NoteRole,
        MonthExpenseRole,
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantMap get(int row) const;

    double totalBalance() const { return m_totalBalance; }
    double totalAssets() const { return m_totalAssets; }
    double totalLiabilities() const { return m_totalLiabilities; }
    QString month() const { return m_month; }
    void setMonth(const QString &month);

public slots:
    void reload();
signals:
    void countChanged();
    void summaryChanged();
    void monthChanged();

private:
    struct Row {
        int id = 0;
        QString name;
        QString type;
        QString typeLabel;
        QString icon;
        QString color;
        double initial = 0.0;
        double balance = 0.0;
        double monthExpense = 0.0;
        QString note;
    };
    QVector<Row> m_rows;
    double m_totalBalance = 0.0;
    double m_totalAssets = 0.0;
    double m_totalLiabilities = 0.0;
    QString m_month;
};

class CategoryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString month READ month WRITE setMonth NOTIFY monthChanged)
    Q_PROPERTY(QString kindFilter READ kindFilter WRITE setKindFilter NOTIFY kindChanged)

public:
    explicit CategoryModel(QObject *parent = nullptr);

    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        KindRole,
        IconRole,
        ColorRole,
        SortRole,
        SpentRole,
        BudgetRole,
        TxCountRole,
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantMap get(int row) const;

    QString month() const { return m_month; }
    void setMonth(const QString &month);
    QString kindFilter() const { return m_kindFilter; }
    void setKindFilter(const QString &kind);

public slots:
    void reload();
signals:
    void countChanged();
    void monthChanged();
    void kindChanged();

private:
    struct Row {
        int id = 0;
        QString name;
        QString kind;
        QString icon;
        QString color;
        int sort = 0;
        double spent = 0.0;
        double budget = 0.0;
        int txCount = 0;
    };
    QVector<Row> m_rows;
    QString m_month;
    QString m_kindFilter;
};

class BudgetModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(double totalBudget READ totalBudget NOTIFY summaryChanged)
    Q_PROPERTY(double totalSpent READ totalSpent NOTIFY summaryChanged)
    Q_PROPERTY(double totalRemaining READ totalRemaining NOTIFY summaryChanged)
    Q_PROPERTY(int totalPercent READ totalPercent NOTIFY summaryChanged)
    Q_PROPERTY(QString month READ month WRITE setMonth NOTIFY monthChanged)

public:
    explicit BudgetModel(QObject *parent = nullptr);

    enum Roles {
        CategoryIdRole = Qt::UserRole + 1,
        NameRole,
        IconRole,
        ColorRole,
        BudgetRole,
        SpentRole,
        RemainingRole,
        PercentRole,
        OverRole,
        NoteRole,
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantMap get(int row) const;

    double totalBudget() const { return m_totalBudget; }
    double totalSpent() const { return m_totalSpent; }
    double totalRemaining() const { return m_totalBudget - m_totalSpent; }
    int totalPercent() const;
    QString month() const { return m_month; }
    void setMonth(const QString &month);

public slots:
    void reload();
signals:
    void countChanged();
    void summaryChanged();
    void monthChanged();

private:
    struct Row {
        int categoryId = 0;
        QString name;
        QString icon;
        QString color;
        double budget = 0.0;
        double spent = 0.0;
        QString note;
    };
    QVector<Row> m_rows;
    double m_totalBudget = 0.0;
    double m_totalSpent = 0.0;
    QString m_month;
};
