import QtQuick 2.9
import QtQuick.Controls 2.2
import "TokyoNightTheme.js" as TokyoNight

Column {
    property alias text: stageLabel.text
    property alias running: stageSpinner.running
    property string warningText: ""

    width: Math.max(0, Math.min(420, parent.width - 48))
    spacing: 12

    onImplicitHeightChanged: {
        var host = ApplicationWindow.window
        if (host && host.directStream)
            host.launchHeight = Math.max(180, implicitHeight + 48)
    }

    BusyIndicator {
        id: stageSpinner
        width: 24
        height: 24
        anchors.horizontalCenter: parent.horizontalCenter
    }

    Label {
        id: stageLabel
        width: parent.width
        font.pixelSize: 14
        color: TokyoNight.text
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
    }

    Label {
        width: parent.width
        visible: text.length > 0
        text: warningText
        font.pixelSize: 12
        color: TokyoNight.accent
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
    }
}
