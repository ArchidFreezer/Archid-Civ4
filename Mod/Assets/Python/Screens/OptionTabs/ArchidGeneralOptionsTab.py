## ArchidGeneralOptionsTab
##
## Tab for the BUG General Options (Main and City Screens).
##

import BugOptionsTab

class ArchidGeneralOptionsTab(BugOptionsTab.BugOptionsTab):
	"Archid General Options Screen Tab"

	def __init__(self, screen):
		BugOptionsTab.BugOptionsTab.__init__(self, "General", "General")

	def create(self, screen):
		tab = self.createTab(screen)
		panel = self.createMainPanel(screen)
		column = self.addOneColumnLayout(screen, panel)

		self.createActionsPanel(screen, column)

	def createActionsPanel(self, screen, panel):
		self.addLabel(screen, panel, "Actions", "Actions:")
		self.addCheckbox(screen, panel, "Actions__SimpleSelectionMode")
		self.addCheckbox(screen, panel, "Actions__RapidUnitCycling")
		self.addCheckbox(screen, panel, "Actions__SentryHealing")
		self.addCheckbox(screen, panel, "Actions__SentryHealingOnlyNeutral", True)
		self.addCheckbox(screen, panel, "Actions__PreChopForests")
		self.addCheckbox(screen, panel, "Actions__PreChopImprovements")
		self.addCheckbox(screen, panel, "Actions__IgnoreHarmlessBarbarians")
