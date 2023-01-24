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
		
		self.createTopPanel(screen, column)
		self.addSpacer(screen, column, "PlatyUI_Tab1")
		self.createFooterPanel(screen, column)

	def createTopPanel(self, screen, panel):
		left, right = self.addTwoColumnLayout(screen, panel, "Header", True)
		self.addCheckbox(screen, left, "PlatyUI__Colours")
		self.addCheckbox(screen, left, "PlatyUI__Panels")
		self.addCheckbox(screen, left, "PlatyUI__Build")
		self.addCheckbox(screen, left, "PlatyUI__Movie")
		self.addTextEdit(screen, right, right, "PlatyUI__GPBar")
		self.addTextEdit(screen, right, right, "PlatyUI__GGBar")
		self.addTextEdit(screen, right, right, "PlatyUI__BLBar")
		
	def createFooterPanel(self, screen, panel):
		footerL, footerR =  self.addTwoColumnLayout(screen, panel, "PlatyUI")
		self.addTextDropdown(screen, footerL, footerR, "PlatyUI__Background")
