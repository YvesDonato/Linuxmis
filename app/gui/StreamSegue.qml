import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Window 2.2

import SdlGamepadKeyNavigation 1.0
import Session 1.0

Item {
    property Session session
    property string appName
    property string stageText : isResume ? qsTr("Resuming %1...").arg(appName) :
                                           qsTr("Starting %1...").arg(appName)
    property bool isResume : false
    property bool quitAfter : false

    function stageStarting(stage)
    {
        // Update the spinner text
        stageText = qsTr("Starting %1...").arg(stage)
    }

    function stageFailed(stage, errorCode, failingPorts)
    {
        // Display the error dialog after Session::exec() returns
        streamSegueErrorDialog.text = qsTr("Starting %1 failed: Error %2").arg(stage).arg(errorCode)

        if (failingPorts) {
            streamSegueErrorDialog.text += "\n\n" + qsTr("Check your firewall and port forwarding rules for port(s): %1").arg(failingPorts)
        }
    }

    function connectionStarted()
    {
        // Hide the UI contents so the user doesn't
        // see them briefly when we pop off the StackView
        status.visible = false

        // Hide the window now that streaming has begun
        window.visible = false
    }

    function displayLaunchError(text)
    {
        // Display the error dialog after Session::exec() returns
        streamSegueErrorDialog.text = text
        console.error(text)
    }

    function displayLaunchWarning(text)
    {
        status.warningText += (status.warningText ? "\n" : "") + text
        console.warn(text)
    }

    function quitStarting()
    {
        // Avoid the push transition animation
        var component = Qt.createComponent("QuitSegue.qml")
        stackView.replace(stackView.currentItem, component.createObject(stackView, {"appName": appName}), StackView.Immediate)

        // Show the Qt window again to show quit segue
        window.visible = true
    }

    function sessionFinished(portTestResult)
    {
        if (portTestResult !== 0 && portTestResult !== -1 && streamSegueErrorDialog.text) {
            streamSegueErrorDialog.text += "\n\n" + qsTr("This PC's Internet connection is blocking Moonlight. Streaming over the Internet may not work while connected to this network.")
        }

        // Re-enable GUI gamepad usage now
        if (!quitAfter)
            SdlGamepadKeyNavigation.enable()

        if (quitAfter) {
            if (streamSegueErrorDialog.text) {
                // Quit when the error dialog is acknowledged
                streamSegueErrorDialog.quitAfter = quitAfter
                streamSegueErrorDialog.open()
            }
            else {
                // Quit immediately
                Qt.quit()
            }
        } else {
            // Exit this view
            stackView.pop()

            // Show the Qt window again after streaming
            window.visible = true

            // Display any launch errors. We do this after
            // the Qt UI is visible again to prevent losing
            // focus on the dialog which would impact gamepad
            // users.
            if (streamSegueErrorDialog.text) {
                streamSegueErrorDialog.quitAfter = quitAfter
                streamSegueErrorDialog.open()
            }
        }
    }

    function sessionReadyForDeletion()
    {
        // Garbage collect the Session object since it's pretty heavyweight
        // and keeps other libraries (like SDL_TTF) around until it is deleted.
        session = null
        gc()
    }

    StackView.onDeactivating: {
        // Show the toolbar again when popped off the stack
        toolBar.visible = true

        // Re-enable GUI gamepad usage now
        if (!quitAfter)
            SdlGamepadKeyNavigation.enable()
    }

    StackView.onActivated: {
        // Hide the toolbar before we start loading
        toolBar.visible = false

        // Hook up our signals
        session.stageStarting.connect(stageStarting)
        session.stageFailed.connect(stageFailed)
        session.connectionStarted.connect(connectionStarted)
        session.displayLaunchError.connect(displayLaunchError)
        session.displayLaunchWarning.connect(displayLaunchWarning)
        session.quitStarting.connect(quitStarting)
        session.sessionFinished.connect(sessionFinished)
        session.readyForDeletion.connect(sessionReadyForDeletion)

        // Let StackView finish activation before Session takes over the loop.
        Qt.callLater(function() {
            if (!session || StackView.status !== StackView.Active)
                return

            // Stop GUI gamepad usage now
            SdlGamepadKeyNavigation.disable()

            // Garbage collect QML stuff before we start streaming,
            // since we'll probably be streaming for a while and we
            // won't be able to GC during the stream.
            gc()

            // Run the streaming session to completion
            session.exec(window)
        })
    }

    LaunchStatus {
        id: status
        anchors.centerIn: parent
        text: stageText
    }
}
