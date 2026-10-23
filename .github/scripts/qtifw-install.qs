Controller.prototype.WelcomePageCallback = function() {
    gui.clickButton(buttons.NextButton);
}
Controller.prototype.CredentialsPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}
Controller.prototype.ComponentSelectionPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}
Controller.prototype.TargetDirectoryPageCallback = function() {
    gui.currentPageWidget().TargetDirectoryLineEdit.text = "C:/Qt/QtIFW-4.6.0";
    gui.clickButton(buttons.NextButton);
}
Controller.prototype.LicenseAgreementPageCallback = function() {
    gui.currentPageWidget().AcceptLicenseRadioButton.checked = true;
    gui.clickButton(buttons.NextButton);
}
Controller.prototype.ReadyForInstallationPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}
Controller.prototype.FinishedPageCallback = function() {
    gui.clickButton(buttons.FinishButton);
}