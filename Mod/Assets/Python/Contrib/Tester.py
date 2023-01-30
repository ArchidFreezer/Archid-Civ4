# Tester.py
#
# by jdog5000
#
# Provides event debug output and other testing faculties

from CvPythonExtensions import *
import CvUtil
import sys
import Popup as PyPopup
from PyHelpers import PyPlayer, PyInfo
import BugOptions
import ColorUtil

FIRE_EVENTTRIGGER = CvUtil.getNewEventID("Tester.fireEventTrigger")
KILL_PLAYER_STUFF = CvUtil.getNewEventID("Tester.killPlayerStuff")
STRANDED_EVENT_ID = CvUtil.getNewEventID("Tester.showStranded")

# globals
gc = CyGlobalContext()

class TesterEventHandler:

	def __init__(self, customEM):

		self.customEM = customEM

		self.customEM.setPopupHandler( FIRE_EVENTTRIGGER, ["fireEventTrigger", fireEventTriggerHandler, self.blankHandler] )
		self.customEM.setPopupHandler( KILL_PLAYER_STUFF, ["killPlayerStuffPopup", killPlayerStuffHandler, self.blankHandler] )

		# Keep game from showing messages about handling these popups
		CvUtil.SilentEvents.extend([KILL_PLAYER_STUFF, FIRE_EVENTTRIGGER])

	def blankHandler( self, playerID, netUserData, popupReturn ) :
		# Dummy handler to take the second event for popup
		return

def showStrandedPopup(argsList):
	bodStr = "Stranded units by player:\n"
	for iPlayer in range(0,gc.getMAX_PLAYERS()) :
		pPlayer = gc.getPlayer(iPlayer)
		if( pPlayer.isAlive() ) :
			bodStr += "\n\n%d: %s"%(iPlayer, pPlayer.getCivilizationDescription(0))
			unitList = PyPlayer(iPlayer).getUnitList()
			for pUnit in unitList :
				pGroup = pUnit.getGroup()
				if( pGroup.getHeadUnit().getID() == pUnit.getID() ) :
					if( pGroup.isStranded() ) :
						bodStr += "\n   %s (%d units) at (%d, %d)"%(pUnit.getName(),pGroup.getNumUnits(),pUnit.getX(),pUnit.getY())

	popup = PyPopup.PyPopup(STRANDED_EVENT_ID)
	popup.setBodyString(bodStr)
	popup.launch()

def reloadOptions(argsList):
	bodStr = "Reloading settings\n"
	options = BugOptions.getOptions()
	if not options.isLoaded():
		options.read()
	option = options.getOption("CityBar__Health")
	bodStr += str(option.getValue())
#		options.write()

	popup = PyPopup.PyPopup()
	popup.setBodyString(bodStr)
	popup.launch()


def killPlayerStuffPopup( self ) :
	'Chooser window for killing a players stuff'

	popup = PyPopup.PyPopup(KILL_PLAYER_STUFF,contextType = EventContextTypes.EVENTCONTEXT_ALL)
	popup.setHeaderString( 'Kill a civs stuff?' )
	popup.setBodyString( 'Which civ, and which stuff?' )
	popup.addSeparator()

	popup.createPythonPullDown( 'Kill this civs ...', 1 )
	for i in range(0,gc.getMAX_PLAYERS()) :
		player = PyPlayer(i)
		if( not player.isNone() ) :
			if( player.isAlive() ) :
				popup.addPullDownString( "%s of the %s"%(player.getName(),player.getCivilizationName()), i, 1 )
	activePlayer = gc.getActivePlayer()
	popup.popup.setSelectedPulldownID( activePlayer.getID(), 1 )

	popup.createPythonPullDown( ' ... stuff', 2 )
	popup.addPullDownString( "Cities", 0, 2 )
	popup.addPullDownString( "Deals", 1, 2 )
	popup.addPullDownString( "Units", 2, 2 )
	popup.popup.setSelectedPulldownID( 2, 2 )

	popup.addSeparator()
	popup.addButton('Cancel')
	popup.launch()

def killPlayerStuffHandler( playerID, netUserData, popupReturn ) :
	'Handles changeCiv popup'

	if( popupReturn.getButtonClicked() == 0 ):  # if you pressed cancel
		return

	playerIdx = popupReturn.getSelectedPullDownValue( 1 )
	killType = popupReturn.getSelectedPullDownValue( 2 )

	player = gc.getPlayer(playerIdx)
	if killType == 0:
		player.killCities()
	elif killType == 1:
		player.killDeals()
	elif killType == 2:
		player.killUnits()

def fireEventTrigger( self ) :
	'Chooser window for firing an event'
	popup = PyPopup.PyPopup(FIRE_EVENTTRIGGER,contextType = EventContextTypes.EVENTCONTEXT_ALL)
	popup.setHeaderString( 'Fire an event' )
	popup.setBodyString( 'Which civ, and which stuff?' )
	popup.addSeparator()

	popup.createPythonPullDown(CyTranslator().getText("TXT_KEY_POPUP_SELECT_EVENT",()), 1)
	for i in range(gc.getNumEventTriggerInfos()):
		trigger = gc.getEventTriggerInfo(i)
		popup.addPullDownString( str(trigger.getType()), i, 1 )
	popup.popup.setSelectedPulldownID( 200, 1 )

	popup.addSeparator()
	popup.addButton('Cancel')
	popup.launch()

def fireEventTriggerHandler( playerID, netUserData, popupReturn ):
	
	if( popupReturn.getButtonClicked() == 0 ):  # if you pressed cancel
		return

	eventTriggerName = None
	eventTriggerNumber = -1
	iButtonId	= popupReturn.getSelectedPullDownValue( 1 )

	if iButtonId < gc.getNumEventTriggerInfos():
		eventTriggerName = gc.getEventTriggerInfo(iButtonId).getType()
		eventTriggerNumber = iButtonId
	if eventTriggerName == None:
		return
	if eventTriggerNumber == -1:
		return
	message = 'Event: %s[%d]' % (eventTriggerName, eventTriggerNumber)
	CyInterface().addImmediateMessage(message, "")
	#message = 'pyPrint: You selected Event: %s[%d] for player %d' % (eventTriggerName, eventTriggerNumber, playerID)
	#CvUtil.pyPrint(message)
	#message = 'print: You selected Event: %s[%d]' % (eventTriggerName, eventTriggerNumber)
	#print message
	pPlayer = gc.getPlayer(playerID)
	pPlayer.trigger(eventTriggerNumber)