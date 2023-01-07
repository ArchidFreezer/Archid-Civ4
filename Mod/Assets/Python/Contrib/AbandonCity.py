##-------------------------------------------------------------------
## Modified from Abandon Raze City by unknown, Bhruic, Nemo, OrionVetran
##  by Dancing Hoskuld with a great deal of help from EmperorFool and Zappara
##  to work in, with and under BUG
##  Minor edit by zeroRPGmaker so mods works correctly with BUG 4.2 under BTS 1.19
##-------------------------------------------------------------------
##
## Things that could be done to improve this mod making it interact with the main mod in a more generic fashon.
## 1. replace the English language stuff with the correct language translations in the text file.
##-------------------------------------------------------------------
##
## To get this working in any mod you need to
## 1. put 	<load mod="Abandon City Mod"/>	in the init.xml in the Python/Config directory
##
## Modders:- If you want any special events when a city is abandoned add them to your OnCityRazed event.
##		This mod calls that event.
##  In RoM Holy Shrines are not world wonders a fix to reflect this is in the code it should have no effect
##    on other mods but the test can be commented out if needs be.
##
##-------------------------------------------------------------------
##
## Changes
##  Jan 2010
##  Removed some RoM scripts which kept the Abandon City event from happening under BUG 4.2 and BTS 1.19
##
##  April 2009
##	A moders section of this code now allows a single place for the setting of the "parameters"
##	 - produce just workers or settlers and workers
##	 - produce 1 worker for every X popuation in the city
##	 - produce 1 settler fo every X population in the city with 1 worker for every Y in the remainder
##   Sell buildings rather than demolish them.  Thanks Zappara
##   Unhappiness from selling religious buildings +1 unhappy for x turns.  Where x depends on game speed.
##   Can not abandon last city.
##   Can not sell free buildings however they need to appear in the list for this program to work.
##
##   Note. If you only have one city and it has no buildings then Ctrl-A will appear not to work.  This was
##   done to to stop a CDT which could be caused otherwise.  Besides there is nothing to sell and you can't
##   abandon your only city.
##   - I had made a typo in the check for this - it is now fixed - thanks Zappara
##
## Late April 2009
##  Civ specific unique units for workers and settlers are now produced when appropriate. - thanks Zappara
##  Percentage of building production cost is now in the parameter section so modders can change it
##    currently set at the Rise of mankind default of 20%.  Popup header text now reflects this percentage.
##  More of the text supports language even if I have not put the language stuff there yet.
##  Fixed to support modular buildings. - thanks Zappara
##
## September 2009
## 	Minor coding changes to make the code cleaner.
##		Includes removing excess coments, define globals once, and put parameter for obsolete reduction.
##	Stopped putting free buildings in the list.  
##		The way the popup works is it puts the items in the list in the order supplied (text and index).
##		It displays (text) and returns (index) for the selected item.
##
##-------------------------------------------------------------------
##
## Potential changes
## 1. has an .ini file (AbandonCity.INI) to hold parameters
##	 - mod is active/inactive
##     - only abandon city (no selling of buildings)
##
## 2. buildings in albhabetical order in the list.
##	
##
## ------------------------------------------
## Decided not to generate upgraded settler units as these could then be settled and the free buildings sold off.
##	- obsolete decision now that selling free buildings gives no money.
##	- further obsolete since now we do not present free buildings to be sold :)
##
## Decided to have the population parameters in the code for modders rather than letting the player change them ad hoc.
##
## Decided that you can sell obsolete buildings.  Just because they are obsolete does not mean that they are not having an effect.



from CvPythonExtensions import *
import CvUtil
import CvEventInterface
import PyHelpers
import Popup as PyPopup
import BugCore
import BugUtil
import SdToolKit
import string

# globals

SD_MOD_ID = "AbandonCity"

ABANDON_CITY_DEMOLISH_BUILDING = CvUtil.getNewEventID("AbandonCity.Active")

gc = CyGlobalContext()
localText = CyTranslator()
PyPlayer = PyHelpers.PyPlayer
PyInfo = PyHelpers.PyInfo

options = BugCore.game.AbandonCity

# program globals - change at own risk
g_BuildingCostReturn = 0.2 # Percentage of building cost converted to money, this will be overwritten by the user setting
g_ConstructModifier = 1	# What is this?
g_iAbandonTrigger = gc.getNumBuildingClassInfos() + 1000 # Will fall over if there are more than 1000 modular buildings


class AbandonCityEventHandler:
	def __init__(self, customEM):
		self.customEM = customEM
		
		self.customEM.setPopupHandler(ABANDON_CITY_DEMOLISH_BUILDING, ["abandonCityPopup", abandonCityHandler, self.blankHandler])

		# Keep game from showing messages about handling these popups
		CvUtil.SilentEvents.extend([ABANDON_CITY_DEMOLISH_BUILDING])

	def blankHandler( self, playerID, netUserData, popupReturn ) :
		# Dummy handler to take the second event for popup
		return

def abandonCityPopup(argsList):

	if not options.isEnabled():
		return
	# Get player and city details. Set up headings etc.
	# Display appropriate dialogue.
	
	bAnythingToDisplay = False
	
	#CyInterface().addImmediateMessage("Abandon City Demolish Building - __eventAbandonCityDestroyBuildingBegin called", None)
	pHeadSelectedCity = CyInterface().getHeadSelectedCity()
	pPlayer = gc.getPlayer(pHeadSelectedCity.getOwner())
	civ = gc.getCivilizationInfo(pPlayer.getCivilizationType())
	g_BuildingCostReturn = options.getBuildingReturn()
	
	szheader = BugUtil.getPlainText("TXT_KEY_ABANDON_CITY_HEADER1")
	ipercentageForHeader = g_BuildingCostReturn * 100
	szheader += " " + u"%d" %(ipercentageForHeader) + BugUtil.getPlainText("TXT_KEY_ABANDON_CITY_HEADER2")

	abandoncity = BugUtil.getPlainText("TXT_KEY_ABANDON_CITY")
	ok = BugUtil.getPlainText("TXT_KEY_MAIN_MENU_OK")
	cancel = BugUtil.getPlainText("TXT_KEY_POPUP_CANCEL")
	popup = PyPopup.PyPopup(ABANDON_CITY_DEMOLISH_BUILDING, EventContextTypes.EVENTCONTEXT_ALL)
	popup.setHeaderString(szheader)
	popup.createPullDown()

	# Check if we are allowed to sell buildings
	if not options.isAbandonOnly():
	
		# find out what the gamespeed building cost modifier is
		# and adjust construct modifier also with building production percent game define
		iGameSpeed = gc.getGame().getGameSpeedType()
		iBuildProdPercent = gc.getDefineINT("BUILDING_PRODUCTION_PERCENT")/100
		g_ConstructModifier = iBuildProdPercent*gc.getGameSpeedInfo(iGameSpeed).getConstructPercent()/100
		
		# Populate list box with non-free buildings
		for i in range(0,gc.getNumBuildingClassInfos()):
			iType = civ.getCivilizationBuildings(i)
			#BugUtil.debug("for loop num %d, building %s", i, gc.getBuildingInfo(i).getDescription())
			
			# RoM 2.7 added check for Holy Shrines so that player can't demolish them, those aren't Great Wonders in RoM
			if (iType > 0 and pHeadSelectedCity.isHasBuilding(iType) and not isWorldWonderClass(gc.getBuildingInfo(iType).getBuildingClassType()) and not gc.getBuildingInfo(iType).getGlobalReligionCommerce() > 0):
				# Only put non-free buildings in the list
				if pPlayer.isBuildingFree(iType) == False:
					# Figure out the gold to be paid for the building.
					# Note that this code is repeated when the building is actually sold.
					iGoldBack = int(gc.getBuildingInfo(iType).getProductionCost() * g_ConstructModifier * g_BuildingCostReturn)

					# If building is obsolete, give only half sum back
					obsoleteTech = gc.getBuildingInfo(iType).getObsoleteTech()
					if ( gc.getTeam(pPlayer.getTeam()).isHasTech(obsoleteTech) == false or obsoleteTech == -1 ):
						iGoldBack = int(iGoldBack/2)

					# Build up text to display in the list box
					szBuilding = gc.getBuildingInfo(iType).getDescription()
					szBuilding = gc.getBuildingInfo(iType).getDescription()
					szBuilding += " (" + u"%d " %(iGoldBack) + localText.getText("TXT_KEY_COMMERCE_GOLD", ())
					# Add unhappiness text here
					if gc.getBuildingInfo(iType).getReligionType() >= 0 : # religious building
						szBuilding += ", " + localText.getText("TXT_KEY_ABANDON_UNHAPPINESS", ()) #+  u"%c" % CyGame().getSymbolID(FontSymbols.UNHAPPY_CHAR)
					szBuilding += ")"

					popup.addPullDownString(szBuilding, iType)
					bAnythingToDisplay = True

	# Only allow abandonment of city if there is more than one city
	if pPlayer.getNumCities() > 1 :
		popup.addPullDownString(abandoncity, g_iAbandonTrigger)
		bAnythingToDisplay = True

	if bAnythingToDisplay:
		popup.addSeparator()
		popup.addButton(ok)
		popup.addButton(cancel)
		popup.launch(False, PopupStates.POPUPSTATE_IMMEDIATE)
		return

def abandonCityHandler(playerID, userData, popupReturn):
	#CyInterface().addImmediateMessage("Abandon City Demolish Building - __eventAbandonCityDestroyBuildingApply called", None)
	pPlayer = gc.getPlayer(playerID)
	civ = gc.getCivilizationInfo(pPlayer.getCivilizationType())
	pHeadSelectedCity = CyInterface().getHeadSelectedCity()
	
	
	if (popupReturn.getButtonClicked() == 0):
		# player chooses to abandon the city, should this affect unhappiness among those culture areas that have citizens in this city?
		#- that is the main mods duty such code should be in its OnCityRazed event which is called by this mod - added to documentation

		if (popupReturn.getSelectedPullDownValue(0) == g_iAbandonTrigger ):
			ix = pHeadSelectedCity.getX()
			iy = pHeadSelectedCity.getY()
			ipopulation = CyInterface().getHeadSelectedCity().getPopulation()

			#Call the onCityRazed event to do whatever the other mods do when a city is raised
			#rather than put the code here making this more generaly usable
			# according to http://www.apolyton.net/forums/showthread.php?t=140611
			# the argsList for CityRazed are: city, iPlayer
			CvEventInterface.getEventManager().fireEvent("cityRazed", pHeadSelectedCity, playerID)

			# Abandon the City
			pHeadSelectedCity.kill()
			CyAudioGame().Play2DSound("AS2D_DISCOVERBONUS")

			# Generate the units
			# get the indices for the units being created this is where we should check for
			indexWorker = CvUtil.findInfoTypeNum(gc.getUnitInfo,gc.getNumUnitInfos(),'UNITCLASS_WORKER')
			# Determine the settler type to produce
			szUnit = 'UNITCLASS_SETTLER'
			if options.isAllowAdvancedSettlers():
				if pPlayer.canTrain(CvUtil.findInfoTypeNum(gc.getUnitInfo,gc.getNumUnitInfos(),'UNIT_PIONEER'), True, False): szUnit = 'UNITCLASS_PIONEER'
				elif pPlayer.canTrain(CvUtil.findInfoTypeNum(gc.getUnitInfo,gc.getNumUnitInfos(),'UNIT_COLONIST'), True, False): szUnit = 'UNITCLASS_COLONIST'
			indexSettler = CvUtil.findInfoTypeNum(gc.getUnitInfo,gc.getNumUnitInfos(),szUnit)
			
			# UU workers and settlers - thanks Zappara
			iUniqueWorkerUnit = civ.getCivilizationUnits(indexWorker)
			iUniqueSettlerUnit = civ.getCivilizationUnits(indexSettler)

			# if settlers and workers
			iPopulationForSettlers = options.getSettlerPop()
			iPopulationForWorkers = options.getWorkerPop()
			if options.isAllowSettlers():
				isettlers = ipopulation/iPopulationForSettlers
				iworkers = (ipopulation - iPopulationForSettlers * isettlers)/iPopulationForWorkers
			else:
				isettlers = 0
				iworkers = ipopulation/iPopulationForWorkers
				
			for i in range(0,iworkers):
				if bLogging: g_autolog.writeLog("Creating worker: %d" % (iUniqueWorkerUnit))
				pPlayer.initUnit(iUniqueWorkerUnit, ix, iy, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
			for i in range(0,isettlers):
				if bLogging: g_autolog.writeLog("Creating settler: %d" % (iUniqueSettlerUnit))
				pPlayer.initUnit(iUniqueSettlerUnit, ix, iy, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)

		else:
			iBuildingType = popupReturn.getSelectedPullDownValue(0)
			iType = civ.getCivilizationBuildings(gc.getBuildingInfo(iBuildingType).getBuildingClassType())

			iGoldBack = int(gc.getBuildingInfo(iType).getProductionCost() * g_ConstructModifier * g_BuildingCostReturn)

			obsoleteTech = gc.getBuildingInfo(iType).getObsoleteTech()
			if ( gc.getTeam(pPlayer.getTeam()).isHasTech(obsoleteTech) == True):
				iGoldBack = int(iGoldBack * options.getBuildingObsoleteFactor())

			pPlayer.changeGold(iGoldBack)

			# unhappiness penalty when demolishing any religion buildings
			if gc.getBuildingInfo(iType).getReligionType() >= 0 : # religious building
				pHeadSelectedCity.changeHurryAngerTimer(pHeadSelectedCity.flatHurryAngerLength())
			pHeadSelectedCity.setNumRealBuilding(iType, False)
			CyAudioGame().Play2DSound("AS2D_DISCOVERBONUS")
			CyInterface().setDirty(InterfaceDirtyBits.CityScreen_DIRTY_BIT, True)
			CyInterface().setDirty(InterfaceDirtyBits.SelectionButtons_DIRTY_BIT, True)

	return
