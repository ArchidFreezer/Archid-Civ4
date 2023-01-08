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

		left, right = self.addTwoColumnLayout(screen, column, "Page", True)

		self.createActionsPanel(screen, left)
		self.addSpacer(screen, left, "GeneralScreen1")
		self.createAutomationsPanel(screen, left)
		self.createAutoPlayPanel(screen, right)

	def createActionsPanel(self, screen, panel):
		self.addLabel(screen, panel, "Actions", "Actions:")
		self.addCheckbox(screen, panel, "Actions__SimpleSelectionMode")
		self.addCheckbox(screen, panel, "Actions__RapidUnitCycling")
		self.addCheckbox(screen, panel, "Actions__SentryHealing")
		self.addCheckbox(screen, panel, "Actions__SentryHealingOnlyNeutral", True)
		self.addCheckbox(screen, panel, "Actions__PreChopForests")
		self.addCheckbox(screen, panel, "Actions__PreChopImprovements")
		self.addCheckbox(screen, panel, "Actions__IgnoreHarmlessBarbarians")

	def createAutomationsPanel(self, screen, panel):
		self.addLabel(screen, panel, "Automations", "Automations:")
		self.addCheckbox(screen, panel, "Automations__PillageBarbarians")
		self.addCheckbox(screen, panel, "Automations__PillageIgnoreDanger")
		self.addCheckbox(screen, panel, "Automations__StayInBorders")

	def createAutoPlayPanel(self, screen, panel):
		self.addLabel(screen, panel, "AIAutoPlay", "AI Auto Play:")
		self.addCheckbox(screen, panel, "AIAutoPlay__Debug")
		self.addCheckbox(screen, panel, "AIAutoPlay__BlockPopups")
		self.addCheckbox(screen, panel, "AIAutoPlay__Refortify")
		self.addCheckbox(screen, panel, "AIAutoPlay__SaveAllDeaths")
		self.addTextEdit(screen, panel, panel, "AIAutoPlay__DefaultTurnsToAuto")
		self.addSpacer(screen, panel, "AIAutoPlay_Tab1")
		self.addLabel(screen, panel, "ChangePlayer", "Change Player:")
		self.addCheckbox(screen, panel, "ChangePlayer__Debug")
