import QtQuick
import DayBook

Text {
    id: root

    property double value: 0
    property bool withSymbol: true
    property bool signedAmount: false
    property bool income: false
    property real amountSize: 16
    property int amountWeight: 600
    property color amountColor: Theme.ink

    text: {
        const currencyKey = App.currencyCode
        if (root.signedAmount)
            return App.signedMoney(root.value, root.income)
        if (root.withSymbol)
            return App.money(root.value)
        return App.moneyPlain(root.value)
    }
    color: root.amountColor
    font.family: Theme.numberFamily
    font.pixelSize: Math.round(root.amountSize)
    font.weight: root.amountWeight
    font.features: ({ "tnum": 1, "lnum": 1, "ss01": 1 })
    verticalAlignment: Text.AlignVCenter
}
