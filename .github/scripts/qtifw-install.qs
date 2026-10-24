function Controller() {
    installer.autoRejectMessageBoxes();
    installer.installationFinished.connect(function() {
        gui.clickButton(buttons.FinishButton);
    });
}

Controller.prototype.WelcomePageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.CredentialsPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.ComponentSelectionPageCallback = function() {
    var widget = gui.currentPageWidget();
    widget.deselectAll();
    widget.selectComponent("Qt Installer Framework");
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.TargetDirectoryPageCallback = function() {
    var widget = gui.currentPageWidget();
    widget.TargetDirectoryLineEdit.setText("C:\\Qt\\QtIFW-4.6");
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.LicenseAgreementPageCallback = function() {
    var widget = gui.currentPageWidget();
    widget.AcceptLicenseRadioButton.setChecked(true);
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.ReadyForInstallationPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.FinishedPageCallback = function() {
    gui.clickButton(buttons.FinishButton);
}