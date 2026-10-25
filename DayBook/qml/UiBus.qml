pragma Singleton
import QtQuick

QtObject {
    id: root

    signal toastRequested(string message, string kind)
    signal openTransactionRequested(int id)
    signal navigateRequested(string page)
    signal openAccountRequested(int id)
    signal openBudgetRequested(int id)

    function toast(message, kind) {
        toastRequested(message, kind === undefined ? "success" : kind)
    }

    function openTransaction(id) {
        openTransactionRequested(id === undefined ? 0 : id)
    }

    function navigate(page) {
        navigateRequested(page)
    }

    function openAccount(id) {
        openAccountRequested(id === undefined ? 0 : id)
    }

    function openBudget(id) {
        openBudgetRequested(id === undefined ? 0 : id)
    }
}
