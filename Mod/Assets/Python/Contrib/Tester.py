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
import CvEventManager
import BugOptions
import BugUtil
import ColorUtil

# globals
gc = CyGlobalContext()
game = CyGame()
localText = CyTranslator()

class Tester :

	def __init__(self, customEM):
		print "Initializing Tester"
		self.LOG_DEBUG = True
		customEM.addEventHandler( "kbdEvent", self.onKbdEvent )
		self.customEM = customEM

	def removeEventHandlers( self ) :
		print "Removing event handlers from Tester"
		self.customEM.removeEventHandler( "kbdEvent", self.onKbdEvent )


	def blankHandler( self, playerID, netUserData, popupReturn ) :
		# Dummy handler to take the second event for popup
		return


	def onKbdEvent(self, argsList ):
		'keypress handler'
		eventType,key,mx,my,px,py = argsList

		if ( eventType == 6 ):
			theKey=int(key)
			if( theKey == int(InputTypes.KB_S) and self.customEM.bShift and self.customEM.bCtrl ) :
				self.showStrandedPopup()
			if( theKey == int(InputTypes.KB_Z) and self.customEM.bShift and self.customEM.bCtrl ) :
				self.reloadOptions()
#			if( theKey == int(InputTypes.KB_Q) and self.customEM.bShift and self.customEM.bCtrl ) :
#				self.showTestPopup()


	def showStrandedPopup( self ) :

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

		popup = PyPopup.PyPopup()
		popup.setBodyString(bodStr)
		popup.launch()

	def reloadOptions( self ) :
	
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

"""
	def showTestPopup( self ) :
		ColorUtil.init()
		bodStr = "Enumerating elements\n"
		match = "COLOR_MAGENTA"
		iMatch = ColorUtil.keyToIndex(match)
#		iMatch = -1
#		for i, key in enumerate(ColorUtil.getColorKeys()):
#			bodStr += "\n" + str(i) + " : " + key
#			if match == key:
#				iMatch = i

		if iMatch > -1:
			bodStr += "\n\nMatch found at index: " + str(iMatch)

		popup = PyPopup.PyPopup()
		popup.setBodyString(bodStr)
		popup.launch()
"""