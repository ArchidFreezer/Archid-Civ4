## BugErrorOptionsTab
##
## Tab for the BUG Error Tracker.
##
## TODO:
##  * Display all config errors
##
## Copyright (c) 2007-2008 The BUG Mod.
##
## Author: EmperorFool

from CvPythonExtensions import *
import BugOptionsTab
import BugUtil

## Globals

gc = CyGlobalContext()

class ArchidErrorOptionsTab(BugOptionsTab.BugOptionsTab):
	"Archid Error Options Screen Tab -- Displayed only when the INI file isn't found."
	
	def __init__(self, screen):
		BugOptionsTab.BugOptionsTab.__init__(self, "ErrorTab", BugUtil.getPlainText("TXT_KEY_ARCHID_OPTTAB_ERROR", "Error"))

	def create(self, screen):
		tab = self.createTab(screen)
		panel = self.createMainPanel(screen, True)
		column = self.addOneColumnLayout(screen, panel)
		
		self.addLabel(screen, column, "FileNotFound", "File Not Found:")
		self.addLabel(screen, column, "IniFilename", "\"" + BugPath.getModName() + ".ini\"")
		
		self.addLabel(screen, column, "DataDirectory", "Data Directory:")
		self.addLabel(screen, column, "DataPath", BugPath.getDataDir())
