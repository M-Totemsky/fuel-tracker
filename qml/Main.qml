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

    property int pendingVehicleId: -1   // für Umbenennen/Löschen-Dialog

    function fmtDate(iso) {
        var p = iso.slice(0, 10).split('-')
        return p.length === 3 ? p[2] + "." + p[1] + "." + p[0] : iso
    }

    function fmtNum(x, digits) {
        var s = x.toFixed(digits)
        return fuelTracker.language === "English" ? s : s.replace('.', ',')
    }

    function fmtMoney(x) {
        return fuelTracker.currencySymbol(fuelTracker.currency) + " " + fmtNum(x, 2)
    }

    function parseDate(text) {
        var t = text.trim()
        if (t.length === 0) { return new Date() }
        var parts = t.split(/[-./]/)
        if (parts.length !== 3) { return null }
        var y, mo, da
        if (parts[0].length === 4) {        // JJJJ-MM-TT / YYYY-MM-DD
            y = parseInt(parts[0], 10)
            mo = parseInt(parts[1], 10) - 1
            da = parseInt(parts[2], 10)
        } else {                            // TT-MM-JJJJ / DD-MM-YYYY
            da = parseInt(parts[0], 10)
            mo = parseInt(parts[1], 10) - 1
            y = parseInt(parts[2], 10)
        }
        if (isNaN(y) || isNaN(mo) || isNaN(da)) { return null }
        return new Date(y, mo, da)
    }

    header: ToolBar {
        background: Rectangle { color: "#2b2b2b" }
        height: 56
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            Label {
                text: "Fuel Tracker"
                color: "#ffffff"
                font.pixelSize: 18
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            ToolButton {
                id: vehicleBtn
                text: fuelTracker.activeVehicleName
                font.pixelSize: 15
                onClicked: vehiclePopup.open()
            }
            ToolButton {
                id: settingsBtn
                text: fuelTracker.strings["settings"]
                font.pixelSize: 15
                onClicked: settingsPopup.open()
            }
        }
    }

    // Schneller Fahrzeugwechsel direkt unter der Kopfzeile
    Popup {
        id: vehiclePopup
        modal: true
        dim: true
        focus: true
        x: 0
        y: root.header.height
        width: root.width
        height: Math.min(vcCol.implicitHeight + 24, root.height - root.header.height - 20)
        padding: 12
        background: Rectangle { color: "#333333" }

        ColumnLayout {
            id: vcCol
            anchors.fill: parent
            spacing: 4

            Label {
                text: fuelTracker.strings["vehicles"]
                color: "#aaaaaa"
                font.pixelSize: 14
            }

            Repeater {
                model: fuelTracker.vehicles
                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: (modelData.isActive ? "✓ " : "") + modelData.name
                          + "  ·  " + modelData.fuelTypeLabel
                    font.pixelSize: 15
                    onClicked: {
                        fuelTracker.setActiveVehicle(modelData.id)
                        vehiclePopup.close()
                    }
                }
            }
        }
    }

    Popup {
        id: settingsPopup
        modal: true
        dim: true
        focus: true
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        width: Math.min(root.width - 40, 420)
        height: Math.min(settingsScroll.Layout.preferredHeight + 76, root.height - 40)
        padding: 16
        background: Rectangle { color: "#333333"; radius: 10 }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            ScrollView {
                id: settingsScroll
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(settingsCol.implicitHeight, root.height - 240)
                clip: true
                contentWidth: availableWidth
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                ColumnLayout {
                    id: settingsCol
                    width: settingsScroll.availableWidth
                    spacing: 10

                    // Reihenfolge: 1. Sprache, 2. Währung, 3. Einheit
                    Label { text: fuelTracker.strings["languageName"]; color: "#ffffff" }
                    ComboBox {
                        Layout.fillWidth: true
                        model: fuelTracker.languages
                        currentIndex: fuelTracker.languages.indexOf(fuelTracker.language)
                        onActivated: fuelTracker.setLanguage(currentText)
                    }

                    Label { text: fuelTracker.strings["currencyName"]; color: "#ffffff" }
                    ComboBox {
                        Layout.fillWidth: true
                        model: fuelTracker.currencies
                        currentIndex: fuelTracker.currencies.indexOf(fuelTracker.currency)
                        onActivated: fuelTracker.setCurrency(currentText)
                    }

                    Label { text: fuelTracker.strings["unitName"]; color: "#ffffff" }
                    ComboBox {
                        Layout.fillWidth: true
                        model: fuelTracker.unitOptions
                        currentIndex: fuelTracker.units.indexOf(fuelTracker.unit)
                        onActivated: function(i) { fuelTracker.setUnit(fuelTracker.units[i]) }
                    }

                    Label { text: fuelTracker.strings["vehicles"]; color: "#ffffff" }

                    // Neues Fahrzeug anlegen
                    RowLayout {
                        spacing: 6
                        TextField {
                            id: newVehicleField
                            Layout.fillWidth: true
                            placeholderText: fuelTracker.strings["name"]
                            color: "#ffffff"
                            inputMethodHints: Qt.ImhNoPredictiveText
                        }
                        ComboBox {
                            id: fuelTypeCombo
                            Layout.preferredWidth: 110
                            model: fuelTracker.fuelTypeOptions
                            currentIndex: 0
                        }
                        Button {
                            text: "+"
                            font.pixelSize: 18
                            onClicked: {
                                if (fuelTracker.addVehicle(newVehicleField.text,
                                                          fuelTypeCombo.currentIndex) >= 0) {
                                    newVehicleField.text = ""
                                }
                            }
                        }
                    }

                    // Fahrzeugliste mit Umbenennen/Löschen
                    Repeater {
                        model: fuelTracker.vehicles
                        Rectangle {
                            Layout.fillWidth: true
                            height: 52
                            color: modelData.isActive ? "#3a4a3a" : "#2a2a2a"
                            radius: 6
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                Column {
                                    spacing: 1
                                    Label {
                                        text: (modelData.isActive ? "✓ " : "") + modelData.name
                                        color: "#ffffff"
                                        font.pixelSize: 15
                                        font.bold: true
                                    }
                                    Label {
                                        text: modelData.fuelTypeLabel
                                        color: "#999999"
                                        font.pixelSize: 12
                                    }
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: fuelTracker.strings["renameVehicle"]
                                    font.pixelSize: 12
                                    onClicked: {
                                        root.pendingVehicleId = modelData.id
                                        renameNameField.text = modelData.name
                                        renamePopup.open()
                                    }
                                }
                                Button {
                                    text: fuelTracker.strings["deleteVehicle"]
                                    font.pixelSize: 12
                                    enabled: fuelTracker.vehicles.length > 1
                                    onClicked: {
                                        root.pendingVehicleId = modelData.id
                                        deleteConfirmPopup.open()
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Button {
                Layout.fillWidth: true
                text: fuelTracker.strings["close"]
                highlighted: true
                onClicked: settingsPopup.close()
            }
        }
    }

    // Umbenennen-Dialog
    Popup {
        id: renamePopup
        modal: true
        dim: true
        focus: true
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        width: Math.min(root.width - 40, 360)
        height: 160
        padding: 16
        background: Rectangle { color: "#333333"; radius: 10 }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Label {
                text: fuelTracker.strings["renameVehicle"]
                color: "#ffffff"
                font.pixelSize: 15
                font.bold: true
            }
            TextField {
                id: renameNameField
                Layout.fillWidth: true
                color: "#ffffff"
                inputMethodHints: Qt.ImhNoPredictiveText
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                Button {
                    text: fuelTracker.strings["cancel"]
                    onClicked: renamePopup.close()
                }
                Button {
                    text: fuelTracker.strings["saveEntry"]
                    highlighted: true
                    onClicked: {
                        fuelTracker.renameVehicle(root.pendingVehicleId, renameNameField.text)
                        renamePopup.close()
                    }
                }
            }
        }
    }

    // Lösch-Rückfrage beim Fahrzeug
    Popup {
        id: deleteConfirmPopup
        modal: true
        dim: true
        focus: true
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        width: Math.min(root.width - 40, 360)
        height: 170
        padding: 16
        background: Rectangle { color: "#333333"; radius: 10 }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Label {
                text: fuelTracker.strings["confirmDeleteVehicle"]
                color: "#ffffff"
                font.pixelSize: 15
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                Item { Layout.fillWidth: true }
                Button {
                    text: fuelTracker.strings["cancel"]
                    onClicked: deleteConfirmPopup.close()
                }
                Button {
                    text: fuelTracker.strings["yes"]
                    highlighted: true
                    onClicked: {
                        fuelTracker.deleteVehicle(root.pendingVehicleId)
                        deleteConfirmPopup.close()
                    }
                }
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
            height: 58
            color: "#313131"
            radius: 8
            Column {
                anchors.centerIn: parent
                spacing: 2
                Label {
                    text: fuelTracker.lastEntrySummary
                    color: "#ffffff"
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Label {
                    text: fuelTracker.strings["totalSpend"] + ": " + root.fmtMoney(fuelTracker.totalCost)
                    color: "#aaaaaa"
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        // Eingabeformular: oben fixiert, damit auch bei geöffneter
        // Tastatur alle Felder + "Speichern" erreichbar sind
        Rectangle {
            id: formCard
            Layout.fillWidth: true
            Layout.preferredHeight: 340
            color: "#262626"
            radius: 10
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: fuelTracker.strings["date"]; color: "#ffffff"; Layout.preferredWidth: 90 }
                    TextField {
                        id: dateField
                        Layout.fillWidth: true
                        text: Qt.formatDateTime(new Date(), "dd.MM.yyyy")
                        placeholderText: "TT.MM.JJJJ"
                        color: "#ffffff"
                        inputMethodHints: Qt.ImhDate
                    }
                    Button {
                        text: fuelTracker.strings["today"]
                        onClicked: dateField.text = Qt.formatDateTime(new Date(), "dd.MM.yyyy")
                    }
                }

                RowLayout {
                    Label {
                        text: fuelTracker.unit === "Gallonen" ? fuelTracker.strings["odometerMi"] : fuelTracker.strings["odometerKm"]
                        color: "#ffffff"; Layout.preferredWidth: 90
                    }
                    TextField {
                        id: kmField
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        placeholderText: fuelTracker.language === "English" ? "e.g. 124350" : "z. B. 124350"
                    }
                }

                RowLayout {
                    Label {
                        text: fuelTracker.isElectric ? fuelTracker.strings["kwhUnit"]
                                                    : (fuelTracker.unit === "Gallonen" ? fuelTracker.strings["unitGallon"] : fuelTracker.strings["unitLiter"])
                        color: "#ffffff"; Layout.preferredWidth: 90
                    }
                    TextField {
                        id: litersField
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        placeholderText: fuelTracker.language === "English"
                            ? (fuelTracker.isElectric ? "e.g. 50" : (fuelTracker.unit === "Gallonen" ? "e.g. 10.5" : "e.g. 42.5"))
                            : (fuelTracker.isElectric ? "z. B. 50" : (fuelTracker.unit === "Gallonen" ? "z. B. 10,5" : "z. B. 42,5"))
                    }
                }

                RowLayout {
                    Label {
                        text: fuelTracker.currencySymbol(fuelTracker.currency) + "/"
                              + (fuelTracker.isElectric ? fuelTracker.strings["kwhUnit"]
                                                        : (fuelTracker.unit === "Gallonen" ? fuelTracker.strings["unitGallon"] : fuelTracker.strings["unitLiter"]))
                        color: "#ffffff"; Layout.preferredWidth: 90
                    }
                    TextField {
                        id: priceField
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        placeholderText: fuelTracker.language === "English"
                            ? (fuelTracker.isElectric ? "e.g. 0.45" : "e.g. 1.819")
                            : (fuelTracker.isElectric ? "z. B. 0,45" : "z. B. 1,819")
                    }
                }

                RowLayout {
                    Label {
                        text: fuelTracker.isElectric ? fuelTracker.strings["fullCharge"] : fuelTracker.strings["fullTank"]
                        color: "#ffffff"; Layout.preferredWidth: 90
                    }
                    Switch {
                        id: fullSwitch
                        checked: true
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: fuelTracker.strings["saveEntry"]
                    highlighted: true
                    onClicked: {
                        var km = parseFloat(kmField.text.replace(',', '.'))
                        var lit = parseFloat(litersField.text.replace(',', '.'))
                        var price = parseFloat(priceField.text.replace(',', '.'))
                        if (isNaN(km) || isNaN(lit) || isNaN(price)) { return }

                        var dt = root.parseDate(dateField.text)
                        if (dt === null) { return }

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
            text: fuelTracker.strings["history"]
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
                        anchors.leftMargin: 10
                        Label { text: root.fmtDate(modelData.date); color: "#ffffff" }
                        Item { Layout.fillWidth: true }
                        Label { text: root.fmtNum(modelData.amount, 1) + " " + modelData.amountUnit; color: "#dddddd" }
                        Label { text: root.fmtNum(modelData.km, 0) + " " + modelData.distUnit; color: "#dddddd" }
                        Label { text: root.fmtMoney(modelData.cost); color: "#9ccc9c" }
                        ToolButton {
                            text: "✕"
                            onClicked: fuelTracker.deleteEntry(modelData.id)
                            contentItem: Label {
                                text: "✕"
                                color: "#ff6b6b"
                                font.pixelSize: 16
                            }
                        }
                    }
                }
                ScrollBar.vertical: ScrollBar {}
            }

            Label {
                anchors.centerIn: parent
                text: fuelTracker.strings["noEntries"]
                color: "#777777"
                visible: fuelTracker.entryCount === 0
            }
        }

        Button {
            Layout.fillWidth: true
            text: fuelTracker.strings["deleteAll"]
            onClicked: fuelTracker.clearAll()
        }
    }
}