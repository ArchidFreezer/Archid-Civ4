## Sid Meier's Civilization 4
## Copyright Firaxis Games 2006
## 
## CvEventManager
## This class is passed an argsList from CvAppInterface.onEvent
## The argsList can contain anything from mouse location to key info
## The EVENTLIST that are being notified can be found 

from CvPythonExtensions import *
import ArchidUtils
import BugCore
import BugUtil
import CvUtil
import Popup as PyPopup

PICK_TRAIT = CvUtil.getNewEventID("AncientMysteriesUtils.pickTrait")

gc = CyGlobalContext()
localText = CyTranslator()

class AncientMysteriesEventHandler:
	
	def __init__(self, customEM) :
		
		print "Initialising Ancient Mysteries Mod"
		
		self.customEM = customEM
		
		self.customEM.addEventHandler( 'buildingBuilt', self.onBuildingBuilt )
		self.customEM.setPopupHandler( PICK_TRAIT, ["pickTrait", pickTraitPopupHandler, self.blankHandler] )

		# Keep game from showing messages about handling these popups
		CvUtil.SilentEvents.extend([PICK_TRAIT])

	def blankHandler( self, playerID, netUserData, popupReturn ) :
		# Dummy handler to take the second event for popup
		return

	def onBuildingBuilt(self, argsList):
		pCity, iBuildingType = argsList

		iOwner = pCity.getOwner()
		pPlayer = gc.getPlayer(iOwner)
		
		if iOwner == gc.getGame().getActivePlayer():
			if iBuildingType == gc.getInfoTypeForString("BUILDING_ANCIENT_MYSTERIES"):
				if not pPlayer.isNone() and pPlayer.isAlive() and not pPlayer.isBarbarian() :
					pickTrait(iOwner, pPlayer)

#
# Main functions here
def pickTrait(iOwner, pPlayer):
	if pPlayer.isHuman():
		# Let the human player select a new trait from a popup
		popup = PyPopup.PyPopup(PICK_TRAIT,contextType = EventContextTypes.EVENTCONTEXT_ALL)
		szTitle = u"<font=4b>" + localText.getText("TXT_KEY_TRAITS_POPUP_TITLE", ()) + u"</font>"
		popup.setHeaderString(szTitle)
		popup.createPullDown()
		for iTrait in range(gc.getNumTraitInfos()):
			if not pPlayer.hasTrait(iTrait):
				szTraitName = gc.getTraitInfo(iTrait).getDescription()
				#CyInterface().addImmediateMessage(str(szTraitName), "")
				popup.addPullDownString(szTraitName, iTrait)
		popup.addSeparator()
		popup.addButton(BugUtil.getPlainText("TXT_KEY_MAIN_MENU_OK"))
		popup.addButton(BugUtil.getPlainText("TXT_KEY_POPUP_CANCEL"))
		popup.launch(False, PopupStates.POPUPSTATE_IMMEDIATE)
	else:
		iFinancialTrait = gc.getInfoTypeForString("TRAIT_FINANCIAL")
		
		# Give the AI player the Financial Trait (If available)
		if not pPlayer.hasTrait(iFinancialTrait):
			pPlayer.setHasTrait((iFinancialTrait), True)
		
		else:
			# Get a list of all the available traits
			arrTraits = []
			for iTrait in range(gc.getNumTraitInfos()):
				if not iTrait == None:
					if not pPlayer.hasTrait(iTrait):
						arrTraits.append(iTrait)
			
			# Give the AI Player a random trait
			index = CyGame().getSorenRandNum(len(arrTraits), "Ancient Mysteries")
			pPlayer.setHasTrait((arrTraits[index]), True)
			
def	pickTraitPopupHandler(playerID, netUserData, popupReturn):
	
	pPlayer = gc.getPlayer(playerID)
	iTrait	= popupReturn.getSelectedPullDownValue(0)

	TraitName = gc.getTraitInfo(iTrait).getDescription()
	#CyInterface().addImmediateMessage(str(iButtonId), "")
	pPlayer.setHasTrait(iTrait, True)
	pTitle = "You have a new Trait!"
	pText = "Oh might ruler, our excavations have discovered the ancient mysteries from long ago.  You have acquired a trait: " + str(TraitName)
	ArchidUtils.standardPopup(pTitle, pText, playerID)
