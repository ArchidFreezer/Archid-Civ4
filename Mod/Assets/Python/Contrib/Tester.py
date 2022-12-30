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

TESTER_STRANDED_EVENT = CvUtil.getNewEventID("Tester.StrandedEvent")

# globals
gc = CyGlobalContext()

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

	popup = PyPopup.PyPopup(TESTER_STRANDED_EVENT)
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