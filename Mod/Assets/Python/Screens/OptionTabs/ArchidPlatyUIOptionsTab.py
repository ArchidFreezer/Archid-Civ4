## BugGeneralOptionsTab
##
## Tab for the BUG Advanced Combat Odds Options.
##
## Copyright (c) 2007-2008 The BUG Mod.
##
## Author: EmperorFool

import BugOptionsTab

class ArchidPlatyUIOptionsTab(BugOptionsTab.BugOptionsTab):
	"Archid Platy UI Options Screen Tab"

	def __init__(self, screen):
		BugOptionsTab.BugOptionsTab.__init__(self, "PlatyUI", "Platy UI")

	def create(self, screen):
		tab = self.createTab(screen)
		panel = self.createMainPanel(screen)
		column = self.addOneColumnLayout(screen, panel)

		self.addCheckbox(screen, column, "PlatyUI__GPBar")
		self.addCheckbox(screen, column, "PlatyUI__Colours")
		self.addCheckbox(screen, column, "PlatyUI__Panels")
		self.addCheckbox(screen, column, "PlatyUI__Build")
		self.addCheckbox(screen, column, "PlatyUI__Movie")
		self.addSpacer(screen, column, "PlatyUI_Tab1")
		leftL, leftR =  self.addTwoColumnLayout(screen, column, "PlatyUI")
		self.addTextDropdown(screen, leftL, leftR, "PlatyUI__Background")
