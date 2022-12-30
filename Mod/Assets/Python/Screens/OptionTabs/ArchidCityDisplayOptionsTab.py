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

		left, right = self.addTwoColumnLayout(screen, column, "Page", True)

		self.createBuildingActualEffectsPanel(screen, left)
		self.addSpacer(screen, left, "CityScreen1")
		self.createMiscellaneousPanel(screen, left)
		self.addSpacer(screen, left, "CityScreen2")
		self.createGreatPersonBarPanel(screen, left)
		self.createCityBarPanel(screen, right)


	def createBuildingActualEffectsPanel(self, screen, panel):
		self.addLabel(screen, panel, "BuildingEffects", "Building Actual Effects in Hovers:")
		left, right = self.addTwoColumnLayout(screen, panel, "BuildingEffects", False)
		self.addCheckbox(screen, left, "MiscHover__BuildingActualEffects")
		self.addCheckbox(screen, left, "MiscHover__BuildingAdditionalFood")
		self.addCheckbox(screen, left, "MiscHover__BuildingAdditionalProduction")
		self.addCheckbox(screen, left, "MiscHover__BuildingAdditionalCommerce")
		self.addCheckbox(screen, left, "MiscHover__BuildingSavedMaintenance")
		self.addSpacer(screen, right, "CityScreen2a")
		self.addCheckbox(screen, right, "MiscHover__BuildingAdditionalHealth")
		self.addCheckbox(screen, right, "MiscHover__BuildingAdditionalHappiness")
		self.addCheckbox(screen, right, "MiscHover__BuildingAdditionalGreatPeople")
		self.addCheckbox(screen, right, "MiscHover__BuildingAdditionalDefence")

	def createCityBarPanel(self, screen, panel):
		self.addLabel(screen, panel, "CitybarHover", "City Bar Hover:")
		left, right = self.addTwoColumnLayout(screen, panel, "CityBarHover", False)

		self.addCheckbox(screen, left, "CityBar__BaseValues")
		self.addCheckbox(screen, left, "CityBar__Health")
		self.addCheckbox(screen, left, "CityBar__Happiness")
		self.addCheckbox(screen, left, "CityBar__FoodAssist")
		self.addCheckbox(screen, left, "CityBar__BaseProduction")
		self.addCheckbox(screen, left, "CityBar__TradeDetail")
		self.addCheckbox(screen, left, "CityBar__Commerce")
		self.addCheckbox(screen, left, "CityBar__CultureTurns")
		self.addCheckbox(screen, left, "CityBar__GreatPersonTurns")

		self.addLabel(screen, right, "Cityanger", "City Anger:")
		self.addCheckbox(screen, right, "CityBar__HurryAnger")
		self.addCheckbox(screen, right, "CityBar__DraftAnger")

		self.addSpacer(screen, right, "CityScreen5")
		self.addCheckbox(screen, right, "CityBar__BuildingActualEffects")
		self.addCheckbox(screen, right, "CityBar__BuildingIcons")
		self.addCheckbox(screen, right, "CityBar__Specialists")
		self.addCheckbox(screen, right, "CityBar__RevoltChance")
		self.addCheckbox(screen, right, "CityBar__HideInstructions")

	def createGreatPersonBarPanel(self, screen, panel):
		self.addLabel(screen, panel, "GreatPersonBar", "Great Person Bar:")
		self.addCheckbox(screen, panel, "MiscHover__GreatPeopleRateBreakdown")

	def createMiscellaneousPanel(self, screen, panel):
		self.addLabel(screen, panel, "Misc", "Miscellaneous:")
		self.addCheckbox(screen, panel, "MiscHover__BaseCommerce")
		self.addCheckbox(screen, panel, "MiscHover__SpecialistActualEffects")
