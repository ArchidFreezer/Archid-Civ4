
from CvPythonExtensions import *
import CvUtil
import BugUtil

gc = CyGlobalContext()
localText = CyTranslator()

# class SanKongEventHandler:
	# def __init__(self, customEM):
				
		# print "Initialising San Kong Mod"
		
		# customEM.addEventHandler( 'buildingBuilt', self.onBuildingBuilt )

def onBuildingBuilt(argsList):
	'Building Completed'
	pCity,iBuildingType = argsList
	
	BugUtil.info("San Kong - 1")

	if (iBuildingType == gc.getInfoTypeForString("BUILDING_CONFUCIAN_SAN_KONG")):

		BugUtil.info("San Kong - 2")

		iPlayer = pCity.getOwner()
		pPlayer = gc.getPlayer(iPlayer)
		iX = pCity.getX()
		iY = pCity.getY()
		iIndex = CyGame().getSorenRandNum(9,"Confucius GP")

		BugUtil.info("San Kong - Rand: %d", iIndex)

		if iIndex == 0:
			iUnitType = gc.getInfoTypeForString("UNIT_PROPHET")
			szMsg = localText.getText("TXT_KEY_SCHOOL_CONFUCIUS_PROPHET",())
		elif iIndex == 1:
			iUnitType = gc.getInfoTypeForString("UNIT_ARTIST")
			szMsg = localText.getText("TXT_KEY_SCHOOL_CONFUCIUS_ARTIST",())
		elif iIndex == 2:
			iUnitType = gc.getInfoTypeForString("UNIT_SCIENTIST")
			szMsg = localText.getText("TXT_KEY_SCHOOL_CONFUCIUS_SCIENTIST",())
		elif iIndex == 3:
			iUnitType = gc.getInfoTypeForString("UNIT_MERCHANT")
			szMsg = localText.getText("TXT_KEY_SCHOOL_CONFUCIUS_MERCHANT",())
		elif iIndex == 4:
			iUnitType = gc.getInfoTypeForString("UNIT_ENGINEER")
			szMsg = localText.getText("TXT_KEY_SCHOOL_CONFUCIUS_ENGINEER",())
		elif iIndex == 5:
			iUnitType = gc.getInfoTypeForString("UNIT_GREAT_GENERAL")
			szMsg = localText.getText("TXT_KEY_SCHOOL_CONFUCIUS_GENERAL",())
		elif iIndex == 6:
			iUnitType = gc.getInfoTypeForString("UNIT_GREAT_SPY")
			szMsg = localText.getText("TXT_KEY_SCHOOL_CONFUCIUS_SPY",())
		elif iIndex == 7:
			iUnitType = gc.getInfoTypeForString("UNIT_DOCTOR")
			szMsg = localText.getText("TXT_KEY_SCHOOL_CONFUCIUS_DOCTOR",())
		elif iIndex == 8:
			iUnitType = gc.getInfoTypeForString("UNIT_GREAT_JESTER")
			szMsg = localText.getText("TXT_KEY_SCHOOL_CONFUCIUS_JESTER",())

		BugUtil.info("San Kong - Unit: %d", iUnitType)

		# Now build the unit
		pUnit = pPlayer.initUnit(iUnitType,iX,iY,UnitAITypes.NO_UNITAI,DirectionTypes.NO_DIRECTION)
		CyInterface().addImmediateMessage(szMsg,None)
