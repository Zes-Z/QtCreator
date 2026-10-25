#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QDate>

class Database
{
public:
    static Database &instance();

    bool init(QString *error = nullptr);
    QSqlDatabase db() const { return m_db; }
    QString path() const { return m_path; }
    QString dataDir() const;

    bool seedDemoData(QString *error = nullptr);
    bool clearFinancialData(QString *error = nullptr);
    bool exportCsv(const QString &filePath, QString *error = nullptr) const;
    bool backup(const QString &filePath, QString *error = nullptr) const;

    bool hasTransactions() const;
    bool isDemoSeeded() const;
    int transactionCount() const;

private:
    Database() = default;
    bool createSchema(QString *error);
    bool ensureDefaults(QString *error);
    bool setMeta(const QString &key, const QString &value);
    QString meta(const QString &key, const QString &fallback = QString()) const;
    int categoryId(const QString &name, const QString &kind) const;
    int accountId(const QString &name) const;

    QSqlDatabase m_db;
    QString m_path;
};
