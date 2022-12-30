## Sid Meier's Civilization 4
## Copyright Firaxis Games 2007

from CvPythonExtensions import *
import CvEventManager
import CvUtil
import BugUtil

import WBCityEditScreen
import WBUnitScreen
import WBPlayerScreen
import WBGameDataScreen
import WBPlotScreen
import CvPlatyBuilderScreen
import CvAdvisorUtils

import Forgetful
import TimeKeeper
import MirrorImage
import ArtDefChecker

# globals

PLATY_UI_WB_PLAYER_SCRIPT_EVENT_ID = CvUtil.getNewEventID("PlatyUI.WBPlayerScript")
PLATY_UI_WB_CITY_SCRIPT_EVENT_ID = CvUtil.getNewEventID("PlatyUI.WBCityScript")
PLATY_UI_WB_UNIT_SCRIPT_EVENT_ID = CvUtil.getNewEventID("PlatyUI.WBUnitScript")
PLATY_UI_WB_GAME_SCRIPT_EVENT_ID = CvUtil.getNewEventID("PlatyUI.WBGameScript")
PLATY_UI_WB_PLOT_SCRIPT_EVENT_ID = CvUtil.getNewEventID("PlatyUI.WBPlotScript")

gc = CyGlobalContext()

### GameUtils handler ###
def getWidgetHelp(argsList):
	eWidgetType, iData1, iData2, bOption = argsList
## Commerce Flexibles ##
	if eWidgetType == WidgetTypes.WIDGET_CHANGE_PERCENT:
		if iData2 > 0:
			return CyTranslator().getText("TXT_KEY_COMMERCE_ADJUSTMENT", (100,))
		else:
			return CyTranslator().getText("TXT_KEY_COMMERCE_ADJUSTMENT", (0,))
## Train XP ##
	elif eWidgetType == WidgetTypes.WIDGET_TRAIN:
		pCity = CyInterface().getHeadSelectedCity()
		if pCity:
			iUnitType = gc.getCivilizationInfo(pCity.getCivilizationType()).getCivilizationUnits(iData1)
			if gc.getUnitInfo(iUnitType).getUnitCombatType() > -1:
				sXPText = CyTranslator().getText("[COLOR_HIGHLIGHT_TEXT] \nXP:", ()) + str(pCity.getProductionExperience(iUnitType))
				return sXPText				
## Hurry Overflow ##
	elif eWidgetType == WidgetTypes.WIDGET_HURRY:
		pCity = CyInterface().getHeadSelectedCity()
		if pCity.canHurry(iData1, True):
			iProductionPerPop = gc.getHurryInfo(iData1).getProductionPerPopulation()
			if iProductionPerPop > 0:
				iProductionNeeded = pCity.getProductionNeeded() - pCity.getProduction()
				iHurryCostModifier = 0
				if pCity.getProductionBuilding() > -1:
					iHurryCostModifier = gc.getBuildingInfo(pCity.getProductionBuilding()).getHurryCostModifier()
				elif pCity.getProductionUnit() > -1:
					iHurryCostModifier = gc.getUnitInfo(pCity.getProductionUnit()).getHurryCostModifier()
				iProductionPerPop = iProductionPerPop * 100 / (iHurryCostModifier + 100)
				iHurryModifier = gc.getPlayer(pCity.getOwner()).getHurryModifier()
				iPopulationNeeded = (iProductionNeeded * (100 + iHurryModifier) + (iProductionPerPop * 100) - 1)  / (iProductionPerPop * 100)
				iOverflow = iPopulationNeeded * iProductionPerPop * 100 / (100 + iHurryModifier) - iProductionNeeded
				iOverflow = min(iOverflow, pCity.getProductionNeeded())
				return CyTranslator().getText("TXT_KEY_HURRY_OVERFLOW", (iOverflow,))
## Happiness Timers ##
	elif eWidgetType == WidgetTypes.WIDGET_HELP_HAPPINESS:
		sText = "\n======================="
		iTimer = CyInterface().getHeadSelectedCity().getHurryAngerTimer()
		if iTimer > 0:
			sText += CyTranslator().getText("TXT_KEY_HURRY_TIMER", (iTimer,))
		iTimer = CyInterface().getHeadSelectedCity().getConscriptAngerTimer()
		if iTimer > 0:
			sText += CyTranslator().getText("TXT_KEY_CONSCRIPT_TIMER", (iTimer,))
		iTimer = CyInterface().getHeadSelectedCity().getDefyResolutionAngerTimer()
		if iTimer > 0:
			sText += CyTranslator().getText("TXT_KEY_RESOLUTION_TIMER", (iTimer,))
		return sText
## Religion Screen ##
	elif eWidgetType == WidgetTypes.WIDGET_HELP_RELIGION:
		if iData1 == -1:
			return CyTranslator().getText("TXT_KEY_CULTURELEVEL_NONE", ())
## Platy WorldBuilder ##
	elif eWidgetType == WidgetTypes.WIDGET_PYTHON:
		if iData1 == 1027:
			return CyTranslator().getText("TXT_KEY_WB_PLOT_DATA",())
		elif iData1 == 1028:
			return gc.getGameOptionInfo(iData2).getHelp()
		elif iData1 == 1029:
			if iData2 == 0:
				sText = CyTranslator().getText("TXT_KEY_WB_PYTHON", ())
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onFirstContact"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onChangeWar"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onVassalState"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onCityAcquired"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onCityBuilt"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onCultureExpansion"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onGoldenAge"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onEndGoldenAge"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onGreatPersonBorn"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onPlayerChangeStateReligion"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onReligionFounded"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onReligionSpread"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onReligionRemove"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onCorporationFounded"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onCorporationSpread"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onCorporationRemove"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onUnitCreated"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onUnitLost"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onUnitPromoted"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onBuildingBuilt"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onProjectBuilt"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onTechAcquired"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onImprovementBuilt"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onImprovementDestroyed"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onRouteBuilt"
				sText += "\n" + CyTranslator().getText("[ICON_BULLET]", ()) + "onPlotRevealed"
				return sText
			elif iData2 == 1:
				return CyTranslator().getText("TXT_KEY_WB_PLAYER_DATA",())
			elif iData2 == 2:
				return CyTranslator().getText("TXT_KEY_WB_TEAM_DATA",())
			elif iData2 == 3:
				return CyTranslator().getText("TXT_KEY_PEDIA_CATEGORY_TECH",())
			elif iData2 == 4:
				return CyTranslator().getText("TXT_KEY_PEDIA_CATEGORY_PROJECT",())
			elif iData2 == 5:
				return CyTranslator().getText("TXT_KEY_PEDIA_CATEGORY_UNIT", ()) + " + " + CyTranslator().getText("TXT_KEY_CONCEPT_CITIES", ())
			elif iData2 == 6:
				return CyTranslator().getText("TXT_KEY_PEDIA_CATEGORY_PROMOTION",())
			elif iData2 == 7:
				return CyTranslator().getText("TXT_KEY_WB_CITY_DATA2",())
			elif iData2 == 8:
				return CyTranslator().getText("TXT_KEY_PEDIA_CATEGORY_BUILDING",())
			elif iData2 == 9:
				return "Platy Builder\nVersion: 4.10"
			elif iData2 == 10:
				return CyTranslator().getText("TXT_KEY_CONCEPT_EVENTS",())
			elif iData2 == 11:
				return CyTranslator().getText("TXT_KEY_WB_RIVER_PLACEMENT",())
			elif iData2 == 12:
				return CyTranslator().getText("TXT_KEY_PEDIA_CATEGORY_IMPROVEMENT",())
			elif iData2 == 13:
				return CyTranslator().getText("TXT_KEY_PEDIA_CATEGORY_BONUS",())
			elif iData2 == 14:
				return CyTranslator().getText("TXT_KEY_WB_PLOT_TYPE",())
			elif iData2 == 15:
				return CyTranslator().getText("TXT_KEY_CONCEPT_TERRAIN",())
			elif iData2 == 16:
				return CyTranslator().getText("TXT_KEY_PEDIA_CATEGORY_ROUTE",())
			elif iData2 == 17:
				return CyTranslator().getText("TXT_KEY_PEDIA_CATEGORY_FEATURE",())
			elif iData2 == 18:
				return CyTranslator().getText("TXT_KEY_MISSION_BUILD_CITY",())
			elif iData2 == 19:
				return CyTranslator().getText("TXT_KEY_WB_ADD_BUILDINGS",())
			elif iData2 == 20:
				return CyTranslator().getText("TXT_KEY_PEDIA_CATEGORY_RELIGION",())
			elif iData2 == 21:
				return CyTranslator().getText("TXT_KEY_CONCEPT_CORPORATIONS",())
			elif iData2 == 22:
				return CyTranslator().getText("TXT_KEY_ESPIONAGE_CULTURE",())
			elif iData2 == 23:
				return CyTranslator().getText("TXT_KEY_PITBOSS_GAME_OPTIONS",())
			elif iData2 == 24:
				return CyTranslator().getText("TXT_KEY_WB_SENSIBILITY",())
			elif iData2 == 27:
				return CyTranslator().getText("TXT_KEY_WB_ADD_UNITS",())
			elif iData2 == 28:
				return CyTranslator().getText("TXT_KEY_WB_TERRITORY",())
			elif iData2 == 29:
				return CyTranslator().getText("TXT_KEY_WB_ERASE_ALL_PLOTS",())
			elif iData2 == 30:
				return CyTranslator().getText("TXT_KEY_WB_REPEATABLE",())
			elif iData2 == 31:
				return CyTranslator().getText("TXT_KEY_PEDIA_HIDE_INACTIVE", ())
			elif iData2 == 32:
				return CyTranslator().getText("TXT_KEY_WB_STARTING_PLOT", ())
			elif iData2 == 33:
				return CyTranslator().getText("TXT_KEY_INFO_SCREEN", ())
			elif iData2 == 34:
				return CyTranslator().getText("TXT_KEY_CONCEPT_TRADE", ())
		elif iData1 > 1029 and iData1 < 1040:
			return "+/-"
## War Attitude Info ##
		elif iData1 == 3838:
			LeaderInfo = gc.getLeaderHeadInfo(iData2)
			sText = LeaderInfo.getDescription()
			sText += CyTranslator().getText("TXT_KEY_NO_WAR_PROBABILITY", ())
			for i in xrange(AttitudeTypes.NUM_ATTITUDE_TYPES):
				sText += u"\n%c%s: %d%%" %(CyGame().getSymbolID(FontSymbols.BULLET_CHAR), gc.getAttitudeInfo(i).getDescription(), LeaderInfo.getNoWarAttitudeProb(i))
			sAttitude = gc.getAttitudeInfo(LeaderInfo.getDeclareWarRefuseAttitudeThreshold()).getDescription()
			sText += CyTranslator().getText("TXT_KEY_DECLARE_WAR_THRESHOLD", (sAttitude,))
			sAttitude = gc.getAttitudeInfo(LeaderInfo.getDeclareWarThemRefuseAttitudeThreshold()).getDescription()
			sText += CyTranslator().getText("TXT_KEY_DECLARE_WAR_THEM_THRESHOLD", (sAttitude,))
			return sText
## Platypedia ##
		elif iData1 == 6780:
			return gc.getFeatureInfo(iData2).getDescription()
		elif iData1 == 6781:
			if iData2 == -2:
				return CyTranslator().getText("TXT_KEY_PEDIA_ALL_GROUPS", ())
			elif iData2 == -1:
				return CyTranslator().getText("TXT_PEDIA_NON_COMBAT", ())
			else:
				return gc.getUnitCombatInfo(iData2).getDescription()
		elif iData1 == 6782:
			return CyGameTextMgr().parseCorporationInfo(iData2, False)
		elif iData1 == 6783:
			return CyTranslator().getText("TXT_KEY_MISC_RIVERS", ())
		elif iData1 == 6785:
			return CyGameTextMgr().getProjectHelp(iData2, False, CyCity())
		elif iData1 == 6786:
			return gc.getVictoryInfo(iData2).getDescription()
		elif iData1 == 6787:
			return gc.getProcessInfo(iData2).getDescription()
		elif iData1 == 6788:
			if iData2 == -1:
				return CyTranslator().getText("TXT_KEY_CULTURELEVEL_NONE", ())
			return gc.getRouteInfo(iData2).getDescription()
		elif iData1 == 6789:
			return gc.getTraitInfo(iData2).getDescription()
		elif iData1 == 6791:
			return gc.getCultureLevelInfo(iData2).getDescription()
		elif iData1 == 6792:
			return gc.getGameSpeedInfo(iData2).getDescription()
		elif iData1 == 6793:
			return gc.getHandicapInfo(iData2).getDescription()
		elif iData1 == 6795:
			return gc.getEraInfo(iData2).getDescription()
		elif iData1 == 6796:
			if iData2 == 999:
				return CyTranslator().getText("TXT_KEY_CIVICS_SCREEN_NO_UPKEEP", ())
			return gc.getUpkeepInfo(iData2).getDescription()
		elif iData1 == 6797:
			return gc.getWorldInfo(iData2).getDescription()
## Camera City Zoom In Distance ##
		elif iData1 == 6999:
			iZoom = int(gc.getDefineFLOAT("CAMERA_CITY_ZOOM_IN_DISTANCE"))
			sText = CyTranslator().getText("TXT_KEY_CAMERA_CITY_ZOOM_IN_DISTANCE", ()) + ": " + str(iZoom) + "\n"
			sText += CyTranslator().getText("TXT_KEY_ZOOM", ())
			return sText
## Field of View ##
		elif iData1 == 7000:
			iZoom = int(gc.getDefineFLOAT("FIELD_OF_VIEW"))
			sText = CyTranslator().getText("TXT_KEY_FIELD_OF_VIEW", ()) + ": " + str(iZoom) + "\n"
			sText += CyTranslator().getText("TXT_KEY_ZOOM", ())
			return sText
## Advisors ##
		elif iData1 == 7001:
			if iData2 == 0:
				return CyTranslator().getText("TXT_KEY_ADVISOR_MILITARY", ())
			elif iData2 == 1:
				return CyTranslator().getText("TXT_KEY_ADVISOR_RELIGION", ())
			elif iData2 == 2:
				return CyTranslator().getText("TXT_KEY_ADVISOR_ECONOMY", ())
			elif iData2 == 3:
				return CyTranslator().getText("TXT_KEY_ADVISOR_SCIENCE", ())
			elif iData2 == 4:
				return CyTranslator().getText("TXT_KEY_ADVISOR_CULTURE", ())
			elif iData2 == 5:
				return CyTranslator().getText("TXT_KEY_ADVISOR_GROWTH", ())
			elif iData2 == -1:
				return CyTranslator().getText("TXT_KEY_PREREQ", ())
## Full Order List ##
		elif iData1 == 7002:
			return CyTranslator().getText("TXT_KEY_ORDER_LIST", ())
## World Tracker ##
		elif iData1 == 7100:
			return CyTranslator().getText("TXT_KEY_WORLD_TRACKER",())
## Platy Options ##
		elif iData1 == 7101:
			return CyTranslator().getText("TXT_KEY_PLATY_OPTIONS",())
## City Hover Text ##
		elif iData1 > 7199 and iData1 < 7300:
			iPlayer = iData1 - 7200
			pPlayer = gc.getPlayer(iPlayer)
			pCity = pPlayer.getCity(iData2)
			if CyGame().GetWorldBuilderMode():
				sText = "<font=3>"
				if pCity.isCapital():
					sText += CyTranslator().getText("[ICON_STAR]", ())
				elif pCity.isGovernmentCenter():
					sText += CyTranslator().getText("[ICON_SILVER_STAR]", ())
				sText += u"%s: %d<font=2>" %(pCity.getName(), pCity.getPopulation())
				sTemp = ""
				if pCity.isConnectedToCapital(iPlayer):
					sTemp += CyTranslator().getText("[ICON_TRADE]", ())
				for i in xrange(gc.getNumReligionInfos()):
					if pCity.isHolyCityByType(i):
						sTemp += u"%c" %(gc.getReligionInfo(i).getHolyCityChar())
					elif pCity.isHasReligion(i):
						sTemp += u"%c" %(gc.getReligionInfo(i).getChar())

				for i in xrange(gc.getNumCorporationInfos()):
					if pCity.isHeadquartersByType(i):
						sTemp += u"%c" %(gc.getCorporationInfo(i).getHeadquarterChar())
					elif pCity.isHasCorporation(i):
						sTemp += u"%c" %(gc.getCorporationInfo(i).getChar())
				if len(sTemp) > 0:
					sText += "\n" + sTemp

				iMaxDefense = pCity.getTotalDefense(False)
				if iMaxDefense > 0:
					sText += u"\n%s: " %(CyTranslator().getText("[ICON_DEFENSE]", ()))
					iCurrent = pCity.getDefenseModifier(False)
					if iCurrent != iMaxDefense:
						sText += u"%d/" %(iCurrent)
					sText += u"%d%%" %(iMaxDefense)

				sText += u"\n%s: %d/%d" %(CyTranslator().getText("[ICON_FOOD]", ()), pCity.getFood(), pCity.growthThreshold())
				iFoodGrowth = pCity.foodDifference(True)
				if iFoodGrowth != 0:
					sText += u" %+d" %(iFoodGrowth)

				if pCity.isProduction():
					sText += u"\n%s:" %(CyTranslator().getText("[ICON_PRODUCTION]", ()))
					if not pCity.isProductionProcess():
						sText += u" %d/%d" %(pCity.getProduction(), pCity.getProductionNeeded())
						iProduction = pCity.getCurrentProductionDifference(False, True)
						if iProduction != 0:
							sText += u" %+d" %(iProduction)
					sText += u" (%s)" %(pCity.getProductionName())
				
				iGPRate = pCity.getGreatPeopleRate()
				iProgress = pCity.getGreatPeopleProgress()
				if iGPRate > 0 or iProgress > 0:
					sText += u"\n%s: %d/%d %+d" %(CyTranslator().getText("[ICON_GREATPEOPLE]", ()), iProgress, pPlayer.greatPeopleThreshold(False), iGPRate)

				sText += u"\n%s: %d/%d (%s)" %(CyTranslator().getText("[ICON_CULTURE]", ()), pCity.getCulture(iPlayer), pCity.getCultureThreshold(), gc.getCultureLevelInfo(pCity.getCultureLevel()).getDescription())

				lTemp = []
				for i in xrange(CommerceTypes.NUM_COMMERCE_TYPES):
					iAmount = pCity.getCommerceRateTimes100(i)
					if iAmount <= 0: continue
					sTemp = u"%d.%02d%c" %(pCity.getCommerceRate(i), pCity.getCommerceRateTimes100(i)%100, gc.getCommerceInfo(i).getChar())
					lTemp.append(sTemp)
				if len(lTemp) > 0:
					sText += "\n"
					for i in xrange(len(lTemp)):
						sText += lTemp[i]
						if i < len(lTemp) - 1:
							sText += ", "

				iMaintenance = pCity.getMaintenanceTimes100()
				if iMaintenance != 0:
					sText += "\n" + CyTranslator().getText("[COLOR_WARNING_TEXT]", ()) + CyTranslator().getText("INTERFACE_CITY_MAINTENANCE", ()) + " </color>"
					sText += u"-%d.%02d%c" %(iMaintenance/100, iMaintenance%100, gc.getCommerceInfo(CommerceTypes.COMMERCE_GOLD).getChar())

				lBuildings = []
				lWonders = []
				for i in xrange(gc.getNumBuildingInfos()):
					if pCity.isHasBuilding(i):
						Info = gc.getBuildingInfo(i)
						if isLimitedWonderClass(Info.getBuildingClassType()):
							lWonders.append(Info.getDescription())
						else:
							lBuildings.append(Info.getDescription())
				if len(lBuildings) > 0:
					lBuildings.sort()
					sText += "\n" + CyTranslator().getText("[COLOR_BUILDING_TEXT]", ()) + CyTranslator().getText("TXT_KEY_PEDIA_CATEGORY_BUILDING", ()) + ": </color>"
					for i in xrange(len(lBuildings)):
						sText += lBuildings[i]
						if i < len(lBuildings) - 1:
							sText += ", "
				if len(lWonders) > 0:
					lWonders.sort()
					sText += "\n" + CyTranslator().getText("[COLOR_SELECTED_TEXT]", ()) + CyTranslator().getText("TXT_KEY_CONCEPT_WONDERS", ()) + ": </color>"
					for i in xrange(len(lWonders)):
						sText += lWonders[i]
						if i < len(lWonders) - 1:
							sText += ", "
				sText += "</font>"
				return sText
			sText = u"<font=2>%s: %d/%d %s</font>" %(pCity.getName(), pCity.getGreatPeopleProgress(), pPlayer.greatPeopleThreshold(False), CyTranslator().getText("[ICON_GREATPEOPLE]", ()))
			for i in xrange(gc.getNumUnitInfos()):
				iProgress = pCity.getGreatPeopleUnitProgress(i)
				if iProgress > 0:
					sText += u"<font=2>\n%s%s: %d</font>" %(CyTranslator().getText("[ICON_BULLET]", ()), gc.getUnitInfo(i).getDescription(), iProgress)
			
			return sText
## Tech Help Text ##
		elif iData1 == 7800:
			return gc.getTechInfo(iData2).getHelp()
## TechScreen Hide ##
		elif iData1 == 7801:
			if iData2 == 0:
				return CyTranslator().getText("TXT_KEY_HIDE_RESEARCHED", ())
			elif iData2 == 1:
				return CyTranslator().getText("TXT_KEY_HIDE_DISABLED", ())
			elif iData2 == 2:
				return CyTranslator().getText("TXT_KEY_HIDE_IRRELEVANT", ())
## Religion Widget Text##
		elif iData1 == 7869:
			return CyGameTextMgr().parseReligionInfo(iData2, False)
## Building Widget Text##
		elif iData1 == 7870:
			return CyGameTextMgr().getBuildingHelp(iData2, False, False, False, None)
## Tech Widget Text##
		elif iData1 == 7871:
			if iData2 == -1:
				return CyTranslator().getText("TXT_KEY_CULTURELEVEL_NONE", ())
			return CyGameTextMgr().getTechHelp(iData2, False, False, False, False, -1)
## Civilization Widget Text##
		elif iData1 == 7872:
			iCiv = iData2 % 10000
			return CyGameTextMgr().parseCivInfos(iCiv, False)
## Promotion Widget Text##
		elif iData1 == 7873:
			return CyGameTextMgr().getPromotionHelp(iData2, False)
## Feature Widget Text##
		elif iData1 == 7874:
			if iData2 == -1:
				return CyTranslator().getText("TXT_KEY_CULTURELEVEL_NONE", ())
			iFeature = iData2 % 10000
			return CyGameTextMgr().getFeatureHelp(iFeature, False)
## Terrain Widget Text##
		elif iData1 == 7875:
			return CyGameTextMgr().getTerrainHelp(iData2, False)
## Leader Widget Text##
		elif iData1 == 7876:
			iLeader = iData2 % 10000
			return CyGameTextMgr().parseLeaderTraits(iLeader, -1, False, False)
## Improvement Widget Text##
		elif iData1 == 7877:
			if iData2 == -1:
				return CyTranslator().getText("TXT_KEY_CULTURELEVEL_NONE", ())
			return CyGameTextMgr().getImprovementHelp(iData2, False)
## Bonus Widget Text##
		elif iData1 == 7878:
			if iData2 == -1:
				return CyTranslator().getText("TXT_KEY_CULTURELEVEL_NONE", ())
			return CyGameTextMgr().getBonusHelp(iData2, False)
## Specialist Widget Text##
		elif iData1 == 7879:
			return CyGameTextMgr().getSpecialistHelp(iData2, False)
## Yield Text##
		elif iData1 == 7880:
			return gc.getYieldInfo(iData2).getDescription()
## Commerce Text##
		elif iData1 == 7881:
			return gc.getCommerceInfo(iData2).getDescription()
## Corporation Screen ##
		elif iData1 == 8201:
			return CyGameTextMgr().parseCorporationInfo(iData2, False)
## Military Screen ##
		elif iData1 == 8202:
			if iData2 == -1:
				return CyTranslator().getText("TXT_KEY_PEDIA_ALL_UNITS", ())
			return CyGameTextMgr().getUnitHelp(iData2, False, False, False, None)
		elif iData1 == 8203:
			return gc.getUnitCombatInfo(iData2).getDescription()
		elif iData1 == 8204:
			return u"%s (%s)" %(gc.getPlayer(iData2).getName(), gc.getPlayer(iData2).getCivilizationShortDescription(0))
		elif iData1 > 8299 and iData1 < 8400:
			iPlayer = iData1 - 8300
			pUnit = gc.getPlayer(iPlayer).getUnit(iData2)
			sText = CyGameTextMgr().getSpecificUnitHelp(pUnit, True, False)
			if CyGame().GetWorldBuilderMode():
				sText += "\n" + CyTranslator().getText("TXT_KEY_WB_UNIT", ()) + " ID: " + str(iData2)
				sText += "\n" + CyTranslator().getText("TXT_KEY_WB_GROUP", ()) + " ID: " + str(pUnit.getGroupID())
				sText += "\n" + "X: " + str(pUnit.getX()) + ", Y: " + str(pUnit.getY())
				sText += "\n" + CyTranslator().getText("TXT_KEY_WB_AREA_ID", ()) + ": "  + str(pUnit.plot().getArea())
			return sText
## Civics Screen ##
		elif iData1 == 8205 or iData1 == 8206:
			sText = CyGameTextMgr().parseCivicInfo(iData2, False, True, False)
			if gc.getCivicInfo(iData2).getUpkeep() > -1:
				sText += "\n" + gc.getUpkeepInfo(gc.getCivicInfo(iData2).getUpkeep()).getDescription()
			else:
				sText += "\n" + CyTranslator().getText("TXT_KEY_CIVICS_SCREEN_NO_UPKEEP", ())
			return sText
## Espionage Screen ##
		elif iData1 == 8207:
			return CyTranslator().getText("TXT_KEY_ESPIONAGE_PASSIVE_AUTOMATIC", ())
		elif iData1 == 8208:
			return CyTranslator().getText("TXT_KEY_ESPIONAGE_MISSIONS_SPY", ())

### Keyboard handlers ###
# Ctrl F1
def showInterfaceHelp(argslist):
	CyInterface().addMessage(CyGame().getActivePlayer(),True,30,CyInterface().getHelpString(),'',0, '', -1, -1, -1, True,True)
	return 1

# Ctrl F2
def showLeaders(argslist):
	iPlayer = CyGame().getActivePlayer()
	(pUnit, iter) = gc.getPlayer(iPlayer).firstUnit(False)
	while(pUnit):
		if pUnit.getLeaderUnitType() > -1:
			sText = u"%s%s</color>" %(CyTranslator().getText("[COLOR_UNIT_TEXT]", ()), pUnit.getName())
			iDenom = pUnit.movesLeft() % gc.getMOVE_DENOMINATOR() > 0
			iCurrMoves = (pUnit.movesLeft() / gc.getMOVE_DENOMINATOR()) + iDenom
			sText += u" %d/%d%c" %(iCurrMoves, pUnit.baseMoves(), CyGame().getSymbolID(FontSymbols.MOVES_CHAR) )

			iX = pUnit.getX()
			iY = pUnit.getY()
			pCity = CyMap().findCity(pUnit.getX(), pUnit.getY(), -1, -1, False, False, -1, -1, CyCity())
			if pCity:
				pOwner = gc.getPlayer(pCity.getOwner())
				sText += u"<color=%d,%d,%d,%d> %s</color>" %(pOwner.getPlayerTextColorR(), pOwner.getPlayerTextColorG(), pOwner.getPlayerTextColorB(), pOwner.getPlayerTextColorA(), pCity.getName())
				if pCity.getX() != iX:
					sText += u" %+dX" %(iX - pCity.getX())
				if pCity.getY() != iY:
					sText += u" %+dY" %(iY - pCity.getY())
			CyInterface().addMessage(iPlayer,True,30,sText,'',0, pUnit.getButton(),ColorTypes(11), pUnit.getX(), pUnit.getY(), True,True)
		(pUnit, iter) = gc.getPlayer(iPlayer).nextUnit(iter, False)
	return 1

# Ctrl F9
def showArtDefChecker(argslist):
	ArtDefChecker.ArtDefChecker().interfaceScreen()
	
# Ctrl F10
def showMirrorImage(argslist):
	MirrorImage.MirrorImage().interfaceScreen()
	return 1
	
# Ctrl F11
def showForgetful(argslist):
	Forgetful.Forgetful().interfaceScreen()
	return 1
	
# Ctrl F12
def showTimeKeeper(argslist):
	TimeKeeper.TimeKeeper().interfaceScreen()
	return 1

### Event handlers ###	
class PlatyUIEventHandler:

	def __init__(self, eventManager):
		self.eventManager = eventManager

		eventManager.setEventHandler("changeWar", self.onChangeWar) # Note SET not ADD
		eventManager.setEventHandler("cityAcquired", self.onCityAcquired) # Note SET not ADD
		eventManager.addEventHandler("cityDoTurn", self.onCityDoTurn)
		eventManager.addEventHandler("cityGrowth", self.onCityGrowth)
		eventManager.setEventHandler("corporationFounded", self.onCorporationFounded) # Note SET not ADD
		eventManager.setEventHandler("corporationRemove", self.onCorporationRemove) # Note SET not ADD
		eventManager.setEventHandler("corporationSpread", self.onCorporationSpread) # Note SET not ADD
		eventManager.setEventHandler("cultureExpansion", self.onCultureExpansion) # Note SET not ADD
		eventManager.setEventHandler("firstContact", self.onFirstContact) # Note SET not ADD
		eventManager.setEventHandler("goldenAge", self.onGoldenAge) # Note SET not ADD
		eventManager.setEventHandler("endGoldenAge", self.onEndGoldenAge) # Note SET not ADD
		eventManager.setEventHandler("greatPersonBorn", self.onGreatPersonBorn) # Note SET not ADD
		eventManager.setEventHandler("improvementBuilt", self.onImprovementBuilt) # Note SET not ADD
		eventManager.setEventHandler("improvementDestroyed", self.onImprovementDestroyed) # Note SET not ADD
		eventManager.addEventHandler("ModNetMessage", self.onModNetMessage)
		eventManager.setEventHandler("playerChangeStateReligion", self.onPlayerChangeStateReligion) # Note SET not ADD
		eventManager.setEventHandler("plotRevealed", self.onPlotRevealed) # Note SET not ADD
		eventManager.setEventHandler("religionFounded", self.onReligionFounded) # Note SET not ADD
		eventManager.setEventHandler("religionRemove", self.onReligionRemove) # Note SET not ADD
		eventManager.setEventHandler("religionSpread", self.onReligionSpread) # Note SET not ADD
		eventManager.setEventHandler("routeBuilt", self.onRouteBuilt) # Note SET not ADD
		eventManager.setEventHandler("techAcquired", self.onTechAcquired) # Note SET not ADD
		eventManager.addEventHandler("unitBuilt", self.onUnitBuilt)
		eventManager.setEventHandler("unitCreated", self.onUnitCreated) # Note SET not ADD
		eventManager.setEventHandler("unitLost", self.onUnitLost) # Note SET not ADD
		eventManager.setPopupHandler(PLATY_UI_WB_PLAYER_SCRIPT_EVENT_ID, ("PlatyUI.WBPlayerScript", self.__eventWBPlayerScriptPopupApply, self.__eventWBScriptPopupBegin))
		eventManager.setPopupHandler(PLATY_UI_WB_CITY_SCRIPT_EVENT_ID, ("PlatyUI.WBCityScript", self.__eventWBCityScriptPopupApply, self.__eventWBScriptPopupBegin))
		eventManager.setPopupHandler(PLATY_UI_WB_UNIT_SCRIPT_EVENT_ID, ("PlatyUI.WBUnitScript", self.__eventWBUnitScriptPopupApply, self.__eventWBScriptPopupBegin))
		eventManager.setPopupHandler(PLATY_UI_WB_GAME_SCRIPT_EVENT_ID, ("PlatyUI.WBGameScript", self.__eventWBGameScriptPopupApply, self.__eventWBScriptPopupBegin))
		eventManager.setPopupHandler(PLATY_UI_WB_PLOT_SCRIPT_EVENT_ID, ("PlatyUI.WBPlotScript", self.__eventWBPlotScriptPopupApply, self.__eventWBScriptPopupBegin))
				
	def onChangeWar(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onChangeWar(argsList)

	def onCityAcquired(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onCityAcquired(argsList)

	def onCityDoTurn(self, argsList):
		'City Production'
		pCity = argsList[0]
		iPlayer = argsList[1]

		if pCity.isDisorder(): return
		pPlayer = gc.getPlayer(iPlayer)
		if not pPlayer.isHuman(): return
		if not pPlayer.isOption(PlayerOptionTypes.PLAYEROPTION_ADVISOR_POPUPS): return

		if pCity.getFoodTurnsLeft() == 1:
			CyInterface().addMessage(iPlayer,True,15,CyTranslator().getText("TXT_KEY_ALERT_GROW_SOON", (pCity.getName(), pCity.getPopulation() + 1,)),"",0, "",gc.getInfoTypeForString("COLOR_YIELD_FOOD"), -1, -1, True,True)
			if pCity.goodHealth() == pCity.badHealth(False):
				CyInterface().addMessage(iPlayer,True,15,CyTranslator().getText("TXT_KEY_ALERT_UNHEALTHY_SOON", (pCity.getName(),)),"",0, "",gc.getInfoTypeForString("COLOR_PLAYER_LIGHT_GREEN"), -1, -1, True,True)
			if pCity.happyLevel() == pCity.unhappyLevel(0):
				CyInterface().addMessage(iPlayer,True,15,CyTranslator().getText("TXT_KEY_ALERT_UNHAPPY_SOON", (pCity.getName(),)),"",0, "",gc.getInfoTypeForString("COLOR_RED"), -1, -1, True,True)

		else:
			iFoodDiff = pCity.foodDifference(True)
			if iFoodDiff < 0 and pCity.getFood() < -iFoodDiff:
				CyInterface().addMessage(iPlayer,True,15,CyTranslator().getText("TXT_KEY_ALERT_STARVE_SOON", (pCity.getName(),)),"",0, "",gc.getInfoTypeForString("COLOR_RED"), -1, -1, True,True)

		iCultureThreshold100 = pCity.getCultureThreshold() * 100
		if iCultureThreshold100 > pCity.getCultureTimes100(iPlayer):
			if iCultureThreshold100 - pCity.getCultureTimes100(iPlayer) <= pCity.getCommerceRateTimes100(CommerceTypes.COMMERCE_CULTURE):
				CyInterface().addMessage(iPlayer,True,15,CyTranslator().getText("TXT_KEY_ALERT_CULTURE_SOON", (pCity.getName(),)),"",0, "",gc.getInfoTypeForString("COLOR_CULTURE_STORED"), -1, -1, True,True)

		bWorldHurry = False
		item = pCity.getProductionBuilding()
		if item > -1:
			itemInfo = gc.getBuildingInfo(item)
			bWorldHurry = isWorldWonderClass(itemInfo.getBuildingClassType())
		if not bWorldHurry:
			item = pCity.getProductionUnit()
			if item > -1:
				itemInfo = gc.getUnitInfo(item)
				bWorldHurry = not isWorldUnitClass(itemInfo.getUnitClassType())
		if bWorldHurry:
			for iHurry in xrange(gc.getNumHurryInfos()):
				if pCity.canHurry(iHurry, True):
					HurryInfo = gc.getHurryInfo(iHurry)
					if HurryInfo.getGoldPerProduction() > 0 and pCity.hurryGold(iHurry) <= pPlayer.getGold():
						CyInterface().addMessage(iPlayer,True,15,CyTranslator().getText("TXT_KEY_ALERT_CAN_HURRY", (pCity.getName(), itemInfo.getDescription(), pCity.hurryGold(iHurry), gc.getCommerceInfo(CommerceTypes.COMMERCE_GOLD).getDescription())),"",0, "",gc.getInfoTypeForString("COLOR_CYAN"), -1, -1, True,True)
					if HurryInfo.getProductionPerPopulation() > 0 and pCity.hurryPopulation(iHurry) <= pCity.maxHurryPopulation():
						CyInterface().addMessage(iPlayer,True,15,CyTranslator().getText("TXT_KEY_ALERT_CAN_HURRY", (pCity.getName(), itemInfo.getDescription(), pCity.hurryPopulation(iHurry), CyTranslator().getText("TXT_KEY_SPECIALIST_CITIZEN", ()))),"",0, "",gc.getInfoTypeForString("COLOR_CYAN"), -1, -1, True,True)

	def onCityGrowth(self, argsList):
		'City Population Growth'
		pCity = argsList[0]
		iPlayer = argsList[1]

		pPlayer = gc.getPlayer(iPlayer)
		if not pPlayer.isHuman(): return
		if not pPlayer.isOption(PlayerOptionTypes.PLAYEROPTION_ADVISOR_POPUPS): return

		CyInterface().addMessage(iPlayer,True,15,CyTranslator().getText("TXT_KEY_ALERT_GROW_NOW", (pCity.getName(), pCity.getPopulation(),)),"",0, "",gc.getInfoTypeForString("COLOR_YIELD_FOOD"), -1, -1, True,True)

		if pCity.badHealth(False) - pCity.goodHealth() == 1:
			CyInterface().addMessage(iPlayer,True,15,CyTranslator().getText("TXT_KEY_ALERT_UNHEALTHY_NOW", (pCity.getName(),)),"",0, "",gc.getInfoTypeForString("COLOR_PLAYER_LIGHT_GREEN"), -1, -1, True,True)
		if pCity.unhappyLevel(0) - pCity.happyLevel() == 1:
			CyInterface().addMessage(iPlayer,True,15,CyTranslator().getText("TXT_KEY_ALERT_UNHAPPY_NOW", (pCity.getName(),)),"",0, "",gc.getInfoTypeForString("COLOR_RED"), -1, -1, True,True)
		
	def onCorporationFounded(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return

		iCorporation, iFounder = argsList
		iCityId = gc.getGame().getHeadquarters(iCorporation).getID()
		if CyGame().isFinalInitialized() and not CyGame().GetWorldBuilderMode():
			if iFounder == CyGame().getActivePlayer() and not CyGame().isNetworkMultiPlayer():
				popupInfo = CyPopupInfo()
				popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON_SCREEN)
				popupInfo.setData1(iCorporation)
				popupInfo.setData2(iCityId)
				popupInfo.setData3(3)
				popupInfo.setText(u"showWonderMovie")
				popupInfo.addPopup(iFounder)
		
		self.eventManager.onCorporationFounded(argsList)

	def onCorporationRemove(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onCorporationRemove(argsList)

	def onCorporationSpread(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onCorporationFounded(argsList)

	def onCultureExpansion(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onCultureExpansion(argsList)

	def onFirstContact(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onFirstContact(argsList)
	
	def onGoldenAge(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onGoldenAge(argsList)

	def onEndGoldenAge(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onEndGoldenAge(argsList)

	def onGreatPersonBorn(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onGreatPersonBorn(argsList)

	def onImprovementBuilt(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onImprovementBuilt(argsList)

	def onImprovementDestroyed(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onImprovementDestroyed(argsList)

	def onModNetMessage(self, argsList):
		iData1, iData2, iData3, iData4, iData5 = argsList
		if iData1 == 5678:
			CommerceType = [CommerceTypes.COMMERCE_GOLD, CommerceTypes.COMMERCE_RESEARCH, CommerceTypes.COMMERCE_CULTURE, CommerceTypes.COMMERCE_ESPIONAGE]
			gc.getPlayer(CyGame().getActivePlayer()).setCommercePercent(CommerceType[iData2], iData3)

	def onPlayerChangeStateReligion(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onPlayerChangeStateReligion(argsList)

	def onPlotRevealed(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onPlotRevealed(argsList)

	def onReligionFounded(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onReligionFounded(argsList)

	def onReligionRemove(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onReligionRemove(argsList)

	def onReligionSpread(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onReligionSpread(argsList)

	def onRouteBuilt(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onRouteBuilt(argsList)

	def onTechAcquired(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onTechAcquired(argsList)

	def onUnitBuilt(self, argsList):
		'Unit Completed'
		city = argsList[0]
		unit = argsList[1]

		if not CyGame().isNetworkMultiPlayer() and city.getOwner() == CyGame().getActivePlayer() and isWorldUnitClass(unit.getUnitClassType()):
			if not CyGame().GetWorldBuilderMode():
				popupInfo = CyPopupInfo()
				popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON_SCREEN)
				popupInfo.setData1(unit.getUnitType())
				popupInfo.setData2(city.getID())
				popupInfo.setData3(4)
				popupInfo.setText(u"showWonderMovie")
				popupInfo.addPopup(city.getOwner())
		
	def onUnitCreated(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onUnitCreated(argsList)

	def onUnitLost(self, argsList):
		if CyGame().GetWorldBuilderMode() and not CvPlatyBuilderScreen.bPython: return
		self.eventManager.onUnitLost(argsList)
	
	def __eventWBPlayerScriptPopupApply(self, playerID, userData, popupReturn):
		sScript = popupReturn.getEditBoxString(0)
		gc.getPlayer(userData[0]).setScriptData(CvUtil.convertToStr(sScript))
		WBPlayerScreen.WBPlayerScreen().placeScript()
		return

	def __eventWBCityScriptPopupApply(self, playerID, userData, popupReturn):
		sScript = popupReturn.getEditBoxString(0)
		pCity = gc.getPlayer(userData[0]).getCity(userData[1])
		pCity.setScriptData(CvUtil.convertToStr(sScript))
		WBCityEditScreen.WBCityEditScreen().placeScript()
		return

	def __eventWBUnitScriptPopupApply(self, playerID, userData, popupReturn):
		sScript = popupReturn.getEditBoxString(0)
		pUnit = gc.getPlayer(userData[0]).getUnit(userData[1])
		pUnit.setScriptData(CvUtil.convertToStr(sScript))
		WBUnitScreen.WBUnitScreen(CvPlatyBuilderScreen.CvWorldBuilderScreen()).placeScript()
		return

	def __eventWBGameScriptPopupApply(self, playerID, userData, popupReturn):
		sScript = popupReturn.getEditBoxString(0)
		CyGame().setScriptData(CvUtil.convertToStr(sScript))
		WBGameDataScreen.WBGameDataScreen(CvPlatyBuilderScreen.CvWorldBuilderScreen()).placeScript()
		return

	def __eventWBPlotScriptPopupApply(self, playerID, userData, popupReturn):
		sScript = popupReturn.getEditBoxString(0)
		pPlot = CyMap().plot(userData[0], userData[1])
		pPlot.setScriptData(CvUtil.convertToStr(sScript))
		WBPlotScreen.WBPlotScreen().placeScript()
		return

	def __eventWBLandmarkPopupApply(self, playerID, userData, popupReturn):
		sScript = popupReturn.getEditBoxString(0)
		pPlot = CyMap().plot(userData[0], userData[1])
		iPlayer = userData[2]
		if userData[3] > -1:
			pSign = CyEngine().getSignByIndex(userData[3])
			iPlayer = pSign.getPlayerType()
			CyEngine().removeSign(pPlot, iPlayer)
		if len(sScript):
			if iPlayer == gc.getBARBARIAN_PLAYER():
				CyEngine().addLandmark(pPlot, CvUtil.convertToStr(sScript))
			else:
				CyEngine().addSign(pPlot, iPlayer, CvUtil.convertToStr(sScript))
		WBPlotScreen.iCounter = 10
		return

	def __eventWBScriptPopupBegin(self):
		return

	def __eventEditCityNameBegin(self, city, bRename):
		popup = PyPopup.PyPopup(CvUtil.EventEditCityName, EventContextTypes.EVENTCONTEXT_ALL)
		popup.setUserData((city.getID(), bRename, CyGame().getActivePlayer()))
		popup.setHeaderString(localText.getText("TXT_KEY_NAME_CITY", ()))
		popup.setBodyString(localText.getText("TXT_KEY_SETTLE_NEW_CITY_NAME", ()))
		popup.createEditBox(city.getName())
		popup.setEditBoxMaxCharCount(25)
		popup.launch()
	
	def __eventEditCityNameApply(self, playerID, userData, popupReturn):
		city = gc.getPlayer(userData[2]).getCity(userData[0])
		cityName = popupReturn.getEditBoxString(0)
		city.setName(cityName, not userData[1])
		if CyGame().GetWorldBuilderMode():
			WBCityEditScreen.WBCityEditScreen().placeStats()

	def __eventEditUnitNameBegin(self, argsList):
		pUnit = argsList
		popup = PyPopup.PyPopup(CvUtil.EventEditUnitName, EventContextTypes.EVENTCONTEXT_ALL)
		popup.setUserData((pUnit.getID(), CyGame().getActivePlayer()))
		popup.setBodyString(localText.getText("TXT_KEY_RENAME_UNIT", ()))
		popup.createEditBox(pUnit.getNameNoDesc())
		popup.setEditBoxMaxCharCount(25)
		popup.launch()

	def __eventEditUnitNameApply(self, playerID, userData, popupReturn):
		unit = gc.getPlayer(userData[1]).getUnit(userData[0])
		newName = popupReturn.getEditBoxString(0)		
		unit.setName(newName)
		if CyGame().GetWorldBuilderMode():
			WBUnitScreen.WBUnitScreen(CvPlatyBuilderScreen.CvWorldBuilderScreen()).placeStats()
			WBUnitScreen.WBUnitScreen(CvPlatyBuilderScreen.CvWorldBuilderScreen()).placeCurrentUnit()	
