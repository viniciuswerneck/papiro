Component.prototype.createOperations = function() {
    component.createOperations();
    
    if (systemInfo.productType === "windows") {
        component.addOperation("CreateShortcut",
            "@TargetDir@/bin/Papiro.exe",
            "@StartMenuDir@/Papiro.lnk",
            "", "", "", "@TargetDir@/bin/Papiro.exe", "0");
        
        component.addOperation("CreateShortcut",
            "@TargetDir@/bin/Papiro.exe",
            "@DesktopDir@/Papiro.lnk",
            "", "", "", "@TargetDir@/bin/Papiro.exe", "0");
    }
}