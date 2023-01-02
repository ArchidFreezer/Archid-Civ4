## Sid Meier's Civilization 4
## Copyright Firaxis Games 2006
## 
## CvEventManager
## This class is passed an argsList from CvAppInterface.onEvent
## The argsList can contain anything from mouse location to key info
## The EVENTLIST that are being notified can be found 

from CvPythonExtensions import *
import CvUtil
import BugCore

gc = CyGlobalContext()

def onCityAcquired(argsList):
	'City Acquired'
	iPreviousOwner,iNewOwner,pCity,bConquest,bTrade = argsList
	pNewOwner = gc.getPlayer(iNewOwner)
	if not pNewOwner.isHuman():
		for iSpecialist in xrange(gc.getNumSpecialistInfos()):
			iFreeSpecialistCount = pCity.getFreeSpecialistCount(iSpecialist)
			if iFreeSpecialistCount > 0:
				SpecialistInfo = gc.getSpecialistInfo(iSpecialist)
				if SpecialistInfo.getType().find("GREAT") > -1:
					iUnit = -1
					for i in xrange(gc.getNumUnitInfos()):
						if gc.getUnitInfo(i).getGreatPeoples(iSpecialist):
							iUnit = i
							break
					if iUnit > -1:
						for i in xrange(iFreeSpecialistCount):
							migrate(pCity, iSpecialist, iUnit, 0)

def onModNetMessage(argsList):
	'Called whenever CyMessageControl().sendModNetMessage() is called - this is all for you modders!'
	iData1, iData2, iData3, iData4, iData5 = argsList
	if iData1 == 9413:
		pCity = CyInterface().getHeadSelectedCity()
		pPlayer = gc.getPlayer(pCity.getOwner())
		if pPlayer.isTurnActive():
			iGold = gc.getGameSpeedInfo(CyGame().getGameSpeedType()).getResearchPercent()
			if pPlayer.getGold() >= iGold:
				SpecialistInfo = gc.getSpecialistInfo(iData2)
				if SpecialistInfo.getType().find("GREAT") > -1:
					iUnit = -1
					for i in xrange(gc.getNumUnitInfos()):
						if gc.getUnitInfo(i).getGreatPeoples(iData2):
							iUnit = i
							break
					if iUnit > -1:
						migrate(pCity, iData2, iUnit, iGold)

def migrate(pCity, iSpecialist, iUnit, iGold):
	pCity.changeFreeSpecialistCount(iSpecialist, -1)
	iPlayer = pCity.getOwner()
	pPlayer = gc.getPlayer(iPlayer)
	pPlayer.initUnit(iUnit, pCity.getX(), pCity.getY(), UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
	pPlayer.changeGold(- iGold)

class MigratingGPGameUtils:

	def getWidgetHelp(self,argsList):
		eWidgetType, iData1, iData2, bOption = argsList
		if eWidgetType == WidgetTypes.WIDGET_FREE_CITIZEN:
			SpecialistInfo = gc.getSpecialistInfo(iData1)
			if SpecialistInfo.getType().find("GREAT") > -1:
				iGold = gc.getGameSpeedInfo(CyGame().getGameSpeedType()).getResearchPercent()
				for i in xrange(gc.getNumUnitInfos()):
					if gc.getUnitInfo(i).getGreatPeoples(iData1):
						return CyTranslator().getText("TXT_KEY_EVACUATE_GP",(SpecialistInfo.getDescription(), iGold,))
						break
		return u""
