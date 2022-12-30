# AI_AUTO_PLAY_MOD
#
# by jdog5000
# version 2.0

from CvPythonExtensions import *
import CvScreenEnums
import CvTopCivs
import CvUtil
import PyHelpers
import Popup as PyPopup
import BugCore

import ChangePlayer

# globals

AI_AUTOPLAY_PICK_TURNS = CvUtil.getNewEventID("AIAutoPlay.PickTurns")
AI_AUTOPLAY_PICK_PLAYER = CvUtil.getNewEventID("AIAutoPlay.PickPlayer")

gc = CyGlobalContext()
PyPlayer = PyHelpers.PyPlayer
PyInfo = PyHelpers.PyInfo
game = CyGame()
localText = CyTranslator()

options = BugCore.game.AIAutoPlay

class AIAutoPlayEventHandler :

	def __init__(self, customEM ) :

		print "Initializing AIAutoPlay Mod"
		
		self.customEM = customEM

		self.customEM.addEventHandler( "EndGameTurn", self.onEndGameTurn )
		self.customEM.addEventHandler( 'BeginPlayerTurn', self.onBeginPlayerTurn )
		self.customEM.addEventHandler( 'EndPlayerTurn', self.onEndPlayerTurn )
		self.customEM.addEventHandler( 'OnLoad', self.onGameLoad )
		self.customEM.addEventHandler( 'GameStart', self.onGameStart )
		self.customEM.addEventHandler( 'victory', self.onVictory )

		self.customEM.setPopupHandler( AI_AUTOPLAY_PICK_TURNS, ["toAIChooserPopup",AIChooserHandler,self.blankHandler] )
		self.customEM.setPopupHandler( AI_AUTOPLAY_PICK_PLAYER, ["pickHumanPopup",pickHumanHandler,self.blankHandler] )

		# Keep game from showing messages about handling these popups
		CvUtil.SilentEvents.extend([AI_AUTOPLAY_PICK_TURNS,AI_AUTOPLAY_PICK_PLAYER])
		
		if( options.isBlockPopups() ) :
			print "Removing some event handlers"
			try :
				self.customEM.removeEventHandler( "cityBuilt", customEM.onCityBuilt )
				self.customEM.addEventHandler( "cityBuilt", self.onCityBuilt )
			except ValueError :
				print "Failed to remove 'onCityBuilt', perhaps not registered"
				self.customEM.setEventHandler( "cityBuilt", self.onCityBuilt )
			
			try :
				self.customEM.removeEventHandler( "BeginGameTurn", customEM.onBeginGameTurn )
				self.customEM.addEventHandler( "BeginGameTurn", self.onBeginGameTurn )
			except ValueError :
				print "Failed to remove 'onBeginGameTurn', perhaps not registered"
				self.customEM.setEventHandler( "BeginGameTurn", self.onBeginGameTurn )


	def blankHandler( self, playerID, netUserData, popupReturn ) :
		# Dummy handler to take the second event for popup
		pass


	def onGameStart( self, argsList ) :
		self.onGameLoad([])

	def onGameLoad( self, argsList ) :
		# Init things which require a game object or other game data to exist
		pass

	def onVictory( self, argsList ) :
		checkPlayer()
		game.setAIAutoPlay(0)
		
	def onEndGameTurn( self, argsList ) :
		if( game.getAIAutoPlay() == 1 ) :
			# About to turn off automation
			pass

	def onBeginPlayerTurn( self, argsList ) :
		iGameTurn, iPlayer = argsList

		if( game.getAIAutoPlay() == 1 and iPlayer > game.getActivePlayer() and gc.getActivePlayer().isAlive() ) :
			# Forces isHuman checks to come through positive for everything after human players turn

			checkPlayer()
			game.setAIAutoPlay(0)
		
		elif( options.isSaveAllDeaths() ) :
			if( game.getAIAutoPlay() == 0 and not gc.getActivePlayer().isAlive() and iPlayer > game.getActivePlayer() ) :
				checkPlayer()
				game.setAIAutoPlay(0)

	def onEndPlayerTurn( self, argsList ) :
		iGameTurn, iPlayer = argsList

		# Can't use isHuman as isHuman has been deactivated by automation
		if( options.isRefortify() and iPlayer == game.getActivePlayer() and game.getAIAutoPlay() == 1 ) :
			doRefortify( game.getActivePlayer() )
		
		if( iPlayer == gc.getBARBARIAN_PLAYER() and game.getAIAutoPlay() == 1 ) :
			# About to turn off automation
			pass

	def onBeginGameTurn( self, argsList):
		'Called at the beginning of the end of each turn'
		iGameTurn = argsList[0]
		if (game.getAIAutoPlay() == 0):
			self.customEM.onBeginGameTurn(argsList)

	def onCityBuilt(self, argsList):
		'City Built'
		city = argsList[0]
		#if (city.getOwner() == CyGame().getActivePlayer() and game.getAIAutoPlay() == 0 ):
		if (game.getAIAutoPlay() == 0): # K-Mod. Some mods may use the event for other player's cities too.
			self.customEM.onCityBuilt(argsList)
		else :
			try :
				CvUtil.pyPrint('City Built Event: %s' %(city.getName()))
			except :
				CvUtil.pyPrint('City Built Event: Error processing city name' )

########################## Keyboard handler functions ###########################################

def haltAutoPlay(argslist):
	if game.getAIAutoPlay():
		game.setAIAutoPlay(0)
		checkPlayer()

def startAutoPlay(argslist):
	if( game.getAIAutoPlay() > 0 ) :
		if( options.isRefortify() ) :
			doRefortify( game.getActivePlayer() )
		game.setAIAutoPlay( 0 )
		checkPlayer()
	else :
		toAIChooser()

def finishMyTurn(argslist):
	# Toggle auto moves
	if(options.isDebug() ) : CyInterface().addImmediateMessage("Moving your units...","")
	game.setAIAutoPlay( 1 )

def refortify(argslist):
	doRefortify( game.getActivePlayer() )

########################## Popup handlers ###########################################

def AIChooserHandler( playerID, netUserData, popupReturn ) :
	'Handles AIChooser popup'
	if( popupReturn.getButtonClicked() == 1 ):  # if you pressed cancel
		return

	numTurns = 0
	if( popupReturn.getEditBoxString(0) != '' ) :
		numTurns = int( popupReturn.getEditBoxString(0) )

	if( numTurns > 0 ) :
		if( options.isDebug() ) : CyInterface().addImmediateMessage("Fully automating for %d turns"%(numTurns),"")
		game.setAIAutoPlay(numTurns)

def pickHumanHandler( iPlayerID, netUserData, popupReturn ) :

	CvUtil.pyPrint('Handling pick human popup')

	if( popupReturn.getButtonClicked() == 0 ):  # if you pressed cancel
		CyInterface().addImmediateMessage("Kill your remaining units if you'd like to see end game screens","")
		return

	toKillPlayer = gc.getActivePlayer()
	newHumanIdx = popupReturn.getSelectedPullDownValue( 1 )
	newPlayer = gc.getPlayer(newHumanIdx)
	ChangePlayer.changeHuman( newHumanIdx, toKillPlayer.getID() )

	if( toKillPlayer.getNumCities() == 0 ) :
		# Kills off the lion in the ice field
		CvUtil.pyPrint("Killing off player %d"%(toKillPlayer.getID()))
		toKillPlayer.killUnits()
		toKillPlayer.setIsHuman(False)


########################## Utility functions ###########################################
			
def toAIChooser() :
	'Chooser window for when user switches to AI auto play'

	screen = CyGInterfaceScreen( "MainInterface", CvScreenEnums.MAIN_INTERFACE )
	xResolution = screen.getXResolution()
	yResolution = screen.getYResolution()
	popupSizeX = 400
	popupSizeY = 250

	popup = PyPopup.PyPopup(AI_AUTOPLAY_PICK_TURNS,contextType = EventContextTypes.EVENTCONTEXT_ALL)
	popup.setPosition((xResolution - popupSizeX )/2, (yResolution-popupSizeY)/2-50)
	popup.setSize(popupSizeX,popupSizeY)
	popup.setHeaderString( localText.getText("TXT_KEY_AIAUTOPLAY_TURN_ON", ()) )
	popup.setBodyString( localText.getText("TXT_KEY_AIAUTOPLAY_TURNS", ()) )
	popup.addSeparator()
	popup.createPythonEditBox( '%d'%(options.getDefaultTurnsToAuto()), 'Number of turns to turn over to AI', 0)
	popup.setEditBoxMaxCharCount( 4, 2, 0 )

	popup.addSeparator()
	popup.addButton("OK")
	popup.addButton(localText.getText("TXT_KEY_AIAUTOPLAY_CANCEL", ()))

	popup.launch(False, PopupStates.POPUPSTATE_IMMEDIATE)

def checkPlayer() :
	
	pPlayer = gc.getActivePlayer()

	if( not pPlayer.isAlive() ) :
		popup = PyPopup.PyPopup(AI_AUTOPLAY_PICK_PLAYER,contextType = EventContextTypes.EVENTCONTEXT_ALL)
		popup.setHeaderString( localText.getText("TXT_KEY_AIAUTOPLAY_PICK_CIV", ()) )
		popup.setBodyString( localText.getText("TXT_KEY_AIAUTOPLAY_CIV_DIED", ()) )
		popup.addSeparator()

		popup.createPythonPullDown( localText.getText("TXT_KEY_AIAUTOPLAY_TAKE_CONTROL_CIV", ()), 1 )
		for i in range(0,gc.getMAX_CIV_PLAYERS()) :
			player = PyPlayer(i)
			if( not player.isNone() and not i == pPlayer.getID() ) :
				if( player.isAlive() ) :
					popup.addPullDownString( localText.getText("TXT_KEY_AIAUTOPLAY_OF_THE", ())%(player.getName(),player.getCivilizationName()), i, 1 )

		activePlayerIdx = gc.getActivePlayer().getID()
		popup.popup.setSelectedPulldownID( activePlayerIdx, 1 )

		popup.addSeparator()

		popup.addButton( localText.getText("TXT_KEY_AIAUTOPLAY_NONE", ()) )
		CvUtil.pyPrint('Launching pick human popup')
		popup.launch()

		iSettler = CvUtil.findInfoTypeNum(gc.getUnitInfo,gc.getNumUnitInfos(),'UNIT_SETTLER')
		gc.getActivePlayer().initUnit( iSettler, 0, 0, UnitAITypes.NO_UNITAI, DirectionTypes.DIRECTION_SOUTH )
		gc.getActivePlayer().setIsHuman( True )

	CvUtil.pyPrint('CDP: Setting autoplay to 0')
	game.setAIAutoPlay(0)
	
	if( not pPlayer.isHuman() ) :
		CvUtil.pyPrint('Returning human player to control of %s'%(pPlayer.getCivilizationDescription(0)))
		game.setActivePlayer( pPlayer.getID(), False )
		pPlayer.setIsHuman( True )

def doRefortify( iPlayer ) :
	 #pyPlayer = PyPlayer( iPlayer )
	pPlayer = gc.getPlayer(iPlayer)
	
	CvUtil.pyPrint( "Refortifying units for player %d"%(iPlayer))

	for groupID in range(0,pPlayer.getNumSelectionGroups()) :
		pGroup = pPlayer.getSelectionGroup(groupID)
		if( pGroup.getNumUnits() > 0 ) :

			headUnit = pGroup.getHeadUnit()
			#CvUtil.pyPrint( "%s fortTurns %d"%(headUnit.getName(),headUnit.getFortifyTurns()) )
			if( headUnit.getFortifyTurns() > 0 and headUnit.getDomainType() == DomainTypes.DOMAIN_LAND ) :
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
