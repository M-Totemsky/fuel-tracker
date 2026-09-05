import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

ApplicationWindow {
    id: root
    visible: true
    width: 480
    height: 800
    title: "Fuel Tracker"
    color: "#1e1e1e"

    // Prio: kleine, klare Einstiegsoberfläche
    header: ToolBar {
        background: Rectangle { color: "#2b2b2b" }
        height: 56
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            Label {
                text: "Fuel Tracker"
                color: "#ffffff"
                font.pixelSize: 20
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            ComboBox {
                id: unitBox
                Layout.preferredWidth: 120
                model: fuelTracker.units
                currentIndex: {
                    var i = fuelTracker.units.indexOf(fuelTracker.unit)
                    return i < 0 ? 0 : i
                }
                onActivated: fuelTracker.setUnit(currentText)
            }
            ComboBox {
                id: curBox
                Layout.preferredWidth: 90
                model: fuelTracker.currencies
                currentIndex: {
                    var i = fuelTracker.currencies.indexOf(fuelTracker.currency)
                    return i < 0 ? 0 : i
                }
                onActivated: fuelTracker.setCurrency(currentText)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // Zusammenfassung
        Rectangle {
            id: summaryCard
            Layout.fillWidth: true
            color: "#313131"
            radius: 10
            height: 120
            Column {
                anchors.centerIn: parent
                spacing: 4
                Label {
                    text: fuelTracker.lastEntrySummary
                    color: "#ffffff"
                    font.pixelSize: 20
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Label {
                    text: fuelTracker.entryCount + " Tankungen"
                    color: "#aaaaaa"
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        // Eingabeformular
        Rectangle {
            id: formCard
            Layout.fillWidth: true
            Layout.preferredHeight: 330
            color: "#262626"
            radius: 10
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Datum"; color: "#ffffff"; Layout.preferredWidth: 110 }
                    TextField {
                        id: dateField
                        Layout.fillWidth: true
                        placeholderText: "JJJJ-MM-TT (leer = heute)"
                        color: "#ffffff"
                        inputMethodHints: Qt.ImhDate
                    }
                }

                // km
                RowLayout {
                    Label { text: "km-Stand"; color: "#ffffff"; Layout.preferredWidth: 110 }
                    TextField {
                        id: kmField
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        placeholderText: "z. B. 124350"
                    }
                }
                // Menge (Einheit abhängig)
                RowLayout {
                    Label { text: fuelTracker.unit; color: "#ffffff"; Layout.preferredWidth: 110 }
                    TextField {
                        id: litersField
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        placeholderText: fuelTracker.unit === "Gallonen" ? "z. B. 10,5" : "z. B. 42,5"
                    }
                }
                // Preis/Einheit
                RowLayout {
                    Label {
                        text: fuelTracker.currency === "USD" ? "$/" + fuelTracker.unit
                            : fuelTracker.currency === "GBP" ? "£/" + fuelTracker.unit
                            : "€/" + fuelTracker.unit
                        color: "#ffffff"; Layout.preferredWidth: 110
                    }
                    TextField {
                        id: priceField
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        placeholderText: "z. B. 1,819"
                    }
                }

                RowLayout {
                    Label { text: "Voll getankt"; color: "#ffffff"; Layout.preferredWidth: 110 }
                    Switch {
                        id: fullSwitch
                        checked: true
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: "Eintrag speichern"
                    highlighted: true
                    onClicked: {
                        var km = parseFloat(kmField.text.replace(',', '.'))
                        var lit = parseFloat(litersField.text.replace(',', '.'))
                        var price = parseFloat(priceField.text.replace(',', '.'))
                        if (isNaN(km) || isNaN(lit) || isNaN(price)) { return }

                        var dtext = dateField.text.trim()
                        var dt
                        if (dtext.length === 0) {
                            dt = new Date()
                        } else {
                            var parts = dtext.split('-')
                            if (parts.length !== 3) { return }
                            var y = parseInt(parts[0], 10)
                            var mo = parseInt(parts[1], 10) - 1
                            var da = parseInt(parts[2], 10)
                            if (isNaN(y) || isNaN(mo) || isNaN(da)) { return }
                            dt = new Date(y, mo, da)
                        }

                        if (fuelTracker.addEntry(dt, km, lit, price, fullSwitch.checked)) {
                            dateField.text = ""
                            kmField.text = ""
                            litersField.text = ""
                            priceField.text = ""
                        }
                    }
                }
            }
        }

        // Verlauf
        Label {
            text: "Verlauf"
            color: "#ffffff"
            font.pixelSize: 18
            font.bold: true
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: fuelTracker.entries
            delegate: Rectangle {
                width: listView.width
                height: 46
                color: index % 2 === 0 ? "#2c2c2c" : "#282828"
                radius: 6
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    Label { text: modelData.date.slice(0, 10); color: "#ffffff" }
                    Item { Layout.fillWidth: true }
                    Label { text: modelData.amount.toFixed(1) + " " + modelData.amountUnit; color: "#dddddd" }
                    Label { text: modelData.km.toFixed(0) + " km"; color: "#dddddd" }
                }
            }
        }

        Button {
            Layout.fillWidth: true
            text: "Alle Einträge löschen"
            onClicked: fuelTracker.clearAll()
        }
    }
}