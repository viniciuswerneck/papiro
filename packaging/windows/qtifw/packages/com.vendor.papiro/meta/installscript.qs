function Component() {
  component.createOperations();
  component.addOperation("CreateShortcut", 
    "@TargetDir@/Papiro.exe", 
    "@StartMenuDir@/Papiro.lnk",
    "", 
    "@TargetDir@/Papiro.exe", 
    0);
  component.addOperation("CreateShortcut", 
    "@TargetDir@/Papiro.exe", 
    "@DesktopDir@/Papiro.lnk",
    "", 
    "@TargetDir@/Papiro.exe", 
    0);
}