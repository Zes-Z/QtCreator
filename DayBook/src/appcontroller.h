#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QSettings>
#include <QDate>

#include "models.h"

class AppController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(TransactionModel *transactions READ transactions CONSTANT)
    Q_PROPERTY(TransactionModel *accountBills READ accountBills CONSTANT)
    Q_PROPERTY(AccountModel *accounts READ accounts CONSTANT)
    Q_PROPERTY(CategoryModel *categories READ categories CONSTANT)
    Q_PROPERTY(BudgetModel *budgets READ budgets CONSTANT)

    Q_PROPERTY(QString currentMonth READ currentMonth WRITE setCurrentMonth NOTIFY currentMonthChanged)
    Q_PROPERTY(QString monthLabel READ monthLabel NOTIFY currentMonthChanged)
    Q_PROPERTY(QString monthShortLabel READ monthShortLabel NOTIFY currentMonthChanged)
    Q_PROPERTY(bool isCurrentMonth READ isCurrentMonth NOTIFY currentMonthChanged)

    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString accentName READ accentName WRITE setAccentName NOTIFY accentNameChanged)
    Q_PROPERTY(QString customAccent READ customAccent NOTIFY customAccentChanged)
    Q_PROPERTY(QColor customAccentColor READ customAccentColor NOTIFY customAccentChanged)
    Q_PROPERTY(QString currencyCode READ currencyCode WRITE setCurrencyCode NOTIFY currencyChanged)
    Q_PROPERTY(QString currencySymbol READ currencySymbol NOTIFY currencyChanged)
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY systemDarkChanged)
    Q_PROPERTY(bool isDark READ isDark NOTIFY appearanceChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY fontChanged)
    Q_PROPERTY(QVariantList fontOptions READ fontOptions CONSTANT)
    Q_PROPERTY(QString fontChoice READ fontChoice NOTIFY fontChanged)
    Q_PROPERTY(QString numberFamily READ numberFamily CONSTANT)
    Q_PROPERTY(QString iconFontFamily READ iconFontFamily CONSTANT)

    Q_PROPERTY(double monthIncome READ monthIncome NOTIFY summaryChanged)
    Q_PROPERTY(double monthExpense READ monthExpense NOTIFY summaryChanged)
    Q_PROPERTY(double monthBalance READ monthBalance NOTIFY summaryChanged)
    Q_PROPERTY(double lastMonthExpense READ lastMonthExpense NOTIFY summaryChanged)
    Q_PROPERTY(double lastMonthIncome READ lastMonthIncome NOTIFY summaryChanged)
    Q_PROPERTY(double expenseChange READ expenseChange NOTIFY summaryChanged)
    Q_PROPERTY(double incomeChange READ incomeChange NOTIFY summaryChanged)
    Q_PROPERTY(double dailyAverage READ dailyAverage NOTIFY summaryChanged)
    Q_PROPERTY(int monthTransactionCount READ monthTransactionCount NOTIFY summaryChanged)
    Q_PROPERTY(double netWorth READ netWorth NOTIFY summaryChanged)
    Q_PROPERTY(double totalAssets READ totalAssets NOTIFY summaryChanged)
    Q_PROPERTY(double totalLiabilities READ totalLiabilities NOTIFY summaryChanged)
    Q_PROPERTY(double monthBudget READ monthBudget NOTIFY summaryChanged)
    Q_PROPERTY(double monthBudgetSpent READ monthBudgetSpent NOTIFY summaryChanged)
    Q_PROPERTY(int monthBudgetPercent READ monthBudgetPercent NOTIFY summaryChanged)

    Q_PROPERTY(QVariantList recentTransactions READ recentTransactions NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList categoryBreakdown READ categoryBreakdown NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList incomeBreakdown READ incomeBreakdown NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList dailySeries READ dailySeries NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList quarterSeries READ quarterSeries NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList monthlyTrend READ monthlyTrend NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList weekdayStats READ weekdayStats NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList allCategoryOptions READ allCategoryOptions NOTIFY summaryChanged)

    Q_PROPERTY(bool isDemoData READ isDemoData NOTIFY summaryChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString dataPath READ dataPath CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(bool isMac READ isMac CONSTANT)
    Q_PROPERTY(QStringList navOrder READ navOrder NOTIFY uiOrderChanged)
    Q_PROPERTY(QStringList summaryOrder READ summaryOrder NOTIFY uiOrderChanged)
    Q_PROPERTY(QString startupPage READ startupPage CONSTANT)
    Q_PROPERTY(QString startupModal READ startupModal CONSTANT)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    bool initialize(QString *error = nullptr);

    TransactionModel *transactions() const { return m_transactions; }
    TransactionModel *accountBills() const { return m_accountBills; }
    AccountModel *accounts() const { return m_accounts; }
    CategoryModel *categories() const { return m_categories; }
    BudgetModel *budgets() const { return m_budgets; }

    QString currentMonth() const { return m_month; }
    void setCurrentMonth(const QString &month);
    QString monthLabel() const;
    QString monthShortLabel() const;
    bool isCurrentMonth() const;

    QString themeMode() const { return m_themeMode; }
    Q_INVOKABLE void setThemeMode(const QString &mode);
    QString accentName() const { return m_accentName; }
    Q_INVOKABLE void setAccentName(const QString &name);
    QString customAccent() const { return m_customAccent; }
    QColor customAccentColor() const { return QColor(m_customAccent); }
    Q_INVOKABLE void setCustomAccent(const QColor &color);
    QString currencyCode() const { return m_currencyCode; }
    QString currencySymbol() const;
    Q_INVOKABLE void setCurrencyCode(const QString &code);
    QStringList navOrder() const { return m_navOrder; }
    Q_INVOKABLE void setNavOrder(const QStringList &order);
    Q_INVOKABLE void moveNavItem(int from, int to);
    QStringList summaryOrder() const { return m_summaryOrder; }
    Q_INVOKABLE void setSummaryOrder(const QStringList &order);
    Q_INVOKABLE void moveSummaryItem(int from, int to);
    bool systemDark() const;
    bool isDark() const;
    QString fontFamily() const { return m_fontFamily; }
    QVariantList fontOptions() const { return m_fontOptions; }
    QString fontChoice() const { return m_fontChoice; }
    Q_INVOKABLE void setFontChoice(const QString &key);
    Q_INVOKABLE QString colorHex(const QColor &color) const { return color.name(QColor::HexRgb); }
    QString numberFamily() const { return m_numberFamily; }
    QString iconFontFamily() const { return m_iconFontFamily; }

    double monthIncome() const { return m_monthIncome; }
    double monthExpense() const { return m_monthExpense; }
    double monthBalance() const { return m_monthIncome - m_monthExpense; }
    double lastMonthExpense() const { return m_lastMonthExpense; }
    double lastMonthIncome() const { return m_lastMonthIncome; }
    double expenseChange() const { return m_expenseChange; }
    double incomeChange() const { return m_incomeChange; }
    double dailyAverage() const { return m_dailyAverage; }
    int monthTransactionCount() const { return m_monthTransactionCount; }
    double netWorth() const { return m_netWorth; }
    double totalAssets() const { return m_totalAssets; }
    double totalLiabilities() const { return m_totalLiabilities; }
    double monthBudget() const { return m_monthBudget; }
    double monthBudgetSpent() const { return m_monthBudgetSpent; }
    int monthBudgetPercent() const;

    QVariantList recentTransactions() const { return m_recentTransactions; }
    QVariantList categoryBreakdown() const { return m_categoryBreakdown; }
    QVariantList incomeBreakdown() const { return m_incomeBreakdown; }
    QVariantList dailySeries() const { return m_dailySeries; }
    QVariantList quarterSeries() const { return m_quarterSeries; }
    QVariantList monthlyTrend() const { return m_monthlyTrend; }
    QVariantList weekdayStats() const { return m_weekdayStats; }
    QVariantList allCategoryOptions() const { return m_allCategoryOptions; }

    bool isDemoData() const { return m_isDemoData; }
    QString lastError() const { return m_lastError; }
    QString dataPath() const;
    QString version() const;
    QString qtVersion() const;
    bool isMac() const;
    QString startupPage() const { return m_startupPage; }
    QString startupModal() const { return m_startupModal; }

    void setStartupPage(const QString &page) { m_startupPage = page; }
    void setStartupModal(const QString &modal) { m_startupModal = modal; }

    Q_INVOKABLE void setAccountBillsAccount(int accountId);
    Q_INVOKABLE void shiftMonth(int delta);
    Q_INVOKABLE void goToToday();

    Q_INVOKABLE QString money(double value) const;
    Q_INVOKABLE QString formatAmountInput(const QString &text) const;
    Q_INVOKABLE QString moneyPlain(double value) const;
    Q_INVOKABLE QString signedMoney(double value, bool income) const;
    Q_INVOKABLE QString compactNumber(double value) const;
    Q_INVOKABLE QString percentLabel(double value) const;

    Q_INVOKABLE QVariantMap transactionById(int id) const;
    Q_INVOKABLE int saveTransaction(const QVariantMap &data);
    Q_INVOKABLE bool deleteTransaction(int id);
    Q_INVOKABLE QVariantList categoryOptions(const QString &kind) const;
    Q_INVOKABLE QVariantList accountOptions() const;
    Q_INVOKABLE QVariantList budgetCategoryOptions() const;

    Q_INVOKABLE int saveAccount(const QVariantMap &data);
    Q_INVOKABLE bool deleteAccount(int id);
    Q_INVOKABLE void moveAccount(int from, int to);
    Q_INVOKABLE bool accountInUse(int id) const;

    Q_INVOKABLE bool saveBudget(int categoryId, double amount);
    Q_INVOKABLE bool saveCustomBudget(const QString &name, double amount,
                                      const QString &icon, const QString &color);
    Q_INVOKABLE bool deleteBudget(int categoryId);

    Q_INVOKABLE QString exportCsvTo(const QString &path);
    Q_INVOKABLE QString suggestedExportName() const;
    Q_INVOKABLE QString backupTo(const QString &path);
    Q_INVOKABLE QString suggestedBackupName() const;
    Q_INVOKABLE void openDataFolder();
    Q_INVOKABLE void copyText(const QString &text) const;

    Q_INVOKABLE bool clearAllData();
    Q_INVOKABLE bool loadDemoData();
    Q_INVOKABLE QString todayText() const;
    Q_INVOKABLE QString relativeDay(const QString &date) const;

signals:
    void currentMonthChanged();
    void themeModeChanged();
    void accentNameChanged();
    void customAccentChanged();
    void fontChanged();
    void currencyChanged();
    void uiOrderChanged();
    void systemDarkChanged();
    void appearanceChanged();
    void summaryChanged();
    void dataChanged();
    void lastErrorChanged();

public slots:
    void refreshAll();

private:
    struct MonthTotals {
        double income = 0.0;
        double expense = 0.0;
        int count = 0;
    };
    MonthTotals totalsForMonth(const QString &month) const;
    void refreshSummary();
    void setLastError(const QString &message);
    QString monthKey(const QDate &date) const { return date.toString("yyyy-MM"); }
    QDate monthStart() const;

    TransactionModel *m_transactions = nullptr;
    TransactionModel *m_accountBills = nullptr;
    AccountModel *m_accounts = nullptr;
    CategoryModel *m_categories = nullptr;
    BudgetModel *m_budgets = nullptr;

    QSettings m_settings;
    QString m_month;
    QString m_themeMode = "system";
    QString m_accentName = "coral";
    QString m_customAccent = "#E85D75";
    QString m_currencyCode = "CNY";
    QStringList m_navOrder;
    QStringList m_summaryOrder;
    QString m_fontFamily;
    QString m_numberFamily;
    QString m_defaultFontFamily;
    QString m_fontChoice = "system";
    QVariantList m_fontOptions;
    QString m_iconFontFamily = "Material Symbols Rounded";

    double m_monthIncome = 0.0;
    double m_monthExpense = 0.0;
    double m_lastMonthExpense = 0.0;
    double m_lastMonthIncome = 0.0;
    double m_expenseChange = 0.0;
    double m_incomeChange = 0.0;
    double m_dailyAverage = 0.0;
    int m_monthTransactionCount = 0;
    double m_netWorth = 0.0;
    double m_totalAssets = 0.0;
    double m_totalLiabilities = 0.0;
    double m_monthBudget = 0.0;
    double m_monthBudgetSpent = 0.0;

    QVariantList m_recentTransactions;
    QVariantList m_categoryBreakdown;
    QVariantList m_incomeBreakdown;
    QVariantList m_dailySeries;
    QVariantList m_quarterSeries;
    QVariantList m_monthlyTrend;
    QVariantList m_weekdayStats;
    QVariantList m_allCategoryOptions;

    bool m_isDemoData = false;
    QString m_lastError;
    QString m_startupPage = "overview";
    QString m_startupModal;
};
