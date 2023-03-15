## Sid Meier's Civilization 4
## Copyright Firaxis Games 2006
## 
## CvEventManager
## This class is passed an argsList from CvAppInterface.onEvent
## The argsList can contain anything from mouse location to key info
## The EVENTLIST that are being notified can be found 

from CvPythonExtensions import *
import BugCore
import BugUtil
import CvUtil

gc = CyGlobalContext()

def doCityCaptureGold(argsList):
	"controls the gold result of capturing a city"
	pOldCity = argsList[0]
	
	iCaptureGold = 0
	
	iCaptureGold += gc.getDefineINT("BASE_CAPTURE_GOLD")
	iCaptureGold += (pOldCity.getPopulation() * gc.getDefineINT("CAPTURE_GOLD_PER_POPULATION"))
	iCaptureGold += CyGame().getSorenRandNum(gc.getDefineINT("CAPTURE_GOLD_RAND1"), "Capture Gold 1")
	iCaptureGold += CyGame().getSorenRandNum(gc.getDefineINT("CAPTURE_GOLD_RAND2"), "Capture Gold 2")

	if (gc.getDefineINT("CAPTURE_GOLD_MAX_TURNS") > 0):
		iCaptureGold *= cyIntRange((CyGame().getGameTurn() - pOldCity.getGameTurnAcquired()), 0, gc.getDefineINT("CAPTURE_GOLD_MAX_TURNS"))
		iCaptureGold /= gc.getDefineINT("CAPTURE_GOLD_MAX_TURNS")

	if pOldCity.getNumActiveBuilding(gc.getInfoTypeForString("BUILDING_NATIONAL_MINT")) > 0:
		iCaptureGold *= 5

	return iCaptureGold