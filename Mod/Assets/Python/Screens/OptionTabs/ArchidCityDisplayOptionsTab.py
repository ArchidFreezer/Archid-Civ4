## BugGeneralOptionsTab
##
## Tab for the BUG Advanced Combat Odds Options.
##
## Copyright (c) 2007-2008 The BUG Mod.
##
## Author: EmperorFool

import BugOptionsTab
import BugUtil

class ArchidCityDisplayOptionsTab(BugOptionsTab.BugOptionsTab):
	"Archid City Display Options Screen Tab"
	
	def __init__(self, screen):
		BugOptionsTab.BugOptionsTab.__init__(self, "CityDisplayTab", BugUtil.getPlainText("TXT_KEY_ARCHID_OPTTAB_CITYDISPLAY", "City Display"))

	def create(self, screen):
		tab = self.createTab(screen)
		panel = self.createMainPanel(screen)
		column = self.addOneColumnLayout(screen, panel)

		self.createCityBarHoverPanel(screen, column)
		
	def createCityBarHoverPanel(self, screen, panel):
		self.addLabel(screen, panel, "CitybarHover", "City Bar Hover:")
		left, right = self.addTwoColumnLayout(screen, panel, panel, False)

		self.addCheckbox(screen, left, "CityBar__BaseProduction")
		self.addCheckbox(screen, left, "CityBar__BaseValues")
		self.addCheckbox(screen, left, "CityBar__BuildingIcons")
		self.addCheckbox(screen, left, "CityBar__Commerce")
		self.addCheckbox(screen, left, "CityBar__CultureTurns")
		self.addCheckbox(screen, left, "CityBar__DraftAnger")
		self.addCheckbox(screen, left, "CityBar__FoodAssist")
		self.addCheckbox(screen, left, "CityBar__GreatPersonTurns")
		self.addCheckbox(screen, left, "CityBar__Happiness")
		self.addCheckbox(screen, right, "CityBar__Health")
		self.addCheckbox(screen, right, "CityBar__HideInstructions")
		self.addCheckbox(screen, right, "CityBar__HurryAnger")
		self.addCheckbox(screen, right, "CityBar__HurryAssist")
		self.addCheckbox(screen, right, "CityBar__HurryAssistIncludeCurrent")
		self.addCheckbox(screen, right, "CityBar__RevoltChance")
		self.addCheckbox(screen, right, "CityBar__Specialists")
		self.addCheckbox(screen, right, "CityBar__TradeDetail")
