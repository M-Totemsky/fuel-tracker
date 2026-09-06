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

    function fmtDate(iso) {
        var p = iso.slice(0, 10).split('-')
        return p.length === 3 ? p[2] + "." + p[1] + "." + p[0] : iso
    }

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
        anchors.margins: 12
        spacing: 10

        // Status (kompakt, direkt über dem Formular)
        Rectangle {
            Layout.fillWidth: true
            height: 44
            color: "#313131"
            radius: 8
            Label {
                anchors.centerIn: parent
                text: fuelTracker.lastEntrySummary
                color: "#ffffff"
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
            }
        }

        // Eingabeformular: oben fixiert, damit auch bei geöffneter
        // Tastatur alle Felder + "Speichern" erreichbar sind
        Rectangle {
            id: formCard
            Layout.fillWidth: true
            color: "#262626"
            radius: 10
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Datum"; color: "#ffffff"; Layout.preferredWidth: 90 }
                    TextField {
                        id: dateField
                        Layout.fillWidth: true
                        text: Qt.formatDateTime(new Date(), "dd.MM.yyyy")
                        placeholderText: "TT.MM.JJJJ"
                        color: "#ffffff"
                        inputMethodHints: Qt.ImhDate
                    }
                    Button {
                        text: "Heute"
                        onClicked: dateField.text = Qt.formatDateTime(new Date(), "dd.MM.yyyy")
                    }
                }

                RowLayout {
                    Label { text: "km-Stand"; color: "#ffffff"; Layout.preferredWidth: 90 }
                    TextField {
                        id: kmField
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        placeholderText: "z. B. 124350"
                    }
                }

                RowLayout {
                    Label { text: fuelTracker.unit; color: "#ffffff"; Layout.preferredWidth: 90 }
                    TextField {
                        id: litersField
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        placeholderText: fuelTracker.unit === "Gallonen" ? "z. B. 10,5" : "z. B. 42,5"
                    }
                }

                RowLayout {
                    Label {
                        text: fuelTracker.currencySymbol(fuelTracker.currency) + "/" + fuelTracker.unit
                        color: "#ffffff"; Layout.preferredWidth: 90
                    }
                    TextField {
                        id: priceField
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        placeholderText: "z. B. 1,819"
                    }
                }

                RowLayout {
                    Label { text: "Voll getankt"; color: "#ffffff"; Layout.preferredWidth: 90 }
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
                            var y, mo, da, parts, p
                            if (dtext.indexOf('-') >= 0) {
                                parts = dtext.split('-')
                                if (parts.length !== 3) { return }
                                y = parseInt(parts[0], 10)
                                mo = parseInt(parts[1], 10) - 1
                                da = parseInt(parts[2], 10)
                            } else {
                                p = dtext.split('.')
                                if (p.length !== 3) { return }
                                da = parseInt(p[0], 10)
                                mo = parseInt(p[1], 10) - 1
                                y = parseInt(p[2], 10)
                            }
                            if (isNaN(y) || isNaN(mo) || isNaN(da)) { return }
                            dt = new Date(y, mo, da)
                        }

                        if (fuelTracker.addEntry(dt, km, lit, price, fullSwitch.checked)) {
                            dateField.text = Qt.formatDateTime(new Date(), "dd.MM.yyyy")
                            kmField.text = ""
                            litersField.text = ""
                            priceField.text = ""
                            listView.positionViewAtEnd()
                        }
                    }
                }
            }
        }

        // Verlauf (eigener Scrollbereich)
        Label {
            text: "Verlauf"
            color: "#ffffff"
            font.pixelSize: 18
            font.bold: true
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: listView
                anchors.fill: parent
                clip: true
                model: fuelTracker.entries
                delegate: Rectangle {
                    width: listView.width
                    height: 48
                    color: index % 2 === 0 ? "#2c2c2c" : "#282828"
                    radius: 6
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        Label { text: root.fmtDate(modelData.date); color: "#ffffff" }
                        Item { Layout.fillWidth: true }
                        Label { text: modelData.amount.toFixed(1) + " " + modelData.amountUnit; color: "#dddddd" }
                        Label { text: modelData.km.toFixed(0) + " km"; color: "#dddddd" }
                    }
                }
                ScrollBar.vertical: ScrollBar {}
            }

            Label {
                anchors.centerIn: parent
                text: "Noch keine Einträge"
                color: "#777777"
                visible: fuelTracker.entryCount === 0
            }
        }

        Button {
            Layout.fillWidth: true
            text: "Alle Einträge löschen"
            onClicked: fuelTracker.clearAll()
        }
    }
}