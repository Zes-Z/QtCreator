#include "appcontroller.h"
#include "models.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QFileInfo>
#include <QQuickWindow>
#include <QTimer>
#include <QCommandLineParser>
#include <QImage>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QStandardPaths>
#include <QDate>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("DayBook");
    QCoreApplication::setOrganizationDomain("daybook.app");
    QCoreApplication::setApplicationName("DayBook");
    QCoreApplication::setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon(":/assets/appicon.png"));

    QQuickStyle::setStyle("Basic");

    QCommandLineParser parser;
    parser.setApplicationDescription("DayBook - a quiet place for your daily numbers.");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption pageOption("page", "Start on a page (overview, transactions, insights, accounts, budgets, settings).", "page");
    QCommandLineOption modalOption("modal", "Open a modal on startup (add, account, budget).", "modal");
    QCommandLineOption screenshotOption("screenshot", "Grab the window to a PNG file after startup.", "path");
    QCommandLineOption delayOption("delay", "Screenshot delay in milliseconds.", "ms", "1400");
    QCommandLineOption sizeOption("size", "Window size for screenshots, for example 1240x840.", "WxH");
    QCommandLineOption selftestOption("selftest", "Run backend self tests against an isolated test database.");
    parser.addOption(pageOption);
    parser.addOption(modalOption);
    parser.addOption(screenshotOption);
    parser.addOption(delayOption);
    parser.addOption(sizeOption);
    parser.addOption(selftestOption);
    parser.process(app);

    const bool selfTest = parser.isSet(selftestOption);
    if (selfTest) {
        QStandardPaths::setTestModeEnabled(true);
        const QString testDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/DayBook";
        QDir(testDir).removeRecursively();
        qInfo() << "DayBook self test data:" << testDir;
    }

    AppController controller;
    QString error;
    if (!controller.initialize(&error)) {
        qCritical() << "DayBook could not start:" << error;
        return 1;
    }
    if (parser.isSet(pageOption))
        controller.setStartupPage(parser.value(pageOption));
    if (parser.isSet(modalOption))
        controller.setStartupModal(parser.value(modalOption));

    if (selfTest) {
        int failures = 0;
        auto check = [&failures](bool ok, const char *name) {
            qInfo().noquote() << (ok ? "PASS" : "FAIL") << name;
            if (!ok)
                ++failures;
        };

        const QVariantList categories = controller.categoryOptions("expense");
        const QVariantList accounts = controller.accountOptions();
        check(!categories.isEmpty(), "demo categories available");
        check(accounts.size() >= 2, "demo accounts available");
        const int categoryId = categories.isEmpty() ? 0 : categories.first().toMap().value("id").toInt();
        const int accountId = accounts.isEmpty() ? 0 : accounts.first().toMap().value("id").toInt();

        QVariantMap tx;
        tx["kind"] = "expense";
        tx["amount"] = 12.34;
        tx["categoryId"] = categoryId;
        tx["accountId"] = accountId;
        tx["note"] = "selftest";
        tx["date"] = QDate::currentDate().toString("yyyy-MM-dd");
        tx["time"] = "12:00";
        const int txId = controller.saveTransaction(tx);
        check(txId > 0, "create transaction");
        const QVariantMap loaded = controller.transactionById(txId);
        check(qAbs(loaded.value("amount").toDouble() - 12.34) < 0.001, "read transaction amount");
        check(loaded.value("note").toString() == "selftest", "read transaction note");

        TransactionModel *txModel = controller.transactions();
        auto rowsMatch = [txModel](const QString &key, const QVariant &value) {
            for (int i = 0; i < txModel->rowCount(); ++i) {
                const QVariantMap row = txModel->get(i);
                if (row.value("isHeader").toBool())
                    continue;
                if (row.value(key) != value)
                    return false;
            }
            return true;
        };
        txModel->clearFilters();
        txModel->setKindFilter("income");
        check(txModel->transactionCount() > 0, "income filter has rows");
        check(rowsMatch("kind", "income"), "income filter only income");
        check(txModel->totalExpense() == 0.0 && txModel->totalIncome() > 0.0, "income filter totals");
        txModel->setKindFilter("expense");
        check(txModel->transactionCount() > 0, "expense filter has rows");
        check(rowsMatch("kind", "expense"), "expense filter only expense");
        check(txModel->totalIncome() == 0.0 && txModel->totalExpense() > 0.0, "expense filter totals");
        txModel->setKindFilter("all");
        txModel->setCategoryFilter(categoryId);
        check(rowsMatch("categoryId", categoryId), "category filter matches");
        txModel->setKindFilter("income");
        check(txModel->categoryFilter() == 0, "mismatched category filter cleared");
        check(rowsMatch("kind", "income"), "type switch after category filter");
        txModel->clearFilters();
        txModel->setAccountFilter(accountId);
        check(rowsMatch("accountId", accountId), "account filter matches");
        txModel->clearFilters();
        txModel->setSearch("selftest");
        check(txModel->transactionCount() >= 1, "search filter matches");
        txModel->clearFilters();

        int freeBudgetCategory = 0;
        const QVariantList budgetOptions = controller.budgetCategoryOptions();
        for (const QVariant &v : budgetOptions) {
            const QVariantMap item = v.toMap();
            if (!item.value("hasBudget").toBool()) {
                freeBudgetCategory = item.value("id").toInt();
                break;
            }
        }
        check(freeBudgetCategory > 0, "unbudgeted category available");
        check(controller.saveBudget(freeBudgetCategory, 520.0), "save budget");
        check(controller.budgets()->rowCount() > 0, "budget model populated");
        check(controller.deleteBudget(freeBudgetCategory), "delete budget");
        check(controller.saveCustomBudget("自测预算对象", 88.0, "category", "#8B7CF6"),
              "create custom budget object");
        bool customBudgetFound = false;
        for (const QVariant &v : controller.budgetCategoryOptions()) {
            const QVariantMap item = v.toMap();
            if (item.value("name").toString() == "自测预算对象" && item.value("hasBudget").toBool()) {
                customBudgetFound = true;
                break;
            }
        }
        check(customBudgetFound, "custom budget object visible");

        QVariantMap account;
        account["name"] = "自测账户";
        account["type"] = "cash";
        account["icon"] = "wallet";
        account["color"] = "#E8A94E";
        account["initial"] = 100.0;
        const int selfAccount = controller.saveAccount(account);
        check(selfAccount > 0, "create account");
        check(controller.deleteAccount(selfAccount), "delete account");

        const QString csvPath = QDir::tempPath() + "/daybook-selftest.csv";
        const QString exported = controller.exportCsvTo(csvPath);
        check(!exported.isEmpty() && QFileInfo(exported).size() > 0, "export csv");
        QFile csv(exported);
        if (csv.open(QIODevice::ReadOnly)) {
            const QString content = QString::fromUtf8(csv.readAll());
            check(content.contains("selftest"), "csv contains test row");
        }

        const QString backupPath = QDir::tempPath() + "/daybook-selftest.db";
        const QString backup = controller.backupTo(backupPath);
        check(!backup.isEmpty() && QFileInfo(backup).size() > 0, "backup database");

        check(controller.deleteTransaction(txId), "delete transaction");
        check(controller.transactionById(txId).isEmpty(), "transaction deleted");

        const int transferFrom = accounts.value(0).toMap().value("id").toInt();
        const int transferTo = accounts.value(1).toMap().value("id").toInt();
        auto balanceOf = [&controller](int id) {
            const QVariantList list = controller.accountOptions();
            for (const QVariant &v : list) {
                const QVariantMap map = v.toMap();
                if (map.value("id").toInt() == id)
                    return map.value("balance").toDouble();
            }
            return 0.0;
        };
        const double fromBefore = balanceOf(transferFrom);
        const double toBefore = balanceOf(transferTo);
        QVariantMap transfer;
        transfer["kind"] = "transfer";
        transfer["amount"] = 5.0;
        transfer["accountId"] = transferFrom;
        transfer["transferAccountId"] = transferTo;
        transfer["note"] = "transfer-selftest";
        transfer["date"] = QDate::currentDate().toString("yyyy-MM-dd");
        transfer["time"] = "12:30";
        const int transferId = controller.saveTransaction(transfer);
        check(transferId > 0, "create transfer");
        const QVariantMap loadedTransfer = controller.transactionById(transferId);
        check(loadedTransfer.value("kind").toString() == "transfer"
              && loadedTransfer.value("transferAccountId").toInt() == transferTo,
              "read transfer destination");
        check(qAbs((fromBefore - balanceOf(transferFrom)) - 5.0) < 0.001
              && qAbs((balanceOf(transferTo) - toBefore) - 5.0) < 0.001,
              "transfer moves balances");
        check(controller.deleteTransaction(transferId), "delete transfer");
        check(qAbs(balanceOf(transferFrom) - fromBefore) < 0.001
              && qAbs(balanceOf(transferTo) - toBefore) < 0.001,
              "delete transfer restores balances");

        QFile::remove(exported);
        QFile::remove(backup);
        qInfo() << "DayBook self test finished; failures:" << failures;
        return failures == 0 ? 0 : 1;
    }

    qmlRegisterSingletonInstance("DayBook", 1, 0, "App", &controller);
    qmlRegisterUncreatableType<TransactionModel>("DayBook", 1, 0, "TransactionModel", "Internal model");
    qmlRegisterUncreatableType<AccountModel>("DayBook", 1, 0, "AccountModel", "Internal model");
    qmlRegisterUncreatableType<CategoryModel>("DayBook", 1, 0, "CategoryModel", "Internal model");
    qmlRegisterUncreatableType<BudgetModel>("DayBook", 1, 0, "BudgetModel", "Internal model");
    QQmlEngine::setObjectOwnership(&controller, QQmlEngine::CppOwnership);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);

    engine.loadFromModule("DayBook", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (window && parser.isSet(sizeOption)) {
        const QString spec = parser.value(sizeOption);
        const QStringList parts = spec.split('x', Qt::SkipEmptyParts);
        if (parts.size() == 2) {
            const int width = parts.at(0).toInt();
            const int height = parts.at(1).toInt();
            if (width > 800 && height > 600)
                window->resize(width, height);
        }
    }

    if (window && parser.isSet(screenshotOption)) {
        {
            const QString path = parser.value(screenshotOption);
            const int delay = qMax(200, parser.value(delayOption).toInt());
            QTimer::singleShot(delay, &app, [window, path, &app]() {
                QImage image = window->grabWindow();
                QDir().mkpath(QFileInfo(path).absolutePath());
                if (image.save(path))
                    qInfo() << "Screenshot saved to" << path << image.size();
                else
                    qWarning() << "Could not save screenshot to" << path;
                app.quit();
            });
        }
    }

    qInfo() << "DayBook window" << (window ? window->size() : QSize()) << "DPR" << (window ? window->devicePixelRatio() : 1.0);
    return app.exec();
}
