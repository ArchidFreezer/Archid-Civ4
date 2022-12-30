# ChangePlayer Mod
#
# by jdog5000
# Version 0.71


from CvPythonExtensions import *
import CvUtil
import PyHelpers
import Popup as PyPopup
import BugCore

# globals

CHANGE_CIV = CvUtil.getNewEventID("ChangePlayer.ChangeCiv")
CHANGE_HUMAN = CvUtil.getNewEventID("ChangePlayer.ChangePlayer")

gc = CyGlobalContext()
PyPlayer = PyHelpers.PyPlayer
PyInfo = PyHelpers.PyInfo
game = CyGame()
localText = CyTranslator()

options = BugCore.game.AIAutoPlay

class ChangePlayerEventHandler:

	def __init__(self, customEM ) :

		print "Initializing ChangePlayer Mod"

		self.customEM = customEM

		self.customEM.setPopupHandler( CHANGE_CIV, ["changeCivPopup",changeCivHandler,self.blankHandler] )
		self.customEM.setPopupHandler( CHANGE_HUMAN, ["changeHumanPopup",changeHumanHandler,self.blankHandler] )
		
		# Keep game from showing messages about handling these popups
		CvUtil.SilentEvents.extend([CHANGE_CIV,CHANGE_HUMAN])

	def blankHandler( self, playerID, netUserData, popupReturn ) :
		# Dummy handler to take the second event for popup
		return


def changeCivPopup(argslist) :
	'Chooser window for switching a players civ'

	popup = PyPopup.PyPopup(CHANGE_CIV,contextType = EventContextTypes.EVENTCONTEXT_ALL)
	popup.setHeaderString( 'Change a civ?' )
	popup.setBodyString( 'Which civ to change, to which civ and leader and what team?' )
	popup.addSeparator()

	popup.createPythonPullDown( 'Switch this civ ...', 1 )
	for i in range(0,gc.getMAX_PLAYERS()) :
		player = PyPlayer(i)
		if( not player.isNone() ) :
			if( player.isAlive() ) :
				popup.addPullDownString( "%s of the %s"%(player.getName(),player.getCivilizationName()), i, 1 )
	activePlayer = gc.getActivePlayer()
	popup.popup.setSelectedPulldownID( activePlayer.getID(), 1 )

	popup.createPythonPullDown( ' ... to this civ', 2 )
	for i in range(0,gc.getNumCivilizationInfos()) :
		newCivInfo = gc.getCivilizationInfo(i)
		popup.addPullDownString( "%s Empire"%(newCivInfo.getAdjective(0)), i, 2 )
	popup.popup.setSelectedPulldownID( activePlayer.getCivilizationType(), 2 )

	popup.createPythonPullDown( ' ... with this leader', 3 )
	for i in range(0,gc.getNumLeaderHeadInfos()) :
		newLeaderInfo = gc.getLeaderHeadInfo(i)
		leaderName = newLeaderInfo.getDescription()
		popup.addPullDownString( "%s"%(leaderName), i, 3 )
	popup.popup.setSelectedPulldownID( activePlayer.getLeaderType(), 3 )
	
	popup.createPythonPullDown( ' ... on this team', 4 )
	popup.addPullDownString( "Keep current team", -1, 4 )  # Team idx of -1 maintains current team setting
	for i in range(0,gc.getMAX_PLAYERS()) :
		player = PyPlayer(i)
		if( not player.isNone() ) :
			if( player.isAlive() ) :
				popup.addPullDownString( "Team with the %s"%(player.getCivilizationName()), i, 4 )
	popup.popup.setSelectedPulldownID( -1, 4 )

	popup.addSeparator()
	popup.addButton('Cancel')
	popup.launch()

def changeCivHandler( playerID, netUserData, popupReturn ) :
	'Handles changeCiv popup'

	if( popupReturn.getButtonClicked() == 0 ):  # if you pressed cancel
		return

	playerIdx = popupReturn.getSelectedPullDownValue( 1 )
	newCivType = popupReturn.getSelectedPullDownValue( 2 )
	newLeaderType = popupReturn.getSelectedPullDownValue( 3 )
	teamIdx = popupReturn.getSelectedPullDownValue( 4 )

	if( options.isDebug() ) : CvUtil.pyPrint( "   CP : You have selected player %d, the %s, on team %d"%(playerIdx, gc.getPlayer(playerIdx).getCivilizationDescription(0), gc.getPlayer(playerIdx).getTeam()) )
	if( options.isDebug() ) : CvUtil.pyPrint( "   CP : New team idx is %d"%(teamIdx) )

	success = changeCiv( playerIdx, newCivType, newLeaderType, teamIdx )

	if( success ) :
		CyInterface().addImmediateMessage("Player %d has been changed"%(playerIdx),"")
		if( options.isDebug() ) : CvUtil.pyPrint( "   CP : Player change completed" )
	else :
		CyInterface().addImmediateMessage("An error occured in changeCiv.","")
		if( options.isDebug() ) : CvUtil.pyPrint( "   CP : Error on changeCiv" )
		return
	
def updateGraphicsHandler( playerID, netUserData, popupReturn ) :
	
	if( popupReturn.getButtonClicked() == 0 ):  # if you pressed update now
		updateGraphics( )

def updateGraphics(argslist) :
	# Switch human player around to force a redraw of unit flags
	iHuman = game.getActivePlayer()
	
	iSwitchTo = -1
	for i in range(0,gc.getMAX_CIV_PLAYERS()) :
		player = PyPlayer(i)
		if( not player.isNone() ) :
			if( not player.isAlive() ) :
				iSwitchTo = i
				break
	
	if( iSwitchTo < 0 ) :
		iSwitchTo = 1 + game.getSorenRandNum( gc.getMAX_CIV_PLAYERS() - 1, 'ChangePlayer')
		
	game.setAIAutoPlay( 3 )
	
	changeHuman( iSwitchTo, iHuman )
	changeHuman( iHuman, iSwitchTo )

def changeHumanPopup(argslist) :
	'Chooser window for switching human player'
	popup = PyPopup.PyPopup(CHANGE_HUMAN,contextType = EventContextTypes.EVENTCONTEXT_ALL)
	popup.setHeaderString( 'Pick a new civ' )
	popup.setBodyString( 'Which civ would you like to lead?\n\n(Note:  Switching to barbs is not recommended except as an experiment)' )
	popup.addSeparator()
	popup.createPythonPullDown( 'Take control of this civ ...', 1 )
	for i in range(0,gc.getMAX_PLAYERS()) :
		player = PyPlayer(i)
		if( not player.isNone() ) :
			if( player.isAlive() ) :
				popup.addPullDownString( "%s of the %s"%(player.getName(),player.getCivilizationName()), i, 1 )

	activePlayerIdx = gc.getActivePlayer().getID()
	popup.popup.setSelectedPulldownID( activePlayerIdx, 1 )
	popup.addSeparator()
	popup.addButton('Cancel')
	popup.launch()

def changeHumanHandler( playerID, netUserData, popupReturn ) :
	'Handles changeHuman popup'

	if( popupReturn.getButtonClicked() == 0 ):  # if you pressed cancel
		return

	newHumanIdx = popupReturn.getSelectedPullDownValue( 1 )
	newPlayer = gc.getPlayer(newHumanIdx)
	oldHumanIdx = playerID
	oldPlayer = gc.getPlayer(oldHumanIdx)

	if( newHumanIdx == oldHumanIdx ) :
		if( options.isDebug() ) : CvUtil.pyPrint( "   CP : You have selected the same civ, no change")
		CyInterface().addImmediateMessage("You retain control of the %s"%(oldPlayer.getCivilizationDescription(0)),"")
		return


	if( options.isDebug() ) : CvUtil.pyPrint( "   CP : You have selected player %d, the %s"%(newHumanIdx, newPlayer.getCivilizationDescription(0)) )

	success = changeHuman( newHumanIdx, oldHumanIdx )

	if( success ) :
		if( options.isDebug() ) : CvUtil.pyPrint( "   CP : Number of human players is now %d"%(game.getNumHumanPlayers()) )
		if( options.isDebug() ) : CvUtil.pyPrint( "   CP : Active player is now %d"%(game.getActivePlayer()) )
##			for i in range(0,gc.getMAX_CIV_PLAYERS()) :
##				if( options.isDebug() ) : CvUtil.pyPrint( "   CP : Player %d is human %d"%(i,gc.getPlayer(i).isHuman()))
		CyInterface().addImmediateMessage("You now control the %s"%(newPlayer.getCivilizationDescription(0)),"")
	else :
		if( options.isDebug() ) : CvUtil.pyPrint( "   CP : Error occured, number of human players is now %d"%(game.getNumHumanPlayers()) )
		CyInterface().addImmediateMessage("An error occured in changeHuman ...","")

########################## Player modification functions ###########################################

def changeCiv( playerIdx, newCivType = -1, newLeaderType = -1, teamIdx = -1 ) :
	# Changes specified players civ, leader
	# Does not change isHuman setting

	player = gc.getPlayer(playerIdx)
	oldCivType = player.getCivilizationType()
	oldLeaderType = player.getLeaderType()
	if( newCivType >= 0 and not newCivType == oldCivType ) :
		player.changeCiv( newCivType )
	
	if( newLeaderType >= 0 and not newLeaderType == oldLeaderType ) :
		player.changeLeader( newLeaderType )
	
	return True

def changePersonality( playerIdx, newPersonality = -1 ) :
	# Changes leader personality of this civ
	
	player = gc.getPlayer(playerIdx)
	
	if( newPersonality < 0 ) :
		iBestValue = 0
		newPersonality = -1

		for iI in range(0,gc.getNumLeaderHeadInfos()) :
			if (not iI == gc.getDefineINT("BARBARIAN_LEADER")) :
				iValue = (1 + game.getSorenRandNum(10000, "Choosing Personality"))

				for iJ in range(0,gc.getMAX_CIV_PLAYERS()) :
					if (gc.getPlayer(iJ).isEverAlive()) :
						if (gc.getPlayer(iJ).getPersonalityType() == iI) :
							iValue /= 2

				if (iValue > iBestValue) :
					iBestValue = iValue
					newPersonality = iI

	if (newPersonality >= 0 and newPersonality < gc.getNumLeaderHeadInfos()) :
		player.setPersonalityType(newPersonality)

def changeHuman( newHumanIdx, oldHumanIdx ) :

	game.changeHumanPlayer( newHumanIdx )
	
	doRefortify( newHumanIdx )
	
	return True

########################## Utility functions ###########################################
			
def doRefortify( iPlayer ) :
	 #pyPlayer = PyPlayer( iPlayer )
	pPlayer = gc.getPlayer(iPlayer)
	
	CvUtil.pyPrint( "Refortifying units for player %d"%(iPlayer))

	for groupID in range(0,pPlayer.getNumSelectionGroups()) :
		pGroup = pPlayer.getSelectionGroup(groupID)
		if( pGroup.getNumUnits() > 0 ) :

			headUnit = pGroup.getHeadUnit()
			#CvUtil.pyPrint( "%s fortTurns %d"%(headUnit.getName(),headUnit.getFortifyTurns()) )
			if( headUnit.getFortifyTurns() > 0 ) :
				if( headUnit.isHurt() ) :
					#CvUtil.pyPrint( "%s is hurt"%(headUnit.getName()) )
					#pGroup.setActivityType(ActivityTypes.ACTIVITY_HEAL)
					pass
				else :
					#CvUtil.pyPrint( "Starting mission ..." )
					#pGroup.pushMission( MissionTypes.MISSION_FORTIFY, 0, 0, 0, False, True, MissionAITypes.MISSIONAI_GUARD_CITY, pGroup.plot(), pGroup.getHeadUnit() )
					pGroup.setActivityType(ActivityTypes.ACTIVITY_SLEEP)
					headUnit.NotifyEntity( MissionTypes.MISSION_FORTIFY )
					pass