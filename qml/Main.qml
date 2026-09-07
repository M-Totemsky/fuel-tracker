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
    property string pendingBackupPath: ""
    property var backups: []

    function showMsg(text) {
        msgLabel.text = text
        msgTimer.restart()
        if (!messagePopup.opened) messagePopup.open()
    }

    // Akzentfarbe wie im App-Icon (knallig gelb) + passende Textfarbe
    property color accent: "#FFD400"
    property color accentText: "#1a1a1a"

    function fmtDate(iso) {
        var p = iso.slice(0, 10).split('-')
        return p.length === 3 ? p[2] + "." + p[1] + "." + p[0] : iso
    }

    function fmtNum(x, digits) {
        var s = x.toFixed(digits)
        return (fuelTracker.language === "English" || fuelTracker.language === "日本語") ? s : s.replace('.', ',')
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
                    background: Rectangle {
                        color: root.accent
                        radius: 6
                    }
                    contentItem: Label {
                        text: fuelTracker.activeVehicleName
                        color: root.accentText
                        font.pixelSize: 15
                    }
                }
                ToolButton {
                    id: settingsBtn
                    objectName: "settingsBtn"
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
                    background: Rectangle {
                        color: modelData.isActive ? root.accent : "#444444"
                        radius: 4
                    }
                    contentItem: Label {
                        text: (modelData.isActive ? "✓ " : "") + modelData.name
                              + "  ·  " + modelData.fuelTypeLabel
                        color: modelData.isActive ? root.accentText : "#ffffff"
                        font.pixelSize: 15
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
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
        x: 0
        y: root.header.height
        width: root.width
        height: root.height - root.header.height
        padding: 12
        background: Rectangle { color: "#2b2b2b" }

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            // Kopfzeile: Titel + Schließen
            RowLayout {
                Label {
                    text: fuelTracker.strings["settings"]
                    color: "#ffffff"
                    font.pixelSize: 18
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                ToolButton {
                    text: "✕"
                    onClicked: settingsPopup.close()
                    contentItem: Label {
                        text: "✕"
                        color: "#ffffff"
                        font.pixelSize: 16
                    }
                }
            }

            // FIXIERTE Zeile zum Anlegen neuer Fahrzeuge: bleibt bei offener
            // Tastatur sichtbar, weil sie am oberen Rand der Seite sitzt.
            RowLayout {
                spacing: 6
                TextField {
                    id: newVehicleField
                    objectName: "newVehicleField"
                    Layout.fillWidth: true
                    placeholderText: fuelTracker.strings["name"]
                    color: "#ffffff"
                    inputMethodHints: Qt.ImhNoPredictiveText
                }
                ComboBox {
                    id: fuelTypeCombo
                    objectName: "fuelTypeCombo"
                    Layout.preferredWidth: 120
                    model: fuelTracker.fuelTypeOptions
                    currentIndex: 0
                }
                Button {
                    objectName: "addVehicleButton"
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 40
                    text: "+"
                    font.pixelSize: 20
                    onClicked: {
                        if (fuelTracker.addVehicle(newVehicleField.text,
                                                  fuelTypeCombo.currentIndex) >= 0) {
                            newVehicleField.text = ""
                        }
                    }
                }
            }

            ScrollView {
                id: settingsScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                ColumnLayout {
                    id: settingsCol
                    width: settingsScroll.availableWidth
                    spacing: 10

                    Label { text: fuelTracker.strings["vehicles"]; color: "#ffffff" }

                    // Fahrzeugliste mit Umbenennen/Löschen
                    Repeater {
                        model: fuelTracker.vehicles
                        Rectangle {
                            Layout.fillWidth: true
                            height: 52
                            color: modelData.isActive ? root.accent : "#2a2a2a"
                            radius: 6
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                Column {
                                    spacing: 1
                                    Label {
                                        text: (modelData.isActive ? "✓ " : "") + modelData.name
                                        color: modelData.isActive ? root.accentText : "#ffffff"
                                        font.pixelSize: 15
                                        font.bold: true
                                    }
                                    Label {
                                        text: modelData.fuelTypeLabel
                                        color: modelData.isActive ? "#555555" : "#999999"
                                        font.pixelSize: 12
                                    }
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: fuelTracker.strings["renameVehicle"]
                                    font.pixelSize: 12
                                    background: Rectangle {
                                        color: modelData.isActive ? "#2b2b2b" : "transparent"
                                        radius: 4
                                        implicitWidth: 92
                                        implicitHeight: 32
                                    }
                                    contentItem: Label {
                                        text: fuelTracker.strings["renameVehicle"]
                                        color: "#ffffff"
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: {
                                        root.pendingVehicleId = modelData.id
                                        renameNameField.text = modelData.name
                                        renamePopup.open()
                                    }
                                }
                                Button {
                                    text: fuelTracker.strings["deleteVehicle"]
                                    font.pixelSize: 12
                                    background: Rectangle {
                                        color: modelData.isActive ? "#2b2b2b" : "transparent"
                                        radius: 4
                                        implicitWidth: 92
                                        implicitHeight: 32
                                    }
                                    contentItem: Label {
                                        text: fuelTracker.strings["deleteVehicle"]
                                        color: "#ffffff"
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: {
                                        root.pendingVehicleId = modelData.id
                                        deleteConfirmPopup.open()
                                    }
                                }
                            }
                        }
                    }

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
                        objectName: "unitCombo"
                        Layout.fillWidth: true
                        model: fuelTracker.unitOptions
                        currentIndex: fuelTracker.units.indexOf(fuelTracker.unit)
                        onActivated: function(i) { fuelTracker.setUnit(fuelTracker.units[i]) }
                    }

                    Label { text: fuelTracker.strings["data"]; color: "#ffffff" }
                    Button {
                        objectName: "csvExportBtn"
                        Layout.fillWidth: true
                        text: fuelTracker.strings["csvExport"]
                        onClicked: {
                            var p = fuelTracker.exportCsv()
                            root.showMsg(p ? fuelTracker.strings["savedTo"] + "\n" + p
                                           : fuelTracker.strings["backupFailed"])
                        }
                    }
                    Button {
                        objectName: "createBackupBtn"
                        Layout.fillWidth: true
                        text: fuelTracker.strings["createBackup"]
                        onClicked: {
                            var p = fuelTracker.createBackup()
                            root.showMsg(p ? fuelTracker.strings["savedTo"] + "\n" + p
                                           : fuelTracker.strings["backupFailed"])
                        }
                    }
                    Button {
                        objectName: "restoreBackupBtn"
                        Layout.fillWidth: true
                        text: fuelTracker.strings["restoreBackup"]
                        onClicked: {
                            root.backups = fuelTracker.backupFiles()
                            restorePopup.open()
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
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

    // Alle Einträge löschen: Bestätigung mit automatischer Sicherung
    Popup {
        id: confirmClearPopup
        modal: true
        dim: true
        focus: true
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        width: Math.min(root.width - 40, 360)
        height: 180
        padding: 16
        background: Rectangle { color: "#333333"; radius: 10 }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Label {
                text: fuelTracker.activeVehicleName + "\n" + fuelTracker.strings["confirmClearAll"]
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
                    onClicked: confirmClearPopup.close()
                }
                Button {
                    text: fuelTracker.strings["yes"]
                    highlighted: true
                    onClicked: {
                        if (fuelTracker.createBackup()) {
                            fuelTracker.clearAll()
                            confirmClearPopup.close()
                            root.showMsg(fuelTracker.strings["savedTo"] + "\n" + fuelTracker.backupFiles()[0])
                        } else {
                            confirmClearPopup.close()
                            root.showMsg(fuelTracker.strings["backupFailed"])
                        }
                    }
                }
            }
        }
    }

    // Sicherung wiederherstellen: Auswahl der gefundenen Sicherungen
    Popup {
        id: restorePopup
        modal: true
        dim: true
        focus: true
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        width: Math.min(root.width - 40, 370)
        height: 380
        padding: 16
        background: Rectangle { color: "#333333"; radius: 10 }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            RowLayout {
                Label {
                    text: fuelTracker.strings["restoreBackup"]
                    color: "#ffffff"
                    font.pixelSize: 16
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                ToolButton {
                    text: "✕"
                    onClicked: restorePopup.close()
                    contentItem: Label {
                        text: "✕"
                        color: "#ffffff"
                        font.pixelSize: 16
                    }
                }
            }

            Label {
                text: fuelTracker.strings["noBackups"]
                color: "#999999"
                visible: root.backups.length === 0
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.backups
                delegate: Button {
                    width: ListView.view.width
                    Layout.preferredHeight: 44
                    text: modelData.split('/').pop()
                    font.pixelSize: 13
                    onClicked: {
                        root.pendingBackupPath = modelData
                        restorePopup.close()
                        confirmRestorePopup.open()
                    }
                }
                ScrollBar.vertical: ScrollBar {}
            }
        }
    }

    // Endgültige Bestätigung einer Wiederherstellung
    Popup {
        id: confirmRestorePopup
        modal: true
        dim: true
        focus: true
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        width: Math.min(root.width - 40, 360)
        height: 180
        padding: 16
        background: Rectangle { color: "#333333"; radius: 10 }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Label {
                text: fuelTracker.strings["confirmRestore"] + "\n\n" + root.pendingBackupPath.split('/').pop()
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
                    onClicked: confirmRestorePopup.close()
                }
                Button {
                    text: fuelTracker.strings["yes"]
                    highlighted: true
                    onClicked: {
                        confirmRestorePopup.close()
                        if (fuelTracker.restoreBackup(root.pendingBackupPath)) {
                            root.showMsg(fuelTracker.strings["restoreDone"])
                        } else {
                            root.showMsg(fuelTracker.strings["backupFailed"])
                        }
                    }
                }
            }
        }
    }

    // Kurzer Hinweis (Toast)
    Popup {
        id: messagePopup
        modal: true
        dim: true
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        width: root.width - 80
        height: 110
        padding: 12
        closePolicy: Popup.NoAutoClose
        background: Rectangle {
            color: "#222222"
            radius: 8
            border.color: "#555555"
            border.width: 1
        }
        Label {
            id: msgLabel
            anchors.fill: parent
            anchors.margins: 10
            text: ""
            color: "#ffffff"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        Timer {
            id: msgTimer
            interval: 2600
            onTriggered: messagePopup.close()
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
                        indicator: Rectangle {
                            implicitWidth: 48
                            implicitHeight: 28
                            x: fullSwitch.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: 14
                            color: fullSwitch.checked ? root.accent : "#555555"
                            border.color: fullSwitch.checked ? root.accent : "#666666"
                            border.width: 1
                            Rectangle {
                                x: fullSwitch.checked ? parent.width - width - 3 : 3
                                y: 3
                                width: 22
                                height: 22
                                radius: 11
                                color: fullSwitch.checked ? "#ffffff" : "#aaaaaa"
                                Behavior on x { NumberAnimation { duration: 120 } }
                            }
                        }
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: fuelTracker.strings["saveEntry"]
                    background: Rectangle {
                        color: root.accent
                        radius: 6
                    }
                    contentItem: Label {
                        text: fuelTracker.strings["saveEntry"]
                        color: root.accentText
                        font.pixelSize: 15
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
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
            onClicked: confirmClearPopup.open()
        }
    }
}