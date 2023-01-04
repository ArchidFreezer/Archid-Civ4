//---------------------------------------------------------------------------------------
//
//  *****************   Civilization IV   ********************
//
//  FILE:    CvGameTextMgr.cpp
//
//  PURPOSE: Interfaces with GameText XML Files to manage the paths of art files
//
//---------------------------------------------------------------------------------------
//  Copyright (c) 2004 Firaxis Games, Inc. All rights reserved.
//---------------------------------------------------------------------------------------

#include "CvGameCoreDLL.h"
#include "CvGameTextMgr.h"
#include "CvGameCoreUtils.h"
#include "CvDLLUtilityIFaceBase.h"
#include "CvDLLInterfaceIFaceBase.h"
#include "CvDLLSymbolIFaceBase.h"
#include "CvInfos.h"
#include "CvXMLLoadUtility.h"
#include "CvCity.h"
#include "CvPlayerAI.h"
#include "CvTeamAI.h"
#include "CvGameAI.h"
#include "CvSelectionGroup.h"
#include "CvMap.h"
#include "CvArea.h"
#include "CvPlot.h"
#include "CvPopupInfo.h"
#include "FProfiler.h"
#include "CyArgsList.h"
#include "CvDLLPythonIFaceBase.h"
#include "CvIniOptions.h"

int shortenID(int iId) {
	return iId;
}

// For displaying Asserts and error messages
static char* szErrorMsg;

//----------------------------------------------------------------------------
//
//	FUNCTION:	GetInstance()
//
//	PURPOSE:	Get the instance of this class.
//
//----------------------------------------------------------------------------
CvGameTextMgr& CvGameTextMgr::GetInstance() {
	static CvGameTextMgr gs_GameTextMgr;
	return gs_GameTextMgr;
}

//----------------------------------------------------------------------------
//
//	FUNCTION:	CvGameTextMgr()
//
//	PURPOSE:	Constructor
//
//----------------------------------------------------------------------------
CvGameTextMgr::CvGameTextMgr() {

}

CvGameTextMgr::~CvGameTextMgr() {
}

//----------------------------------------------------------------------------
//
//	FUNCTION:	Initialize()
//
//	PURPOSE:	Allocates memory
//
//----------------------------------------------------------------------------
void CvGameTextMgr::Initialize() {

}

//----------------------------------------------------------------------------
//
//	FUNCTION:	DeInitialize()
//
//	PURPOSE:	Clears memory
//
//----------------------------------------------------------------------------
void CvGameTextMgr::DeInitialize() {
	for (int i = 0; i < (int)m_apbPromotion.size(); i++) {
		delete[] m_apbPromotion[i];
	}
}

//----------------------------------------------------------------------------
//
//	FUNCTION:	Reset()
//
//	PURPOSE:	Accesses CvXMLLoadUtility to clean global text memory and
//				reload the XML files
//
//----------------------------------------------------------------------------
void CvGameTextMgr::Reset() {
	CvXMLLoadUtility pXML;
	pXML.LoadGlobalText();
}


// Returns the current language
int CvGameTextMgr::getCurrentLanguage() {
	return gDLL->getCurrentLanguage();
}

void CvGameTextMgr::setYearStr(CvWString& szString, int iGameTurn, bool bSave, CalendarTypes eCalendar, int iStartYear, GameSpeedTypes eSpeed) {
	int iTurnYear = getTurnYearForGame(iGameTurn, iStartYear, eCalendar, eSpeed);

	if (iTurnYear < 0) {
		if (bSave) {
			szString = gDLL->getText("TXT_KEY_TIME_BC_SAVE", CvWString::format(L"%04d", -iTurnYear).GetCString());
		} else {
			szString = gDLL->getText("TXT_KEY_TIME_BC", -(iTurnYear));
		}
	} else if (iTurnYear > 0) {
		if (bSave) {
			szString = gDLL->getText("TXT_KEY_TIME_AD_SAVE", CvWString::format(L"%04d", iTurnYear).GetCString());
		} else {
			szString = gDLL->getText("TXT_KEY_TIME_AD", iTurnYear);
		}
	} else {
		if (bSave) {
			szString = gDLL->getText("TXT_KEY_TIME_AD_SAVE", L"0001");
		} else {
			szString = gDLL->getText("TXT_KEY_TIME_AD", 1);
		}
	}
}


void CvGameTextMgr::setDateStr(CvWString& szString, int iGameTurn, bool bSave, CalendarTypes eCalendar, int iStartYear, GameSpeedTypes eSpeed) {
	CvWString szYearBuffer;
	setYearStr(szYearBuffer, iGameTurn, bSave, eCalendar, iStartYear, eSpeed);

	switch (eCalendar) {
	case CALENDAR_DEFAULT:
		if (0 == (getTurnMonthForGame(iGameTurn + 1, iStartYear, eCalendar, eSpeed) - getTurnMonthForGame(iGameTurn, iStartYear, eCalendar, eSpeed)) % GC.getNumMonthInfos()) {
			szString = szYearBuffer;
		} else {
			int iMonth = getTurnMonthForGame(iGameTurn, iStartYear, eCalendar, eSpeed);
			if (bSave) {
				szString = (szYearBuffer + "-" + GC.getMonthInfo((MonthTypes)(iMonth % GC.getNumMonthInfos())).getDescription());
			} else {
				szString = (GC.getMonthInfo((MonthTypes)(iMonth % GC.getNumMonthInfos())).getDescription() + CvString(", ") + szYearBuffer);
			}
		}
		break;
	case CALENDAR_YEARS:
	case CALENDAR_BI_YEARLY:
		szString = szYearBuffer;
		break;

	case CALENDAR_TURNS:
		szString = gDLL->getText("TXT_KEY_TIME_TURN", (iGameTurn + 1));
		break;

	case CALENDAR_SEASONS:
		if (bSave) {
			szString = (szYearBuffer + "-" + GC.getSeasonInfo((SeasonTypes)(iGameTurn % GC.getNumSeasonInfos())).getDescription());
		} else {
			szString = (GC.getSeasonInfo((SeasonTypes)(iGameTurn % GC.getNumSeasonInfos())).getDescription() + CvString(", ") + szYearBuffer);
		}
		break;

	case CALENDAR_MONTHS:
		if (bSave) {
			szString = (szYearBuffer + "-" + GC.getMonthInfo((MonthTypes)(iGameTurn % GC.getNumMonthInfos())).getDescription());
		} else {
			szString = (GC.getMonthInfo((MonthTypes)(iGameTurn % GC.getNumMonthInfos())).getDescription() + CvString(", ") + szYearBuffer);
		}
		break;

	case CALENDAR_WEEKS:
	{
		CvWString szWeekBuffer = gDLL->getText("TXT_KEY_TIME_WEEK", ((iGameTurn % GC.getDefineINT("WEEKS_PER_MONTHS")) + 1));

		if (bSave) {
			szString = (szYearBuffer + "-" + GC.getMonthInfo((MonthTypes)((iGameTurn / GC.getDefineINT("WEEKS_PER_MONTHS")) % GC.getNumMonthInfos())).getDescription() + "-" + szWeekBuffer);
		} else {
			szString = (szWeekBuffer + ", " + GC.getMonthInfo((MonthTypes)((iGameTurn / GC.getDefineINT("WEEKS_PER_MONTHS")) % GC.getNumMonthInfos())).getDescription() + ", " + szYearBuffer);
		}
	}
	break;

	default:
		FAssert(false);
	}
}


void CvGameTextMgr::setTimeStr(CvWString& szString, int iGameTurn, bool bSave) {
	setDateStr(szString, iGameTurn, bSave, GC.getGameINLINE().getCalendar(), GC.getGameINLINE().getStartYear(), GC.getGameINLINE().getGameSpeedType());
}


void CvGameTextMgr::setInterfaceTime(CvWString& szString, PlayerTypes ePlayer) {
	if (GET_PLAYER(ePlayer).isGoldenAge()) {
		szString.Format(L"%c(%d) ", gDLL->getSymbolID(GOLDEN_AGE_CHAR), GET_PLAYER(ePlayer).getGoldenAgeTurns());
	} else {
		szString.clear();
	}

	CvWString szTempBuffer;
	setTimeStr(szTempBuffer, GC.getGameINLINE().getGameTurn(), false);
	szString += CvWString(szTempBuffer);
}


void CvGameTextMgr::setGoldStr(CvWString& szString, PlayerTypes ePlayer) {
	if (GET_PLAYER(ePlayer).getGold() < 0) {
		szString.Format(L"%c: " SETCOLR L"%d" SETCOLR, GC.getCommerceInfo(COMMERCE_GOLD).getChar(), TEXT_COLOR("COLOR_NEGATIVE_TEXT"), GET_PLAYER(ePlayer).getGold());
	} else {
		szString.Format(L"%c: %d", GC.getCommerceInfo(COMMERCE_GOLD).getChar(), GET_PLAYER(ePlayer).getGold());
	}

	int iGoldRate = GET_PLAYER(ePlayer).calculateGoldRate();
	if (iGoldRate < 0) {
		szString += gDLL->getText("TXT_KEY_MISC_NEG_GOLD_PER_TURN", iGoldRate);
	} else if (iGoldRate > 0) {
		szString += gDLL->getText("TXT_KEY_MISC_POS_GOLD_PER_TURN", iGoldRate);
	}

	if (GET_PLAYER(ePlayer).isStrike()) {
		szString += gDLL->getText("TXT_KEY_MISC_STRIKE");
	}
}


void CvGameTextMgr::setResearchStr(CvWString& szString, PlayerTypes ePlayer) {
	szString = gDLL->getText("TXT_KEY_MISC_RESEARCH_STRING", GC.getTechInfo(GET_PLAYER(ePlayer).getCurrentResearch()).getTextKeyWide());

	CvWString szTempBuffer;
	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getTechCount(GET_PLAYER(ePlayer).getCurrentResearch()) > 0) {
		szTempBuffer.Format(L" %d", (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getTechCount(GET_PLAYER(ePlayer).getCurrentResearch()) + 1));
		szString += szTempBuffer;
	}

	szTempBuffer.Format(L" (%d)", GET_PLAYER(ePlayer).getResearchTurnsLeft(GET_PLAYER(ePlayer).getCurrentResearch(), true));
	szString += szTempBuffer;
}


void CvGameTextMgr::setOOSSeeds(CvWString& szString, PlayerTypes ePlayer) {
	if (GET_PLAYER(ePlayer).isHuman()) {
		int iNetID = GET_PLAYER(ePlayer).getNetID();
		if (gDLL->isConnected(iNetID)) {
			szString = gDLL->getText("TXT_KEY_PLAYER_OOS", gDLL->GetSyncOOS(iNetID), gDLL->GetOptionsOOS(iNetID));
		}
	}
}

void CvGameTextMgr::setNetStats(CvWString& szString, PlayerTypes ePlayer) {
	if (ePlayer != GC.getGameINLINE().getActivePlayer()) {
		if (GET_PLAYER(ePlayer).isHuman()) {
			if (gDLL->getInterfaceIFace()->isNetStatsVisible()) {
				int iNetID = GET_PLAYER(ePlayer).getNetID();
				if (gDLL->isConnected(iNetID)) {
					szString = gDLL->getText("TXT_KEY_MISC_NUM_MS", gDLL->GetLastPing(iNetID));
				} else {
					szString = gDLL->getText("TXT_KEY_MISC_DISCONNECTED");
				}
			}
		} else {
			szString = gDLL->getText("TXT_KEY_MISC_AI");
		}
	}
}


void CvGameTextMgr::setMinimizePopupHelp(CvWString& szString, const CvPopupInfo& info) {
	switch (info.getButtonPopupType()) {
	case BUTTONPOPUP_CHOOSEPRODUCTION:
	{
		CvCity* pCity = GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getCity(info.getData1());
		if (pCity != NULL) {
			UnitTypes eTrainUnit = NO_UNIT;
			BuildingTypes eConstructBuilding = NO_BUILDING;
			ProjectTypes eCreateProject = NO_PROJECT;

			switch (info.getData2()) {
			case (ORDER_TRAIN):
				eTrainUnit = (UnitTypes)info.getData3();
				break;
			case (ORDER_CONSTRUCT):
				eConstructBuilding = (BuildingTypes)info.getData3();
				break;
			case (ORDER_CREATE):
				eCreateProject = (ProjectTypes)info.getData3();
				break;
			default:
				break;
			}

			if (eTrainUnit != NO_UNIT) {
				szString += gDLL->getText("TXT_KEY_MINIMIZED_CHOOSE_PRODUCTION_UNIT", GC.getUnitInfo(eTrainUnit).getTextKeyWide(), pCity->getNameKey());
			} else if (eConstructBuilding != NO_BUILDING) {
				szString += gDLL->getText("TXT_KEY_MINIMIZED_CHOOSE_PRODUCTION_BUILDING", GC.getBuildingInfo(eConstructBuilding).getTextKeyWide(), pCity->getNameKey());
			} else if (eCreateProject != NO_PROJECT) {
				szString += gDLL->getText("TXT_KEY_MINIMIZED_CHOOSE_PRODUCTION_PROJECT", GC.getProjectInfo(eCreateProject).getTextKeyWide(), pCity->getNameKey());
			} else {
				szString += gDLL->getText("TXT_KEY_MINIMIZED_CHOOSE_PRODUCTION", pCity->getNameKey());
			}
		}
	}
	break;

	case BUTTONPOPUP_CHANGERELIGION:
	{
		ReligionTypes eReligion = ((ReligionTypes)(info.getData1()));
		if (eReligion != NO_RELIGION) {
			szString += gDLL->getText("TXT_KEY_MINIMIZED_CHANGE_RELIGION", GC.getReligionInfo(eReligion).getTextKeyWide());
		}
	}
	break;

	case BUTTONPOPUP_CHOOSETECH:
		if (info.getData1() > 0) {
			szString += gDLL->getText("TXT_KEY_MINIMIZED_CHOOSE_TECH_FREE");
		} else {
			szString += gDLL->getText("TXT_KEY_MINIMIZED_CHOOSE_TECH");
		}
		break;

	case BUTTONPOPUP_CHANGECIVIC:
	{
		CivicTypes eCivic = ((CivicTypes)(info.getData2()));
		if (eCivic != NO_CIVIC) {
			szString += gDLL->getText("TXT_KEY_MINIMIZED_CHANGE_CIVIC", GC.getCivicInfo(eCivic).getTextKeyWide());
		}
	}
	break;
	}
}

void CvGameTextMgr::setEspionageMissionHelp(CvWStringBuffer& szBuffer, const CvUnit* pUnit) {
	if (pUnit->isSpy()) {
		PlayerTypes eOwner = pUnit->plot()->getOwnerINLINE();
		if (NO_PLAYER != eOwner) {
			const CvPlayer& kOwner = GET_PLAYER(eOwner);
			if (kOwner.getTeam() != pUnit->getTeam()) {
				if (!pUnit->canEspionage(pUnit->plot())) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_UNIT_HELP_NO_ESPIONAGE"));

					if (pUnit->hasMoved() || pUnit->isMadeAttack()) {
						szBuffer.append(gDLL->getText("TXT_KEY_UNIT_HELP_NO_ESPIONAGE_REASON_MOVED"));
					} else if (!pUnit->isInvisible(kOwner.getTeam(), false)) {
						szBuffer.append(gDLL->getText("TXT_KEY_UNIT_HELP_NO_ESPIONAGE_REASON_VISIBLE", kOwner.getNameKey()));
					}
				} else if (pUnit->getFortifyTurns() > 0) {
					int iModifier = -(pUnit->getFortifyTurns() * GC.getDefineINT("ESPIONAGE_EACH_TURN_UNIT_COST_DECREASE"));
					if (0 != iModifier) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_COST", iModifier));
					}
				}
			}
		}
	}
}


void CvGameTextMgr::setUnitHelp(CvWStringBuffer& szString, const CvUnit* pUnit, bool bOneLine, bool bShort) {
	PROFILE_FUNC();

	bool bShift = GC.shiftKey();
	bool bAlt = GC.altKey();

	CvWString szTempBuffer;
	szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_UNIT_TEXT"), pUnit->getName().GetCString());
	szString.append(szTempBuffer);

	if (pUnit->getHomeCity() != NULL && !pUnit->getUnitInfo().isHiddenNationality()) {
		szTempBuffer.Format(SETCOLR L" (%s)" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), pUnit->getHomeCity()->getName().GetCString());
		szString.append(szTempBuffer);
	}

	if (!pUnit->isEnabled()) {
		szTempBuffer.Format(L", %c", gDLL->getSymbolID(UNHAPPY_CHAR));
		szString.append(szTempBuffer);
	}

	szString.append(L", ");

	if (pUnit->getDomainType() == DOMAIN_AIR) {
		if (pUnit->airBaseCombatStr() > 0) {
			if (pUnit->isFighting()) {
				szTempBuffer.Format(L"?/%d%c, ", pUnit->airBaseCombatStr(), gDLL->getSymbolID(STRENGTH_CHAR));
			} else if (pUnit->isHurt()) {
				szTempBuffer.Format(L"%.1f/%d%c, ", (((float)(pUnit->airBaseCombatStr() * pUnit->currHitPoints())) / ((float)(pUnit->maxHitPoints()))), pUnit->airBaseCombatStr(), gDLL->getSymbolID(STRENGTH_CHAR));
			} else {
				szTempBuffer.Format(L"%d%c, ", pUnit->airBaseCombatStr(), gDLL->getSymbolID(STRENGTH_CHAR));
			}
			szString.append(szTempBuffer);
		}
	} else {
		if (pUnit->canFight()) {
			if (pUnit->isFighting()) {
				szTempBuffer.Format(L"?/%d%c, ", pUnit->baseCombatStr(), gDLL->getSymbolID(STRENGTH_CHAR));
			} else if (pUnit->isHurt()) {
				szTempBuffer.Format(L"%.1f/%d%c, ", (((float)(pUnit->baseCombatStr() * pUnit->currHitPoints())) / ((float)(pUnit->maxHitPoints()))), pUnit->baseCombatStr(), gDLL->getSymbolID(STRENGTH_CHAR));
			} else {
				szTempBuffer.Format(L"%d%c, ", pUnit->baseCombatStr(), gDLL->getSymbolID(STRENGTH_CHAR));
			}
			szString.append(szTempBuffer);
		}
	}

	int iCurrMoves = ((pUnit->movesLeft() / GC.getMOVE_DENOMINATOR()) + (((pUnit->movesLeft() % GC.getMOVE_DENOMINATOR()) > 0) ? 1 : 0));
	if ((pUnit->baseMoves() == iCurrMoves) || (pUnit->getTeam() != GC.getGameINLINE().getActiveTeam())) {
		szTempBuffer.Format(L"%d%c", pUnit->baseMoves(), gDLL->getSymbolID(MOVES_CHAR));
	} else {
		szTempBuffer.Format(L"%d/%d%c", iCurrMoves, pUnit->baseMoves(), gDLL->getSymbolID(MOVES_CHAR));
	}
	szString.append(szTempBuffer);

	switch (pUnit->getRangeType()) {
	case UNITRANGE_HOME:
		szTempBuffer.Format(L", %c", gDLL->getSymbolID(HOME_BOUND_CHAR));
		szString.append(szTempBuffer);
		break;
	case UNITRANGE_TERRITORY:
		szTempBuffer.Format(L", %c", gDLL->getSymbolID(TERRITORY_BOUND_CHAR));
		szString.append(szTempBuffer);
		break;
	case UNITRANGE_RANGE:
		szTempBuffer.Format(L", %d %c", pUnit->getRange(), gDLL->getSymbolID(RANGE_BOUND_CHAR));
		szString.append(szTempBuffer);
		break;
	case UNITRANGE_UNLIMITED:
	default:
		break;
	}

	if (pUnit->airRange() > 0) {
		szString.append(gDLL->getText("TXT_KEY_UNIT_HELP_AIR_RANGE", pUnit->airRange()));
	}

	BuildTypes eBuild = pUnit->getBuildType();
	if (eBuild != NO_BUILD) {
		szString.append(L", ");
		szTempBuffer.Format(L"%s (%d)", GC.getBuildInfo(eBuild).getDescription(), pUnit->plot()->getBuildTurnsLeft(eBuild, 0, 0));
		szString.append(szTempBuffer);
	}

	if (pUnit->getImmobileTimer() > 0) {
		szString.append(L", ");
		szString.append(gDLL->getText("TXT_KEY_UNIT_HELP_IMMOBILE", pUnit->getImmobileTimer()));
	}

	if (GC.getGameINLINE().isDebugMode() && !bAlt && !bShift) {
		FAssertMsg(pUnit->AI_getUnitAIType() != NO_UNITAI, "pUnit's AI type expected to != NO_UNITAI");
		szTempBuffer.Format(L" (%s)", GC.getUnitAIInfo(pUnit->AI_getUnitAIType()).getDescription());
		szString.append(szTempBuffer);
	}

	if ((pUnit->getTeam() == GC.getGameINLINE().getActiveTeam()) || GC.getGameINLINE().isDebugMode()) {
		if ((pUnit->getExperience100() > 0) && !(pUnit->isFighting())) {
			float fValue = (float)pUnit->getExperience100();
			if (fmod(fValue, 100) == 0) {
				szTempBuffer.Format(L"%.0f", fValue / 100);
			} else if (fmod(fValue, 10) == 0) {
				szTempBuffer.Format(L"%.1f", fValue / 100);
			} else {
				szTempBuffer.Format(L"%.2f", fValue / 100);
			}
			szString.append(gDLL->getText("TXT_KEY_UNIT_HELP_LEVEL", szTempBuffer.GetCString(), pUnit->experienceNeeded()));
		}
	}

	// Show slaver info in popup
	if (pUnit->getMaxSlaves() > 0) {
		szString.append(gDLL->getText("TXT_KEY_UNIT_HELP_SLAVE_COUNT", pUnit->getSlaveCountTotal(), pUnit->getMaxSlaves()));
	}

	// Show city slave type
	if (pUnit->isSlave()) {
		for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
			if (pUnit->getSlaveSpecialistType() == eSpecialist) {
				szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_UNIT_CAN_JOIN").c_str());
				CvWString szSpecialistLink = CvWString::format(L"<link=literal>%s</link>", GC.getSpecialistInfo(eSpecialist).getDescription());
				setListHelp(szString, szTempBuffer, szSpecialistLink.GetCString(), L", ", true);
				break;
			}
		}
	}

	if (pUnit->getOwnerINLINE() != GC.getGameINLINE().getActivePlayer() && !pUnit->isAnimal() && !pUnit->getUnitInfo().isHiddenNationality()) {
		szString.append(L", ");
		szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, GET_PLAYER(pUnit->getOwnerINLINE()).getPlayerTextColorR(), GET_PLAYER(pUnit->getOwnerINLINE()).getPlayerTextColorG(), GET_PLAYER(pUnit->getOwnerINLINE()).getPlayerTextColorB(), GET_PLAYER(pUnit->getOwnerINLINE()).getPlayerTextColorA(), GET_PLAYER(pUnit->getOwnerINLINE()).getName());
		szString.append(szTempBuffer);
	}

	for (PromotionTypes ePromotion = (PromotionTypes)0; ePromotion < GC.getNumPromotionInfos(); ePromotion = (PromotionTypes)(ePromotion + 1)) {
		if (pUnit->isHasPromotion(ePromotion)) {
			szTempBuffer.Format(L"<img=%S size=16></img>", GC.getPromotionInfo(ePromotion).getButton());
			szString.append(szTempBuffer);
		}
	}
	if (bAlt && (gDLL->getChtLvl() > 0)) {
		CvSelectionGroup* eGroup = pUnit->getGroup();
		if (eGroup != NULL) {
			if (pUnit->isGroupHead())
				szString.append(CvWString::format(L"\nLeading "));
			else
				szString.append(L"\n");

			szTempBuffer.Format(L"Group(%d), %d units", eGroup->getID(), eGroup->getNumUnits());
			szString.append(szTempBuffer);
		}
	}

	if (!bOneLine) {
		setEspionageMissionHelp(szString, pUnit);

		if (pUnit->cargoSpace() > 0) {
			if (pUnit->getTeam() == GC.getGameINLINE().getActiveTeam()) {
				szTempBuffer = NEWLINE + gDLL->getText("TXT_KEY_UNIT_HELP_CARGO_SPACE", pUnit->getCargo(), pUnit->cargoSpace());
			} else {
				szTempBuffer = NEWLINE + gDLL->getText("TXT_KEY_UNIT_CARGO_SPACE", pUnit->cargoSpace());
			}
			szString.append(szTempBuffer);

			if (pUnit->specialCargo() != NO_SPECIALUNIT) {
				szString.append(gDLL->getText("TXT_KEY_UNIT_CARRIES", GC.getSpecialUnitInfo(pUnit->specialCargo()).getTextKeyWide()));
			}
		}

		if (pUnit->fortifyModifier() != 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_UNIT_HELP_FORTIFY_BONUS", pUnit->fortifyModifier()));
		}

		if (!bShort) {
			if (pUnit->nukeRange() >= 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_CAN_NUKE"));
			}

			if (pUnit->alwaysInvisible()) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_INVISIBLE_ALL"));
			} else if (pUnit->getInvisibleType() != NO_INVISIBLE) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_INVISIBLE_MOST"));
			}

			for (int iInvisible = 0; iInvisible < pUnit->getNumSeeInvisibleTypes(); ++iInvisible) {
				if (pUnit->getSeeInvisibleType(iInvisible) != pUnit->getInvisibleType()) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_SEE_INVISIBLE", GC.getInvisibleInfo(pUnit->getSeeInvisibleType(iInvisible)).getTextKeyWide()));
				}
			}

			if (pUnit->canMoveImpassable()) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_CAN_MOVE_IMPASSABLE"));
			}
		}

		if (pUnit->maxFirstStrikes() > 0) {
			if (pUnit->firstStrikes() == pUnit->maxFirstStrikes()) {
				if (pUnit->firstStrikes() == 1) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_ONE_FIRST_STRIKE"));
				} else {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_NUM_FIRST_STRIKES", pUnit->firstStrikes()));
				}
			} else {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_FIRST_STRIKE_CHANCES", pUnit->firstStrikes(), pUnit->maxFirstStrikes()));
			}
		}

		if (pUnit->immuneToFirstStrikes()) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_UNIT_IMMUNE_FIRST_STRIKES"));
		}

		if (!bShort) {
			if (pUnit->noDefensiveBonus()) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_NO_DEFENSE_BONUSES"));
			}

			if (pUnit->flatMovementCost()) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_FLAT_MOVEMENT"));
			}

			if (pUnit->ignoreTerrainCost()) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_IGNORE_TERRAIN"));
			}

			if (pUnit->isBlitz()) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_BLITZ_TEXT"));
			}

			if (pUnit->isAmphib()) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_AMPHIB_TEXT"));
			}

			if (pUnit->isRiver()) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_RIVER_ATTACK_TEXT"));
			}

			if (pUnit->isEnemyRoute()) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_ENEMY_ROADS_TEXT"));
			}

			if (pUnit->isAlwaysHeal()) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_ALWAYS_HEAL_TEXT"));
			}

			if (pUnit->isHillsDoubleMove()) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_HILLS_MOVE_TEXT"));
			}

			if (pUnit->isCanMovePeaks()) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_CAN_MOVE_PEAKS_TEXT"));
			}

			for (TerrainTypes eTerrain = (TerrainTypes)0; eTerrain < GC.getNumTerrainInfos(); eTerrain = (TerrainTypes)(eTerrain + 1)) {
				if (pUnit->isTerrainDoubleMove(eTerrain)) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_PROMOTION_DOUBLE_MOVE_TEXT", GC.getTerrainInfo(eTerrain).getTextKeyWide()));
				}
			}

			for (FeatureTypes eFeature = (FeatureTypes)0; eFeature < GC.getNumFeatureInfos(); eFeature = (FeatureTypes)(eFeature + 1)) {
				if (pUnit->isFeatureDoubleMove(eFeature)) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_PROMOTION_DOUBLE_MOVE_TEXT", GC.getFeatureInfo(eFeature).getTextKeyWide()));
				}
			}

			if (pUnit->getExtraVisibilityRange() != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_VISIBILITY_TEXT", pUnit->getExtraVisibilityRange()));
			}

			if (pUnit->getExtraMoveDiscount() != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_MOVE_DISCOUNT_TEXT", -(pUnit->getExtraMoveDiscount())));
			}

			if (pUnit->getExtraEnemyHeal() != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_HEALS_EXTRA_TEXT", pUnit->getExtraEnemyHeal()) + gDLL->getText("TXT_KEY_PROMOTION_ENEMY_LANDS_TEXT"));
			}

			if (pUnit->getExtraNeutralHeal() != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_HEALS_EXTRA_TEXT", pUnit->getExtraNeutralHeal()) + gDLL->getText("TXT_KEY_PROMOTION_NEUTRAL_LANDS_TEXT"));
			}

			if (pUnit->getExtraFriendlyHeal() != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_HEALS_EXTRA_TEXT", pUnit->getExtraFriendlyHeal()) + gDLL->getText("TXT_KEY_PROMOTION_FRIENDLY_LANDS_TEXT"));
			}

			if (pUnit->getSameTileHeal() != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_HEALS_SAME_TEXT", pUnit->getSameTileHeal()) + gDLL->getText("TXT_KEY_PROMOTION_DAMAGE_TURN_TEXT"));
			}

			if (pUnit->getAdjacentTileHeal() != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_HEALS_ADJACENT_TEXT", pUnit->getAdjacentTileHeal()) + gDLL->getText("TXT_KEY_PROMOTION_DAMAGE_TURN_TEXT"));
			}
		}

		if (pUnit->currInterceptionProbability() > 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_UNIT_INTERCEPT_AIRCRAFT", pUnit->currInterceptionProbability()));
		}

		if (pUnit->evasionProbability() > 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_UNIT_EVADE_INTERCEPTION", pUnit->evasionProbability()));
		}

		if (pUnit->withdrawalProbability() > 0) {
			if (bShort) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_WITHDRAWL_PROBABILITY_SHORT", pUnit->withdrawalProbability()));
			} else {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_WITHDRAWL_PROBABILITY", pUnit->withdrawalProbability()));
			}
		}

		if (pUnit->combatLimit() < GC.getMAX_HIT_POINTS() && pUnit->canAttack()) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_UNIT_COMBAT_LIMIT", (100 * pUnit->combatLimit()) / GC.getMAX_HIT_POINTS()));
		}

		if (pUnit->collateralDamage() > 0) {
			szString.append(NEWLINE);
			if (pUnit->getExtraCollateralDamage() == 0) {
				szString.append(gDLL->getText("TXT_KEY_UNIT_COLLATERAL_DAMAGE", (100 * pUnit->getUnitInfo().getCollateralDamageLimit() / GC.getMAX_HIT_POINTS())));
			} else {
				szString.append(gDLL->getText("TXT_KEY_UNIT_COLLATERAL_DAMAGE_EXTRA", pUnit->getExtraCollateralDamage()));
			}
		}

		for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
			if (pUnit->getUnitInfo().getUnitCombatCollateralImmune(eUnitCombat)) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_COLLATERAL_IMMUNE", GC.getUnitCombatInfo(eUnitCombat).getTextKeyWide()));
			}
		}

		if (pUnit->getCollateralDamageProtection() > 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_PROMOTION_COLLATERAL_PROTECTION_TEXT", pUnit->getCollateralDamageProtection()));
		}

		if (pUnit->getExtraCombatPercent() != 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_PROMOTION_STRENGTH_TEXT", pUnit->getExtraCombatPercent()));
		}

		if (pUnit->cityAttackModifier() == pUnit->cityDefenseModifier()) {
			if (pUnit->cityAttackModifier() != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_CITY_STRENGTH_MOD", pUnit->cityAttackModifier()));
			}
		} else {
			if (pUnit->cityAttackModifier() != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_CITY_ATTACK_TEXT", pUnit->cityAttackModifier()));
			}

			if (pUnit->cityDefenseModifier() != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_PROMOTION_CITY_DEFENSE_TEXT", pUnit->cityDefenseModifier()));
			}
		}

		if (pUnit->animalCombatModifier() != 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_UNIT_ANIMAL_COMBAT_MOD", pUnit->animalCombatModifier()));
		}

		if (pUnit->getDropRange() > 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_UNIT_PARADROP_RANGE", pUnit->getDropRange()));
		}

		if (pUnit->hillsAttackModifier() == pUnit->hillsDefenseModifier()) {
			if (pUnit->hillsAttackModifier() != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_HILLS_STRENGTH", pUnit->hillsAttackModifier()));
			}
		} else {
			if (pUnit->hillsAttackModifier() != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_HILLS_ATTACK", pUnit->hillsAttackModifier()));
			}

			if (pUnit->hillsDefenseModifier() != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_HILLS_DEFENSE", pUnit->hillsDefenseModifier()));
			}
		}

		for (TerrainTypes eTerrain = (TerrainTypes)0; eTerrain < GC.getNumTerrainInfos(); eTerrain = (TerrainTypes)(eTerrain + 1)) {
			const CvTerrainInfo& kTerrain = GC.getTerrainInfo(eTerrain);
			if (pUnit->terrainAttackModifier(eTerrain) == pUnit->terrainDefenseModifier(eTerrain)) {
				if (pUnit->terrainAttackModifier(eTerrain) != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_STRENGTH", pUnit->terrainAttackModifier(eTerrain), kTerrain.getTextKeyWide()));
				}
			} else {
				if (pUnit->terrainAttackModifier(eTerrain) != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_ATTACK", pUnit->terrainAttackModifier(eTerrain), kTerrain.getTextKeyWide()));
				}

				if (pUnit->terrainDefenseModifier(eTerrain) != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_DEFENSE", pUnit->terrainDefenseModifier(eTerrain), kTerrain.getTextKeyWide()));
				}
			}
		}

		for (FeatureTypes eFeature = (FeatureTypes)0; eFeature < GC.getNumFeatureInfos(); eFeature = (FeatureTypes)(eFeature + 1)) {
			const CvFeatureInfo& kFeature = GC.getFeatureInfo(eFeature);
			if (pUnit->featureAttackModifier(eFeature) == pUnit->featureDefenseModifier(eFeature)) {
				if (pUnit->featureAttackModifier(eFeature) != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_STRENGTH", pUnit->featureAttackModifier(eFeature), kFeature.getTextKeyWide()));
				}
			} else {
				if (pUnit->featureAttackModifier(eFeature) != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_ATTACK", pUnit->featureAttackModifier(eFeature), kFeature.getTextKeyWide()));
				}

				if (pUnit->featureDefenseModifier(eFeature) != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_DEFENSE", pUnit->featureDefenseModifier(eFeature), kFeature.getTextKeyWide()));
				}
			}
		}

		for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
			const CvUnitClassInfo& kUnitClass = GC.getUnitClassInfo(eUnitClass);
			if (pUnit->getUnitInfo().getUnitClassAttackModifier(eUnitClass) == GC.getUnitInfo(pUnit->getUnitType()).getUnitClassDefenseModifier(eUnitClass)) {
				if (pUnit->getUnitInfo().getUnitClassAttackModifier(eUnitClass) != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_MOD_VS_TYPE", pUnit->getUnitInfo().getUnitClassAttackModifier(eUnitClass), kUnitClass.getTextKeyWide()));
				}
			} else {
				if (pUnit->getUnitInfo().getUnitClassAttackModifier(eUnitClass) != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_ATTACK_MOD_VS_CLASS", pUnit->getUnitInfo().getUnitClassAttackModifier(eUnitClass), kUnitClass.getTextKeyWide()));
				}

				if (pUnit->getUnitInfo().getUnitClassDefenseModifier(eUnitClass) != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_DEFENSE_MOD_VS_CLASS", pUnit->getUnitInfo().getUnitClassDefenseModifier(eUnitClass), kUnitClass.getTextKeyWide()));
				}
			}
		}

		for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
			if (pUnit->unitCombatModifier(eUnitCombat) != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_MOD_VS_TYPE", pUnit->unitCombatModifier(eUnitCombat), GC.getUnitCombatInfo(eUnitCombat).getTextKeyWide()));
			}
		}

		for (DomainTypes eDomain = (DomainTypes)0; eDomain < NUM_DOMAIN_TYPES; eDomain = (DomainTypes)(eDomain + 1)) {
			if (pUnit->domainModifier(eDomain) != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_MOD_VS_TYPE", pUnit->domainModifier(eDomain), GC.getDomainInfo(eDomain).getTextKeyWide()));
			}
		}

		szTempBuffer.clear();
		bool bFirst = true;
		for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
			if (pUnit->getUnitInfo().getTargetUnitClass(eUnitClass)) {
				if (bFirst) {
					bFirst = false;
				} else {
					szTempBuffer += L", ";
				}

				szTempBuffer += CvWString::format(L"<link=literal>%s</link>", GC.getUnitClassInfo(eUnitClass).getDescription());
			}
		}

		if (!bFirst) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_UNIT_TARGETS_UNIT_FIRST", szTempBuffer.GetCString()));
		}

		szTempBuffer.clear();
		bFirst = true;
		for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
			if (pUnit->getUnitInfo().getDefenderUnitClass(eUnitClass)) {
				if (bFirst) {
					bFirst = false;
				} else {
					szTempBuffer += L", ";
				}

				szTempBuffer += CvWString::format(L"<link=literal>%s</link>", GC.getUnitClassInfo(eUnitClass).getDescription());
			}
		}

		if (!bFirst) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_UNIT_DEFENDS_UNIT_FIRST", szTempBuffer.GetCString()));
		}

		szTempBuffer.clear();
		bFirst = true;
		for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
			if (pUnit->getUnitInfo().getTargetUnitCombat(eUnitCombat)) {
				if (bFirst) {
					bFirst = false;
				} else {
					szTempBuffer += L", ";
				}

				szTempBuffer += CvWString::format(L"<link=literal>%s</link>", GC.getUnitCombatInfo(eUnitCombat).getDescription());
			}
		}

		if (!bFirst) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_UNIT_TARGETS_UNIT_FIRST", szTempBuffer.GetCString()));
		}

		szTempBuffer.clear();
		bFirst = true;
		for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
			if (pUnit->getUnitInfo().getDefenderUnitCombat(eUnitCombat)) {
				if (bFirst) {
					bFirst = false;
				} else {
					szTempBuffer += L", ";
				}

				szTempBuffer += CvWString::format(L"<link=literal>%s</link>", GC.getUnitCombatInfo(eUnitCombat).getDescription());
			}
		}

		if (!bFirst) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_UNIT_DEFENDS_UNIT_FIRST", szTempBuffer.GetCString()));
		}

		szTempBuffer.clear();
		bFirst = true;
		for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
			if (pUnit->getUnitInfo().getFlankingStrikeUnitClass(eUnitClass) > 0) {
				if (bFirst) {
					bFirst = false;
				} else {
					szTempBuffer += L", ";
				}

				szTempBuffer += CvWString::format(L"<link=literal>%s</link>", GC.getUnitClassInfo(eUnitClass).getDescription());
			}
		}

		if (!bFirst) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_UNIT_FLANKING_STRIKES", szTempBuffer.GetCString()));
		}

		if (pUnit->bombardRate() > 0) {
			if (bShort) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_BOMBARD_RATE_SHORT", ((pUnit->bombardRate() * 100) / GC.getMAX_CITY_DEFENSE_DAMAGE())));
			} else {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_BOMBARD_RATE", ((pUnit->bombardRate() * 100) / GC.getMAX_CITY_DEFENSE_DAMAGE())));
			}
		}

		if (pUnit->isSpy()) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_UNIT_IS_SPY"));
		}

		if (pUnit->getUnitInfo().isNoRevealMap()) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_UNIT_VISIBILITY_MOVE_RANGE"));
		}

		if (!CvWString(pUnit->getUnitInfo().getHelp()).empty()) {
			szString.append(NEWLINE);
			szString.append(pUnit->getUnitInfo().getHelp());
		}

		if (bShift && (gDLL->getChtLvl() > 0)) {
			szTempBuffer.Format(L"\nUnitAI Type = %s.", GC.getUnitAIInfo(pUnit->AI_getUnitAIType()).getDescription());
			szString.append(szTempBuffer);
			szTempBuffer.Format(L"\nSacrifice Value = %d.", pUnit->AI_sacrificeValue(NULL));
			szString.append(szTempBuffer);
		}
	}
}


void CvGameTextMgr::setPlotListHelp(CvWStringBuffer& szString, CvPlot* pPlot, bool bOneLine, bool bShort) {
	PROFILE_FUNC();

	int numPromotionInfos = GC.getNumPromotionInfos();

	// if cheatmode and ctrl, display grouping info instead
	if ((gDLL->getChtLvl() > 0) && GC.ctrlKey()) {
		if (pPlot->isVisible(GC.getGameINLINE().getActiveTeam(), true)) {
			CvWString szTempString;

			CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();
			while (pUnitNode != NULL) {
				CvUnit* pHeadUnit = ::getUnit(pUnitNode->m_data);
				pUnitNode = pPlot->nextUnitNode(pUnitNode);

				// is this unit the head of a group, not cargo, and visible?
				if (pHeadUnit && pHeadUnit->isGroupHead() && !pHeadUnit->isCargo() && !pHeadUnit->isInvisible(GC.getGameINLINE().getActiveTeam(), true)) {
					// head unit name and unitai
					szString.append(CvWString::format(SETCOLR L"%s" ENDCOLR, 255, 190, 0, 255, pHeadUnit->getName().GetCString()));
					szString.append(CvWString::format(L" (%d)", shortenID(pHeadUnit->getID())));
					getUnitAIString(szTempString, pHeadUnit->AI_getUnitAIType());
					szString.append(CvWString::format(SETCOLR L" %s " ENDCOLR, GET_PLAYER(pHeadUnit->getOwnerINLINE()).getPlayerTextColorR(), GET_PLAYER(pHeadUnit->getOwnerINLINE()).getPlayerTextColorG(), GET_PLAYER(pHeadUnit->getOwnerINLINE()).getPlayerTextColorB(), GET_PLAYER(pHeadUnit->getOwnerINLINE()).getPlayerTextColorA(), szTempString.GetCString()));

					// promotion icons
					for (int iPromotionIndex = 0; iPromotionIndex < numPromotionInfos; iPromotionIndex++) {
						PromotionTypes ePromotion = (PromotionTypes)iPromotionIndex;
						if (pHeadUnit->isHasPromotion(ePromotion)) {
							szString.append(CvWString::format(L"<img=%S size=16></img>", GC.getPromotionInfo(ePromotion).getButton()));
						}
					}

					// group
					CvSelectionGroup* pHeadGroup = pHeadUnit->getGroup();
					FAssertMsg(pHeadGroup != NULL, "unit has NULL group");
					if (pHeadGroup->getNumUnits() > 1) {
						szString.append(CvWString::format(L"\nGroup:%d [%d units", shortenID(pHeadGroup->getID()), pHeadGroup->getNumUnits()));
						if (pHeadGroup->getCargo() > 0) {
							szString.append(CvWString::format(L" + %d cargo", pHeadGroup->getCargo()));
						}
						szString.append(CvWString::format(L"]"));

						// get average damage
						int iAverageDamage = 0;
						CLLNode<IDInfo>* pUnitNode = pHeadGroup->headUnitNode();
						while (pUnitNode != NULL) {
							CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
							pUnitNode = pHeadGroup->nextUnitNode(pUnitNode);

							iAverageDamage += (pLoopUnit->getDamage() * pLoopUnit->maxHitPoints()) / 100;
						}
						iAverageDamage /= pHeadGroup->getNumUnits();
						if (iAverageDamage > 0) {
							szString.append(CvWString::format(L" %d%%", 100 - iAverageDamage));
						}
					}

					if (!pHeadGroup->isHuman() && pHeadGroup->isStranded()) {
						szString.append(CvWString::format(SETCOLR L"\n***STRANDED***" ENDCOLR, TEXT_COLOR("COLOR_RED")));
					}

					if (!GC.altKey()) {
						// mission ai
						MissionAITypes eMissionAI = pHeadGroup->AI_getMissionAIType();
						if (eMissionAI != NO_MISSIONAI) {
							getMissionAIString(szTempString, eMissionAI);
							szString.append(CvWString::format(SETCOLR L"\n%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), szTempString.GetCString()));
						}

						// mission
						MissionTypes eMissionType = (MissionTypes)pHeadGroup->getMissionType(0);
						if (eMissionType != NO_MISSION) {
							getMissionTypeString(szTempString, eMissionType);
							szString.append(CvWString::format(SETCOLR L"\n%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), szTempString.GetCString()));
						}

						// mission unit
						CvUnit* pMissionUnit = pHeadGroup->AI_getMissionAIUnit();
						if (pMissionUnit != NULL && (eMissionAI != NO_MISSIONAI || eMissionType != NO_MISSION)) {
							// mission unit
							szString.append(L"\n to ");
							szString.append(CvWString::format(SETCOLR L"%s" ENDCOLR, GET_PLAYER(pMissionUnit->getOwnerINLINE()).getPlayerTextColorR(), GET_PLAYER(pMissionUnit->getOwnerINLINE()).getPlayerTextColorG(), GET_PLAYER(pMissionUnit->getOwnerINLINE()).getPlayerTextColorB(), GET_PLAYER(pMissionUnit->getOwnerINLINE()).getPlayerTextColorA(), pMissionUnit->getName().GetCString()));
							szString.append(CvWString::format(L"(%d) G:%d", shortenID(pMissionUnit->getID()), shortenID(pMissionUnit->getGroupID())));
							getUnitAIString(szTempString, pMissionUnit->AI_getUnitAIType());
							szString.append(CvWString::format(SETCOLR L" %s" ENDCOLR, GET_PLAYER(pMissionUnit->getOwnerINLINE()).getPlayerTextColorR(), GET_PLAYER(pMissionUnit->getOwnerINLINE()).getPlayerTextColorG(), GET_PLAYER(pMissionUnit->getOwnerINLINE()).getPlayerTextColorB(), GET_PLAYER(pMissionUnit->getOwnerINLINE()).getPlayerTextColorA(), szTempString.GetCString()));
						}

						// mission plot
						if (eMissionAI != NO_MISSIONAI || eMissionType != NO_MISSION) {
							// first try the plot from the missionAI
							CvPlot* pMissionPlot = pHeadGroup->AI_getMissionAIPlot();

							// if MissionAI does not have a plot, get one from the mission itself
							if (pMissionPlot == NULL && eMissionType != NO_MISSION) {
								switch (eMissionType) {
								case MISSION_MOVE_TO:
								case MISSION_ROUTE_TO:
									pMissionPlot = GC.getMapINLINE().plotINLINE(pHeadGroup->getMissionData1(0), pHeadGroup->getMissionData2(0));
									break;

								case MISSION_MOVE_TO_UNIT:
									if (pMissionUnit != NULL) {
										pMissionPlot = pMissionUnit->plot();
									}
									break;
								}
							}

							if (pMissionPlot != NULL) {
								szString.append(CvWString::format(L"\n [%d,%d]", pMissionPlot->getX_INLINE(), pMissionPlot->getY_INLINE()));

								CvCity* pCity = pMissionPlot->getWorkingCity();
								if (pCity != NULL) {
									szString.append(L" (");

									if (!pMissionPlot->isCity()) {
										DirectionTypes eDirection = estimateDirection(dxWrap(pMissionPlot->getX_INLINE() - pCity->plot()->getX_INLINE()), dyWrap(pMissionPlot->getY_INLINE() - pCity->plot()->getY_INLINE()));

										getDirectionTypeString(szTempString, eDirection);
										szString.append(CvWString::format(L"%s of ", szTempString.GetCString()));
									}

									szString.append(CvWString::format(SETCOLR L"%s" ENDCOLR L")", GET_PLAYER(pCity->getOwnerINLINE()).getPlayerTextColorR(), GET_PLAYER(pCity->getOwnerINLINE()).getPlayerTextColorG(), GET_PLAYER(pCity->getOwnerINLINE()).getPlayerTextColorB(), GET_PLAYER(pCity->getOwnerINLINE()).getPlayerTextColorA(), pCity->getName().GetCString()));
								} else {
									if (pMissionPlot != pPlot) {
										DirectionTypes eDirection = estimateDirection(dxWrap(pMissionPlot->getX_INLINE() - pPlot->getX_INLINE()), dyWrap(pMissionPlot->getY_INLINE() - pPlot->getY_INLINE()));

										getDirectionTypeString(szTempString, eDirection);
										szString.append(CvWString::format(L" (%s)", szTempString.GetCString()));
									}

									PlayerTypes eMissionPlotOwner = pMissionPlot->getOwnerINLINE();
									if (eMissionPlotOwner != NO_PLAYER) {
										szString.append(CvWString::format(L", " SETCOLR L"%s" ENDCOLR, GET_PLAYER(eMissionPlotOwner).getPlayerTextColorR(), GET_PLAYER(eMissionPlotOwner).getPlayerTextColorG(), GET_PLAYER(eMissionPlotOwner).getPlayerTextColorB(), GET_PLAYER(eMissionPlotOwner).getPlayerTextColorA(), GET_PLAYER(eMissionPlotOwner).getName()));
									}
								}
							}
						}

						// activity
						ActivityTypes eActivityType = (ActivityTypes)pHeadGroup->getActivityType();
						if (eActivityType != NO_ACTIVITY) {
							getActivityTypeString(szTempString, eActivityType);
							szString.append(CvWString::format(SETCOLR L"\n%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), szTempString.GetCString()));
						}
					}

					if (!GC.altKey() && !GC.shiftKey()) {
						// display cargo for head unit
						std::vector<CvUnit*> aCargoUnits;
						pHeadUnit->getCargoUnits(aCargoUnits);
						for (uint i = 0; i < aCargoUnits.size(); ++i) {
							CvUnit* pCargoUnit = aCargoUnits[i];
							if (!pCargoUnit->isInvisible(GC.getGameINLINE().getActiveTeam(), true)) {
								// name and unitai
								szString.append(CvWString::format(SETCOLR L"\n %s" ENDCOLR, TEXT_COLOR("COLOR_ALT_HIGHLIGHT_TEXT"), pCargoUnit->getName().GetCString()));
								szString.append(CvWString::format(L"(%d)", shortenID(pCargoUnit->getID())));
								getUnitAIString(szTempString, pCargoUnit->AI_getUnitAIType());
								szString.append(CvWString::format(SETCOLR L" %s " ENDCOLR, GET_PLAYER(pCargoUnit->getOwnerINLINE()).getPlayerTextColorR(), GET_PLAYER(pCargoUnit->getOwnerINLINE()).getPlayerTextColorG(), GET_PLAYER(pCargoUnit->getOwnerINLINE()).getPlayerTextColorB(), GET_PLAYER(pCargoUnit->getOwnerINLINE()).getPlayerTextColorA(), szTempString.GetCString()));

								// promotion icons
								for (int iPromotionIndex = 0; iPromotionIndex < numPromotionInfos; iPromotionIndex++) {
									PromotionTypes ePromotion = (PromotionTypes)iPromotionIndex;
									if (pCargoUnit->isHasPromotion(ePromotion)) {
										szString.append(CvWString::format(L"<img=%S size=16></img>", GC.getPromotionInfo(ePromotion).getButton()));
									}
								}
							}
						}

						// display grouped units
						CLLNode<IDInfo>* pUnitNode3 = pPlot->headUnitNode();
						while (pUnitNode3 != NULL) {
							CvUnit* pUnit = ::getUnit(pUnitNode3->m_data);
							pUnitNode3 = pPlot->nextUnitNode(pUnitNode3);

							// is this unit not head, in head's group and visible?
							if (pUnit && (pUnit != pHeadUnit) && (pUnit->getGroupID() == pHeadUnit->getGroupID()) && !pUnit->isInvisible(GC.getGameINLINE().getActiveTeam(), true)) {
								FAssertMsg(!pUnit->isCargo(), "unit is cargo but head unit is not cargo");
								// name and unitai
								szString.append(CvWString::format(SETCOLR L"\n-%s" ENDCOLR, TEXT_COLOR("COLOR_UNIT_TEXT"), pUnit->getName().GetCString()));
								szString.append(CvWString::format(L" (%d)", shortenID(pUnit->getID())));
								getUnitAIString(szTempString, pUnit->AI_getUnitAIType());
								szString.append(CvWString::format(SETCOLR L" %s " ENDCOLR, GET_PLAYER(pUnit->getOwnerINLINE()).getPlayerTextColorR(), GET_PLAYER(pUnit->getOwnerINLINE()).getPlayerTextColorG(), GET_PLAYER(pUnit->getOwnerINLINE()).getPlayerTextColorB(), GET_PLAYER(pUnit->getOwnerINLINE()).getPlayerTextColorA(), szTempString.GetCString()));

								// promotion icons
								for (int iPromotionIndex = 0; iPromotionIndex < numPromotionInfos; iPromotionIndex++) {
									PromotionTypes ePromotion = (PromotionTypes)iPromotionIndex;
									if (pUnit->isHasPromotion(ePromotion)) {
										szString.append(CvWString::format(L"<img=%S size=16></img>", GC.getPromotionInfo(ePromotion).getButton()));
									}
								}

								// display cargo for loop unit
								std::vector<CvUnit*> aLoopCargoUnits;
								pUnit->getCargoUnits(aLoopCargoUnits);
								for (uint i = 0; i < aLoopCargoUnits.size(); ++i) {
									CvUnit* pCargoUnit = aLoopCargoUnits[i];
									if (!pCargoUnit->isInvisible(GC.getGameINLINE().getActiveTeam(), true)) {
										// name and unitai
										szString.append(CvWString::format(SETCOLR L"\n %s" ENDCOLR, TEXT_COLOR("COLOR_ALT_HIGHLIGHT_TEXT"), pCargoUnit->getName().GetCString()));
										szString.append(CvWString::format(L"(%d)", shortenID(pCargoUnit->getID())));
										getUnitAIString(szTempString, pCargoUnit->AI_getUnitAIType());
										szString.append(CvWString::format(SETCOLR L" %s " ENDCOLR, GET_PLAYER(pCargoUnit->getOwnerINLINE()).getPlayerTextColorR(), GET_PLAYER(pCargoUnit->getOwnerINLINE()).getPlayerTextColorG(), GET_PLAYER(pCargoUnit->getOwnerINLINE()).getPlayerTextColorB(), GET_PLAYER(pCargoUnit->getOwnerINLINE()).getPlayerTextColorA(), szTempString.GetCString()));

										// promotion icons
										for (int iPromotionIndex = 0; iPromotionIndex < numPromotionInfos; iPromotionIndex++) {
											PromotionTypes ePromotion = (PromotionTypes)iPromotionIndex;
											if (pCargoUnit->isHasPromotion(ePromotion)) {
												szString.append(CvWString::format(L"<img=%S size=16></img>", GC.getPromotionInfo(ePromotion).getButton()));
											}
										}
									}
								}
							}
						}
					}

					if (!GC.altKey()) {
						if (pPlot->getTeam() == NO_TEAM || GET_TEAM(pHeadGroup->getTeam()).isAtWar(pPlot->getTeam())) {
							szString.append(NEWLINE);
							CvWString szTempBuffer;

							//AI strategies
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_DAGGER)) {
								szTempBuffer.Format(L"Dagger, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_CRUSH)) {
								szTempBuffer.Format(L"Crush, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_ALERT1)) {
								szTempBuffer.Format(L"Alert1, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_ALERT2)) {
								szTempBuffer.Format(L"Alert2, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_TURTLE)) {
								szTempBuffer.Format(L"Turtle, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_LAST_STAND)) {
								szTempBuffer.Format(L"Last Stand, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_FINAL_WAR)) {
								szTempBuffer.Format(L"FinalWar, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_GET_BETTER_UNITS)) {
								szTempBuffer.Format(L"GetBetterUnits, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_FASTMOVERS)) {
								szTempBuffer.Format(L"FastMovers, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_LAND_BLITZ)) {
								szTempBuffer.Format(L"LandBlitz, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_AIR_BLITZ)) {
								szTempBuffer.Format(L"AirBlitz, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_OWABWNW)) {
								szTempBuffer.Format(L"OWABWNW, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_PRODUCTION)) {
								szTempBuffer.Format(L"Production, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_MISSIONARY)) {
								szTempBuffer.Format(L"Missionary, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_BIG_ESPIONAGE)) {
								szTempBuffer.Format(L"BigEspionage, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_ECONOMY_FOCUS)) // K-Mod
							{
								szTempBuffer.Format(L"EconomyFocus, ");
								szString.append(szTempBuffer);
							}
							if (GET_PLAYER(pHeadGroup->getOwner()).AI_isDoStrategy(AI_STRATEGY_ESPIONAGE_ECONOMY)) // K-Mod
							{
								szTempBuffer.Format(L"EspionageEconomy, ");
								szString.append(szTempBuffer);
							}

							//Area battle plans.
							if (pPlot->area()->getAreaAIType(pHeadGroup->getTeam()) == AREAAI_OFFENSIVE) {
								szTempBuffer.Format(L"\n Area AI = OFFENSIVE");
							} else if (pPlot->area()->getAreaAIType(pHeadGroup->getTeam()) == AREAAI_DEFENSIVE) {
								szTempBuffer.Format(L"\n Area AI = DEFENSIVE");
							} else if (pPlot->area()->getAreaAIType(pHeadGroup->getTeam()) == AREAAI_MASSING) {
								szTempBuffer.Format(L"\n Area AI = MASSING");
							} else if (pPlot->area()->getAreaAIType(pHeadGroup->getTeam()) == AREAAI_ASSAULT) {
								szTempBuffer.Format(L"\n Area AI = ASSAULT");
							} else if (pPlot->area()->getAreaAIType(pHeadGroup->getTeam()) == AREAAI_ASSAULT_MASSING) {
								szTempBuffer.Format(L"\n Area AI = ASSAULT_MASSING");
							} else if (pPlot->area()->getAreaAIType(pHeadGroup->getTeam()) == AREAAI_NEUTRAL) {
								szTempBuffer.Format(L"\n Area AI = NEUTRAL");
							}

							CvCity* pTargetCity = pPlot->area()->getTargetCity(pHeadGroup->getOwner());
							if (pTargetCity) {
								szString.append(CvWString::format(L"\nTarget City: %s (%d)", pTargetCity->getName().c_str(), pTargetCity->getOwner()));
							} else {
								szString.append(CvWString::format(L"\nTarget City: None"));
							}

							if (GC.shiftKey()) {
								CvCity* pLoopCity;
								int iLoop = 0;
								int iBestTargetValue = (pTargetCity != NULL ? GET_PLAYER(pHeadGroup->getOwner()).AI_targetCityValue(pTargetCity, false, true) : 0);
								int iTargetValue = 0;
								szString.append(CvWString::format(L"\n\nTarget City values:\n"));
								for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++) {
									if (GET_TEAM(pHeadGroup->getTeam()).AI_getWarPlan(GET_PLAYER((PlayerTypes)iPlayer).getTeam()) != NO_WARPLAN) {
										if (pPlot->area()->getCitiesPerPlayer((PlayerTypes)iPlayer) > 0) {
											for (pLoopCity = GET_PLAYER((PlayerTypes)iPlayer).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER((PlayerTypes)iPlayer).nextCity(&iLoop)) {
												if (pLoopCity->area() == pPlot->area()) {
													iTargetValue = GET_PLAYER(pHeadGroup->getOwner()).AI_targetCityValue(pLoopCity, false, true);

													if ((GC.getMapINLINE().calculatePathDistance(pPlot, pLoopCity->plot()) < 20)) {
														szString.append(CvWString::format(L"\n%s : %d + rand %d", pLoopCity->getName().c_str(), iTargetValue, (pLoopCity->getPopulation() / 2)));
													}
												}
											}
										}
									}
								}
							}
						}
					}

					// double space non-empty groups
					if (pHeadGroup->getNumUnits() > 1 || pHeadUnit->hasCargo()) {
						szString.append(NEWLINE);
					}

					szString.append(NEWLINE);
				}
			}
		}

		return;
	}


	static const uint iMaxNumUnits = 15;
	static std::vector<CvUnit*> apUnits;
	static std::vector<int> aiUnitNumbers;
	static std::vector<int> aiUnitStrength;
	static std::vector<int> aiUnitMaxStrength;
	static std::vector<CvUnit*> plotUnits;

	GC.getGameINLINE().getPlotUnits(pPlot, plotUnits);

	int iNumVisibleUnits = 0;
	if (pPlot->isVisible(GC.getGameINLINE().getActiveTeam(), true)) {
		CLLNode<IDInfo>* pUnitNode5 = pPlot->headUnitNode();
		while (pUnitNode5 != NULL) {
			CvUnit* pUnit = ::getUnit(pUnitNode5->m_data);
			pUnitNode5 = pPlot->nextUnitNode(pUnitNode5);

			if (pUnit && !pUnit->isInvisible(GC.getGameINLINE().getActiveTeam(), true)) {
				++iNumVisibleUnits;
			}
		}
	}

	apUnits.erase(apUnits.begin(), apUnits.end());

	if (iNumVisibleUnits > iMaxNumUnits) {
		aiUnitNumbers.erase(aiUnitNumbers.begin(), aiUnitNumbers.end());
		aiUnitStrength.erase(aiUnitStrength.begin(), aiUnitStrength.end());
		aiUnitMaxStrength.erase(aiUnitMaxStrength.begin(), aiUnitMaxStrength.end());

		if (m_apbPromotion.size() == 0) {
			for (int iI = 0; iI < (GC.getNumUnitInfos() * MAX_PLAYERS); ++iI) {
				m_apbPromotion.push_back(new int[numPromotionInfos]);
			}
		}

		for (int iI = 0; iI < (GC.getNumUnitInfos() * MAX_PLAYERS); ++iI) {
			aiUnitNumbers.push_back(0);
			aiUnitStrength.push_back(0);
			aiUnitMaxStrength.push_back(0);
			for (int iJ = 0; iJ < numPromotionInfos; iJ++) {
				m_apbPromotion[iI][iJ] = 0;
			}
		}
	}

	int iCount = 0;
	for (int iI = iMaxNumUnits; iI < iNumVisibleUnits && iI < (int)plotUnits.size(); ++iI) {
		CvUnit* pLoopUnit = plotUnits[iI];

		if (pLoopUnit != NULL && pLoopUnit != pPlot->getCenterUnit()) {
			apUnits.push_back(pLoopUnit);

			if (iNumVisibleUnits > iMaxNumUnits) {
				int iIndex = pLoopUnit->getUnitType() * MAX_PLAYERS + pLoopUnit->getOwner();
				if (aiUnitNumbers[iIndex] == 0) {
					++iCount;
				}
				++aiUnitNumbers[iIndex];

				int iBase = (DOMAIN_AIR == pLoopUnit->getDomainType() ? pLoopUnit->airBaseCombatStr() : pLoopUnit->baseCombatStr());
				if (iBase > 0 && pLoopUnit->maxHitPoints() > 0) {
					aiUnitMaxStrength[iIndex] += 100 * iBase;
					aiUnitStrength[iIndex] += (100 * iBase * pLoopUnit->currHitPoints()) / pLoopUnit->maxHitPoints();
				}

				for (int iJ = 0; iJ < numPromotionInfos; iJ++) {
					if (pLoopUnit->isHasPromotion((PromotionTypes)iJ)) {
						++m_apbPromotion[iIndex][iJ];
					}
				}
			}
		}
	}


	if (iNumVisibleUnits > 0) {
		if (pPlot->getCenterUnit()) {
			setUnitHelp(szString, pPlot->getCenterUnit(), iNumVisibleUnits > iMaxNumUnits, true);
		}

		uint iNumShown = std::min<uint>(iMaxNumUnits, iNumVisibleUnits);
		for (uint iI = 0; iI < iNumShown && iI < (int)plotUnits.size(); ++iI) {
			CvUnit* pLoopUnit = plotUnits[iI];
			if (pLoopUnit != pPlot->getCenterUnit()) {
				szString.append(NEWLINE);
				setUnitHelp(szString, pLoopUnit, true, true);
			}
		}

		bool bFirst = true;
		if (iNumVisibleUnits > iMaxNumUnits) {
			for (int iI = 0; iI < GC.getNumUnitInfos(); ++iI) {
				for (int iJ = 0; iJ < MAX_PLAYERS; iJ++) {
					int iIndex = iI * MAX_PLAYERS + iJ;

					if (aiUnitNumbers[iIndex] > 0) {
						if (iCount < 5 || bFirst) {
							szString.append(NEWLINE);
							bFirst = false;
						} else {
							szString.append(L", ");
						}
						szString.append(CvWString::format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_UNIT_TEXT"), GC.getUnitInfo((UnitTypes)iI).getDescription()));
						szString.append(CvWString::format(L" (%d)", aiUnitNumbers[iIndex]));

						if (aiUnitMaxStrength[iIndex] > 0) {
							int iBase = (aiUnitMaxStrength[iIndex] / aiUnitNumbers[iIndex]) / 100;
							int iCurrent = (aiUnitStrength[iIndex] / aiUnitNumbers[iIndex]) / 100;
							int iCurrent100 = (aiUnitStrength[iIndex] / aiUnitNumbers[iIndex]) % 100;
							if (0 == iCurrent100) {
								if (iBase == iCurrent) {
									szString.append(CvWString::format(L" %d", iBase));
								} else {
									szString.append(CvWString::format(L" %d/%d", iCurrent, iBase));
								}
							} else {
								szString.append(CvWString::format(L" %d.%02d/%d", iCurrent, iCurrent100, iBase));
							}
							szString.append(CvWString::format(L"%c", gDLL->getSymbolID(STRENGTH_CHAR)));
						}


						for (int iK = 0; iK < numPromotionInfos; iK++) {
							if (m_apbPromotion[iIndex][iK] > 0) {
								szString.append(CvWString::format(L"%d<img=%S size=16></img>", m_apbPromotion[iIndex][iK], GC.getPromotionInfo((PromotionTypes)iK).getButton()));
							}
						}

						if (iJ != GC.getGameINLINE().getActivePlayer() && !GC.getUnitInfo((UnitTypes)iI).isAnimal() && !GC.getUnitInfo((UnitTypes)iI).isHiddenNationality()) {
							szString.append(L", ");
							szString.append(CvWString::format(SETCOLR L"%s" ENDCOLR, GET_PLAYER((PlayerTypes)iJ).getPlayerTextColorR(), GET_PLAYER((PlayerTypes)iJ).getPlayerTextColorG(), GET_PLAYER((PlayerTypes)iJ).getPlayerTextColorB(), GET_PLAYER((PlayerTypes)iJ).getPlayerTextColorA(), GET_PLAYER((PlayerTypes)iJ).getName()));
						}
					}
				}
			}
		}
	}
}

//Calculates the probability of a particular combat outcome
//Returns a float value (between 0 and 1)
//Written by PieceOfMind
//n_A = hits taken by attacker, n_D = hits taken by defender.
static float getCombatOddsSpecific(CvUnit* pAttacker, CvUnit* pDefender, int n_A, int n_D) {
	int iAttackerStrength = pAttacker->currCombatStr(NULL, NULL);
	int iAttackerFirepower = pAttacker->currFirepower(NULL, NULL);
	int iDefenderStrength = pDefender->currCombatStr(pDefender->plot(), pAttacker);
	int iDefenderFirepower = pDefender->currFirepower(pDefender->plot(), pAttacker);

	int iStrengthFactor = ((iAttackerFirepower + iDefenderFirepower + 1) / 2);
	int iDamageToAttacker = std::max(1, ((GC.getDefineINT("COMBAT_DAMAGE") * (iDefenderFirepower + iStrengthFactor)) / (iAttackerFirepower + iStrengthFactor)));
	int iDamageToDefender = std::max(1, ((GC.getDefineINT("COMBAT_DAMAGE") * (iAttackerFirepower + iStrengthFactor)) / (iDefenderFirepower + iStrengthFactor)));

	int iDefenderOdds = ((GC.getDefineINT("COMBAT_DIE_SIDES") * iDefenderStrength) / (iAttackerStrength + iDefenderStrength));
	int iAttackerOdds = GC.getDefineINT("COMBAT_DIE_SIDES") - iDefenderOdds;

	if (GC.getDefineINT("ACO_IgnoreBarbFreeWins") == 0) {
		if (pDefender->isBarbarian()) {
			//defender is barbarian
			if (!GET_PLAYER(pAttacker->getOwnerINLINE()).isBarbarian() && GET_PLAYER(pAttacker->getOwnerINLINE()).getWinsVsBarbs() < GC.getHandicapInfo(GET_PLAYER(pAttacker->getOwnerINLINE()).getHandicapType()).getFreeWinsVsBarbs()) {
				//attacker is not barb and attacker player has free wins left
				//I have assumed in the following code only one of the units (attacker and defender) can be a barbarian

				iDefenderOdds = std::min((10 * GC.getDefineINT("COMBAT_DIE_SIDES")) / 100, iDefenderOdds);
				iAttackerOdds = std::max((90 * GC.getDefineINT("COMBAT_DIE_SIDES")) / 100, iAttackerOdds);
			}
		} else if (pAttacker->isBarbarian()) {
			//attacker is barbarian
			if (!GET_PLAYER(pDefender->getOwnerINLINE()).isBarbarian() && GET_PLAYER(pDefender->getOwnerINLINE()).getWinsVsBarbs() < GC.getHandicapInfo(GET_PLAYER(pDefender->getOwnerINLINE()).getHandicapType()).getFreeWinsVsBarbs()) {
				//defender is not barbarian and defender has free wins left and attacker is barbarian
				iAttackerOdds = std::min((10 * GC.getDefineINT("COMBAT_DIE_SIDES")) / 100, iAttackerOdds);
				iDefenderOdds = std::max((90 * GC.getDefineINT("COMBAT_DIE_SIDES")) / 100, iDefenderOdds);
			}
		}
	}

	int iDefenderHitLimit = pDefender->maxHitPoints() - pAttacker->combatLimit();

	int iNeededRoundsAttacker = (pDefender->currHitPoints() - pDefender->maxHitPoints() + pAttacker->combatLimit() - (((pAttacker->combatLimit()) == pDefender->maxHitPoints()) ? 1 : 0)) / iDamageToDefender + 1;

	int N_D = (std::max(0, pDefender->currHitPoints() - iDefenderHitLimit) + iDamageToDefender - (((pAttacker->combatLimit()) == GC.getMAX_HIT_POINTS()) ? 1 : 0)) / iDamageToDefender;

	int N_A = (pAttacker->currHitPoints() - 1) / iDamageToAttacker + 1;

	float RetreatOdds = ((float)(std::min((pAttacker->withdrawalProbability()), 100))) / 100.0f;

	int AttFSnet = ((pDefender->immuneToFirstStrikes()) ? 0 : pAttacker->firstStrikes()) - ((pAttacker->immuneToFirstStrikes()) ? 0 : pDefender->firstStrikes());
	int AttFSC = (pDefender->immuneToFirstStrikes()) ? 0 : (pAttacker->chanceFirstStrikes());
	int DefFSC = (pAttacker->immuneToFirstStrikes()) ? 0 : (pDefender->chanceFirstStrikes());

	float P_A = (float)iAttackerOdds / GC.getDefineINT("COMBAT_DIE_SIDES");
	float P_D = (float)iDefenderOdds / GC.getDefineINT("COMBAT_DIE_SIDES");
	float answer = 0.0f;
	if (n_A < N_A && n_D == iNeededRoundsAttacker)   // (1) Defender dies or is taken to combat limit
	{
		float sum1 = 0.0f;
		for (int i = (-AttFSnet - AttFSC < 1 ? 1 : -AttFSnet - AttFSC); i <= DefFSC - AttFSnet; i++) {
			for (int j = 0; j <= i; j++) {
				if (n_A >= j) {
					sum1 += (float)getBinomialCoefficient(i, j) * pow(P_A, (float)(i - j)) * getBinomialCoefficient(iNeededRoundsAttacker - 1 + n_A - j, iNeededRoundsAttacker - 1);

				} //if
			}//for j
		}//for i
		sum1 *= pow(P_D, (float)n_A) * pow(P_A, (float)iNeededRoundsAttacker);
		answer += sum1;

		float sum2 = 0.0f;

		for (int i = (0 < AttFSnet - DefFSC ? AttFSnet - DefFSC : 0); i <= AttFSnet + AttFSC; i++) {

			for (int j = 0; j <= i; j++) {
				if (N_D > j) {
					sum2 = sum2 + getBinomialCoefficient(n_A + iNeededRoundsAttacker - j - 1, n_A) * (float)getBinomialCoefficient(i, j) * pow(P_A, (float)iNeededRoundsAttacker) * pow(P_D, (float)(n_A + i - j));
				} else if (n_A == 0) {
					sum2 = sum2 + (float)getBinomialCoefficient(i, j) * pow(P_A, (float)j) * pow(P_D, (float)(i - j));
				} else {
					sum2 = sum2 + 0.0f;
				}
			}//for j

		}//for i
		answer += sum2;
	} else if (n_D < N_D && n_A == N_A)  // (2) Attacker dies!
	{
		float sum1 = 0.0f;
		for (int i = (-AttFSnet - AttFSC < 1 ? 1 : -AttFSnet - AttFSC); i <= DefFSC - AttFSnet; i++) {
			for (int j = 0; j <= i; j++) {
				if (N_A > j) {
					sum1 += getBinomialCoefficient(n_D + N_A - j - 1, n_D) * (float)getBinomialCoefficient(i, j) * pow(P_D, (float)(N_A)) * pow(P_A, (float)(n_D + i - j));
				} else {
					if (n_D == 0) {
						sum1 += (float)getBinomialCoefficient(i, j) * pow(P_D, (float)(j)) * pow(P_A, (float)(i - j));
					}//if (inside if) else sum += 0
				}//if
			}//for j
		}//for i
		answer += sum1;
		float sum2 = 0.0f;
		for (int i = (0 < AttFSnet - DefFSC ? AttFSnet - DefFSC : 0); i <= AttFSnet + AttFSC; i++) {
			for (int j = 0; j <= i; j++) {
				if (n_D >= j) {
					sum2 += (float)getBinomialCoefficient(i, j) * pow(P_D, (float)(i - j)) * getBinomialCoefficient(N_A - 1 + n_D - j, N_A - 1);
				} //if
			}//for j
		}//for i
		sum2 *= pow(P_A, (float)(n_D)) * pow(P_D, (float)(N_A));
		answer += sum2;
		answer = answer * (1.0f - RetreatOdds);
	} else if (n_A == (N_A - 1) && n_D < N_D)  // (3) Attacker retreats!
	{
		float sum1 = 0.0f;
		for (int i = (AttFSnet + AttFSC > -1 ? 1 : -AttFSnet - AttFSC); i <= DefFSC - AttFSnet; i++) {
			for (int j = 0; j <= i; j++) {
				if (N_A > j) {
					sum1 += getBinomialCoefficient(n_D + N_A - j - 1, n_D) * (float)getBinomialCoefficient(i, j) * pow(P_D, (float)(N_A)) * pow(P_A, (float)(n_D + i - j));
				} else {
					if (n_D == 0) {
						sum1 += (float)getBinomialCoefficient(i, j) * pow(P_D, (float)(j)) * pow(P_A, (float)(i - j));
					}//if (inside if) else sum += 0
				}//if
			}//for j
		}//for i
		answer += sum1;

		float sum2 = 0.0f;
		for (int i = (0 < AttFSnet - DefFSC ? AttFSnet - DefFSC : 0); i <= AttFSnet + AttFSC; i++) {
			for (int j = 0; j <= i; j++) {
				if (n_D >= j) {
					sum2 += (float)getBinomialCoefficient(i, j) * pow(P_D, (float)(i - j)) * getBinomialCoefficient(N_A - 1 + n_D - j, N_A - 1);
				} //if
			}//for j
		}//for i
		sum2 *= pow(P_A, (float)(n_D)) * pow(P_D, (float)(N_A));
		answer += sum2;
		answer = answer * RetreatOdds;//
	} else {
		//Unexpected value.  Process should not reach here.
		FAssertMsg(false, "unexpected value in getCombatOddsSpecific");
	}

	answer = answer / ((float)(AttFSC + DefFSC + 1)); // dividing by (t+w+1) as is necessary
	return answer;
}// getCombatOddsSpecific

// I had to add this function to the header file CvGameCoreUtils.h
// Returns true if help was given...
// K-Mod note: this function can change the center unit on the plot. (because of a change I made)
// Also, I've made some unmarked structural changes to this function, to make it easier to read and to fix a few minor bugs.
bool CvGameTextMgr::setCombatPlotHelp(CvWStringBuffer& szString, CvPlot* pPlot) {
	PROFILE_FUNC();

	/*
	Note that due to the large amount of extra content added to this function (setCombatPlotHelp), this should never be used in any function that needs to be called repeatedly (e.g. hundreds of times) quickly.
	It is fine for a human player mouse-over (which is what it is used for).
	*/
	bool ACO_enabled = getOptionBOOL("ACO__Enabled", false);

	bool bShift = GC.shiftKey();
	int iView = bShift ? 2 : 1;
	if (getOptionBOOL("ACO__SwapViews", false)) {
		iView = 3 - iView; //swaps 1 and 2.
	}
	CvWString szTempBuffer2;
	CvWString szTempBuffer;
	CvWString szOffenseOdds;
	CvWString szDefenseOdds;
	int iModifier;

	if (gDLL->getInterfaceIFace()->getLengthSelectionList() == 0) {
		return false;
	}

	bool bValid = false;

	switch (gDLL->getInterfaceIFace()->getSelectionList()->getDomainType()) {
	case DOMAIN_SEA:
		bValid = pPlot->isWater();
		break;

	case DOMAIN_AIR:
		bValid = true;
		break;

	case DOMAIN_LAND:
		bValid = !(pPlot->isWater());
		break;

	case DOMAIN_IMMOBILE:
		break;

	default:
		FAssert(false);
		break;
	}

	if (!bValid) {
		return false;
	}

	int iOdds;
	CvUnit* pAttacker = gDLL->getInterfaceIFace()->getSelectionList()->AI_getBestGroupAttacker(pPlot, false, iOdds);

	if (pAttacker == NULL)
		pAttacker = gDLL->getInterfaceIFace()->getSelectionList()->AI_getBestGroupAttacker(pPlot, false, iOdds, true); // bypass checks for moves and war etc.

	if (pAttacker == NULL)
		return false;

	CvUnit* pDefender = pPlot->getBestDefender(NO_PLAYER, pAttacker->getOwnerINLINE(), pAttacker, !GC.altKey());
	if (pDefender == NULL || !pDefender->canDefend(pPlot) || !pAttacker->canAttack(*pDefender))
		return false;

	// K-Mod. If the plot's center unit isn't one of our own units, then use this defender as the plot's center unit.
	// With this, the map will accurately shows who we're up against.
	if (gDLL->getInterfaceIFace()->getSelectionPlot() != pPlot) {
		if (pDefender->getOwnerINLINE() == GC.getGameINLINE().getActivePlayer() ||
			!pPlot->getCenterUnit() || // I don't think this is possible... but it's pretty cheap to check.
			pPlot->getCenterUnit()->getOwnerINLINE() != GC.getGameINLINE().getActivePlayer()) {
			pPlot->setCenterUnit(pDefender);
		}
	}

	bool ACO_ForceOriginalOdds = getOptionBOOL("ACO__ForceOriginalOdds", false);
	if (pAttacker->getDomainType() != DOMAIN_AIR) {
		int iCombatOdds = getCombatOdds(pAttacker, pDefender);

		if (pAttacker->combatLimit() >= GC.getMAX_HIT_POINTS()) {
			if (!ACO_enabled || ACO_ForceOriginalOdds) {
				if (iCombatOdds > 999) {
					szTempBuffer = L"&gt; 99.9";
				} else if (iCombatOdds < 1) {
					szTempBuffer = L"&lt; 0.1";
				} else {
					szTempBuffer.Format(L"%.1f", ((float)iCombatOdds) / 10.0f);
				}

				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_ODDS", szTempBuffer.GetCString()));
				if (ACO_enabled) {
					szString.append(NEWLINE);
				}
			}
		}


		int iWithdrawal = 0;

		if (pAttacker->combatLimit() < GC.getMAX_HIT_POINTS()) {
			iWithdrawal += 100 * iCombatOdds;
		}

		iWithdrawal += std::min(100, pAttacker->withdrawalProbability()) * (1000 - iCombatOdds);

		if (iWithdrawal > 0 || pAttacker->combatLimit() < GC.getMAX_HIT_POINTS()) {
			if (!ACO_enabled || ACO_ForceOriginalOdds) {
				if (iWithdrawal > 99900) {
					szTempBuffer = L"&gt; 99.9";
				} else if (iWithdrawal < 100) {
					szTempBuffer = L"&lt; 0.1";
				} else {
					szTempBuffer.Format(L"%.1f", iWithdrawal / 1000.0f);
				}

				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_ODDS_RETREAT", szTempBuffer.GetCString()));
				if (ACO_enabled) {
					szString.append(NEWLINE);
				}
			}
		}

		if (ACO_enabled) {

			//Change this to true when you need to spot errors, particular in the expected hit points calculations
			bool ACO_debug = getOptionBOOL("ACO__Debug", false);

			int iAttackerExperienceModifier = 0;
			int iDefenderExperienceModifier = 0;
			for (int ePromotion = 0; ePromotion < GC.getNumPromotionInfos(); ++ePromotion) {
				if (pAttacker->isHasPromotion((PromotionTypes)ePromotion) && GC.getPromotionInfo((PromotionTypes)ePromotion).getExperiencePercent() != 0) {
					iAttackerExperienceModifier += GC.getPromotionInfo((PromotionTypes)ePromotion).getExperiencePercent();
				}
			}

			for (int ePromotion = 0; ePromotion < GC.getNumPromotionInfos(); ++ePromotion) {
				if (pDefender->isHasPromotion((PromotionTypes)ePromotion) && GC.getPromotionInfo((PromotionTypes)ePromotion).getExperiencePercent() != 0) {
					iDefenderExperienceModifier += GC.getPromotionInfo((PromotionTypes)ePromotion).getExperiencePercent();
				}
			}

			int iAttackerStrength = pAttacker->currCombatStr(NULL, NULL);
			int iAttackerFirepower = pAttacker->currFirepower(NULL, NULL);
			int iDefenderStrength = pDefender->currCombatStr(pPlot, pAttacker);
			int iDefenderFirepower = pDefender->currFirepower(pPlot, pAttacker);

			FAssert((iAttackerStrength + iDefenderStrength) * (iAttackerFirepower + iDefenderFirepower) > 0);

			int iStrengthFactor = ((iAttackerFirepower + iDefenderFirepower + 1) / 2);
			int iDamageToAttacker = std::max(1, ((GC.getDefineINT("COMBAT_DAMAGE") * (iDefenderFirepower + iStrengthFactor)) / (iAttackerFirepower + iStrengthFactor)));
			int iDamageToDefender = std::max(1, ((GC.getDefineINT("COMBAT_DAMAGE") * (iAttackerFirepower + iStrengthFactor)) / (iDefenderFirepower + iStrengthFactor)));
			int iFlankAmount = iDamageToAttacker;

			int iDefenderOdds = ((GC.getDefineINT("COMBAT_DIE_SIDES") * iDefenderStrength) / (iAttackerStrength + iDefenderStrength));
			int iAttackerOdds = GC.getDefineINT("COMBAT_DIE_SIDES") - iDefenderOdds;


			// Barbarian related code.
			if (getOptionBOOL("ACO__IgnoreBarbFreeWins", true)) { //Are we not going to ignore barb free wins?  If not, skip this section...
				if (pDefender->isBarbarian()) {
					//defender is barbarian
					if (!GET_PLAYER(pAttacker->getOwnerINLINE()).isBarbarian() && GET_PLAYER(pAttacker->getOwnerINLINE()).getWinsVsBarbs() < GC.getHandicapInfo(GET_PLAYER(pAttacker->getOwnerINLINE()).getHandicapType()).getFreeWinsVsBarbs()) {
						//attacker is not barb and attacker player has free wins left
						//I have assumed in the following code only one of the units (attacker and defender) can be a barbarian
						iDefenderOdds = std::min((10 * GC.getDefineINT("COMBAT_DIE_SIDES")) / 100, iDefenderOdds);
						iAttackerOdds = std::max((90 * GC.getDefineINT("COMBAT_DIE_SIDES")) / 100, iAttackerOdds);
						szTempBuffer.Format(SETCOLR L"%d\n" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getHandicapInfo(GET_PLAYER(pAttacker->getOwnerINLINE()).getHandicapType()).getFreeWinsVsBarbs() - GET_PLAYER(pAttacker->getOwnerINLINE()).getWinsVsBarbs());
						szString.append(gDLL->getText("TXT_ACO_BarbFreeWinsLeft"));
						szString.append(szTempBuffer.GetCString());
					}
				} else {
					//defender is not barbarian
					if (pAttacker->isBarbarian()) {
						//attacker is barbarian
						if (!GET_PLAYER(pDefender->getOwnerINLINE()).isBarbarian() && GET_PLAYER(pDefender->getOwnerINLINE()).getWinsVsBarbs() < GC.getHandicapInfo(GET_PLAYER(pDefender->getOwnerINLINE()).getHandicapType()).getFreeWinsVsBarbs()) {
							//defender is not barbarian and defender has free wins left and attacker is barbarian
							iAttackerOdds = std::min((10 * GC.getDefineINT("COMBAT_DIE_SIDES")) / 100, iAttackerOdds);
							iDefenderOdds = std::max((90 * GC.getDefineINT("COMBAT_DIE_SIDES")) / 100, iDefenderOdds);
							szTempBuffer.Format(SETCOLR L"%d\n" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getHandicapInfo(GET_PLAYER(pDefender->getOwnerINLINE()).getHandicapType()).getFreeWinsVsBarbs() - GET_PLAYER(pDefender->getOwnerINLINE()).getWinsVsBarbs());
							szString.append(gDLL->getText("TXT_ACO_BarbFreeWinsLeft"));
							szString.append(szTempBuffer.GetCString());
						}
					}
				}
			}


			//XP calculations
			int iExperience = 0;
			int iWithdrawXP = GC.getDefineINT("EXPERIENCE_FROM_WITHDRAWL");//thanks to phungus420

			if (pAttacker->combatLimit() < 100) {
				iExperience = GC.getDefineINT("EXPERIENCE_FROM_WITHDRAWL");
			} else {
				iExperience = (pDefender->attackXPValue() * iDefenderStrength) / iAttackerStrength;
				iExperience = range(iExperience, GC.getDefineINT("MIN_EXPERIENCE_PER_COMBAT"), GC.getDefineINT("MAX_EXPERIENCE_PER_COMBAT"));
			}

			int iDefExperienceKill = (pAttacker->defenseXPValue() * iAttackerStrength) / iDefenderStrength;
			iDefExperienceKill = range(iDefExperienceKill, GC.getDefineINT("MIN_EXPERIENCE_PER_COMBAT"), GC.getDefineINT("MAX_EXPERIENCE_PER_COMBAT"));

			int iBonusAttackerXP = (iExperience * iAttackerExperienceModifier) / 100;
			int iBonusDefenderXP = (iDefExperienceKill * iDefenderExperienceModifier) / 100;
			int iBonusWithdrawXP = (GC.getDefineINT("EXPERIENCE_FROM_WITHDRAWL") * iAttackerExperienceModifier) / 100;


			//The following code adjusts the XP for barbarian encounters.  In standard game, barb and animal xp cap is 10,5 respectively.
			if (pDefender->isBarbarian()) {
				if (pDefender->isAnimal()) {
					//animal
					iExperience = range(iExperience, 0, GC.getDefineINT("ANIMAL_MAX_XP_VALUE") - (pAttacker->getExperience()));
					if (iExperience < 0) {
						iExperience = 0;
					}
					iWithdrawXP = range(iWithdrawXP, 0, GC.getDefineINT("ANIMAL_MAX_XP_VALUE") - (pAttacker->getExperience()));
					if (iWithdrawXP < 0) {
						iWithdrawXP = 0;
					}
					iBonusAttackerXP = range(iBonusAttackerXP, 0, GC.getDefineINT("ANIMAL_MAX_XP_VALUE") - (pAttacker->getExperience() + iExperience));
					if (iBonusAttackerXP < 0) {
						iBonusAttackerXP = 0;
					}
					iBonusWithdrawXP = range(iBonusWithdrawXP, 0, GC.getDefineINT("ANIMAL_MAX_XP_VALUE") - (pAttacker->getExperience() + iWithdrawXP));
					if (iBonusWithdrawXP < 0) {
						iBonusWithdrawXP = 0;
					}
				} else {
					//normal barbarian
					iExperience = range(iExperience, 0, GC.getDefineINT("BARBARIAN_MAX_XP_VALUE") - pAttacker->getExperience());
					if (iExperience < 0) {
						iExperience = 0;
					}
					iWithdrawXP = range(iWithdrawXP, 0, GC.getDefineINT("BARBARIAN_MAX_XP_VALUE") - (pAttacker->getExperience()));
					if (iWithdrawXP < 0) {
						iWithdrawXP = 0;
					}
					iBonusAttackerXP = range(iBonusAttackerXP, 0, GC.getDefineINT("BARBARIAN_MAX_XP_VALUE") - (pAttacker->getExperience() + iExperience));
					if (iBonusAttackerXP < 0) {
						iBonusAttackerXP = 0;
					}
					iBonusWithdrawXP = range(iBonusWithdrawXP, 0, GC.getDefineINT("BARBARIAN_MAX_XP_VALUE") - (pAttacker->getExperience() + iWithdrawXP));
					if (iBonusWithdrawXP < 0) {
						iBonusWithdrawXP = 0;
					}
				}
			}

			int iNeededRoundsAttacker = (pDefender->currHitPoints() - pDefender->maxHitPoints() + pAttacker->combatLimit() - (((pAttacker->combatLimit()) == pDefender->maxHitPoints()) ? 1 : 0)) / iDamageToDefender + 1;
			//The extra term introduced here was to account for the incorrect way it treated units that had combatLimits.
			//A catapult that deals 25HP per round, and has a combatLimit of 75HP must deal four successful hits before it kills the warrior -not 3.  This is proved in the way CvUnit::resolvecombat works
			// The old formula (with just a plain -1 instead of a conditional -1 or 0) was incorrectly saying three.

			int iNeededRoundsDefender = (pAttacker->currHitPoints() - 1) / iDamageToAttacker + 1;

			int iDefenderHitLimit = pDefender->maxHitPoints() - pAttacker->combatLimit();

			//NOW WE CALCULATE SOME INTERESTING STUFF :)

			float E_HP_Att = 0.0f;//expected damage dealt to attacker
			float E_HP_Def = 0.0f;
			float E_HP_Att_Withdraw; //Expected hitpoints for attacker if attacker withdraws (not the same as retreat)
			float E_HP_Att_Victory; //Expected hitpoints for attacker if attacker kills defender
			int E_HP_Att_Retreat = (pAttacker->currHitPoints()) - (iNeededRoundsDefender - 1) * iDamageToAttacker;//this one is predetermined easily
			float E_HP_Def_Withdraw;
			float E_HP_Def_Defeat; // if attacker dies
			//Note E_HP_Def is the same for if the attacker withdraws or dies

			float AttackerUnharmed = getCombatOddsSpecific(pAttacker, pDefender, 0, iNeededRoundsAttacker);
			float DefenderUnharmed = getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender, 0);
			DefenderUnharmed += getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender - 1, 0);//attacker withdraws or retreats

			float prob_bottom_Att_HP; // The probability the attacker exits combat with min HP
			float prob_bottom_Def_HP; // The probability the defender exits combat with min HP



			if (ACO_debug) {
				szTempBuffer.Format(L"E[HP ATTACKER]");
				szString.append(szTempBuffer.GetCString());
			}
			// already covers both possibility of defender not being killed AND being killed
			for (int n_A = 0; n_A < iNeededRoundsDefender; n_A++) {
				E_HP_Att += ((pAttacker->currHitPoints()) - n_A * iDamageToAttacker) * getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker);

				if (ACO_debug) {
					szTempBuffer.Format(L"+%d * %.2f%%  (Def %d) (%d:%d)", ((pAttacker->currHitPoints()) - n_A * iDamageToAttacker), 100.0f * getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker), iDefenderHitLimit, n_A, iNeededRoundsAttacker);
					szString.append(NEWLINE);
					szString.append(szTempBuffer.GetCString());
				}
			}
			E_HP_Att_Victory = E_HP_Att;//NOT YET NORMALISED
			E_HP_Att_Withdraw = E_HP_Att;//NOT YET NORMALIZED
			prob_bottom_Att_HP = getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender - 1, iNeededRoundsAttacker);
			if ((pAttacker->withdrawalProbability()) > 0) {
				// if withdraw odds involved
				if (ACO_debug) {
					szTempBuffer.Format(L"Attacker retreat odds");
					szString.append(NEWLINE);
					szString.append(szTempBuffer.GetCString());
				}
				for (int n_D = 0; n_D < iNeededRoundsAttacker; n_D++) {
					E_HP_Att += ((pAttacker->currHitPoints()) - (iNeededRoundsDefender - 1) * iDamageToAttacker) * getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender - 1, n_D);
					prob_bottom_Att_HP += getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender - 1, n_D);
					if (ACO_debug) {
						szTempBuffer.Format(L"+%d * %.2f%%  (Def %d) (%d:%d)", ((pAttacker->currHitPoints()) - (iNeededRoundsDefender - 1) * iDamageToAttacker), 100.0f * getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender - 1, n_D), (pDefender->currHitPoints()) - n_D * iDamageToDefender, iNeededRoundsDefender - 1, n_D);
						szString.append(NEWLINE);
						szString.append(szTempBuffer.GetCString());
					}
				}
			}
			// finished with the attacker HP I think.

			if (ACO_debug) {
				szTempBuffer.Format(L"E[HP DEFENDER]\nOdds that attacker dies or retreats");
				szString.append(NEWLINE);
				szString.append(szTempBuffer.GetCString());
			}
			for (int n_D = 0; n_D < iNeededRoundsAttacker; n_D++) {
				E_HP_Def += ((pDefender->currHitPoints()) - n_D * iDamageToDefender) * (getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender, n_D) + getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender - 1, n_D));
				if (ACO_debug) {
					szTempBuffer.Format(L"+%d * %.2f%%  (Att 0 or %d) (%d:%d)", ((pDefender->currHitPoints()) - n_D * iDamageToDefender), 100.0f * (getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender, n_D) + getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender - 1, n_D)), (pAttacker->currHitPoints()) - (iNeededRoundsDefender - 1) * iDamageToAttacker, iNeededRoundsDefender, n_D);
					szString.append(NEWLINE);
					szString.append(szTempBuffer.GetCString());
				}
			}
			prob_bottom_Def_HP = getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender, iNeededRoundsAttacker - 1);
			E_HP_Def_Defeat = E_HP_Def;
			E_HP_Def_Withdraw = 0.0f;

			if (pAttacker->combatLimit() < (pDefender->maxHitPoints()))//if attacker has a combatLimit (eg. catapult)
			{
				if (pAttacker->combatLimit() == iDamageToDefender * (iNeededRoundsAttacker - 1)) {
					//Then we have an odd situation because the last successful hit by an attacker will do 0 damage, and doing either iNeededRoundsAttacker or iNeededRoundsAttacker-1 will cause the same damage
					if (ACO_debug) {
						szTempBuffer.Format(L"Odds that attacker withdraws at combatLimit (abnormal)");
						szString.append(NEWLINE);
						szString.append(szTempBuffer.GetCString());
					}
					for (int n_A = 0; n_A < iNeededRoundsDefender; n_A++) {
						E_HP_Def += (float)iDefenderHitLimit * getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker);
						E_HP_Def_Withdraw += (float)iDefenderHitLimit * getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker);
						prob_bottom_Def_HP += getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker);
						if (ACO_debug) {
							szTempBuffer.Format(L"+%d * %.2f%%  (Att %d) (%d:%d)", iDefenderHitLimit, 100.0f * getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker), 100 - n_A * iDamageToAttacker, n_A, iNeededRoundsAttacker);
							szString.append(NEWLINE);
							szString.append(szTempBuffer.GetCString());
						}
					}
				} else // normal situation
				{
					if (ACO_debug) {
						szTempBuffer.Format(L"Odds that attacker withdraws at combatLimit (normal)", pAttacker->combatLimit());
						szString.append(NEWLINE);
						szString.append(szTempBuffer.GetCString());
					}

					for (int n_A = 0; n_A < iNeededRoundsDefender; n_A++) {

						E_HP_Def += (float)iDefenderHitLimit * getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker);
						E_HP_Def_Withdraw += (float)iDefenderHitLimit * getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker);
						prob_bottom_Def_HP += getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker);
						if (ACO_debug) {
							szTempBuffer.Format(L"+%d * %.2f%%  (Att %d) (%d:%d)", iDefenderHitLimit, 100.0f * getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker), GC.getMAX_HIT_POINTS() - n_A * iDamageToAttacker, n_A, iNeededRoundsAttacker);
							szString.append(NEWLINE);
							szString.append(szTempBuffer.GetCString());
						}
					}//for
				}//else
			}
			if (ACO_debug) {
				szString.append(NEWLINE);
			}

			float Scaling_Factor = 1.6f;//how many pixels per 1% of odds

			float AttackerKillOdds = 0.0f;
			float PullOutOdds = 0.0f;//Withdraw odds
			float RetreatOdds = 0.0f;
			float DefenderKillOdds = 0.0f;

			float CombatRatio = ((float)(pAttacker->currCombatStr(NULL, NULL))) / ((float)(pDefender->currCombatStr(pPlot, pAttacker)));
			// THE ALL-IMPORTANT COMBATRATIO


			float AttXP = (pDefender->attackXPValue()) / CombatRatio;
			float DefXP = (pAttacker->defenseXPValue()) * CombatRatio;// These two values are simply for the Unrounded XP display

			// General odds
			if (pAttacker->combatLimit() == (pDefender->maxHitPoints())) //ie. we can kill the defender... I hope this is the most general form
			{
				for (int n_A = 0; n_A < iNeededRoundsDefender; n_A++) {
					AttackerKillOdds += getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker);
				}//for
			} else {
				// else we cannot kill the defender (eg. catapults attacking)
				for (int n_A = 0; n_A < iNeededRoundsDefender; n_A++) {
					PullOutOdds += getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker);
				}//for
			}
			if ((pAttacker->withdrawalProbability()) > 0) {
				for (int n_D = 0; n_D < iNeededRoundsAttacker; n_D++) {
					RetreatOdds += getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender - 1, n_D);
				}//for
			}
			for (int n_D = 0; n_D < iNeededRoundsAttacker; n_D++) {
				DefenderKillOdds += getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender, n_D);
			}//for

			if (iView & getOptionINT("ACO__ShowSurvivalOdds", 3)) {
				szTempBuffer.Format(L"%.2f%%", 100.0f * (AttackerKillOdds + RetreatOdds + PullOutOdds));
				szTempBuffer2.Format(L"%.2f%%", 100.0f * (RetreatOdds + PullOutOdds + DefenderKillOdds));
				szString.append(gDLL->getText("TXT_ACO_SurvivalOdds"));
				szString.append(gDLL->getText("TXT_ACO_VS", szTempBuffer.GetCString(), szTempBuffer2.GetCString()));
				szString.append(NEWLINE);
			}

			if (pAttacker->withdrawalProbability() >= 100) {
				// a rare situation indeed
				szString.append(gDLL->getText("TXT_ACO_SurvivalGuaranteed"));
				szString.append(NEWLINE);
			}

			float prob1 = 100.0f * (AttackerKillOdds + PullOutOdds);//up to win odds
			float prob2 = prob1 + 100.0f * RetreatOdds;//up to retreat odds

			float prob = 100.0f * (AttackerKillOdds + RetreatOdds + PullOutOdds);
			int pixels_left = 199;// 1 less than 200 to account for right end bar
			int pixels = (2 * ((int)(prob1 + 0.5))) - 1;  // 1% per pixel // subtracting one to account for left end bar
			int fullBlocks = pixels / 10;
			int lastBlock = pixels % 10;

			szString.append(L"<img=Art/ACO/green_bar_left_end.dds>");
			for (int i = 0; i < fullBlocks; ++i) {
				szString.append(L"<img=Art/ACO/green_bar_10.dds>");
				pixels_left -= 10;
			}
			if (lastBlock > 0) {
				szTempBuffer2.Format(L"<img=Art/ACO/green_bar_%d.dds>", lastBlock);
				szString.append(szTempBuffer2);
				pixels_left -= lastBlock;
			}


			pixels = 2 * ((int)(prob2 + 0.5)) - (pixels + 1);//the number up to the next one...
			fullBlocks = pixels / 10;
			lastBlock = pixels % 10;
			for (int i = 0; i < fullBlocks; ++i) {
				szString.append(L"<img=Art/ACO/yellow_bar_10.dds>");
				pixels_left -= 10;
			}
			if (lastBlock > 0) {
				szTempBuffer2.Format(L"<img=Art/ACO/yellow_bar_%d.dds>", lastBlock);
				szString.append(szTempBuffer2);
				pixels_left -= lastBlock;
			}

			fullBlocks = pixels_left / 10;
			lastBlock = pixels_left % 10;
			for (int i = 0; i < fullBlocks; ++i) {
				szString.append(L"<img=Art/ACO/red_bar_10.dds>");
			}
			if (lastBlock > 0) {
				szTempBuffer2.Format(L"<img=Art/ACO/red_bar_%d.dds>", lastBlock);
				szString.append(szTempBuffer2);
			}

			szString.append(L"<img=Art/ACO/red_bar_right_end.dds> ");


			szString.append(NEWLINE);
			if (pAttacker->combatLimit() == (pDefender->maxHitPoints())) {
				szTempBuffer.Format(L": " SETCOLR L"%.2f%% " L"%d" ENDCOLR, TEXT_COLOR("COLOR_POSITIVE_TEXT"), 100.0f * AttackerKillOdds, iExperience);
				szString.append(gDLL->getText("TXT_ACO_Victory"));
				szString.append(szTempBuffer.GetCString());
				if (iAttackerExperienceModifier > 0) {
					szTempBuffer.Format(SETCOLR L"+%d" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), iBonusAttackerXP);
					szString.append(szTempBuffer.GetCString());
				}

				szString.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));
				szString.append(gDLL->getText("TXT_ACO_XP"));
				szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
				szString.append("  (");
				szString.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));
				szTempBuffer.Format(L"%.1f", E_HP_Att_Victory / AttackerKillOdds);
				szString.append(szTempBuffer.GetCString());
				szString.append(gDLL->getText("TXT_ACO_HP"));
				szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
			} else {
				szTempBuffer.Format(L": " SETCOLR L"%.2f%% " L"%d" ENDCOLR, TEXT_COLOR("COLOR_POSITIVE_TEXT"), 100.0f * PullOutOdds, GC.getDefineINT("EXPERIENCE_FROM_WITHDRAWL"));
				szString.append(gDLL->getText("TXT_ACO_Withdraw"));
				szString.append(szTempBuffer.GetCString());
				if (iAttackerExperienceModifier > 0) {
					szTempBuffer.Format(SETCOLR L"+%d" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), iBonusWithdrawXP);
					szString.append(szTempBuffer.GetCString());
				}

				szString.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));
				szString.append(gDLL->getText("TXT_ACO_XP"));
				szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
				szString.append("  (");
				szString.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));
				szTempBuffer.Format(L"%.1f", E_HP_Att_Withdraw / PullOutOdds);
				szString.append(szTempBuffer.GetCString());
				szString.append(gDLL->getText("TXT_ACO_HP"));
				szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
				szString.append(",");
				szString.append(gDLL->getText("TXT_KEY_COLOR_NEGATIVE"));
				szTempBuffer.Format(L"%d", iDefenderHitLimit);
				szString.append(szTempBuffer.GetCString());
				szString.append(gDLL->getText("TXT_ACO_HP"));
				szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
			}
			szString.append(")");

			if (iDefenderOdds == 0) {
				szString.append(gDLL->getText("TXT_ACO_GuaranteedNoDefenderHit"));
				DefenderKillOdds = 0.0f;
			}

			if ((pAttacker->withdrawalProbability()) > 0)//if there are retreat odds
			{
				szString.append(NEWLINE);
				szTempBuffer.Format(L": " SETCOLR L"%.2f%% " ENDCOLR SETCOLR L"%d" ENDCOLR, TEXT_COLOR("COLOR_UNIT_TEXT"), 100.0f * RetreatOdds, TEXT_COLOR("COLOR_POSITIVE_TEXT"), GC.getDefineINT("EXPERIENCE_FROM_WITHDRAWL"));
				szString.append(gDLL->getText("TXT_ACO_Retreat"));
				szString.append(szTempBuffer.GetCString());
				if (iAttackerExperienceModifier > 0) {
					szTempBuffer.Format(SETCOLR L"+%d" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), iBonusWithdrawXP);
					szString.append(szTempBuffer.GetCString());
				}
				szString.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));
				szString.append(gDLL->getText("TXT_ACO_XP"));
				szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
				szString.append("  (");
				szTempBuffer.Format(SETCOLR L"%d" ENDCOLR, TEXT_COLOR("COLOR_UNIT_TEXT"), E_HP_Att_Retreat);
				szString.append(szTempBuffer.GetCString());
				szString.append(gDLL->getText("TXT_ACO_HP_NEUTRAL"));
				szString.append(")");
			}

			szString.append(NEWLINE);
			szTempBuffer.Format(L": " SETCOLR L"%.2f%% " L"%d" ENDCOLR, TEXT_COLOR("COLOR_NEGATIVE_TEXT"), 100.0f * DefenderKillOdds, iDefExperienceKill);
			szString.append(gDLL->getText("TXT_ACO_Defeat"));
			szString.append(szTempBuffer.GetCString());
			if (iDefenderExperienceModifier > 0) {
				szTempBuffer.Format(SETCOLR L"+%d" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), iBonusDefenderXP);
				szString.append(szTempBuffer.GetCString());
			}
			szString.append(gDLL->getText("TXT_KEY_COLOR_NEGATIVE"));
			szString.append(gDLL->getText("TXT_ACO_XP"));
			szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
			szString.append("  (");
			szString.append(gDLL->getText("TXT_KEY_COLOR_NEGATIVE"));
			szTempBuffer.Format(L"%.1f", (iDefenderOdds != 0 ? E_HP_Def_Defeat / (RetreatOdds + DefenderKillOdds) : 0.0));
			szString.append(szTempBuffer.GetCString());
			szString.append(gDLL->getText("TXT_ACO_HP"));
			szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
			szString.append(")");


			float HP_percent_cutoff = 0.5f; // Probabilities lower than this (in percent) will not be shown individually for the HP detail section.
			if (!getOptionBOOL("ACO__MergeShortBars", false)) {
				HP_percent_cutoff = 0.0f;
			}
			int first_combined_HP_Att = 0;
			int first_combined_HP_Def = 0;
			int last_combined_HP;
			float combined_HP_sum = 0.0f;
			BOOL bIsCondensed = false;



			//START ATTACKER DETAIL HP HERE
			// Individual bars for each attacker HP outcome.
			if (iView & getOptionINT("ACO__ShowAttackerHealthBars", 2)) {
				for (int n_A = 0; n_A < iNeededRoundsDefender - 1; n_A++) {
					float prob = 100.0f * getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker);
					if (prob > HP_percent_cutoff || n_A == 0) {
						if (bIsCondensed) // then we need to print the prev ones
						{
							int pixels = (int)(Scaling_Factor * combined_HP_sum + 0.5);  // 1% per pixel
							int fullBlocks = (pixels) / 10;
							int lastBlock = (pixels) % 10;
							szString.append(NEWLINE);
							szString.append(L"<img=Art/ACO/green_bar_left_end.dds>");
							for (int iI = 0; iI < fullBlocks; ++iI) {
								szString.append(L"<img=Art/ACO/green_bar_10.dds>");
							}
							if (lastBlock > 0) {
								szTempBuffer2.Format(L"<img=Art/ACO/green_bar_%d.dds>", lastBlock);
								szString.append(szTempBuffer2);
							}
							szString.append(L"<img=Art/ACO/green_bar_right_end.dds>");
							szString.append(L" ");

							szString.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));
							if (last_combined_HP != first_combined_HP_Att) {
								szTempBuffer.Format(L"%d", last_combined_HP);
								szString.append(szTempBuffer.GetCString());
								szString.append(gDLL->getText("TXT_ACO_HP"));
								szString.append(gDLL->getText("-"));
							}

							szTempBuffer.Format(L"%d", first_combined_HP_Att);
							szString.append(szTempBuffer.GetCString());
							szString.append(gDLL->getText("TXT_ACO_HP"));
							szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
							szTempBuffer.Format(L" %.2f%%", combined_HP_sum);
							szString.append(szTempBuffer.GetCString());

							bIsCondensed = false;//resetting
							combined_HP_sum = 0.0f;//resetting this variable
							last_combined_HP = 0;
						}

						szString.append(NEWLINE);
						int pixels = (int)(Scaling_Factor * prob + 0.5);  // 1% per pixel
						int fullBlocks = (pixels) / 10;
						int lastBlock = (pixels) % 10;
						szString.append(L"<img=Art/ACO/green_bar_left_end.dds>");
						for (int iI = 0; iI < fullBlocks; ++iI) {
							szString.append(L"<img=Art/ACO/green_bar_10.dds>");
						}
						if (lastBlock > 0) {
							szTempBuffer2.Format(L"<img=Art/ACO/green_bar_%d.dds>", lastBlock);
							szString.append(szTempBuffer2);
						}
						szString.append(L"<img=Art/ACO/green_bar_right_end.dds>");
						szString.append(L" ");

						szString.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));
						szTempBuffer.Format(L"%d", ((pAttacker->currHitPoints()) - n_A * iDamageToAttacker));
						szString.append(szTempBuffer.GetCString());
						szString.append(gDLL->getText("TXT_ACO_HP"));
						szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
						szTempBuffer.Format(L" %.2f%%", prob);
						szString.append(szTempBuffer.GetCString());
					} else // we add to the condensed list
					{
						bIsCondensed = true;
						first_combined_HP_Att = std::max(first_combined_HP_Att, ((pAttacker->currHitPoints()) - n_A * iDamageToAttacker));
						last_combined_HP = ((pAttacker->currHitPoints()) - n_A * iDamageToAttacker);
						combined_HP_sum += prob;
					}
				}

				if (bIsCondensed) // then we need to print the prev ones
				{
					szString.append(NEWLINE);
					int pixels = (int)(Scaling_Factor * combined_HP_sum + 0.5);  // 1% per pixel
					int fullBlocks = (pixels) / 10;
					int lastBlock = (pixels) % 10;

					szString.append(L"<img=Art/ACO/green_bar_left_end.dds>");
					for (int iI = 0; iI < fullBlocks; ++iI) {
						szString.append(L"<img=Art/ACO/green_bar_10.dds>");
					}
					if (lastBlock > 0) {
						szTempBuffer2.Format(L"<img=Art/ACO/green_bar_%d.dds>", lastBlock);
						szString.append(szTempBuffer2);
					}

					szString.append(L"<img=Art/ACO/green_bar_right_end.dds>");
					szString.append(L" ");

					szString.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));
					if (last_combined_HP != first_combined_HP_Att) {
						szTempBuffer.Format(L"%d", last_combined_HP);
						szString.append(szTempBuffer.GetCString());
						szString.append(gDLL->getText("TXT_ACO_HP"));
						szString.append(gDLL->getText("-"));
					}
					szTempBuffer.Format(L"%d", first_combined_HP_Att);
					szString.append(szTempBuffer.GetCString());
					szString.append(gDLL->getText("TXT_ACO_HP"));
					szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
					szTempBuffer.Format(L" %.2f%%", combined_HP_sum);
					szString.append(szTempBuffer.GetCString());

					bIsCondensed = false;//resetting
					combined_HP_sum = 0.0f;//resetting this variable
					last_combined_HP = 0;
				}
				// At the moment I am not allowing the lowest Attacker HP value to be condensed, as it would be confusing if it includes retreat odds
				// I may include this in the future though, but probably only if retreat odds are zero.

				float prob_victory = 100.0f * getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender - 1, iNeededRoundsAttacker);
				float prob_retreat = 100.0f * RetreatOdds;

				szString.append(NEWLINE);
				int green_pixels = (int)(Scaling_Factor * prob_victory + 0.5);
				int yellow_pixels = (int)(Scaling_Factor * (prob_retreat + prob_victory) + 0.5) - green_pixels;//makes the total length of the bar more accurate - more important than the length of the pieces
				green_pixels += 1;//we put an extra 2 on every one of the bar pixel counts
				if (yellow_pixels >= 1) {
					yellow_pixels += 1;
				} else {
					green_pixels += 1;
				}
				szString.append(L"<img=Art/ACO/green_bar_left_end.dds>");
				green_pixels--;

				green_pixels--;//subtracting off the right end
				int fullBlocks = green_pixels / 10;
				int lastBlock = green_pixels % 10;
				for (int iI = 0; iI < fullBlocks; ++iI) {
					szString.append(L"<img=Art/ACO/green_bar_10.dds>");
				}//for
				if (lastBlock > 0) {
					szTempBuffer2.Format(L"<img=Art/ACO/green_bar_%d.dds>", lastBlock);
					szString.append(szTempBuffer2);
				}//if
				if (yellow_pixels >= 1)// then there will at least be a right end yellow pixel
				{
					yellow_pixels--;//subtracting off right end
					fullBlocks = yellow_pixels / 10;
					lastBlock = yellow_pixels % 10;
					for (int iI = 0; iI < fullBlocks; ++iI) {
						szString.append(L"<img=Art/ACO/yellow_bar_10.dds>");
					}//for
					if (lastBlock > 0) {
						szTempBuffer2.Format(L"<img=Art/ACO/yellow_bar_%d.dds>", lastBlock);
						szString.append(szTempBuffer2);
					}
					szString.append(L"<img=Art/ACO/yellow_bar_right_end.dds>");
					//finished
				} else {
					szString.append(L"<img=Art/ACO/green_bar_right_end.dds>");
					//finished
				}//else if

				szString.append(L" ");
				szString.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));
				szTempBuffer.Format(L"%d", ((pAttacker->currHitPoints()) - (iNeededRoundsDefender - 1) * iDamageToAttacker));
				szString.append(szTempBuffer.GetCString());
				szString.append(gDLL->getText("TXT_ACO_HP"));
				szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
				szTempBuffer.Format(L" %.2f%%", prob_victory + prob_retreat);
				szString.append(szTempBuffer.GetCString());
			}
			//END ATTACKER DETAIL HP HERE


			//START DEFENDER DETAIL HP HERE
			first_combined_HP_Def = pDefender->currHitPoints();
			if (iView & getOptionINT("ACO__ShowDefenderHealthBars", 2)) {
				float prob = 0.0f;
				int def_HP;
				for (int n_D = iNeededRoundsAttacker; n_D >= 1; n_D--)//
				{
					if (pAttacker->combatLimit() >= pDefender->maxHitPoints())// a unit with a combat limit
					{
						if (n_D == iNeededRoundsAttacker) {
							n_D--;//we don't need to do HP for when the unit is dead.
						}
					}

					def_HP = std::max((pDefender->currHitPoints()) - n_D * iDamageToDefender, (pDefender->maxHitPoints() - pAttacker->combatLimit()));

					if ((pDefender->maxHitPoints() - pAttacker->combatLimit()) == pDefender->currHitPoints() - (n_D - 1) * iDamageToDefender) {
						// if abnormal
						if (n_D == iNeededRoundsAttacker) {
							n_D--;
							def_HP = (pDefender->maxHitPoints() - pAttacker->combatLimit());
							prob += 100.0f * PullOutOdds;
							prob += 100.0f * (getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender, n_D) + (getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender - 1, n_D)));
						}
					} else {
						//not abnormal
						if (n_D == iNeededRoundsAttacker) {
							prob += 100.0f * PullOutOdds;
						} else {
							prob += 100.0f * (getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender, n_D) + (getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender - 1, n_D)));
						}
					}

					if (prob > HP_percent_cutoff || (pAttacker->combatLimit() < pDefender->maxHitPoints() && (n_D == iNeededRoundsAttacker))) {
						if (bIsCondensed) // then we need to print the prev ones
						{
							szString.append(NEWLINE);

							int pixels = (int)(Scaling_Factor * combined_HP_sum + 0.5);  // 1% per pixel
							int fullBlocks = (pixels) / 10;
							int lastBlock = (pixels) % 10;
							szString.append(L"<img=Art/ACO/red_bar_left_end.dds>");
							for (int iI = 0; iI < fullBlocks; ++iI) {
								szString.append(L"<img=Art/ACO/red_bar_10.dds>");
							}
							if (lastBlock > 0) {
								szTempBuffer2.Format(L"<img=Art/ACO/red_bar_%d.dds>", lastBlock);
								szString.append(szTempBuffer2);
							}
							szString.append(L"<img=Art/ACO/red_bar_right_end.dds>");
							szString.append(L" ");
							szString.append(gDLL->getText("TXT_KEY_COLOR_NEGATIVE"));
							szTempBuffer.Format(L"%dHP", first_combined_HP_Def);
							szString.append(szTempBuffer.GetCString());
							szString.append(gDLL->getText("TXT_ACO_HP"));
							if (first_combined_HP_Def != last_combined_HP) {
								szString.append("-");
								szTempBuffer.Format(L"%d", last_combined_HP);
								szString.append(szTempBuffer.GetCString());
								szString.append(gDLL->getText("TXT_ACO_HP"));
							}
							szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
							szTempBuffer.Format(L" %.2f%%", combined_HP_sum);
							szString.append(szTempBuffer.GetCString());

							bIsCondensed = false;//resetting
							combined_HP_sum = 0.0f;//resetting this variable
						}

						szString.append(NEWLINE);
						int pixels = (int)(Scaling_Factor * prob + 0.5);  // 1% per pixel
						int fullBlocks = (pixels) / 10;
						int lastBlock = (pixels) % 10;
						szString.append(L"<img=Art/ACO/red_bar_left_end.dds>");
						for (int iI = 0; iI < fullBlocks; ++iI) {
							szString.append(L"<img=Art/ACO/red_bar_10.dds>");
						}
						if (lastBlock > 0) {
							szTempBuffer2.Format(L"<img=Art/ACO/red_bar_%d.dds>", lastBlock);
							szString.append(szTempBuffer2);
						}
						szString.append(L"<img=Art/ACO/red_bar_right_end.dds>");
						szString.append(L" ");

						szTempBuffer.Format(SETCOLR L"%d" ENDCOLR, TEXT_COLOR("COLOR_NEGATIVE_TEXT"), def_HP);
						szString.append(szTempBuffer.GetCString());
						szString.append(gDLL->getText("TXT_KEY_COLOR_NEGATIVE"));
						szString.append(gDLL->getText("TXT_ACO_HP"));
						szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
						szTempBuffer.Format(L" %.2f%%", prob);
						szString.append(szTempBuffer.GetCString());
					} else {
						bIsCondensed = true;
						first_combined_HP_Def = (std::min(first_combined_HP_Def, def_HP));
						last_combined_HP = std::max(((pDefender->currHitPoints()) - n_D * iDamageToDefender), pDefender->maxHitPoints() - pAttacker->combatLimit());
						combined_HP_sum += prob;
					}
					prob = 0.0f;
				}//for n_D

				if (bIsCondensed && iNeededRoundsAttacker > 1) // then we need to print the prev ones
					// the reason we need iNeededRoundsAttacker to be greater than 1 is that if it's equal to 1 then we end up with the defender detailed HP bar show up twice, because it will also get printed below
				{
					szString.append(NEWLINE);
					int pixels = (int)(Scaling_Factor * combined_HP_sum + 0.5);  // 1% per pixel
					int fullBlocks = (pixels) / 10;
					int lastBlock = (pixels) % 10;
					szString.append(L"<img=Art/ACO/red_bar_left_end.dds>");
					for (int iI = 0; iI < fullBlocks; ++iI) {
						szString.append(L"<img=Art/ACO/red_bar_10.dds>");
					}
					if (lastBlock > 0) {
						szTempBuffer2.Format(L"<img=Art/ACO/red_bar_%d.dds>", lastBlock);
						szString.append(szTempBuffer2);
					}
					szString.append(L"<img=Art/ACO/red_bar_right_end.dds>");
					szString.append(L" ");
					szString.append(gDLL->getText("TXT_KEY_COLOR_NEGATIVE"));
					szTempBuffer.Format(L"%d", first_combined_HP_Def);
					szString.append(szTempBuffer.GetCString());
					szString.append(gDLL->getText("TXT_ACO_HP"));
					if (first_combined_HP_Def != last_combined_HP) {
						szTempBuffer.Format(L"-%d", last_combined_HP);
						szString.append(szTempBuffer.GetCString());
						szString.append(gDLL->getText("TXT_ACO_HP"));
					}
					szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
					szTempBuffer.Format(L" %.2f%%", combined_HP_sum);
					szString.append(szTempBuffer.GetCString());

					bIsCondensed = false;//resetting
					combined_HP_sum = 0.0f;//resetting this variable
				}

				//print the unhurt value...always

				prob = 100.0f * (getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender, 0) + (getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender - 1, 0)));
				int pixels = (int)(Scaling_Factor * prob + 0.5);  // 1% per pixel
				int fullBlocks = (pixels) / 10;
				int lastBlock = (pixels) % 10;

				szString.append(NEWLINE);
				szString.append(L"<img=Art/ACO/red_bar_left_end.dds>");
				for (int iI = 0; iI < fullBlocks; ++iI) {
					szString.append(L"<img=Art/ACO/red_bar_10.dds>");
				}
				if (lastBlock > 0) {
					szTempBuffer2.Format(L"<img=Art/ACO/red_bar_%d.dds>", lastBlock);
					szString.append(szTempBuffer2);
				}
				szString.append(L"<img=Art/ACO/red_bar_right_end.dds>");
				szString.append(L" ");
				szString.append(gDLL->getText("TXT_KEY_COLOR_NEGATIVE"));
				szTempBuffer.Format(L"%d", pDefender->currHitPoints());
				szString.append(szTempBuffer.GetCString());
				szString.append(gDLL->getText("TXT_ACO_HP"));
				szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
				szTempBuffer.Format(L" %.2f%%", prob);
				szString.append(szTempBuffer.GetCString());
			}
			//END DEFENDER DETAIL HP HERE

			szString.append(NEWLINE);

			int ACO_ShowBasicInfo = getOptionINT("ACO__ShowBasicInfo", 3);
			if (iView & ACO_ShowBasicInfo) {
				szTempBuffer.Format(SETCOLR L"%d" ENDCOLR L", " SETCOLR L"%d " ENDCOLR, TEXT_COLOR("COLOR_POSITIVE_TEXT"), iDamageToDefender, TEXT_COLOR("COLOR_NEGATIVE_TEXT"), iDamageToAttacker);
				szString.append(NEWLINE);
				szString.append(szTempBuffer.GetCString());
				szString.append(gDLL->getText("TXT_ACO_HP"));
				szString.append(" ");
				szString.append(gDLL->getText("TXT_ACO_MULTIPLY"));
				szTempBuffer.Format(L" " SETCOLR L"%d" ENDCOLR L", " SETCOLR L"%d " ENDCOLR, TEXT_COLOR("COLOR_POSITIVE_TEXT"), iNeededRoundsAttacker, TEXT_COLOR("COLOR_NEGATIVE_TEXT"), iNeededRoundsDefender);
				szString.append(szTempBuffer.GetCString());
				szString.append(gDLL->getText("TXT_ACO_HitsAt"));
				szTempBuffer.Format(SETCOLR L" %.1f%%" ENDCOLR, TEXT_COLOR("COLOR_POSITIVE_TEXT"), float(iAttackerOdds) * 100.0f / float(GC.getDefineINT("COMBAT_DIE_SIDES")));
				szString.append(szTempBuffer.GetCString());
			}
			if (!(iView & getOptionINT("ACO__ShowExperienceRange", 2)) || pAttacker->combatLimit() < pDefender->maxHitPoints()) { //medium and high only
				if (iView & ACO_ShowBasicInfo) {
					szTempBuffer.Format(L". R=" SETCOLR L"%.2f" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), CombatRatio);
					szString.append(szTempBuffer.GetCString());
				}
			} else {
				//we do an XP range display
				//This should hopefully now work for any max and min XP values.

				if (pAttacker->combatLimit() == (pDefender->maxHitPoints())) {
					FAssert(GC.getDefineINT("MAX_EXPERIENCE_PER_COMBAT") > GC.getDefineINT("MIN_EXPERIENCE_PER_COMBAT")); //ensuring the differences is at least 1
					int size = GC.getDefineINT("MAX_EXPERIENCE_PER_COMBAT") - GC.getDefineINT("MIN_EXPERIENCE_PER_COMBAT");
					float* CombatRatioThresholds = new float[size];

					for (int i = 0; i < size; i++) //setup the array
					{
						CombatRatioThresholds[i] = ((float)(pDefender->attackXPValue())) / ((float)(GC.getDefineINT("MAX_EXPERIENCE_PER_COMBAT") - i));
						//For standard game, this is the list created:
						//  {4/10, 4/9, 4/8,
						//   4/7, 4/6, 4/5,
						//   4/4, 4/3, 4/2}
					}
					for (int i = size - 1; i >= 0; i--) // find which range we are in
					{
						//starting at i = 8, going through to i = 0
						if (CombatRatio > CombatRatioThresholds[i]) {

							if (i == (size - 1))//highest XP value already
							{
								szString.append(NEWLINE);
								szTempBuffer.Format(L"(%.2f:%d", CombatRatioThresholds[i], GC.getDefineINT("MIN_EXPERIENCE_PER_COMBAT") + 1);
								szString.append(szTempBuffer.GetCString());
								szString.append(gDLL->getText("TXT_ACO_XP"));
								szTempBuffer.Format(L"), (R=" SETCOLR L"%.2f" ENDCOLR L":%d", TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), CombatRatio, iExperience);
								szString.append(szTempBuffer.GetCString());
								szString.append(gDLL->getText("TXT_ACO_XP"));
								szString.append(")");
							} else // normal situation
							{
								szString.append(NEWLINE);
								szTempBuffer.Format(L"(%.2f:%d", CombatRatioThresholds[i], GC.getDefineINT("MAX_EXPERIENCE_PER_COMBAT") - i);
								szString.append(szTempBuffer.GetCString());
								szString.append(gDLL->getText("TXT_ACO_XP"));
								szTempBuffer.Format(L"), (R=" SETCOLR L"%.2f" ENDCOLR L":%d", TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), CombatRatio, GC.getDefineINT("MAX_EXPERIENCE_PER_COMBAT") - (i + 1));
								szString.append(szTempBuffer.GetCString());
								szString.append(gDLL->getText("TXT_ACO_XP"));
								szTempBuffer.Format(L"), (>%.2f:%d", CombatRatioThresholds[i + 1], GC.getDefineINT("MAX_EXPERIENCE_PER_COMBAT") - (i + 2));
								szString.append(szTempBuffer.GetCString());
								szString.append(gDLL->getText("TXT_ACO_XP"));
								szString.append(")");
							}
							break;

						} else//very rare (ratio less than or equal to 0.4)
						{
							if (i == 0)//maximum XP
							{
								szString.append(NEWLINE);
								szTempBuffer.Format(L"(R=" SETCOLR L"%.2f" ENDCOLR L":%d", TEXT_COLOR("COLOR_POSITIVE_TEXT"), CombatRatio, GC.getDefineINT("MAX_EXPERIENCE_PER_COMBAT"));
								szString.append(szTempBuffer.GetCString());

								szTempBuffer.Format(L"), (>%.2f:%d", CombatRatioThresholds[i], GC.getDefineINT("MAX_EXPERIENCE_PER_COMBAT") - 1);
								szString.append(szTempBuffer.GetCString());
								szString.append(gDLL->getText("TXT_ACO_XP"));
								szString.append(")");
								break;
							}//if
						}// else if
					}//for
					delete CombatRatioThresholds;
					//throw away the array
				}//if
			} // else if
			//Finished Showing XP range display


			if (iView & getOptionINT("ACO__ShowAverageHealth", 2)) {
				szTempBuffer.Format(L"%.1f", E_HP_Att);
				szTempBuffer2.Format(L"%.1f", E_HP_Def);
				szString.append(gDLL->getText("TXT_ACO_AverageHP"));
				szString.append(gDLL->getText("TXT_ACO_VS", szTempBuffer.GetCString(), szTempBuffer2.GetCString()));
			}

			if (iView & getOptionINT("ACO__ShowUnharmedOdds", 2)) {
				szTempBuffer.Format(L"%.2f%%", 100.0f * AttackerUnharmed);
				szTempBuffer2.Format(L"%.2f%%", 100.0f * DefenderUnharmed);
				szString.append(gDLL->getText("TXT_ACO_Unharmed"));
				szString.append(gDLL->getText("TXT_ACO_VS", szTempBuffer.GetCString(), szTempBuffer2.GetCString()));
			}

			if (iView & getOptionINT("ACO__ShowUnroundedExperience", 0)) {
				szTempBuffer.Format(L"%.2f", AttXP);
				szTempBuffer2.Format(L"%.2f", DefXP);
				szString.append(gDLL->getText("TXT_ACO_UnroundedXP"));
				szString.append(gDLL->getText("TXT_ACO_VS", szTempBuffer.GetCString(), szTempBuffer2.GetCString()));
			}

			szString.append(NEWLINE);
			if (iView & getOptionINT("ACO__ShowShiftInstructions", 1)) {
				szString.append(gDLL->getText("TXT_ACO_PressSHIFT"));
				szString.append(NEWLINE);
			}

		}//if ACO_enabled
	}

	//////////

	if (ACO_enabled) {
		szString.append(NEWLINE);

		szTempBuffer.Format(L"%.2f", ((pAttacker->getDomainType() == DOMAIN_AIR) ? pAttacker->airCurrCombatStrFloat(pDefender) : pAttacker->currCombatStrFloat(NULL, NULL)));

		if (pAttacker->isHurt()) {
			szTempBuffer.append(L" (");
			szTempBuffer.append(gDLL->getText("TXT_ACO_INJURED_HP", pAttacker->currHitPoints(), pAttacker->maxHitPoints()));
			szTempBuffer.append(L")");
		}


		szTempBuffer2.Format(L"%.2f", pDefender->currCombatStrFloat(pPlot, pAttacker));

		if (pDefender->isHurt()) {
			szTempBuffer2.append(L" (");
			szTempBuffer2.append(gDLL->getText("TXT_ACO_INJURED_HP", pDefender->currHitPoints(), pDefender->maxHitPoints()));
			szTempBuffer2.append(L")");
		}

		szString.append(gDLL->getText("TXT_ACO_VS", szTempBuffer.GetCString(), szTempBuffer2.GetCString()));

		bool ACO_ShowModifierLabels = getOptionBOOL("ACO__ShowModifierLabels", false);
		if (((!(pDefender->immuneToFirstStrikes())) && (pAttacker->maxFirstStrikes() > 0)) || (pAttacker->maxCombatStr(NULL, NULL) != pAttacker->baseCombatStr() * 100)) {
			//if attacker uninjured strength is not the same as base strength (i.e. modifiers are in effect) or first strikes exist, then
			if (ACO_ShowModifierLabels) {
				szString.append(gDLL->getText("TXT_ACO_AttackModifiers"));
			}
		}//if
		if (iView & getOptionINT("ACO__ShowAttackerInfo", 0)) {
			szString.append(NEWLINE);
			setUnitHelp(szString, pAttacker, true, true);
		}



		szString.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));

		szString.append(L' ');//XXX

		if (!(pDefender->immuneToFirstStrikes())) {
			if (pAttacker->maxFirstStrikes() > 0) {
				if (pAttacker->firstStrikes() == pAttacker->maxFirstStrikes()) {
					if (pAttacker->firstStrikes() == 1) {
						szString.append(NEWLINE);
						szString.append(gDLL->getText("TXT_KEY_UNIT_ONE_FIRST_STRIKE"));
					} else {
						szString.append(NEWLINE);
						szString.append(gDLL->getText("TXT_KEY_UNIT_NUM_FIRST_STRIKES", pAttacker->firstStrikes()));
					}
				} else {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_FIRST_STRIKE_CHANCES", pAttacker->firstStrikes(), pAttacker->maxFirstStrikes()));
				}
			}
		}

		iModifier = pAttacker->getExtraCombatPercent();

		if (iModifier != 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_EXTRA_STRENGTH", iModifier));
		}


		szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));

		szString.append(L' ');//XXX

		if (((!(pAttacker->immuneToFirstStrikes())) && (pDefender->maxFirstStrikes() > 0)) || (pDefender->maxCombatStr(pPlot, pAttacker) != pDefender->baseCombatStr() * 100)) {
			//if attacker uninjured strength is not the same as base strength (i.e. modifiers are in effect) or first strikes exist, then
			if (ACO_ShowModifierLabels) {
				szString.append(gDLL->getText("TXT_ACO_DefenseModifiers"));
			}
		}//if
		if (iView & getOptionINT("ACO__ShowDefenderInfo", 3)) {
			szString.append(NEWLINE);
			setUnitHelp(szString, pDefender, true, true);
		}


		if (iView & getOptionINT("ACO__ShowDefenceModifiers", 3)) {
			//if defense modifiers are enabled - recommend leaving this on unless Total defense Modifier is enabled

			szString.append(gDLL->getText("TXT_KEY_COLOR_NEGATIVE"));

			szString.append(L' ');//XXX

			if (!(pAttacker->immuneToFirstStrikes())) {
				if (pDefender->maxFirstStrikes() > 0) {
					if (pDefender->firstStrikes() == pDefender->maxFirstStrikes()) {
						if (pDefender->firstStrikes() == 1) {
							szString.append(NEWLINE);
							szString.append(gDLL->getText("TXT_KEY_UNIT_ONE_FIRST_STRIKE"));
						} else {
							szString.append(NEWLINE);
							szString.append(gDLL->getText("TXT_KEY_UNIT_NUM_FIRST_STRIKES", pDefender->firstStrikes()));
						}
					} else {
						szString.append(NEWLINE);
						szString.append(gDLL->getText("TXT_KEY_UNIT_FIRST_STRIKE_CHANCES", pDefender->firstStrikes(), pDefender->maxFirstStrikes()));
					}
				}
			}

			if (!(pAttacker->isRiver())) {
				if (pAttacker->plot()->isRiverCrossing(directionXY(pAttacker->plot(), pPlot))) {
					iModifier = GC.getRIVER_ATTACK_MODIFIER();

					if (iModifier != 0) {
						szString.append(NEWLINE);
						szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_RIVER_MOD", -(iModifier)));
					}
				}
			}

			if (!(pAttacker->isAmphib())) {
				if (!(pPlot->isWater()) && pAttacker->plot()->isWater()) {
					iModifier = GC.getAMPHIB_ATTACK_MODIFIER();

					if (iModifier != 0) {
						szString.append(NEWLINE);
						szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_AMPHIB_MOD", -(iModifier)));
					}
				}
			}

			iModifier = pDefender->getExtraCombatPercent();

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_EXTRA_STRENGTH", iModifier));
			}

			iModifier = pDefender->unitClassDefenseModifier(pAttacker->getUnitClassType());

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_MOD_VS_TYPE", iModifier, GC.getUnitClassInfo(pAttacker->getUnitClassType()).getTextKeyWide()));
			}

			if (pAttacker->getUnitCombatType() != NO_UNITCOMBAT) {
				iModifier = pDefender->unitCombatModifier(pAttacker->getUnitCombatType());

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_MOD_VS_TYPE", iModifier, GC.getUnitCombatInfo(pAttacker->getUnitCombatType()).getTextKeyWide()));
				}
			}

			iModifier = pDefender->domainModifier(pAttacker->getDomainType());

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_MOD_VS_TYPE", iModifier, GC.getDomainInfo(pAttacker->getDomainType()).getTextKeyWide()));
			}

			if (!(pDefender->noDefensiveBonus())) {
				iModifier = pPlot->defenseModifier(pDefender->getTeam(), (pAttacker != NULL) ? pAttacker->ignoreBuildingDefense() : true);

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_TILE_MOD", iModifier));
				}
			}

			iModifier = pDefender->fortifyModifier();

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_FORTIFY_MOD", iModifier));
			}

			if (pPlot->isCity(true, pDefender->getTeam())) {
				iModifier = pDefender->cityDefenseModifier();

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_CITY_MOD", iModifier));
				}
			}

			if (pPlot->isHills()) {
				iModifier = pDefender->hillsDefenseModifier();

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_HILLS_MOD", iModifier));
				}
			}

			if (pPlot->getFeatureType() != NO_FEATURE) {
				iModifier = pDefender->featureDefenseModifier(pPlot->getFeatureType());

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_UNIT_MOD", iModifier, GC.getFeatureInfo(pPlot->getFeatureType()).getTextKeyWide()));
				}
			} else {
				iModifier = pDefender->terrainDefenseModifier(pPlot->getTerrainType());

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_UNIT_MOD", iModifier, GC.getTerrainInfo(pPlot->getTerrainType()).getTextKeyWide()));
				}
			}

			szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));

			szString.append(L' ');//XXX

			szString.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));

			szString.append(L' ');//XXX


			iModifier = pAttacker->unitClassAttackModifier(pDefender->getUnitClassType());

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_MOD_VS_TYPE", -iModifier, GC.getUnitClassInfo(pDefender->getUnitClassType()).getTextKeyWide()));
			}

			if (pDefender->getUnitCombatType() != NO_UNITCOMBAT) {
				iModifier = pAttacker->unitCombatModifier(pDefender->getUnitCombatType());

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_MOD_VS_TYPE", -iModifier, GC.getUnitCombatInfo(pDefender->getUnitCombatType()).getTextKeyWide()));
				}
			}

			iModifier = pAttacker->domainModifier(pDefender->getDomainType());

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_MOD_VS_TYPE", -iModifier, GC.getDomainInfo(pDefender->getDomainType()).getTextKeyWide()));
			}

			if (pPlot->isCity(true, pDefender->getTeam())) {
				iModifier = pAttacker->cityAttackModifier();

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_CITY_MOD", -iModifier));
				}
			}

			if (pPlot->isHills()) {
				iModifier = pAttacker->hillsAttackModifier();

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_HILLS_MOD", -iModifier));
				}
			}

			if (pPlot->getFeatureType() != NO_FEATURE) {
				iModifier = pAttacker->featureAttackModifier(pPlot->getFeatureType());

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_UNIT_MOD", -iModifier, GC.getFeatureInfo(pPlot->getFeatureType()).getTextKeyWide()));
				}
			} else {
				iModifier = pAttacker->terrainAttackModifier(pPlot->getTerrainType());

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_UNIT_MOD", -iModifier, GC.getTerrainInfo(pPlot->getTerrainType()).getTextKeyWide()));
				}
			}

			iModifier = pAttacker->getKamikazePercent();
			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_KAMIKAZE_MOD", -iModifier));
			}

			if (pDefender->isAnimal()) {
				iModifier = -GC.getHandicapInfo(GET_PLAYER(pAttacker->getOwnerINLINE()).getHandicapType()).getAnimalCombatModifier(); // K-Mod

				iModifier += pAttacker->getUnitInfo().getAnimalCombatModifier();

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_ANIMAL_COMBAT_MOD", -iModifier));
				}
			}

			if (pDefender->isBarbarian()) {
				iModifier = -GC.getHandicapInfo(GET_PLAYER(pAttacker->getOwnerINLINE()).getHandicapType()).getBarbarianCombatModifier(); // K-Mod

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_BARBARIAN_COMBAT_MOD", -iModifier));
				}
			}
		}//if

		szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));

		szString.append(L' ');//XXX

		if (iView & getOptionINT("ACO__ShowTotalDefenceModifier", 2)) {
			if (pDefender->maxCombatStr(pPlot, pAttacker) > pDefender->baseCombatStr() * 100) // modifier is positive
			{
				szTempBuffer.Format(SETCOLR L"%d%%" ENDCOLR,
					TEXT_COLOR("COLOR_NEGATIVE_TEXT"), (((pDefender->maxCombatStr(pPlot, pAttacker))) / pDefender->baseCombatStr()) - 100);
			} else   // modifier is negative
			{
				szTempBuffer.Format(SETCOLR L"%d%%" ENDCOLR,
					TEXT_COLOR("COLOR_POSITIVE_TEXT"), (100 - ((pDefender->baseCombatStr() * 10000) / (pDefender->maxCombatStr(pPlot, pAttacker)))));
			}

			szString.append(gDLL->getText("TXT_ACO_TotalDefenseModifier"));
			szString.append(szTempBuffer.GetCString());
		}
	}//if

	/** What follows in the "else" block, is the original code **/
	else {
		//ACO is not enabled
		szOffenseOdds.Format(L"%.2f", ((pAttacker->getDomainType() == DOMAIN_AIR) ? pAttacker->airCurrCombatStrFloat(pDefender) : pAttacker->currCombatStrFloat(NULL, NULL)));
		szDefenseOdds.Format(L"%.2f", pDefender->currCombatStrFloat(pPlot, pAttacker));
		szString.append(NEWLINE);
		szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_ODDS_VS", szOffenseOdds.GetCString(), szDefenseOdds.GetCString()));

		szString.append(L' ');//XXX

		szString.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));

		szString.append(L' ');//XXX

		iModifier = pAttacker->getExtraCombatPercent();

		if (iModifier != 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_EXTRA_STRENGTH", iModifier));
		}

		iModifier = pAttacker->unitClassAttackModifier(pDefender->getUnitClassType());

		if (iModifier != 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_MOD_VS_TYPE", iModifier, GC.getUnitClassInfo(pDefender->getUnitClassType()).getTextKeyWide()));
		}

		if (pDefender->getUnitCombatType() != NO_UNITCOMBAT) {
			iModifier = pAttacker->unitCombatModifier(pDefender->getUnitCombatType());

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_MOD_VS_TYPE", iModifier, GC.getUnitCombatInfo(pDefender->getUnitCombatType()).getTextKeyWide()));
			}
		}

		iModifier = pAttacker->domainModifier(pDefender->getDomainType());

		if (iModifier != 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_MOD_VS_TYPE", iModifier, GC.getDomainInfo(pDefender->getDomainType()).getTextKeyWide()));
		}

		if (pPlot->isCity(true, pDefender->getTeam())) {
			iModifier = pAttacker->cityAttackModifier();

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_CITY_MOD", iModifier));
			}
		}

		if (pPlot->isHills()) {
			iModifier = pAttacker->hillsAttackModifier();

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_HILLS_MOD", iModifier));
			}
		}

		if (pPlot->getFeatureType() != NO_FEATURE) {
			iModifier = pAttacker->featureAttackModifier(pPlot->getFeatureType());

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_UNIT_MOD", iModifier, GC.getFeatureInfo(pPlot->getFeatureType()).getTextKeyWide()));
			}
		} else {
			iModifier = pAttacker->terrainAttackModifier(pPlot->getTerrainType());

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_UNIT_MOD", iModifier, GC.getTerrainInfo(pPlot->getTerrainType()).getTextKeyWide()));
			}
		}

		iModifier = pAttacker->getKamikazePercent();
		if (iModifier != 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_COMBAT_KAMIKAZE_MOD", iModifier));
		}

		if (pDefender->isAnimal()) {
			iModifier = -GC.getHandicapInfo(GET_PLAYER(pAttacker->getOwnerINLINE()).getHandicapType()).getAnimalCombatModifier(); // K-Mod

			iModifier += pAttacker->getUnitInfo().getAnimalCombatModifier();

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_ANIMAL_COMBAT_MOD", iModifier));
			}
		}

		if (pDefender->isBarbarian()) {
			iModifier = -GC.getHandicapInfo(GET_PLAYER(pAttacker->getOwnerINLINE()).getHandicapType()).getBarbarianCombatModifier(); // K-Mod

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_UNIT_BARBARIAN_COMBAT_MOD", iModifier));
			}
		}

		if (!(pDefender->immuneToFirstStrikes())) {
			if (pAttacker->maxFirstStrikes() > 0) {
				if (pAttacker->firstStrikes() == pAttacker->maxFirstStrikes()) {
					if (pAttacker->firstStrikes() == 1) {
						szString.append(NEWLINE);
						szString.append(gDLL->getText("TXT_KEY_UNIT_ONE_FIRST_STRIKE"));
					} else {
						szString.append(NEWLINE);
						szString.append(gDLL->getText("TXT_KEY_UNIT_NUM_FIRST_STRIKES", pAttacker->firstStrikes()));
					}
				} else {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_FIRST_STRIKE_CHANCES", pAttacker->firstStrikes(), pAttacker->maxFirstStrikes()));
				}
			}
		}

		if (pAttacker->isHurt()) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_HP", pAttacker->currHitPoints(), pAttacker->maxHitPoints()));
		}

		szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));

		szString.append(L' ');//XXX

		szString.append(gDLL->getText("TXT_KEY_COLOR_NEGATIVE"));

		szString.append(L' ');//XXX

		if (!(pAttacker->isRiver())) {
			if (pAttacker->plot()->isRiverCrossing(directionXY(pAttacker->plot(), pPlot))) {
				iModifier = GC.getRIVER_ATTACK_MODIFIER();

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_RIVER_MOD", -(iModifier)));
				}
			}
		}

		if (!(pAttacker->isAmphib())) {
			if (!(pPlot->isWater()) && pAttacker->plot()->isWater()) {
				iModifier = GC.getAMPHIB_ATTACK_MODIFIER();

				if (iModifier != 0) {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_AMPHIB_MOD", -(iModifier)));
				}
			}
		}

		iModifier = pDefender->getExtraCombatPercent();

		if (iModifier != 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_EXTRA_STRENGTH", iModifier));
		}

		iModifier = pDefender->unitClassDefenseModifier(pAttacker->getUnitClassType());

		if (iModifier != 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_MOD_VS_TYPE", iModifier, GC.getUnitClassInfo(pAttacker->getUnitClassType()).getTextKeyWide()));
		}

		if (pAttacker->getUnitCombatType() != NO_UNITCOMBAT) {
			iModifier = pDefender->unitCombatModifier(pAttacker->getUnitCombatType());

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_MOD_VS_TYPE", iModifier, GC.getUnitCombatInfo(pAttacker->getUnitCombatType()).getTextKeyWide()));
			}
		}

		iModifier = pDefender->domainModifier(pAttacker->getDomainType());

		if (iModifier != 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_MOD_VS_TYPE", iModifier, GC.getDomainInfo(pAttacker->getDomainType()).getTextKeyWide()));
		}

		if (!(pDefender->noDefensiveBonus())) {
			iModifier = pPlot->defenseModifier(pDefender->getTeam(), (pAttacker != NULL) ? pAttacker->ignoreBuildingDefense() : true);

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_TILE_MOD", iModifier));
			}
		}

		iModifier = pDefender->fortifyModifier();

		if (iModifier != 0) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_FORTIFY_MOD", iModifier));
		}

		if (pPlot->isCity(true, pDefender->getTeam())) {
			iModifier = pDefender->cityDefenseModifier();

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_CITY_MOD", iModifier));
			}
		}

		if (pPlot->isHills()) {
			iModifier = pDefender->hillsDefenseModifier();

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_HILLS_MOD", iModifier));
			}
		}

		if (pPlot->getFeatureType() != NO_FEATURE) {
			iModifier = pDefender->featureDefenseModifier(pPlot->getFeatureType());

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_UNIT_MOD", iModifier, GC.getFeatureInfo(pPlot->getFeatureType()).getTextKeyWide()));
			}
		} else {
			iModifier = pDefender->terrainDefenseModifier(pPlot->getTerrainType());

			if (iModifier != 0) {
				szString.append(NEWLINE);
				szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_UNIT_MOD", iModifier, GC.getTerrainInfo(pPlot->getTerrainType()).getTextKeyWide()));
			}
		}

		if (!(pAttacker->immuneToFirstStrikes())) {
			if (pDefender->maxFirstStrikes() > 0) {
				if (pDefender->firstStrikes() == pDefender->maxFirstStrikes()) {
					if (pDefender->firstStrikes() == 1) {
						szString.append(NEWLINE);
						szString.append(gDLL->getText("TXT_KEY_UNIT_ONE_FIRST_STRIKE"));
					} else {
						szString.append(NEWLINE);
						szString.append(gDLL->getText("TXT_KEY_UNIT_NUM_FIRST_STRIKES", pDefender->firstStrikes()));
					}
				} else {
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_UNIT_FIRST_STRIKE_CHANCES", pDefender->firstStrikes(), pDefender->maxFirstStrikes()));
				}
			}
		}

		if (pDefender->isHurt()) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_COMBAT_PLOT_HP", pDefender->currHitPoints(), pDefender->maxHitPoints()));
		}
	}

	szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));

	if (GC.getGameINLINE().isDebugMode()) // BBAI: Only display this info in debug mode so game can be played with cheat code entered
	{
		szTempBuffer.Format(L"\nStack Compare Value = %d",
			gDLL->getInterfaceIFace()->getSelectionList()->AI_compareStacks(pPlot));
		szString.append(szTempBuffer);

		if (pPlot->getPlotCity() != NULL) {
			szTempBuffer.Format(L"\nBombard turns = %d",
				gDLL->getInterfaceIFace()->getSelectionList()->getBombardTurns(pPlot->getPlotCity()));
			szString.append(szTempBuffer);
		}

		const CvPlayerAI& kPlayer = GET_PLAYER(GC.getGameINLINE().getActivePlayer());
		int iOurStrengthDefense = kPlayer.AI_localDefenceStrength(pPlot, kPlayer.getTeam(), DOMAIN_LAND, 1, true, true, true);
		int iOurStrengthOffense = kPlayer.AI_localAttackStrength(pPlot, kPlayer.getTeam(), DOMAIN_LAND, 1, false, true, false);
		szTempBuffer.Format(L"\nPlot Strength(Ours)= d%d, o%d", iOurStrengthDefense, iOurStrengthOffense);
		szString.append(szTempBuffer);
		int iEnemyStrengthDefense = kPlayer.AI_localDefenceStrength(pPlot, NO_TEAM, DOMAIN_LAND, 1, true, true, true);
		int iEnemyStrengthOffense = kPlayer.AI_localAttackStrength(pPlot, NO_TEAM, DOMAIN_LAND, 1, false, true, false);
		szTempBuffer.Format(L"\nPlot Strength(Enemy)= d%d, o%d", iEnemyStrengthDefense, iEnemyStrengthOffense);
		szString.append(szTempBuffer);
	}

	szString.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));

	return true;
}

// DO NOT REMOVE - needed for font testing - Moose
void createTestFontString(CvWStringBuffer& szString) {
	szString.assign(L"!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[×]^_`abcdefghijklmnopqrstuvwxyz\n");
	szString.append(L"{}~\\ßÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖØÙÚÛÜÝÞŸßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ¿¡«»°ŠŒŽšœž™©®€£¢”‘“…’");
	for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI)
		szString.append(CvWString::format(L"%c", GC.getYieldInfo((YieldTypes)iI).getChar()));

	szString.append(L"\n");
	for (int iI = 0; iI < NUM_COMMERCE_TYPES; ++iI)
		szString.append(CvWString::format(L"%c", GC.getCommerceInfo((CommerceTypes)iI).getChar()));
	szString.append(L"\n");
	for (int iI = 0; iI < GC.getNumReligionInfos(); ++iI) {
		szString.append(CvWString::format(L"%c", GC.getReligionInfo((ReligionTypes)iI).getChar()));
		szString.append(CvWString::format(L"%c", GC.getReligionInfo((ReligionTypes)iI).getHolyCityChar()));
	}
	for (int iI = 0; iI < GC.getNumCorporationInfos(); ++iI) {
		szString.append(CvWString::format(L"%c", GC.getCorporationInfo((CorporationTypes)iI).getChar()));
		szString.append(CvWString::format(L"%c", GC.getCorporationInfo((CorporationTypes)iI).getHeadquarterChar()));
	}
	szString.append(L"\n");
	for (int iI = 0; iI < GC.getNumBonusInfos(); ++iI)
		szString.append(CvWString::format(L"%c", GC.getBonusInfo((BonusTypes)iI).getChar()));
	for (int iI = 0; iI < MAX_NUM_SYMBOLS; ++iI)
		szString.append(CvWString::format(L"%c", gDLL->getSymbolID(iI)));
}

void CvGameTextMgr::setPlotHelp(CvWStringBuffer& szString, CvPlot* pPlot) {
	PROFILE_FUNC();

	CvWString szTempBuffer;

	bool bShift = GC.shiftKey();
	bool bAlt = GC.altKey();
	bool bCtrl = GC.ctrlKey();

	if (bCtrl && (gDLL->getChtLvl() > 0)) {
		if (bShift && pPlot->headUnitNode() != NULL) {
			return;
		}

		if (pPlot->getOwnerINLINE() != NO_PLAYER) {
			int iPlotDanger = GET_PLAYER(pPlot->getOwnerINLINE()).AI_getPlotDanger(pPlot, 2);
			if (iPlotDanger > 0) {
				szString.append(CvWString::format(L"\nPlot Danger = %d", iPlotDanger));
			}
		}

		CvCity* pPlotCity = pPlot->getPlotCity();
		if (pPlotCity != NULL) {
			PlayerTypes ePlayer = pPlot->getOwnerINLINE();
			CvPlayerAI& kPlayer = GET_PLAYER(ePlayer);
			TeamTypes eTeam = kPlayer.getTeam();
			CvTeamAI& kTeam = GET_TEAM(eTeam);

			int iCityDefenders = pPlot->plotCount(PUF_canDefendGroupHead, -1, -1, ePlayer, NO_TEAM, PUF_isCityAIType);
			int iAttackGroups = pPlot->plotCount(PUF_isUnitAIType, UNITAI_ATTACK, -1, ePlayer);
			szString.append(CvWString::format(L"\nDefenders [D+A]/N ([%d + %d] / %d)", iCityDefenders, iAttackGroups, pPlotCity->AI_neededDefenders()));

			szString.append(CvWString::format(L"\nFloating Defenders H/N (%d / %d)", kPlayer.AI_getTotalFloatingDefenders(pPlotCity->area()), kPlayer.AI_getTotalFloatingDefendersNeeded(pPlotCity->area())));
			szString.append(CvWString::format(L"\nAir Defenders H/N (%d / %d)", pPlotCity->plot()->plotCount(PUF_canAirDefend, -1, -1, pPlotCity->getOwnerINLINE(), NO_TEAM, PUF_isDomainType, DOMAIN_AIR), pPlotCity->AI_neededAirDefenders()));
			szString.append(CvWString::format(L"\nThreat C/P (%d / %d)", pPlotCity->AI_cityThreat(), kPlayer.AI_getTotalAreaCityThreat(pPlotCity->area())));

			bool bFirst = true;
			for (PlayerTypes eLoopPlayer = (PlayerTypes)0; eLoopPlayer < MAX_PLAYERS; eLoopPlayer = (PlayerTypes)(eLoopPlayer + 1)) {
				CvPlayerAI& kLoopPlayer = GET_PLAYER(eLoopPlayer);
				if ((eLoopPlayer != ePlayer) && kLoopPlayer.isAlive()) {
					if (pPlotCity->isCapital()) {
						if (true) {
							int iCloseness = pPlotCity->AI_playerCloseness(eLoopPlayer, DEFAULT_PLAYER_CLOSENESS);
							int iPlayerCloseness = kPlayer.AI_playerCloseness(eLoopPlayer, DEFAULT_PLAYER_CLOSENESS);

							if (kTeam.isHasMet(kLoopPlayer.getTeam()) || iPlayerCloseness != 0) {
								if (bFirst) {
									bFirst = false;

									szString.append(CvWString::format(L"\n\nCloseness + War: (in %d wars)", kTeam.getAtWarCount(true)));
								}

								szString.append(CvWString::format(L"\n%s(%d) : %d ", kLoopPlayer.getName(), DEFAULT_PLAYER_CLOSENESS, iCloseness));
								szString.append(CvWString::format(L" [%d, ", iPlayerCloseness));
								if (eTeam != kLoopPlayer.getTeam()) {
									szString.append(CvWString::format(L"%d]", kTeam.AI_teamCloseness(kLoopPlayer.getTeam(), DEFAULT_PLAYER_CLOSENESS)));
									if (kTeam.isHasMet(kLoopPlayer.getTeam()) && kTeam.AI_getAttitude(kLoopPlayer.getTeam()) != ATTITUDE_FRIENDLY) {
										int iStartWarVal = kTeam.AI_startWarVal(kLoopPlayer.getTeam(), WARPLAN_TOTAL);

										if (kTeam.isAtWar(kLoopPlayer.getTeam())) {
											szString.append(CvWString::format(L"\n   At War:   "));
										} else if (kTeam.AI_getWarPlan(kLoopPlayer.getTeam()) != NO_WARPLAN) {
											szString.append(CvWString::format(L"\n   Plan. War:"));
										} else if (!kTeam.canDeclareWar(kLoopPlayer.getTeam())) {
											szString.append(CvWString::format(L"\n   Can't War:"));
										} else {
											szString.append(CvWString::format(L"\n   No War:   "));
										}

										if (iStartWarVal > 1200) {
											szString.append(CvWString::format(SETCOLR L" %d" ENDCOLR, TEXT_COLOR("COLOR_RED"), iStartWarVal));
										} else if (iStartWarVal > 600) {
											szString.append(CvWString::format(SETCOLR L" %d" ENDCOLR, TEXT_COLOR("COLOR_YELLOW"), iStartWarVal));
										} else {
											szString.append(CvWString::format(L" %d", iStartWarVal));
										}

										szString.append(CvWString::format(L" (%d", kTeam.AI_calculatePlotWarValue(kLoopPlayer.getTeam())));
										szString.append(CvWString::format(L", %d", kTeam.AI_calculateBonusWarValue(kLoopPlayer.getTeam())));
										szString.append(CvWString::format(L", %d", kTeam.AI_calculateCapitalProximity(kLoopPlayer.getTeam())));
										szString.append(CvWString::format(L", %4s", GC.getAttitudeInfo(kTeam.AI_getAttitude(kLoopPlayer.getTeam())).getDescription(0)));
										szString.append(CvWString::format(L", %d%%)", 100 - kTeam.AI_noWarAttitudeProb(kTeam.AI_getAttitude(kLoopPlayer.getTeam()))));
									}
								} else {
									szString.append(CvWString::format(L"-]"));
								}
							}
						}
					} else {
						int iCloseness = pPlotCity->AI_playerCloseness(eLoopPlayer, DEFAULT_PLAYER_CLOSENESS);

						if (iCloseness != 0) {
							if (bFirst) {
								bFirst = false;

								szString.append(CvWString::format(L"\n\nCloseness:"));
							}

							szString.append(CvWString::format(L"\n%s(%d) : %d ", kLoopPlayer.getName(), DEFAULT_PLAYER_CLOSENESS, iCloseness));
							szString.append(CvWString::format(L" [%d, ", kPlayer.AI_playerCloseness(eLoopPlayer, DEFAULT_PLAYER_CLOSENESS)));
							if (eTeam != kLoopPlayer.getTeam()) {
								szString.append(CvWString::format(L"%d]", kTeam.AI_teamCloseness(kLoopPlayer.getTeam(), DEFAULT_PLAYER_CLOSENESS)));
							} else {
								szString.append(CvWString::format(L"-]"));
							}
						}
					}
				}
			}

			int iWorkersHave = pPlotCity->AI_getWorkersHave();
			int iWorkersNeeded = pPlotCity->AI_getWorkersNeeded();
			szString.append(CvWString::format(L"\n\nWorkers H/N (%d , %d)", iWorkersHave, iWorkersNeeded));

			int iWorkBoatsNeeded = pPlotCity->AI_neededSeaWorkers();
			szString.append(CvWString::format(L"\n\nWorkboats Needed = %d", iWorkBoatsNeeded));

			int iAreaSiteBestValue = 0;
			int iNumAreaCitySites = kPlayer.AI_getNumAreaCitySites(pPlot->getArea(), iAreaSiteBestValue);
			int iOtherSiteBestValue = 0;
			int iNumOtherCitySites = (pPlot->waterArea() == NULL) ? 0 : kPlayer.AI_getNumAdjacentAreaCitySites(pPlot->waterArea()->getID(), pPlot->getArea(), iOtherSiteBestValue);

			szString.append(CvWString::format(L"\n\nArea Sites = %d (%d)", iNumAreaCitySites, iAreaSiteBestValue));
			szString.append(CvWString::format(L"\nOther Sites = %d (%d)", iNumOtherCitySites, iOtherSiteBestValue));
		} else if (pPlot->getOwner() != NO_PLAYER) {
			if (bShift && !bAlt) {
				const CvPlayerAI& kPlayer = GET_PLAYER(pPlot->getOwnerINLINE());
				int* paiBonusClassRevealed = new int[GC.getNumBonusClassInfos()];
				int* paiBonusClassUnrevealed = new int[GC.getNumBonusClassInfos()];
				int* paiBonusClassHave = new int[GC.getNumBonusClassInfos()];

				for (BonusClassTypes eBonusClass = (BonusClassTypes)0; eBonusClass < GC.getNumBonusClassInfos(); eBonusClass = (BonusClassTypes)(eBonusClass + 1)) {
					paiBonusClassRevealed[eBonusClass] = 0;
					paiBonusClassUnrevealed[eBonusClass] = 0;
					paiBonusClassHave[eBonusClass] = 0;
				}

				for (BonusTypes eBonus = (BonusTypes)0; eBonus < GC.getNumBonusInfos(); eBonus = (BonusTypes)(eBonus + 1)) {
					const CvBonusInfo& kBonus = GC.getBonusInfo(eBonus);
					TechTypes eRevealTech = (TechTypes)kBonus.getTechReveal();
					BonusClassTypes eBonusClass = (BonusClassTypes)kBonus.getBonusClassType();
					if (eRevealTech != NO_TECH) {
						if ((GET_TEAM(pPlot->getTeam()).isHasTech(eRevealTech))) {
							paiBonusClassRevealed[eBonusClass]++;
						} else {
							paiBonusClassUnrevealed[eBonusClass]++;
						}

						if (kPlayer.getNumAvailableBonuses(eBonus) > 0) {
							paiBonusClassHave[eBonusClass]++;
						} else if (kPlayer.countOwnedBonuses(eBonus) > 0) {
							paiBonusClassHave[eBonusClass]++;
						}
					}
				}

				for (TechTypes eTech = (TechTypes)0; eTech < GC.getNumTechInfos(); eTech = (TechTypes)(eTech + 1)) {
					int iPathLength = kPlayer.findPathLength(eTech, false);

					if (iPathLength <= 3 && !GET_TEAM(pPlot->getTeam()).isHasTech(eTech)) {
						bool bDummy;
						szString.append(CvWString::format(L"\n%s(%d)=%8d", GC.getTechInfo(eTech).getDescription(), iPathLength, kPlayer.AI_techValue(eTech, 1, false, true, paiBonusClassRevealed, paiBonusClassUnrevealed, paiBonusClassHave)));
						szString.append(CvWString::format(L" (bld:%d (%d old), ", kPlayer.AI_techBuildingValue(eTech, true, bDummy), kPlayer.AI_techBuildingValue_old(eTech, 1, bDummy)));
						int iObs = kPlayer.AI_obsoleteBuildingPenalty(eTech, true);
						if (iObs != 0)
							szString.append(CvWString::format(L"obs:%d, ", -iObs));
						szString.append(CvWString::format(L"unt:%d)", kPlayer.AI_techUnitValue(eTech, 1, bDummy)));
					}
				}
			} else if (bAlt && !bShift) {
				if (pPlot->isHasPathToEnemyCity(pPlot->getTeam())) {
					szString.append(CvWString::format(L"\nCan reach an enemy city\n\n"));
				} else {
					szString.append(CvWString::format(L"\nNo reachable enemy cities\n\n"));
				}
				for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
					const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
					if (kPlayer.isAlive()) {
						if (pPlot->isHasPathToPlayerCity(pPlot->getTeam(), ePlayer)) {
							szString.append(CvWString::format(SETCOLR L"Can reach %s city" ENDCOLR, TEXT_COLOR("COLOR_GREEN"), GET_PLAYER(ePlayer).getName()));
						} else {
							szString.append(CvWString::format(SETCOLR L"Cannot reach any %s city" ENDCOLR, TEXT_COLOR("COLOR_NEGATIVE_TEXT"), GET_PLAYER(ePlayer).getName()));
						}

						if (GET_TEAM(pPlot->getTeam()).isAtWar(kPlayer.getTeam())) {
							szString.append(CvWString::format(L" (enemy)"));
						}
						szString.append(CvWString::format(L"\n"));
					}

				}
			} else if (bShift && bAlt) {
				for (CivicTypes eCivic = (CivicTypes)0; eCivic < GC.getNumCivicInfos(); eCivic = (CivicTypes)(eCivic + 1)) {
					szString.append(CvWString::format(L"\n %s = %d", GC.getCivicInfo(eCivic).getDescription(), GET_PLAYER(pPlot->getOwner()).AI_civicValue(eCivic)));
				}
			} else if (pPlot->headUnitNode() == NULL) {
				std::vector<UnitAITypes> vecUnitAIs;

				if (pPlot->getFeatureType() != NO_FEATURE) {
					szString.append(CvWString::format(L"\nDefense unit AIs:"));
					vecUnitAIs.push_back(UNITAI_CITY_DEFENSE);
					vecUnitAIs.push_back(UNITAI_COUNTER);
					vecUnitAIs.push_back(UNITAI_CITY_COUNTER);
				} else {
					szString.append(CvWString::format(L"\nAttack unit AIs:"));
					vecUnitAIs.push_back(UNITAI_ATTACK);
					vecUnitAIs.push_back(UNITAI_ATTACK_CITY);
					vecUnitAIs.push_back(UNITAI_COUNTER);
				}

				CvCity* pCloseCity = GC.getMapINLINE().findCity(pPlot->getX_INLINE(), pPlot->getY_INLINE(), pPlot->getOwner(), NO_TEAM, true);

				if (pCloseCity != NULL) {
					for (uint iI = 0; iI < vecUnitAIs.size(); iI++) {
						CvWString szTempString;
						getUnitAIString(szTempString, vecUnitAIs[iI]);
						szString.append(CvWString::format(L"\n  %s  ", szTempString.GetCString()));
						for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
							UnitTypes eUnit = (UnitTypes)GC.getCivilizationInfo(GET_PLAYER(pPlot->getOwner()).getCivilizationType()).getCivilizationUnits(eUnitClass);
							if (eUnit != NO_UNIT && pCloseCity->canTrain(eUnit)) {
								int iValue = GET_PLAYER(pPlot->getOwner()).AI_unitValue(eUnit, vecUnitAIs[iI], pPlot->area());
								if (iValue > 0) {
									szString.append(CvWString::format(L"\n %s = %d", GC.getUnitInfo(eUnit).getDescription(), iValue));
								}
							}
						}
					}
				}
			}
		}
		return;
	} else if (bShift && !bAlt && (gDLL->getChtLvl() > 0)) {
		szString.append(GC.getTerrainInfo(pPlot->getTerrainType()).getDescription());

		FAssert((0 < GC.getNumBonusInfos()) && "GC.getNumBonusInfos() is not greater than zero but an array is being allocated in CvInterface::updateHelpStrings");
		for (BonusTypes eBonus = (BonusTypes)0; eBonus < GC.getNumBonusInfos(); eBonus = (BonusTypes)(eBonus + 1)) {
			if (pPlot->isPlotGroupConnectedBonus(GC.getGameINLINE().getActivePlayer(), eBonus)) {
				szString.append(NEWLINE);
				szString.append(GC.getBonusInfo(eBonus).getDescription());
				szString.append(CvWString::format(L" (%d)", GET_PLAYER(GC.getGameINLINE().getActivePlayer()).AI_bonusVal(eBonus, 0, true)));
			}
		}

		if (pPlot->getPlotGroup(GC.getGameINLINE().getActivePlayer()) != NULL) {
			szTempBuffer.Format(L"\n(%d, %d) group: %d", pPlot->getX_INLINE(), pPlot->getY_INLINE(), pPlot->getPlotGroup(GC.getGameINLINE().getActivePlayer())->getID());
		} else {
			szTempBuffer.Format(L"\n(%d, %d) group: (-1, -1)", pPlot->getX_INLINE(), pPlot->getY_INLINE());
		}
		szString.append(szTempBuffer);

		szTempBuffer.Format(L"\nArea: %d", pPlot->getArea());
		szString.append(szTempBuffer);
		szTempBuffer.Format(L"\nLatitude: %d", pPlot->getLatitude());
		szString.append(szTempBuffer);

		char tempChar = 'x';
		if (pPlot->getRiverNSDirection() == CARDINALDIRECTION_NORTH) {
			tempChar = 'N';
		} else if (pPlot->getRiverNSDirection() == CARDINALDIRECTION_SOUTH) {
			tempChar = 'S';
		}
		szTempBuffer.Format(L"\nNSRiverFlow: %c", tempChar);
		szString.append(szTempBuffer);

		tempChar = 'x';
		if (pPlot->getRiverWEDirection() == CARDINALDIRECTION_WEST) {
			tempChar = 'W';
		} else if (pPlot->getRiverWEDirection() == CARDINALDIRECTION_EAST) {
			tempChar = 'E';
		}
		szTempBuffer.Format(L"\nWERiverFlow: %c", tempChar);
		szString.append(szTempBuffer);

		if (pPlot->getRouteType() != NO_ROUTE) {
			szTempBuffer.Format(L"\nRoute: %s", GC.getRouteInfo(pPlot->getRouteType()).getDescription());
			szString.append(szTempBuffer);
		}

		if (pPlot->getRouteSymbol() != NULL) {
			szTempBuffer.Format(L"\nConnection: %i", gDLL->getRouteIFace()->getConnectionMask(pPlot->getRouteSymbol()));
			szString.append(szTempBuffer);
		}

		for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
			const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
			if (kPlayer.isAlive()) {
				if (pPlot->getCulture(ePlayer) > 0) {
					szTempBuffer.Format(L"\n%s Culture: %d", kPlayer.getName(), pPlot->getCulture(ePlayer));
					szString.append(szTempBuffer);
				}
			}
		}

		PlayerTypes eActivePlayer = GC.getGameINLINE().getActivePlayer();
		int iActualFoundValue = pPlot->getFoundValue(eActivePlayer);
		int iCalcFoundValue = GET_PLAYER(eActivePlayer).AI_foundValue(pPlot->getX_INLINE(), pPlot->getY_INLINE(), -1, false);
		int iStartingFoundValue = GET_PLAYER(eActivePlayer).AI_foundValue(pPlot->getX_INLINE(), pPlot->getY_INLINE(), -1, true);

		szTempBuffer.Format(L"\nFound Value: %d, (%d, %d)", iActualFoundValue, iCalcFoundValue, iStartingFoundValue);
		szString.append(szTempBuffer);

		CvCity* pWorkingCity = pPlot->getWorkingCity();
		if (NULL != pWorkingCity) {
			int iPlotIndex = pWorkingCity->getCityPlotIndex(pPlot);
			int iBuildValue = pWorkingCity->AI_getBestBuildValue(iPlotIndex);
			BuildTypes eBestBuild = pWorkingCity->AI_getBestBuild(iPlotIndex);

			szString.append(NEWLINE);

			int iFoodMultiplier, iProductionMultiplier, iCommerceMultiplier, iDesiredFoodChange;
			pWorkingCity->AI_getYieldMultipliers(iFoodMultiplier, iProductionMultiplier, iCommerceMultiplier, iDesiredFoodChange);

			szTempBuffer.Format(L"\n%s yield multipliers: ", pWorkingCity->getName().c_str());
			szString.append(szTempBuffer);
			szTempBuffer.Format(L"\n   Food %d, Prod %d, Com %d, DesFoodChange %d", iFoodMultiplier, iProductionMultiplier, iCommerceMultiplier, iDesiredFoodChange);
			szString.append(szTempBuffer);
			szTempBuffer.Format(L"\nTarget pop: %d, (%d good tiles)", pWorkingCity->AI_getTargetPopulation(), pWorkingCity->AI_countGoodPlots());
			szString.append(szTempBuffer);

			ImprovementTypes eImprovement = pPlot->getImprovementType();

			if (NO_BUILD != eBestBuild) {
				const CvBuildInfo& kBuild = GC.getBuildInfo(eBestBuild);
				if (kBuild.getImprovement() != NO_IMPROVEMENT && eImprovement != NO_IMPROVEMENT && eImprovement != kBuild.getImprovement()) {
					szTempBuffer.Format(SETCOLR L"\nBest Build: %s (%d) replacing %s" ENDCOLR, TEXT_COLOR("COLOR_RED"), kBuild.getDescription(), iBuildValue, GC.getImprovementInfo(eImprovement).getDescription());
				} else {
					szTempBuffer.Format(SETCOLR L"\nBest Build: %s (%d)" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), kBuild.getDescription(), iBuildValue);
				}

				szString.append(szTempBuffer);
			}

			szTempBuffer.Format(L"\nActual Build Values: ");
			szString.append(szTempBuffer);

			for (ImprovementTypes eLoopImprovement = (ImprovementTypes)0; eLoopImprovement < GC.getNumImprovementInfos(); eLoopImprovement = (ImprovementTypes)(eLoopImprovement + 1)) {
				if (pPlot->canHaveImprovement(eLoopImprovement, pWorkingCity->getTeam())) {
					int iOtherBuildValue = pWorkingCity->AI_getImprovementValue(pPlot, eLoopImprovement, iFoodMultiplier, iProductionMultiplier, iCommerceMultiplier, iDesiredFoodChange);
					int iOldValue = pWorkingCity->AI_getImprovementValue(pPlot, eLoopImprovement, iFoodMultiplier, iProductionMultiplier, iCommerceMultiplier, iDesiredFoodChange);

					szTempBuffer.Format(L"\n   %s : %d  (old %d)", GC.getImprovementInfo(eLoopImprovement).getDescription(), iOtherBuildValue, iOldValue);
					szString.append(szTempBuffer);
				}
			}

			szTempBuffer.Format(L"\nStandard Build Values: ");
			szString.append(szTempBuffer);

			for (ImprovementTypes eLoopImprovement = (ImprovementTypes)0; eLoopImprovement < GC.getNumImprovementInfos(); eLoopImprovement = (ImprovementTypes)(eLoopImprovement + 1)) {
				int iOtherBuildValue = pWorkingCity->AI_getImprovementValue(pPlot, eLoopImprovement, 100, 100, 100, 0);
				if (iOtherBuildValue > 0) {
					szTempBuffer.Format(L"\n   %s : %d", GC.getImprovementInfo(eLoopImprovement).getDescription(), iOtherBuildValue);
					szString.append(szTempBuffer);
				}
			}

			szString.append(NEWLINE);

		}

		if (pPlot->getPlotCity() != NULL) {
			PlayerTypes ePlayer = pPlot->getOwnerINLINE();
			CvPlayerAI& kPlayer = GET_PLAYER(ePlayer);

			szString.append(CvWString::format(L"\n\nAI unit class weights ..."));
			for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
				if (kPlayer.AI_getUnitClassWeight(eUnitClass) != 0) {
					szString.append(CvWString::format(L"\n%s = %d", GC.getUnitClassInfo(eUnitClass).getDescription(), kPlayer.AI_getUnitClassWeight(eUnitClass)));
				}
			}
			szString.append(CvWString::format(L"\n\nalso unit combat type weights..."));
			for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
				if (kPlayer.AI_getUnitCombatWeight(eUnitCombat) != 0) {
					szString.append(CvWString::format(L"\n%s = % d", GC.getUnitCombatInfo(eUnitCombat).getDescription(), kPlayer.AI_getUnitCombatWeight(eUnitCombat)));
				}
			}
		}
	} else if (!bShift && bAlt && (gDLL->getChtLvl() > 0)) {
		if (pPlot->isOwned()) {
			const CvPlayerAI& kPlotOwner = GET_PLAYER(pPlot->getOwnerINLINE());
			szTempBuffer.Format(L"\nThis player has %d area cities", pPlot->area()->getCitiesPerPlayer(pPlot->getOwnerINLINE()));
			szString.append(szTempBuffer);
			for (ReligionTypes eReligion = (ReligionTypes)0; eReligion < GC.getNumReligionInfos(); eReligion = (ReligionTypes)(eReligion + 1)) {
				int iNeededMissionaries = kPlotOwner.AI_neededMissionaries(pPlot->area(), (eReligion));
				if (iNeededMissionaries > 0) {
					szTempBuffer.Format(L"\nNeeded %c missionaries = %d", GC.getReligionInfo(eReligion).getChar(), iNeededMissionaries);
					szString.append(szTempBuffer);
				}
			}

			int iOurDefense = kPlotOwner.AI_localDefenceStrength(pPlot, pPlot->getTeam(), DOMAIN_LAND, 0, true, false, true);
			int iEnemyOffense = kPlotOwner.AI_localAttackStrength(pPlot, NO_TEAM);
			if (iEnemyOffense > 0) {
				szString.append(CvWString::format(SETCOLR L"\nDanger: %.2f (%d/%d)" ENDCOLR, TEXT_COLOR("COLOR_NEGATIVE_TEXT"),
					(iEnemyOffense * 1.0f) / std::max(1, iOurDefense), iEnemyOffense, iOurDefense));
			}

			CvCity* pCity = pPlot->getPlotCity();
			if (pCity != NULL) {

				szTempBuffer.Format(L"\nWater World Percent = %d", pCity->AI_calculateWaterWorldPercent());
				szString.append(szTempBuffer);

				const CvPlayerAI& kPlayer = GET_PLAYER(pCity->getOwnerINLINE());
				int iBuildUnitProb = static_cast<CvCityAI*>(pCity)->AI_buildUnitProb();
				szString.append(CvWString::format(L"\nUnit Cost: %d (max: %d)", kPlayer.AI_unitCostPerMil(), kPlayer.AI_maxUnitCostPerMil(pCity->area(), iBuildUnitProb)));

				szString.append(CvWString::format(L"\nUpgrade all units: %d gold", kPlayer.AI_getGoldToUpgradeAllUnits()));
				{
					int iValue = 0;
					for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
						iValue += kPlayer.getCommerceRate(eCommerce) * kPlayer.AI_commerceWeight(eCommerce) / 100;
					}
					int iLoop;
					for (CvCity* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop)) {
						iValue += 2 * pLoopCity->getYieldRate(YIELD_PRODUCTION);
					}
					iValue /= kPlayer.getTotalPopulation();
					szString.append(CvWString::format(L"\nAverage citizen value: %d", iValue));

					//
					szString.append(CvWString::format(L"\nBuild unit prob: %d%%", iBuildUnitProb));
					BuildingTypes eBestBuilding = static_cast<CvCityAI*>(pCity)->AI_bestBuildingThreshold(0, 0, 0, true);
					int iBestBuildingValue = (eBestBuilding == NO_BUILDING) ? 0 : pCity->AI_buildingValue(eBestBuilding, 0, 0, true);

					// Note. cf. adjustments made in AI_chooseProduction
					if (GC.getNumEraInfos() > 1) {
						iBestBuildingValue *= 2 * (GC.getNumEraInfos() - 1) - kPlayer.getCurrentEra();
						iBestBuildingValue /= GC.getNumEraInfos() - 1;
					}
					{
						int iTargetCities = GC.getWorldInfo(GC.getMapINLINE().getWorldSize()).getTargetNumCities();
						int iDummy;
						if (kPlayer.AI_getNumAreaCitySites(pPlot->getArea(), iDummy) > 0 && kPlayer.getNumCities() < iTargetCities) {
							iBestBuildingValue *= kPlayer.getNumCities() + iTargetCities;
							iBestBuildingValue /= 2 * iTargetCities;
						}
					}
					//

					iBuildUnitProb *= (250 + iBestBuildingValue);
					iBuildUnitProb /= (100 + 3 * iBestBuildingValue);
					szString.append(CvWString::format(L" (%d%%)", iBuildUnitProb));
				}

				szString.append(CvWString::format(L"\n\nRanks:"));
				szString.append(CvWString::format(L"\nPopulation:%d", pCity->findPopulationRank()));

				szString.append(CvWString::format(L"\nFood:%d(%d), ", pCity->findYieldRateRank(YIELD_FOOD), pCity->findBaseYieldRateRank(YIELD_FOOD)));
				szString.append(CvWString::format(L"Prod:%d(%d), ", pCity->findYieldRateRank(YIELD_PRODUCTION), pCity->findBaseYieldRateRank(YIELD_PRODUCTION)));
				szString.append(CvWString::format(L"Commerce:%d(%d)", pCity->findYieldRateRank(YIELD_COMMERCE), pCity->findBaseYieldRateRank(YIELD_COMMERCE)));

				szString.append(CvWString::format(L"\nGold:%d, ", pCity->findCommerceRateRank(COMMERCE_GOLD)));
				szString.append(CvWString::format(L"Research:%d, ", pCity->findCommerceRateRank(COMMERCE_RESEARCH)));
				szString.append(CvWString::format(L"Culture:%d", pCity->findCommerceRateRank(COMMERCE_CULTURE)));
			}
			szString.append(NEWLINE);

			//AI strategies
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_DAGGER)) {
				szTempBuffer.Format(L"Dagger, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_CRUSH)) {
				szTempBuffer.Format(L"Crush, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_ALERT1)) {
				szTempBuffer.Format(L"Alert1, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_ALERT2)) {
				szTempBuffer.Format(L"Alert2, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_TURTLE)) {
				szTempBuffer.Format(L"Turtle, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_LAST_STAND)) {
				szTempBuffer.Format(L"LastStand, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_FINAL_WAR)) {
				szTempBuffer.Format(L"FinalWar, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_GET_BETTER_UNITS)) {
				szTempBuffer.Format(L"GetBetterUnits, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_FASTMOVERS)) {
				szTempBuffer.Format(L"FastMovers, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_LAND_BLITZ)) {
				szTempBuffer.Format(L"LandBlitz, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_AIR_BLITZ)) {
				szTempBuffer.Format(L"AirBlitz, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_OWABWNW)) {
				szTempBuffer.Format(L"OWABWNW, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_PRODUCTION)) {
				szTempBuffer.Format(L"Production, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_MISSIONARY)) {
				szTempBuffer.Format(L"Missionary, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_BIG_ESPIONAGE)) {
				szTempBuffer.Format(L"BigEspionage, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_ECONOMY_FOCUS)) // K-Mod
			{
				szTempBuffer.Format(L"EconomyFocus, ");
				szString.append(szTempBuffer);
			}
			if (kPlotOwner.AI_isDoStrategy(AI_STRATEGY_ESPIONAGE_ECONOMY)) // K-Mod
			{
				szTempBuffer.Format(L"EspionageEconomy, ");
				szString.append(szTempBuffer);
			}

			//Area battle plans.
			if (pPlot->area()->getAreaAIType(pPlot->getTeam()) == AREAAI_OFFENSIVE) {
				szTempBuffer.Format(L"\n Area AI = OFFENSIVE");
			} else if (pPlot->area()->getAreaAIType(pPlot->getTeam()) == AREAAI_DEFENSIVE) {
				szTempBuffer.Format(L"\n Area AI = DEFENSIVE");
			} else if (pPlot->area()->getAreaAIType(pPlot->getTeam()) == AREAAI_MASSING) {
				szTempBuffer.Format(L"\n Area AI = MASSING");
			} else if (pPlot->area()->getAreaAIType(pPlot->getTeam()) == AREAAI_ASSAULT) {
				szTempBuffer.Format(L"\n Area AI = ASSAULT");
			} else if (pPlot->area()->getAreaAIType(pPlot->getTeam()) == AREAAI_ASSAULT_MASSING) {
				szTempBuffer.Format(L"\n Area AI = ASSAULT_MASSING");
			} else if (pPlot->area()->getAreaAIType(pPlot->getTeam()) == AREAAI_NEUTRAL) {
				szTempBuffer.Format(L"\n Area AI = NEUTRAL");
			}

			const CvTeamAI& kPlotTeam = GET_TEAM(pPlot->getTeam());
			szString.append(szTempBuffer);
			szString.append(CvWString::format(L"\n\nNum Wars: %d + %d minor", GET_TEAM(pPlot->getTeam()).getAtWarCount(true), GET_TEAM(pPlot->getTeam()).getAtWarCount(false) - GET_TEAM(pPlot->getTeam()).getAtWarCount(true)));
			szString.append(CvWString::format(L"\nWarplans:"));
			for (TeamTypes eTeam = (TeamTypes)0; eTeam < MAX_TEAMS; eTeam = (TeamTypes)(eTeam + 1)) {
				const CvTeam& kLoopTeam = GET_TEAM(eTeam);

				if (kLoopTeam.isAlive() || kLoopTeam.isBarbarian()) {
					if (kPlotTeam.AI_getWarPlan(eTeam) == WARPLAN_ATTACKED) {
						szString.append(CvWString::format(L"\n%s: ATTACKED", kLoopTeam.getName().c_str()));
					} else if (kPlotTeam.AI_getWarPlan(eTeam) == WARPLAN_ATTACKED_RECENT) {
						szString.append(CvWString::format(L"\n%s: ATTACKED_RECENT", kLoopTeam.getName().c_str()));
					} else if (kPlotTeam.AI_getWarPlan(eTeam) == WARPLAN_PREPARING_LIMITED) {
						szString.append(CvWString::format(L"\n%s: PREP_LIM", kLoopTeam.getName().c_str()));
					} else if (kPlotTeam.AI_getWarPlan(eTeam) == WARPLAN_PREPARING_TOTAL) {
						szString.append(CvWString::format(L"\n%s: PREP_TOTAL", kLoopTeam.getName().c_str()));
					} else if (kPlotTeam.AI_getWarPlan(eTeam) == WARPLAN_LIMITED) {
						szString.append(CvWString::format(L"\n%s: LIMITED", kLoopTeam.getName().c_str()));
					} else if (kPlotTeam.AI_getWarPlan(eTeam) == WARPLAN_TOTAL) {
						szString.append(CvWString::format(L"\n%s: TOTAL", kLoopTeam.getName().c_str()));
					} else if (kPlotTeam.AI_getWarPlan(eTeam) == WARPLAN_DOGPILE) {
						szString.append(CvWString::format(L"\n%s: DOGPILE", kLoopTeam.getName().c_str()));
					} else if (kPlotTeam.AI_getWarPlan(eTeam) == NO_WARPLAN) {
						if (kPlotTeam.isAtWar(eTeam)) {
							szString.append(CvWString::format(SETCOLR L"\n%s: NO_WARPLAN!" ENDCOLR, TEXT_COLOR("COLOR_WARNING_TEXT"), kLoopTeam.getName().c_str()));
						}
					}
				}

				if (kPlotTeam.isMinorCiv() || kPlotTeam.isBarbarian()) {
					if (pPlot->getTeam() != eTeam && !kPlotTeam.isAtWar(eTeam)) {
						szString.append(CvWString::format(SETCOLR L"\n%s: minor/barb not at war!" ENDCOLR, TEXT_COLOR("COLOR_WARNING_TEXT"), GET_TEAM(eTeam).getName().c_str()));
					}
				}
			}

			CvCity* pTargetCity = pPlot->area()->getTargetCity(pPlot->getOwner());
			if (pTargetCity) {
				szString.append(CvWString::format(L"\nTarget City: %s", pTargetCity->getName().c_str()));
			} else {
				szString.append(CvWString::format(L"\nTarget City: None"));
			}
		}

		bool bFirst = true;
		for (int iI = 0; iI < MAX_PLAYERS; ++iI) {
			PlayerTypes ePlayer = (PlayerTypes)iI;
			CvPlayerAI& kPlayer = GET_PLAYER(ePlayer);

			if (kPlayer.isAlive()) {
				int iActualFoundValue = pPlot->getFoundValue(ePlayer);
				int iCalcFoundValue = kPlayer.AI_foundValue(pPlot->getX_INLINE(), pPlot->getY_INLINE(), -1, false);
				int iStartingFoundValue = kPlayer.AI_foundValue(pPlot->getX_INLINE(), pPlot->getY_INLINE(), -1, true);
				int iBestAreaFoundValue = pPlot->area()->getBestFoundValue(ePlayer);
				int iCitySiteBestValue;
				int iNumAreaCitySites = kPlayer.AI_getNumAreaCitySites(pPlot->getArea(), iCitySiteBestValue);

				if ((iActualFoundValue > 0 || iCalcFoundValue > 0 || iStartingFoundValue > 0) || ((pPlot->getOwner() == iI) && (iBestAreaFoundValue > 0))) {
					if (bFirst) {
						szString.append(CvWString::format(SETCOLR L"\nFound Values:" ENDCOLR, TEXT_COLOR("COLOR_UNIT_TEXT")));
						bFirst = false;
					}

					szString.append(NEWLINE);

					bool bIsRevealed = pPlot->isRevealed(kPlayer.getTeam(), false);

					szString.append(CvWString::format(SETCOLR, TEXT_COLOR(bIsRevealed ? (((iActualFoundValue > 0) && (iActualFoundValue == iBestAreaFoundValue)) ? "COLOR_UNIT_TEXT" : "COLOR_ALT_HIGHLIGHT_TEXT") : "COLOR_HIGHLIGHT_TEXT")));

					if (!bIsRevealed) {
						szString.append(CvWString::format(L"("));
					}

					szString.append(CvWString::format(L"%s: %d", kPlayer.getName(), iActualFoundValue));

					if (!bIsRevealed) {
						szString.append(CvWString::format(L")"));
					}

					szString.append(CvWString::format(ENDCOLR));

					if (iCalcFoundValue > 0 || iStartingFoundValue > 0) {
						szTempBuffer.Format(L" (%d,%ds)", iCalcFoundValue, iStartingFoundValue);
						szString.append(szTempBuffer);
					}

					int iDeadlockCount = kPlayer.AI_countDeadlockedBonuses(pPlot);
					if (iDeadlockCount > 0) {
						szTempBuffer.Format(L", " SETCOLR L"d=%d" ENDCOLR, TEXT_COLOR("COLOR_NEGATIVE_TEXT"), iDeadlockCount);
						szString.append(szTempBuffer);
					}

					if (kPlayer.AI_isPlotCitySite(pPlot)) {
						szTempBuffer.Format(L", " SETCOLR L"X" ENDCOLR, TEXT_COLOR("COLOR_UNIT_TEXT"));
						szString.append(szTempBuffer);
					}

					if ((iBestAreaFoundValue > 0) || (iNumAreaCitySites > 0)) {
						int iBestFoundValue = kPlayer.findBestFoundValue();

						szTempBuffer.Format(L"\n  Area Best = %d, Best = %d, Sites = %d", iBestAreaFoundValue, iBestFoundValue, iNumAreaCitySites);
						szString.append(szTempBuffer);
					}
				}
			}
		}
	} else if (bShift && bAlt && (gDLL->getChtLvl() > 0)) {
		CvCity* pCity = pPlot->getWorkingCity();
		if (pCity != NULL) {
			// some functions we want to call are not in CvCity, worse some are protected, so made us a friend
			CvCityAI* pCityAI = static_cast<CvCityAI*>(pCity);

			int iGrowthValue = pCityAI->AI_growthValuePerFood();

			// if we over the city, then do an array of all the plots
			if (pPlot->getPlotCity() != NULL) {
				// if control key is down, ignore food
				bool bIgnoreFood = GC.ctrlKey();

				// line one is: blank, 20, 9, 10, blank
				setCityPlotYieldValueString(szString, pCity, -1, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 20, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 9, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 10, bIgnoreFood, iGrowthValue);
				szString.append(L"\n");

				// line two is: 19, 8, 1, 2, 11
				setCityPlotYieldValueString(szString, pCity, 19, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 8, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 1, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 2, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 11, bIgnoreFood, iGrowthValue);
				szString.append(L"\n");

				// line three is: 18, 7, 0, 3, 12
				setCityPlotYieldValueString(szString, pCity, 18, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 7, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 0, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 3, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 12, bIgnoreFood, iGrowthValue);
				szString.append(L"\n");

				// line four is: 17, 6, 5, 4, 13
				setCityPlotYieldValueString(szString, pCity, 17, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 6, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 5, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 4, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 13, bIgnoreFood, iGrowthValue);
				szString.append(L"\n");

				// line five is: blank, 16, 15, 14, blank
				setCityPlotYieldValueString(szString, pCity, -1, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 16, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 15, bIgnoreFood, iGrowthValue);
				setCityPlotYieldValueString(szString, pCity, 14, bIgnoreFood, iGrowthValue);

				// show specialist values too
				for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
					int iMaxThisSpecialist = pCity->getMaxSpecialistCount(eSpecialist);
					int iSpecialistCount = pCity->getSpecialistCount(eSpecialist);
					bool bUsingSpecialist = (iSpecialistCount > 0);
					bool bIsDefaultSpecialist = (eSpecialist == GC.getDefineINT("DEFAULT_SPECIALIST"));

					// can this city have any of this specialist?
					if (iMaxThisSpecialist > 0 || bIsDefaultSpecialist) {
						// start color
						if (pCity->getForceSpecialistCount(eSpecialist) > 0)
							szString.append(CvWString::format(L"\n" SETCOLR, TEXT_COLOR("COLOR_NEGATIVE_TEXT")));
						else if (bUsingSpecialist)
							szString.append(CvWString::format(L"\n" SETCOLR, TEXT_COLOR("COLOR_ALT_HIGHLIGHT_TEXT")));
						else
							szString.append(CvWString::format(L"\n" SETCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT")));

						// add name
						szString.append(GC.getSpecialistInfo(eSpecialist).getDescription());

						// end color
						szString.append(CvWString::format(ENDCOLR));

						// add usage
						szString.append(CvWString::format(L": (%d/%d) ", iSpecialistCount, iMaxThisSpecialist));

						// add value
						int iValue = pCityAI->AI_specialistValue(eSpecialist, bUsingSpecialist, false, iGrowthValue);
						setYieldValueString(szString, iValue, bUsingSpecialist);
					}
				}
				{
					for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1))
						szString.append(CvWString::format(L"%s%d%c", eYield == 0 ? L"" : L", ", GET_PLAYER(pCity->getOwnerINLINE()).AI_averageYieldMultiplier(eYield), GC.getYieldInfo(eYield).getChar()));
					szString.append(L")");

					szString.append(L"\nCity yield mults: (");
					for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1))
						szString.append(CvWString::format(L"%s%d%c", eYield == 0 ? L"" : L", ", pCity->AI_yieldMultiplier(eYield), GC.getYieldInfo(eYield).getChar()));
					szString.append(L")");

					szString.append(L"\nCity spec mults:  (");
					for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1))
						szString.append(CvWString::format(L"%s%d%c", eYield == 0 ? L"" : L", ", pCityAI->AI_specialYieldMultiplier(eYield), GC.getYieldInfo(eYield).getChar()));
					szString.append(L")");

					szString.append(CvWString::format(L"\nCity weights: ("));
					for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1))
						szString.append(CvWString::format(L"%s%d%c", eCommerce == 0 ? L"" : L", ", GET_PLAYER(pCity->getOwnerINLINE()).AI_commerceWeight(eCommerce, pCity), GC.getCommerceInfo(eCommerce).getChar()));
					szString.append(L")");

					szString.append(CvWString::format(L"\nExchange"));
					for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
						int iCommerce = GET_PLAYER(pCity->getOwnerINLINE()).AI_averageCommerceExchange(eCommerce);
						szTempBuffer.Format(L", %d%c", iCommerce, GC.getCommerceInfo(eCommerce).getChar());
						szString.append(szTempBuffer);
					}

					szString.append(CvWString::format(L"\nAvg mults"));
					for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
						int iCommerce = GET_PLAYER(pCity->getOwnerINLINE()).AI_averageCommerceMultiplier(eCommerce);
						szTempBuffer.Format(L", %d%c", iCommerce, GC.getCommerceInfo(eCommerce).getChar());
						szString.append(szTempBuffer);
					}
					szString.append(CvWString::format(L"\nAvg %c pressure: %d", GC.getCommerceInfo(COMMERCE_CULTURE).getChar(), GET_PLAYER(pCity->getOwnerINLINE()).AI_averageCulturePressure()));

					if (GET_PLAYER(pCity->getOwnerINLINE()).AI_isFinancialTrouble()) {
						szTempBuffer.Format(L"$$$!!!");
						szString.append(szTempBuffer);
					}
				}
			} else {
				bool bWorkingPlot = pCity->isWorkingPlot(pPlot);

				if (bWorkingPlot)
					szTempBuffer.Format(SETCOLR L"%s is working" ENDCOLR, TEXT_COLOR("COLOR_ALT_HIGHLIGHT_TEXT"), pCity->getName().GetCString());
				else
					szTempBuffer.Format(SETCOLR L"%s not working" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), pCity->getName().GetCString());
				szString.append(szTempBuffer);

				int iValue = pCityAI->AI_plotValue(pPlot, bWorkingPlot, false, false, iGrowthValue);
				int iRawValue = pCityAI->AI_plotValue(pPlot, bWorkingPlot, true, false, iGrowthValue);
				int iMagicValue = pCityAI->AI_getPlotMagicValue(pPlot, pCityAI->healthRate() == 0);

				szTempBuffer.Format(L"\nvalue = %d\nraw value = %d\nmagic value = %d", iValue, iRawValue, iMagicValue);
				szString.append(szTempBuffer);
			}
		}

		// calc some bonus info
		BonusTypes eBonus;
		if (GC.getGameINLINE().isDebugMode()) {
			eBonus = pPlot->getBonusType();
		} else {
			eBonus = pPlot->getBonusType(GC.getGameINLINE().getActiveTeam());
		}
		if (eBonus != NO_BONUS) {
			szString.append(CvWString::format(L"\n%s values:", GC.getBonusInfo(eBonus).getDescription()));

			for (PlayerTypes eLoopPlayer = (PlayerTypes)0; eLoopPlayer < MAX_PLAYERS; eLoopPlayer = (PlayerTypes)(eLoopPlayer + 1)) {
				CvPlayerAI& kLoopPlayer = GET_PLAYER(eLoopPlayer);
				if (kLoopPlayer.isAlive()) {
					BonusTypes eNonObsBonus = pPlot->getNonObsoleteBonusType(kLoopPlayer.getTeam());
					if (eNonObsBonus != NO_BONUS) {
						szString.append(CvWString::format(L"\n %s: %d", kLoopPlayer.getName(), kLoopPlayer.AI_bonusVal(eNonObsBonus, 0, true)));
					} else {
						szString.append(CvWString::format(L"\n %s: unknown (%d)", kLoopPlayer.getName(), kLoopPlayer.AI_bonusVal(eBonus, 0, true)));
					}
				}
			}
		}
	} else {
		PlayerTypes eRevealOwner = pPlot->getRevealedOwner(GC.getGameINLINE().getActiveTeam(), true);

		if (eRevealOwner != NO_PLAYER) {
			if (pPlot->isActiveVisible(true)) {
				szTempBuffer.Format(L"%d%% " SETCOLR L"%s" ENDCOLR, pPlot->calculateCulturePercent(eRevealOwner), GET_PLAYER(eRevealOwner).getPlayerTextColorR(), GET_PLAYER(eRevealOwner).getPlayerTextColorG(), GET_PLAYER(eRevealOwner).getPlayerTextColorB(), GET_PLAYER(eRevealOwner).getPlayerTextColorA(), GET_PLAYER(eRevealOwner).getCivilizationAdjective());
				if (getOptionBOOL("MiscHover__PlotWorkingCity", true)) {
					CvCity* pWorkingCity = pPlot->getWorkingCity();

					if (pWorkingCity != NULL && pWorkingCity->getOwnerINLINE() == GC.getGameINLINE().getActivePlayer()) {
						szTempBuffer.append(L", ");

						if (pWorkingCity->isWorkingPlot(pPlot)) {
							szTempBuffer.append(CvWString::format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), pWorkingCity->getName().GetCString()));
						} else {
							szTempBuffer.append(CvWString::format(L"%s", pWorkingCity->getName().GetCString()));
						}
					}
				}
				szString.append(szTempBuffer);
				szString.append(NEWLINE);

				for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
					if (ePlayer != eRevealOwner) {
						CvPlayer& kPlayer = GET_PLAYER(ePlayer);
						if (kPlayer.isAlive() && pPlot->getCulture(ePlayer) > 0) {
							int iCulturePercent = pPlot->calculateCulturePercent(ePlayer);
							if (iCulturePercent >= 1) {
								szTempBuffer.Format(L"%d%% " SETCOLR L"%s" ENDCOLR, iCulturePercent, kPlayer.getPlayerTextColorR(), kPlayer.getPlayerTextColorG(), kPlayer.getPlayerTextColorB(), kPlayer.getPlayerTextColorA(), kPlayer.getCivilizationAdjective());
								szString.append(szTempBuffer);
								szString.append(NEWLINE);
							}
						}
					}
				}

			} else {
				szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, GET_PLAYER(eRevealOwner).getPlayerTextColorR(), GET_PLAYER(eRevealOwner).getPlayerTextColorG(), GET_PLAYER(eRevealOwner).getPlayerTextColorB(), GET_PLAYER(eRevealOwner).getPlayerTextColorA(), GET_PLAYER(eRevealOwner).getCivilizationDescription());
				szString.append(szTempBuffer);
				szString.append(NEWLINE);
			}
		}

		int iDefenseModifier = pPlot->defenseModifier((eRevealOwner != NO_PLAYER ? GET_PLAYER(eRevealOwner).getTeam() : NO_TEAM), true, true);

		if (iDefenseModifier != 0) {
			szString.append(gDLL->getText("TXT_KEY_PLOT_BONUS", iDefenseModifier));
			szString.append(NEWLINE);
		}

		if (pPlot->getTerrainType() != NO_TERRAIN) {
			if (pPlot->isPeak()) {
				szString.append(gDLL->getText("TXT_KEY_PLOT_PEAK"));
			} else {
				if (pPlot->isWater()) {
					szTempBuffer.Format(SETCOLR, TEXT_COLOR("COLOR_WATER_TEXT"));
					szString.append(szTempBuffer);
				}

				if (pPlot->isHills()) {
					szString.append(gDLL->getText("TXT_KEY_PLOT_HILLS"));
				}

				if (pPlot->getFeatureType() != NO_FEATURE) {
					szTempBuffer.Format(L"%s/", GC.getFeatureInfo(pPlot->getFeatureType()).getDescription());
					szString.append(szTempBuffer);
				}

				szString.append(GC.getTerrainInfo(pPlot->getTerrainType()).getDescription());

				if (pPlot->isWater()) {
					szString.append(ENDCOLR);
				}
			}
		}

		if (pPlot->hasYield()) {
			for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1)) {
				int iYield = pPlot->calculateYield(eYield, true);

				if (iYield != 0) {
					szTempBuffer.Format(L", %d%c", iYield, GC.getYieldInfo(eYield).getChar());
					szString.append(szTempBuffer);
				}
			}
		}

		if (pPlot->isFreshWater()) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_PLOT_FRESH_WATER"));
		}

		if (pPlot->isLake()) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_PLOT_FRESH_WATER_LAKE"));
		}

		if (pPlot->isImpassable(GC.getGameINLINE().getActiveTeam())) {
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_PLOT_IMPASSABLE"));
		}

		BonusTypes eBonus;
		if (GC.getGameINLINE().isDebugMode()) {
			eBonus = pPlot->getBonusType();
		} else {
			eBonus = pPlot->getBonusType(GC.getGameINLINE().getActiveTeam());
		}

		if (eBonus != NO_BONUS) {
			szTempBuffer.Format(L"%c " SETCOLR L"%s" ENDCOLR, GC.getBonusInfo(eBonus).getChar(), TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getBonusInfo(eBonus).getDescription());
			szString.append(NEWLINE);
			szString.append(szTempBuffer);

			// K-Mod. I've rearranged and edited some of the code in this section to fix some bugs.
			// (for example, obsolete bonuses shouldn't say "requires x improvement"; and neither should bonuses that are already connected with a fort.)
			if (!GET_TEAM(GC.getGameINLINE().getActiveTeam()).isBonusObsolete(eBonus)) {
				if (GC.getBonusInfo(eBonus).getHealth() != 0) {
					szTempBuffer.Format(L", +%d%c", abs(GC.getBonusInfo(eBonus).getHealth()), ((GC.getBonusInfo(eBonus).getHealth() > 0) ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR)));
					szString.append(szTempBuffer);
				}

				if (GC.getBonusInfo(eBonus).getHappiness() != 0) {
					szTempBuffer.Format(L", +%d%c", abs(GC.getBonusInfo(eBonus).getHappiness()), ((GC.getBonusInfo(eBonus).getHappiness() > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR)));
					szString.append(szTempBuffer);
				}

				if (!(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isHasTech((TechTypes)GC.getBonusInfo(eBonus).getTechCityTrade()))) {
					szString.append(gDLL->getText("TXT_KEY_PLOT_RESEARCH", GC.getTechInfo((TechTypes)GC.getBonusInfo(eBonus).getTechCityTrade()).getTextKeyWide()));
				}

				ImprovementTypes ePlotImprovement = pPlot->getImprovementType();

				if (!pPlot->isCity()) {
					if (!GET_PLAYER(GC.getGameINLINE().getActivePlayer()).doesImprovementConnectBonus(pPlot->getImprovementType(), eBonus)) {
						// K-Mod note: unfortunately, the following code assumes that there is only one improvement marked "isImprovementBonusTrade" for each bonus.
						for (BuildTypes eLoopBuild = (BuildTypes)0; eLoopBuild < GC.getNumBuildInfos(); eLoopBuild = (BuildTypes)(eLoopBuild + 1)) {
							const CvBuildInfo& kLoopBuild = GC.getBuildInfo(eLoopBuild);
							if (kLoopBuild.getImprovement() != NO_IMPROVEMENT) {
								CvImprovementInfo& kImprovementInfo = GC.getImprovementInfo((ImprovementTypes)kLoopBuild.getImprovement());
								if (kImprovementInfo.isImprovementBonusTrade(eBonus)) {
									if (pPlot->canHaveImprovement((ImprovementTypes)kLoopBuild.getImprovement(), GC.getGameINLINE().getActiveTeam(), true)) {
										if (GET_TEAM(GC.getGameINLINE().getActiveTeam()).isHasTech((TechTypes)kLoopBuild.getTechPrereq())) {
											szString.append(gDLL->getText("TXT_KEY_PLOT_REQUIRES", kImprovementInfo.getTextKeyWide()));
										} else if (GC.getBonusInfo(eBonus).getTechCityTrade() != kLoopBuild.getTechPrereq()) {
											szString.append(gDLL->getText("TXT_KEY_PLOT_RESEARCH", GC.getTechInfo((TechTypes)kLoopBuild.getTechPrereq()).getTextKeyWide()));
										}

										bool bFirst = true;

										for (YieldTypes eLoopYield = (YieldTypes)0; eLoopYield < NUM_YIELD_TYPES; eLoopYield = (YieldTypes)(eLoopYield + 1)) {
											const CvYieldInfo& kLoopYield = GC.getYieldInfo(eLoopYield);
											int iYieldChange = kImprovementInfo.getImprovementBonusYield(eBonus, eLoopYield) + kImprovementInfo.getYieldChange(eLoopYield);
											if (iYieldChange != 0) {
												if (iYieldChange > 0) {
													szTempBuffer.Format(L"+%d%c", iYieldChange, kLoopYield.getChar());
												} else {
													szTempBuffer.Format(L"%d%c", iYieldChange, kLoopYield.getChar());
												}
												setListHelp(szString, L"\n", szTempBuffer, L", ", bFirst);
												bFirst = false;
											}
										}

										if (!bFirst) {
											szString.append(gDLL->getText("TXT_KEY_BONUS_WITH_IMPROVEMENT", kImprovementInfo.getTextKeyWide()));
										}

										break;
									}
								}
							}
						}
					}
					if (!(pPlot->isBonusNetwork(GC.getGameINLINE().getActiveTeam()))) {
						szString.append(gDLL->getText("TXT_KEY_PLOT_REQUIRES_ROUTE"));
					}
				}

				// K-Mod note: I'm not sure whether or not the help should be displayed when the bonus is obsolete.
				// The fact is, none of the bonuses have help text in K-Mod (or standard bts.)
				if (!CvWString(GC.getBonusInfo(eBonus).getHelp()).empty()) {
					szString.append(NEWLINE);
					szString.append(GC.getBonusInfo(eBonus).getHelp());
				}
			} // end !isBonusObsolete
		}

		ImprovementTypes eImprovement = pPlot->getRevealedImprovementType(GC.getGameINLINE().getActiveTeam(), true);

		if (eImprovement != NO_IMPROVEMENT) {
			szString.append(NEWLINE);
			szString.append(GC.getImprovementInfo(eImprovement).getDescription());

			bool bFound = false;

			for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1)) {
				if (GC.getImprovementInfo(eImprovement).getIrrigatedYieldChange(eYield) != 0) {
					bFound = true;
					break;
				}
			}

			if (bFound) {
				if (pPlot->isIrrigationAvailable()) {
					szString.append(gDLL->getText("TXT_KEY_PLOT_IRRIGATED"));
				} else {
					szString.append(gDLL->getText("TXT_KEY_PLOT_NOT_IRRIGATED"));
				}
			}

			if (GC.getImprovementInfo(eImprovement).getImprovementUpgrade() != NO_IMPROVEMENT) {
				if ((pPlot->getUpgradeProgress() > 0) || pPlot->isBeingWorked()) {
					int iTurns = pPlot->getUpgradeTimeLeft(eImprovement, eRevealOwner);
					szString.append(gDLL->getText("TXT_KEY_PLOT_IMP_UPGRADE", iTurns, GC.getImprovementInfo((ImprovementTypes)GC.getImprovementInfo(eImprovement).getImprovementUpgrade()).getTextKeyWide()));
				} else {
					szString.append(gDLL->getText("TXT_KEY_PLOT_WORK_TO_UPGRADE", GC.getImprovementInfo((ImprovementTypes)GC.getImprovementInfo(eImprovement).getImprovementUpgrade()).getTextKeyWide()));
				}
			}
		}

		if (pPlot->getRevealedRouteType(GC.getGameINLINE().getActiveTeam(), true) != NO_ROUTE) {
			szString.append(NEWLINE);
			szString.append(GC.getRouteInfo(pPlot->getRevealedRouteType(GC.getGameINLINE().getActiveTeam(), true)).getDescription());
		}

		if (GET_TEAM(GC.getGameINLINE().getActiveTeam()).isMapCentering() && getOptionBOOL("MiscHover__ShowCoords", true)) {
			szTempBuffer.Format(L"\nCoords: (%d, %d)", pPlot->getX_INLINE(), pPlot->getY_INLINE());
			szString.append(szTempBuffer);
		}

		BuildTypes eBestBuild = NO_BUILD;
		bool bBestPartiallyBuilt = false;

		if (getOptionBOOL("MiscHover__PlotRecommendedBuild", true)) {
			CvCity* pWorkingCity = pPlot->getWorkingCity();
			if (pWorkingCity != NULL && pWorkingCity->getOwnerINLINE() == GC.getGameINLINE().getActivePlayer()) {
				eBestBuild = pWorkingCity->AI_getBestBuild(pWorkingCity->getCityPlotIndex(pPlot));

				if (eBestBuild != NO_BUILD) {
					CvBuildInfo& kBestBuild = GC.getBuildInfo(eBestBuild);
					ImprovementTypes ePlotImprovement = pPlot->getImprovementType();

					if (ePlotImprovement != NO_IMPROVEMENT && ePlotImprovement == kBestBuild.getImprovement()) {
						eBestBuild = NO_BUILD;
					} else if (kBestBuild.getRoute() != NO_ROUTE && (pPlot->isWater() || kBestBuild.getRoute() == pPlot->getRouteType())) {
						eBestBuild = NO_BUILD;
					}
				}
			}
			if (eBestBuild != NO_BUILD) {
				szTempBuffer.Format(L"\nAI Recommends: %s", GC.getBuildInfo(eBestBuild).getDescription());
				szString.append(szTempBuffer);
			}
		}

		if (pPlot->hasAnyBuildProgress() && getOptionBOOL("MiscHover__PartialBuilds", true)) {
			PlayerTypes ePlayer = GC.getGameINLINE().getActivePlayer();

			for (BuildTypes eBuild = (BuildTypes)0; eBuild < GC.getNumBuildInfos(); eBuild = (BuildTypes)(eBuild + 1)) {
				if (pPlot->getBuildProgress(eBuild) > 0 && pPlot->canBuild(eBuild, ePlayer)) {
					int iTurns = pPlot->getBuildTurnsLeft(eBuild, GC.getGame().getActivePlayer());

					if (iTurns > 0 && iTurns < MAX_INT) {
						szString.append(NEWLINE);
						szString.append(GC.getBuildInfo(eBuild).getDescription());
						szString.append(L": ");
						szString.append(gDLL->getText("TXT_KEY_ACTION_NUM_TURNS", iTurns));
					}
				}
			}
		}

		if (GC.getGameINLINE().isDebugMode()) {
			bool bFirst = true;
			for (TeamTypes eTeam = (TeamTypes)0; eTeam < MAX_TEAMS; eTeam = (TeamTypes)(eTeam + 1)) {
				if (pPlot->getBlockadedCount(eTeam) > 0 && GET_TEAM(eTeam).isAlive()) {
					if (bFirst) {
						szString.append(CvWString::format(SETCOLR, TEXT_COLOR("COLOR_NEGATIVE_TEXT")));
						szString.append(NEWLINE);
						szString.append(gDLL->getText("TXT_KEY_PLOT_BLOCKADED"));
						szString.append(CvWString::format(ENDCOLR));

						szString.append(CvWString::format(L"Teams:"));
						bFirst = false;
					}
					szString.append(CvWString::format(L" %s,", GET_TEAM(eTeam).getName().c_str()));
				}
			}
		} else if (pPlot->getBlockadedCount(GC.getGameINLINE().getActiveTeam()) > 0) {
			szString.append(CvWString::format(SETCOLR, TEXT_COLOR("COLOR_NEGATIVE_TEXT")));
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_PLOT_BLOCKADED"));
			szString.append(CvWString::format(ENDCOLR));
		}

	}

	if (pPlot->getFeatureType() != NO_FEATURE) {
		int iDamage = GC.getFeatureInfo(pPlot->getFeatureType()).getTurnDamage();

		if (iDamage > 0) {
			szString.append(CvWString::format(SETCOLR, TEXT_COLOR("COLOR_NEGATIVE_TEXT")));
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_PLOT_DAMAGE", iDamage));
			szString.append(CvWString::format(ENDCOLR));
		} else if (iDamage < 0) {
			szString.append(CvWString::format(SETCOLR, TEXT_COLOR("COLOR_POSITIVE_TEXT")));
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_PLOT_DAMAGE", iDamage));
			szString.append(CvWString::format(ENDCOLR));
		}

	}
}


void CvGameTextMgr::setCityPlotYieldValueString(CvWStringBuffer& szString, CvCity* pCity, int iIndex, bool bIgnoreFood, int iGrowthValue) {
	PROFILE_FUNC();

	CvPlot* pPlot = NULL;

	if (iIndex >= 0 && iIndex < NUM_CITY_PLOTS)
		pPlot = pCity->getCityIndexPlot(iIndex);

	if (pPlot != NULL && pPlot->getWorkingCity() == pCity) {
		CvCityAI* pCityAI = static_cast<CvCityAI*>(pCity);
		bool bWorkingPlot = pCity->isWorkingPlot(iIndex);

		int iValue = pCityAI->AI_plotValue(pPlot, bWorkingPlot, bIgnoreFood, false, iGrowthValue);

		setYieldValueString(szString, iValue, /*bActive*/ bWorkingPlot);
	} else
		setYieldValueString(szString, 0, /*bActive*/ false, /*bMakeWhitespace*/ true);
}

void CvGameTextMgr::setYieldValueString(CvWStringBuffer& szString, int iValue, bool bActive, bool bMakeWhitespace) {
	PROFILE_FUNC();

	static bool bUseFloats = false;

	if (bActive)
		szString.append(CvWString::format(SETCOLR, TEXT_COLOR("COLOR_ALT_HIGHLIGHT_TEXT")));
	else
		szString.append(CvWString::format(SETCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT")));

	if (!bMakeWhitespace) {
		if (bUseFloats) {
			float fValue = ((float)iValue) / 10000;
			szString.append(CvWString::format(L"%2.3f " ENDCOLR, fValue));
		} else
			szString.append(CvWString::format(L"%05d  " ENDCOLR, iValue / 10));
	} else
		szString.append(CvWString::format(L"           " ENDCOLR));
}

void CvGameTextMgr::setCityBarHelp(CvWStringBuffer& szString, CvCity* pCity) {
	PROFILE_FUNC();

	int iFoodDifference = pCity->foodDifference();
	szString.append(pCity->getName());

	CvWString szTempBuffer;
	int iRate = 0;
	// Health icon
	if (getOptionBOOL("CityBar__Health", "False")) {
		iRate = pCity->goodHealth() - pCity->badHealth();
		if (iRate > 0) {
			szTempBuffer.Format(L", %d %c", iRate, gDLL->getSymbolID(HEALTHY_CHAR));
			szString.append(szTempBuffer);
		} else if (iRate < 0) {
			szTempBuffer.Format(L", %d %c", -iRate, gDLL->getSymbolID(UNHEALTHY_CHAR));
			szString.append(szTempBuffer);
		}
	}

	// Happiness icon
	if (getOptionBOOL("CityBar__Happiness", "False")) {
		if (pCity->isDisorder()) {
			int iAngryPop = pCity->angryPopulation();
			if (iAngryPop > 0) {
				szTempBuffer.Format(L", %d %c", iAngryPop, gDLL->getSymbolID(ANGRY_POP_CHAR));
				szString.append(szTempBuffer);
			}
		} else {
			iRate = pCity->happyLevel() - pCity->unhappyLevel();
			if (iRate > 0) {
				szTempBuffer.Format(L", %d %c", iRate, gDLL->getSymbolID(HAPPY_CHAR));
				szString.append(szTempBuffer);
			} else if (iRate < 0) {
				szTempBuffer.Format(L", %d %c", -iRate, gDLL->getSymbolID(UNHAPPY_CHAR));
				szString.append(szTempBuffer);
			}
		}
	}

	// Hurry anger turns
	if (getOptionBOOL("CityBar__HurryAnger", "False") && pCity->getOwnerINLINE() == GC.getGameINLINE().getActivePlayer()) {
		iRate = pCity->getHurryAngerTimer();
		if (iRate > 0) {
			int iPop = ((iRate - 1) / pCity->flatHurryAngerLength() + 1) * GC.getDefineINT("HURRY_POP_ANGER");
			szTempBuffer.Format(L" (%d %c %d)", iPop, gDLL->getSymbolID(ANGRY_POP_CHAR), iRate);
			szString.append(szTempBuffer);
		}
	}

	// Draft anger turns
	if (getOptionBOOL("CityBar__DraftAnger", "False") && pCity->getOwnerINLINE() == GC.getGameINLINE().getActivePlayer()) {
		iRate = pCity->getConscriptAngerTimer();
		if (iRate > 0) {
			int iPop = ((iRate - 1) / pCity->flatConscriptAngerLength() + 1) * GC.getDefineINT("CONSCRIPT_POP_ANGER");
			szTempBuffer.Format(L" (%d %c %d)", iPop, gDLL->getSymbolID(CITIZEN_CHAR), iRate);
			szString.append(szTempBuffer);
		}
	}

	// Food assist
	if ((iFoodDifference != 0 || !pCity->isFoodProduction()) && getOptionBOOL("CityBar__FoodAssist", "False")) {
		if (iFoodDifference > 0) {
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_FOOD_GROW", iFoodDifference, pCity->getFood(), pCity->growthThreshold(), pCity->getFoodTurnsLeft()));
		} else if (iFoodDifference == 0) {
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_FOOD_STAGNATE", pCity->getFood(), pCity->growthThreshold()));
		} else if (pCity->getFood() + iFoodDifference >= 0) {
			int iTurnsToStarve = pCity->getFood() / -iFoodDifference + 1;
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_FOOD_SHRINK", iFoodDifference, pCity->getFood(), pCity->growthThreshold(), iTurnsToStarve));
		} else {
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_FOOD_STARVE", iFoodDifference, pCity->getFood(), pCity->growthThreshold()));
		}
	} else {
		if (iFoodDifference <= 0) {
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_GROWTH", pCity->getFood(), pCity->growthThreshold()));
		} else {
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_FOOD_GROWTH", iFoodDifference, pCity->getFood(), pCity->growthThreshold(), pCity->getFoodTurnsLeft()));
		}
	}

	// Base production
	bool bBaseValues = (gDLL->ctrlKey() && getOptionBOOL("CityBar__BaseValues", "True"));
	bool bBaseProduction = getOptionBOOL("CityBar__BaseProduction", "False");
	if (pCity->getProductionNeeded() != MAX_INT) {
		int iBaseProductionDiffNoFood;
		if (bBaseValues) {
			iBaseProductionDiffNoFood = pCity->getBaseYieldRate(YIELD_PRODUCTION);
		} else {
			iBaseProductionDiffNoFood = pCity->getCurrentProductionDifference(true, false);
		}
		int iProductionDiffNoFood = pCity->getCurrentProductionDifference(true, true);
		int iProductionDiffJustFood = (pCity->getCurrentProductionDifference(false, true) - iProductionDiffNoFood);

		if (iProductionDiffJustFood > 0) {
			if ((iProductionDiffNoFood != iBaseProductionDiffNoFood) && bBaseProduction) {
				szString.append(gDLL->getText("TXT_KEY_CITY_BAR_FOOD_HAMMER_PRODUCTION_WITH_BASE", iProductionDiffJustFood, iProductionDiffNoFood, pCity->getProductionName(), pCity->getProduction(), pCity->getProductionNeeded(), pCity->getProductionTurnsLeft(), iBaseProductionDiffNoFood));
			} else {
				szString.append(gDLL->getText("TXT_KEY_CITY_BAR_FOOD_HAMMER_PRODUCTION", iProductionDiffJustFood, iProductionDiffNoFood, pCity->getProductionName(), pCity->getProduction(), pCity->getProductionNeeded(), pCity->getProductionTurnsLeft()));
			}
		} else if (iProductionDiffNoFood > 0) {
			if ((iProductionDiffNoFood != iBaseProductionDiffNoFood) && bBaseProduction) {
				szString.append(gDLL->getText("TXT_KEY_CITY_BAR_HAMMER_PRODUCTION_WITH_BASE", iProductionDiffNoFood, pCity->getProductionName(), pCity->getProduction(), pCity->getProductionNeeded(), pCity->getProductionTurnsLeft(), iBaseProductionDiffNoFood));
			} else {
				szString.append(gDLL->getText("TXT_KEY_CITY_BAR_HAMMER_PRODUCTION", iProductionDiffNoFood, pCity->getProductionName(), pCity->getProduction(), pCity->getProductionNeeded(), pCity->getProductionTurnsLeft()));
			}
		} else {
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_PRODUCTION", pCity->getProductionName(), pCity->getProduction(), pCity->getProductionNeeded()));
		}

		if (pCity->getOwnerINLINE() == GC.getGame().getActivePlayer() && getOptionBOOL("CityBar__BuildingActualEffects", true)) {
			if (pCity->isProductionBuilding()) {
				BuildingTypes eBuilding = pCity->getProductionBuilding();
				CvWString szStart;

				szStart.Format(NEWLINE L"<img=%S size=24></img>", GC.getBuildingInfo(eBuilding).getButton());
				setBuildingNetEffectsHelp(szString, eBuilding, pCity);
			}
		}
	} else if (bBaseProduction) {
		int iOverflow = pCity->getOverflowProduction();
		int iBaseProductionDiffNoFood;
		if (bBaseValues) {
			iBaseProductionDiffNoFood = pCity->getBaseYieldRate(YIELD_PRODUCTION);
		} else {
			iBaseProductionDiffNoFood = pCity->getCurrentProductionDifference(true, false);
		}
		if (iOverflow > 0 || iBaseProductionDiffNoFood > 0) {
			if (iOverflow > 0) {
				szString.append(gDLL->getText("TXT_KEY_CITY_BAR_BASE_PRODUCTION_WITH_OVERFLOW", iOverflow, iBaseProductionDiffNoFood));
			} else {
				szString.append(gDLL->getText("TXT_KEY_CITY_BAR_BASE_PRODUCTION", iBaseProductionDiffNoFood));
			}
		}
	}

	// Hurry assist
	bool bFirst = true;
	if (getOptionBOOL("CityBar__HurryAssist", "False") && pCity->getOwnerINLINE() == GC.getGameINLINE().getActivePlayer()) {
		bool bFirstHurry = true;
		for (HurryTypes eHurry = (HurryTypes)0; eHurry < GC.getNumHurryInfos(); eHurry = (HurryTypes)(eHurry + 1)) {
			if (pCity->canHurry(eHurry)) {
				if (bFirstHurry) {
					szString.append(NEWLINE);
					szString.append("Hurry:");
					bFirstHurry = false;
				}
				bFirst = true;
				szString.append(L" (");
				int iPopulation = pCity->hurryPopulation(eHurry);
				if (iPopulation > 0) {
					szTempBuffer.Format(L"%d %c", -iPopulation, gDLL->getSymbolID(CITIZEN_CHAR));
					setListHelp(szString, NULL, szTempBuffer, L", ", bFirst);
					bFirst = false;
				}
				int iGold = pCity->hurryGold(eHurry);
				if (iGold > 0) {
					szTempBuffer.Format(L"%d %c", -iGold, GC.getCommerceInfo(COMMERCE_GOLD).getChar());
					setListHelp(szString, NULL, szTempBuffer, L", ", bFirst);
					bFirst = false;
				}
				int iOverflowProduction = 0;
				int iOverflowGold = 0;
				if (pCity->hurryOverflow(eHurry, &iOverflowProduction, &iOverflowGold, getOptionBOOL("CityBar__HurryAssistIncludeCurrent", "False"))) {
					if (iOverflowProduction > 0) {
						szTempBuffer.Format(L"%d %c", iOverflowProduction, GC.getYieldInfo(YIELD_PRODUCTION).getChar());
						setListHelp(szString, NULL, szTempBuffer, L", ", bFirst);
						bFirst = false;
					}
					if (iOverflowGold > 0) {
						szTempBuffer.Format(L"%d %c", iOverflowGold, GC.getCommerceInfo(COMMERCE_GOLD).getChar());
						setListHelp(szString, NULL, szTempBuffer, L", ", bFirst);
						bFirst = false;
					}
				}
				szString.append(L")");
			}
		}
	}

	// Trade Detail
	if (getOptionBOOL("CityBar__TradeDetail", "False")) {
		int iTotalTrade = 0;
		int iDomesticTrade = 0;
		int iDomesticRoutes = 0;
		int iForeignTrade = 0;
		int iForeignRoutes = 0;
		pCity->calculateTradeTotals(YIELD_COMMERCE, iDomesticTrade, iDomesticRoutes, iForeignTrade, iForeignRoutes, NO_PLAYER, bBaseValues);
		iTotalTrade = iDomesticTrade + iForeignTrade;

		bFirst = true;
		if (iTotalTrade != 0) {
			szTempBuffer.Format(L"%c: %d %c", gDLL->getSymbolID(TRADE_CHAR), iTotalTrade, GC.getYieldInfo(YIELD_COMMERCE).getChar());
			setListHelp(szString, NEWLINE, szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
		if (iDomesticTrade != 0) {
			szTempBuffer.Format(L"%c: %d %c", gDLL->getSymbolID(STAR_CHAR), iDomesticTrade, GC.getYieldInfo(YIELD_COMMERCE).getChar());
			setListHelp(szString, NEWLINE, szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
		if (iForeignTrade != 0) {
			szTempBuffer.Format(L"%c: %d %c", gDLL->getSymbolID(SILVER_STAR_CHAR), iForeignTrade, GC.getYieldInfo(YIELD_COMMERCE).getChar());
			setListHelp(szString, NEWLINE, szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
	}

	// Commerce
	if (getOptionBOOL("CityBar__Commerce", "False")) {
		if (bBaseValues) {
			iRate = pCity->getBaseYieldRate(YIELD_COMMERCE);
		} else {
			iRate = pCity->getYieldRate(YIELD_COMMERCE);
		}
		if (iRate != 0) {
			szTempBuffer.Format(L"%d %c", iRate, GC.getYieldInfo(YIELD_COMMERCE).getChar());
			setListHelp(szString, NEWLINE, szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
	}

	for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
		if (bBaseValues) {
			iRate = pCity->getBaseCommerceRateTimes100(eCommerce);
		} else {
			iRate = pCity->getCommerceRateTimes100(eCommerce);
		}

		if (iRate != 0) {
			szTempBuffer.Format(L"%d.%02d %c", iRate / 100, iRate % 100, GC.getCommerceInfo(eCommerce).getChar());
			setListHelp(szString, NEWLINE, szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
	}

	iRate = bBaseValues ? pCity->getBaseGreatPeopleRate() : pCity->getGreatPeopleRate();
	if (iRate != 0) {
		szTempBuffer.Format(L"%d%c", iRate, gDLL->getSymbolID(GREAT_PEOPLE_CHAR));
		setListHelp(szString, NEWLINE, szTempBuffer, L", ", bFirst);
		bFirst = false;
	}

	if (!bFirst) {
		szString.append(gDLL->getText("TXT_KEY_PER_TURN"));
	}

	szString.append(NEWLINE);
	szString.append(gDLL->getText("INTERFACE_CITY_MAINTENANCE"));
	int iMaintenance = pCity->getMaintenanceTimes100();
	szString.append(CvWString::format(L" -%d.%02d %c", iMaintenance / 100, iMaintenance % 100, GC.getCommerceInfo(COMMERCE_GOLD).getChar()));

	// Building icons
	if (getOptionBOOL("CityBar__BuildingIcons", "False")) {
		bFirst = true;
		for (BuildingTypes eBuilding = (BuildingTypes)0; eBuilding < GC.getNumBuildingInfos(); eBuilding = (BuildingTypes)(eBuilding + 1)) {
			if (pCity->getNumRealBuilding(eBuilding) > 0) {
				if (bFirst) {
					szString.append(NEWLINE);
					bFirst = false;
				}
				szTempBuffer.Format(L"<img=%S size=24></img>", GC.getBuildingInfo(eBuilding).getButton());
				szString.append(szTempBuffer);
			}
		}
	} else {
		bFirst = true;
		for (BuildingTypes eBuilding = (BuildingTypes)0; eBuilding < GC.getNumBuildingInfos(); eBuilding = (BuildingTypes)(eBuilding + 1)) {
			if (pCity->getNumRealBuilding(eBuilding) > 0) {
				setListHelp(szString, NEWLINE, GC.getBuildingInfo(eBuilding).getDescription(), L", ", bFirst);
				bFirst = false;
			}
		}
	}

	// Culture turns
	int iCultureRate = pCity->getCommerceRateTimes100(COMMERCE_CULTURE);
	if (iCultureRate > 0 && getOptionBOOL("CityBar__CultureTurns", "False")) {
		if (pCity->getCultureLevel() != NO_CULTURELEVEL) {
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_CULTURE", pCity->getCulture(pCity->getOwnerINLINE()), pCity->getCultureThreshold(), GC.getCultureLevelInfo(pCity->getCultureLevel()).getTextKeyWide()));
		} else {
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_CULTURE_NO_LEVEL", pCity->getCulture(pCity->getOwnerINLINE()), pCity->getCultureThreshold()));
		}
		// all values are *100
		int iCulture = pCity->getCultureTimes100(pCity->getOwnerINLINE());
		int iCultureLeft = 100 * pCity->getCultureThreshold() - iCulture;
		int iCultureTurns = (iCultureLeft + iCultureRate - 1) / iCultureRate;
		szString.append(L" ");
		szString.append(gDLL->getText("INTERFACE_CITY_TURNS", iCultureTurns));
	} else {
		if (pCity->getCultureLevel() != NO_CULTURELEVEL) {
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_CULTURE", pCity->getCulture(pCity->getOwnerINLINE()), pCity->getCultureThreshold(), GC.getCultureLevelInfo(pCity->getCultureLevel()).getTextKeyWide()));
		}
	}

	// Great People turns
	int iGppRate = pCity->getGreatPeopleRate();
	if (iGppRate > 0 && getOptionBOOL("CityBar__GreatPersonTurns", "False")) {
		int iGpp = pCity->getGreatPeopleProgress();
		int iGppTotal = GET_PLAYER(pCity->getOwnerINLINE()).greatPeopleThreshold(false);
		szString.append(gDLL->getText("TXT_KEY_CITY_BAR_GREAT_PEOPLE", iGpp, iGppTotal));
		int iGppLeft = iGppTotal - iGpp;
		int iGppTurns = (iGppLeft + iGppRate - 1) / iGppRate;
		szString.append(L" ");
		szString.append(gDLL->getText("INTERFACE_CITY_TURNS", iGppTurns));
	} else {
		if (pCity->getGreatPeopleProgress() > 0) {
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_GREAT_PEOPLE", pCity->getGreatPeopleProgress(), GET_PLAYER(pCity->getOwnerINLINE()).greatPeopleThreshold(false)));
		}
	}

	// Specialists
	if (getOptionBOOL("CityBar__Specialists", "False")) {
		// regular specialists
		bFirst = true;
		for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
			int iCount = pCity->getSpecialistCount(eSpecialist);
			if (iCount > 0) {
				if (bFirst) {
					szString.append(NEWLINE);
					bFirst = false;
				}
				CvSpecialistInfo& kSpecialistInfo = GC.getSpecialistInfo(eSpecialist);
				for (int iJ = 0; iJ < iCount; ++iJ) {
					szTempBuffer.Format(L"<img=%S size=24></img>", kSpecialistInfo.getButton());
					szString.append(szTempBuffer);
				}
			}
		}

		// free specialists (ToA, GL) and settled great people
		bFirst = true;
		for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
			int iCount = pCity->getFreeSpecialistCount(eSpecialist);
			if (iCount > 0) {
				if (bFirst) {
					szString.append(NEWLINE);
					bFirst = false;
				}
				CvSpecialistInfo& kSpecialistInfo = GC.getSpecialistInfo(eSpecialist);
				for (int iJ = 0; iJ < iCount; ++iJ) {
					szTempBuffer.Format(L"<img=%S size=24></img>", kSpecialistInfo.getButton());
					szString.append(szTempBuffer);
				}
			}
		}
	}

	int iNumUnits = pCity->plot()->countNumAirUnits(GC.getGameINLINE().getActiveTeam());
	if (pCity->getAirUnitCapacity(GC.getGameINLINE().getActiveTeam()) > 0 && iNumUnits > 0) {
		szString.append(NEWLINE);
		szString.append(gDLL->getText("TXT_KEY_CITY_BAR_AIR_UNIT_CAPACITY", iNumUnits, pCity->getAirUnitCapacity(GC.getGameINLINE().getActiveTeam())));
	}

	// Revolt Chance
	if (getOptionBOOL("CityBar__RevoltChance", "False")) {
		PlayerTypes eCulturalOwner = pCity->plot()->calculateCulturalOwner();

		if (eCulturalOwner != NO_PLAYER) {
			if (GET_PLAYER(eCulturalOwner).getTeam() != pCity->getTeam()) {
				int iCityStrength = pCity->cultureStrength(eCulturalOwner);
				int iGarrison = pCity->cultureGarrison(eCulturalOwner);

				if (iCityStrength > iGarrison) {
					szTempBuffer.Format(L"%.2f", std::max(0.0f, (1.0f - (float)iGarrison / (float)iCityStrength)) * std::min(100.0f, (float)pCity->getRevoltTestProbability()));
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_MISC_CHANCE_OF_REVOLT", szTempBuffer.GetCString()));
				}
			}
		}
	}

	// Hide UI Instructions
	if (!getOptionBOOL("CityBar__HideInstructions", "False")) {
		if (bBaseValues) {
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_CTRL_BASE_VALUES"));
		}
		// unchanged
		szString.append(gDLL->getText("TXT_KEY_CITY_BAR_SELECT", pCity->getNameKey()));
		szString.append(gDLL->getText("TXT_KEY_CITY_BAR_SELECT_CTRL"));
		szString.append(gDLL->getText("TXT_KEY_CITY_BAR_SELECT_ALT"));
	}
}


void CvGameTextMgr::parseTraits(CvWStringBuffer& szHelpString, TraitTypes eTrait, CivilizationTypes eCivilization, bool bDawnOfMan) {
	PROFILE_FUNC();

	CvWString szTempBuffer;
	if (eTrait == NO_TRAIT) {
		// Unknown Trait
		szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), gDLL->getText("TXT_KEY_TRAIT_PLAYER_UNKNOWN").c_str());
		szHelpString.append(szTempBuffer);
		return;
	}

	const CvTraitInfo& kTrait = GC.getTraitInfo(eTrait);

	// Trait Name
	CvWString szText = kTrait.getDescription();
	if (bDawnOfMan) {
		szTempBuffer.Format(L"%s", szText.GetCString());
	} else {
		szTempBuffer.Format(NEWLINE SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_ALT_HIGHLIGHT_TEXT"), szText.GetCString());
	}
	szHelpString.append(szTempBuffer);

	if (!bDawnOfMan) {
		if (!CvWString(kTrait.getHelp()).empty()) {
			szHelpString.append(kTrait.getHelp());
		}

		if (kTrait.isUnitRangeUnbound()) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_ENABLES_UNIT_RANGE_UNBOUND"));
		}

		if (kTrait.isUnitTerritoryUnbound()) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_ENABLES_UNIT_TERRITORY_UNBOUND"));
		}

		if (kTrait.getUnitRangeChange() != 0) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_UNIT_RANGE_CHANGE", kTrait.getUnitRangeChange()));
		}

		if (kTrait.getUnitRangePercentChange() != 0) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_UNIT_RANGE_MODIFY", kTrait.getUnitRangePercentChange()));
		}

		CvWString szBuffer;
		if (kTrait.getStarSignMitigateChangePercent() != 0) {
			szBuffer = kTrait.getStarSignMitigateChangePercent() > 0 ? "TXT_KEY_STAR_SIGN_TRAIT_MITIGATE_GOOD" : "TXT_KEY_STAR_SIGN_TRAIT_MITIGATE_BAD";
			szHelpString.append(NEWLINE);
			szHelpString.append(gDLL->getText(szBuffer, abs(kTrait.getStarSignMitigateChangePercent())));
		}
		if (kTrait.getStarSignScaleChangePercent() != 0) {
			szBuffer = kTrait.getStarSignScaleChangePercent() > 0 ? "TXT_KEY_STAR_SIGN_TRAIT_SCALE_GOOD" : "TXT_KEY_STAR_SIGN_TRAIT_SCALE_BAD";
			szHelpString.append(NEWLINE);
			szHelpString.append(gDLL->getText(szBuffer, abs(kTrait.getStarSignScaleChangePercent())));
		}

		// iHealth
		if (kTrait.getHealth() != 0) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_HEALTH", kTrait.getHealth()));
		}

		// iHappiness
		if (kTrait.getHappiness() != 0) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_HAPPINESS", kTrait.getHappiness()));
		}

		// iMaxAnarchy
		if (kTrait.getMaxAnarchy() != -1) {
			if (kTrait.getMaxAnarchy() == 0) {
				szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_NO_ANARCHY"));
			} else {
				szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_MAX_ANARCHY", kTrait.getMaxAnarchy()));
			}
		}

		// iUpkeepModifier
		if (kTrait.getUpkeepModifier() != 0) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_CIVIC_UPKEEP_MODIFIER", kTrait.getUpkeepModifier()));
		}

		// iLevelExperienceModifier
		if (kTrait.getLevelExperienceModifier() != 0) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_CIVIC_LEVEL_MODIFIER", kTrait.getLevelExperienceModifier()));
		}

		// iGreatPeopleRateModifier
		if (kTrait.getGreatPeopleRateModifier() != 0) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_GREAT_PEOPLE_MODIFIER", kTrait.getGreatPeopleRateModifier()));
		}

		// iGreatGeneralRateModifier
		if (kTrait.getGreatGeneralRateModifier() != 0) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_GREAT_GENERAL_MODIFIER", kTrait.getGreatGeneralRateModifier()));
		}

		if (kTrait.getDomesticGreatGeneralRateModifier() != 0) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_DOMESTIC_GREAT_GENERAL_MODIFIER", kTrait.getDomesticGreatGeneralRateModifier()));
		}

		// Wonder Production Effects
		if ((kTrait.getMaxGlobalBuildingProductionModifier() != 0)
			|| (kTrait.getMaxTeamBuildingProductionModifier() != 0)
			|| (kTrait.getMaxPlayerBuildingProductionModifier() != 0)) {
			if ((kTrait.getMaxGlobalBuildingProductionModifier() == kTrait.getMaxTeamBuildingProductionModifier())
				&& (kTrait.getMaxGlobalBuildingProductionModifier() == kTrait.getMaxPlayerBuildingProductionModifier())) {
				szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_WONDER_PRODUCTION_MODIFIER", kTrait.getMaxGlobalBuildingProductionModifier()));
			} else {
				if (kTrait.getMaxGlobalBuildingProductionModifier() != 0) {
					szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_WORLD_WONDER_PRODUCTION_MODIFIER", kTrait.getMaxGlobalBuildingProductionModifier()));
				}

				if (kTrait.getMaxTeamBuildingProductionModifier() != 0) {
					szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_TEAM_WONDER_PRODUCTION_MODIFIER", kTrait.getMaxTeamBuildingProductionModifier()));
				}

				if (kTrait.getMaxPlayerBuildingProductionModifier() != 0) {
					szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_NATIONAL_WONDER_PRODUCTION_MODIFIER", kTrait.getMaxPlayerBuildingProductionModifier()));
				}
			}
		}

		// ExtraYieldThresholds
		for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1)) {
			const CvYieldInfo& kYield = GC.getYieldInfo(eYield);
			if (kTrait.getExtraYieldThreshold(eYield) > 0) {
				szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_EXTRA_YIELD_THRESHOLDS", kYield.getChar(), kTrait.getExtraYieldThreshold(eYield), kYield.getChar()));
			}
			// Trade Yield Modifiers
			if (kTrait.getTradeYieldModifier(eYield) != 0) {
				szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_TRADE_YIELD_MODIFIERS", kTrait.getTradeYieldModifier(eYield), kYield.getChar(), "YIELD"));
			}
		}

		// CommerceChanges
		for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
			const CvCommerceInfo& kCommerce = GC.getCommerceInfo(eCommerce);
			if (kTrait.getCommerceChange(eCommerce) != 0) {
				szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_COMMERCE_CHANGES", kTrait.getCommerceChange(eCommerce), kCommerce.getChar(), "COMMERCE"));
			}

			if (kTrait.getCommerceModifier(eCommerce) != 0) {
				szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_COMMERCE_MODIFIERS", kTrait.getCommerceModifier(eCommerce), kCommerce.getChar(), "COMMERCE"));
			}
		}

		//Yield From Unit Modifiers
		szTempBuffer.clear();
		bool bFoundKillYield = false;
		int iLast = 0;

		for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1)) {
			int iYield = kTrait.getYieldFromUnitModifier(eYield);
			if (iYield != 0) {
				if (iYield != iLast) {
					szTempBuffer += CvWString::format(L"%d%% %c", iYield, GC.getYieldInfo(eYield).getChar());
					iLast = iYield;
				} else {
					szTempBuffer += L", ";
					szTempBuffer += CvWString::format(L"%c", GC.getYieldInfo(eYield).getChar());
				}
				bFoundKillYield = true;
			}
		}

		if (bFoundKillYield) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_FROM_UNIT_MODS", szTempBuffer.GetCString()));
		}

		// Base Yield From Units
		szTempBuffer.clear();
		bool bFirst = true;
		bFoundKillYield = false;
		iLast = 0;

		for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1)) {
			int iYield = kTrait.getBaseYieldFromUnit(eYield);
			if (iYield != 0) {
				if (bFirst) {
					szTempBuffer += gDLL->getText("TXT_KEY_TRAIT_BASE_FROM_UNIT");
					bFirst = false;
				}
				if (iYield != iLast) {
					szTempBuffer += CvWString::format(L"%d %c", iYield, GC.getYieldInfo(eYield).getChar());
					iLast = iYield;
				} else {
					szTempBuffer += L", ";
					szTempBuffer += CvWString::format(L"%c", GC.getYieldInfo(eYield).getChar());
				}
				bFoundKillYield = true;
			}
		}

		if (bFoundKillYield) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_BASE_FROM_UNIT", szTempBuffer.GetCString()));
		}

		//Commerce From Unit Modifiers
		szTempBuffer.clear();
		bool bFoundKillCommerce = false;
		iLast = 0;

		for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
			int iCommerce = kTrait.getCommerceFromUnitModifier(eCommerce);
			if (iCommerce != 0) {
				if (iCommerce != iLast) {
					szTempBuffer += CvWString::format(L"%d%% %c", iCommerce, GC.getCommerceInfo(eCommerce).getChar());
					iLast = iCommerce;
				} else {
					szTempBuffer += L", ";
					szTempBuffer += CvWString::format(L"%c", GC.getCommerceInfo(eCommerce).getChar());
				}
				bFoundKillCommerce = true;
			}
		}

		if (bFoundKillCommerce) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_FROM_UNIT_MODS", szTempBuffer.GetCString()));
		}

		// Base Commerce From Units
		szTempBuffer.clear();
		bFirst = true;
		bFoundKillCommerce = false;
		iLast = 0;

		for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
			int iCommerce = kTrait.getBaseCommerceFromUnit(eCommerce);
			if (iCommerce != 0) {
				if (bFirst) {
					szTempBuffer += gDLL->getText("TXT_KEY_TRAIT_BASE_FROM_UNIT");
					bFirst = false;
				}
				if (iCommerce != iLast) {
					szTempBuffer += CvWString::format(L"%d %c", iCommerce, GC.getCommerceInfo(eCommerce).getChar());
					iLast = iCommerce;
				} else {
					szTempBuffer += L", ";
					szTempBuffer += CvWString::format(L"%c", GC.getCommerceInfo(eCommerce).getChar());
				}
				bFoundKillCommerce = true;
			}
		}

		if (bFoundKillCommerce) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_BASE_FROM_UNIT", szTempBuffer.GetCString()));
		}

		// Free Promotions
		bool bFoundPromotion = false;
		szTempBuffer.clear();
		for (PromotionTypes ePromotion = (PromotionTypes)0; ePromotion < GC.getNumPromotionInfos(); ePromotion = (PromotionTypes)(ePromotion + 1)) {
			if (kTrait.isFreePromotion(ePromotion)) {
				if (bFoundPromotion) {
					szTempBuffer += L", ";
				}

				szTempBuffer += CvWString::format(L"<link=literal>%s</link>", GC.getPromotionInfo(ePromotion).getDescription());
				bFoundPromotion = true;
			}
		}

		if (bFoundPromotion) {
			szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_FREE_PROMOTIONS", szTempBuffer.GetCString()));

			for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
				if (kTrait.isFreePromotionUnitCombat(eUnitCombat)) {
					szTempBuffer.Format(L"\n        %c<link=literal>%s</link>", gDLL->getSymbolID(BULLET_CHAR), GC.getUnitCombatInfo(eUnitCombat).getDescription());
					szHelpString.append(szTempBuffer);
				}
			}
		}

		// No Civic Maintenance
		for (CivicOptionTypes eCivicOption = (CivicOptionTypes)0; eCivicOption < GC.getNumCivicOptionInfos(); eCivicOption = (CivicOptionTypes)(eCivicOption + 1)) {
			const CvCivicOptionInfo& kCivicOption = GC.getCivicOptionInfo(eCivicOption);
			if (kCivicOption.getTraitNoUpkeep(eTrait)) {
				szHelpString.append(gDLL->getText("TXT_KEY_TRAIT_NO_UPKEEP", kCivicOption.getTextKeyWide()));
			}
		}

		// Increase Building/Unit Production Speeds
		iLast = 0;
		for (SpecialUnitTypes eSpecialUnit = (SpecialUnitTypes)0; eSpecialUnit < GC.getNumSpecialUnitInfos(); eSpecialUnit = (SpecialUnitTypes)(eSpecialUnit + 1)) {
			const CvSpecialUnitInfo& kSpecialUnit = GC.getSpecialUnitInfo(eSpecialUnit);
			if (kSpecialUnit.getProductionTraits(eTrait) != 0) {
				if (kSpecialUnit.getProductionTraits(eTrait) == 100) {
					szText = gDLL->getText("TXT_KEY_TRAIT_DOUBLE_SPEED");
				} else {
					szText = gDLL->getText("TXT_KEY_TRAIT_PRODUCTION_MODIFIER", kSpecialUnit.getProductionTraits(eTrait));
				}
				setListHelp(szHelpString, szText.GetCString(), kSpecialUnit.getDescription(), L", ", kSpecialUnit.getProductionTraits(eTrait) != iLast);
				iLast = kSpecialUnit.getProductionTraits(eTrait);
			}
		}

		// Unit Classes
		UnitTypes eLoopUnit = NO_UNIT;
		iLast = 0;
		for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
			if (eCivilization == NO_CIVILIZATION) {
				eLoopUnit = ((UnitTypes)(GC.getUnitClassInfo(eUnitClass).getDefaultUnitIndex()));
			} else {
				eLoopUnit = ((UnitTypes)(GC.getCivilizationInfo(eCivilization).getCivilizationUnits(eUnitClass)));
			}

			if (eLoopUnit != NO_UNIT && !isWorldUnitClass(eUnitClass)) {
				const CvUnitInfo& kUnit = GC.getUnitInfo(eLoopUnit);
				if (kUnit.getProductionTraits(eTrait) != 0) {
					if (kUnit.getProductionTraits(eTrait) == 100) {
						szText = gDLL->getText("TXT_KEY_TRAIT_DOUBLE_SPEED");
					} else {
						szText = gDLL->getText("TXT_KEY_TRAIT_PRODUCTION_MODIFIER", kUnit.getProductionTraits(eTrait));
					}
					CvWString szUnit;
					szUnit.Format(L"<link=literal>%s</link>", kUnit.getDescription());
					setListHelp(szHelpString, szText.GetCString(), szUnit, L", ", (kUnit.getProductionTraits(eTrait) != iLast));
					iLast = kUnit.getProductionTraits(eTrait);
				}
			}
		}

		// SpecialBuildings
		iLast = 0;
		for (SpecialBuildingTypes eSpecialBuilding = (SpecialBuildingTypes)0; eSpecialBuilding < GC.getNumSpecialBuildingInfos(); eSpecialBuilding = (SpecialBuildingTypes)(eSpecialBuilding + 1)) {
			const CvSpecialBuildingInfo& kSpecialBuilding = GC.getSpecialBuildingInfo(eSpecialBuilding);
			if (kSpecialBuilding.getProductionTraits(eTrait) != 0) {
				if (kSpecialBuilding.getProductionTraits(eTrait) == 100) {
					szText = gDLL->getText("TXT_KEY_TRAIT_DOUBLE_SPEED");
				} else {
					szText = gDLL->getText("TXT_KEY_TRAIT_PRODUCTION_MODIFIER", kSpecialBuilding.getProductionTraits(eTrait));
				}
				setListHelp(szHelpString, szText.GetCString(), kSpecialBuilding.getDescription(), L", ", (kSpecialBuilding.getProductionTraits(eTrait) != iLast));
				iLast = kSpecialBuilding.getProductionTraits(eTrait);
			}
		}

		// Buildings
		iLast = 0;
		BuildingTypes eLoopBuilding;
		for (BuildingClassTypes eBuildingClass = (BuildingClassTypes)0; eBuildingClass < GC.getNumBuildingClassInfos(); eBuildingClass = (BuildingClassTypes)(eBuildingClass + 1)) {
			if (eCivilization == NO_CIVILIZATION) {
				eLoopBuilding = ((BuildingTypes)(GC.getBuildingClassInfo(eBuildingClass).getDefaultBuildingIndex()));
			} else {
				eLoopBuilding = ((BuildingTypes)(GC.getCivilizationInfo(eCivilization).getCivilizationBuildings(eBuildingClass)));
			}

			if (eLoopBuilding != NO_BUILDING && !isWorldWonderClass(eBuildingClass)) {
				const CvBuildingInfo& kLoopBuilding = GC.getBuildingInfo(eLoopBuilding);
				if (kLoopBuilding.getProductionTraits(eTrait) != 0) {
					if (kLoopBuilding.getProductionTraits(eTrait) == 100) {
						szText = gDLL->getText("TXT_KEY_TRAIT_DOUBLE_SPEED");
					} else {
						szText = gDLL->getText("TXT_KEY_TRAIT_PRODUCTION_MODIFIER", kLoopBuilding.getProductionTraits(eTrait));
					}

					CvWString szBuilding;
					szBuilding.Format(L"<link=literal>%s</link>", kLoopBuilding.getDescription());
					setListHelp(szHelpString, szText.GetCString(), szBuilding, L", ", (kLoopBuilding.getProductionTraits(eTrait) != iLast));
					iLast = kLoopBuilding.getProductionTraits(eTrait);
				}
			}
		}

		// Buildings
		iLast = 0;
		for (BuildingClassTypes eBuildingClass = (BuildingClassTypes)0; eBuildingClass < GC.getNumBuildingClassInfos(); eBuildingClass = (BuildingClassTypes)(eBuildingClass + 1)) {
			if (eCivilization == NO_CIVILIZATION) {
				eLoopBuilding = ((BuildingTypes)(GC.getBuildingClassInfo(eBuildingClass).getDefaultBuildingIndex()));
			} else {
				eLoopBuilding = ((BuildingTypes)(GC.getCivilizationInfo(eCivilization).getCivilizationBuildings(eBuildingClass)));
			}

			if (eLoopBuilding != NO_BUILDING && !isWorldWonderClass(eBuildingClass)) {
				const CvBuildingInfo& kLoopBuilding = GC.getBuildingInfo(eLoopBuilding);
				int iHappiness = kLoopBuilding.getHappinessTraits(eTrait);
				if (iHappiness != 0) {
					if (iHappiness > 0) {
						szText = gDLL->getText("TXT_KEY_TRAIT_BUILDING_HAPPINESS", iHappiness, gDLL->getSymbolID(HAPPY_CHAR));
					} else {
						szText = gDLL->getText("TXT_KEY_TRAIT_BUILDING_HAPPINESS", -iHappiness, gDLL->getSymbolID(UNHAPPY_CHAR));
					}

					CvWString szBuilding;
					szBuilding.Format(L"<link=literal>%s</link>", kLoopBuilding.getDescription());
					setListHelp(szHelpString, szText.GetCString(), szBuilding, L", ", (iHappiness != iLast));
					iLast = iHappiness;
				}
			}
		}

	}
}


//
// parseLeaderTraits - SimpleCivPicker							// LOCALIZATION READY
//
void CvGameTextMgr::parseLeaderTraits(CvWStringBuffer& szHelpString, LeaderHeadTypes eLeader, CivilizationTypes eCivilization, bool bDawnOfMan, bool bCivilopediaText) {
	PROFILE_FUNC();

	//	Build help string
	CvWString szTempBuffer;	// Formatting
	if (eLeader != NO_LEADER) {
		if (!bDawnOfMan && !bCivilopediaText) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getLeaderHeadInfo(eLeader).getDescription());
			szHelpString.append(szTempBuffer);

			for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
				const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
				if (kPlayer.isEverAlive() && kPlayer.getLeaderType() == eLeader) {
					// World Views
					bool bFirst = true;
					for (WorldViewTypes eWorldView = (WorldViewTypes)0; eWorldView < NUM_WORLD_VIEWS; eWorldView = (WorldViewTypes)(eWorldView + 1)) {
						if (kPlayer.isWorldViewActivated(eWorldView)) {
							if (!bFirst) {
								szHelpString.append(L", ");
							} else {
								szHelpString.append(gDLL->getText("TXT_KEY_WV_LABEL"));
								bFirst = false;
							}
							szHelpString.append(GC.getWorldViewInfo(eWorldView).getDescription());
						}
					}
					// Star Signs
					if (!kPlayer.isStarSignForceDisabled()) {
						if (kPlayer.getStarSignScalePercent() != 0)
							szHelpString.append(gDLL->getText("TXT_KEY_STAR_SIGN_PLAYER_SCALE", kPlayer.getStarSignScalePercent()));
						if (kPlayer.isStarSignGoodOnly())
							szHelpString.append(gDLL->getText("TXT_KEY_STAR_SIGN_PLAYER_NO_BAD"));
						else if (kPlayer.getStarSignMitigatePercent() != 0)
							szHelpString.append(gDLL->getText("TXT_KEY_STAR_SIGN_PLAYER_MITIGATE", kPlayer.getStarSignMitigatePercent()));
					}
					break;
				}
			}
		}

		FAssert((GC.getNumTraitInfos() > 0) &&
			"GC.getNumTraitInfos() is less than or equal to zero but is expected to be larger than zero in CvSimpleCivPicker::setLeaderText");

		bool bFirst = true;
		for (TraitTypes eTrait = (TraitTypes)0; eTrait < GC.getNumTraitInfos(); eTrait = (TraitTypes)(eTrait + 1)) {
			if (GC.getGame().isLeaderEverActive(eLeader)) {
				for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
					CvPlayer& kPlayer = GET_PLAYER(ePlayer);
					if (kPlayer.isEverAlive() && kPlayer.getLeaderType() == eLeader && kPlayer.hasTrait(eTrait)) {
						if (!bFirst) {
							if (bDawnOfMan) {
								szHelpString.append(L", ");
							}
						} else {
							bFirst = false;
						}
						parseTraits(szHelpString, eTrait, eCivilization, bDawnOfMan);
					}
				}
			} else if (GC.getLeaderHeadInfo(eLeader).hasTrait(eTrait)) {
				if (!bFirst) {
					if (bDawnOfMan) {
						szHelpString.append(L", ");
					}
				} else {
					bFirst = false;
				}
				parseTraits(szHelpString, eTrait, eCivilization, bDawnOfMan);
			}
		}
	} else {
		//	Random leader
		szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), gDLL->getText("TXT_KEY_TRAIT_PLAYER_UNKNOWN").c_str());
		szHelpString.append(szTempBuffer);
	}
}

//
// parseLeaderTraits - SimpleCivPicker							// LOCALIZATION READY
//
void CvGameTextMgr::parseLeaderShortTraits(CvWStringBuffer& szHelpString, LeaderHeadTypes eLeader) {
	PROFILE_FUNC();

	//	Build help string
	if (eLeader != NO_LEADER) {
		FAssert((GC.getNumTraitInfos() > 0) &&
			"GC.getNumTraitInfos() is less than or equal to zero but is expected to be larger than zero in CvSimpleCivPicker::setLeaderText");

		bool bFirst = true;
		for (int iI = 0; iI < GC.getNumTraitInfos(); ++iI) {
			if (GC.getLeaderHeadInfo(eLeader).hasTrait(iI)) {
				if (!bFirst) {
					szHelpString.append(L"/");
				}
				szHelpString.append(gDLL->getText(GC.getTraitInfo((TraitTypes)iI).getShortDescription()));
				bFirst = false;
			}
		}
	} else {
		//	Random leader
		szHelpString.append(CvWString("???/???"));
	}
}

//
// Build Civilization Info Help Text
//
void CvGameTextMgr::parseCivInfos(CvWStringBuffer& szInfoText, CivilizationTypes eCivilization, bool bDawnOfMan, bool bLinks) {
	PROFILE_FUNC();

	CvWString szBuffer;
	CvWString szTempString;
	CvWString szText;

	if (eCivilization != NO_CIVILIZATION) {
		const CvCivilizationInfo& kCiv = GC.getCivilizationInfo(eCivilization);

		if (!bDawnOfMan) {
			// Civ Name
			szBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), kCiv.getDescription());
			szInfoText.append(szBuffer);

			// Free Techs
			szBuffer.Format(NEWLINE SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_ALT_HIGHLIGHT_TEXT"), gDLL->getText("TXT_KEY_FREE_TECHS").GetCString());
			szInfoText.append(szBuffer);

			bool bFound = false;
			for (int iI = 0; iI < GC.getNumTechInfos(); ++iI) {
				if (kCiv.isCivilizationFreeTechs(iI)) {
					bFound = true;
					// Add Tech
					szText.Format((bLinks ? L"<link=literal>%s</link>" : L"%s"), GC.getTechInfo((TechTypes)iI).getDescription());
					szBuffer.Format(L"%s  %c%s", NEWLINE, gDLL->getSymbolID(BULLET_CHAR), szText.GetCString());
					szInfoText.append(szBuffer);
				}
			}

			if (!bFound) {
				szBuffer.Format(L"%s  %s", NEWLINE, gDLL->getText("TXT_KEY_FREE_TECHS_NO").GetCString());
				szInfoText.append(szBuffer);
			}
		}

		// Free Units
		szText = gDLL->getText("TXT_KEY_FREE_UNITS");
		if (bDawnOfMan) {
			szTempString.Format(L"%s: ", szText.GetCString());
		} else {
			szTempString.Format(NEWLINE SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_ALT_HIGHLIGHT_TEXT"), szText.GetCString());
		}
		szInfoText.append(szTempString);

		bool bFound = false;
		for (int iI = 0; iI < GC.getNumUnitClassInfos(); ++iI) {
			UnitTypes eDefaultUnit = ((UnitTypes)(kCiv.getCivilizationUnits(iI)));
			UnitTypes eUniqueUnit = ((UnitTypes)(GC.getUnitClassInfo((UnitClassTypes)iI).getDefaultUnitIndex()));
			if ((eDefaultUnit != NO_UNIT) && (eUniqueUnit != NO_UNIT)) {
				if (eDefaultUnit != eUniqueUnit) {
					// Add Unit
					if (bDawnOfMan) {
						if (bFound) {
							szInfoText.append(L", ");
						}
						szBuffer.Format((bLinks ? L"<link=literal>%s</link> - (<link=literal>%s</link>)" : L"%s - (%s)"),
							GC.getUnitInfo(eDefaultUnit).getDescription(),
							GC.getUnitInfo(eUniqueUnit).getDescription());
					} else {
						szBuffer.Format(L"\n  %c%s - (%s)", gDLL->getSymbolID(BULLET_CHAR),
							GC.getUnitInfo(eDefaultUnit).getDescription(),
							GC.getUnitInfo(eUniqueUnit).getDescription());
					}
					szInfoText.append(szBuffer);
					bFound = true;
				}
			}
		}

		if (!bFound) {
			szText = gDLL->getText("TXT_KEY_FREE_UNITS_NO");
			if (bDawnOfMan) {
				szTempString.Format(L"%s", szText.GetCString());
			} else {
				szTempString.Format(L"%s  %s", NEWLINE, szText.GetCString());
			}
			szInfoText.append(szTempString);
			bFound = true;
		}


		// Free Buildings
		szText = gDLL->getText("TXT_KEY_UNIQUE_BUILDINGS");
		if (bDawnOfMan) {
			if (bFound) {
				szInfoText.append(NEWLINE);
			}
			szTempString.Format(L"%s: ", szText.GetCString());
		} else {
			szTempString.Format(NEWLINE SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_ALT_HIGHLIGHT_TEXT"), szText.GetCString());
		}
		szInfoText.append(szTempString);

		bFound = false;
		for (int iI = 0; iI < GC.getNumBuildingClassInfos(); ++iI) {
			BuildingTypes eDefaultBuilding = ((BuildingTypes)(kCiv.getCivilizationBuildings(iI)));
			BuildingTypes eUniqueBuilding = ((BuildingTypes)(GC.getBuildingClassInfo((BuildingClassTypes)iI).getDefaultBuildingIndex()));
			if ((eDefaultBuilding != NO_BUILDING) && (eUniqueBuilding != NO_BUILDING)) {
				if (eDefaultBuilding != eUniqueBuilding) {
					// Add Building
					if (bDawnOfMan) {
						if (bFound) {
							szInfoText.append(L", ");
						}
						szBuffer.Format((bLinks ? L"<link=literal>%s</link> - (<link=literal>%s</link>)" : L"%s - (%s)"),
							GC.getBuildingInfo(eDefaultBuilding).getDescription(),
							GC.getBuildingInfo(eUniqueBuilding).getDescription());
					} else {
						szBuffer.Format(L"\n  %c%s - (%s)", gDLL->getSymbolID(BULLET_CHAR),
							GC.getBuildingInfo(eDefaultBuilding).getDescription(),
							GC.getBuildingInfo(eUniqueBuilding).getDescription());
					}
					szInfoText.append(szBuffer);
					bFound = true;
				}
			}
		}
		if (!bFound) {
			szText = gDLL->getText("TXT_KEY_UNIQUE_BUILDINGS_NO");
			if (bDawnOfMan) {
				szTempString.Format(L"%s", szText.GetCString());
			} else {
				szTempString.Format(L"%s  %s", NEWLINE, szText.GetCString());
			}
			szInfoText.append(szTempString);
		}
	} else {
		//	This is a random civ, let us know here...
		szInfoText.append(gDLL->getText("TXT_KEY_CIV_UNKNOWN"));
	}
}

void CvGameTextMgr::parseSpecialistHelp(CvWStringBuffer& szHelpString, SpecialistTypes eSpecialist, CvCity* pCity, bool bCivilopediaText) {
	parseSpecialistHelpActual(szHelpString, eSpecialist, pCity, bCivilopediaText, 0);
}

void CvGameTextMgr::parseSpecialistHelpActual(CvWStringBuffer& szHelpString, SpecialistTypes eSpecialist, CvCity* pCity, bool bCivilopediaText, int iChange) {
	PROFILE_FUNC();

	CvWString szText;

	if (eSpecialist != NO_SPECIALIST) {
		int aiCommerces[NUM_COMMERCE_TYPES];
		int aiYields[NUM_YIELD_TYPES];
		const CvSpecialistInfo& kSpecialist = GC.getSpecialistInfo(eSpecialist); // K-Mod

		if (!bCivilopediaText) {
			szHelpString.append(kSpecialist.getDescription());
		}

		for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI) {
			if (GC.getGameINLINE().getActivePlayer() == NO_PLAYER) {
				aiYields[iI] = kSpecialist.getYieldChange(iI);
			} else {
				aiYields[iI] = GET_PLAYER((pCity != NULL) ? pCity->getOwnerINLINE() : GC.getGameINLINE().getActivePlayer()).specialistYield(eSpecialist, ((YieldTypes)iI));
			}
		}
		setYieldChangeHelp(szHelpString, L"", L"", L"", aiYields);

		for (int iI = 0; iI < NUM_COMMERCE_TYPES; ++iI) {
			if (GC.getGameINLINE().getActivePlayer() == NO_PLAYER) {
				aiCommerces[iI] = kSpecialist.getCommerceChange(iI);
			} else {
				aiCommerces[iI] = GET_PLAYER((pCity != NULL) ? pCity->getOwnerINLINE() : GC.getGameINLINE().getActivePlayer()).specialistCommerce(((SpecialistTypes)eSpecialist), ((CommerceTypes)iI));
			}
		}
		setCommerceChangeHelp(szHelpString, L"", L"", L"", aiCommerces);

		if (kSpecialist.getStarSignMitigateChange() != 0) {
			szText = kSpecialist.getStarSignMitigateChange() > 0 ? "TXT_KEY_STAR_SIGN_MITIGATE_GOOD" : "TXT_KEY_STAR_SIGN_MITIGATE_BAD";
			szHelpString.append(NEWLINE);
			szHelpString.append(gDLL->getText(szText, abs(kSpecialist.getStarSignMitigateChange())));
		}
		if (kSpecialist.getStarSignScaleChange() != 0) {
			szText = kSpecialist.getStarSignScaleChange() > 0 ? "TXT_KEY_STAR_SIGN_SCALE_GOOD" : "TXT_KEY_STAR_SIGN_SCALE_BAD";
			szHelpString.append(NEWLINE);
			szHelpString.append(gDLL->getText(szText, abs(kSpecialist.getStarSignScaleChange())));
		}

		if (kSpecialist.getExperience() > 0) {
			szHelpString.append(NEWLINE);
			szHelpString.append(gDLL->getText("TXT_KEY_SPECIALIST_EXPERIENCE", kSpecialist.getExperience()));
		}

		if (kSpecialist.getGreatPeopleRateChange() != 0) {
			szHelpString.append(NEWLINE);
			szHelpString.append(gDLL->getText("TXT_KEY_SPECIALIST_BIRTH_RATE", kSpecialist.getGreatPeopleRateChange()));

			if (!bCivilopediaText && gDLL->getChtLvl() > 0 && GC.ctrlKey()) {
				szHelpString.append(NEWLINE);
				szHelpString.append(CvWString::format(L"weight: %d", GET_PLAYER((pCity != NULL) ? pCity->getOwnerINLINE() : GC.getGameINLINE().getActivePlayer()).AI_getGreatPersonWeight((UnitClassTypes)kSpecialist.getGreatPeopleUnitClass())));
			}
		}

		// BUG - Specialist Actual Effects - start
		if (iChange != 0 && pCity && (GC.altKey() || getOptionBOOL("MiscHover__SpecialistActualEffects", true)) && (pCity->getOwnerINLINE() == GC.getGame().getActivePlayer() || gDLL->getChtLvl() > 0)) {
			bool bStarted = false;
			CvWString szStart;
			szStart.Format(L"\n"SETCOLR L"(%s", TEXT_COLOR("COLOR_LIGHT_GREY"), gDLL->getText("TXT_KEY_ACTUAL_EFFECTS").GetCString());

			// Yield
			for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1)) {
				aiYields[eYield] = pCity->getAdditionalYieldBySpecialist(eYield, eSpecialist, iChange);
			}
			bStarted = setResumableYieldChangeHelp(szHelpString, szStart, L": ", L"", aiYields, false, false, bStarted);

			// Commerce
			for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
				aiCommerces[eCommerce] = pCity->getAdditionalCommerceTimes100BySpecialist(eCommerce, eSpecialist, iChange);
			}
			bStarted = setResumableCommerceTimes100ChangeHelp(szHelpString, szStart, L": ", L"", aiCommerces, false, bStarted);

			// Great People
			int iGreatPeopleRate = pCity->getAdditionalGreatPeopleRateBySpecialist(eSpecialist, iChange);
			bStarted = setResumableValueChangeHelp(szHelpString, szStart, L": ", L"", iGreatPeopleRate, gDLL->getSymbolID(GREAT_PEOPLE_CHAR), false, false, bStarted);

			if (bStarted)
				szHelpString.append(L")" ENDCOLR);
		}
		// BUG - Specialist Actual Effects - end

		if (!CvWString(kSpecialist.getHelp()).empty() && !bCivilopediaText) {
			szHelpString.append(NEWLINE);
			szHelpString.append(kSpecialist.getHelp());
		}
	}
}

void CvGameTextMgr::parseFreeSpecialistHelp(CvWStringBuffer& szHelpString, const CvCity& kCity) {
	PROFILE_FUNC();

	for (int iLoopSpecialist = 0; iLoopSpecialist < GC.getNumSpecialistInfos(); iLoopSpecialist++) {
		SpecialistTypes eSpecialist = (SpecialistTypes)iLoopSpecialist;
		int iNumSpecialists = kCity.getFreeSpecialistCount(eSpecialist);

		if (iNumSpecialists > 0) {
			int aiYields[NUM_YIELD_TYPES];
			int aiCommerces[NUM_COMMERCE_TYPES];

			szHelpString.append(NEWLINE);
			szHelpString.append(CvWString::format(L"%s (%d): ", GC.getSpecialistInfo(eSpecialist).getDescription(), iNumSpecialists));

			for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI) {
				aiYields[iI] = iNumSpecialists * GET_PLAYER(kCity.getOwnerINLINE()).specialistYield(eSpecialist, ((YieldTypes)iI));
			}

			CvWStringBuffer szYield;
			setYieldChangeHelp(szYield, L"", L"", L"", aiYields, false, false);
			szHelpString.append(szYield);

			for (int iI = 0; iI < NUM_COMMERCE_TYPES; ++iI) {
				aiCommerces[iI] = iNumSpecialists * GET_PLAYER(kCity.getOwnerINLINE()).specialistCommerce(eSpecialist, ((CommerceTypes)iI));
			}

			CvWStringBuffer szCommerceString;
			setCommerceChangeHelp(szCommerceString, L"", L"", L"", aiCommerces, false, false);
			if (!szYield.isEmpty() && !szCommerceString.isEmpty()) {
				szHelpString.append(L", ");
			}
			szHelpString.append(szCommerceString);

			if (GC.getSpecialistInfo(eSpecialist).getExperience() > 0) {
				if (!szYield.isEmpty() || !szYield.isEmpty()) {
					szHelpString.append(L", ");
				}
				szHelpString.append(gDLL->getText("TXT_KEY_SPECIALIST_EXPERIENCE_SHORT", iNumSpecialists * GC.getSpecialistInfo(eSpecialist).getExperience()));
			}
		}
	}
}


//
// Promotion Help
//
void CvGameTextMgr::parsePromotionHelp(CvWStringBuffer& szBuffer, PromotionTypes ePromotion, const wchar* pcNewline) {
	PROFILE_FUNC();

	if (NO_PROMOTION == ePromotion) {
		return;
	}

	const CvPromotionInfo& kPromotion = GC.getPromotionInfo(ePromotion);

	if (kPromotion.getEnslaveCountChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_ENSLAVE_TEXT", kPromotion.getEnslaveCountChange()));
	}

	if (kPromotion.isBlitz()) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_BLITZ_TEXT"));
	}

	if (kPromotion.isAmphib()) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_AMPHIB_TEXT"));
	}

	if (kPromotion.isRiver()) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_RIVER_ATTACK_TEXT"));
	}

	if (kPromotion.isEnemyRoute()) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_ENEMY_ROADS_TEXT"));
	}

	if (kPromotion.isAlwaysHeal()) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_ALWAYS_HEAL_TEXT"));
	}

	if (kPromotion.isHillsDoubleMove()) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_HILLS_MOVE_TEXT"));
	}

	if (kPromotion.isCanMovePeaks()) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_CAN_MOVE_PEAKS_TEXT"));
	}

	if (kPromotion.isImmuneToFirstStrikes()) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_IMMUNE_FIRST_STRIKES_TEXT"));
	}

	if (kPromotion.isUnitRangeUnbound()) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_RANGE_UNBOUND"));
	}

	if (kPromotion.isUnitTerritoryUnbound()) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_TERRITORY_UNBOUND"));
	}

	for (TerrainTypes eTerrain = (TerrainTypes)0; eTerrain < GC.getNumTerrainInfos(); eTerrain = (TerrainTypes)(eTerrain + 1)) {
		if (kPromotion.getTerrainDoubleMove(eTerrain)) {
			szBuffer.append(pcNewline);
			szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_DOUBLE_MOVE_TEXT", GC.getTerrainInfo(eTerrain).getTextKeyWide()));
		}
	}

	for (FeatureTypes eFeature = (FeatureTypes)0; eFeature < GC.getNumFeatureInfos(); eFeature = (FeatureTypes)(eFeature + 1)) {
		if (kPromotion.getFeatureDoubleMove(eFeature)) {
			szBuffer.append(pcNewline);
			szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_DOUBLE_MOVE_TEXT", GC.getFeatureInfo(eFeature).getTextKeyWide()));
		}
	}

	if (kPromotion.getVisibilityChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_VISIBILITY_TEXT", kPromotion.getVisibilityChange()));
	}

	if (kPromotion.getMovesChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_MOVE_TEXT", kPromotion.getMovesChange()));
	}

	if (kPromotion.getMoveDiscountChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_MOVE_DISCOUNT_TEXT", -(kPromotion.getMoveDiscountChange())));
	}

	if (kPromotion.getUnitRangeChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_RANGE_CHANGE", kPromotion.getUnitRangeChange()));
	}

	if (kPromotion.getUnitRangePercentChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_RANGE_MODIFY", kPromotion.getUnitRangePercentChange()));
	}

	if (kPromotion.getAirRangeChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_AIR_RANGE_TEXT", kPromotion.getAirRangeChange()));
	}

	if (kPromotion.getInterceptChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_INTERCEPT_TEXT", kPromotion.getInterceptChange()));
	}

	if (kPromotion.getEvasionChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_EVASION_TEXT", kPromotion.getEvasionChange()));
	}

	if (kPromotion.getWithdrawalChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_WITHDRAWAL_TEXT", kPromotion.getWithdrawalChange()));
	}

	if (kPromotion.getCargoChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_CARGO_TEXT", kPromotion.getCargoChange()));
	}

	if (kPromotion.getCollateralDamageChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_COLLATERAL_DAMAGE_TEXT", kPromotion.getCollateralDamageChange()));
	}

	if (kPromotion.getBombardRateChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_BOMBARD_TEXT", kPromotion.getBombardRateChange()));
	}

	if (kPromotion.getFirstStrikesChange() != 0) {
		if (kPromotion.getFirstStrikesChange() == 1) {
			szBuffer.append(pcNewline);
			szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_FIRST_STRIKE_TEXT", kPromotion.getFirstStrikesChange()));
		} else {
			szBuffer.append(pcNewline);
			szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_FIRST_STRIKES_TEXT", kPromotion.getFirstStrikesChange()));
		}
	}

	if (kPromotion.getChanceFirstStrikesChange() != 0) {
		if (kPromotion.getChanceFirstStrikesChange() == 1) {
			szBuffer.append(pcNewline);
			szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_FIRST_STRIKE_CHANCE_TEXT", kPromotion.getChanceFirstStrikesChange()));
		} else {
			szBuffer.append(pcNewline);
			szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_FIRST_STRIKES_CHANCE_TEXT", kPromotion.getChanceFirstStrikesChange()));
		}
	}

	if (kPromotion.getEnemyHealChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_HEALS_EXTRA_TEXT", kPromotion.getEnemyHealChange()) + gDLL->getText("TXT_KEY_PROMOTION_ENEMY_LANDS_TEXT"));
	}

	if (kPromotion.getNeutralHealChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_HEALS_EXTRA_TEXT", kPromotion.getNeutralHealChange()) + gDLL->getText("TXT_KEY_PROMOTION_NEUTRAL_LANDS_TEXT"));
	}

	if (kPromotion.getFriendlyHealChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_HEALS_EXTRA_TEXT", kPromotion.getFriendlyHealChange()) + gDLL->getText("TXT_KEY_PROMOTION_FRIENDLY_LANDS_TEXT"));
	}

	if (kPromotion.getSameTileHealChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_HEALS_SAME_TEXT", kPromotion.getSameTileHealChange()) + gDLL->getText("TXT_KEY_PROMOTION_DAMAGE_TURN_TEXT"));
	}

	if (kPromotion.getAdjacentTileHealChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_HEALS_ADJACENT_TEXT", kPromotion.getAdjacentTileHealChange()) + gDLL->getText("TXT_KEY_PROMOTION_DAMAGE_TURN_TEXT"));
	}

	if (kPromotion.getCombatPercent() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_STRENGTH_TEXT", kPromotion.getCombatPercent()));
	}

	if (kPromotion.getCityAttackPercent() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_CITY_ATTACK_TEXT", kPromotion.getCityAttackPercent()));
	}

	if (kPromotion.getCityDefensePercent() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_CITY_DEFENSE_TEXT", kPromotion.getCityDefensePercent()));
	}

	if (kPromotion.getHillsAttackPercent() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_HILLS_ATTACK", kPromotion.getHillsAttackPercent()));
	}

	if (kPromotion.getHillsDefensePercent() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_HILLS_DEFENSE_TEXT", kPromotion.getHillsDefensePercent()));
	}

	if (kPromotion.getRevoltProtection() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_REVOLT_PROTECTION_TEXT", kPromotion.getRevoltProtection()));
	}

	if (kPromotion.getCollateralDamageProtection() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_COLLATERAL_PROTECTION_TEXT", kPromotion.getCollateralDamageProtection()));
	}

	if (kPromotion.getPillageChange() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_PILLAGE_CHANGE_TEXT", kPromotion.getPillageChange()));
	}

	if (kPromotion.getUpgradeDiscount() != 0) {
		if (100 == kPromotion.getUpgradeDiscount()) {
			szBuffer.append(pcNewline);
			szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_UPGRADE_DISCOUNT_FREE_TEXT"));
		} else {
			szBuffer.append(pcNewline);
			szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_UPGRADE_DISCOUNT_TEXT", kPromotion.getUpgradeDiscount()));
		}
	}

	if (kPromotion.getExperiencePercent() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_FASTER_EXPERIENCE_TEXT", kPromotion.getExperiencePercent()));
	}

	if (kPromotion.getKamikazePercent() != 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_KAMIKAZE_TEXT", kPromotion.getKamikazePercent()));
	}

	for (TerrainTypes eTerrain = (TerrainTypes)0; eTerrain < GC.getNumTerrainInfos(); eTerrain = (TerrainTypes)(eTerrain + 1)) {
		if (kPromotion.getTerrainAttackPercent(eTerrain) != 0) {
			szBuffer.append(pcNewline);
			szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_ATTACK_TEXT", kPromotion.getTerrainAttackPercent(eTerrain), GC.getTerrainInfo(eTerrain).getTextKeyWide()));
		}

		if (kPromotion.getTerrainDefensePercent(eTerrain) != 0) {
			szBuffer.append(pcNewline);
			szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_DEFENSE_TEXT", kPromotion.getTerrainDefensePercent(eTerrain), GC.getTerrainInfo(eTerrain).getTextKeyWide()));
		}
	}

	for (FeatureTypes eFeature = (FeatureTypes)0; eFeature < GC.getNumFeatureInfos(); eFeature = (FeatureTypes)(eFeature + 1)) {
		if (kPromotion.getFeatureAttackPercent(eFeature) != 0) {
			szBuffer.append(pcNewline);
			szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_ATTACK_TEXT", kPromotion.getFeatureAttackPercent(eFeature), GC.getFeatureInfo(eFeature).getTextKeyWide()));
		}

		if (kPromotion.getFeatureDefensePercent(eFeature) != 0) {
			szBuffer.append(pcNewline);
			szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_DEFENSE_TEXT", kPromotion.getFeatureDefensePercent(eFeature), GC.getFeatureInfo(eFeature).getTextKeyWide()));
		}
	}

	for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
		if (kPromotion.getUnitCombatModifierPercent(eUnitCombat) != 0) {
			szBuffer.append(pcNewline);
			szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_VERSUS_TEXT", kPromotion.getUnitCombatModifierPercent(eUnitCombat), GC.getUnitCombatInfo(eUnitCombat).getTextKeyWide()));
		}
	}

	for (DomainTypes eDomain = (DomainTypes)0; eDomain < NUM_DOMAIN_TYPES; eDomain = (DomainTypes)(eDomain + 1)) {
		if (kPromotion.getDomainModifierPercent(eDomain) != 0) {
			szBuffer.append(pcNewline);
			szBuffer.append(gDLL->getText("TXT_KEY_PROMOTION_VERSUS_TEXT", kPromotion.getDomainModifierPercent(eDomain), GC.getDomainInfo(eDomain).getTextKeyWide()));
		}
	}

	if (wcslen(kPromotion.getHelp()) > 0) {
		szBuffer.append(pcNewline);
		szBuffer.append(kPromotion.getHelp());
	}
}

//	Function:			parseCivicInfo()
//	Description:	Will parse the civic info help
//	Parameters:		szHelpText -- the text to put it into
//								civicInfo - what to parse
//	Returns:			nothing
void CvGameTextMgr::parseCivicInfo(CvWStringBuffer& szHelpText, CivicTypes eCivic, bool bCivilopediaText, bool bPlayerContext, bool bSkipName) {
	PROFILE_FUNC();

	if (NO_CIVIC == eCivic) {
		return;
	}

	const CvCivicInfo& kCivic = GC.getCivicInfo(eCivic);

	szHelpText.clear();

	FAssert(GC.getGameINLINE().getActivePlayer() != NO_PLAYER || !bPlayerContext);

	if (!bSkipName) {
		szHelpText.append(kCivic.getDescription());
	}

	if (!bCivilopediaText) {
		if (!bPlayerContext || !(GET_PLAYER(GC.getGameINLINE().getActivePlayer()).canDoCivics(eCivic))) {
			if (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isHasTech((TechTypes)(kCivic.getTechPrereq())))) {
				if (kCivic.getTechPrereq() != NO_TECH) {
					szHelpText.append(NEWLINE);
					szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_REQUIRES", GC.getTechInfo((TechTypes)kCivic.getTechPrereq()).getTextKeyWide()));
				}
			}
		}
	}

	// Special Building Not Required...
	for (SpecialBuildingTypes eSpecialBuilding = (SpecialBuildingTypes)0; eSpecialBuilding < GC.getNumSpecialBuildingInfos(); eSpecialBuilding = (SpecialBuildingTypes)(eSpecialBuilding + 1)) {
		if (kCivic.isSpecialBuildingNotRequired(eSpecialBuilding)) {
			// XXX "Missionaries"??? - Now in XML
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_BUILD_MISSIONARIES", GC.getSpecialBuildingInfo(eSpecialBuilding).getTextKeyWide()));
		}
	}

	// Valid Specialists...
	bool bFirst = true;

	for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
		if (kCivic.isSpecialistValid(eSpecialist)) {
			CvWString szFirstBuffer;
			szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_CIVIC_UNLIMTED").c_str());
			CvWString szSpecialist;
			szSpecialist.Format(L"<link=literal>%s</link>", GC.getSpecialistInfo(eSpecialist).getDescription());
			setListHelp(szHelpText, szFirstBuffer, szSpecialist, L", ", bFirst);
			bFirst = false;
		}
	}

	if (kCivic.isUnitRangeUnbound()) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_MISC_ENABLES_UNIT_RANGE_UNBOUND"));
	}

	if (kCivic.isUnitTerritoryUnbound()) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_MISC_ENABLES_UNIT_TERRITORY_UNBOUND"));
	}

	if (kCivic.getUnitRangeChange() != 0) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_MISC_UNIT_RANGE_CHANGE", kCivic.getUnitRangeChange()));
	}

	if (kCivic.getUnitRangePercentChange() != 0) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_MISC_UNIT_RANGE_MODIFY", kCivic.getUnitRangePercentChange()));
	}

	//	Great People Modifier...
	if (kCivic.getGreatPeopleRateModifier() != 0) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_GREAT_PEOPLE_MOD", kCivic.getGreatPeopleRateModifier()));
	}

	//	Great General Modifier...
	if (kCivic.getGreatGeneralRateModifier() != 0) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_GREAT_GENERAL_MOD", kCivic.getGreatGeneralRateModifier()));
	}

	if (kCivic.getDomesticGreatGeneralRateModifier() != 0) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_DOMESTIC_GREAT_GENERAL_MODIFIER", kCivic.getDomesticGreatGeneralRateModifier()));
	}

	//	State Religion Great People Modifier...
	if (kCivic.getStateReligionGreatPeopleRateModifier() != 0) {
		if (bPlayerContext && (GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getStateReligion() != NO_RELIGION)) {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_GREAT_PEOPLE_MOD_RELIGION", kCivic.getStateReligionGreatPeopleRateModifier(), GC.getReligionInfo(GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getStateReligion()).getChar()));
		} else {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_GREAT_PEOPLE_MOD_STATE_RELIGION", kCivic.getStateReligionGreatPeopleRateModifier(), gDLL->getSymbolID(RELIGION_CHAR)));
		}
	}

	// Star Signs ...
	CvWString szBuffer;
	if (kCivic.isEnableStarSigns()) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_STAR_SIGN_ENABLE"));
	}
	if (kCivic.getStarSignMitigateChangePercent() != 0) {
		szBuffer = kCivic.getStarSignMitigateChangePercent() > 0 ? "TXT_KEY_STAR_SIGN_MITIGATE_GOOD" : "TXT_KEY_STAR_SIGN_MITIGATE_BAD";
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText(szBuffer, abs(kCivic.getStarSignMitigateChangePercent())));
	}
	if (kCivic.getStarSignScaleChangePercent() != 0) {
		szBuffer = kCivic.getStarSignScaleChangePercent() > 0 ? "TXT_KEY_STAR_SIGN_SCALE_GOOD" : "TXT_KEY_STAR_SIGN_SCALE_BAD";
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText(szBuffer, abs(kCivic.getStarSignScaleChangePercent())));
	}

	//	Distance Maintenance Modifer...
	if (kCivic.getDistanceMaintenanceModifier() != 0) {
		if (kCivic.getDistanceMaintenanceModifier() <= -100) {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_DISTANCE_MAINT"));
		} else {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_DISTANCE_MAINT_MOD", kCivic.getDistanceMaintenanceModifier()));
		}
	}

	//	Num Cities Maintenance Modifer...
	if (kCivic.getNumCitiesMaintenanceModifier() != 0) {
		if (kCivic.getNumCitiesMaintenanceModifier() <= -100) {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_NO_MAINT_NUM_CITIES"));
		} else {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_NO_MAINT_NUM_CITIES_MOD", kCivic.getNumCitiesMaintenanceModifier()));
		}
	}

	//	Corporations Maintenance Modifer...
	if (kCivic.getCorporationMaintenanceModifier() != 0) {
		if (kCivic.getCorporationMaintenanceModifier() <= -100) {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_NO_MAINT_CORPORATION"));
		} else {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_NO_MAINT_CORPORATION_MOD", kCivic.getCorporationMaintenanceModifier()));
		}
	}

	//	Extra Health
	if (kCivic.getExtraHealth() != 0) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_EXTRA_HEALTH", abs(kCivic.getExtraHealth()), ((kCivic.getExtraHealth() > 0) ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR))));
	}

	//	Extra Happiness (new in K-Mod)
	if (kCivic.getExtraHappiness() != 0) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_EXTRA_HEALTH", abs(kCivic.getExtraHappiness()), kCivic.getExtraHappiness() > 0 ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR)));
		// note: TXT_KEY_CIVIC_EXTRA_HEALTH just says "[blah] in all cities", so it's ok for happiness as well as for health.
	}

	//	Free Experience
	if (kCivic.getFreeExperience() != 0) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_FREE_XP", kCivic.getFreeExperience()));
	}

	//	Worker speed modifier
	if (kCivic.getWorkerSpeedModifier() != 0) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_WORKER_SPEED", kCivic.getWorkerSpeedModifier()));
	}

	//	Improvement upgrade rate modifier
	if (kCivic.getImprovementUpgradeRateModifier() != 0) {
		bFirst = true;

		for (ImprovementTypes eImprovement = (ImprovementTypes)0; eImprovement < GC.getNumImprovementInfos(); eImprovement = (ImprovementTypes)(eImprovement + 1)) {
			if (GC.getImprovementInfo(eImprovement).getImprovementUpgrade() != NO_IMPROVEMENT) {
				CvWString szFirstBuffer;
				szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_CIVIC_IMPROVEMENT_UPGRADE", kCivic.getImprovementUpgradeRateModifier()).c_str());
				CvWString szImprovement;
				szImprovement.Format(L"<link=literal>%s</link>", GC.getImprovementInfo(eImprovement).getDescription());
				setListHelp(szHelpText, szFirstBuffer, szImprovement, L", ", bFirst);
				bFirst = false;
			}
		}
	}

	//	Military unit production modifier
	if (kCivic.getMilitaryProductionModifier() != 0) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_MILITARY_PRODUCTION", kCivic.getMilitaryProductionModifier()));
	}

	//	Free units population percent
	if ((kCivic.getBaseFreeUnits() != 0) || (kCivic.getFreeUnitsPopulationPercent() != 0)) {
		if (bPlayerContext) {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_FREE_UNITS", (kCivic.getBaseFreeUnits() + ((GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getTotalPopulation() * kCivic.getFreeUnitsPopulationPercent()) / 100))));
		} else {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_UNIT_SUPPORT"));
		}
	}

	//	Free military units population percent
	if ((kCivic.getBaseFreeMilitaryUnits() != 0) || (kCivic.getFreeMilitaryUnitsPopulationPercent() != 0)) {
		if (bPlayerContext) {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_FREE_MILITARY_UNITS", (kCivic.getBaseFreeMilitaryUnits() + ((GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getTotalPopulation() * kCivic.getFreeMilitaryUnitsPopulationPercent()) / 100))));
		} else {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_MILITARY_UNIT_SUPPORT"));
		}
	}

	//	Happiness per military unit
	if (kCivic.getHappyPerMilitaryUnit() != 0) {
		szHelpText.append(NEWLINE);
		// Use absolute value with unhappy face
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_UNIT_HAPPINESS", abs(kCivic.getHappyPerMilitaryUnit()), ((kCivic.getHappyPerMilitaryUnit() > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR))));
	}

	//	Military units produced with food
	if (kCivic.isMilitaryFoodProduction()) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_MILITARY_FOOD"));
	}

	//	Conscription
	if (getWorldSizeMaxConscript(eCivic) != 0) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_CONSCRIPTION", getWorldSizeMaxConscript(eCivic)));
	}

	//	Population Unhealthiness
	if (kCivic.getUnhealthyPopulationModifier() != 0) {
		// If the modifier is less than -100, display the old NoUnhealth. text
		// Note: this could be techinically inaccurate if we combine this modifier with a positive modifier
		if (kCivic.getUnhealthyPopulationModifier() <= -100) {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_NO_POP_UNHEALTHY"));
		} else {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_UNHEALTHY_POP_MODIFIER", kCivic.getUnhealthyPopulationModifier()));
		}
	}

	//	Building Unhealthiness
	if (kCivic.isBuildingOnlyHealthy()) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_NO_BUILDING_UNHEALTHY"));
	}

	//	Population Unhealthiness
	if (0 != kCivic.getExpInBorderModifier()) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_EXPERIENCE_IN_BORDERS", kCivic.getExpInBorderModifier()));
	}

	//	War Weariness
	if (kCivic.getWarWearinessModifier() != 0) {
		if (kCivic.getWarWearinessModifier() <= -100) {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_NO_WAR_WEARINESS"));
		} else {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_EXTRA_WAR_WEARINESS", kCivic.getWarWearinessModifier()));
		}
	}

	//	Free specialists
	if (kCivic.getFreeSpecialist() != 0) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_FREE_SPECIALISTS", kCivic.getFreeSpecialist()));
	}

	//	Trade routes
	if (kCivic.getTradeRoutes() != 0) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_TRADE_ROUTES", kCivic.getTradeRoutes()));
	}

	//	No Foreign Trade
	if (kCivic.isNoForeignTrade()) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_NO_FOREIGN_TRADE"));
	}

	//	No Corporations
	if (kCivic.isNoCorporations()) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_NO_CORPORATIONS"));
	}

	//	No Foreign Corporations
	if (kCivic.isNoForeignCorporations()) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_NO_FOREIGN_CORPORATIONS"));
	}

	//	Freedom Anger
	if (kCivic.getCivicPercentAnger() != 0) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_FREEDOM_ANGER", kCivic.getTextKeyWide()));
	}

	if (!(kCivic.isStateReligion())) {
		bool bFound = false;

		for (CivicTypes eCivic = (CivicTypes)0; eCivic < GC.getNumCivicInfos(); eCivic = (CivicTypes)(eCivic + 1)) {
			if ((GC.getCivicInfo(eCivic).getCivicOptionType() == kCivic.getCivicOptionType()) && (GC.getCivicInfo(eCivic).isStateReligion())) {
				bFound = true;
			}
		}

		if (bFound) {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_NO_STATE_RELIGION"));
		}
	}

	if (kCivic.getStateReligionHappiness() != 0) {
		if (bPlayerContext && (GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getStateReligion() != NO_RELIGION)) {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_STATE_RELIGION_HAPPINESS", abs(kCivic.getStateReligionHappiness()), ((kCivic.getStateReligionHappiness() > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR)), GC.getReligionInfo(GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getStateReligion()).getChar()));
		} else {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_RELIGION_HAPPINESS", abs(kCivic.getStateReligionHappiness()), ((kCivic.getStateReligionHappiness() > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR))));
		}
	}

	if (kCivic.getNonStateReligionHappiness() != 0) {
		if (!kCivic.isStateReligion()) {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_NON_STATE_REL_HAPPINESS_NO_STATE", abs(kCivic.getNonStateReligionHappiness()), ((kCivic.getNonStateReligionHappiness() > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR))));
		} else {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_NON_STATE_REL_HAPPINESS_WITH_STATE", abs(kCivic.getNonStateReligionHappiness()), ((kCivic.getNonStateReligionHappiness() > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR))));
		}
	}

	//	State Religion Unit Production Modifier
	if (kCivic.getStateReligionUnitProductionModifier() != 0) {
		if (bPlayerContext && (GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getStateReligion() != NO_RELIGION)) {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_REL_TRAIN_BONUS", GC.getReligionInfo(GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getStateReligion()).getChar(), kCivic.getStateReligionUnitProductionModifier()));
		} else {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_STATE_REL_TRAIN_BONUS", kCivic.getStateReligionUnitProductionModifier()));
		}
	}

	//	State Religion Building Production Modifier
	if (kCivic.getStateReligionBuildingProductionModifier() != 0) {
		if (bPlayerContext && (GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getStateReligion() != NO_RELIGION)) {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_REL_BUILDING_BONUS", GC.getReligionInfo(GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getStateReligion()).getChar(), kCivic.getStateReligionBuildingProductionModifier()));
		} else {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_STATE_REL_BUILDING_BONUS", kCivic.getStateReligionBuildingProductionModifier()));
		}
	}

	//	State Religion Free Experience
	if (kCivic.getStateReligionFreeExperience() != 0) {
		if (bPlayerContext && (GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getStateReligion() != NO_RELIGION)) {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_REL_FREE_XP", kCivic.getStateReligionFreeExperience(), GC.getReligionInfo(GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getStateReligion()).getChar()));
		} else {
			szHelpText.append(NEWLINE);
			szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_STATE_REL_FREE_XP", kCivic.getStateReligionFreeExperience()));
		}
	}

	if (kCivic.isNoNonStateReligionSpread()) {
		szHelpText.append(NEWLINE);
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_NO_NON_STATE_SPREAD"));
	}

	//	Yield Modifiers
	setYieldChangeHelp(szHelpText, L"", L"", gDLL->getText("TXT_KEY_CIVIC_IN_ALL_CITIES").GetCString(), kCivic.getYieldModifierArray(), true);

	//	Capital Yield Modifiers
	setYieldChangeHelp(szHelpText, L"", L"", gDLL->getText("TXT_KEY_CIVIC_IN_CAPITAL").GetCString(), kCivic.getCapitalYieldModifierArray(), true);

	//	Trade Yield Modifiers
	setYieldChangeHelp(szHelpText, L"", L"", gDLL->getText("TXT_KEY_CIVIC_FROM_TRADE_ROUTES").GetCString(), kCivic.getTradeYieldModifierArray(), true);

	//	Commerce Modifier
	setCommerceChangeHelp(szHelpText, L"", L"", gDLL->getText("TXT_KEY_CIVIC_IN_ALL_CITIES").GetCString(), kCivic.getCommerceModifierArray(), true);

	//	Capital Commerce Modifiers
	setCommerceChangeHelp(szHelpText, L"", L"", gDLL->getText("TXT_KEY_CIVIC_IN_CAPITAL").GetCString(), kCivic.getCapitalCommerceModifierArray(), true);

	//	Specialist Commerce
	setCommerceChangeHelp(szHelpText, L"", L"", gDLL->getText("TXT_KEY_CIVIC_PER_SPECIALIST").GetCString(), kCivic.getSpecialistExtraCommerceArray());

	//	Largest City Happiness
	if (kCivic.getLargestCityHappiness() != 0) {
		szHelpText.append(NEWLINE);
		// Use absolute value with unhappy face
		szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_LARGEST_CITIES_HAPPINESS", abs(kCivic.getLargestCityHappiness()), ((kCivic.getLargestCityHappiness() > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR)), GC.getWorldInfo(GC.getMapINLINE().getWorldSize()).getTargetNumCities()));
	}

	//	Improvement Yields
	for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1)) {
		int iLast = 0;

		for (ImprovementTypes eImprovement = (ImprovementTypes)0; eImprovement < GC.getNumImprovementInfos(); eImprovement = (ImprovementTypes)(eImprovement + 1)) {
			int iImprovementYieldChange = kCivic.getImprovementYieldChanges(eImprovement, eYield);
			if (iImprovementYieldChange != 0) {
				CvWString szFirstBuffer;
				szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_CIVIC_IMPROVEMENT_YIELD_CHANGE", iImprovementYieldChange, GC.getYieldInfo(eYield).getChar()).c_str());
				CvWString szImprovement;
				szImprovement.Format(L"<link=literal>%s</link>", GC.getImprovementInfo(eImprovement).getDescription());
				setListHelp(szHelpText, szFirstBuffer, szImprovement, L", ", (iImprovementYieldChange != iLast));
				iLast = iImprovementYieldChange;
			}
		}
	}

	//	Building Happiness
	for (BuildingClassTypes eBuildingClass = (BuildingClassTypes)0; eBuildingClass < GC.getNumBuildingClassInfos(); eBuildingClass = (BuildingClassTypes)(eBuildingClass + 1)) {
		int iBuildingHappinessChange = kCivic.getBuildingHappinessChanges(eBuildingClass);
		if (iBuildingHappinessChange != 0) {
			if (bPlayerContext && NO_PLAYER != GC.getGameINLINE().getActivePlayer()) {
				BuildingTypes eBuilding = (BuildingTypes)GC.getCivilizationInfo(GC.getGameINLINE().getActiveCivilizationType()).getCivilizationBuildings(eBuildingClass);
				if (NO_BUILDING != eBuilding) {
					szHelpText.append(NEWLINE);
					// Use absolute value with unhappy face
					szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_BUILDING_HAPPINESS", abs(iBuildingHappinessChange), ((iBuildingHappinessChange > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR)), GC.getBuildingInfo(eBuilding).getTextKeyWide()));
				}
			} else {
				szHelpText.append(NEWLINE);
				// Use absolute value with unhappy face
				szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_BUILDING_HAPPINESS", abs(iBuildingHappinessChange), ((iBuildingHappinessChange > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR)), GC.getBuildingClassInfo(eBuildingClass).getTextKeyWide()));
			}
		}

		int iBuildingHealthChange = kCivic.getBuildingHealthChanges(eBuildingClass);
		if (iBuildingHealthChange != 0) {
			if (bPlayerContext && NO_PLAYER != GC.getGameINLINE().getActivePlayer()) {
				BuildingTypes eBuilding = (BuildingTypes)GC.getCivilizationInfo(GC.getGameINLINE().getActiveCivilizationType()).getCivilizationBuildings(eBuildingClass);
				if (NO_BUILDING != eBuilding) {
					szHelpText.append(NEWLINE);
					// Use absolute value with unhealthy symbol
					szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_BUILDING_HAPPINESS", abs(iBuildingHealthChange), ((iBuildingHealthChange > 0) ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR)), GC.getBuildingInfo(eBuilding).getTextKeyWide()));
				}
			} else {
				szHelpText.append(NEWLINE);
				// Use absolute value with unhealthy symbol
				szHelpText.append(gDLL->getText("TXT_KEY_CIVIC_BUILDING_HAPPINESS", abs(iBuildingHealthChange), ((iBuildingHealthChange > 0) ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR)), GC.getBuildingClassInfo(eBuildingClass).getTextKeyWide()));
			}
		}
	}

	//	Feature Happiness
	int iLast = 0;
	for (FeatureTypes eFeature = (FeatureTypes)0; eFeature < GC.getNumFeatureInfos(); eFeature = (FeatureTypes)(eFeature + 1)) {
		int iFeatureHappinessChange = kCivic.getFeatureHappinessChanges(eFeature);
		if (iFeatureHappinessChange != 0) {
			// Use absolute value with unhappy face
			CvWString szFirstBuffer;
			szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_CIVIC_FEATURE_HAPPINESS", abs(iFeatureHappinessChange), ((iFeatureHappinessChange > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR))).c_str());
			CvWString szFeature;
			szFeature.Format(L"<link=literal>%s</link>", GC.getFeatureInfo(eFeature).getDescription());
			setListHelp(szHelpText, szFirstBuffer, szFeature, L", ", (iFeatureHappinessChange != iLast));
			iLast = iFeatureHappinessChange;
		}
	}

	//	Hurry types
	for (HurryTypes eHurry = (HurryTypes)0; eHurry < GC.getNumHurryInfos(); eHurry = (HurryTypes)(eHurry + 1)) {
		if (kCivic.isHurry(eHurry)) {
			szHelpText.append(CvWString::format(L"%s%c%s", NEWLINE, gDLL->getSymbolID(BULLET_CHAR), GC.getHurryInfo(eHurry).getDescription()));
		}
	}

	//	Gold cost per unit
	if (kCivic.getGoldPerUnit() != 0) {
		szHelpText.append(CvWString::format(L"\n%c%+.2f%c %s", gDLL->getSymbolID(BULLET_CHAR), (float)kCivic.getGoldPerUnit() / 100, GC.getCommerceInfo(COMMERCE_GOLD).getChar(),
			gDLL->getText("TXT_KEY_CIVIC_SUPPORT_COSTS").GetCString()));
	}

	//	Gold cost per military unit
	if (kCivic.getGoldPerMilitaryUnit() != 0) {
		szHelpText.append(CvWString::format(L"\n%c%+.2f%c %s", gDLL->getSymbolID(BULLET_CHAR), (float)kCivic.getGoldPerMilitaryUnit() / 100, GC.getCommerceInfo(COMMERCE_GOLD).getChar(),
			gDLL->getText("TXT_KEY_CIVIC_MILITARY_SUPPORT_COSTS").GetCString()));
	}

	BuildingTypes eLoopBuilding;
	bFirst = true;
	bool bNewPrereqAnd = false;
	bool bNewPrereqOr = false;
	bool bCurrPrereqAnd = false;
	bool bCurrPrereqOr = false;

	for (BuildingClassTypes eBuildingClass = (BuildingClassTypes)0; eBuildingClass < GC.getNumBuildingClassInfos(); eBuildingClass = (BuildingClassTypes)(eBuildingClass + 1)) {
		bNewPrereqAnd = false;
		bNewPrereqOr = false;
		if (bPlayerContext && NO_PLAYER != GC.getGameINLINE().getActivePlayer()) {
			eLoopBuilding = (BuildingTypes)GC.getCivilizationInfo(GC.getGameINLINE().getActiveCivilizationType()).getCivilizationBuildings(eBuildingClass);
		} else {
			eLoopBuilding = (BuildingTypes)GC.getBuildingClassInfo(eBuildingClass).getDefaultBuildingIndex();
		}
		if (eLoopBuilding != NO_BUILDING) {
			const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eLoopBuilding);
			for (int iI = 0; iI < kBuilding.getNumPrereqAndCivics(); iI++) {
				if (eCivic == (CivicTypes)kBuilding.getPrereqAndCivic(iI)) {
					bNewPrereqAnd = true;
					break;
				}
			}
			for (int iI = 0; !bNewPrereqAnd && iI < kBuilding.getNumPrereqOrCivics(); iI++) {
				if (eCivic == (CivicTypes)kBuilding.getPrereqOrCivic(iI)) {
					bNewPrereqOr = true;
					break;
				}
			}
			if (bNewPrereqAnd || bNewPrereqOr) {
				CvWString szBuilding;
				CvWString szFirstBuffer;
				szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_CIVIC_UNLOCKS_BUILDING").c_str());
				szBuilding.Format(L"<link=literal>%s</link>", kBuilding.getDescription());
				setListHelp(szHelpText, szFirstBuffer, szBuilding, L", ", bFirst);
				bFirst = false;
			}
		}
	}
	bFirst = true;
	if (NO_PLAYER != GC.getGameINLINE().getActivePlayer()) {
		const CvPlayer& kActivePlayer = GET_PLAYER(GC.getGameINLINE().getActivePlayer());
		if (!kActivePlayer.isCivic(eCivic)) {
			for (BuildingClassTypes eBuildingClass = (BuildingClassTypes)0; eBuildingClass < GC.getNumBuildingClassInfos(); eBuildingClass = (BuildingClassTypes)(eBuildingClass + 1)) {
				bCurrPrereqAnd = false;
				bCurrPrereqOr = false;
				eLoopBuilding = (BuildingTypes)GC.getCivilizationInfo(GC.getGameINLINE().getActiveCivilizationType()).getCivilizationBuildings(eBuildingClass);

				if (eLoopBuilding != NO_BUILDING) {
					const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eLoopBuilding);
					CivicTypes eCurCivic = kActivePlayer.getCivics((CivicOptionTypes)GC.getCivicInfo(eCivic).getCivicOptionType());
					for (int iI = 0; iI < kBuilding.getNumPrereqAndCivics(); iI++) {
						if (eCurCivic == (CivicTypes)kBuilding.getPrereqAndCivic(iI)) {
							bCurrPrereqAnd = true;
							break;
						}
					}
					for (int iI = 0; iI < kBuilding.getNumPrereqOrCivics(); iI++) {
						if (eCurCivic == (CivicTypes)kBuilding.getPrereqOrCivic(iI)) {
							bCurrPrereqOr = true;
							break;
						}
					}
					bool bObsolete = false;
					if (bCurrPrereqAnd) {
						bObsolete = true;
					} else if (bCurrPrereqOr && !bNewPrereqOr) {
						bObsolete = true;
						for (CivicTypes eLoopCivic = (CivicTypes)0; bObsolete && eLoopCivic < GC.getNumCivicInfos(); eLoopCivic = (CivicTypes)(eLoopCivic + 1)) {
							if (eLoopCivic != eCurCivic && eLoopCivic != eCivic) {
								for (int iI = 0; bObsolete && iI < kBuilding.getNumPrereqOrCivics(); iI++) {
									if (eLoopCivic == (CivicTypes)kBuilding.getPrereqOrCivic(iI) && kActivePlayer.isCivic(eLoopCivic)) {
										bObsolete = false;
									}
								}
							}
						}
					}
					if (bObsolete) {
						CvWString szBuilding;
						CvWString szFirstBuffer;
						szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_CIVIC_BLOCKS_BUILDING").c_str());
						szBuilding.Format(L"<link=literal>%s</link>", kBuilding.getDescription());
						setListHelp(szHelpText, szFirstBuffer, szBuilding, L", ", bFirst);
						bFirst = false;
					}
				}
			}
		}
	}

	bFirst = true;
	for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
		UnitTypes eLoopUnit;
		if (bPlayerContext && NO_PLAYER != GC.getGameINLINE().getActivePlayer()) {
			eLoopUnit = (UnitTypes)GC.getCivilizationInfo(GC.getGameINLINE().getActiveCivilizationType()).getCivilizationUnits(eUnitClass);
		} else {
			eLoopUnit = (UnitTypes)GC.getUnitClassInfo(eUnitClass).getDefaultUnitIndex();
		}
		if (eLoopUnit != NO_UNIT) {
			if ((GC.getUnitInfo(eLoopUnit).isPrereqOrCivic(eCivic) || GC.getUnitInfo(eLoopUnit).isPrereqAndCivic(eCivic))) {
				CvWString szUnit;
				CvWString szFirstBuffer;
				szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_CIVIC_UNLOCKS_UNIT").c_str());
				szUnit.Format(L"<link=literal>%s</link>", GC.getUnitInfo(eLoopUnit).getDescription());
				setListHelp(szHelpText, szFirstBuffer, szUnit, L", ", bFirst);
				bFirst = false;
			}
		}
	}
	bFirst = true;
	if (NO_PLAYER != GC.getGameINLINE().getActivePlayer()) {
		if (!GET_PLAYER(GC.getGameINLINE().getActivePlayer()).isCivic(eCivic)) {
			for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
				UnitTypes eLoopUnit = (UnitTypes)GC.getCivilizationInfo(GC.getGameINLINE().getActiveCivilizationType()).getCivilizationUnits(eUnitClass);

				if (eLoopUnit != NO_UNIT) {
					bool bObsolete = false;
					CivicTypes eCurCivic = GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getCivics((CivicOptionTypes)GC.getCivicInfo(eCivic).getCivicOptionType());
					if (GC.getUnitInfo(eLoopUnit).isPrereqAndCivic(eCurCivic)) {
						bObsolete = true;
					} else if (GC.getUnitInfo(eLoopUnit).isPrereqOrCivic(eCurCivic) && !GC.getUnitInfo(eLoopUnit).isPrereqOrCivic(eCivic)) {
						bObsolete = true;
						for (CivicTypes eLoopCivic = (CivicTypes)0; eLoopCivic < GC.getNumCivicInfos(); eLoopCivic = (CivicTypes)(eLoopCivic + 1)) {
							if (eLoopCivic != eCurCivic && eLoopCivic != eCivic) {
								if (GC.getUnitInfo(eLoopUnit).isPrereqOrCivic(eLoopCivic)) {
									if (GET_PLAYER(GC.getGameINLINE().getActivePlayer()).isCivic(eLoopCivic)) {
										bObsolete = false;
										break;
									}
								}
							}
						}
					}
					if (bObsolete) {
						CvWString szUnit;
						CvWString szFirstBuffer;
						szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_CIVIC_BLOCKS_UNIT").c_str());
						szUnit.Format(L"<link=literal>%s</link>", GC.getUnitInfo(eLoopUnit).getDescription());
						setListHelp(szHelpText, szFirstBuffer, szUnit, L", ", bFirst);
						bFirst = false;
					}
				}
			}
		}
	}

	if (!CvWString(kCivic.getHelp()).empty()) {
		szHelpText.append(CvWString::format(L"%s%s", NEWLINE, kCivic.getHelp()).c_str());
	}
}


void CvGameTextMgr::setTechHelp(CvWStringBuffer& szBuffer, TechTypes eTech, bool bCivilopediaText, bool bPlayerContext, bool bStrategyText, bool bTreeInfo, TechTypes eFromTech) {
	PROFILE_FUNC();

	// show debug info if cheat level > 0 and alt down
	bool bAlt = GC.altKey();
	if (bAlt && (gDLL->getChtLvl() > 0)) {
		szBuffer.clear();

		for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
			const CvPlayerAI& kPlayer = GET_PLAYER(ePlayer);
			const CvTeamAI& kTeam = GET_TEAM(kPlayer.getTeam());
			if (kPlayer.isAlive()) {
				CvWString szTempBuffer;
				szTempBuffer.Format(L"%s: ", kPlayer.getName());
				szBuffer.append(szTempBuffer);

				TechTypes ePlayerTech = kPlayer.getCurrentResearch();
				if (ePlayerTech == NO_TECH)
					szTempBuffer.Format(L"-\n");
				else
					szTempBuffer.Format(L"%s (%d->%dt)(%d/%d)\n", GC.getTechInfo(ePlayerTech).getDescription(), kPlayer.calculateResearchRate(ePlayerTech), kPlayer.getResearchTurnsLeft(ePlayerTech, true), kTeam.getResearchProgress(ePlayerTech), kTeam.getResearchCost(ePlayerTech));

				szBuffer.append(szTempBuffer);
			}
		}

		return;
	}


	if (NO_TECH == eTech) {
		return;
	}

	const CvTechInfo& kTech = GC.getTechInfo(eTech);

	//	Tech Name
	if (!bCivilopediaText && (!bTreeInfo || (NO_TECH == eFromTech))) {
		CvWString szTempBuffer;
		szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_TECH_TEXT"), kTech.getDescription());
		szBuffer.append(szTempBuffer);
	}

	PlayerTypes eActivePlayer = GC.getGameINLINE().getActivePlayer();
	FAssert(eActivePlayer != NO_PLAYER || !bPlayerContext);

	if (bTreeInfo && (NO_TECH != eFromTech)) {
		buildTechTreeString(szBuffer, eTech, bPlayerContext, eFromTech);
	}

	const CvPlayer& kActivePlayer = GET_PLAYER(eActivePlayer);
	//	Obsolete Buildings
	for (BuildingClassTypes eBuildingClass = (BuildingClassTypes)0; eBuildingClass < GC.getNumBuildingClassInfos(); eBuildingClass = (BuildingClassTypes)(eBuildingClass + 1)) {
		if (!bPlayerContext || (kActivePlayer.getBuildingClassCount(eBuildingClass) > 0)) {
			BuildingTypes eLoopBuilding;
			if (eActivePlayer != NO_PLAYER) {
				eLoopBuilding = (BuildingTypes)GC.getCivilizationInfo(GC.getGameINLINE().getActiveCivilizationType()).getCivilizationBuildings(eBuildingClass);
			} else {
				eLoopBuilding = (BuildingTypes)GC.getBuildingClassInfo(eBuildingClass).getDefaultBuildingIndex();
			}

			if (eLoopBuilding != NO_BUILDING) {
				//	Obsolete Buildings Check...
				if (GC.getBuildingInfo(eLoopBuilding).getObsoleteTech() == eTech) {
					buildObsoleteString(szBuffer, eLoopBuilding, true);
				}
			}
		}
	}

	// Obsolete Builds
	for (BuildTypes eBuild = (BuildTypes)0; eBuild < GC.getNumBuildInfos(); eBuild = (BuildTypes)(eBuild + 1)) {
		if (GC.getBuildInfo(eBuild).getObsoleteTech() == eTech) {
			buildObsoleteBuildString(szBuffer, eBuild, true);
		}
	}

	// Obsolete Units
	for (UnitClassTypes eUnitLoopClass = (UnitClassTypes)0; eUnitLoopClass < GC.getNumUnitClassInfos(); eUnitLoopClass = (UnitClassTypes)(eUnitLoopClass + 1)) {
		if (!bPlayerContext || (GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getUnitClassCount(eUnitLoopClass) > 0)) {
			UnitTypes eLoopUnit;
			if (GC.getGameINLINE().getActivePlayer() != NO_PLAYER) {
				eLoopUnit = (UnitTypes)GC.getCivilizationInfo(GC.getGameINLINE().getActiveCivilizationType()).getCivilizationUnits(eUnitLoopClass);
			} else {
				eLoopUnit = (UnitTypes)GC.getUnitClassInfo(eUnitLoopClass).getDefaultUnitIndex();
			}

			if (eLoopUnit != NO_UNIT) {
				//	Obsolete Units Check...
				if (GC.getUnitInfo(eLoopUnit).getObsoleteTech() == eTech) {
					buildObsoleteUnitString(szBuffer, eLoopUnit, true);
				}
			}
		}
	}

	//	Obsolete Bonuses
	for (BonusTypes eBonus = (BonusTypes)0; eBonus < GC.getNumBonusInfos(); eBonus = (BonusTypes)(eBonus + 1)) {
		if (GC.getBonusInfo(eBonus).getTechObsolete() == eTech) {
			buildObsoleteBonusString(szBuffer, eBonus, true);
		}
	}

	for (SpecialBuildingTypes eSpecialBuilding = (SpecialBuildingTypes)0; eSpecialBuilding < GC.getNumSpecialBuildingInfos(); eSpecialBuilding = (SpecialBuildingTypes)(eSpecialBuilding + 1)) {
		if (GC.getSpecialBuildingInfo(eSpecialBuilding).getObsoleteTech() == eTech) {
			buildObsoleteSpecialString(szBuffer, eSpecialBuilding, true);
		}
	}

	//	Route movement change...
	buildMoveString(szBuffer, eTech, true, bPlayerContext);

	//	Creates a free unit...
	buildFirstFreeUnitString(szBuffer, eTech, true, bPlayerContext);
	buildFreeUnitString(szBuffer, eTech, true, bPlayerContext);

	//	Allows units to ignore movement limits...
	buildUnitRangeUnboundString(szBuffer, eTech, true, bPlayerContext);

	//	Allows units to change range limits...
	buildUnitRangeChangeString(szBuffer, eTech, true, bPlayerContext);

	//	Allows units to change range limits...
	buildUnitRangePercentageChangeString(szBuffer, eTech, true, bPlayerContext);

	//	Allows units to range outside the civ territory...
	buildUnitTerritoryUnboundString(szBuffer, eTech, true, bPlayerContext);

	//	Increases feature production...
	buildFeatureProductionString(szBuffer, eTech, true, bPlayerContext);

	//	Increases worker build rate...
	buildWorkerRateString(szBuffer, eTech, true, bPlayerContext);

	//	Trade Routed per city change...
	buildTradeRouteString(szBuffer, eTech, true, bPlayerContext);

	//	Health increase...
	buildHealthRateString(szBuffer, eTech, true, bPlayerContext);

	//	Happiness increase...
	buildHappinessRateString(szBuffer, eTech, true, bPlayerContext);

	//	Free Techs...
	buildFreeTechString(szBuffer, eTech, true, bPlayerContext);

	//	Line of Sight Bonus across water...
	buildLOSString(szBuffer, eTech, true, bPlayerContext);

	//	Centers world map...
	buildMapCenterString(szBuffer, eTech, true, bPlayerContext);

	//	Reveals World Map...
	buildMapRevealString(szBuffer, eTech, true);

	//	Enables map trading...
	buildMapTradeString(szBuffer, eTech, true, bPlayerContext);

	//	Enables tech trading...
	buildTechTradeString(szBuffer, eTech, true, bPlayerContext);

	//	Enables gold trading...
	buildGoldTradeString(szBuffer, eTech, true, bPlayerContext);

	//	Enables open borders...
	buildOpenBordersString(szBuffer, eTech, true, bPlayerContext);

	//	Enables defensive pacts...
	buildDefensivePactString(szBuffer, eTech, true, bPlayerContext);

	//	Enables permanent alliances...
	buildPermanentAllianceString(szBuffer, eTech, true, bPlayerContext);

	//	Enables Embassies...
	buildEmbassyString(szBuffer, eTech, true, bPlayerContext);

	//	Enables Limited Borders...
	buildLimitedBordersString(szBuffer, eTech, true, bPlayerContext);

	//	Enables Free Trade...
	buildFreeTradeAgreementString(szBuffer, eTech, true, bPlayerContext);

	//	Enables Non Aggression Pact...
	buildNonAggressionString(szBuffer, eTech, true, bPlayerContext);

	//	Peak passability...
	buildCanPassPeaksString(szBuffer, eTech, true, bPlayerContext);

	//	If it Removes Movement slowdown...
	buildMoveFastPeaksString(szBuffer, eTech, true, bPlayerContext);

	//	Can found cities on peak...
	buildCanFoundOnPeaksString(szBuffer, eTech, true, bPlayerContext);

	//	Enables bridge building...
	buildBridgeString(szBuffer, eTech, true, bPlayerContext);

	//	Can spread irrigation...
	buildIrrigationString(szBuffer, eTech, true, bPlayerContext);

	//	Ignore irrigation...
	buildIgnoreIrrigationString(szBuffer, eTech, true, bPlayerContext);

	//	Coastal work...
	buildWaterWorkString(szBuffer, eTech, true, bPlayerContext);

	//	Enables vassal state...
	buildVassalStateString(szBuffer, eTech, true, bPlayerContext);

	// Worldview revolt chance...
	buildWorldViewRevoltTurnChangeString(szBuffer, eTech, true, bPlayerContext);

	//	Build farm, irrigation, etc...
	for (BuildTypes eBuild = (BuildTypes)0; eBuild < GC.getNumBuildInfos(); eBuild = (BuildTypes)(eBuild + 1)) {
		buildImprovementString(szBuffer, eTech, eBuild, true, bPlayerContext);
	}

	//	Extra moves for certain domains...
	for (DomainTypes eDomain = (DomainTypes)0; eDomain < NUM_DOMAIN_TYPES; eDomain = (DomainTypes)(eDomain + 1)) {
		buildDomainExtraMovesString(szBuffer, eTech, eDomain, true, bPlayerContext);
	}

	//	Adjusting culture, science, etc
	for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
		buildAdjustString(szBuffer, eTech, eCommerce, true, bPlayerContext);
	}

	//	Enabling trade routes on water...?
	for (TerrainTypes eTerrain = (TerrainTypes)0; eTerrain < GC.getNumTerrainInfos(); eTerrain = (TerrainTypes)(eTerrain + 1)) {
		buildTerrainTradeString(szBuffer, eTech, eTerrain, true, bPlayerContext);
	}

	buildRiverTradeString(szBuffer, eTech, true, bPlayerContext);

	//	Special Buildings
	for (SpecialBuildingTypes eSpecialBuilding = (SpecialBuildingTypes)0; eSpecialBuilding < GC.getNumSpecialBuildingInfos(); eSpecialBuilding = (SpecialBuildingTypes)(eSpecialBuilding + 1)) {
		buildSpecialBuildingString(szBuffer, eTech, eSpecialBuilding, true, bPlayerContext);
	}

	//	Build farm, mine, etc...
	for (ImprovementTypes eImprovement = (ImprovementTypes)0; eImprovement < GC.getNumImprovementInfos(); eImprovement = (ImprovementTypes)(eImprovement + 1)) {
		buildYieldChangeString(szBuffer, eTech, eImprovement, true, bPlayerContext);
	}

	setYieldChangeHelp(szBuffer, gDLL->getText("TXT_KEY_TECH_FOREST_PLOTS").c_str(), L": ", L"", kTech.getForestPlotYieldChangeArray());
	setYieldChangeHelp(szBuffer, gDLL->getText("TXT_KEY_TECH_RIVER_PLOTS").c_str(), L": ", L"", kTech.getRiverPlotYieldChangeArray());
	setYieldChangeHelp(szBuffer, gDLL->getText("TXT_KEY_TECH_SEA_PLOTS").c_str(), L": ", L"", kTech.getSeaPlotYieldChangeArray());

	if (GC.getTechInfo(eTech).isCaptureCities()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_TECH_ALLOW_CITY_CAPTURE"));
	}

	bool bFirst = true;
	for (BonusTypes eBonus = (BonusTypes)0; eBonus < GC.getNumBonusInfos(); eBonus = (BonusTypes)(eBonus + 1)) {
		bFirst = buildBonusRevealString(szBuffer, eTech, eBonus, bFirst, true, bPlayerContext);
	}

	bFirst = true;
	for (CivicTypes eCivic = (CivicTypes)0; eCivic < GC.getNumCivicInfos(); eCivic = (CivicTypes)(eCivic + 1)) {
		bFirst = buildCivicRevealString(szBuffer, eTech, eCivic, bFirst, true, bPlayerContext);
	}

	if (!bCivilopediaText) {
		bFirst = true;
		for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
			if (!bPlayerContext || !kActivePlayer.isProductionMaxedUnitClass(eUnitClass)) {
				UnitTypes eLoopUnit;
				if (eActivePlayer != NO_PLAYER) {
					eLoopUnit = (UnitTypes)GC.getCivilizationInfo(GC.getGameINLINE().getActiveCivilizationType()).getCivilizationUnits(eUnitClass);
				} else {
					eLoopUnit = (UnitTypes)GC.getUnitClassInfo(eUnitClass).getDefaultUnitIndex();
				}

				if (eLoopUnit != NO_UNIT) {
					const CvUnitInfo& kUnit = GC.getUnitInfo(eLoopUnit);
					if (!bPlayerContext || !(kActivePlayer.canTrain(eLoopUnit))) {
						CvWString szFirstBuffer;
						CvWString szTempBuffer;
						for (int iJ = 0; iJ < kUnit.getNumPrereqAndTechs(); iJ++) {
							if (kUnit.getPrereqAndTech(iJ) == eTech) {
								szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_TECH_CAN_TRAIN").c_str());
								szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_UNIT_TEXT"), kUnit.getDescription());
								setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", bFirst);
								bFirst = false;
								break;
							}
						}
					}
				}
			}
		}

		bFirst = true;
		for (BuildingClassTypes eBuildingClass = (BuildingClassTypes)0; eBuildingClass < GC.getNumBuildingClassInfos(); eBuildingClass = (BuildingClassTypes)(eBuildingClass + 1)) {
			if (!bPlayerContext || !kActivePlayer.isProductionMaxedBuildingClass(eBuildingClass)) {
				BuildingTypes eLoopBuilding;
				if (eActivePlayer != NO_PLAYER) {
					eLoopBuilding = (BuildingTypes)GC.getCivilizationInfo(GC.getGameINLINE().getActiveCivilizationType()).getCivilizationBuildings(eBuildingClass);
				} else {
					eLoopBuilding = (BuildingTypes)GC.getBuildingClassInfo(eBuildingClass).getDefaultBuildingIndex();
				}

				if (eLoopBuilding != NO_BUILDING) {
					if (!bPlayerContext || !(kActivePlayer.canConstruct(eLoopBuilding, false, true))) {
						CvWString szFirstBuffer;
						CvWString szTempBuffer;
						for (int iI = 0; iI < GC.getBuildingInfo(eLoopBuilding).getNumPrereqAndTechs(); iI++) {
							if (GC.getBuildingInfo(eLoopBuilding).getPrereqAndTech(iI) == eTech) {
								szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_TECH_CAN_CONSTRUCT").c_str());
								szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eLoopBuilding).getDescription());
								setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", bFirst);
								bFirst = false;
							}
						}
					}
				}
			}
		}

		bFirst = true;
		for (ProjectTypes eProject = (ProjectTypes)0; eProject < GC.getNumProjectInfos(); eProject = (ProjectTypes)(eProject + 1)) {
			if (!bPlayerContext || !kActivePlayer.isProductionMaxedProject(eProject)) {
				if (!bPlayerContext || !kActivePlayer.canCreate(eProject, false, true)) {
					if (GC.getProjectInfo(eProject).getTechPrereq() == eTech) {
						CvWString szFirstBuffer;
						CvWString szTempBuffer;
						szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_TECH_CAN_CREATE").c_str());
						szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_PROJECT_TEXT"), GC.getProjectInfo(eProject).getDescription());
						setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", bFirst);
						bFirst = false;
					}
				}
			}
		}
	}

	bFirst = true;
	for (ProcessTypes eProcess = (ProcessTypes)0; eProcess < GC.getNumProcessInfos(); eProcess = (ProcessTypes)(eProcess + 1)) {
		bFirst = buildProcessInfoString(szBuffer, eTech, eProcess, bFirst, true, bPlayerContext);
	}

	bFirst = true;
	for (ReligionTypes eReligion = (ReligionTypes)0; eReligion < GC.getNumReligionInfos(); eReligion = (ReligionTypes)(eReligion + 1)) {
		if (!bPlayerContext || !GC.getGameINLINE().isReligionSlotTaken(eReligion)) {
			bFirst = buildFoundReligionString(szBuffer, eTech, eReligion, bFirst, true, bPlayerContext);
		}
	}

	bFirst = true;
	for (CorporationTypes eCorporation = (CorporationTypes)0; eCorporation < GC.getNumCorporationInfos(); eCorporation = (CorporationTypes)(eCorporation + 1)) {
		if (!bPlayerContext || !GC.getGameINLINE().isCorporationFounded(eCorporation)) {
			bFirst = buildFoundCorporationString(szBuffer, eTech, eCorporation, bFirst, true, bPlayerContext);
		}
	}

	bFirst = true;
	for (PromotionTypes ePromotion = (PromotionTypes)0; ePromotion < GC.getNumPromotionInfos(); ePromotion = (PromotionTypes)(ePromotion + 1)) {
		bFirst = buildPromotionString(szBuffer, eTech, ePromotion, bFirst, true, bPlayerContext);
	}

	if (bTreeInfo && NO_TECH == eFromTech) {
		buildSingleLineTechTreeString(szBuffer, eTech, bPlayerContext);
	}

	if (!CvWString(kTech.getHelp()).empty()) {
		szBuffer.append(CvWString::format(L"%s%s", NEWLINE, kTech.getHelp()).c_str());
	}

	if (!bCivilopediaText) {
		CvWString szTempBuffer;
		if (eActivePlayer == NO_PLAYER) {
			szTempBuffer.Format(L"\n%d%c", kTech.getResearchCost(), GC.getCommerceInfo(COMMERCE_RESEARCH).getChar());
			szBuffer.append(szTempBuffer);
		} else if (GET_TEAM(GC.getGameINLINE().getActiveTeam()).isHasTech(eTech)) {
			szTempBuffer.Format(L"\n%d%c", GET_TEAM(GC.getGameINLINE().getActiveTeam()).getResearchCost(eTech), GC.getCommerceInfo(COMMERCE_RESEARCH).getChar());
			szBuffer.append(szTempBuffer);
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_TECH_NUM_TURNS", kActivePlayer.getResearchTurnsLeft(eTech, (GC.ctrlKey() || !(GC.shiftKey())))));

			szTempBuffer.Format(L" (%d/%d %c)", GET_TEAM(GC.getGameINLINE().getActiveTeam()).getResearchProgress(eTech), GET_TEAM(GC.getGameINLINE().getActiveTeam()).getResearchCost(eTech), GC.getCommerceInfo(COMMERCE_RESEARCH).getChar());
			szBuffer.append(szTempBuffer);
		}
	}

	if (eActivePlayer != NO_PLAYER) {
		if (kActivePlayer.canResearch(eTech)) {
			for (int iI = 0; iI < GC.getNumUnitInfos(); ++iI) {
				CvUnitInfo& kUnit = GC.getUnitInfo((UnitTypes)iI);

				if (kUnit.getBaseDiscover() > 0 || kUnit.getDiscoverMultiplier() > 0) {
					if (::getDiscoveryTech((UnitTypes)iI, eActivePlayer) == eTech) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_TECH_GREAT_PERSON_DISCOVER", kUnit.getTextKeyWide()));
					}
				}
			}

			if (kActivePlayer.getCurrentEra() < kTech.getEra()) {
				// Check if there is a change in the max number of cities allowed based on the Era
				int iNextEraMaxCities = GC.getEraInfo((EraTypes)GC.getTechInfo(eTech).getEra()).getMaxCities();
				if (iNextEraMaxCities != GC.getEraInfo(kActivePlayer.getCurrentEra()).getMaxCities()) {
					if (iNextEraMaxCities > 0) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_ERA_CITY_RESTRICTION", iNextEraMaxCities));
					} else if (iNextEraMaxCities == -1) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_ERA_NO_CITY_RESTRICTION"));
					}
				}
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_TECH_ERA_ADVANCE", GC.getEraInfo((EraTypes)kTech.getEra()).getTextKeyWide()));
			}
		}
	}

	if (bStrategyText) {
		if (!CvWString(kTech.getStrategy()).empty()) {
			if ((eActivePlayer == NO_PLAYER) || kActivePlayer.isOption(PLAYEROPTION_ADVISOR_HELP)) {
				szBuffer.append(SEPARATOR);
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_SIDS_TIPS"));
				szBuffer.append(L'\"');
				szBuffer.append(kTech.getStrategy());
				szBuffer.append(L'\"');
			}
		}
	}
}


void CvGameTextMgr::setBasicUnitHelp(CvWStringBuffer& szBuffer, UnitTypes eUnit, bool bCivilopediaText) {
	PROFILE_FUNC();

	CvWString szTempBuffer;

	if (NO_UNIT == eUnit) {
		return;
	}

	const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);

	if (!bCivilopediaText) {
		szBuffer.append(NEWLINE);

		if (kUnit.getDomainType() == DOMAIN_AIR) {
			if (kUnit.getAirCombat() > 0) {
				szTempBuffer.Format(L"%d%c, ", kUnit.getAirCombat(), gDLL->getSymbolID(STRENGTH_CHAR));
				szBuffer.append(szTempBuffer);
			}
		} else {
			if (kUnit.getCombat() > 0) {
				szTempBuffer.Format(L"%d%c, ", kUnit.getCombat(), gDLL->getSymbolID(STRENGTH_CHAR));
				szBuffer.append(szTempBuffer);
			}
		}

		szTempBuffer.Format(L"%d%c", kUnit.getMoves(), gDLL->getSymbolID(MOVES_CHAR));
		szBuffer.append(szTempBuffer);

		if (kUnit.getAirRange() > 0) {
			szBuffer.append(L", ");
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_AIR_RANGE", kUnit.getAirRange()));
		}
	}

	if (kUnit.isGoldenAge()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_GOLDEN_AGE"));
	}

	if (kUnit.getLeaderExperience() > 0) {
		if (0 == GC.getDefineINT("WARLORD_EXTRA_EXPERIENCE_PER_UNIT_PERCENT")) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_LEADER", kUnit.getLeaderExperience()));
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_LEADER_EXPERIENCE", kUnit.getLeaderExperience()));
		}
	}

	if (NO_PROMOTION != kUnit.getLeaderPromotion()) {
		szBuffer.append(CvWString::format(L"%s%c%s", NEWLINE, gDLL->getSymbolID(BULLET_CHAR), gDLL->getText("TXT_KEY_PROMOTION_WHEN_LEADING").GetCString()));
		parsePromotionHelp(szBuffer, (PromotionTypes)kUnit.getLeaderPromotion(), L"\n   ");
	}

	if ((kUnit.getBaseDiscover() > 0) || (kUnit.getDiscoverMultiplier() > 0)) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_DISCOVER_TECH"));
	}

	if ((kUnit.getBaseHurry() > 0) || (kUnit.getHurryMultiplier() > 0)) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_HURRY_PRODUCTION"));
	}

	if ((kUnit.getBaseTrade() > 0) || (kUnit.getTradeMultiplier() > 0)) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_TRADE_MISSION"));
	}

	if (kUnit.getEnslaveCount() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_ENSLAVER_COUNT", kUnit.getEnslaveCount()));
	}

	if (kUnit.isSlave()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_TYPE_SLAVE"));
	}

	if (kUnit.getGreatWorkCulture() > 0) {
		int iCulture = kUnit.getGreatWorkCulture();
		if (NO_GAMESPEED != GC.getGameINLINE().getGameSpeedType()) {
			iCulture *= GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getUnitGreatWorkPercent();
			iCulture /= 100;
		}

		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_GREAT_WORK", iCulture));
	}

	if (kUnit.getEspionagePoints() > 0) {
		int iEspionage = kUnit.getEspionagePoints();
		if (NO_GAMESPEED != GC.getGameINLINE().getGameSpeedType()) {
			iEspionage *= GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getUnitGreatWorkPercent();
			iEspionage /= 100;
		}

		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_ESPIONAGE_MISSION", iEspionage));
	}

	bool bFirst = true;
	for (ReligionTypes eReligion = (ReligionTypes)0; eReligion < GC.getNumReligionInfos(); eReligion = (ReligionTypes)(eReligion + 1)) {
		if (kUnit.getReligionSpreads(eReligion) > 0) {
			szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_UNIT_CAN_SPREAD").c_str());
			CvWString szReligion;
			szReligion.Format(L"<link=literal>%s</link>", GC.getReligionInfo(eReligion).getDescription());
			setListHelp(szBuffer, szTempBuffer, szReligion, L", ", bFirst);
			bFirst = false;
		}
	}

	bFirst = true;
	for (CorporationTypes eCorporation = (CorporationTypes)0; eCorporation < GC.getNumCorporationInfos(); eCorporation = (CorporationTypes)(eCorporation + 1)) {
		if (kUnit.getCorporationSpreads(eCorporation) > 0) {
			szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_UNIT_CAN_EXPAND").c_str());
			CvWString szCorporation;
			szCorporation.Format(L"<link=literal>%s</link>", GC.getCorporationInfo(eCorporation).getDescription());
			setListHelp(szBuffer, szTempBuffer, szCorporation, L", ", bFirst);
			bFirst = false;
		}
	}

	bFirst = true;
	for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
		if (kUnit.getGreatPeoples(eSpecialist)) {
			szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_UNIT_CAN_JOIN").c_str());
			CvWString szSpecialistLink = CvWString::format(L"<link=literal>%s</link>", GC.getSpecialistInfo(eSpecialist).getDescription());
			setListHelp(szBuffer, szTempBuffer, szSpecialistLink.GetCString(), L", ", bFirst);
			bFirst = false;
		}
	}

	for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
		if (kUnit.getSlaveSpecialistType() == eSpecialist && kUnit.isSlave()) {
			szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_UNIT_CAN_JOIN").c_str());
			CvWString szSpecialistLink = CvWString::format(L"<link=literal>%s</link>", GC.getSpecialistInfo(eSpecialist).getDescription());
			setListHelp(szBuffer, szTempBuffer, szSpecialistLink.GetCString(), L", ", true);
			break;
		}
	}

	bFirst = true;
	for (BuildingTypes eBuilding = (BuildingTypes)0; eBuilding < GC.getNumBuildingInfos(); eBuilding = (BuildingTypes)(eBuilding + 1)) {
		if (kUnit.getBuildings(eBuilding) || kUnit.getForceBuildings(eBuilding)) {
			szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_UNIT_CAN_CONSTRUCT").c_str());
			CvWString szBuildingLink = CvWString::format(L"<link=literal>%s</link>", GC.getBuildingInfo(eBuilding).getDescription());
			setListHelp(szBuffer, szTempBuffer, szBuildingLink.GetCString(), L", ", bFirst);
			bFirst = false;
		}
	}

	if (kUnit.getCargoSpace() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_CARGO_SPACE", kUnit.getCargoSpace()));

		if (kUnit.getSpecialCargo() != NO_SPECIALUNIT) {
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_CARRIES", GC.getSpecialUnitInfo((SpecialUnitTypes)kUnit.getSpecialCargo()).getTextKeyWide()));
		}
	}

	szTempBuffer.Format(L"%s%s ", NEWLINE, gDLL->getText("TXT_KEY_UNIT_CANNOT_ENTER").GetCString());

	bFirst = true;
	for (TerrainTypes eTerrain = (TerrainTypes)0; eTerrain < GC.getNumTerrainInfos(); eTerrain = (TerrainTypes)(eTerrain + 1)) {
		if (kUnit.getTerrainImpassable(eTerrain)) {
			CvWString szTerrain;
			TechTypes eTech = (TechTypes)kUnit.getTerrainPassableTech(eTerrain);
			if (NO_TECH == eTech) {
				szTerrain.Format(L"<link=literal>%s</link>", GC.getTerrainInfo(eTerrain).getDescription());
			} else {
				szTerrain = gDLL->getText("TXT_KEY_TERRAIN_UNTIL_TECH", GC.getTerrainInfo(eTerrain).getTextKeyWide(), GC.getTechInfo(eTech).getTextKeyWide());
			}
			setListHelp(szBuffer, szTempBuffer, szTerrain, L", ", bFirst);
			bFirst = false;
		}
	}

	for (FeatureTypes eFeature = (FeatureTypes)0; eFeature < GC.getNumFeatureInfos(); eFeature = (FeatureTypes)(eFeature + 1)) {
		if (kUnit.getFeatureImpassable(eFeature)) {
			CvWString szFeature;
			TechTypes eTech = (TechTypes)kUnit.getTerrainPassableTech(eFeature);
			if (NO_TECH == eTech) {
				szFeature.Format(L"<link=literal>%s</link>", GC.getFeatureInfo(eFeature).getDescription());
			} else {
				szFeature = gDLL->getText("TXT_KEY_TERRAIN_UNTIL_TECH", GC.getFeatureInfo(eFeature).getTextKeyWide(), GC.getTechInfo(eTech).getTextKeyWide());
			}
			setListHelp(szBuffer, szTempBuffer, szFeature, L", ", bFirst);
			bFirst = false;
		}
	}

	if (kUnit.isInvisible()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_INVISIBLE_ALL"));
	} else if (kUnit.getInvisibleType() != NO_INVISIBLE) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_INVISIBLE_MOST"));
	}

	for (int iInvisible = 0; iInvisible < kUnit.getNumSeeInvisibleTypes(); ++iInvisible) {
		if (bCivilopediaText || (kUnit.getSeeInvisibleType(iInvisible) != kUnit.getInvisibleType())) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_SEE_INVISIBLE", GC.getInvisibleInfo((InvisibleTypes)kUnit.getSeeInvisibleType(iInvisible)).getTextKeyWide()));
		}
	}

	if (kUnit.isCanMoveImpassable()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_CAN_MOVE_IMPASSABLE"));
	}


	if (kUnit.isNoBadGoodies()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_NO_BAD_GOODIES"));
	}

	if (kUnit.isHiddenNationality()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_HIDDEN_NATIONALITY"));
	}

	if (kUnit.isAlwaysHostile()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_ALWAYS_HOSTILE"));
	}

	if (GC.getUnitInfo(eUnit).isMilitaryTrade() || GC.getUnitInfo(eUnit).isWorkerTrade()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_TRADABLE_UNIT"));
	}

	if (kUnit.isOnlyDefensive()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_ONLY_DEFENSIVE"));
	}

	if (kUnit.isNoCapture()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_CANNOT_CAPTURE"));
	}

	if (kUnit.isRivalTerritory()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_EXPLORE_RIVAL"));
	}

	if (kUnit.isFound()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_FOUND_CITY"));
	}

	if (kUnit.isSingleBuild()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_SINGLE_BUILD"));
	}

	int iCount = 0;
	for (BuildTypes eBuild = (BuildTypes)0; eBuild < GC.getNumBuildInfos(); eBuild = (BuildTypes)(eBuild + 1)) {
		if (kUnit.getBuilds(eBuild)) {
			iCount++;
		}
	}

	if (iCount > ((GC.getNumBuildInfos() * 3) / 4)) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_IMPROVE_PLOTS"));
	} else {
		bFirst = true;
		for (BuildTypes eBuild = (BuildTypes)0; eBuild < GC.getNumBuildInfos(); eBuild = (BuildTypes)(eBuild + 1)) {
			if (kUnit.getBuilds(eBuild)) {
				szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_UNIT_CAN").c_str());
				setListHelp(szBuffer, szTempBuffer, GC.getBuildInfo(eBuild).getDescription(), L", ", bFirst);
				bFirst = false;
			}
		}
	}

	if (kUnit.getNukeRange() != -1) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_CAN_NUKE"));
	}

	if (kUnit.isCounterSpy()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_EXPOSE_SPIES"));
	}

	if ((kUnit.getFirstStrikes() + kUnit.getChanceFirstStrikes()) > 0) {
		if (kUnit.getChanceFirstStrikes() == 0) {
			if (kUnit.getFirstStrikes() == 1) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_UNIT_ONE_FIRST_STRIKE"));
			} else {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_UNIT_NUM_FIRST_STRIKES", kUnit.getFirstStrikes()));
			}
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_FIRST_STRIKE_CHANCES", kUnit.getFirstStrikes(), kUnit.getFirstStrikes() + kUnit.getChanceFirstStrikes()));
		}
	}

	if (kUnit.isFirstStrikeImmune()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_IMMUNE_FIRST_STRIKES"));
	}

	if (kUnit.isNoDefensiveBonus()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_NO_DEFENSE_BONUSES"));
	}

	if (kUnit.isFlatMovementCost()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_FLAT_MOVEMENT"));
	}

	if (kUnit.isIgnoreTerrainCost()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_IGNORE_TERRAIN"));
	}

	if (kUnit.getInterceptionProbability() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_INTERCEPT_AIRCRAFT", kUnit.getInterceptionProbability()));
	}

	if (kUnit.getEvasionProbability() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_EVADE_INTERCEPTION", kUnit.getEvasionProbability()));
	}

	if (kUnit.getWithdrawalProbability() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_WITHDRAWL_PROBABILITY", kUnit.getWithdrawalProbability()));
	}

	if (kUnit.getCombatLimit() < GC.getMAX_HIT_POINTS() && kUnit.getCombat() > 0 && !kUnit.isOnlyDefensive()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_COMBAT_LIMIT", (100 * kUnit.getCombatLimit()) / GC.getMAX_HIT_POINTS()));
	}

	if (kUnit.getCollateralDamage() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_COLLATERAL_DAMAGE", (100 * kUnit.getCollateralDamageLimit() / GC.getMAX_HIT_POINTS())));
	}

	for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
		if (kUnit.getUnitCombatCollateralImmune(eUnitCombat)) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_COLLATERAL_IMMUNE", GC.getUnitCombatInfo(eUnitCombat).getTextKeyWide()));
		}
	}

	if (kUnit.getCityAttackModifier() == kUnit.getCityDefenseModifier()) {
		if (kUnit.getCityAttackModifier() != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_CITY_STRENGTH_MOD", kUnit.getCityAttackModifier()));
		}
	} else {
		if (kUnit.getCityAttackModifier() != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_CITY_ATTACK_MOD", kUnit.getCityAttackModifier()));
		}

		if (kUnit.getCityDefenseModifier() != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_CITY_DEFENSE_MOD", kUnit.getCityDefenseModifier()));
		}
	}

	if (kUnit.getAnimalCombatModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_ANIMAL_COMBAT_MOD", kUnit.getAnimalCombatModifier()));
	}

	if (kUnit.getDropRange() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_PARADROP_RANGE", kUnit.getDropRange()));
	}

	if (kUnit.getHillsDefenseModifier() == kUnit.getHillsAttackModifier()) {
		if (kUnit.getHillsAttackModifier() != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_HILLS_STRENGTH", kUnit.getHillsAttackModifier()));
		}
	} else {
		if (kUnit.getHillsAttackModifier() != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_HILLS_ATTACK", kUnit.getHillsAttackModifier()));
		}

		if (kUnit.getHillsDefenseModifier() != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_HILLS_DEFENSE", kUnit.getHillsDefenseModifier()));
		}
	}

	for (TerrainTypes eTerrain = (TerrainTypes)0; eTerrain < GC.getNumTerrainInfos(); eTerrain = (TerrainTypes)(eTerrain + 1)) {
		if (kUnit.getTerrainDefenseModifier(eTerrain) != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_DEFENSE", kUnit.getTerrainDefenseModifier(eTerrain), GC.getTerrainInfo(eTerrain).getTextKeyWide()));
		}

		if (kUnit.getTerrainAttackModifier(eTerrain) != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_ATTACK", kUnit.getTerrainAttackModifier(eTerrain), GC.getTerrainInfo(eTerrain).getTextKeyWide()));
		}
	}

	for (FeatureTypes eFeature = (FeatureTypes)0; eFeature < GC.getNumFeatureInfos(); eFeature = (FeatureTypes)(eFeature + 1)) {
		if (kUnit.getFeatureDefenseModifier(eFeature) != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_DEFENSE", kUnit.getFeatureDefenseModifier(eFeature), GC.getFeatureInfo(eFeature).getTextKeyWide()));
		}

		if (kUnit.getFeatureAttackModifier(eFeature) != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_ATTACK", kUnit.getFeatureAttackModifier(eFeature), GC.getFeatureInfo(eFeature).getTextKeyWide()));
		}
	}

	for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
		if (kUnit.getUnitClassAttackModifier(eUnitClass) == kUnit.getUnitClassDefenseModifier(eUnitClass)) {
			if (kUnit.getUnitClassAttackModifier(eUnitClass) != 0) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_UNIT_MOD_VS_TYPE", kUnit.getUnitClassAttackModifier(eUnitClass), GC.getUnitClassInfo(eUnitClass).getTextKeyWide()));
			}
		} else {
			if (kUnit.getUnitClassAttackModifier(eUnitClass) != 0) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_UNIT_ATTACK_MOD_VS_CLASS", kUnit.getUnitClassAttackModifier(eUnitClass), GC.getUnitClassInfo(eUnitClass).getTextKeyWide()));
			}

			if (kUnit.getUnitClassDefenseModifier(eUnitClass) != 0) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_UNIT_DEFENSE_MOD_VS_CLASS", kUnit.getUnitClassDefenseModifier(eUnitClass), GC.getUnitClassInfo(eUnitClass).getTextKeyWide()));
			}
		}
	}

	for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
		if (kUnit.getUnitCombatModifier(eUnitCombat) != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_MOD_VS_TYPE", kUnit.getUnitCombatModifier(eUnitCombat), GC.getUnitCombatInfo(eUnitCombat).getTextKeyWide()));
		}
	}

	for (DomainTypes eDomain = (DomainTypes)0; eDomain < NUM_DOMAIN_TYPES; eDomain = (DomainTypes)(eDomain + 1)) {
		if (kUnit.getDomainModifier(eDomain) != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_MOD_VS_TYPE_NO_LINK", kUnit.getDomainModifier(eDomain), GC.getDomainInfo(eDomain).getTextKeyWide()));
		}
	}

	szTempBuffer.clear();
	bFirst = true;
	for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
		if (kUnit.getTargetUnitClass(eUnitClass)) {
			if (bFirst) {
				bFirst = false;
			} else {
				szTempBuffer += L", ";
			}

			szTempBuffer += CvWString::format(L"<link=literal>%s</link>", GC.getUnitClassInfo(eUnitClass).getDescription());
		}
	}

	if (!bFirst) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_TARGETS_UNIT_FIRST", szTempBuffer.GetCString()));
	}

	szTempBuffer.clear();
	bFirst = true;
	for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
		if (kUnit.getDefenderUnitClass(eUnitClass)) {
			if (bFirst) {
				bFirst = false;
			} else {
				szTempBuffer += L", ";
			}

			szTempBuffer += CvWString::format(L"<link=literal>%s</link>", GC.getUnitClassInfo(eUnitClass).getDescription());
		}
	}

	if (!bFirst) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_DEFENDS_UNIT_FIRST", szTempBuffer.GetCString()));
	}

	szTempBuffer.clear();
	bFirst = true;
	for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
		if (kUnit.getTargetUnitCombat(eUnitCombat)) {
			if (bFirst) {
				bFirst = false;
			} else {
				szTempBuffer += L", ";
			}

			szTempBuffer += CvWString::format(L"<link=literal>%s</link>", GC.getUnitCombatInfo(eUnitCombat).getDescription());
		}
	}

	if (!bFirst) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_TARGETS_UNIT_FIRST", szTempBuffer.GetCString()));
	}

	szTempBuffer.clear();
	bFirst = true;
	for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
		if (kUnit.getDefenderUnitCombat(eUnitCombat)) {
			if (bFirst) {
				bFirst = false;
			} else {
				szTempBuffer += L", ";
			}

			szTempBuffer += CvWString::format(L"<link=literal>%s</link>", GC.getUnitCombatInfo(eUnitCombat).getDescription());
		}
	}

	if (!bFirst) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_DEFENDS_UNIT_FIRST", szTempBuffer.GetCString()));
	}

	szTempBuffer.clear();
	bFirst = true;
	for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
		if (kUnit.getFlankingStrikeUnitClass(eUnitClass) > 0) {
			if (bFirst) {
				bFirst = false;
			} else {
				szTempBuffer += L", ";
			}

			szTempBuffer += CvWString::format(L"<link=literal>%s</link>", GC.getUnitClassInfo(eUnitClass).getDescription());
		}
	}

	if (!bFirst) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_FLANKING_STRIKES", szTempBuffer.GetCString()));
	}

	if (kUnit.getBombRate() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_BOMB_RATE", ((kUnit.getBombRate() * 100) / GC.getMAX_CITY_DEFENSE_DAMAGE())));
	}

	if (kUnit.getBombardRate() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_BOMBARD_RATE", ((kUnit.getBombardRate() * 100) / GC.getMAX_CITY_DEFENSE_DAMAGE())));
	}

	bFirst = true;
	for (PromotionTypes ePromotion = (PromotionTypes)0; ePromotion < GC.getNumPromotionInfos(); ePromotion = (PromotionTypes)(ePromotion + 1)) {
		if (kUnit.getFreePromotions(ePromotion)) {
			szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_UNIT_STARTS_WITH").c_str());
			setListHelp(szBuffer, szTempBuffer, CvWString::format(L"<link=literal>%s</link>", GC.getPromotionInfo(ePromotion).getDescription()), L", ", bFirst);
			bFirst = false;
		}
	}

	if (kUnit.getExtraCost() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_EXTRA_COST", kUnit.getExtraCost()));
	}

	if (bCivilopediaText) {
		// Trait
		for (TraitTypes eTrait = (TraitTypes)0; eTrait < GC.getNumTraitInfos(); eTrait = (TraitTypes)(eTrait + 1)) {
			if (kUnit.getProductionTraits(eTrait) != 0) {
				if (kUnit.getProductionTraits(eTrait) == 100) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_DOUBLE_SPEED_TRAIT", GC.getTraitInfo(eTrait).getTextKeyWide()));
				} else {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_PRODUCTION_MODIFIER_TRAIT", kUnit.getProductionTraits(eTrait), GC.getTraitInfo(eTrait).getTextKeyWide()));
				}
			}
		}
	}

	if (!CvWString(kUnit.getHelp()).empty()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(kUnit.getHelp());
	}
}


void CvGameTextMgr::setUnitHelp(CvWStringBuffer& szBuffer, UnitTypes eUnit, bool bCivilopediaText, bool bStrategyText, bool bTechChooserText, CvCity* pCity) {
	PROFILE_FUNC();

	CvWString szTempBuffer;

	if (NO_UNIT == eUnit) {
		return;
	}

	const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);

	PlayerTypes ePlayer;
	if (pCity != NULL) {
		ePlayer = pCity->getOwnerINLINE();
	} else {
		ePlayer = GC.getGameINLINE().getActivePlayer();
	}

	if (!bCivilopediaText) {
		szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_UNIT_TEXT"), kUnit.getDescription());
		szBuffer.append(szTempBuffer);

		if (kUnit.getUnitCombatType() != NO_UNITCOMBAT) {
			szTempBuffer.Format(L" (%s)", GC.getUnitCombatInfo((UnitCombatTypes)kUnit.getUnitCombatType()).getDescription());
			szBuffer.append(szTempBuffer);
		}
	}

	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());

	// test for unique unit
	UnitClassTypes eUnitClass = (UnitClassTypes)kUnit.getUnitClassType();
	const CvUnitClassInfo& kUnitClass = GC.getUnitClassInfo(eUnitClass);
	UnitTypes eDefaultUnit = (UnitTypes)kUnitClass.getDefaultUnitIndex();

	if (NO_UNIT != eDefaultUnit && eDefaultUnit != eUnit) {
		for (CivilizationTypes eCivilization = (CivilizationTypes)0; eCivilization < GC.getNumCivilizationInfos(); eCivilization = (CivilizationTypes)(eCivilization + 1)) {
			UnitTypes eUniqueUnit = (UnitTypes)GC.getCivilizationInfo(eCivilization).getCivilizationUnits(eUnitClass);
			if (eUniqueUnit == eUnit) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_UNIQUE_UNIT", GC.getCivilizationInfo(eCivilization).getTextKeyWide()));
			}
		}

		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_REPLACES_UNIT", GC.getUnitInfo(eDefaultUnit).getTextKeyWide()));
	}

	if (isWorldUnitClass(eUnitClass)) {
		if (pCity == NULL) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_WORLD_UNIT_ALLOWED", kUnitClass.getMaxGlobalInstances()));
		} else {
			int iRemaining = std::max(0, kUnitClass.getMaxGlobalInstances() - (ePlayer != NO_PLAYER ? GC.getGameINLINE().getUnitClassCreatedCount(eUnitClass) + kTeam.getUnitClassMaking(eUnitClass) : 0));
			szBuffer.append(NEWLINE);

			if (iRemaining <= 0)
				szBuffer.append(CvWString::format(SETCOLR, TEXT_COLOR("COLOR_WARNING_TEXT")));

			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_WORLD_UNIT_LEFT", iRemaining));

			if (iRemaining <= 0)
				szBuffer.append(ENDCOLR);
		}
	}

	if (isTeamUnitClass(eUnitClass)) {
		if (pCity == NULL) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_TEAM_UNIT_ALLOWED", kUnitClass.getMaxTeamInstances()));
		} else {
			int iRemaining = std::max(0, kUnitClass.getMaxTeamInstances() - (ePlayer != NO_PLAYER ? kTeam.getUnitClassCountPlusMaking(eUnitClass) : 0));
			szBuffer.append(NEWLINE);

			if (iRemaining <= 0)
				szBuffer.append(CvWString::format(SETCOLR, TEXT_COLOR("COLOR_WARNING_TEXT")));

			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_TEAM_UNIT_LEFT", iRemaining));

			if (iRemaining <= 0)
				szBuffer.append(ENDCOLR);
		}
	}

	if (isNationalUnitClass(eUnitClass)) {
		if (pCity == NULL) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_NATIONAL_UNIT_ALLOWED", kUnitClass.getMaxPlayerInstances()));
		} else {
			int iRemaining = std::max(0, kUnitClass.getMaxPlayerInstances() - (ePlayer != NO_PLAYER ? kPlayer.getUnitClassCountPlusMaking(eUnitClass) : 0));
			szBuffer.append(NEWLINE);

			if (iRemaining <= 0)
				szBuffer.append(CvWString::format(SETCOLR, TEXT_COLOR("COLOR_WARNING_TEXT")));

			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_NATIONAL_UNIT_LEFT", iRemaining));

			if (iRemaining <= 0)
				szBuffer.append(ENDCOLR);
		}
	}

	if (0 != kUnitClass.getInstanceCostModifier()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_INSTANCE_COST_MOD", kUnitClass.getInstanceCostModifier()));
	}

	setBasicUnitHelp(szBuffer, eUnit, bCivilopediaText);

	bool bFirst = true;
	if ((pCity == NULL) || !(pCity->canTrain(eUnit))) {
		if (pCity != NULL) {
			if (GC.getGameINLINE().isNoNukes()) {
				if (kUnit.getNukeRange() != -1) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_UNIT_NO_NUKES"));
				}
			}
		}

		if (kUnit.getHolyCity() != NO_RELIGION) {
			if (pCity == NULL || !pCity->isHolyCity((ReligionTypes)kUnit.getHolyCity())) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_UNIT_REQUIRES_HOLY_CITY", GC.getReligionInfo((ReligionTypes)(kUnit.getHolyCity())).getChar()));
			}
		}

		bFirst = true;
		if (kUnit.getSpecialUnitType() != NO_SPECIALUNIT) {
			if (pCity == NULL || !GC.getGameINLINE().isSpecialUnitValid((SpecialUnitTypes)kUnit.getSpecialUnitType())) {
				for (ProjectTypes eProject = (ProjectTypes)0; eProject < GC.getNumProjectInfos(); eProject = (ProjectTypes)(eProject + 1)) {
					const CvProjectInfo& kProject = GC.getProjectInfo(eProject);
					if (kProject.getEveryoneSpecialUnit() == kUnit.getSpecialUnitType()) {
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						CvWString szProject;
						szProject.Format(L"<link=literal>%s</link>", kProject.getDescription());
						setListHelp(szBuffer, szTempBuffer, szProject, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					}
				}
			}
		}
		if (!bFirst) {
			szBuffer.append(ENDCOLR);
		}

		bFirst = true;
		if (kUnit.getNukeRange() != -1) {
			if (NULL == pCity || !GC.getGameINLINE().isNukesValid()) {
				for (ProjectTypes eProject = (ProjectTypes)0; eProject < GC.getNumProjectInfos(); eProject = (ProjectTypes)(eProject + 1)) {
					const CvProjectInfo& kProject = GC.getProjectInfo(eProject);
					if (kProject.isAllowsNukes()) {
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						CvWString szProject;
						szProject.Format(L"<link=literal>%s</link>", kProject.getDescription());
						setListHelp(szBuffer, szTempBuffer, szProject, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					}
				}

				for (BuildingTypes eBuilding = (BuildingTypes)0; eBuilding < GC.getNumBuildingInfos(); eBuilding = (BuildingTypes)(eBuilding + 1)) {
					const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);
					if (kBuilding.isAllowsNukes()) {
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						CvWString szBuilding;
						szBuilding.Format(L"<link=literal>%s</link>", kBuilding.getDescription());
						setListHelp(szBuffer, szTempBuffer, szBuilding, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					}
				}
			}
		}
		if (!bFirst) {
			szBuffer.append(ENDCOLR);
		}

		if (!bCivilopediaText) {
			if (kUnit.getMinCultureLevel() != NO_CULTURELEVEL) {
				if (pCity == NULL || pCity->getCultureLevel() < kUnit.getMinCultureLevel()) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_UNIT_REQUIRES_CULTURE", GC.getCultureLevelInfo(kUnit.getMinCultureLevel()).getDescription()));
				}
			}

			if (kUnit.getMinPopulation() > 0) {
				if (pCity == NULL || pCity->getPopulation() < kUnit.getMinPopulation()) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_UNIT_REQUIRES_POPULATION", kUnit.getMinPopulation()));
				}
			}

			if (kUnit.isPrereqPower()) {
				if (pCity == NULL || !pCity->isPower()) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_UNIT_REQUIRES_POWER"));
					szBuffer.append(CvWString::format(L" (%c)", gDLL->getSymbolID(POWER_CHAR)));
				}
			}

			if (kUnit.getPrereqBuilding() != NO_BUILDING) {
				BuildingTypes ePrereqBuilding = (BuildingTypes)kUnit.getPrereqBuilding();
				if (pCity == NULL || (pCity->getNumBuilding(ePrereqBuilding) <= 0)) {
					// K-Mod. Check if the player has an exemption from the building requirement.
					SpecialBuildingTypes eSpecialBuilding = (SpecialBuildingTypes)GC.getBuildingInfo(ePrereqBuilding).getSpecialBuildingType();

					if (pCity == NULL || eSpecialBuilding == NO_SPECIALBUILDING || !GET_PLAYER(pCity->getOwnerINLINE()).isSpecialBuildingNotRequired(eSpecialBuilding)) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_UNIT_REQUIRES_STRING", GC.getBuildingInfo(ePrereqBuilding).getTextKeyWide()));
					}
				}
			}

			if (!bTechChooserText) {
				if (kUnit.getPrereqAndTech(0) != NO_TECH) {
					if (ePlayer == NO_PLAYER || !kTeam.isHasTech((TechTypes)kUnit.getPrereqAndTech(0))) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_UNIT_REQUIRES_STRING", GC.getTechInfo((TechTypes)kUnit.getPrereqAndTech(0)).getTextKeyWide()));
					}
				}
			}

			bFirst = true;
			for (int iPrereqIndex = 1; iPrereqIndex < kUnit.getNumPrereqAndTechs(); ++iPrereqIndex) {
				if (kUnit.getPrereqAndTech(iPrereqIndex) != NO_TECH) {
					if (bTechChooserText || ePlayer == NO_PLAYER || !kTeam.isHasTech((TechTypes)kUnit.getPrereqAndTech(iPrereqIndex))) {
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						setListHelp(szBuffer, szTempBuffer, GC.getTechInfo((TechTypes)kUnit.getPrereqAndTech(iPrereqIndex)).getDescription(), gDLL->getText("TXT_KEY_AND").c_str(), bFirst);
						bFirst = false;
					}
				}
			}
			if (!bFirst) {
				szBuffer.append(ENDCOLR);
			}

			bFirst = true;
			for (int iIndex = 1; iIndex < kUnit.getNumPrereqWorldViews(); ++iIndex) {
				if (kUnit.getPrereqWorldView(iIndex) != NO_WORLD_VIEW) {
					if (bTechChooserText || ePlayer == NO_PLAYER || !(GET_PLAYER(ePlayer).isWorldViewActivated((WorldViewTypes)iIndex))) {
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						setListHelp(szBuffer, szTempBuffer, GC.getWorldViewInfo((WorldViewTypes)kUnit.getPrereqWorldView(iIndex)).getDescription(), gDLL->getText("TXT_KEY_AND").c_str(), bFirst);
						bFirst = false;
					}
				}
			}
			if (!bFirst) {
				szBuffer.append(ENDCOLR);
			}

			bFirst = true;
			for (int iI = 0; iI < kUnit.getNumPrereqAndCivics(); iI++) {
				CivicTypes eLoopCivic = (CivicTypes)kUnit.getPrereqAndCivic(iI);
				if ((ePlayer == NO_PLAYER) || !(GET_PLAYER(ePlayer).isCivic(eLoopCivic))) {
					szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
					setListHelp(szBuffer, szTempBuffer, GC.getCivicInfo(eLoopCivic).getDescription(), gDLL->getText("TXT_KEY_AND").c_str(), bFirst);
					bFirst = false;
				}
			}
			if (!bFirst) {
				szBuffer.append(ENDCOLR);
			}

			bFirst = true;
			for (int iI = 0; iI < kUnit.getNumPrereqOrCivics(); iI++) {
				CivicTypes eLoopCivic = (CivicTypes)kUnit.getPrereqOrCivic(iI);
				if ((ePlayer == NO_PLAYER) || !(GET_PLAYER(ePlayer).isCivic(eLoopCivic))) {
					szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
					setListHelp(szBuffer, szTempBuffer, GC.getCivicInfo(eLoopCivic).getDescription(), gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
					bFirst = false;
				}
			}
			if (!bFirst) {
				szBuffer.append(ENDCOLR);
			}

			if (kUnit.getPrereqAndBonus() != NO_BONUS) {
				if (pCity == NULL || !pCity->hasBonus((BonusTypes)kUnit.getPrereqAndBonus())) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_UNIT_REQUIRES_STRING", GC.getBonusInfo((BonusTypes)kUnit.getPrereqAndBonus()).getTextKeyWide()));
				}
			}

			bFirst = true;
			for (int iPrereqIndex = 0; iPrereqIndex < kUnit.getNumPrereqOrBonuses(); ++iPrereqIndex) {
				BonusTypes ePrereqBonus = (BonusTypes)kUnit.getPrereqOrBonus(iPrereqIndex);
				if (ePrereqBonus != NO_BONUS) {
					if ((pCity == NULL) || !(pCity->hasBonus(ePrereqBonus))) {
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						setListHelp(szBuffer, szTempBuffer, GC.getBonusInfo(ePrereqBonus).getDescription(), gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					}
				}
			}
			if (!bFirst) {
				szBuffer.append(ENDCOLR);
			}
		}
	}

	if (!bCivilopediaText && GC.getGameINLINE().getActivePlayer() != NO_PLAYER) {
		if (pCity == NULL) {
			szTempBuffer.Format(L"%s%d%c", NEWLINE, kPlayer.getProductionNeeded(eUnit), GC.getYieldInfo(YIELD_PRODUCTION).getChar());
			szBuffer.append(szTempBuffer);
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_TURNS", pCity->getProductionTurnsLeft(eUnit, ((GC.ctrlKey() || !(GC.shiftKey())) ? 0 : pCity->getOrderQueueLength())), pCity->getProductionNeeded(eUnit), GC.getYieldInfo(YIELD_PRODUCTION).getChar()));

			int iProduction = pCity->getUnitProduction(eUnit);

			if (iProduction > 0) {
				szTempBuffer.Format(L" - %d/%d%c", iProduction, pCity->getProductionNeeded(eUnit), GC.getYieldInfo(YIELD_PRODUCTION).getChar());
				szBuffer.append(szTempBuffer);
			} else {
				szTempBuffer.Format(L" - %d%c", pCity->getProductionNeeded(eUnit), GC.getYieldInfo(YIELD_PRODUCTION).getChar());
				szBuffer.append(szTempBuffer);
			}
		}

		// OR Terrain
		bFirst = true;
		bool bValid = false;
		if (pCity != NULL) {
			for (int iI = 0; iI < kUnit.getNumPrereqOrTerrains(); ++iI) {
				if (pCity->hasVicinityTerrain((TerrainTypes)kUnit.getPrereqOrTerrain(iI))) {
					bValid = true;
					break;
				}
			}
		}
		if (!bValid) {
			for (int iI = 0; iI < kUnit.getNumPrereqOrTerrains(); ++iI) {
				TerrainTypes eTerrain = (TerrainTypes)kUnit.getPrereqOrTerrain(iI);
				if (pCity == NULL || !pCity->hasVicinityTerrain(eTerrain)) {
					CvWString szTempBuffer;
					szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
					CvWString szTerrain;
					szTerrain.Format(L"<link=literal>%s</link>", GC.getTerrainInfo(eTerrain).getDescription());
					setListHelp(szBuffer, szTempBuffer, szTerrain, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
					bFirst = false;
				}
			}
			if (!bFirst) {
				szBuffer.append(gDLL->getText("TXT_KEY_IN_CITY_VICINITY"));
			}
		}

		// AND Terrain
		bFirst = true;
		bValid = false;
		if (pCity != NULL) {
			bValid = true;
			for (int iI = 0; iI < kUnit.getNumPrereqAndTerrains() && bValid; ++iI) {
				if (!pCity->hasVicinityTerrain((TerrainTypes)kUnit.getPrereqAndTerrain(iI))) {
					bValid = false;
				}
			}
		}
		if (!bValid) {
			for (int iI = 0; iI < kUnit.getNumPrereqAndTerrains(); ++iI) {
				TerrainTypes eTerrain = (TerrainTypes)kUnit.getPrereqAndTerrain(iI);
				if (pCity == NULL || !pCity->hasVicinityTerrain(eTerrain)) {
					CvWString szTempBuffer;
					szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
					CvWString szTerrain;
					szTerrain.Format(L"<link=literal>%s</link>", GC.getTerrainInfo(eTerrain).getDescription());
					setListHelp(szBuffer, szTempBuffer, szTerrain, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
					bFirst = false;
				}
			}
			if (!bFirst) {
				szBuffer.append(gDLL->getText("TXT_KEY_IN_CITY_VICINITY"));
			}
		}

		// Vicinity Improvement
		bFirst = true;
		bValid = false;
		if (pCity != NULL) {
			for (int iI = 0; iI < kUnit.getNumPrereqVicinityImprovements(); ++iI) {
				if (pCity->hasVicinityImprovement((ImprovementTypes)kUnit.getPrereqVicinityImprovement(iI))) {
					bValid = true;
					break;
				}
			}
		}
		if (!bValid) {
			for (int iI = 0; iI < kUnit.getNumPrereqVicinityImprovements(); ++iI) {
				ImprovementTypes eImprovement = (ImprovementTypes)kUnit.getPrereqVicinityImprovement(iI);
				if (pCity == NULL || !pCity->hasVicinityImprovement(eImprovement)) {
					CvWString szTempBuffer;
					szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
					CvWString szImprovement;
					szImprovement.Format(L"<link=literal>%s</link>", GC.getImprovementInfo(eImprovement).getDescription());
					setListHelp(szBuffer, szTempBuffer, szImprovement, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
					bFirst = false;
				}
			}
			if (!bFirst) {
				szBuffer.append(gDLL->getText("TXT_KEY_IN_CITY_VICINITY"));
			}
		}

		// Vicinity Feature
		bFirst = true;
		bValid = false;
		if (pCity != NULL) {
			for (int iI = 0; iI < kUnit.getNumPrereqVicinityFeatures(); ++iI) {
				if (pCity->hasVicinityFeature((FeatureTypes)kUnit.getPrereqVicinityFeature(iI))) {
					bValid = true;
					break;
				}
			}
		}
		if (!bValid) {
			for (int iI = 0; iI < kUnit.getNumPrereqVicinityFeatures(); ++iI) {
				FeatureTypes eFeature = (FeatureTypes)kUnit.getPrereqVicinityFeature(iI);
				if (pCity == NULL || !pCity->hasVicinityFeature(eFeature)) {
					CvWString szTempBuffer;
					szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
					CvWString szFeature;
					szFeature.Format(L"<link=literal>%s</link>", GC.getFeatureInfo(eFeature).getDescription());
					setListHelp(szBuffer, szTempBuffer, szFeature, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
					bFirst = false;
				}
			}
			if (!bFirst) {
				szBuffer.append(gDLL->getText("TXT_KEY_IN_CITY_VICINITY"));
			}
		}

		// OR Bonus
		bFirst = true;
		bValid = false;
		if (pCity != NULL) {
			for (int iI = 0; iI < kUnit.getNumPrereqVicinityOrBonus(); ++iI) {
				if (pCity->hasVicinityBonus((BonusTypes)kUnit.getPrereqVicinityOrBonus(iI))) {
					bValid = true;
					break;
				}
			}
		}
		if (!bValid) {
			for (int iI = 0; iI < kUnit.getNumPrereqVicinityOrBonus(); ++iI) {
				BonusTypes eBonus = (BonusTypes)kUnit.getPrereqVicinityOrBonus(iI);
				if (pCity == NULL || !pCity->hasVicinityBonus(eBonus)) {
					CvWString szTempBuffer;
					szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
					CvWString szBonus;
					szBonus.Format(L"<link=literal>%s</link>", GC.getBonusInfo(eBonus).getDescription());
					setListHelp(szBuffer, szTempBuffer, szBonus, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
					bFirst = false;
				}
			}
			if (!bFirst) {
				szBuffer.append(gDLL->getText("TXT_KEY_IN_CITY_VICINITY"));
			}
		}

		// AND Bonus
		bFirst = true;
		bValid = false;
		if (pCity != NULL) {
			bValid = true;
			for (int iI = 0; iI < kUnit.getNumPrereqVicinityAndBonus() && bValid; ++iI) {
				if (!pCity->hasVicinityBonus((BonusTypes)kUnit.getPrereqVicinityAndBonus(iI))) {
					bValid = false;
				}
			}
		}
		if (!bValid) {
			for (int iI = 0; iI < kUnit.getNumPrereqVicinityAndBonus(); ++iI) {
				BonusTypes eBonus = (BonusTypes)kUnit.getPrereqVicinityAndBonus(iI);
				if (pCity == NULL || !pCity->hasVicinityBonus(eBonus)) {
					CvWString szTempBuffer;
					szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
					CvWString szBonus;
					szBonus.Format(L"<link=literal>%s</link>", GC.getBonusInfo(eBonus).getDescription());
					setListHelp(szBuffer, szTempBuffer, szBonus, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
					bFirst = false;
				}
			}
			if (!bFirst) {
				szBuffer.append(gDLL->getText("TXT_KEY_IN_CITY_VICINITY"));
			}
		}

		// OR BuildingClass
		bFirst = true;
		bValid = false;
		if (pCity != NULL) {
			for (int iI = 0; iI < kUnit.getNumPrereqOrBuildingClasses(); ++iI) {
				if (pCity->getNumActiveBuildingClass((BuildingClassTypes)kUnit.getPrereqOrBuildingClass(iI)) > 0) {
					bValid = true;
					break;
				}
			}
		}
		if (!bValid) {
			for (int iI = 0; iI < kUnit.getNumPrereqOrBuildingClasses(); ++iI) {
				BuildingClassTypes eBuildingClass = (BuildingClassTypes)kUnit.getPrereqOrBuildingClass(iI);
				if (pCity == NULL || !(pCity->getNumActiveBuildingClass(eBuildingClass) > 0)) {
					BuildingTypes ePrereqBuilding = (ePlayer != NO_PLAYER) ? (BuildingTypes)(GC.getCivilizationInfo(GET_PLAYER(ePlayer).getCivilizationType()).getCivilizationBuildings(eBuildingClass)) : (BuildingTypes)GC.getBuildingClassInfo(eBuildingClass).getDefaultBuildingIndex();
					CvWString szTempBuffer;
					szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
					CvWString szBonus;
					szBonus.Format(L"<link=literal>%s</link>", GC.getBuildingInfo(ePrereqBuilding).getDescription());
					setListHelp(szBuffer, szTempBuffer, szBonus, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
					bFirst = false;
				}
			}
			if (!bFirst) {
				szBuffer.append(gDLL->getText("TXT_KEY_IN_CITY_VICINITY"));
			}
		}

		// NOT BuildingClass
		bFirst = true;
		bValid = false;
		if (pCity != NULL) {
			for (int iI = 0; iI < kUnit.getNumPrereqNotBuildingClasses(); ++iI) {
				if (pCity->getNumActiveBuildingClass((BuildingClassTypes)kUnit.getPrereqNotBuildingClass(iI)) <= 0) {
					bValid = true;
					break;
				}
			}
		}
		if (!bValid) {
			for (int iI = 0; iI < kUnit.getNumPrereqNotBuildingClasses(); ++iI) {
				BuildingClassTypes eBuildingClass = (BuildingClassTypes)kUnit.getPrereqNotBuildingClass(iI);
				if (pCity == NULL || (pCity->getNumActiveBuildingClass(eBuildingClass) > 0)) {
					BuildingTypes ePrereqBuilding = (ePlayer != NO_PLAYER) ? (BuildingTypes)(GC.getCivilizationInfo(GET_PLAYER(ePlayer).getCivilizationType()).getCivilizationBuildings(eBuildingClass)) : (BuildingTypes)GC.getBuildingClassInfo(eBuildingClass).getDefaultBuildingIndex();
					CvWString szFirstBuffer;
					szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_NOT_REQUIRED_TO_BUILD").c_str());
					CvWString szTempBuffer;
					szTempBuffer.Format(L"<link=literal>%s</link>", GC.getBuildingInfo(ePrereqBuilding).getDescription());
					setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", bFirst);
					bFirst = false;
				}
			}
		}
	}

	for (BonusTypes eBonus = (BonusTypes)0; eBonus < GC.getNumBonusInfos(); eBonus = (BonusTypes)(eBonus + 1)) {
		const CvBonusInfo& kBonus = GC.getBonusInfo(eBonus);
		if (kUnit.getBonusProductionModifier(eBonus) != 0) {
			if (pCity != NULL) {
				if (pCity->hasBonus(eBonus)) {
					szBuffer.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));
				} else {
					szBuffer.append(gDLL->getText("TXT_KEY_COLOR_NEGATIVE"));
				}
			}
			if (!bCivilopediaText) {
				szBuffer.append(L" (");
			} else {
				szTempBuffer.Format(L"%s%c", NEWLINE, gDLL->getSymbolID(BULLET_CHAR), szTempBuffer);
				szBuffer.append(szTempBuffer);
			}
			if (kUnit.getBonusProductionModifier(eBonus) == 100) {
				szBuffer.append(gDLL->getText("TXT_KEY_UNIT_DOUBLE_SPEED", kBonus.getTextKeyWide()));
			} else {
				szBuffer.append(gDLL->getText("TXT_KEY_UNIT_BUILDS_FASTER", kUnit.getBonusProductionModifier(eBonus), kBonus.getTextKeyWide()));
			}
			if (!bCivilopediaText) {
				szBuffer.append(L")");
			}
			if (pCity != NULL) {
				szBuffer.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
			}
		}
	}

	if (kUnit.getObsoleteTech() != NO_TECH) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_OBSOLETE_WITH", GC.getTechInfo((TechTypes)kUnit.getObsoleteTech()).getTextKeyWide()));
	}

	if (bStrategyText) {
		if (!CvWString(kUnit.getStrategy()).empty()) {
			if ((ePlayer == NO_PLAYER) || kPlayer.isOption(PLAYEROPTION_ADVISOR_HELP)) {
				szBuffer.append(SEPARATOR);
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_SIDS_TIPS"));
				szBuffer.append(L'\"');
				szBuffer.append(kUnit.getStrategy());
				szBuffer.append(L'\"');
			}
		}
	}

	if (bCivilopediaText) {
		if (eDefaultUnit == eUnit) {
			for (int iI = 0; iI < GC.getNumUnitInfos(); ++iI) {
				if (iI != eUnit) {
					if (eUnitClass == GC.getUnitInfo((UnitTypes)iI).getUnitClassType()) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_REPLACED_BY_UNIT", GC.getUnitInfo((UnitTypes)iI).getTextKeyWide()));
					}
				}
			}
		}
	}

	if (pCity != NULL) {
		if ((gDLL->getChtLvl() > 0) && GC.ctrlKey()) {
			szBuffer.append(NEWLINE);
			for (int iUnitAI = 0; iUnitAI < NUM_UNITAI_TYPES; iUnitAI++) {
				int iTempValue = GET_PLAYER(pCity->getOwner()).AI_unitValue(eUnit, (UnitAITypes)iUnitAI, pCity->area());
				if (iTempValue != 0) {
					CvWString szTempString;
					getUnitAIString(szTempString, (UnitAITypes)iUnitAI);
					szBuffer.append(CvWString::format(L"(%s : %d) ", szTempString.GetCString(), iTempValue));
				}
			}
		}
	}
}

/*
* Calls new function below without displaying actual effects.
*/
void CvGameTextMgr::setBuildingHelp(CvWStringBuffer& szBuffer, BuildingTypes eBuilding, bool bCivilopediaText, bool bStrategyText, bool bTechChooserText, CvCity* pCity) {
	setBuildingHelpActual(szBuffer, eBuilding, bCivilopediaText, bStrategyText, bTechChooserText, pCity, false);
}

/*
* Adds the actual effects of adding a building to the city.
*/
void CvGameTextMgr::setBuildingNetEffectsHelp(CvWStringBuffer& szBuffer, BuildingTypes eBuilding, CvCity* pCity) {
	if (NULL != pCity) {
		bool bStarted = false;
		CvWString szStart;
		szStart.Format(L"\n"SETCOLR L"(%s", TEXT_COLOR("COLOR_LIGHT_GREY"), gDLL->getText("TXT_KEY_ACTUAL_EFFECTS").GetCString());

		// Defense
		int iDefense = pCity->getAdditionalDefenseByBuilding(eBuilding);
		bStarted = setResumableValueChangeHelp(szBuffer, szStart, L": ", L"", iDefense, gDLL->getSymbolID(DEFENSE_CHAR), true, false, bStarted);

		// Happiness
		int iGood = 0;
		int iBad = 0;
		pCity->getAdditionalHappinessByBuilding(eBuilding, iGood, iBad);
		//int iAngryPop = pCity->getAdditionalAngryPopuplation(iGood, iBad);
		bStarted = setResumableGoodBadChangeHelp(szBuffer, szStart, L": ", L"", iGood, gDLL->getSymbolID(HAPPY_CHAR), iBad, gDLL->getSymbolID(UNHAPPY_CHAR), false, false, bStarted);
		//bStarted = setResumableValueChangeHelp(szBuffer, szStart, L": ", L"", iAngryPop, gDLL->getSymbolID(ANGRY_POP_CHAR), false, bNewLine, bStarted);

		// Health
		iGood = 0;
		iBad = 0;
		pCity->getAdditionalHealthByBuilding(eBuilding, iGood, iBad);
		//int iSpoiledFood = pCity->getAdditionalSpoiledFood(iGood, iBad);
		//int iStarvation = pCity->getAdditionalStarvation(iSpoiledFood);
		bStarted = setResumableGoodBadChangeHelp(szBuffer, szStart, L": ", L"", iGood, gDLL->getSymbolID(HEALTHY_CHAR), iBad, gDLL->getSymbolID(UNHEALTHY_CHAR), false, false, bStarted);
		//bStarted = setResumableValueChangeHelp(szBuffer, szStart, L": ", L"", iSpoiledFood, gDLL->getSymbolID(EATEN_FOOD_CHAR), false, false, bStarted);
		//bStarted = setResumableValueChangeHelp(szBuffer, szStart, L": ", L"", iStarvation, gDLL->getSymbolID(BAD_FOOD_CHAR), false, false, bStarted);

		// Yield
		int aiYields[NUM_YIELD_TYPES];
		for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI) {
			aiYields[iI] = pCity->getAdditionalYieldByBuilding((YieldTypes)iI, eBuilding);
		}
		bStarted = setResumableYieldChangeHelp(szBuffer, szStart, L": ", L"", aiYields, false, false, bStarted);

		// Commerce
		int aiCommerces[NUM_COMMERCE_TYPES];
		for (int iI = 0; iI < NUM_COMMERCE_TYPES; ++iI) {
			aiCommerces[iI] = pCity->getAdditionalCommerceTimes100ByBuilding((CommerceTypes)iI, eBuilding);
		}
		// Maintenance - add to gold
		aiCommerces[COMMERCE_GOLD] += pCity->getSavedMaintenanceTimes100ByBuilding(eBuilding);
		bStarted = setResumableCommerceTimes100ChangeHelp(szBuffer, szStart, L": ", L"", aiCommerces, false, bStarted);

		// Great People
		int iGreatPeopleRate = pCity->getAdditionalGreatPeopleRateByBuilding(eBuilding);
		bStarted = setResumableValueChangeHelp(szBuffer, szStart, L": ", L"", iGreatPeopleRate, gDLL->getSymbolID(GREAT_PEOPLE_CHAR), false, false, bStarted);

		if (bStarted)
			szBuffer.append(L")" ENDCOLR);
	}
}

/*
* Adds option to display actual effects.
*/
void CvGameTextMgr::setBuildingHelpActual(CvWStringBuffer& szBuffer, BuildingTypes eBuilding, bool bCivilopediaText, bool bStrategyText, bool bTechChooserText, CvCity* pCity, bool bActual) {
	PROFILE_FUNC();

	CvWString szFirstBuffer;
	CvWString szTempBuffer;

	if (NO_BUILDING == eBuilding) {
		return;
	}

	CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);

	PlayerTypes ePlayer;
	if (pCity != NULL) {
		ePlayer = pCity->getOwnerINLINE();
	} else {
		ePlayer = GC.getGameINLINE().getActivePlayer();
	}

	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	if (!bCivilopediaText) {
		szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), kBuilding.getDescription());
		szBuffer.append(szTempBuffer);

		int iHappiness;
		iHappiness = kBuilding.getHappiness();
		if (pCity) {
			// special modifiers (eg. events). These modifiers don't get their own line of text, so they need to be included here.
			iHappiness += pCity->getBuildingHappyChange((BuildingClassTypes)kBuilding.getBuildingClassType());
			iHappiness += kPlayer.getExtraBuildingHappiness(eBuilding);
			// 'Extra building happiness' includes happiness from several sources, including events, civics, traits, and boosts from other buildings.
			// My aim here is to only include in the total what isn't already in the list of bonuses below. As far as I know the only thing that would
			// be double-reported is the civic happiness. So I'll subtract that.
			for (CivicTypes eCivic = (CivicTypes)0; eCivic < GC.getNumCivicInfos(); eCivic = (CivicTypes)(eCivic + 1)) {
				if (kPlayer.isCivic(eCivic)) {
					iHappiness -= GC.getCivicInfo(eCivic).getBuildingHappinessChanges(kBuilding.getBuildingClassType());
				}
			}
		}

		if (iHappiness != 0) {
			szTempBuffer.Format(L", +%d%c", abs(iHappiness), ((iHappiness > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR)));
			szBuffer.append(szTempBuffer);
		}

		int iHealth;
		iHealth = kBuilding.getHealth();
		if (pCity) {
			// special modifiers (eg. events). These modifiers don't get their own line of text, so they need to be included here.
			iHealth += pCity->getBuildingHealthChange((BuildingClassTypes)kBuilding.getBuildingClassType());
			iHealth += kPlayer.getExtraBuildingHealth(eBuilding);
			// We need to subtract any civic bonuses from 'extra building health', so as not to double-report. (see comments for the happiness section.)
			for (CivicTypes eCivic = (CivicTypes)0; eCivic < GC.getNumCivicInfos(); eCivic = (CivicTypes)(eCivic + 1)) {
				if (kPlayer.isCivic(eCivic)) {
					iHealth -= GC.getCivicInfo(eCivic).getBuildingHealthChanges(kBuilding.getBuildingClassType());
				}
			}
		}

		if (iHealth != 0) {
			szTempBuffer.Format(L", +%d%c", abs(iHealth), ((iHealth > 0) ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR)));
			szBuffer.append(szTempBuffer);
		}

		int aiYields[NUM_YIELD_TYPES];
		for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1)) {
			aiYields[eYield] = kBuilding.getYieldChange(eYield);

			if (NULL != pCity) {
				aiYields[eYield] += pCity->getBuildingYieldChange((BuildingClassTypes)kBuilding.getBuildingClassType(), eYield);
			}
		}
		setYieldChangeHelp(szBuffer, L", ", L"", L"", aiYields, false, false);

		int aiCommerces[NUM_COMMERCE_TYPES];
		for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
			if ((NULL != pCity) && (pCity->getNumBuilding(eBuilding) > 0)) {
				aiCommerces[eCommerce] = pCity->getBuildingCommerceByBuilding(eCommerce, eBuilding);
			} else {
				aiCommerces[eCommerce] = kBuilding.getCommerceChange(eCommerce);
				aiCommerces[eCommerce] += kBuilding.getObsoleteSafeCommerceChange(eCommerce);
				if (ePlayer != NO_PLAYER && kBuilding.getReligionType() != NO_RELIGION && kBuilding.getReligionType() == kPlayer.getStateReligion()) {
					aiCommerces[eCommerce] += kPlayer.getStateReligionBuildingCommerce(eCommerce);
				}
			}
		}
		setCommerceChangeHelp(szBuffer, L", ", L"", L"", aiCommerces, false, false);

		setYieldChangeHelp(szBuffer, L", ", L"", L"", kBuilding.getYieldModifierArray(), true, bCivilopediaText);
		setCommerceChangeHelp(szBuffer, L", ", L"", L"", kBuilding.getCommerceModifierArray(), true, bCivilopediaText);

		if (kBuilding.getGreatPeopleRateChange() != 0) {
			szTempBuffer.Format(L", %s%d%c", ((kBuilding.getGreatPeopleRateChange() > 0) ? "+" : ""), kBuilding.getGreatPeopleRateChange(), gDLL->getSymbolID(GREAT_PEOPLE_CHAR));
			szBuffer.append(szTempBuffer);

			if (kBuilding.getGreatPeopleUnitClass() != NO_UNITCLASS) {
				UnitTypes eGreatPeopleUnit;
				if (ePlayer != NO_PLAYER) {
					eGreatPeopleUnit = ((UnitTypes)(GC.getCivilizationInfo(kPlayer.getCivilizationType()).getCivilizationUnits(kBuilding.getGreatPeopleUnitClass())));
				} else {
					eGreatPeopleUnit = (UnitTypes)GC.getUnitClassInfo((UnitClassTypes)kBuilding.getGreatPeopleUnitClass()).getDefaultUnitIndex();
				}

				if (eGreatPeopleUnit != NO_UNIT) {
					szTempBuffer.Format(L" (%s)", GC.getUnitInfo(eGreatPeopleUnit).getDescription());
					szBuffer.append(szTempBuffer);
				}
			}
		}
	}

	// test for unique building
	BuildingClassTypes eBuildingClass = (BuildingClassTypes)kBuilding.getBuildingClassType();
	BuildingTypes eDefaultBuilding = (BuildingTypes)GC.getBuildingClassInfo(eBuildingClass).getDefaultBuildingIndex();

	if (NO_BUILDING != eDefaultBuilding && eDefaultBuilding != eBuilding) {
		for (CivilizationTypes eCivilization = (CivilizationTypes)0; eCivilization < GC.getNumCivilizationInfos(); eCivilization = (CivilizationTypes)(eCivilization + 1)) {
			BuildingTypes eUniqueBuilding = (BuildingTypes)GC.getCivilizationInfo(eCivilization).getCivilizationBuildings((int)eBuildingClass);
			if (eUniqueBuilding == eBuilding) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_UNIQUE_BUILDING", GC.getCivilizationInfo(eCivilization).getTextKeyWide()));
			}
		}

		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_REPLACES_UNIT", GC.getBuildingInfo(eDefaultBuilding).getTextKeyWide()));
	}

	if (bCivilopediaText) {
		setYieldChangeHelp(szBuffer, L"", L"", L"", kBuilding.getYieldModifierArray(), true, bCivilopediaText);

		setCommerceChangeHelp(szBuffer, L"", L"", L"", kBuilding.getCommerceModifierArray(), true, bCivilopediaText);
	} else {
		const CvBuildingClassInfo& kBuildingClass = GC.getBuildingClassInfo(eBuildingClass);
		const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
		if (isWorldWonderClass(eBuildingClass)) {
			if (pCity == NULL || ePlayer == NO_PLAYER) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_WORLD_WONDER_ALLOWED", kBuildingClass.getMaxGlobalInstances()));
			} else {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_WORLD_WONDER_LEFT", kBuildingClass.getMaxGlobalInstances() - GC.getGameINLINE().getBuildingClassCreatedCount(eBuildingClass) - kTeam.getBuildingClassMaking(eBuildingClass)));
			}
		}

		if (isTeamWonderClass(eBuildingClass)) {
			if (pCity == NULL || ePlayer == NO_PLAYER) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_TEAM_WONDER_ALLOWED", kBuildingClass.getMaxTeamInstances()));
			} else {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_TEAM_WONDER_LEFT", kBuildingClass.getMaxTeamInstances() - kTeam.getBuildingClassCountPlusMaking(eBuildingClass)));
			}
		}

		if (isNationalWonderClass(eBuildingClass)) {
			if (pCity == NULL || ePlayer == NO_PLAYER) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_NATIONAL_WONDER_ALLOWED", kBuildingClass.getMaxPlayerInstances()));
			} else {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_NATIONAL_WONDER_LEFT", kBuildingClass.getMaxPlayerInstances() - kPlayer.getBuildingClassCountPlusMaking(eBuildingClass)));
			}
		}
	}

	if (kBuilding.getStarSignMitigateChangePercent() != 0) {
		szTempBuffer = kBuilding.getStarSignMitigateChangePercent() > 0 ? "TXT_KEY_STAR_SIGN_MITIGATE_GOOD" : "TXT_KEY_STAR_SIGN_MITIGATE_BAD";
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText(szTempBuffer, abs(kBuilding.getStarSignMitigateChangePercent())));
	}
	if (kBuilding.getStarSignScaleChangePercent() != 0) {
		szTempBuffer = kBuilding.getStarSignScaleChangePercent() > 0 ? "TXT_KEY_STAR_SIGN_SCALE_GOOD" : "TXT_KEY_STAR_SIGN_SCALE_BAD";
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText(szTempBuffer, abs(kBuilding.getStarSignScaleChangePercent())));
	}
	if (kBuilding.getGlobalStarSignMitigateChangePercent() != 0) {
		szTempBuffer = kBuilding.getGlobalStarSignMitigateChangePercent() > 0 ? "TXT_KEY_STAR_SIGN_GLOBAL_MITIGATE_GOOD" : "TXT_KEY_STAR_SIGN_GOBAL_MITIGATE_BAD";
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText(szTempBuffer, abs(kBuilding.getGlobalStarSignMitigateChangePercent())));
	}
	if (kBuilding.getGlobalStarSignScaleChangePercent() != 0) {
		szTempBuffer = kBuilding.getGlobalStarSignScaleChangePercent() > 0 ? "TXT_KEY_STAR_SIGN_GLOBAL_SCALE_GOOD" : "TXT_KEY_STAR_SIGN_GLOBAL_SCALE_BAD";
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText(szTempBuffer, abs(kBuilding.getGlobalStarSignScaleChangePercent())));
	}
	if (kBuilding.isForceDisableStarSigns()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_STAR_SIGN_DISABLE"));
	}
	if (kBuilding.isStarSignGoodOnly()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_STAR_SIGN_GOOD_ONLY"));
	}

	if (kBuilding.getGlobalReligionCommerce() != NO_RELIGION) {
		szFirstBuffer = gDLL->getText("TXT_KEY_BUILDING_PER_CITY_WITH", GC.getReligionInfo((ReligionTypes)kBuilding.getGlobalReligionCommerce()).getChar());
		setCommerceChangeHelp(szBuffer, L"", L"", szFirstBuffer, GC.getReligionInfo((ReligionTypes)kBuilding.getGlobalReligionCommerce()).getGlobalReligionCommerceArray());
	}

	if (NO_CORPORATION != kBuilding.getFoundsCorporation()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_FOUNDS_CORPORATION", GC.getCorporationInfo((CorporationTypes)kBuilding.getFoundsCorporation()).getTextKeyWide()));
	}

	if (kBuilding.getGlobalCorporationCommerce() != NO_CORPORATION) {
		szFirstBuffer = gDLL->getText("TXT_KEY_BUILDING_PER_CITY_WITH", GC.getCorporationInfo((CorporationTypes)kBuilding.getGlobalCorporationCommerce()).getChar());
		setCommerceChangeHelp(szBuffer, L"", L"", szFirstBuffer, GC.getCorporationInfo((CorporationTypes)kBuilding.getGlobalCorporationCommerce()).getHeadquarterCommerceArray());
	}

	if (kBuilding.getNoBonus() != NO_BONUS) {
		CvBonusInfo& kBonus = GC.getBonusInfo((BonusTypes)kBuilding.getNoBonus());
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_DISABLES", kBonus.getTextKeyWide(), kBonus.getChar()));
	}

	if (kBuilding.getFreeBonus() != NO_BONUS) {
		const CvBonusInfo& kBonus = GC.getBonusInfo((BonusTypes)kBuilding.getFreeBonus());
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_PROVIDES", GC.getGameINLINE().getNumFreeBonuses(eBuilding), kBonus.getTextKeyWide(), kBonus.getChar()));

		if (kBonus.getHealth() != 0) {
			szTempBuffer.Format(L", +%d%c", abs(kBonus.getHealth()), kBonus.getHealth() > 0 ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR));
			szBuffer.append(szTempBuffer);
		}

		if (GC.getBonusInfo((BonusTypes)(kBuilding.getFreeBonus())).getHappiness() != 0) {
			szTempBuffer.Format(L", +%d%c", abs(kBonus.getHappiness()), kBonus.getHappiness() > 0 ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR));
			szBuffer.append(szTempBuffer);
		}
	}

	if (kBuilding.getFreeBuildingClass() != NO_BUILDINGCLASS) {
		BuildingTypes eFreeBuilding;
		if (ePlayer != NO_PLAYER) {
			eFreeBuilding = ((BuildingTypes)(GC.getCivilizationInfo(kPlayer.getCivilizationType()).getCivilizationBuildings(kBuilding.getFreeBuildingClass())));
		} else {
			eFreeBuilding = (BuildingTypes)GC.getBuildingClassInfo((BuildingClassTypes)kBuilding.getFreeBuildingClass()).getDefaultBuildingIndex();
		}

		if (NO_BUILDING != eFreeBuilding) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_FREE_IN_CITY", GC.getBuildingInfo(eFreeBuilding).getTextKeyWide()));
		}
	}

	if (kBuilding.getWorkableRadius() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_EXPANDS_WORKABLE_RADIUS", kBuilding.getWorkableRadius()));
	}

	if (kBuilding.getFreePromotion() != NO_PROMOTION) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_FREE_PROMOTION", GC.getPromotionInfo((PromotionTypes)(kBuilding.getFreePromotion())).getTextKeyWide()));
	}

	if (kBuilding.getCivicOption() != NO_CIVICOPTION) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_ENABLES_CIVICS", GC.getCivicOptionInfo((CivicOptionTypes)(kBuilding.getCivicOption())).getTextKeyWide()));
	}

	if (kBuilding.isPower()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_PROVIDES_POWER"));

		// K-Mod. Also include base health change from power.
		int iPowerHealth = GC.getDefineINT("POWER_HEALTH_CHANGE") + (kBuilding.isDirtyPower() ? GC.getDefineINT("DIRTY_POWER_HEALTH_CHANGE") : 0);
		if (iPowerHealth) {
			szTempBuffer.Format(L" (+%d%c)", abs(iPowerHealth), iPowerHealth > 0 ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR));
			szBuffer.append(szTempBuffer);
		}
	}

	if (kBuilding.isAreaCleanPower()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_PROVIDES_AREA_CLEAN_POWER"));
	}

	if (kBuilding.isAreaBorderObstacle()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_BORDER_OBSTACLE"));
	}

	for (VoteSourceTypes eVoteSource = (VoteSourceTypes)0; eVoteSource < GC.getNumVoteSourceInfos(); eVoteSource = (VoteSourceTypes)(eVoteSource + 1)) {
		if (kBuilding.getVoteSourceType() == eVoteSource) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_DIPLO_VOTE", GC.getVoteSourceInfo(eVoteSource).getTextKeyWide()));
		}
	}

	if (kBuilding.isForceTeamVoteEligible()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_ELECTION_ELIGIBILITY"));
	}

	if (kBuilding.isCapital()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_CAPITAL"));
	}

	if (kBuilding.isGovernmentCenter()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_REDUCES_MAINTENANCE"));
	}

	if (kBuilding.isGoldenAge()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_GOLDEN_AGE"));
	}

	if (kBuilding.isAllowsNukes()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_ENABLES_NUKES"));
	}

	if (kBuilding.isMapCentering()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_CENTERS_MAP"));
	}

	if (kBuilding.isNoUnhappiness()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_NO_UNHAPPY"));
	}

	if (kBuilding.getUnhealthyPopulationModifier() != 0) {
		// If the modifier is less than -100, display the old NoUnhealth. text
		// Note: this could be techinically inaccurate if we combine this modifier with a positive modifier
		if (kBuilding.getUnhealthyPopulationModifier() <= -100) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_CIVIC_NO_POP_UNHEALTHY"));
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNHEALTHY_POP_MODIFIER", kBuilding.getUnhealthyPopulationModifier()));
		}
	}

	if (kBuilding.isBuildingOnlyHealthy()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_NO_UNHEALTHY_BUILDINGS"));
	}

	if (kBuilding.isSlaveMarket()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_TRADE_SLAVES"));
	}

	if (kBuilding.getGreatPeopleRateModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_BIRTH_RATE_MOD", kBuilding.getGreatPeopleRateModifier()));
	}

	if (kBuilding.getGreatGeneralRateModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_GENERAL_RATE_MOD", kBuilding.getGreatGeneralRateModifier()));
	}

	if (kBuilding.getDomesticGreatGeneralRateModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_DOMESTIC_GREAT_GENERAL_MODIFIER", kBuilding.getDomesticGreatGeneralRateModifier()));
	}

	if (kBuilding.getGlobalGreatPeopleRateModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_BIRTH_RATE_MOD_ALL_CITIES", kBuilding.getGlobalGreatPeopleRateModifier()));
	}

	if (kBuilding.getAnarchyModifier() != 0) {
		if (-100 == kBuilding.getAnarchyModifier()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_NO_ANARCHY"));
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_ANARCHY_MOD", kBuilding.getAnarchyModifier()));

			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_ANARCHY_TIMER_MOD", kBuilding.getAnarchyModifier()));
		}
	}

	if (kBuilding.getGoldenAgeModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_GOLDENAGE_MOD", kBuilding.getGoldenAgeModifier()));
	}

	if (kBuilding.getGlobalHurryModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_HURRY_MOD", kBuilding.getGlobalHurryModifier()));
	}

	if (kBuilding.getFreeExperience() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_FREE_XP_UNITS", kBuilding.getFreeExperience()));
	}

	if (kBuilding.getGlobalFreeExperience() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_FREE_XP_ALL_CITIES", kBuilding.getGlobalFreeExperience()));
	}

	if (kBuilding.getFoodKept() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_STORES_FOOD", kBuilding.getFoodKept()));
	}

	if (kBuilding.getAirlift() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_AIRLIFT", kBuilding.getAirlift()));
	}

	if (kBuilding.getAirModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_AIR_DAMAGE_MOD", kBuilding.getAirModifier()));
	}

	if (kBuilding.getAirUnitCapacity() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_AIR_UNIT_CAPACITY", kBuilding.getAirUnitCapacity()));
	}

	if (kBuilding.getNukeModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_NUKE_DAMAGE_MOD", kBuilding.getNukeModifier()));
	}

	if (kBuilding.getNukeExplosionRand() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_NUKE_EXPLOSION_CHANCE"));
	}

	if (kBuilding.getFreeSpecialist() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_FREE_SPECIALISTS", kBuilding.getFreeSpecialist()));
	}

	if (kBuilding.getAreaFreeSpecialist() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_FREE_SPECIALISTS_CONT", kBuilding.getAreaFreeSpecialist()));
	}

	if (kBuilding.getGlobalFreeSpecialist() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_FREE_SPECIALISTS_ALL_CITIES", kBuilding.getGlobalFreeSpecialist()));
	}

	if (kBuilding.getMaintenanceModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_MAINT_MOD", kBuilding.getMaintenanceModifier()));
	}

	if (kBuilding.getHurryAngerModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_HURRY_ANGER_MOD", kBuilding.getHurryAngerModifier()));
	}

	if (kBuilding.getWarWearinessModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_WAR_WEAR_MOD", kBuilding.getWarWearinessModifier()));
	}

	if (kBuilding.getGlobalWarWearinessModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_WAR_WEAR_MOD_ALL_CITIES", kBuilding.getGlobalWarWearinessModifier()));
	}

	if (kBuilding.getEnemyWarWearinessModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_ENEMY_WAR_WEAR", kBuilding.getEnemyWarWearinessModifier()));
	}

	if (kBuilding.getHealRateChange() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_HEAL_MOD", kBuilding.getHealRateChange()));
	}

	if (kBuilding.getAreaHealth() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_HEALTH_CHANGE_CONT", abs(kBuilding.getAreaHealth()), ((kBuilding.getAreaHealth() > 0) ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR))));
	}

	if (kBuilding.getGlobalHealth() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_HEALTH_CHANGE_ALL_CITIES", abs(kBuilding.getGlobalHealth()), ((kBuilding.getGlobalHealth() > 0) ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR))));
	}

	// Use absolute value with unhappy face
	if (kBuilding.getAreaHappiness() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_HAPPY_CHANGE_CONT", abs(kBuilding.getAreaHappiness()), ((kBuilding.getAreaHappiness() > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR))));
	}

	if (kBuilding.getGlobalHappiness() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_HAPPY_CHANGE_ALL_CITIES", abs(kBuilding.getGlobalHappiness()), ((kBuilding.getGlobalHappiness() > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR))));
	}

	if (kBuilding.getStateReligionHappiness() != 0) {
		if (kBuilding.getReligionType() != NO_RELIGION) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_RELIGION_HAPPINESS", abs(kBuilding.getStateReligionHappiness()), ((kBuilding.getStateReligionHappiness() > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR)), GC.getReligionInfo((ReligionTypes)(kBuilding.getReligionType())).getChar()));
		}
	}

	if (kBuilding.getWorkerSpeedModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_WORKER_MOD", kBuilding.getWorkerSpeedModifier()));
	}

	if (kBuilding.getMilitaryProductionModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_MILITARY_MOD", kBuilding.getMilitaryProductionModifier()));
	}

	if (kBuilding.getSpaceProductionModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_SPACESHIP_MOD", kBuilding.getSpaceProductionModifier()));
	}

	if (kBuilding.getGlobalSpaceProductionModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_SPACESHIP_MOD_ALL_CITIES", kBuilding.getGlobalSpaceProductionModifier()));
	}

	if (kBuilding.getTradeRoutes() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_TRADE_ROUTES", kBuilding.getTradeRoutes()));
	}

	if (kBuilding.getCoastalTradeRoutes() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_COASTAL_TRADE_ROUTES", kBuilding.getCoastalTradeRoutes()));
	}

	if (kBuilding.getGlobalTradeRoutes() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_TRADE_ROUTES_ALL_CITIES", kBuilding.getGlobalTradeRoutes()));
	}

	if (kBuilding.getTradeRouteModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_TRADE_ROUTE_MOD", kBuilding.getTradeRouteModifier()));
	}

	if (kBuilding.getForeignTradeRouteModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_FOREIGN_TRADE_ROUTE_MOD", kBuilding.getForeignTradeRouteModifier()));
	}

	if (kBuilding.getGlobalPopulationChange() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_GLOBAL_POP", kBuilding.getGlobalPopulationChange()));
	}

	if (kBuilding.getFreeTechs() != 0) {
		if (kBuilding.getFreeTechs() == 1) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_FREE_TECH"));
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_FREE_TECHS", kBuilding.getFreeTechs()));
		}
	}

	if (kBuilding.getDefenseModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_DEFENSE_MOD", kBuilding.getDefenseModifier()));
	}

	if (kBuilding.getBombardDefenseModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_BOMBARD_DEFENSE_MOD", -kBuilding.getBombardDefenseModifier()));
	}

	if (kBuilding.getAllCityDefenseModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_DEFENSE_MOD_ALL_CITIES", kBuilding.getAllCityDefenseModifier()));
	}

	if (kBuilding.getEspionageDefenseModifier() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_ESPIONAGE_DEFENSE_MOD", kBuilding.getEspionageDefenseModifier()));

		if (kBuilding.getEspionageDefenseModifier() > 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_EXPOSE_SPIES"));
		}
	}

	setYieldChangeHelp(szBuffer, gDLL->getText("TXT_KEY_BUILDING_WATER_PLOTS").c_str(), L": ", L"", kBuilding.getSeaPlotYieldChangeArray());

	setYieldChangeHelp(szBuffer, gDLL->getText("TXT_KEY_BUILDING_RIVER_PLOTS").c_str(), L": ", L"", kBuilding.getRiverPlotYieldChangeArray());

	setYieldChangeHelp(szBuffer, gDLL->getText("TXT_KEY_BUILDING_WATER_PLOTS_ALL_CITIES").c_str(), L": ", L"", kBuilding.getGlobalSeaPlotYieldChangeArray());

	setYieldChangeHelp(szBuffer, L"", L"", gDLL->getText("TXT_KEY_BUILDING_WITH_POWER").c_str(), kBuilding.getPowerYieldModifierArray(), true);

	setYieldChangeHelp(szBuffer, L"", L"", gDLL->getText("TXT_KEY_BUILDING_ALL_CITIES_THIS_CONTINENT").c_str(), kBuilding.getAreaYieldModifierArray(), true);

	setYieldChangeHelp(szBuffer, L"", L"", gDLL->getText("TXT_KEY_BUILDING_ALL_CITIES").c_str(), kBuilding.getGlobalYieldModifierArray(), true);

	setCommerceChangeHelp(szBuffer, L"", L"", gDLL->getText("TXT_KEY_BUILDING_ALL_CITIES").c_str(), kBuilding.getGlobalCommerceModifierArray(), true);

	setCommerceChangeHelp(szBuffer, L"", L"", gDLL->getText("TXT_KEY_BUILDING_PER_SPECIALIST_ALL_CITIES").c_str(), kBuilding.getSpecialistExtraCommerceArray());

	szTempBuffer = gDLL->getText("TXT_KEY_BUILDING_STATE_REL_BUILDINGS");

	setCommerceChangeHelp(szBuffer, L"", L"", szTempBuffer, kBuilding.getStateReligionCommerceArray());

	for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
		const CvCommerceInfo& kCommerce = GC.getCommerceInfo(eCommerce);
		int iCommerceHappiness = kBuilding.getCommerceHappiness(eCommerce);
		if (iCommerceHappiness != 0) {
			szBuffer.append(NEWLINE);
			// Use absolute value with unhappy face
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_PER_LEVEL", iCommerceHappiness > 0 ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR), abs(100 / iCommerceHappiness), kCommerce.getChar()));
		}

		if (kBuilding.isCommerceFlexible(eCommerce) != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_ADJUST_COMM_RATE", kCommerce.getChar()));
		}
	}

	if (kBuilding.isAnySpecialistYieldChange()) {
		for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
			szFirstBuffer = gDLL->getText("TXT_KEY_BUILDING_FROM_IN_ALL_CITIES", GC.getSpecialistInfo(eSpecialist).getTextKeyWide());
			setYieldChangeHelp(szBuffer, L"", L"", szFirstBuffer, kBuilding.getSpecialistYieldChangeArray(eSpecialist));
		}
	}

	if (kBuilding.isAnyBonusYieldModifier()) {
		for (BonusTypes eBonus = (BonusTypes)0; eBonus < GC.getNumBonusInfos(); eBonus = (BonusTypes)(eBonus + 1)) {
			szFirstBuffer = gDLL->getText("TXT_KEY_BUILDING_WITH_BONUS", GC.getBonusInfo(eBonus).getTextKeyWide());
			setYieldChangeHelp(szBuffer, L"", L"", szFirstBuffer, kBuilding.getBonusYieldModifierArray(eBonus), true);
		}
	}

	for (ReligionTypes eReligion = (ReligionTypes)0; eReligion < GC.getNumReligionInfos(); eReligion = (ReligionTypes)(eReligion + 1)) {
		if (kBuilding.getReligionChange(eReligion) > 0) {
			szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_SPREADS_RELIGION", GC.getReligionInfo(eReligion).getChar()).c_str());
			szBuffer.append(szTempBuffer);
		}
	}

	for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
		const CvSpecialistInfo& kSpecialist = GC.getSpecialistInfo(eSpecialist);
		int iSpecialistCount = kBuilding.getSpecialistCount(eSpecialist);
		if (iSpecialistCount > 0) {
			if (iSpecialistCount == 1) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_TURN_CITIZEN_INTO", kSpecialist.getTextKeyWide()));
			} else {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_TURN_CITIZENS_INTO", iSpecialistCount, kSpecialist.getTextKeyWide()));
			}
		}

		int iFreeSpecialistCount = kBuilding.getFreeSpecialistCount(eSpecialist);
		if (iFreeSpecialistCount > 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_FREE_SPECIALIST", iFreeSpecialistCount, kSpecialist.getTextKeyWide()));
		}
	}

	int iLast = 0;
	for (ImprovementTypes eImprovement = (ImprovementTypes)0; eImprovement < GC.getNumImprovementInfos(); eImprovement = (ImprovementTypes)(eImprovement + 1)) {
		int iImprovementFreeSpecialist = kBuilding.getImprovementFreeSpecialist(eImprovement);
		if (iImprovementFreeSpecialist > 0) {
			szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_IMPROVEMENT_FREE_SPECIALISTS", iImprovementFreeSpecialist).GetCString());
			szTempBuffer.Format(L"<link=literal>%s</link>", GC.getImprovementInfo(eImprovement).getDescription());
			setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", (iImprovementFreeSpecialist != iLast));
			iLast = iImprovementFreeSpecialist;
		}
	}

	iLast = 0;
	for (BonusTypes eBonus = (BonusTypes)0; eBonus < GC.getNumBonusInfos(); eBonus = (BonusTypes)(eBonus + 1)) {
		int iChange = kBuilding.getBonusHealthChanges(eBonus);
		if (iChange != 0) {
			szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_HEALTH_HAPPINESS_CHANGE", abs(iChange), iChange > 0 ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR)).c_str());
			szTempBuffer.Format(L"<link=literal>%s</link>", GC.getBonusInfo(eBonus).getDescription());
			setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", (iChange != iLast));
			iLast = iChange;
		}
	}

	iLast = 0;
	for (CivicTypes eCivic = (CivicTypes)0; eCivic < GC.getNumCivicInfos(); eCivic = (CivicTypes)(eCivic + 1)) {
		int iChange = GC.getCivicInfo(eCivic).getBuildingHealthChanges(kBuilding.getBuildingClassType());
		if (0 != iChange) {
			szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_CIVIC_HEALTH_HAPPINESS_CHANGE", abs(iChange), ((iChange > 0) ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR))).c_str());
			szTempBuffer.Format(L"<link=literal>%s</link>", GC.getCivicInfo(eCivic).getDescription());
			setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", (iChange != iLast));
			iLast = iChange;
		}
	}

	iLast = 0;
	for (BonusTypes eBonus = (BonusTypes)0; eBonus < GC.getNumBonusInfos(); eBonus = (BonusTypes)(eBonus + 1)) {
		int iChange = kBuilding.getBonusHappinessChanges(eBonus);
		if (iChange != 0) {
			szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_HEALTH_HAPPINESS_CHANGE", abs(iChange), (iChange > 0 ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR))).c_str());
			szTempBuffer.Format(L"<link=literal>%s</link>", GC.getBonusInfo(eBonus).getDescription());
			setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", (iChange != iLast));
			iLast = iChange;
		}
	}

	iLast = 0;
	for (CivicTypes eCivic = (CivicTypes)0; eCivic < GC.getNumCivicInfos(); eCivic = (CivicTypes)(eCivic + 1)) {
		int iChange = GC.getCivicInfo(eCivic).getBuildingHappinessChanges(kBuilding.getBuildingClassType());
		if (0 != iChange) {
			szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_CIVIC_HEALTH_HAPPINESS_CHANGE", abs(iChange), ((iChange > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR))).c_str());
			szTempBuffer.Format(L"<link=literal>%s</link>", GC.getCivicInfo(eCivic).getDescription());
			setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", (iChange != iLast));
			iLast = iChange;
		}
	}

	for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
		if (kBuilding.getUnitCombatFreeExperience(eUnitCombat) != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_FREE_XP", GC.getUnitCombatInfo(eUnitCombat).getTextKeyWide(), kBuilding.getUnitCombatFreeExperience(eUnitCombat)));
		}
	}

	for (DomainTypes eDomain = (DomainTypes)0; eDomain < NUM_DOMAIN_TYPES; eDomain = (DomainTypes)(eDomain + 1)) {
		if (kBuilding.getDomainFreeExperience(eDomain) != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_FREE_XP", GC.getDomainInfo(eDomain).getTextKeyWide(), kBuilding.getDomainFreeExperience(eDomain)));
		}
	}

	for (DomainTypes eDomain = (DomainTypes)0; eDomain < NUM_DOMAIN_TYPES; eDomain = (DomainTypes)(eDomain + 1)) {
		if (kBuilding.getDomainProductionModifier(eDomain) != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_BUILDS_FASTER_DOMAIN", GC.getDomainInfo(eDomain).getTextKeyWide(), kBuilding.getDomainProductionModifier(eDomain)));
		}
	}

	bool bFirst = true;
	for (BuildingClassTypes eLoopBuildingClass = (BuildingClassTypes)0; eLoopBuildingClass < GC.getNumBuildingClassInfos(); eLoopBuildingClass = (BuildingClassTypes)(eLoopBuildingClass + 1)) {
		BuildingTypes eLoopBuilding = NO_BUILDING;
		if (ePlayer != NO_PLAYER) {
			eLoopBuilding = ((BuildingTypes)(GC.getCivilizationInfo(GET_PLAYER(ePlayer).getCivilizationType()).getCivilizationBuildings(eLoopBuildingClass)));
		} else {
			eLoopBuilding = (BuildingTypes)GC.getBuildingClassInfo(eLoopBuildingClass).getDefaultBuildingIndex();
		}

		if (eLoopBuilding != NO_BUILDING) {
			CvBuildingInfo& kLoopBuilding = GC.getBuildingInfo(eLoopBuilding);
			if (kBuilding.isReplacedByBuildingClass(kLoopBuilding.getBuildingClassType())) {
				if ((pCity == NULL) || (pCity->getNumBuilding(eLoopBuilding) == 0)) {
					szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_REPLACED_BY_BUILDING").c_str());
					szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), kLoopBuilding.getDescription());
					setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", bFirst);
					bFirst = false;
				}
			}
		}
	}

	bFirst = true;
	for (BuildingClassTypes eLoopBuildingClass = (BuildingClassTypes)0; eLoopBuildingClass < GC.getNumBuildingClassInfos(); eLoopBuildingClass = (BuildingClassTypes)(eLoopBuildingClass + 1)) {
		BuildingTypes eLoopBuilding = NO_BUILDING;
		if (ePlayer != NO_PLAYER) {
			eLoopBuilding = ((BuildingTypes)(GC.getCivilizationInfo(GET_PLAYER(ePlayer).getCivilizationType()).getCivilizationBuildings(eLoopBuildingClass)));
		} else {
			eLoopBuilding = (BuildingTypes)GC.getBuildingClassInfo(eLoopBuildingClass).getDefaultBuildingIndex();
		}

		if (eLoopBuilding != NO_BUILDING) {
			CvBuildingInfo& kLoopBuilding = GC.getBuildingInfo(eLoopBuilding);
			if (kLoopBuilding.isReplacedByBuildingClass(kBuilding.getBuildingClassType())) {
				if ((pCity == NULL) || (pCity->getNumBuilding(eBuilding) == 0)) {
					szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_REPLACES_BUILDING").c_str());
					szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), kLoopBuilding.getDescription());
					setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", bFirst);
					bFirst = false;
				}
			}
		}
	}

	bFirst = true;
	for (UnitTypes eUnit = (UnitTypes)0; eUnit < GC.getNumUnitInfos(); eUnit = (UnitTypes)(eUnit + 1)) {
		const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);
		if (kUnit.getPrereqBuilding() == eBuilding) {
			szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_REQUIRED_TO_TRAIN").c_str());
			szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_UNIT_TEXT"), kUnit.getDescription());
			setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
	}

	bFirst = true;
	for (UnitTypes eUnit = (UnitTypes)0; eUnit < GC.getNumUnitInfos(); eUnit = (UnitTypes)(eUnit + 1)) {
		const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);
		if (kUnit.getBuildings(eBuilding) || kUnit.getForceBuildings(eBuilding)) {
			szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_UNIT_REQUIRED_TO_BUILD").c_str());
			szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_UNIT_TEXT"), kUnit.getDescription());
			setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
	}

	iLast = 0;
	for (BuildingClassTypes eLoopBuildingClass = (BuildingClassTypes)0; eLoopBuildingClass < GC.getNumBuildingClassInfos(); eLoopBuildingClass = (BuildingClassTypes)(eLoopBuildingClass + 1)) {
		const CvBuildingClassInfo& kBuildingClass = GC.getBuildingClassInfo(eLoopBuildingClass);
		int iChange = kBuilding.getBuildingHappinessChanges(eLoopBuildingClass);
		if (iChange != 0) {
			// Use absolute value with unhappy face
			szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_HAPPINESS_CHANGE", abs(iChange), (iChange > 0 ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR))).c_str());
			CvWString szBuilding;
			if (NO_PLAYER != ePlayer) {
				BuildingTypes ePlayerBuilding = ((BuildingTypes)(GC.getCivilizationInfo(kPlayer.getCivilizationType()).getCivilizationBuildings(eLoopBuildingClass)));
				if (NO_BUILDING != ePlayerBuilding) {
					szBuilding.Format(L"<link=literal>%s</link>", kBuildingClass.getDescription());
				}
			} else {
				szBuilding.Format(L"<link=literal>%s</link>", kBuildingClass.getDescription());
			}
			setListHelp(szBuffer, szTempBuffer, szBuilding, L", ", (iChange != iLast));
			iLast = iChange;
		}
	}

	if (kBuilding.getPowerBonus() != NO_BONUS) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_PROVIDES_POWER_WITH", GC.getBonusInfo((BonusTypes)kBuilding.getPowerBonus()).getTextKeyWide()));

		// K-Mod. Also include base health change from power.
		int iPowerHealth = GC.getDefineINT("POWER_HEALTH_CHANGE") + (kBuilding.isDirtyPower() ? GC.getDefineINT("DIRTY_POWER_HEALTH_CHANGE") : 0);
		if (iPowerHealth) {
			szTempBuffer.Format(L" (+%d%c)", abs(iPowerHealth), iPowerHealth > 0 ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR));
			szBuffer.append(szTempBuffer);
		}
	}

	if (bCivilopediaText) {
		// Trait
		for (TraitTypes eTrait = (TraitTypes)0; eTrait < GC.getNumTraitInfos(); eTrait = (TraitTypes)(eTrait + 1)) {
			const CvTraitInfo& kTrait = GC.getTraitInfo(eTrait);
			int iProductionTrait = kBuilding.getProductionTraits(eTrait);
			if (iProductionTrait != 0) {
				if (iProductionTrait == 100) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_DOUBLE_SPEED_TRAIT", kTrait.getTextKeyWide()));
				} else {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_PRODUCTION_MODIFIER_TRAIT", iProductionTrait, kTrait.getTextKeyWide()));
				}
			}
		}

		for (TraitTypes eTrait = (TraitTypes)0; eTrait < GC.getNumTraitInfos(); eTrait = (TraitTypes)(eTrait + 1)) {
			if (kBuilding.getHappinessTraits(eTrait) != 0) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_HAPPINESS_TRAIT", kBuilding.getHappinessTraits(eTrait), GC.getTraitInfo(eTrait).getTextKeyWide()));
			}
		}
	}

	if (bCivilopediaText) {
		if (kBuilding.getGreatPeopleUnitClass() != NO_UNITCLASS) {
			UnitTypes eGreatPeopleUnit;
			if (ePlayer != NO_PLAYER) {
				eGreatPeopleUnit = ((UnitTypes)(GC.getCivilizationInfo(kPlayer.getCivilizationType()).getCivilizationUnits(kBuilding.getGreatPeopleUnitClass())));
			} else {
				eGreatPeopleUnit = (UnitTypes)GC.getUnitClassInfo((UnitClassTypes)kBuilding.getGreatPeopleUnitClass()).getDefaultUnitIndex();
			}

			if (eGreatPeopleUnit != NO_UNIT) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_LIKELY_TO_GENERATE", GC.getUnitInfo(eGreatPeopleUnit).getTextKeyWide()));
			}
		}

		if (kBuilding.getFreeStartEra() != NO_ERA) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_FREE_START_ERA", GC.getEraInfo((EraTypes)kBuilding.getFreeStartEra()).getTextKeyWide()));
		}
	}

	if (!CvWString(kBuilding.getHelp()).empty()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(kBuilding.getHelp());
	}

	buildBuildingRequiresString(szBuffer, eBuilding, bCivilopediaText, bTechChooserText, pCity);

	if (pCity != NULL) {
		BuildingClassTypes eBuildClass = (BuildingClassTypes)kBuilding.getBuildingClassType();
		if (!(GC.getBuildingClassInfo(eBuildClass).isNoLimit())) {
			if (isWorldWonderClass(eBuildClass)) {
				if (pCity->isWorldWondersMaxed()) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_WORLD_WONDERS_PER_CITY", GC.getDefineINT("MAX_WORLD_WONDERS_PER_CITY")));
				}
			} else if (isTeamWonderClass(eBuildClass)) {
				if (pCity->isTeamWondersMaxed()) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_TEAM_WONDERS_PER_CITY", GC.getDefineINT("MAX_TEAM_WONDERS_PER_CITY")));
				}
			} else if (isNationalWonderClass(eBuildClass)) {
				if (pCity->isNationalWondersMaxed()) {
					int iMaxNumWonders = (GC.getGameINLINE().isOption(GAMEOPTION_ONE_CITY_CHALLENGE) && GET_PLAYER(pCity->getOwnerINLINE()).isHuman()) ? GC.getDefineINT("MAX_NATIONAL_WONDERS_PER_CITY_FOR_OCC") : GC.getDefineINT("MAX_NATIONAL_WONDERS_PER_CITY");
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_NATIONAL_WONDERS_PER_CITY", iMaxNumWonders));
				}
			} else {
				if (pCity->isBuildingsMaxed()) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_NUM_PER_CITY", GC.getDefineINT("MAX_BUILDINGS_PER_CITY")));
				}
			}
		}
	}

	if ((pCity == NULL) || pCity->getNumRealBuilding(eBuilding) < GC.getCITY_MAX_NUM_BUILDINGS()) {
		if (!bCivilopediaText) {
			if (pCity == NULL) {
				if (kBuilding.getProductionCost() > 0) {
					szTempBuffer.Format(L"\n%d%c", (ePlayer != NO_PLAYER ? kPlayer.getProductionNeeded(eBuilding) : kBuilding.getProductionCost()), GC.getYieldInfo(YIELD_PRODUCTION).getChar());
					szBuffer.append(szTempBuffer);
				}
			} else {
				if (bActual && (GC.altKey() || getOptionBOOL("MiscHover__BuildingActualEffects", true)) && (pCity->getOwnerINLINE() == GC.getGame().getActivePlayer() || gDLL->getChtLvl() > 0)) {
					setBuildingNetEffectsHelp(szBuffer, eBuilding, pCity);
				}

				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_NUM_TURNS", pCity->getProductionTurnsLeft(eBuilding, ((GC.ctrlKey() || !(GC.shiftKey())) ? 0 : pCity->getOrderQueueLength()))));

				int iProduction = pCity->getBuildingProduction(eBuilding);

				int iProductionNeeded = pCity->getProductionNeeded(eBuilding);
				if (iProduction > 0) {
					szTempBuffer.Format(L" - %d/%d%c", iProduction, iProductionNeeded, GC.getYieldInfo(YIELD_PRODUCTION).getChar());
					szBuffer.append(szTempBuffer);
				} else {
					szTempBuffer.Format(L" - %d%c", iProductionNeeded, GC.getYieldInfo(YIELD_PRODUCTION).getChar());
					szBuffer.append(szTempBuffer);
				}
			}
		}

		for (BonusTypes eBonus = (BonusTypes)0; eBonus < GC.getNumBonusInfos(); eBonus = (BonusTypes)(eBonus + 1)) {
			const CvBonusInfo& kBonus = GC.getBonusInfo(eBonus);
			int iModifier = kBuilding.getBonusProductionModifier(eBonus);
			if (iModifier != 0) {
				if (pCity != NULL) {
					if (pCity->hasBonus(eBonus)) {
						szBuffer.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));
					} else {
						szBuffer.append(gDLL->getText("TXT_KEY_COLOR_NEGATIVE"));
					}
				}
				if (!bCivilopediaText) {
					szBuffer.append(L" (");
				} else {
					szTempBuffer.Format(L"\n%c", gDLL->getSymbolID(BULLET_CHAR), szTempBuffer);
					szBuffer.append(szTempBuffer);
				}
				if (iModifier == 100) {
					szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_DOUBLE_SPEED_WITH", kBonus.getTextKeyWide()));
				} else {
					szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_BUILDS_FASTER_WITH", iModifier, kBonus.getTextKeyWide()));
				}
				if (!bCivilopediaText) {
					szBuffer.append(L')');
				}
				if (pCity != NULL) {
					szBuffer.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
				}
			}
		}

		if (kBuilding.getObsoleteTech() != NO_TECH) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_OBSOLETE_WITH", GC.getTechInfo((TechTypes)kBuilding.getObsoleteTech()).getTextKeyWide()));

			if (kBuilding.getDefenseModifier() != 0 || kBuilding.getBombardDefenseModifier() != 0) {
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_OBSOLETE_EXCEPT"));
			}
		}

		if (kBuilding.getSpecialBuildingType() != NO_SPECIALBUILDING) {
			if (GC.getSpecialBuildingInfo((SpecialBuildingTypes)kBuilding.getSpecialBuildingType()).getObsoleteTech() != NO_TECH) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_OBSOLETE_WITH", GC.getTechInfo((TechTypes)GC.getSpecialBuildingInfo((SpecialBuildingTypes)kBuilding.getSpecialBuildingType()).getObsoleteTech()).getTextKeyWide()));
			}
		}
	}

	// K-Mod. Moved from inside that }, above.
	if (pCity && gDLL->getChtLvl() > 0 && GC.ctrlKey()) {
		int iBuildingValue = pCity->AI_buildingValue(eBuilding, 0, 0, true);
		szBuffer.append(CvWString::format(L"\nAI Building Value = %d", iBuildingValue));
	}
	//

	if (bStrategyText) {
		if (!CvWString(kBuilding.getStrategy()).empty()) {
			if ((ePlayer == NO_PLAYER) || kPlayer.isOption(PLAYEROPTION_ADVISOR_HELP)) {
				szBuffer.append(SEPARATOR);
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_SIDS_TIPS"));
				szBuffer.append(L'\"');
				szBuffer.append(kBuilding.getStrategy());
				szBuffer.append(L'\"');
			}
		}
	}

	if (bCivilopediaText) {
		if (eDefaultBuilding == eBuilding) {
			for (BuildingTypes eLoopBuilding = (BuildingTypes)0; eLoopBuilding < GC.getNumBuildingInfos(); eLoopBuilding = (BuildingTypes)(eLoopBuilding + 1)) {
				if (eLoopBuilding != eBuilding) {
					const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eLoopBuilding);
					if (eBuildingClass == kBuilding.getBuildingClassType()) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_REPLACED_BY_BUILDING", kBuilding.getTextKeyWide()));
					}
				}
			}
		}
	}
}

void CvGameTextMgr::buildBuildingRequiresString(CvWStringBuffer& szBuffer, BuildingTypes eBuilding, bool bCivilopediaText, bool bTechChooserText, const CvCity* pCity) {
	// K-mod note. I've made a couple of style adjustments throughout this function to make it easier for me to read & maintain.

	const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);

	PlayerTypes ePlayer = pCity ? pCity->getOwnerINLINE() : GC.getGameINLINE().getActivePlayer();
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);

	if (NULL == pCity || !pCity->canConstruct(eBuilding)) {
		if (kBuilding.getHolyCity() != NO_RELIGION) {
			if (pCity == NULL || !pCity->isHolyCity((ReligionTypes)(kBuilding.getHolyCity()))) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_ACTION_ONLY_HOLY_CONSTRUCT", GC.getReligionInfo((ReligionTypes)kBuilding.getHolyCity()).getChar()));
			}
		}

		bool bFirst = true;

		if (kBuilding.getSpecialBuildingType() != NO_SPECIALBUILDING) {
			if ((pCity == NULL) || !(GC.getGameINLINE().isSpecialBuildingValid((SpecialBuildingTypes)(kBuilding.getSpecialBuildingType())))) {
				for (ProjectTypes eProject = (ProjectTypes)0; eProject < GC.getNumProjectInfos(); eProject = (ProjectTypes)(eProject + 1)) {
					const CvProjectInfo& kProject = GC.getProjectInfo(eProject);
					if (kProject.getEveryoneSpecialBuilding() == kBuilding.getSpecialBuildingType()) {
						CvWString szTempBuffer;
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						CvWString szProject;
						szProject.Format(L"<link=literal>%s</link>", kProject.getDescription());
						setListHelp(szBuffer, szTempBuffer, szProject, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					}
				}
			}
		}

		if (!bFirst) {
			szBuffer.append(ENDCOLR);
		}

		if (kBuilding.getSpecialBuildingType() != NO_SPECIALBUILDING) {
			if ((pCity == NULL) || !(GC.getGameINLINE().isSpecialBuildingValid((SpecialBuildingTypes)(kBuilding.getSpecialBuildingType())))) {
				TechTypes eTech = (TechTypes)GC.getSpecialBuildingInfo((SpecialBuildingTypes)kBuilding.getSpecialBuildingType()).getTechPrereqAnyone();
				if (NO_TECH != eTech) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_REQUIRES_TECH_ANYONE", GC.getTechInfo(eTech).getTextKeyWide()));
				}
			}
		}

		for (BuildingClassTypes eBuildingClass = (BuildingClassTypes)0; eBuildingClass < GC.getNumBuildingClassInfos(); eBuildingClass = (BuildingClassTypes)(eBuildingClass + 1)) {
			// EF: show "Requires Hospital" if "Requires Hospital (x/5)" requirement has been met
			bool bShowedPrereq = false;

			// K-Mod note: I've rearranged the conditions in the following block. Originally it was something like this:
			//if (ePlayer == NO_PLAYER && kBuilding.getPrereqNumOfBuildingClass((BuildingClassTypes)iI) > 0)
			//else if (ePlayer != NO_PLAYER && kPlayer.getBuildingClassPrereqBuilding(eBuilding, ((BuildingClassTypes)iI)) > 0)
			//

			// K-Mod. Check that we can actually build this class of building. (Previously this was checked in every single block below.)
			BuildingTypes eLoopBuilding = (BuildingTypes)(ePlayer == NO_PLAYER ? GC.getBuildingClassInfo(eBuildingClass).getDefaultBuildingIndex() : GC.getCivilizationInfo(kPlayer.getCivilizationType()).getCivilizationBuildings(eBuildingClass));
			if (eLoopBuilding == NO_BUILDING)
				continue;

			const CvBuildingInfo& kLoopBuilding = GC.getBuildingInfo(eLoopBuilding);
			if (kBuilding.getPrereqNumOfBuildingClass(eBuildingClass) > 0) {
				if (ePlayer == NO_PLAYER) {
					CvWString szTempBuffer;
					szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_REQUIRES_NUM_SPECIAL_BUILDINGS_NO_CITY", kLoopBuilding.getTextKeyWide(), kBuilding.getPrereqNumOfBuildingClass(eBuildingClass)).c_str());

					szBuffer.append(szTempBuffer);
					bShowedPrereq = true;
				} else {
					// K-Mod. In the city screen, include in the prereqs calculation the number of eBuilding under construction.
					// But in the civilopedia, don't even include the number we have already built!
					int iNeeded = kPlayer.getBuildingClassPrereqBuilding(eBuilding, eBuildingClass, bCivilopediaText ? -kPlayer.getBuildingClassCount((BuildingClassTypes)kBuilding.getBuildingClassType()) : kPlayer.getBuildingClassMaking((BuildingClassTypes)kBuilding.getBuildingClassType()));
					FAssert(iNeeded > 0);

					CvWString szTempBuffer;
					if (pCity != NULL) {
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_REQUIRES_NUM_SPECIAL_BUILDINGS", kLoopBuilding.getTextKeyWide(), kPlayer.getBuildingClassCount(eBuildingClass), iNeeded).c_str()); // K-Mod
					} else {
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_REQUIRES_NUM_SPECIAL_BUILDINGS_NO_CITY", kLoopBuilding.getTextKeyWide(), iNeeded).c_str()); // K-Mod
					}

					szBuffer.append(szTempBuffer);
					bShowedPrereq = true;
				}
			}
		}

		bFirst = true;
		for (int iIndex = 1; iIndex < kBuilding.getNumPrereqWorldViews(); ++iIndex) {
			if (kBuilding.getPrereqWorldView(iIndex) != NO_WORLD_VIEW) {
				if (bTechChooserText || ePlayer == NO_PLAYER || !kPlayer.isWorldViewActivated((WorldViewTypes)iIndex)) {
					CvWString szTempBuffer;
					szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
					setListHelp(szBuffer, szTempBuffer, GC.getWorldViewInfo((WorldViewTypes)kBuilding.getPrereqWorldView(iIndex)).getDescription(), gDLL->getText("TXT_KEY_AND").c_str(), bFirst);
					bFirst = false;
				}
			}
		}
		if (!bFirst) {
			szBuffer.append(ENDCOLR);
		}

		if (kBuilding.isStateReligion()) {
			if (NULL == pCity || NO_PLAYER == ePlayer || NO_RELIGION == kPlayer.getStateReligion() || !pCity->isHasReligion(kPlayer.getStateReligion())) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_REQUIRES_STATE_RELIGION"));
			}
		}

		if (kBuilding.getNumCitiesPrereq() > 0) {
			if (NO_PLAYER == ePlayer || kPlayer.getNumCities() < kBuilding.getNumCitiesPrereq()) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_REQUIRES_NUM_CITIES", kBuilding.getNumCitiesPrereq()));
			}
		}

		if (kBuilding.getUnitLevelPrereq() > 0) {
			if (NO_PLAYER == ePlayer || kPlayer.getHighestUnitLevel() < kBuilding.getUnitLevelPrereq()) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_REQUIRES_UNIT_LEVEL", kBuilding.getUnitLevelPrereq()));
			}
		}

		if (kBuilding.getMinLatitude() > 0) {
			if (NULL == pCity || pCity->plot()->getLatitude() < kBuilding.getMinLatitude()) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_MIN_LATITUDE", kBuilding.getMinLatitude()));
			}
		}

		if (kBuilding.getMaxLatitude() < 90) {
			if (NULL == pCity || pCity->plot()->getLatitude() > kBuilding.getMaxLatitude()) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_MAX_LATITUDE", kBuilding.getMaxLatitude()));
			}
		}

		if (kBuilding.getMinPopulation() > 0) {
			if (pCity == NULL || pCity->getPopulation() < kBuilding.getMinPopulation()) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_REQUIRES_POPULATION", kBuilding.getMinPopulation()));
			}
		}

		if (kBuilding.getMinCultureLevel() != NO_CULTURELEVEL) {
			if (pCity == NULL || pCity->getCultureLevel() < kBuilding.getMinCultureLevel()) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_REQUIRES_CULTURE", GC.getCultureLevelInfo(kBuilding.getMinCultureLevel()).getDescription()));
			}
		}

		if (kBuilding.isPrereqPower()) {
			if (pCity == NULL || !pCity->isPower()) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_REQUIRES_POWER"));
			}
		}

		if (pCity != NULL) {
			if (GC.getGameINLINE().isNoNukes()) {
				if (kBuilding.isAllowsNukes()) {
					for (UnitTypes eUnit = (UnitTypes)0; eUnit < GC.getNumUnitInfos(); eUnit = (UnitTypes)(eUnit + 1)) {
						if (GC.getUnitInfo(eUnit).getNukeRange() != -1) {
							szBuffer.append(NEWLINE);
							szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_NO_NUKES"));
							break;
						}
					}
				}
			}
		}

		if (bCivilopediaText) {
			if (kBuilding.getVictoryPrereq() != NO_VICTORY) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_REQUIRES_VICTORY", GC.getVictoryInfo((VictoryTypes)(kBuilding.getVictoryPrereq())).getTextKeyWide()));
			}

			if (kBuilding.getMaxStartEra() != NO_ERA) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_MAX_START_ERA", GC.getEraInfo((EraTypes)kBuilding.getMaxStartEra()).getTextKeyWide()));
			}

			if (kBuilding.getNumTeamsPrereq() > 0) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_REQUIRES_NUM_TEAMS", kBuilding.getNumTeamsPrereq()));
			}
		} else {
			if (!bTechChooserText) {
				if (kBuilding.getNumPrereqAndTechs() > 0) {
					if (ePlayer == NO_PLAYER || !(GET_TEAM(kPlayer.getTeam()).isHasTech((TechTypes)(kBuilding.getPrereqAndTech(0))))) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_REQUIRES_STRING", GC.getTechInfo((TechTypes)(kBuilding.getPrereqAndTech(0))).getTextKeyWide()));
					}
				}
			}

			bool bFirst = true;
			for (int iI = 1; iI < kBuilding.getNumPrereqAndTechs(); ++iI) {
				if (kBuilding.getPrereqAndTech(iI) != NO_TECH) {
					if (bTechChooserText || ePlayer == NO_PLAYER || !(GET_TEAM(kPlayer.getTeam()).isHasTech((TechTypes)(kBuilding.getPrereqAndTech(iI))))) {
						CvWString szTempBuffer;
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						setListHelp(szBuffer, szTempBuffer, GC.getTechInfo(((TechTypes)(kBuilding.getPrereqAndTech(iI)))).getDescription(), gDLL->getText("TXT_KEY_AND").c_str(), bFirst);
						bFirst = false;
					}
				}
			}

			if (!bFirst) {
				szBuffer.append(ENDCOLR);
			}

			if (kBuilding.getPrereqAndBonus() != NO_BONUS) {
				if ((pCity == NULL) || !(pCity->hasBonus((BonusTypes)kBuilding.getPrereqAndBonus()))) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_UNIT_REQUIRES_STRING", GC.getBonusInfo((BonusTypes)kBuilding.getPrereqAndBonus()).getTextKeyWide()));
				}
			}

			CvWStringBuffer szBonusList;
			bFirst = true;

			for (int iI = 0; iI < kBuilding.getNumPrereqOrBonuses(); ++iI) {
				if (kBuilding.getPrereqOrBonuses(iI) != NO_BONUS) {
					if ((pCity == NULL) || !(pCity->hasBonus((BonusTypes)kBuilding.getPrereqOrBonuses(iI)))) {
						CvWString szTempBuffer;
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						setListHelp(szBonusList, szTempBuffer, GC.getBonusInfo((BonusTypes)kBuilding.getPrereqOrBonuses(iI)).getDescription(), gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					} else if (NULL != pCity) {
						bFirst = true;
						break;
					}
				}
			}

			if (!bFirst) {
				szBonusList.append(ENDCOLR);
				szBuffer.append(szBonusList);
			}

			// AND Civics
			bFirst = true;
			bool bValid = true;
			for (int iI = 0; iI < kBuilding.getNumPrereqAndCivics() && bValid; ++iI) {
				CivicTypes eCivic = (CivicTypes)kBuilding.getPrereqAndCivic(iI);
				if ((GC.getGameINLINE().getActivePlayer() == NO_PLAYER) || !(GET_PLAYER(GC.getGameINLINE().getActivePlayer()).isCivic(eCivic))) {
					bValid = false;
				}
			}
			if (!bValid) {
				for (int iI = 0; iI < kBuilding.getNumPrereqAndCivics() && bValid; ++iI) {
					CivicTypes eCivic = (CivicTypes)kBuilding.getPrereqAndCivic(iI);
					if ((GC.getGameINLINE().getActivePlayer() == NO_PLAYER) || !(GET_PLAYER(GC.getGameINLINE().getActivePlayer()).isCivic(eCivic))) {
						CvWString szTempBuffer;
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						CvWString szCivic;
						szCivic.Format(L"<link=literal>%s</link>", GC.getCivicInfo(eCivic).getDescription());
						setListHelp(szBuffer, szTempBuffer, szCivic, gDLL->getText("TXT_KEY_AND").c_str(), bFirst);
						bFirst = false;
					}
				}
			}

			// OR Civics
			bFirst = true;
			bValid = false;
			if (pCity != NULL) {
				for (int iI = 0; iI < kBuilding.getNumPrereqOrCivics() && !bValid; ++iI) {
					CivicTypes eCivic = (CivicTypes)kBuilding.getPrereqOrCivic(iI);
					if ((GC.getGameINLINE().getActivePlayer() == NO_PLAYER) || !(GET_PLAYER(GC.getGameINLINE().getActivePlayer()).isCivic(eCivic))) {
						bValid = true;
						break;
					}
				}
			}
			if (!bValid) {
				for (int iI = 0; iI < kBuilding.getNumPrereqOrCivics() && !bValid; ++iI) {
					CivicTypes eCivic = (CivicTypes)kBuilding.getPrereqOrCivic(iI);
					if ((GC.getGameINLINE().getActivePlayer() == NO_PLAYER) || !(GET_PLAYER(GC.getGameINLINE().getActivePlayer()).isCivic(eCivic))) {
						CvWString szTempBuffer;
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						CvWString szCivic;
						szCivic.Format(L"<link=literal>%s</link>", GC.getCivicInfo(eCivic).getDescription());
						setListHelp(szBuffer, szTempBuffer, szCivic, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					}
				}
			}

			// OR Terrain
			bFirst = true;
			bValid = false;
			if (pCity != NULL) {
				for (int iI = 0; iI < kBuilding.getNumPrereqOrTerrains(); ++iI) {
					if (pCity->hasVicinityTerrain((TerrainTypes)kBuilding.getPrereqOrTerrain(iI))) {
						bValid = true;
						break;
					}
				}
			}
			if (!bValid) {
				for (int iI = 0; iI < kBuilding.getNumPrereqOrTerrains(); ++iI) {
					TerrainTypes eTerrain = (TerrainTypes)kBuilding.getPrereqOrTerrain(iI);
					if (pCity == NULL || !pCity->hasVicinityTerrain(eTerrain)) {
						CvWString szTempBuffer;
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						CvWString szTerrain;
						szTerrain.Format(L"<link=literal>%s</link>", GC.getTerrainInfo(eTerrain).getDescription());
						setListHelp(szBuffer, szTempBuffer, szTerrain, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					}
				}
				if (!bFirst) {
					szBuffer.append(gDLL->getText("TXT_KEY_IN_CITY_VICINITY"));
				}
			}

			// AND Terrain
			bFirst = true;
			bValid = false;
			if (pCity != NULL) {
				bValid = true;
				for (int iI = 0; iI < kBuilding.getNumPrereqAndTerrains() && bValid; ++iI) {
					if (!pCity->hasVicinityTerrain((TerrainTypes)kBuilding.getPrereqAndTerrain(iI))) {
						bValid = false;
					}
				}
			}
			if (!bValid) {
				for (int iI = 0; iI < kBuilding.getNumPrereqAndTerrains(); ++iI) {
					TerrainTypes eTerrain = (TerrainTypes)kBuilding.getPrereqAndTerrain(iI);
					if (pCity == NULL || !pCity->hasVicinityTerrain(eTerrain)) {
						CvWString szTempBuffer;
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						CvWString szTerrain;
						szTerrain.Format(L"<link=literal>%s</link>", GC.getTerrainInfo(eTerrain).getDescription());
						setListHelp(szBuffer, szTempBuffer, szTerrain, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					}
				}
				if (!bFirst) {
					szBuffer.append(gDLL->getText("TXT_KEY_IN_CITY_VICINITY"));
				}
			}

			// OR Bonus
			bFirst = true;
			bValid = false;
			if (pCity != NULL) {
				for (int iI = 0; iI < kBuilding.getNumPrereqVicinityOrBonus(); ++iI) {
					if (pCity->hasVicinityBonus((BonusTypes)kBuilding.getPrereqVicinityOrBonus(iI))) {
						bValid = true;
						break;
					}
				}
			}
			if (!bValid) {
				for (int iI = 0; iI < kBuilding.getNumPrereqVicinityOrBonus(); ++iI) {
					BonusTypes eBonus = (BonusTypes)kBuilding.getPrereqVicinityOrBonus(iI);
					if (pCity == NULL || !pCity->hasVicinityBonus(eBonus)) {
						CvWString szTempBuffer;
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						CvWString szBonus;
						szBonus.Format(L"<link=literal>%s</link>", GC.getBonusInfo(eBonus).getDescription());
						setListHelp(szBuffer, szTempBuffer, szBonus, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					}
				}
				if (!bFirst) {
					szBuffer.append(gDLL->getText("TXT_KEY_IN_CITY_VICINITY"));
				}
			}

			// AND Bonus
			bFirst = true;
			bValid = false;
			if (pCity != NULL) {
				bValid = true;
				for (int iI = 0; iI < kBuilding.getNumPrereqVicinityAndBonus() && bValid; ++iI) {
					if (!pCity->hasVicinityBonus((BonusTypes)kBuilding.getPrereqVicinityAndBonus(iI))) {
						bValid = false;
					}
				}
			}
			if (!bValid) {
				for (int iI = 0; iI < kBuilding.getNumPrereqVicinityAndBonus(); ++iI) {
					BonusTypes eBonus = (BonusTypes)kBuilding.getPrereqVicinityAndBonus(iI);
					if (pCity == NULL || !pCity->hasVicinityBonus(eBonus)) {
						CvWString szTempBuffer;
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						CvWString szBonus;
						szBonus.Format(L"<link=literal>%s</link>", GC.getBonusInfo(eBonus).getDescription());
						setListHelp(szBuffer, szTempBuffer, szBonus, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					}
				}
				if (!bFirst) {
					szBuffer.append(gDLL->getText("TXT_KEY_IN_CITY_VICINITY"));
				}
			}

			// AND BuildingClass
			bFirst = true;
			bValid = false;
			if (pCity != NULL) {
				bValid = true;
				for (int iI = 0; iI < kBuilding.getNumPrereqAndBuildingClasses() && bValid; ++iI) {
					BuildingClassTypes eBuildingClass = (BuildingClassTypes)kBuilding.getPrereqAndBuildingClass(iI);
					if (pCity->getNumActiveBuildingClass(eBuildingClass) <= 0) {
						bValid = false;
					}
				}
			}
			if (!bValid) {
				for (int iI = 0; iI < kBuilding.getNumPrereqAndBuildingClasses(); ++iI) {
					BuildingClassTypes eBuildingClass = (BuildingClassTypes)kBuilding.getPrereqAndBuildingClass(iI);
					if (pCity == NULL || !(pCity->getNumActiveBuildingClass(eBuildingClass) > 0)) {
						BuildingTypes ePrereqBuilding = (ePlayer != NO_PLAYER) ? (BuildingTypes)(GC.getCivilizationInfo(kPlayer.getCivilizationType()).getCivilizationBuildings(eBuildingClass)) : (BuildingTypes)GC.getBuildingClassInfo(eBuildingClass).getDefaultBuildingIndex();
						CvWString szTempBuffer;
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						CvWString szBuildingClass;
						szBuildingClass.Format(L"<link=literal>%s</link>", GC.getBuildingInfo(ePrereqBuilding).getDescription());
						setListHelp(szBuffer, szTempBuffer, szBuildingClass, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					}
				}
			}

			// OR BuildingClass
			bFirst = true;
			bValid = false;
			if (pCity != NULL) {
				for (int iI = 0; iI < kBuilding.getNumPrereqOrBuildingClasses(); ++iI) {
					if (pCity->getNumActiveBuildingClass((BuildingClassTypes)kBuilding.getPrereqOrBuildingClass(iI)) > 0) {
						bValid = true;
						break;
					}
				}
			}
			if (!bValid) {
				for (int iI = 0; iI < kBuilding.getNumPrereqOrBuildingClasses(); ++iI) {
					BuildingClassTypes eBuildingClass = (BuildingClassTypes)kBuilding.getPrereqOrBuildingClass(iI);
					if (pCity == NULL || !(pCity->getNumActiveBuildingClass(eBuildingClass) > 0)) {
						BuildingTypes ePrereqBuilding = (ePlayer != NO_PLAYER) ? (BuildingTypes)(GC.getCivilizationInfo(kPlayer.getCivilizationType()).getCivilizationBuildings(eBuildingClass)) : (BuildingTypes)GC.getBuildingClassInfo(eBuildingClass).getDefaultBuildingIndex();
						CvWString szTempBuffer;
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						CvWString szBuildingClass;
						szBuildingClass.Format(L"<link=literal>%s</link>", GC.getBuildingInfo(ePrereqBuilding).getDescription());
						setListHelp(szBuffer, szTempBuffer, szBuildingClass, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					}
				}
			}

			// NOT BuildingClass
			bFirst = true;
			bValid = false;
			if (pCity != NULL) {
				for (int iI = 0; iI < kBuilding.getNumPrereqNotBuildingClasses(); ++iI) {
					if (pCity->getNumActiveBuildingClass((BuildingClassTypes)kBuilding.getPrereqNotBuildingClass(iI)) <= 0) {
						bValid = true;
						break;
					}
				}
			}
			if (!bValid) {
				for (int iI = 0; iI < kBuilding.getNumPrereqNotBuildingClasses(); ++iI) {
					BuildingClassTypes eBuildingClass = (BuildingClassTypes)kBuilding.getPrereqNotBuildingClass(iI);
					if (pCity == NULL || (pCity->getNumActiveBuildingClass(eBuildingClass) > 0)) {
						BuildingTypes ePrereqBuilding = (ePlayer != NO_PLAYER) ? (BuildingTypes)(GC.getCivilizationInfo(kPlayer.getCivilizationType()).getCivilizationBuildings(eBuildingClass)) : (BuildingTypes)GC.getBuildingClassInfo(eBuildingClass).getDefaultBuildingIndex();
						CvWString szFirstBuffer;
						szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_BUILDING_NOT_REQUIRED_TO_BUILD").c_str());
						CvWString szTempBuffer;
						szTempBuffer.Format(L"<link=literal>%s</link>", GC.getBuildingInfo(ePrereqBuilding).getDescription());
						setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", bFirst);
						bFirst = false;
					}
				}
			}

			// Vicinity Improvement
			bFirst = true;
			bValid = false;
			if (pCity != NULL) {
				for (int iI = 0; iI < kBuilding.getNumPrereqVicinityImprovements(); ++iI) {
					if (pCity->hasVicinityImprovement((ImprovementTypes)kBuilding.getPrereqVicinityImprovement(iI))) {
						bValid = true;
						break;
					}
				}
			}
			if (!bValid) {
				for (int iI = 0; iI < kBuilding.getNumPrereqVicinityImprovements(); ++iI) {
					ImprovementTypes eImprovement = (ImprovementTypes)kBuilding.getPrereqVicinityImprovement(iI);
					if (pCity == NULL || !pCity->hasVicinityImprovement(eImprovement)) {
						CvWString szTempBuffer;
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						CvWString szImprovement;
						szImprovement.Format(L"<link=literal>%s</link>", GC.getImprovementInfo(eImprovement).getDescription());
						setListHelp(szBuffer, szTempBuffer, szImprovement, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					}
				}
				if (!bFirst) {
					szBuffer.append(gDLL->getText("TXT_KEY_IN_CITY_VICINITY"));
				}
			}

			// Vicinity Feature
			bFirst = true;
			bValid = false;
			if (pCity != NULL) {
				for (int iI = 0; iI < kBuilding.getNumPrereqVicinityFeatures(); ++iI) {
					if (pCity->hasVicinityFeature((FeatureTypes)kBuilding.getPrereqVicinityFeature(iI))) {
						bValid = true;
						break;
					}
				}
			}
			if (!bValid) {
				for (int iI = 0; iI < kBuilding.getNumPrereqVicinityFeatures(); ++iI) {
					FeatureTypes eFeature = (FeatureTypes)kBuilding.getPrereqVicinityFeature(iI);
					if (pCity == NULL || !pCity->hasVicinityFeature(eFeature)) {
						CvWString szTempBuffer;
						szTempBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
						CvWString szFeature;
						szFeature.Format(L"<link=literal>%s</link>", GC.getFeatureInfo(eFeature).getDescription());
						setListHelp(szBuffer, szTempBuffer, szFeature, gDLL->getText("TXT_KEY_OR").c_str(), bFirst);
						bFirst = false;
					}
				}
				if (!bFirst) {
					szBuffer.append(gDLL->getText("TXT_KEY_IN_CITY_VICINITY"));
				}
			}

			if (NO_CORPORATION != kBuilding.getFoundsCorporation()) {
				const CvCorporationInfo& kCorp = GC.getCorporationInfo((CorporationTypes)kBuilding.getFoundsCorporation());

				bFirst = true;
				szBonusList.clear();
				for (int iI = 0; iI < kCorp.getNumPrereqBonuses(); ++iI) {
					BonusTypes eBonus = (BonusTypes)kCorp.getPrereqBonus(iI);
					if (NO_BONUS != eBonus) {
						if ((pCity == NULL) || !(pCity->hasBonus(eBonus))) {
							CvWString szTempBuffer, szFirstBuffer;
							szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_REQUIRES").c_str());
							szTempBuffer.Format(L"<link=literal>%s</link>", GC.getBonusInfo(eBonus).getDescription());
							setListHelp(szBonusList, szFirstBuffer, szTempBuffer, gDLL->getText("TXT_KEY_OR"), bFirst);
							bFirst = false;
						} else if (NULL != pCity) {
							bFirst = true;
							break;
						}
					}
				}

				if (!bFirst) {
					szBonusList.append(ENDCOLR);
					szBuffer.append(szBonusList);
				}
			}
		}
	}
}


void CvGameTextMgr::setProjectHelp(CvWStringBuffer& szBuffer, ProjectTypes eProject, bool bCivilopediaText, CvCity* pCity) {
	PROFILE_FUNC();

	CvWString szTempBuffer;
	CvWString szFirstBuffer;

	if (NO_PROJECT == eProject) {
		return;
	}

	const CvProjectInfo& kProject = GC.getProjectInfo(eProject);

	PlayerTypes ePlayer;
	if (pCity != NULL) {
		ePlayer = pCity->getOwnerINLINE();
	} else {
		ePlayer = GC.getGameINLINE().getActivePlayer();
	}

	if (!bCivilopediaText) {
		szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_PROJECT_TEXT"), kProject.getDescription());
		szBuffer.append(szTempBuffer);

		if (isWorldProject(eProject)) {
			if (pCity == NULL) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_WORLD_NUM_ALLOWED", kProject.getMaxGlobalInstances()));
			} else {
				szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_WORLD_NUM_LEFT", (kProject.getMaxGlobalInstances() - GC.getGameINLINE().getProjectCreatedCount(eProject) - GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getProjectMaking(eProject))));
			}
		}

		if (isTeamProject(eProject)) {
			if (pCity == NULL) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_TEAM_NUM_ALLOWED", kProject.getMaxTeamInstances()));
			} else {
				szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_TEAM_NUM_LEFT", (kProject.getMaxTeamInstances() - GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getProjectCount(eProject) - GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getProjectMaking(eProject))));
			}
		}
	}

	if (kProject.getNukeInterception() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_CHANCE_INTERCEPT_NUKES", kProject.getNukeInterception()));
	}

	if (kProject.getTechShare() != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_TECH_SHARE", kProject.getTechShare()));
	}

	if (kProject.isAllowsNukes()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_ENABLES_NUKES"));
	}

	if (kProject.getEveryoneSpecialUnit() != NO_SPECIALUNIT) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_ENABLES_SPECIAL", GC.getSpecialUnitInfo((SpecialUnitTypes)(kProject.getEveryoneSpecialUnit())).getTextKeyWide()));
	}

	if (kProject.getEveryoneSpecialBuilding() != NO_SPECIALBUILDING) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_ENABLES_SPECIAL", GC.getSpecialBuildingInfo((SpecialBuildingTypes)(kProject.getEveryoneSpecialBuilding())).getTextKeyWide()));
	}

	for (int iI = 0; iI < GC.getNumVictoryInfos(); ++iI) {
		if (kProject.getVictoryThreshold(iI) > 0) {
			if (kProject.getVictoryThreshold(iI) == kProject.getVictoryMinThreshold(iI)) {
				szTempBuffer.Format(L"%d", kProject.getVictoryThreshold(iI));
			} else {
				szTempBuffer.Format(L"%d-%d", kProject.getVictoryMinThreshold(iI), kProject.getVictoryThreshold(iI));
			}

			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_REQUIRED_FOR_VICTORY", szTempBuffer.GetCString(), GC.getVictoryInfo((VictoryTypes)iI).getTextKeyWide()));
		}
	}

	bool bFirst = true;
	for (int iI = 0; iI < GC.getNumProjectInfos(); ++iI) {
		if (GC.getProjectInfo((ProjectTypes)iI).getAnyoneProjectPrereq() == eProject) {
			szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_PROJECT_REQUIRED_TO_CREATE_ANYONE").c_str());
			szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_PROJECT_TEXT"), GC.getProjectInfo((ProjectTypes)iI).getDescription());
			setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
	}

	bFirst = true;
	for (int iI = 0; iI < GC.getNumProjectInfos(); ++iI) {
		if (GC.getProjectInfo((ProjectTypes)iI).getProjectsNeeded(eProject) > 0) {
			if ((pCity == NULL) || pCity->canCreate(((ProjectTypes)iI), false, true)) {
				szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_PROJECT_REQUIRED_TO_CREATE").c_str());
				szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_PROJECT_TEXT"), GC.getProjectInfo((ProjectTypes)iI).getDescription());
				setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", bFirst);
				bFirst = false;
			}
		}
	}

	if ((pCity == NULL) || !(pCity->canCreate(eProject))) {
		if (pCity != NULL) {
			if (GC.getGameINLINE().isNoNukes()) {
				if (kProject.isAllowsNukes()) {
					for (int iI = 0; iI < GC.getNumUnitInfos(); ++iI) {
						if (GC.getUnitInfo((UnitTypes)iI).getNukeRange() != -1) {
							szBuffer.append(NEWLINE);
							szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_NO_NUKES"));
							break;
						}
					}
				}
			}
		}

		if (kProject.getAnyoneProjectPrereq() != NO_PROJECT) {
			if ((pCity == NULL) || (GC.getGameINLINE().getProjectCreatedCount((ProjectTypes)(kProject.getAnyoneProjectPrereq())) == 0)) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_REQUIRES_ANYONE", GC.getProjectInfo((ProjectTypes)kProject.getAnyoneProjectPrereq()).getTextKeyWide()));
			}
		}

		for (int iI = 0; iI < GC.getNumProjectInfos(); ++iI) {
			if (kProject.getProjectsNeeded(iI) > 0) {
				if ((pCity == NULL) || (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getProjectCount((ProjectTypes)iI) < kProject.getProjectsNeeded(iI))) {
					if (pCity != NULL) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_REQUIRES", GC.getProjectInfo((ProjectTypes)iI).getTextKeyWide(), GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getProjectCount((ProjectTypes)iI), kProject.getProjectsNeeded(iI)));
					} else {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_REQUIRES_NO_CITY", GC.getProjectInfo((ProjectTypes)iI).getTextKeyWide(), kProject.getProjectsNeeded(iI)));
					}
				}
			}
		}

		if (bCivilopediaText) {
			if (kProject.getVictoryPrereq() != NO_VICTORY) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_REQUIRES_STRING_VICTORY", GC.getVictoryInfo((VictoryTypes)(kProject.getVictoryPrereq())).getTextKeyWide()));
			}
		}
	}

	if (!bCivilopediaText) {
		if (pCity == NULL) {
			if (ePlayer != NO_PLAYER) {
				szTempBuffer.Format(L"\n%d%c", GET_PLAYER(ePlayer).getProductionNeeded(eProject), GC.getYieldInfo(YIELD_PRODUCTION).getChar());
			} else {
				szTempBuffer.Format(L"\n%d%c", kProject.getProductionCost(), GC.getYieldInfo(YIELD_PRODUCTION).getChar());
			}
			szBuffer.append(szTempBuffer);
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_NUM_TURNS", pCity->getProductionTurnsLeft(eProject, ((GC.ctrlKey() || !(GC.shiftKey())) ? 0 : pCity->getOrderQueueLength()))));

			int iProduction = pCity->getProjectProduction(eProject);

			if (iProduction > 0) {
				szTempBuffer.Format(L" - %d/%d%c", iProduction, pCity->getProductionNeeded(eProject), GC.getYieldInfo(YIELD_PRODUCTION).getChar());
			} else {
				szTempBuffer.Format(L" - %d%c", pCity->getProductionNeeded(eProject), GC.getYieldInfo(YIELD_PRODUCTION).getChar());
			}
			szBuffer.append(szTempBuffer);
		}
	}

	for (int iI = 0; iI < GC.getNumBonusInfos(); ++iI) {
		if (kProject.getBonusProductionModifier(iI) != 0) {
			if (pCity != NULL) {
				if (pCity->hasBonus((BonusTypes)iI)) {
					szBuffer.append(gDLL->getText("TXT_KEY_COLOR_POSITIVE"));
				} else {
					szBuffer.append(gDLL->getText("TXT_KEY_COLOR_NEGATIVE"));
				}
			}
			if (!bCivilopediaText) {
				szBuffer.append(L" (");
			} else {
				szTempBuffer.Format(L"%s%c", NEWLINE, gDLL->getSymbolID(BULLET_CHAR), szTempBuffer);
				szBuffer.append(szTempBuffer);
			}
			if (kProject.getBonusProductionModifier(iI) == 100) {
				szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_DOUBLE_SPEED_WITH", GC.getBonusInfo((BonusTypes)iI).getTextKeyWide()));
			} else {
				szBuffer.append(gDLL->getText("TXT_KEY_PROJECT_BUILDS_FASTER_WITH", kProject.getBonusProductionModifier(iI), GC.getBonusInfo((BonusTypes)iI).getTextKeyWide()));
			}
			if (!bCivilopediaText) {
				szBuffer.append(L')');
			}
			if (pCity != NULL) {
				szBuffer.append(gDLL->getText("TXT_KEY_COLOR_REVERT"));
			}
		}
	}

	if (pCity && gDLL->getChtLvl() > 0 && GC.ctrlKey()) {
		int iValue = pCity->AI_projectValue(eProject);
		szBuffer.append(CvWString::format(L"\nProject Value (base) = %d", iValue));

		ProjectTypes eBestProject = ((CvCityAI*)pCity)->AI_bestProject(&iValue);
		if (eBestProject == eProject) {
			szBuffer.append(CvWString::format(SETCOLR L"\n(Best project value (scaled) = %d)" ENDCOLR, TEXT_COLOR("COLOR_LIGHT_GREY"), iValue));
		}
	}
	//
}


void CvGameTextMgr::setProcessHelp(CvWStringBuffer& szBuffer, ProcessTypes eProcess) {
	szBuffer.append(GC.getProcessInfo(eProcess).getDescription());

	for (int iI = 0; iI < NUM_COMMERCE_TYPES; ++iI) {
		if (GC.getProcessInfo(eProcess).getProductionToCommerceModifier(iI) != 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_PROCESS_CONVERTS", GC.getProcessInfo(eProcess).getProductionToCommerceModifier(iI), GC.getYieldInfo(YIELD_PRODUCTION).getChar(), GC.getCommerceInfo((CommerceTypes)iI).getChar()));
		}
	}
}

void CvGameTextMgr::setBadHealthHelp(CvWStringBuffer& szBuffer, CvCity& city) {
	if (city.badHealth() > 0) {
		int iHealth = -(city.getFreshWaterBadHealth());
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_FROM_FRESH_WATER", iHealth));
			szBuffer.append(NEWLINE);
		}

		iHealth = -(city.getFeatureBadHealth());
		if (iHealth > 0) {
			FeatureTypes eFeature = NO_FEATURE;

			for (int iI = 0; iI < city.getNumCityPlots(); ++iI) {
				CvPlot* pLoopPlot = plotCity(city.getX_INLINE(), city.getY_INLINE(), iI);

				if (pLoopPlot != NULL) {
					if (pLoopPlot->getFeatureType() != NO_FEATURE) {
						if (GC.getFeatureInfo(pLoopPlot->getFeatureType()).getHealthPercent() < 0) {
							if (eFeature == NO_FEATURE) {
								eFeature = pLoopPlot->getFeatureType();
							} else if (eFeature != pLoopPlot->getFeatureType()) {
								eFeature = NO_FEATURE;
								break;
							}
						}
					}
				}
			}

			szBuffer.append(gDLL->getText("TXT_KEY_MISC_FEAT_HEALTH", iHealth, ((eFeature == NO_FEATURE) ? L"TXT_KEY_MISC_FEATURES" : GC.getFeatureInfo(eFeature).getTextKeyWide())));
			szBuffer.append(NEWLINE);
		}

		iHealth = city.getEspionageHealthCounter();
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HEALTH_FROM_ESPIONAGE", iHealth));
			szBuffer.append(NEWLINE);
		}

		iHealth = -(city.getPowerBadHealth());
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HEALTH_FROM_POWER", iHealth));
			szBuffer.append(NEWLINE);
		}

		iHealth = -(city.getBonusBadHealth());
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HEALTH_FROM_BONUSES", iHealth));
			szBuffer.append(NEWLINE);
		}

		iHealth = -(city.totalBadBuildingHealth());
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HEALTH_FROM_BUILDINGS", iHealth));
			szBuffer.append(NEWLINE);
		}

		iHealth = -(GET_PLAYER(city.getOwnerINLINE()).getExtraHealth());
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HEALTH_FROM_CIV", iHealth));
			szBuffer.append(NEWLINE);
		}

		iHealth = -city.getExtraHealth();
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_UNHEALTH_EXTRA", iHealth));
			szBuffer.append(NEWLINE);
		}

		iHealth = -(GC.getHandicapInfo(city.getHandicapType()).getHealthBonus());
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HEALTH_FROM_HANDICAP", iHealth));
			szBuffer.append(NEWLINE);
		}

		iHealth = city.unhealthyPopulation();
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HEALTH_FROM_POP", iHealth));
			if (city.getUnhealthyPopulationModifier() != 0) {
				wchar szTempBuffer[1024];

				swprintf(szTempBuffer, 1024, L" (%+d%%)", city.getUnhealthyPopulationModifier());
				szBuffer.append(szTempBuffer);
			}
			szBuffer.append(NEWLINE);
		}

		szBuffer.append(L"-----------------------\n");

		szBuffer.append(gDLL->getText("TXT_KEY_MISC_TOTAL_UNHEALTHY", city.badHealth()));
	}
}

void CvGameTextMgr::setGoodHealthHelp(CvWStringBuffer& szBuffer, CvCity& city) {
	if (city.goodHealth() > 0) {
		int iHealth = city.getFreshWaterGoodHealth();
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HEALTH_FROM_FRESH_WATER", iHealth));
			szBuffer.append(NEWLINE);
		}

		iHealth = city.getFeatureGoodHealth();
		if (iHealth > 0) {
			FeatureTypes eFeature = NO_FEATURE;

			for (int iI = 0; iI < city.getNumCityPlots(); ++iI) {
				CvPlot* pLoopPlot = plotCity(city.getX_INLINE(), city.getY_INLINE(), iI);

				if (pLoopPlot != NULL) {
					if (pLoopPlot->getFeatureType() != NO_FEATURE) {
						if (GC.getFeatureInfo(pLoopPlot->getFeatureType()).getHealthPercent() > 0) {
							if (eFeature == NO_FEATURE) {
								eFeature = pLoopPlot->getFeatureType();
							} else if (eFeature != pLoopPlot->getFeatureType()) {
								eFeature = NO_FEATURE;
								break;
							}
						}
					}
				}
			}

			szBuffer.append(gDLL->getText("TXT_KEY_MISC_FEAT_GOOD_HEALTH", iHealth, ((eFeature == NO_FEATURE) ? L"TXT_KEY_MISC_FEATURES" : GC.getFeatureInfo(eFeature).getTextKeyWide())));
			szBuffer.append(NEWLINE);
		}

		iHealth = city.getPowerGoodHealth();
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_GOOD_HEALTH_FROM_POWER", iHealth));
			szBuffer.append(NEWLINE);
		}

		iHealth = city.getBonusGoodHealth();
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_GOOD_HEALTH_FROM_BONUSES", iHealth));
			szBuffer.append(NEWLINE);
		}

		iHealth = city.totalGoodBuildingHealth();
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_GOOD_HEALTH_FROM_BUILDINGS", iHealth));
			szBuffer.append(NEWLINE);
		}

		iHealth = GET_PLAYER(city.getOwnerINLINE()).getExtraHealth();
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_GOOD_HEALTH_FROM_CIV", iHealth));
			szBuffer.append(NEWLINE);
		}

		iHealth = city.getExtraHealth();
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HEALTH_EXTRA", iHealth));
			szBuffer.append(NEWLINE);
		}

		iHealth = GC.getHandicapInfo(city.getHandicapType()).getHealthBonus();
		if (iHealth > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_GOOD_HEALTH_FROM_HANDICAP", iHealth));
			szBuffer.append(NEWLINE);
		}

		szBuffer.append(L"-----------------------\n");

		szBuffer.append(gDLL->getText("TXT_KEY_MISC_TOTAL_HEALTHY", city.goodHealth()));
	}
}

bool CvGameTextMgr::setBuildingAdditionalHealthHelp(CvWStringBuffer& szBuffer, const CvCity& city, const CvWString& szStart, bool bStarted) {
	const CvCivilizationInfo& kCivilization = GC.getCivilizationInfo(GET_PLAYER(city.getOwnerINLINE()).getCivilizationType());

	for (int iI = 0; iI < GC.getNumBuildingClassInfos(); iI++) {
		BuildingTypes eBuilding = (BuildingTypes)kCivilization.getCivilizationBuildings((BuildingClassTypes)iI);

		if (eBuilding != NO_BUILDING && city.canConstruct(eBuilding, false, true, false)) {
			int iGood = 0, iBad = 0;
			int iChange = city.getAdditionalHealthByBuilding(eBuilding, iGood, iBad);

			if (iGood != 0 || iBad != 0) {
				int iSpoiledFood = city.getAdditionalSpoiledFood(iGood, iBad);
				int iStarvation = city.getAdditionalStarvation(iSpoiledFood);

				if (!bStarted) {
					szBuffer.append(szStart);
					bStarted = true;
				}

				CvWString szLabel;
				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				bool bStartedLine = setResumableGoodBadChangeHelp(szBuffer, szLabel, L": ", L"", iGood, gDLL->getSymbolID(HEALTHY_CHAR), iBad, gDLL->getSymbolID(UNHEALTHY_CHAR), false, true);
				setResumableValueChangeHelp(szBuffer, szLabel, L": ", L"", iSpoiledFood, gDLL->getSymbolID(EATEN_FOOD_CHAR), false, true, bStartedLine);
				setResumableValueChangeHelp(szBuffer, szLabel, L": ", L"", iStarvation, gDLL->getSymbolID(BAD_FOOD_CHAR), false, true, bStartedLine);
			}
		}
	}

	return bStarted;
}

void CvGameTextMgr::setAngerHelp(CvWStringBuffer& szBuffer, CvCity& city) {
	if (city.isOccupation()) {
		szBuffer.append(gDLL->getText("TXT_KEY_ANGER_RESISTANCE"));
	} else if (GET_PLAYER(city.getOwnerINLINE()).isAnarchy()) {
		szBuffer.append(gDLL->getText("TXT_KEY_ANGER_ANARCHY"));
	} else if (city.unhappyLevel() > 0) {
		// XXX decomp these???
		int iNewAngerPercent = city.getOvercrowdingPercentAnger();
		int iNewAnger = (iNewAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR();
		int iAnger = iNewAnger;
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_OVERCROWDING", iAnger));
			szBuffer.append(NEWLINE);
		}
		int iOldAngerPercent = iNewAngerPercent;
		int iOldAnger = iNewAnger;

		iNewAngerPercent += city.getNoMilitaryPercentAnger();
		iNewAnger += (((iNewAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()) - ((iOldAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()));
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_MILITARY_PROTECTION", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAngerPercent = iNewAngerPercent;
		iOldAnger = iNewAnger;

		iNewAngerPercent += city.getCulturePercentAnger();
		iNewAnger += (((iNewAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()) - ((iOldAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()));
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_OCCUPIED", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAngerPercent = iNewAngerPercent;
		iOldAnger = iNewAnger;

		iNewAngerPercent += city.getReligionPercentAnger();
		iNewAnger += (((iNewAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()) - ((iOldAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()));
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_RELIGION_FIGHT", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAngerPercent = iNewAngerPercent;
		iOldAnger = iNewAnger;

		iNewAngerPercent += city.getHurryPercentAnger();
		iNewAnger += (((iNewAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()) - ((iOldAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()));
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_OPPRESSION", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAngerPercent = iNewAngerPercent;
		iOldAnger = iNewAnger;

		iNewAngerPercent += city.getConscriptPercentAnger();
		iNewAnger += (((iNewAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()) - ((iOldAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()));
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_DRAFT", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAngerPercent = iNewAngerPercent;
		iOldAnger = iNewAnger;

		iNewAngerPercent += city.getDefyResolutionPercentAnger();
		iNewAnger += (((iNewAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()) - ((iOldAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()));
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_DEFY_RESOLUTION", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAngerPercent = iNewAngerPercent;
		iOldAnger = iNewAnger;

		iNewAngerPercent += city.getWarWearinessPercentAnger();
		iNewAnger += (((iNewAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()) - ((iOldAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()));
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_WAR_WEAR", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAngerPercent = iNewAngerPercent;
		iOldAnger = iNewAnger;

		// Global warming unhappiness
		// when I say 'percent' I mean 1/100. Unfortunately, people who made the rest of the game meant something else...
		// so I have to multiply my GwPercentAnger by 10 to make it fit in.
		iNewAngerPercent += std::max(0, GET_PLAYER(city.getOwnerINLINE()).getGwPercentAnger() * 10);
		iNewAnger += (((iNewAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()) - ((iOldAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()));
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_GLOBAL_WARMING", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAngerPercent = iNewAngerPercent;
		iOldAnger = iNewAnger;

		iNewAnger += std::max(0, city.getVassalUnhappiness());
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_UNHAPPY_VASSAL", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAnger = iNewAnger;

		iNewAnger += std::max(0, city.getEspionageHappinessCounter());
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_ESPIONAGE", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAnger = iNewAnger;

		for (int iI = 0; iI < GC.getNumCivicInfos(); ++iI) {
			iNewAngerPercent += GET_PLAYER(city.getOwnerINLINE()).getCivicPercentAnger((CivicTypes)iI);
			iNewAnger += (((iNewAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()) - ((iOldAngerPercent * city.getPopulation()) / GC.getPERCENT_ANGER_DIVISOR()));
			iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
			if (iAnger > 0) {
				szBuffer.append(gDLL->getText("TXT_KEY_ANGER_DEMAND_CIVIC", iAnger, GC.getCivicInfo((CivicTypes)iI).getTextKeyWide()));
				szBuffer.append(NEWLINE);
			}
			iOldAngerPercent = iNewAngerPercent;
			iOldAnger = iNewAnger;
		}

		iNewAnger -= std::min(0, city.getLargestCityHappiness());
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_BIG_CITY", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAnger = iNewAnger;

		iNewAnger -= std::min(0, city.getMilitaryHappiness());
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_MILITARY_PRESENCE", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAnger = iNewAnger;

		iNewAnger -= std::min(0, city.getCurrentStateReligionHappiness());
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_STATE_RELIGION", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAnger = iNewAnger;

		if (city.isStarSignAnger()) {
			iAnger = ((++iNewAnger - iOldAnger) + std::min(0, iOldAnger));
			if (iAnger > 0) {
				szBuffer.append(gDLL->getText("TXT_KEY_ANGER_STAR_SIGN", iAnger, city.getStarSignAngerTimer()));
				szBuffer.append(NEWLINE);
			}
			iOldAnger = iNewAnger;
		}

		iNewAnger -= std::min(0, (city.getBuildingBadHappiness() + city.getExtraBuildingBadHappiness()));
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_BUILDINGS", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAnger = iNewAnger;

		iNewAnger -= std::min(0, city.getFeatureBadHappiness());
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_FEATURES", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAnger = iNewAnger;

		iNewAnger -= std::min(0, city.getBonusBadHappiness());
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_BONUS", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAnger = iNewAnger;

		iNewAnger -= std::min(0, city.getReligionBadHappiness());
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_RELIGIOUS_FREEDOM", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAnger = iNewAnger;

		iNewAnger -= std::min(0, city.getCommerceHappiness());
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_BAD_ENTERTAINMENT", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAnger = iNewAnger;

		iNewAnger -= std::min(0, city.area()->getBuildingHappiness(city.getOwnerINLINE()));
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_BUILDINGS", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAnger = iNewAnger;

		iNewAnger -= std::min(0, GET_PLAYER(city.getOwnerINLINE()).getBuildingHappiness());
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_BUILDINGS", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAnger = iNewAnger;

		iNewAnger -= std::min(0, (city.getExtraHappiness() + GET_PLAYER(city.getOwnerINLINE()).getExtraHappiness()));
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_ARGH", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAnger = iNewAnger;

		iNewAnger -= std::min(0, GC.getHandicapInfo(city.getHandicapType()).getHappyBonus());
		iAnger = ((iNewAnger - iOldAnger) + std::min(0, iOldAnger));
		if (iAnger > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_HANDICAP", iAnger));
			szBuffer.append(NEWLINE);
		}
		iOldAnger = iNewAnger;

		szBuffer.append(L"-----------------------\n");

		szBuffer.append(gDLL->getText("TXT_KEY_ANGER_TOTAL_UNHAPPY", iOldAnger));

		FAssert(iOldAnger == city.unhappyLevel());
	}
}


void CvGameTextMgr::setHappyHelp(CvWStringBuffer& szBuffer, CvCity& city) {
	if (city.isOccupation() || GET_PLAYER(city.getOwnerINLINE()).isAnarchy()) {
		return;
	}
	if (city.happyLevel() > 0) {
		int iTotalHappy = 0;
		int iHappy = city.getLargestCityHappiness();
		if (iHappy > 0) {
			iTotalHappy += iHappy;
			szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_BIG_CITY", iHappy));
			szBuffer.append(NEWLINE);
		}

		iHappy = city.getMilitaryHappiness();
		if (iHappy > 0) {
			iTotalHappy += iHappy;
			szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_MILITARY_PRESENCE", iHappy));
			szBuffer.append(NEWLINE);
		}

		iHappy = city.getVassalHappiness();
		if (iHappy > 0) {
			iTotalHappy += iHappy;
			szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_VASSAL", iHappy));
			szBuffer.append(NEWLINE);
		}

		iHappy = city.getCurrentStateReligionHappiness();
		if (iHappy > 0) {
			iTotalHappy += iHappy;
			szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_STATE_RELIGION", iHappy));
			szBuffer.append(NEWLINE);
		}

		iHappy = (city.getBuildingGoodHappiness() + city.getExtraBuildingGoodHappiness());
		if (iHappy > 0) {
			iTotalHappy += iHappy;
			szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_BUILDINGS", iHappy));
			szBuffer.append(NEWLINE);
		}

		iHappy = city.getFeatureGoodHappiness();
		if (iHappy > 0) {
			iTotalHappy += iHappy;
			szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_FEATURES", iHappy));
			szBuffer.append(NEWLINE);
		}

		iHappy = city.getBonusGoodHappiness();
		if (iHappy > 0) {
			iTotalHappy += iHappy;
			szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_BONUS", iHappy));
			szBuffer.append(NEWLINE);
		}

		iHappy = city.getReligionGoodHappiness();
		if (iHappy > 0) {
			iTotalHappy += iHappy;
			szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_RELIGIOUS_FREEDOM", iHappy));
			szBuffer.append(NEWLINE);
		}

		iHappy = city.getCommerceHappiness();
		if (iHappy > 0) {
			iTotalHappy += iHappy;
			szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_ENTERTAINMENT", iHappy));
			szBuffer.append(NEWLINE);
		}

		iHappy = city.area()->getBuildingHappiness(city.getOwnerINLINE());
		if (iHappy > 0) {
			iTotalHappy += iHappy;
			szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_BUILDINGS", iHappy));
			szBuffer.append(NEWLINE);
		}

		iHappy = GET_PLAYER(city.getOwnerINLINE()).getBuildingHappiness();
		if (iHappy > 0) {
			iTotalHappy += iHappy;
			szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_BUILDINGS", iHappy));
			szBuffer.append(NEWLINE);
		}

		iHappy = (city.getExtraHappiness() + GET_PLAYER(city.getOwnerINLINE()).getExtraHappiness());
		if (iHappy > 0) {
			iTotalHappy += iHappy;
			szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_YEAH", iHappy));
			szBuffer.append(NEWLINE);
		}

		if (city.getHappinessTimer() > 0) {
			iHappy = GC.getDefineINT("TEMP_HAPPY");
			iTotalHappy += iHappy;
			szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_TEMP", iHappy, city.getHappinessTimer()));
			szBuffer.append(NEWLINE);
		}

		iHappy = GC.getHandicapInfo(city.getHandicapType()).getHappyBonus();
		if (iHappy > 0) {
			iTotalHappy += iHappy;
			szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_HANDICAP", iHappy));
			szBuffer.append(NEWLINE);
		}

		szBuffer.append(L"-----------------------\n");

		szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_TOTAL_HAPPY", iTotalHappy));

		FAssert(iTotalHappy == city.happyLevel())
	}
}

bool CvGameTextMgr::setBuildingAdditionalHappinessHelp(CvWStringBuffer& szBuffer, const CvCity& city, const CvWString& szStart, bool bStarted) {
	CvWString szLabel;
	const CvCivilizationInfo& kCivilization = GC.getCivilizationInfo(GET_PLAYER(city.getOwnerINLINE()).getCivilizationType());

	for (int iI = 0; iI < GC.getNumBuildingClassInfos(); iI++) {
		BuildingTypes eBuilding = (BuildingTypes)kCivilization.getCivilizationBuildings((BuildingClassTypes)iI);

		if (eBuilding != NO_BUILDING && city.canConstruct(eBuilding, false, true, false)) {
			int iGood = 0, iBad = 0;
			int iChange = city.getAdditionalHappinessByBuilding(eBuilding, iGood, iBad);

			if (iGood != 0 || iBad != 0) {
				int iAngryPop = city.getAdditionalAngryPopuplation(iGood, iBad);

				if (!bStarted) {
					szBuffer.append(szStart);
					bStarted = true;
				}

				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				bool bStartedLine = setResumableGoodBadChangeHelp(szBuffer, szLabel, L": ", L"", iGood, gDLL->getSymbolID(HAPPY_CHAR), iBad, gDLL->getSymbolID(UNHAPPY_CHAR), false, true);
				setResumableValueChangeHelp(szBuffer, szLabel, L": ", L"", iAngryPop, gDLL->getSymbolID(ANGRY_POP_CHAR), false, true, bStartedLine);
			}
		}
	}

	return bStarted;
}

void CvGameTextMgr::setYieldChangeHelp(CvWStringBuffer& szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, const int* piYieldChange, bool bPercent, bool bNewLine) {
	setResumableYieldChangeHelp(szBuffer, szStart, szSpace, szEnd, piYieldChange, bPercent, bNewLine);
}

/*
* Adds the ability to pass in and get back the value of bStarted so that
* it can be used with other setResumable<xx>ChangeHelp() calls on a single line.
*/
bool CvGameTextMgr::setResumableYieldChangeHelp(CvWStringBuffer& szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, const int* piYieldChange, bool bPercent, bool bNewLine, bool bStarted) {
	CvWString szTempBuffer;

	for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI) {
		if (piYieldChange[iI] != 0) {
			if (!bStarted) {
				if (bNewLine) {
					szTempBuffer.Format(L"\n%c", gDLL->getSymbolID(BULLET_CHAR));
				}
				szTempBuffer += CvWString::format(L"%s%s%s%d%s%c",
					szStart.GetCString(),
					szSpace.GetCString(),
					piYieldChange[iI] > 0 ? L"+" : L"",
					piYieldChange[iI],
					bPercent ? L"%" : L"",
					GC.getYieldInfo((YieldTypes)iI).getChar());
			} else {
				szTempBuffer.Format(L", %s%d%s%c",
					piYieldChange[iI] > 0 ? L"+" : L"",
					piYieldChange[iI],
					bPercent ? L"%" : L"",
					GC.getYieldInfo((YieldTypes)iI).getChar());
			}
			szBuffer.append(szTempBuffer);

			bStarted = true;
		}
	}

	if (bStarted) {
		szBuffer.append(szEnd);
	}

	// added
	return bStarted;
}

void CvGameTextMgr::setCommerceChangeHelp(CvWStringBuffer& szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, const int* piCommerceChange, bool bPercent, bool bNewLine) {
	setResumableCommerceChangeHelp(szBuffer, szStart, szSpace, szEnd, piCommerceChange, bPercent, bNewLine);
}

/*
* Adds the ability to pass in and get back the value of bStarted so that
* it can be used with other setResumable<xx>ChangeHelp() calls on a single line.
*/
bool CvGameTextMgr::setResumableCommerceChangeHelp(CvWStringBuffer& szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, const int* piCommerceChange, bool bPercent, bool bNewLine, bool bStarted) {
	CvWString szTempBuffer;

	for (int iI = 0; iI < NUM_COMMERCE_TYPES; ++iI) {
		if (piCommerceChange[iI] != 0) {
			if (!bStarted) {
				if (bNewLine) {
					szTempBuffer.Format(L"\n%c", gDLL->getSymbolID(BULLET_CHAR));
				}
				szTempBuffer += CvWString::format(L"%s%s%s%d%s%c", szStart.GetCString(), szSpace.GetCString(), ((piCommerceChange[iI] > 0) ? L"+" : L""), piCommerceChange[iI], ((bPercent) ? L"%" : L""), GC.getCommerceInfo((CommerceTypes)iI).getChar());
			} else {
				szTempBuffer.Format(L", %s%d%s%c", ((piCommerceChange[iI] > 0) ? L"+" : L""), piCommerceChange[iI], ((bPercent) ? L"%" : L""), GC.getCommerceInfo((CommerceTypes)iI).getChar());
			}
			szBuffer.append(szTempBuffer);

			bStarted = true;
		}
	}

	if (bStarted) {
		szBuffer.append(szEnd);
	}

	return bStarted;
}

/*
* Displays float values by dividing each value by 100.
*/
void CvGameTextMgr::setCommerceTimes100ChangeHelp(CvWStringBuffer& szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, const int* piCommerceChange, bool bNewLine, bool bStarted) {
	setResumableCommerceTimes100ChangeHelp(szBuffer, szStart, szSpace, szEnd, piCommerceChange, bNewLine);
}

/*
* Adds the ability to pass in and get back the value of bStarted so that
* it can be used with other setResumable<xx>ChangeHelp() calls on a single line.
*/
bool CvGameTextMgr::setResumableCommerceTimes100ChangeHelp(CvWStringBuffer& szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, const int* piCommerceChange, bool bNewLine, bool bStarted) {
	CvWString szTempBuffer;

	for (int iI = 0; iI < NUM_COMMERCE_TYPES; ++iI) {
		int iChange = piCommerceChange[iI];
		if (iChange != 0) {
			if (!bStarted) {
				if (bNewLine) {
					szTempBuffer.Format(L"\n%c", gDLL->getSymbolID(BULLET_CHAR));
				}
				szTempBuffer += CvWString::format(L"%s%s", szStart.GetCString(), szSpace.GetCString());
				bStarted = true;
			} else {
				szTempBuffer.Format(L", ");
			}
			szBuffer.append(szTempBuffer);

			if (iChange % 100 == 0) {
				szTempBuffer.Format(L"%+d%c", iChange / 100, GC.getCommerceInfo((CommerceTypes)iI).getChar());
			} else {
				if (iChange >= 0) {
					szBuffer.append(L"+");
				} else {
					iChange = -iChange;
					szBuffer.append(L"-");
				}
				szTempBuffer.Format(L"%d.%02d%c", iChange / 100, iChange % 100, GC.getCommerceInfo((CommerceTypes)iI).getChar());
			}
			szBuffer.append(szTempBuffer);
		}
	}

	if (bStarted) {
		szBuffer.append(szEnd);
	}

	return bStarted;
}

/*
* Adds the ability to pass in and get back the value of bStarted so that
* it can be used with other setResumable<xx>ChangeHelp() calls on a single line.
*/
bool CvGameTextMgr::setResumableGoodBadChangeHelp(CvWStringBuffer& szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, int iGood, int iGoodSymbol, int iBad, int iBadSymbol, bool bPercent, bool bNewLine, bool bStarted) {
	bStarted = setResumableValueChangeHelp(szBuffer, szStart, szSpace, szEnd, iGood, iGoodSymbol, bPercent, bNewLine, bStarted);
	bStarted = setResumableValueChangeHelp(szBuffer, szStart, szSpace, szEnd, iBad, iBadSymbol, bPercent, bNewLine, bStarted);

	return bStarted;
}

/*
* Adds the ability to pass in and get back the value of bStarted so that
* it can be used with other setResumable<xx>ChangeHelp() calls on a single line.
*/
bool CvGameTextMgr::setResumableValueChangeHelp(CvWStringBuffer& szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, int iValue, int iSymbol, bool bPercent, bool bNewLine, bool bStarted) {
	CvWString szTempBuffer;

	if (iValue != 0) {
		if (!bStarted) {
			if (bNewLine) {
				szTempBuffer.Format(L"\n%c", gDLL->getSymbolID(BULLET_CHAR));
			}
			szTempBuffer += CvWString::format(L"%s%s", szStart.GetCString(), szSpace.GetCString());
		} else {
			szTempBuffer = L", ";
		}
		szBuffer.append(szTempBuffer);

		szTempBuffer.Format(L"%+d%s%c", iValue, bPercent ? L"%" : L"", iSymbol);
		szBuffer.append(szTempBuffer);

		bStarted = true;
	}

	return bStarted;
}

/*
* Adds the ability to pass in and get back the value of bStarted so that
* it can be used with other setResumable<xx>ChangeHelp() calls on a single line.
*/
bool CvGameTextMgr::setResumableValueTimes100ChangeHelp(CvWStringBuffer& szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, int iValue, int iSymbol, bool bNewLine, bool bStarted) {
	CvWString szTempBuffer;

	if (iValue != 0) {
		if (!bStarted) {
			if (bNewLine) {
				szTempBuffer.Format(L"\n%c", gDLL->getSymbolID(BULLET_CHAR));
			}
			szTempBuffer += CvWString::format(L"%s%s", szStart.GetCString(), szSpace.GetCString());
		} else {
			szTempBuffer = L", ";
		}
		szBuffer.append(szTempBuffer);

		if (iValue % 100 == 0) {
			szTempBuffer.Format(L"%+d%c", iValue / 100, iSymbol);
		} else {
			if (iValue >= 0) {
				szBuffer.append(L"+");
			} else {
				iValue = -iValue;
				szBuffer.append(L"-");
			}
			szTempBuffer.Format(L"%d.%02d%c", iValue / 100, iValue % 100, iSymbol);
		}
		szBuffer.append(szTempBuffer);

		bStarted = true;
	}

	return bStarted;
}

// This function has been effectly rewritten for K-Mod. (there were a lot of things to change.)
void CvGameTextMgr::setBonusHelp(CvWStringBuffer& szBuffer, BonusTypes eBonus, bool bCivilopediaText) {
	if (NO_BONUS == eBonus) {
		return;
	}

	const CvBonusInfo& kBonus = GC.getBonusInfo(eBonus);

	int iHappiness = kBonus.getHappiness();
	int iHealth = kBonus.getHealth();

	if (bCivilopediaText) {
		// K-Mod. for the civilopedia text, display the basic bonuses as individual bullet points.
		// (they are displayed beside the name of the bonus when outside of the civilopedia.)
		if (iHappiness != 0) {
			szBuffer.append(CvWString::format(L"\n%c+%d%c", gDLL->getSymbolID(BULLET_CHAR), abs(iHappiness), iHappiness > 0 ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR)));
		}
		if (iHealth != 0) {
			szBuffer.append(CvWString::format(L"\n%c+%d%c", gDLL->getSymbolID(BULLET_CHAR), abs(iHealth), iHealth > 0 ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR)));
		}
	} else {
		szBuffer.append(CvWString::format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), kBonus.getDescription()));

		if (NO_PLAYER != GC.getGameINLINE().getActivePlayer()) {
			CvPlayer& kActivePlayer = GET_PLAYER(GC.getGameINLINE().getActivePlayer());

			// K-Mod. Bonuses now display "(Obsolete)" instead of "(player has 0)" when the bonus is obsolete.
			if (GET_TEAM(kActivePlayer.getTeam()).isBonusObsolete(eBonus)) {
				szBuffer.append(gDLL->getText("TXT_KEY_BONUS_OBSOLETE"));
			} else {
				// display the basic bonuses next to the name of the bonus
				if (iHappiness != 0) {
					szBuffer.append(CvWString::format(L", +%d%c", abs(iHappiness), iHappiness > 0 ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR)));
				}
				if (iHealth != 0) {
					szBuffer.append(CvWString::format(L", +%d%c", abs(iHealth), iHealth > 0 ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR)));
				}

				szBuffer.append(gDLL->getText("TXT_KEY_BONUS_AVAILABLE_PLAYER", kActivePlayer.getNumAvailableBonuses(eBonus), kActivePlayer.getNameKey()));

				for (CorporationTypes eCorporation = (CorporationTypes)0; eCorporation < GC.getNumCorporationInfos(); eCorporation = (CorporationTypes)(eCorporation + 1)) {
					const CvCorporationInfo& kLoopCorp = GC.getCorporationInfo(eCorporation);

					bool bCorpBonus = false;
					if (kActivePlayer.getHasCorporationCount(eCorporation) > 0) {
						for (int i = 0; !bCorpBonus && i < kLoopCorp.getNumPrereqBonuses(); ++i) {
							bCorpBonus = eBonus == GC.getCorporationInfo(eCorporation).getPrereqBonus(i);
						}
					}

					if (bCorpBonus)
						szBuffer.append(GC.getCorporationInfo(eCorporation).getChar());
				}
			}
		}

		// nasty const_cast, but can't change the const-correctness of the underlying function
		setYieldChangeHelp(szBuffer, L"", L"", gDLL->getText("TXT_KEY_BONUS_ON_PLOT"), const_cast<CvBonusInfo&>(kBonus).getYieldChangeArray());

		if (kBonus.getTechReveal() != NO_TECH) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BONUS_REVEALED_BY", GC.getTechInfo((TechTypes)kBonus.getTechReveal()).getTextKeyWide()));
		}
	}

	// K-Mod. Only display the perks of the bonus if it is not already obsolete
	if (bCivilopediaText || GC.getGameINLINE().getActiveTeam() == NO_TEAM || !GET_TEAM(GC.getGameINLINE().getActiveTeam()).isBonusObsolete(eBonus)) {
		CivilizationTypes eCivilization = GC.getGameINLINE().getActiveCivilizationType();

		for (int i = 0; i < GC.getNumBuildingClassInfos(); i++) {
			BuildingTypes eLoopBuilding;
			if (eCivilization == NO_CIVILIZATION) {
				eLoopBuilding = ((BuildingTypes)(GC.getBuildingClassInfo((BuildingClassTypes)i).getDefaultBuildingIndex()));
			} else {
				eLoopBuilding = ((BuildingTypes)(GC.getCivilizationInfo(eCivilization).getCivilizationBuildings(i)));
			}

			if (eLoopBuilding != NO_BUILDING) {
				CvBuildingInfo& kBuilding = GC.getBuildingInfo(eLoopBuilding);
				if (kBuilding.getBonusHappinessChanges(eBonus) != 0) {
					szBuffer.append(CvWString::format(L"\n%s", gDLL->getText("TXT_KEY_BUILDING_CIVIC_HEALTH_HAPPINESS_CHANGE", abs(kBuilding.getBonusHappinessChanges(eBonus)),
						kBuilding.getBonusHappinessChanges(eBonus) > 0 ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR)).c_str()));
					szBuffer.append(CvWString::format(L"<link=literal>%s</link>", kBuilding.getDescription()));
				}

				if (kBuilding.getBonusHealthChanges(eBonus) != 0) {
					szBuffer.append(CvWString::format(L"\n%s", gDLL->getText("TXT_KEY_BUILDING_CIVIC_HEALTH_HAPPINESS_CHANGE", abs(kBuilding.getBonusHealthChanges(eBonus)),
						kBuilding.getBonusHealthChanges(eBonus) > 0 ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR)).c_str()));
					szBuffer.append(CvWString::format(L"<link=literal>%s</link>", kBuilding.getDescription()));
				}
			}
		}

		if (!CvWString(kBonus.getHelp()).empty()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(kBonus.getHelp());
		}
	}
}

void CvGameTextMgr::setReligionHelp(CvWStringBuffer& szBuffer, ReligionTypes eReligion, bool bCivilopedia) {

	if (NO_RELIGION == eReligion) {
		return;
	}
	CvReligionInfo& kReligion = GC.getReligionInfo(eReligion);

	if (!bCivilopedia) {
		szBuffer.append(CvWString::format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), kReligion.getDescription()));
	}

	setCommerceChangeHelp(szBuffer, gDLL->getText("TXT_KEY_RELIGION_HOLY_CITY").c_str(), L": ", L"", kReligion.getHolyCityCommerceArray());
	setCommerceChangeHelp(szBuffer, gDLL->getText("TXT_KEY_RELIGION_ALL_CITIES").c_str(), L": ", L"", kReligion.getStateReligionCommerceArray());

	if (!bCivilopedia) {
		if (kReligion.getTechPrereq() != NO_TECH) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_RELIGION_FOUNDED_FIRST", GC.getTechInfo((TechTypes)kReligion.getTechPrereq()).getTextKeyWide()));
		}
	}

	if (kReligion.getFreeUnitClass() != NO_UNITCLASS) {
		UnitTypes eFreeUnit;
		if (GC.getGameINLINE().getActivePlayer() != NO_PLAYER) {
			eFreeUnit = ((UnitTypes)(GC.getCivilizationInfo(GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getCivilizationType()).getCivilizationUnits(kReligion.getFreeUnitClass())));
		} else {
			eFreeUnit = (UnitTypes)GC.getUnitClassInfo((UnitClassTypes)kReligion.getFreeUnitClass()).getDefaultUnitIndex();
		}

		if (eFreeUnit != NO_UNIT) {
			if (kReligion.getNumFreeUnits() > 1) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_RELIGION_FOUNDER_RECEIVES_NUM", GC.getUnitInfo(eFreeUnit).getTextKeyWide(), kReligion.getNumFreeUnits()));
			} else if (kReligion.getNumFreeUnits() > 0) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_RELIGION_FOUNDER_RECEIVES", GC.getUnitInfo(eFreeUnit).getTextKeyWide()));
			}
		}
	}
}

void CvGameTextMgr::setReligionHelpCity(CvWStringBuffer& szBuffer, ReligionTypes eReligion, CvCity* pCity, bool bCityScreen, bool bForceReligion, bool bForceState, bool bNoStateReligion) {
	CvWString szTempBuffer;
	bool bHandled = false;

	if (pCity == NULL) {
		return;
	}

	ReligionTypes eStateReligion = (bNoStateReligion ? NO_RELIGION : GET_PLAYER(pCity->getOwnerINLINE()).getStateReligion());

	if (bCityScreen) {
		szBuffer.append(CvWString::format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getReligionInfo(eReligion).getDescription()));
		szBuffer.append(NEWLINE);

		if (!(GC.getGameINLINE().isReligionFounded(eReligion)) && !GC.getGameINLINE().isOption(GAMEOPTION_PICK_RELIGION)) {
			if (GC.getReligionInfo(eReligion).getTechPrereq() != NO_TECH) {
				szBuffer.append(gDLL->getText("TXT_KEY_RELIGION_FOUNDED_FIRST", GC.getTechInfo((TechTypes)(GC.getReligionInfo(eReligion).getTechPrereq())).getTextKeyWide()));
			}
		}
	}

	if (GC.getGameINLINE().isReligionFounded(eReligion) && pCity && gDLL->getChtLvl() > 0 && GC.ctrlKey()) {
		szBuffer.append(CvWString::format(L"grip: %d", pCity->getReligionGrip(eReligion)));
		szBuffer.append(NEWLINE);
	}

	if (!bForceReligion) {
		if (!(pCity->isHasReligion(eReligion))) {
			return;
		}
	}

	if (eStateReligion == eReligion || eStateReligion == NO_RELIGION || bForceState) {
		for (int i = 0; i < NUM_COMMERCE_TYPES; i++) {
			int iCommerce = GC.getReligionInfo(eReligion).getStateReligionCommerce((CommerceTypes)i);

			if (pCity->isHolyCity(eReligion)) {
				iCommerce += GC.getReligionInfo(eReligion).getHolyCityCommerce((CommerceTypes)i);
			}

			if (iCommerce != 0) {
				if (bHandled) {
					szBuffer.append(L", ");
				}

				szTempBuffer.Format(L"%s%d%c", iCommerce > 0 ? "+" : "", iCommerce, GC.getCommerceInfo((CommerceTypes)i).getChar());
				szBuffer.append(szTempBuffer);
				bHandled = true;
			}
		}
	}

	if (eStateReligion == eReligion || bForceState) {
		int iHappiness = (pCity->getStateReligionHappiness(eReligion) + GET_PLAYER(pCity->getOwnerINLINE()).getStateReligionHappiness());

		if (iHappiness != 0) {
			if (bHandled) {
				szBuffer.append(L", ");
			}
			// Use absolute value with unhappy face
			szTempBuffer.Format(L"%d%c", abs(iHappiness), ((iHappiness > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR)));
			szBuffer.append(szTempBuffer);
			bHandled = true;
		}

		int iProductionModifier = GET_PLAYER(pCity->getOwnerINLINE()).getStateReligionBuildingProductionModifier();
		if (iProductionModifier != 0) {
			if (bHandled) {
				szBuffer.append(L", ");
			}

			szBuffer.append(gDLL->getText("TXT_KEY_RELIGION_BUILDING_PROD_MOD", iProductionModifier));
			bHandled = true;
		}

		iProductionModifier = GET_PLAYER(pCity->getOwnerINLINE()).getStateReligionUnitProductionModifier();
		if (iProductionModifier != 0) {
			if (bHandled) {
				szBuffer.append(L", ");
			}

			szBuffer.append(gDLL->getText("TXT_KEY_RELIGION_UNIT_PROD_MOD", iProductionModifier));
			bHandled = true;
		}

		int iFreeExperience = GET_PLAYER(pCity->getOwnerINLINE()).getStateReligionFreeExperience();
		if (iFreeExperience != 0) {
			if (bHandled) {
				szBuffer.append(L", ");
			}

			szBuffer.append(gDLL->getText("TXT_KEY_RELIGION_FREE_XP", iFreeExperience));
			bHandled = true;
		}

		int iGreatPeopleRateModifier = GET_PLAYER(pCity->getOwnerINLINE()).getStateReligionGreatPeopleRateModifier();
		if (iGreatPeopleRateModifier != 0) {
			if (bHandled) {
				szBuffer.append(L", ");
			}

			szBuffer.append(gDLL->getText("TXT_KEY_RELIGION_BIRTH_RATE_MOD", iGreatPeopleRateModifier));
			bHandled = true;
		}
	}
}

void CvGameTextMgr::setCorporationHelp(CvWStringBuffer& szBuffer, CorporationTypes eCorporation, bool bCivilopedia) {
	CvWString szTempBuffer;

	if (NO_CORPORATION == eCorporation) {
		return;
	}
	const CvCorporationInfo& kCorporation = GC.getCorporationInfo(eCorporation);

	if (!bCivilopedia) {
		szBuffer.append(CvWString::format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), kCorporation.getDescription()));
	}

	szTempBuffer.clear();
	for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1)) {
		int iYieldProduced = GC.getCorporationInfo(eCorporation).getYieldProduced(eYield);
		if (NO_PLAYER != GC.getGameINLINE().getActivePlayer()) {
			iYieldProduced *= GC.getWorldInfo(GC.getMapINLINE().getWorldSize()).getCorporationMaintenancePercent();
			iYieldProduced /= 100;
		}

		if (iYieldProduced != 0) {
			if (!szTempBuffer.empty()) {
				szTempBuffer += L", ";
			}

			if (iYieldProduced % 100 == 0) {
				szTempBuffer += CvWString::format(L"%s%d%c",
					iYieldProduced > 0 ? L"+" : L"",
					iYieldProduced / 100,
					GC.getYieldInfo(eYield).getChar());
			} else {
				szTempBuffer += CvWString::format(L"%s%.2f%c",
					iYieldProduced > 0 ? L"+" : L"",
					0.01f * abs(iYieldProduced),
					GC.getYieldInfo(eYield).getChar());
			}
		}
	}

	if (!szTempBuffer.empty()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_CORPORATION_ALL_CITIES", szTempBuffer.GetCString()));
	}

	szTempBuffer.clear();
	for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
		int iCommerceProduced = GC.getCorporationInfo(eCorporation).getCommerceProduced(eCommerce);
		if (NO_PLAYER != GC.getGameINLINE().getActivePlayer()) {
			iCommerceProduced *= GC.getWorldInfo(GC.getMapINLINE().getWorldSize()).getCorporationMaintenancePercent();
			iCommerceProduced /= 100;
		}
		if (iCommerceProduced != 0) {
			if (!szTempBuffer.empty()) {
				szTempBuffer += L", ";
			}

			if (iCommerceProduced % 100 == 0) {
				szTempBuffer += CvWString::format(L"%s%d%c",
					iCommerceProduced > 0 ? L"+" : L"",
					iCommerceProduced / 100,
					GC.getCommerceInfo(eCommerce).getChar());
			} else {
				szTempBuffer += CvWString::format(L"%s%.2f%c",
					iCommerceProduced > 0 ? L"+" : L"",
					0.01f * abs(iCommerceProduced),
					GC.getCommerceInfo(eCommerce).getChar());
			}

		}
	}

	if (!szTempBuffer.empty()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_CORPORATION_ALL_CITIES", szTempBuffer.GetCString()));
	}

	if (!bCivilopedia) {
		if (kCorporation.getTechPrereq() != NO_TECH) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_CORPORATION_FOUNDED_FIRST", GC.getTechInfo((TechTypes)kCorporation.getTechPrereq()).getTextKeyWide()));
		}
	}

	bool bFirst = true;
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_CORPORATION_BONUS_REQUIRED"));
		for (int iPrereqBonus = 0; iPrereqBonus < kCorporation.getNumPrereqBonuses(); ++iPrereqBonus) {
			BonusTypes ePrereqBonus = (BonusTypes)kCorporation.getPrereqBonus(iPrereqBonus);
			if (NO_BONUS != ePrereqBonus) {
				if (bFirst) {
					bFirst = false;
				} else {
					szBuffer.append(L", ");
				}

				szBuffer.append(gDLL->getText("TXT_KEY_CORPORATION_BONUS_CONSUMES", GC.getBonusInfo(ePrereqBonus).getTextKeyWide(), GC.getBonusInfo(ePrereqBonus).getChar()));
			}
		}
	}

	if (kCorporation.getBonusProduced() != NO_BONUS) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_CORPORATION_BONUS_PRODUCED", GC.getBonusInfo((BonusTypes)kCorporation.getBonusProduced()).getChar()));
	}

	if (kCorporation.getFreeUnitClass() != NO_UNITCLASS) {
		UnitTypes eFreeUnit;
		if (GC.getGameINLINE().getActivePlayer() != NO_PLAYER) {
			eFreeUnit = ((UnitTypes)(GC.getCivilizationInfo(GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getCivilizationType()).getCivilizationUnits(kCorporation.getFreeUnitClass())));
		} else {
			eFreeUnit = (UnitTypes)GC.getUnitClassInfo((UnitClassTypes)kCorporation.getFreeUnitClass()).getDefaultUnitIndex();
		}

		if (eFreeUnit != NO_UNIT) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_RELIGION_FOUNDER_RECEIVES", GC.getUnitInfo(eFreeUnit).getTextKeyWide()));
		}
	}

	std::vector<CorporationTypes> aCompetingCorps;
	bFirst = true;
	for (CorporationTypes eLoopCorporation = (CorporationTypes)0; eLoopCorporation < GC.getNumCorporationInfos(); eLoopCorporation = (CorporationTypes)(eLoopCorporation + 1)) {
		if (eLoopCorporation != eCorporation) {
			bool bCompeting = false;

			CvCorporationInfo& kLoopCorporation = GC.getCorporationInfo(eLoopCorporation);
			for (int iBonus1 = 0; iBonus1 < kLoopCorporation.getNumPrereqBonuses(); ++iBonus1) {
				BonusTypes eLoopBonus = (BonusTypes)kLoopCorporation.getPrereqBonus(iBonus1);
				if (eLoopBonus != NO_BONUS && kCorporation.isPrereqBonus(eLoopBonus)) {
					bCompeting = true;
					break;
				}
			}

			if (bCompeting) {
				CvWString szTemp = CvWString::format(L"<link=literal>%s</link>", kLoopCorporation.getDescription());
				setListHelp(szBuffer, gDLL->getText("TXT_KEY_CORPORATION_COMPETES").c_str(), szTemp.GetCString(), L", ", bFirst);
				bFirst = false;
			}
		}
	}
}

void CvGameTextMgr::setCorporationHelpCity(CvWStringBuffer& szBuffer, CorporationTypes eCorporation, CvCity* pCity, bool bCityScreen, bool bForceCorporation) {
	if (pCity == NULL) {
		return;
	}

	const CvCorporationInfo& kCorporation = GC.getCorporationInfo(eCorporation);

	if (bCityScreen) {
		szBuffer.append(CvWString::format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), kCorporation.getDescription()));
		szBuffer.append(NEWLINE);

		if (!(GC.getGameINLINE().isCorporationFounded(eCorporation))) {
			if (GC.getCorporationInfo(eCorporation).getTechPrereq() != NO_TECH) {
				szBuffer.append(gDLL->getText("TXT_KEY_CORPORATION_FOUNDED_FIRST", GC.getTechInfo((TechTypes)(kCorporation.getTechPrereq())).getTextKeyWide()));
			}
		}
	}

	if (!bForceCorporation) {
		if (!(pCity->isHasCorporation(eCorporation))) {
			return;
		}
	}

	int iNumResources = 0;
	for (int i = 0; i < kCorporation.getNumPrereqBonuses(); ++i) {
		BonusTypes eBonus = (BonusTypes)kCorporation.getPrereqBonus(i);
		if (NO_BONUS != eBonus) {
			iNumResources += pCity->getNumBonuses(eBonus);
		}
	}

	bool bActive = (pCity->isActiveCorporation(eCorporation) || (bForceCorporation && iNumResources > 0));

	bool bHandled = false;
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) {
		int iYield = 0;

		if (bActive) {
			iYield += (kCorporation.getYieldProduced(i) * iNumResources * GC.getWorldInfo(GC.getMapINLINE().getWorldSize()).getCorporationMaintenancePercent()) / 100;
		}

		if (iYield != 0) {
			if (bHandled) {
				szBuffer.append(L", ");
			}

			CvWString szTempBuffer;
			szTempBuffer.Format(L"%s%d%c", iYield > 0 ? "+" : "", (iYield + 99) / 100, GC.getYieldInfo((YieldTypes)i).getChar());
			szBuffer.append(szTempBuffer);
			bHandled = true;
		}
	}

	bHandled = false;
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i) {
		int iCommerce = 0;

		if (bActive) {
			iCommerce += (kCorporation.getCommerceProduced(i) * iNumResources * GC.getWorldInfo(GC.getMapINLINE().getWorldSize()).getCorporationMaintenancePercent()) / 100;
		}

		if (iCommerce != 0) {
			if (bHandled) {
				szBuffer.append(L", ");
			}

			CvWString szTempBuffer;
			szTempBuffer.Format(L"%s%d%c", iCommerce > 0 ? "+" : "", (iCommerce + 99) / 100, GC.getCommerceInfo((CommerceTypes)i).getChar());
			szBuffer.append(szTempBuffer);
			bHandled = true;
		}
	}

	int iMaintenance = 0;

	if (bActive) {
		iMaintenance += pCity->calculateCorporationMaintenanceTimes100(eCorporation);
		iMaintenance *= 100 + pCity->getMaintenanceModifier();
		iMaintenance /= 100;
	}

	if (0 != iMaintenance) {
		if (bHandled) {
			szBuffer.append(L", ");
		}

		CvWString szTempBuffer;
		szTempBuffer.Format(L"%d%c", -iMaintenance / 100, GC.getCommerceInfo(COMMERCE_GOLD).getChar());
		szBuffer.append(szTempBuffer);
		bHandled = true;
	}

	if (bCityScreen) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_CORPORATION_BONUS_REQUIRED"));
		bool bFirst = true;
		for (int i = 0; i < kCorporation.getNumPrereqBonuses(); ++i) {
			if (NO_BONUS != kCorporation.getPrereqBonus(i)) {
				if (bFirst) {
					bFirst = false;
				} else {
					szBuffer.append(L", ");
				}

				szBuffer.append(CvWString::format(L"%c", GC.getBonusInfo((BonusTypes)kCorporation.getPrereqBonus(i)).getChar()));
			}
		}

		if (bActive) {
			if (kCorporation.getBonusProduced() != NO_BONUS) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_CORPORATION_BONUS_PRODUCED", GC.getBonusInfo((BonusTypes)kCorporation.getBonusProduced()).getChar()));
			}
		}
	} else {
		if (kCorporation.getBonusProduced() != NO_BONUS) {
			if (bActive) {
				if (bHandled) {
					szBuffer.append(L", ");
				}

				szBuffer.append(CvWString::format(L"%c", GC.getBonusInfo((BonusTypes)kCorporation.getBonusProduced()).getChar()));
			}
		}
	}
}

void CvGameTextMgr::buildObsoleteString(CvWStringBuffer& szBuffer, int iItem, bool bList, bool bPlayerContext) {
	CvWString szTempBuffer;

	if (bList) {
		szBuffer.append(NEWLINE);
	}
	szBuffer.append(gDLL->getText("TXT_KEY_TECH_OBSOLETES", GC.getBuildingInfo((BuildingTypes)iItem).getTextKeyWide()));
}

void CvGameTextMgr::buildObsoleteBonusString(CvWStringBuffer& szBuffer, int iItem, bool bList, bool bPlayerContext) {
	CvWString szTempBuffer;

	if (bList) {
		szBuffer.append(NEWLINE);
	}
	szBuffer.append(gDLL->getText("TXT_KEY_TECH_OBSOLETES", GC.getBonusInfo((BonusTypes)iItem).getTextKeyWide()));
}

void CvGameTextMgr::buildObsoleteSpecialString(CvWStringBuffer& szBuffer, int iItem, bool bList, bool bPlayerContext) {
	CvWString szTempBuffer;

	if (bList) {
		szBuffer.append(NEWLINE);
	}
	szBuffer.append(gDLL->getText("TXT_KEY_TECH_OBSOLETES_NO_LINK", GC.getSpecialBuildingInfo((SpecialBuildingTypes)iItem).getTextKeyWide()));
}

void CvGameTextMgr::buildMoveString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	for (int iI = 0; iI < GC.getNumRouteInfos(); ++iI) {
		int iMoveDiff = ((GC.getMOVE_DENOMINATOR() / std::max(1, (GC.getRouteInfo((RouteTypes)iI).getMovementCost() + ((bPlayerContext) ? GET_TEAM(GC.getGameINLINE().getActiveTeam()).getRouteChange((RouteTypes)iI) : 0)))) - (GC.getMOVE_DENOMINATOR() / std::max(1, (GC.getRouteInfo((RouteTypes)iI).getMovementCost() + ((bPlayerContext) ? GET_TEAM(GC.getGameINLINE().getActiveTeam()).getRouteChange((RouteTypes)iI) : 0) + GC.getRouteInfo((RouteTypes)iI).getTechMovementChange(eTech)))));

		if (iMoveDiff != 0) {
			if (bList) {
				szBuffer.append(NEWLINE);
			}
			szBuffer.append(gDLL->getText("TXT_KEY_UNIT_MOVEMENT", -(iMoveDiff), GC.getRouteInfo((RouteTypes)iI).getTextKeyWide()));
			bList = true;
		}
	}
}

void CvGameTextMgr::buildFirstFreeUnitString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	UnitTypes eFirstFreeUnit = NO_UNIT;
	if (GC.getGameINLINE().getActivePlayer() != NO_PLAYER) {
		eFirstFreeUnit = GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getTechFreeUnit(eTech, true);
	} else {
		const CvTechInfo& kTech = GC.getTechInfo(eTech);
		if (kTech.getFirstFreeUnitClass() != NO_UNITCLASS) {
			eFirstFreeUnit = (UnitTypes)GC.getUnitClassInfo((UnitClassTypes)kTech.getFirstFreeUnitClass()).getDefaultUnitIndex();
		}
	}

	if (eFirstFreeUnit != NO_UNIT) {
		if (!bPlayerContext || (GC.getGameINLINE().countKnownTechNumTeams(eTech) == 0)) {
			if (bList) {
				szBuffer.append(NEWLINE);
			}
			szBuffer.append(gDLL->getText("TXT_KEY_TECH_FIRST_RECEIVES", GC.getUnitInfo(eFirstFreeUnit).getTextKeyWide()));
		}
	}
}

void CvGameTextMgr::buildFreeUnitString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	UnitTypes eFreeUnit = NO_UNIT;
	if (GC.getGameINLINE().getActivePlayer() != NO_PLAYER) {
		eFreeUnit = GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getTechFreeUnit(eTech, false);
	} else {
		if (GC.getTechInfo(eTech).getFreeUnitClass() != NO_UNITCLASS) {
			eFreeUnit = (UnitTypes)GC.getUnitClassInfo((UnitClassTypes)GC.getTechInfo(eTech).getFreeUnitClass()).getDefaultUnitIndex();
		}
	}

	if (eFreeUnit != NO_UNIT) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_TECH_RECEIVE_UNIT", GC.getUnitInfo(eFreeUnit).getTextKeyWide()));
	}
}

void CvGameTextMgr::buildFeatureProductionString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).getFeatureProductionModifier() != 0) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_FEATURE_PRODUCTION_MODIFIER", GC.getTechInfo(eTech).getFeatureProductionModifier()));
	}
}

void CvGameTextMgr::buildWorkerRateString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).getWorkerSpeedModifier() != 0) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_WORKERS_FASTER", GC.getTechInfo(eTech).getWorkerSpeedModifier()));
	}
}

void CvGameTextMgr::buildTradeRouteString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).getTradeRoutes() != 0) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_TRADE_ROUTES", GC.getTechInfo(eTech).getTradeRoutes()));
	}
}

void CvGameTextMgr::buildHealthRateString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).getHealth() != 0) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HEALTH_ALL_CITIES", abs(GC.getTechInfo(eTech).getHealth()), ((GC.getTechInfo(eTech).getHealth() > 0) ? gDLL->getSymbolID(HEALTHY_CHAR) : gDLL->getSymbolID(UNHEALTHY_CHAR))));
	}
}

void CvGameTextMgr::buildHappinessRateString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).getHappiness() != 0) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HAPPINESS_ALL_CITIES", abs(GC.getTechInfo(eTech).getHappiness()), ((GC.getTechInfo(eTech).getHappiness() > 0) ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR))));
	}
}

void CvGameTextMgr::buildFreeTechString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).getFirstFreeTechs() > 0) {
		if (!bPlayerContext || (GC.getGameINLINE().countKnownTechNumTeams(eTech) == 0)) {
			if (bList) {
				szBuffer.append(NEWLINE);
			}

			if (GC.getTechInfo(eTech).getFirstFreeTechs() == 1) {
				szBuffer.append(gDLL->getText("TXT_KEY_TECH_FIRST_FREE_TECH"));
			} else {
				szBuffer.append(gDLL->getText("TXT_KEY_TECH_FIRST_FREE_TECHS", GC.getTechInfo(eTech).getFirstFreeTechs()));
			}
		}
	}
}

void CvGameTextMgr::buildLOSString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isExtraWaterSeeFrom() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isExtraWaterSeeFrom()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_UNIT_EXTRA_SIGHT"));
	}
}

void CvGameTextMgr::buildMapCenterString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isMapCentering() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isMapCentering()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_CENTERS_MAP"));
	}
}

void CvGameTextMgr::buildMapRevealString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList) {
	if (GC.getTechInfo(eTech).isMapVisible()) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_REVEALS_MAP"));
	}
}

void CvGameTextMgr::buildMapTradeString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isMapTrading() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isMapTrading()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_MAP_TRADING"));
	}
}

void CvGameTextMgr::buildTechTradeString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isTechTrading() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isTechTrading()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_TECH_TRADING"));
	}
}

void CvGameTextMgr::buildGoldTradeString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isGoldTrading() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isGoldTrading()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_GOLD_TRADING"));
	}
}

void CvGameTextMgr::buildOpenBordersString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isOpenBordersTrading() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isOpenBordersTrading()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_OPEN_BORDERS"));
	}
}

void CvGameTextMgr::buildDefensivePactString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isDefensivePactTrading() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isDefensivePactTrading()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_DEFENSIVE_PACTS"));
	}
}

void CvGameTextMgr::buildPermanentAllianceString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isPermanentAllianceTrading() && (!bPlayerContext || (!(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isPermanentAllianceTrading()) && GC.getGameINLINE().isOption(GAMEOPTION_PERMANENT_ALLIANCES)))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_PERM_ALLIANCES"));
	}
}

void CvGameTextMgr::buildVassalStateString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isVassalStateTrading() && (!bPlayerContext || (!(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isVassalStateTrading()) && GC.getGameINLINE().isOption(GAMEOPTION_NO_VASSAL_STATES)))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_VASSAL_STATES"));
	}
}

void CvGameTextMgr::buildBridgeString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isBridgeBuilding() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isBridgeBuilding()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_BRIDGE_BUILDING"));
	}
}

void CvGameTextMgr::buildIrrigationString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isIrrigation() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isIrrigation()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_SPREAD_IRRIGATION"));
	}
}

void CvGameTextMgr::buildIgnoreIrrigationString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isIgnoreIrrigation() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isIgnoreIrrigation()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_IRRIGATION_ANYWHERE"));
	}
}

void CvGameTextMgr::buildWaterWorkString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isWaterWork() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isWaterWork()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_WATER_WORK"));
	}
}

void CvGameTextMgr::buildImprovementString(CvWStringBuffer& szBuffer, TechTypes eTech, int iImprovement, bool bList, bool bPlayerContext) {
	bool bTechFound = false;

	if (GC.getBuildInfo((BuildTypes)iImprovement).getTechPrereq() == NO_TECH) {
		for (int iJ = 0; iJ < GC.getNumFeatureInfos(); iJ++) {
			if (GC.getBuildInfo((BuildTypes)iImprovement).getFeatureTech(iJ) == eTech) {
				bTechFound = true;
			}
		}
	} else {
		if (GC.getBuildInfo((BuildTypes)iImprovement).getTechPrereq() == eTech) {
			bTechFound = true;
		}
	}

	if (bTechFound) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}

		szBuffer.append(gDLL->getText("TXT_KEY_MISC_CAN_BUILD_IMPROVEMENT", GC.getBuildInfo((BuildTypes)iImprovement).getTextKeyWide()));
	}
}

void CvGameTextMgr::buildDomainExtraMovesString(CvWStringBuffer& szBuffer, TechTypes eTech, int iDomainType, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).getDomainExtraMoves(iDomainType) != 0) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}

		szBuffer.append(gDLL->getText("TXT_KEY_MISC_EXTRA_MOVES", GC.getTechInfo(eTech).getDomainExtraMoves(iDomainType), GC.getDomainInfo((DomainTypes)iDomainType).getTextKeyWide()));
	}
}

void CvGameTextMgr::buildAdjustString(CvWStringBuffer& szBuffer, TechTypes eTech, int iCommerceType, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isCommerceFlexible(iCommerceType) && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isCommerceFlexible((CommerceTypes)iCommerceType)))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ADJUST_COMMERCE_RATE", GC.getCommerceInfo((CommerceTypes)iCommerceType).getChar()));
	}
}

void CvGameTextMgr::buildTerrainTradeString(CvWStringBuffer& szBuffer, TechTypes eTech, int iTerrainType, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isTerrainTrade(iTerrainType) && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isTerrainTrade((TerrainTypes)iTerrainType)))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_ON_TERRAIN", gDLL->getSymbolID(TRADE_CHAR), GC.getTerrainInfo((TerrainTypes)iTerrainType).getTextKeyWide()));
	}
}

void CvGameTextMgr::buildRiverTradeString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isRiverTrade() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isRiverTrade()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_ON_TERRAIN", gDLL->getSymbolID(TRADE_CHAR), gDLL->getText("TXT_KEY_MISC_RIVERS").GetCString()));
	}
}

void CvGameTextMgr::buildSpecialBuildingString(CvWStringBuffer& szBuffer, TechTypes eTech, int iBuildingType, bool bList, bool bPlayerContext) {
	if (GC.getSpecialBuildingInfo((SpecialBuildingTypes)iBuildingType).getTechPrereq() == eTech) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_CAN_CONSTRUCT_BUILDING", GC.getSpecialBuildingInfo((SpecialBuildingTypes)iBuildingType).getTextKeyWide()));
	}

	if (GC.getSpecialBuildingInfo((SpecialBuildingTypes)iBuildingType).getTechPrereqAnyone() == eTech) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_CAN_CONSTRUCT_BUILDING_ANYONE", GC.getSpecialBuildingInfo((SpecialBuildingTypes)iBuildingType).getTextKeyWide()));
	}
}

void CvGameTextMgr::buildYieldChangeString(CvWStringBuffer& szBuffer, TechTypes eTech, int iYieldType, bool bList, bool bPlayerContext) {
	CvWString szTempBuffer;
	if (bList) {
		szTempBuffer.Format(L"<link=literal>%s</link>", GC.getImprovementInfo((ImprovementTypes)iYieldType).getDescription());
	} else {
		szTempBuffer.Format(L"%c<link=literal>%s</link>", gDLL->getSymbolID(BULLET_CHAR), GC.getImprovementInfo((ImprovementTypes)iYieldType).getDescription());
	}

	setYieldChangeHelp(szBuffer, szTempBuffer, L": ", L"", GC.getImprovementInfo((ImprovementTypes)iYieldType).getTechYieldChangesArray(eTech), false, bList);
}

bool CvGameTextMgr::buildBonusRevealString(CvWStringBuffer& szBuffer, TechTypes eTech, int iBonusType, bool bFirst, bool bList, bool bPlayerContext) {
	CvWString szTempBuffer;

	if (GC.getBonusInfo((BonusTypes)iBonusType).getTechReveal() == eTech) {
		if (bList && bFirst) {
			szBuffer.append(NEWLINE);
		}
		szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getBonusInfo((BonusTypes)iBonusType).getDescription());
		setListHelp(szBuffer, gDLL->getText("TXT_KEY_MISC_REVEALS").c_str(), szTempBuffer, L", ", bFirst);
		bFirst = false;
	}
	return bFirst;
}

bool CvGameTextMgr::buildCivicRevealString(CvWStringBuffer& szBuffer, TechTypes eTech, int iCivicType, bool bFirst, bool bList, bool bPlayerContext) {
	CvWString szTempBuffer;

	if (GC.getCivicInfo((CivicTypes)iCivicType).getTechPrereq() == eTech) {
		if (bList && bFirst) {
			szBuffer.append(NEWLINE);
		}
		szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getCivicInfo((CivicTypes)iCivicType).getDescription());
		setListHelp(szBuffer, gDLL->getText("TXT_KEY_MISC_ENABLES").c_str(), szTempBuffer, L", ", bFirst);
		bFirst = false;
	}
	return bFirst;
}

bool CvGameTextMgr::buildProcessInfoString(CvWStringBuffer& szBuffer, TechTypes eTech, int iProcessType, bool bFirst, bool bList, bool bPlayerContext) {
	CvWString szTempBuffer;

	if (GC.getProcessInfo((ProcessTypes)iProcessType).getTechPrereq() == eTech) {
		if (bList && bFirst) {
			szBuffer.append(NEWLINE);
		}
		szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getProcessInfo((ProcessTypes)iProcessType).getDescription());
		setListHelp(szBuffer, gDLL->getText("TXT_KEY_MISC_CAN_BUILD").c_str(), szTempBuffer, L", ", bFirst);
		bFirst = false;
	}
	return bFirst;
}

bool CvGameTextMgr::buildFoundReligionString(CvWStringBuffer& szBuffer, TechTypes eTech, int iReligionType, bool bFirst, bool bList, bool bPlayerContext) {
	CvWString szTempBuffer;

	if (GC.getReligionInfo((ReligionTypes)iReligionType).getTechPrereq() == eTech) {
		if (!bPlayerContext || (GC.getGameINLINE().countKnownTechNumTeams(eTech) == 0)) {
			if (bList && bFirst) {
				szBuffer.append(NEWLINE);
			}

			if (GC.getGameINLINE().isOption(GAMEOPTION_PICK_RELIGION)) {
				szTempBuffer = gDLL->getText("TXT_KEY_RELIGION_UNKNOWN");
			} else {
				szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getReligionInfo((ReligionTypes)iReligionType).getDescription());
			}
			setListHelp(szBuffer, gDLL->getText("TXT_KEY_MISC_FIRST_DISCOVER_FOUNDS").c_str(), szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
	}
	return bFirst;
}

bool CvGameTextMgr::buildFoundCorporationString(CvWStringBuffer& szBuffer, TechTypes eTech, int iCorporationType, bool bFirst, bool bList, bool bPlayerContext) {
	CvWString szTempBuffer;

	if (GC.getCorporationInfo((CorporationTypes)iCorporationType).getTechPrereq() == eTech) {
		if (!bPlayerContext || (GC.getGameINLINE().countKnownTechNumTeams(eTech) == 0)) {
			if (bList && bFirst) {
				szBuffer.append(NEWLINE);
			}
			szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getCorporationInfo((CorporationTypes)iCorporationType).getDescription());
			setListHelp(szBuffer, gDLL->getText("TXT_KEY_MISC_FIRST_DISCOVER_INCORPORATES").c_str(), szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
	}
	return bFirst;
}

bool CvGameTextMgr::buildPromotionString(CvWStringBuffer& szBuffer, TechTypes eTech, int iPromotionType, bool bFirst, bool bList, bool bPlayerContext) {
	CvWString szTempBuffer;

	if (GC.getPromotionInfo((PromotionTypes)iPromotionType).getTechPrereq() == eTech) {
		if (bList && bFirst) {
			szBuffer.append(NEWLINE);
		}
		szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getPromotionInfo((PromotionTypes)iPromotionType).getDescription());
		setListHelp(szBuffer, gDLL->getText("TXT_KEY_MISC_ENABLES").c_str(), szTempBuffer, L", ", bFirst);
		bFirst = false;
	}
	return bFirst;
}

// Displays a list of derived technologies - no distinction between AND/OR prerequisites
void CvGameTextMgr::buildSingleLineTechTreeString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bPlayerContext) {
	CvWString szTempBuffer;	// Formatting

	if (NO_TECH == eTech) {
		// you need to specify a tech of origin for this method to do anything
		return;
	}

	bool bFirst = true;
	for (TechTypes eLoopTech = (TechTypes)0; eLoopTech < GC.getNumTechInfos(); eLoopTech = (TechTypes)(eLoopTech + 1)) {
		const CvTechInfo& kLoopTech = GC.getTechInfo(eLoopTech);
		bool bTechAlreadyAccessible = false;
		if (bPlayerContext) {
			bTechAlreadyAccessible = (GET_TEAM(GC.getGameINLINE().getActiveTeam()).isHasTech(eLoopTech) || GET_PLAYER(GC.getGameINLINE().getActivePlayer()).canResearch(eLoopTech));
		}
		if (!bTechAlreadyAccessible) {
			bool bTechFound = false;

			if (!bTechFound) {
				for (int iJ = 0; iJ < kLoopTech.getNumPrereqOrTechs(); iJ++) {
					if (kLoopTech.getPrereqOrTech(iJ) == eTech) {
						bTechFound = true;
						break;
					}
				}
			}

			if (!bTechFound) {
				for (int iJ = 0; iJ < kLoopTech.getNumPrereqAndTechs(); iJ++) {
					if (kLoopTech.getPrereqAndTech(iJ) == eTech) {
						bTechFound = true;
						break;
					}
				}
			}

			if (bTechFound) {
				szTempBuffer.Format(SETCOLR L"<link=literal>%s</link>" ENDCOLR, TEXT_COLOR("COLOR_TECH_TEXT"), kLoopTech.getDescription());
				setListHelp(szBuffer, gDLL->getText("TXT_KEY_MISC_LEADS_TO").c_str(), szTempBuffer, L", ", bFirst);
				bFirst = false;
			}
		}
	}
}

// Information about other prerequisite technologies to eTech besides eFromTech
void CvGameTextMgr::buildTechTreeString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bPlayerContext, TechTypes eFromTech) {
	CvWString szTempBuffer;	// Formatting

	if (NO_TECH == eTech || NO_TECH == eFromTech) {
		return;
	}

	const CvTechInfo& kTech = GC.getTechInfo(eTech);

	szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_TECH_TEXT"), GC.getTechInfo(eTech).getDescription());
	szBuffer.append(szTempBuffer);

	// Loop through OR prerequisites to make list
	CvWString szOtherOrTechs;
	int nOtherOrTechs = 0;
	bool bOrTechFound = false;
	for (int iJ = 0; iJ < kTech.getNumPrereqOrTechs(); iJ++) {
		TechTypes eTestTech = (TechTypes)kTech.getPrereqOrTech(iJ);
		if (eTestTech >= 0) {
			bool bTechAlreadyResearched = false;
			if (bPlayerContext) {
				bTechAlreadyResearched = GET_TEAM(GC.getGameINLINE().getActiveTeam()).isHasTech(eTestTech);
			}
			if (!bTechAlreadyResearched) {
				if (eTestTech == eFromTech) {
					bOrTechFound = true;
				} else {
					szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_TECH_TEXT"), GC.getTechInfo(eTestTech).getDescription());
					setListHelp(szOtherOrTechs, L"", szTempBuffer, gDLL->getText("TXT_KEY_OR").c_str(), 0 == nOtherOrTechs);
					nOtherOrTechs++;
				}
			}
		}
	}

	// Loop through AND prerequisites to make list
	CvWString szOtherAndTechs;
	int nOtherAndTechs = 0;
	bool bAndTechFound = false;
	for (int iJ = 0; iJ < kTech.getNumPrereqAndTechs(); iJ++) {
		TechTypes eTestTech = (TechTypes)kTech.getPrereqAndTech(iJ);
		if (eTestTech >= 0) {
			bool bTechAlreadyResearched = false;
			if (bPlayerContext) {
				bTechAlreadyResearched = GET_TEAM(GC.getGameINLINE().getActiveTeam()).isHasTech(eTestTech);
			}
			if (!bTechAlreadyResearched) {
				if (eTestTech == eFromTech) {
					bAndTechFound = true;
				} else {
					szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_TECH_TEXT"), GC.getTechInfo(eTestTech).getDescription());
					setListHelp(szOtherAndTechs, L"", szTempBuffer, L", ", 0 == nOtherAndTechs);
					nOtherAndTechs++;
				}
			}
		}
	}

	if (bOrTechFound || bAndTechFound) {
		if (nOtherAndTechs > 0 || nOtherOrTechs > 0) {
			szBuffer.append(L' ');

			if (nOtherAndTechs > 0) {
				szBuffer.append(gDLL->getText("TXT_KEY_WITH_SPACE"));
				szBuffer.append(szOtherAndTechs);
			}

			if (nOtherOrTechs > 0) {
				if (bAndTechFound) {
					if (nOtherAndTechs > 0) {
						szBuffer.append(gDLL->getText("TXT_KEY_AND_SPACE"));
					} else {
						szBuffer.append(gDLL->getText("TXT_KEY_WITH_SPACE"));
					}
					szBuffer.append(szOtherOrTechs);
				} else if (bOrTechFound) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_MISC_ALTERNATIVELY_DERIVED", kTech.getTextKeyWide(), szOtherOrTechs.GetCString()));
				}
			}
		}
	}
}

void CvGameTextMgr::setPromotionHelp(CvWStringBuffer& szBuffer, PromotionTypes ePromotion, bool bCivilopediaText) {
	if (!bCivilopediaText) {
		CvWString szTempBuffer;

		if (NO_PROMOTION == ePromotion) {
			return;
		}
		CvPromotionInfo& promo = GC.getPromotionInfo(ePromotion);

		szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), promo.getDescription());
		szBuffer.append(szTempBuffer);
	}

	parsePromotionHelp(szBuffer, ePromotion);
}

void CvGameTextMgr::setUnitCombatHelp(CvWStringBuffer& szBuffer, UnitCombatTypes eUnitCombat) {
	szBuffer.append(GC.getUnitCombatInfo(eUnitCombat).getDescription());
}

void CvGameTextMgr::setImprovementHelp(CvWStringBuffer& szBuffer, ImprovementTypes eImprovement, bool bCivilopediaText) {
	CvWString szTempBuffer;
	CvWString szFirstBuffer;
	int iTurns;

	if (NO_IMPROVEMENT == eImprovement) {
		return;
	}

	CvImprovementInfo& kImprovement = GC.getImprovementInfo(eImprovement);
	if (!bCivilopediaText) {
		szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), kImprovement.getDescription());
		szBuffer.append(szTempBuffer);

		setYieldChangeHelp(szBuffer, L", ", L"", L"", kImprovement.getYieldChangeArray(), false, false);

		setYieldChangeHelp(szBuffer, L"", L"", gDLL->getText("TXT_KEY_MISC_WITH_IRRIGATION").c_str(), kImprovement.getIrrigatedYieldChangeArray());
		setYieldChangeHelp(szBuffer, L"", L"", gDLL->getText("TXT_KEY_MISC_ON_HILLS").c_str(), kImprovement.getHillsYieldChangeArray());
		setYieldChangeHelp(szBuffer, L"", L"", gDLL->getText("TXT_KEY_MISC_ALONG_RIVER").c_str(), kImprovement.getRiverSideYieldChangeArray());

		for (int iTech = 0; iTech < GC.getNumTechInfos(); iTech++) {
			for (int iYield = 0; iYield < NUM_YIELD_TYPES; iYield++) {
				if (0 != kImprovement.getTechYieldChanges(iTech, iYield)) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_IMPROVEMENT_WITH_TECH", kImprovement.getTechYieldChanges(iTech, iYield), GC.getYieldInfo((YieldTypes)iYield).getChar(), GC.getTechInfo((TechTypes)iTech).getTextKeyWide()));
				}
			}
		}

		//	Civics
		for (int iYield = 0; iYield < NUM_YIELD_TYPES; iYield++) {
			for (int iCivic = 0; iCivic < GC.getNumCivicInfos(); iCivic++) {
				int iChange = GC.getCivicInfo((CivicTypes)iCivic).getImprovementYieldChanges(eImprovement, iYield);
				if (0 != iChange) {
					szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getCivicInfo((CivicTypes)iCivic).getDescription());
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_CIVIC_IMPROVEMENT_YIELD_CHANGE", iChange, GC.getYieldInfo((YieldTypes)iYield).getChar()));
					szBuffer.append(szTempBuffer);
				}
			}
		}
	}

	if (kImprovement.isRequiresRiverSide()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_IMPROVEMENT_REQUIRES_RIVER"));
	}
	if (kImprovement.isCarriesIrrigation()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_IMPROVEMENT_CARRIES_IRRIGATION"));
	}
	if (bCivilopediaText) {
		if (kImprovement.isNoFreshWater()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_IMPROVEMENT_NO_BUILD_FRESH_WATER"));
		}
		if (kImprovement.isWater()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_IMPROVEMENT_BUILD_ONLY_WATER"));
		}
		if (kImprovement.isRequiresFlatlands()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_IMPROVEMENT_ONLY_BUILD_FLATLANDS"));
		}
	}

	if (kImprovement.getImprovementUpgrade() != NO_IMPROVEMENT) {
		TechTypes eTechPrereq = (TechTypes)kImprovement.getUpgradeTech();
		iTurns = GC.getGameINLINE().getImprovementUpgradeTime(eImprovement);

		szBuffer.append(NEWLINE);
		if (eTechPrereq == NO_TECH) {
			szBuffer.append(gDLL->getText("TXT_KEY_IMPROVEMENT_EVOLVES", GC.getImprovementInfo((ImprovementTypes)kImprovement.getImprovementUpgrade()).getTextKeyWide(), iTurns));
		} else {
			szBuffer.append(gDLL->getText("TXT_KEY_IMPROVEMENT_EVOLVES_TECH", GC.getImprovementInfo((ImprovementTypes)kImprovement.getImprovementUpgrade()).getTextKeyWide(), iTurns, GC.getTechInfo(eTechPrereq).getTextKeyWide()));
		}
	}

	int iLast = -1;
	for (int iBonus = 0; iBonus < GC.getNumBonusInfos(); iBonus++) {
		int iRand = kImprovement.getImprovementBonusDiscoverRand(iBonus);
		if (iRand > 0) {
			szFirstBuffer.Format(L"%s%s", NEWLINE, gDLL->getText("TXT_KEY_IMPROVEMENT_CHANCE_DISCOVER").c_str());
			szTempBuffer.Format(L"%c", GC.getBonusInfo((BonusTypes)iBonus).getChar());
			setListHelp(szBuffer, szFirstBuffer, szTempBuffer, L", ", iRand != iLast);
			iLast = iRand;
		}
	}

	if (0 != kImprovement.getDefenseModifier()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_IMPROVEMENT_DEFENSE_MODIFIER", kImprovement.getDefenseModifier()));
	}

	if (0 != kImprovement.getHappiness()) {
		szBuffer.append(NEWLINE);
		// Use absolute value with unhappy face
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ICON_CHANGE_NEARBY_CITIES", abs(kImprovement.getHappiness()), (kImprovement.getHappiness() > 0 ? gDLL->getSymbolID(HAPPY_CHAR) : gDLL->getSymbolID(UNHAPPY_CHAR))));
	}

	if (kImprovement.isActsAsCity()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_IMPROVEMENT_DEFENSE_MODIFIER_EXTRA"));
	}

	if (kImprovement.getFeatureGrowthProbability() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_IMPROVEMENT_MORE_GROWTH"));
	} else if (kImprovement.getFeatureGrowthProbability() < 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_IMPROVEMENT_LESS_GROWTH"));
	}

	if (bCivilopediaText) {
		if (kImprovement.getPillageGold() > 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_IMPROVEMENT_PILLAGE_YIELDS", kImprovement.getPillageGold()));
		}
	}
}


void CvGameTextMgr::getDealString(CvWStringBuffer& szBuffer, CvDeal& deal, PlayerTypes ePlayerPerspective) {
	PlayerTypes ePlayer1 = deal.getFirstPlayer();
	PlayerTypes ePlayer2 = deal.getSecondPlayer();

	const CLinkList<TradeData>* pListPlayer1 = deal.getFirstTrades();
	const CLinkList<TradeData>* pListPlayer2 = deal.getSecondTrades();

	getDealString(szBuffer, ePlayer1, ePlayer2, pListPlayer1, pListPlayer2, ePlayerPerspective);
}

void CvGameTextMgr::getDealString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer1, PlayerTypes ePlayer2, const CLinkList<TradeData>* pListPlayer1, const CLinkList<TradeData>* pListPlayer2, PlayerTypes ePlayerPerspective) {
	if (NO_PLAYER == ePlayer1 || NO_PLAYER == ePlayer2) {
		FAssertMsg(false, "Deal needs two parties");
		return;
	}

	CvWStringBuffer szDealOne;
	if (NULL != pListPlayer1 && pListPlayer1->getLength() > 0) {
		CLLNode<TradeData>* pTradeNode;
		bool bFirst = true;
		for (pTradeNode = pListPlayer1->head(); pTradeNode; pTradeNode = pListPlayer1->next(pTradeNode)) {
			CvWStringBuffer szTrade;
			getTradeString(szTrade, pTradeNode->m_data, ePlayer1, ePlayer2);
			setListHelp(szDealOne, L"", szTrade.getCString(), L", ", bFirst);
			bFirst = false;
		}
	}

	CvWStringBuffer szDealTwo;
	if (NULL != pListPlayer2 && pListPlayer2->getLength() > 0) {
		CLLNode<TradeData>* pTradeNode;
		bool bFirst = true;
		for (pTradeNode = pListPlayer2->head(); pTradeNode; pTradeNode = pListPlayer2->next(pTradeNode)) {
			CvWStringBuffer szTrade;
			getTradeString(szTrade, pTradeNode->m_data, ePlayer2, ePlayer1);
			setListHelp(szDealTwo, L"", szTrade.getCString(), L", ", bFirst);
			bFirst = false;
		}
	}

	if (!szDealOne.isEmpty()) {
		if (!szDealTwo.isEmpty()) {
			if (ePlayerPerspective == ePlayer1) {
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_OUR_DEAL", szDealOne.getCString(), GET_PLAYER(ePlayer2).getNameKey(), szDealTwo.getCString()));
			} else if (ePlayerPerspective == ePlayer2) {
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_OUR_DEAL", szDealTwo.getCString(), GET_PLAYER(ePlayer1).getNameKey(), szDealOne.getCString()));
			} else {
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEAL", GET_PLAYER(ePlayer1).getNameKey(), szDealOne.getCString(), GET_PLAYER(ePlayer2).getNameKey(), szDealTwo.getCString()));
			}
		} else {
			if (ePlayerPerspective == ePlayer1) {
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEAL_ONESIDED_OURS", szDealOne.getCString(), GET_PLAYER(ePlayer2).getNameKey()));
			} else if (ePlayerPerspective == ePlayer2) {
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEAL_ONESIDED_THEIRS", szDealOne.getCString(), GET_PLAYER(ePlayer1).getNameKey()));
			} else {
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEAL_ONESIDED", GET_PLAYER(ePlayer1).getNameKey(), szDealOne.getCString(), GET_PLAYER(ePlayer2).getNameKey()));
			}
		}
	} else if (!szDealTwo.isEmpty()) {
		if (ePlayerPerspective == ePlayer1) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEAL_ONESIDED_THEIRS", szDealTwo.getCString(), GET_PLAYER(ePlayer2).getNameKey()));
		} else if (ePlayerPerspective == ePlayer2) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEAL_ONESIDED_OURS", szDealTwo.getCString(), GET_PLAYER(ePlayer1).getNameKey()));
		} else {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEAL_ONESIDED", GET_PLAYER(ePlayer2).getNameKey(), szDealTwo.getCString(), GET_PLAYER(ePlayer1).getNameKey()));
		}
	}
}

void CvGameTextMgr::getWarplanString(CvWStringBuffer& szString, WarPlanTypes eWarPlan) {
	switch (eWarPlan) {
	case WARPLAN_ATTACKED_RECENT: szString.assign(L"new defensive war"); break;
	case WARPLAN_ATTACKED: szString.assign(L"defensive war"); break;
	case WARPLAN_PREPARING_LIMITED: szString.assign(L"preparing limited war"); break;
	case WARPLAN_PREPARING_TOTAL: szString.assign(L"preparing total war"); break;
	case WARPLAN_LIMITED: szString.assign(L"limited war"); break;
	case WARPLAN_TOTAL: szString.assign(L"total war"); break;
	case WARPLAN_DOGPILE: szString.assign(L"dogpile war"); break;
	case NO_WARPLAN: szString.assign(L"no warplan"); break;
	default:  szString.assign(L"unknown warplan"); break;
	}
}

void CvGameTextMgr::getAttitudeString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer, PlayerTypes eTargetPlayer) {
	CvWString szTempBuffer;
	CvPlayerAI& kPlayer = GET_PLAYER(ePlayer);
	TeamTypes eTeam = (TeamTypes)kPlayer.getTeam();
	CvTeamAI& kTeam = GET_TEAM(eTeam);

	if (kPlayer.isHuman())
		return;
	szBuffer.append(NEWLINE);

	szBuffer.append(gDLL->getText("TXT_KEY_ATTITUDE_TOWARDS", GC.getAttitudeInfo(kPlayer.AI_getAttitude(eTargetPlayer)).getTextKeyWide(), GET_PLAYER(eTargetPlayer).getNameKey()));

	// (K-Mod note: vassal information has been moved from here to a new function)

	// Attitude breakdown
	int iAttitudeChange = 0;
	for (int iPass = 0; iPass < 2; iPass++) {
		iAttitudeChange = kPlayer.AI_getCloseBordersAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_LAND_TARGET", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getWarAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_WAR", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getPeaceAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_PEACE", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getSameReligionAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_SAME_RELIGION", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getDifferentReligionAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_DIFFERENT_RELIGION", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getBonusTradeAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_BONUS_TRADE", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getOpenBordersAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_OPEN_BORDERS", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getDefensivePactAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_DEFENSIVE_PACT", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getRivalDefensivePactAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_RIVAL_DEFENSIVE_PACT", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getRivalVassalAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_RIVAL_VASSAL", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getShareWarAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_SHARE_WAR", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getFavoriteCivicAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_FAVORITE_CIVIC", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getTradeAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_TRADE", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getRivalTradeAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_RIVAL_TRADE", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getColonyAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_FREEDOM", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getAttitudeExtra(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText(((iAttitudeChange > 0) ? "TXT_KEY_MISC_ATTITUDE_EXTRA_GOOD" : "TXT_KEY_MISC_ATTITUDE_EXTRA_BAD"), iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		iAttitudeChange = kPlayer.AI_getEmbassyAttitude(eTargetPlayer);
		if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_EMBASSY", iAttitudeChange).GetCString());
			szBuffer.append(NEWLINE);
			szBuffer.append(szTempBuffer);
		}

		for (MemoryTypes eMemory = (MemoryTypes)0; eMemory < NUM_MEMORY_TYPES; eMemory = (MemoryTypes)(eMemory + 1)) {
			iAttitudeChange = kPlayer.AI_getMemoryAttitude(eTargetPlayer, eMemory);
			if ((iPass == 0) ? (iAttitudeChange > 0) : (iAttitudeChange < 0)) {
				szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR((iAttitudeChange > 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT"), gDLL->getText("TXT_KEY_MISC_ATTITUDE_MEMORY", iAttitudeChange, GC.getMemoryInfo(eMemory).getDescription()).GetCString());
				szBuffer.append(NEWLINE);
				szBuffer.append(szTempBuffer);
			}
		}
	}

}

// K-Mod
void CvGameTextMgr::getVassalInfoString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) {
	FAssert(ePlayer != NO_PLAYER);

	const CvTeam& kTeam = GET_TEAM(GET_PLAYER(ePlayer).getTeam());

	for (TeamTypes i = (TeamTypes)0; i < MAX_TEAMS; i = (TeamTypes)(i + 1)) {
		const CvTeam& kLoopTeam = GET_TEAM(i);
		if (kLoopTeam.isAlive()) {
			if (kTeam.isVassal(i)) {
				szBuffer.append(NEWLINE);
				szBuffer.append(CvWString::format(SETCOLR, TEXT_COLOR("COLOR_LIGHT_GREY")));
				szBuffer.append(gDLL->getText("TXT_KEY_ATTITUDE_VASSAL_OF", kLoopTeam.getName().GetCString()));
				setVassalRevoltHelp(szBuffer, i, kTeam.getID());
				szBuffer.append(ENDCOLR);
			} else if (kLoopTeam.isVassal(kTeam.getID())) {
				szBuffer.append(NEWLINE);
				szBuffer.append(CvWString::format(SETCOLR, TEXT_COLOR("COLOR_LIGHT_GREY")));
				szBuffer.append(gDLL->getText("TXT_KEY_ATTITUDE_MASTER_OF", kLoopTeam.getName().GetCString()));
				szBuffer.append(ENDCOLR);
			}
		}
	}
}

void CvGameTextMgr::getWarWearinessString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer, PlayerTypes eTargetPlayer) const {
	FAssert(ePlayer != NO_PLAYER);
	// Show ePlayer's war weariness towards eTargetPlayer.
	// (note: this is the reverse of what was shown in the original code.)
	// War weariness should be shown in it natural units - it's a percentage of population
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);

	int iWarWeariness = 0;
	if (eTargetPlayer == NO_PLAYER || eTargetPlayer == ePlayer) {
		// If eTargetPlayer == NO_PLAYER, show ePlayer's total war weariness?
		// There are a couple of problems with displaying the total war weariness: information leak, out-of-date information...
		// lets do it only for the active player.
		if (GC.getGameINLINE().getActivePlayer() == ePlayer)
			iWarWeariness = kPlayer.getWarWearinessPercentAnger();
	} else {
		const CvPlayer& kTargetPlayer = GET_PLAYER(eTargetPlayer);
		if (atWar(kPlayer.getTeam(), kTargetPlayer.getTeam()) && (GC.getGameINLINE().isDebugMode() || GET_PLAYER(GC.getGameINLINE().getActivePlayer()).canSeeDemographics(ePlayer))) {
			iWarWeariness = kPlayer.getModifiedWarWearinessPercentAnger(GET_TEAM(kPlayer.getTeam()).getWarWeariness(kTargetPlayer.getTeam(), true) / 100);
		}
	}

	iWarWeariness *= 100;
	iWarWeariness /= GC.getPERCENT_ANGER_DIVISOR();

	if (iWarWeariness != 0)
		szBuffer.append(CvWString::format(L"\n%s: %d%%", gDLL->getText("TXT_KEY_WAR_WEAR_HELP").GetCString(), iWarWeariness));
}

void CvGameTextMgr::getEspionageString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer, PlayerTypes eTargetPlayer) {
	FAssertMsg(false, "obsolete function. (getEspionageString)"); // K-Mod
	if (!GC.getGameINLINE().isOption(GAMEOPTION_NO_ESPIONAGE)) {
		CvPlayer& kPlayer = GET_PLAYER(ePlayer);
		TeamTypes eTeam = (TeamTypes)kPlayer.getTeam();
		CvTeam& kTeam = GET_TEAM(eTeam);
		CvPlayer& kTargetPlayer = GET_PLAYER(eTargetPlayer);

		szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_AGAINST_PLAYER", kTargetPlayer.getNameKey(), kTeam.getEspionagePointsAgainstTeam(kTargetPlayer.getTeam()), GET_TEAM(kTargetPlayer.getTeam()).getEspionagePointsAgainstTeam(kPlayer.getTeam())));
	}
}

void CvGameTextMgr::getTradeString(CvWStringBuffer& szBuffer, const TradeData& tradeData, PlayerTypes ePlayer1, PlayerTypes ePlayer2) {
	switch (tradeData.m_eItemType) {
	case TRADE_GOLD:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_GOLD", tradeData.m_iData));
		break;
	case TRADE_GOLD_PER_TURN:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_GOLD_PER_TURN", tradeData.m_iData));
		break;
	case TRADE_MAPS:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_WORLD_MAP"));
		break;
	case TRADE_SURRENDER:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_CAPITULATE"));
		break;
	case TRADE_VASSAL:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_VASSAL"));
		break;
	case TRADE_OPEN_BORDERS:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_OPEN_BORDERS"));
		break;
	case TRADE_DEFENSIVE_PACT:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEFENSIVE_PACT"));
		break;
	case TRADE_NON_AGGRESSION:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_NON_AGGRESSION"));
		break;
	case TRADE_PERMANENT_ALLIANCE:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_PERMANENT_ALLIANCE"));
		break;
	case TRADE_PEACE_TREATY:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_PEACE_TREATY", GC.getDefineINT("PEACE_TREATY_LENGTH")));
		break;
	case TRADE_TECHNOLOGIES:
		szBuffer.assign(CvWString::format(L"%s", GC.getTechInfo((TechTypes)tradeData.m_iData).getDescription()));
		break;
	case TRADE_RESOURCES:
		szBuffer.assign(CvWString::format(L"%s", GC.getBonusInfo((BonusTypes)tradeData.m_iData).getDescription()));
		break;
	case TRADE_CITIES:
		szBuffer.assign(CvWString::format(L"%s", GET_PLAYER(ePlayer1).getCity(tradeData.m_iData)->getName().GetCString()));
		break;
	case TRADE_PEACE:
	case TRADE_WAR:
	case TRADE_EMBARGO:
		szBuffer.assign(CvWString::format(L"%s", GET_TEAM((TeamTypes)tradeData.m_iData).getName().GetCString()));
		break;
	case TRADE_CIVIC:
		szBuffer.assign(CvWString::format(L"%s", GC.getCivicInfo((CivicTypes)tradeData.m_iData).getDescription()));
		break;
	case TRADE_RELIGION:
		szBuffer.assign(CvWString::format(L"%s", GC.getReligionInfo((ReligionTypes)tradeData.m_iData).getDescription()));
		break;
	case TRADE_EMBASSY:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_EMBASSY"));
		break;
	case TRADE_LIMITED_BORDERS:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_LIMITED_BORDERS"));
		break;
	case TRADE_FREE_TRADE_ZONE:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_FREE_TRADE_ZONE"));
		break;
	case TRADE_WORKER:
	case TRADE_MILITARY_UNIT:
		szBuffer.assign(CvWString::format(L"%s", GET_PLAYER(ePlayer1).getUnit(tradeData.m_iData)->getName().GetCString()));
		break;
	default:
		FAssert(false);
		break;
	}
}

void CvGameTextMgr::setFeatureHelp(CvWStringBuffer& szBuffer, FeatureTypes eFeature, bool bCivilopediaText) {
	if (NO_FEATURE == eFeature) {
		return;
	}
	const CvFeatureInfo& kFeature = GC.getFeatureInfo(eFeature);

	int aiYields[NUM_YIELD_TYPES];
	if (!bCivilopediaText) {
		szBuffer.append(kFeature.getDescription());

		for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI) {
			aiYields[iI] = kFeature.getYieldChange(iI);
		}
		setYieldChangeHelp(szBuffer, L"", L"", L"", aiYields);
	}
	for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI) {
		aiYields[iI] = kFeature.getRiverYieldChange(iI);
	}
	setYieldChangeHelp(szBuffer, L"", L"", gDLL->getText("TXT_KEY_TERRAIN_NEXT_TO_RIVER"), aiYields);

	for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI) {
		aiYields[iI] = kFeature.getHillsYieldChange(iI);
	}
	setYieldChangeHelp(szBuffer, L"", L"", gDLL->getText("TXT_KEY_TERRAIN_ON_HILLS"), aiYields);

	if (kFeature.getMovementCost() != 1) {
		szBuffer.append(gDLL->getText("TXT_KEY_TERRAIN_MOVEMENT_COST", kFeature.getMovementCost()));
	}

	CvWString szHealth;
	szHealth.Format(L"%.2f", 0.01f * abs(kFeature.getHealthPercent()));
	if (kFeature.getHealthPercent() > 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_FEATURE_GOOD_HEALTH", szHealth.GetCString()));
	} else if (kFeature.getHealthPercent() < 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_FEATURE_BAD_HEALTH", szHealth.GetCString()));
	}

	if (kFeature.getDefenseModifier() != 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_TERRAIN_DEFENSE_MODIFIER", kFeature.getDefenseModifier()));
	}

	if (kFeature.getTurnDamage() > 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_TERRAIN_TURN_DAMAGE", kFeature.getTurnDamage()));
	} else if (kFeature.getTurnDamage() < 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_TERRAIN_TURN_HEALING", -kFeature.getTurnDamage()));
	}


	if (kFeature.isAddsFreshWater()) {
		szBuffer.append(gDLL->getText("TXT_KEY_FEATURE_ADDS_FRESH_WATER"));
	}

	if (kFeature.isImpassable()) {
		szBuffer.append(gDLL->getText("TXT_KEY_TERRAIN_IMPASSABLE"));
	}

	if (kFeature.isNoCity()) {
		szBuffer.append(gDLL->getText("TXT_KEY_TERRAIN_NO_CITIES"));
	}

	if (kFeature.isNoImprovement()) {
		szBuffer.append(gDLL->getText("TXT_KEY_FEATURE_NO_IMPROVEMENT"));
	}

}


void CvGameTextMgr::setTerrainHelp(CvWStringBuffer& szBuffer, TerrainTypes eTerrain, bool bCivilopediaText) {
	if (NO_TERRAIN == eTerrain) {
		return;
	}
	const CvTerrainInfo& kTerrain = GC.getTerrainInfo(eTerrain);

	int aiYields[NUM_YIELD_TYPES];
	if (!bCivilopediaText) {
		szBuffer.append(kTerrain.getDescription());

		for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI) {
			aiYields[iI] = kTerrain.getYield(iI);
		}
		setYieldChangeHelp(szBuffer, L"", L"", L"", aiYields);
	}
	for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI) {
		aiYields[iI] = kTerrain.getRiverYieldChange(iI);
	}
	setYieldChangeHelp(szBuffer, L"", L"", gDLL->getText("TXT_KEY_TERRAIN_NEXT_TO_RIVER"), aiYields);

	for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI) {
		aiYields[iI] = kTerrain.getHillsYieldChange(iI);
	}
	setYieldChangeHelp(szBuffer, L"", L"", gDLL->getText("TXT_KEY_TERRAIN_ON_HILLS"), aiYields);

	if (kTerrain.getMovementCost() != 1) {
		szBuffer.append(gDLL->getText("TXT_KEY_TERRAIN_MOVEMENT_COST", kTerrain.getMovementCost()));
	}

	if (kTerrain.getBuildModifier() != 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_TERRAIN_BUILD_MODIFIER", kTerrain.getBuildModifier()));
	}

	if (kTerrain.getDefenseModifier() != 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_TERRAIN_DEFENSE_MODIFIER", kTerrain.getDefenseModifier()));
	}

	if (kTerrain.isImpassable()) {
		szBuffer.append(gDLL->getText("TXT_KEY_TERRAIN_IMPASSABLE"));
	}
	if (!kTerrain.isFound()) {
		szBuffer.append(gDLL->getText("TXT_KEY_TERRAIN_NO_CITIES"));
		bool bFirst = true;
		if (kTerrain.isFoundCoast()) {
			szBuffer.append(gDLL->getText("TXT_KEY_TERRAIN_COASTAL_CITIES"));
			bFirst = false;
		}
		if (!bFirst) {
			szBuffer.append(gDLL->getText("TXT_KEY_OR"));
		}
		if (kTerrain.isFoundFreshWater()) {
			szBuffer.append(gDLL->getText("TXT_KEY_TERRAIN_FRESH_WATER_CITIES"));
			bFirst = false;
		}
	}
}

void CvGameTextMgr::buildFinanceInflationString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) {
	if (NO_PLAYER == ePlayer) {
		return;
	}
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);

	int iInflationRate = kPlayer.calculateInflationRate();
	if (iInflationRate != 0) {
		int iPreInflation = kPlayer.calculatePreInflatedCosts();
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_INFLATION", iPreInflation, iInflationRate, iInflationRate, iPreInflation, (iPreInflation * iInflationRate) / 100));
	}
}

void CvGameTextMgr::buildFinanceUnitCostString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) {
	if (NO_PLAYER == ePlayer) {
		return;
	}
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);

	int iFreeUnits = 0;
	int iFreeMilitaryUnits = 0;
	int iUnits = kPlayer.getNumUnits();
	int iMilitaryUnits = kPlayer.getNumMilitaryUnits();
	int iPaidUnits = iUnits;
	int iPaidMilitaryUnits = iMilitaryUnits;
	int iMilitaryCost = 0;
	int iUnitCost = 0; // was called "base unit cost"
	int iExtraCost = 0;
	int iCost = kPlayer.calculateUnitCost(iFreeUnits, iFreeMilitaryUnits, iPaidUnits, iPaidMilitaryUnits, iUnitCost, iMilitaryCost, iExtraCost);
	int iHandicap = iCost - iUnitCost - iMilitaryCost - iExtraCost;

	szBuffer.append(NEWLINE);
	szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_UNIT_COST", iUnits, iFreeUnits, iUnitCost)); // K-Mod

	if (iMilitaryCost != 0) // K-Mod
	{
		szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_UNIT_COST_2", iMilitaryUnits, iFreeMilitaryUnits, iMilitaryCost)); // K-Mod
	}
	if (iExtraCost != 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_UNIT_COST_3", iExtraCost));
	}
	if (iHandicap != 0) {
		FAssertMsg(false, "not all unit costs were accounted for"); // K-Mod (handicap modifier are now rolled into the other costs)
		szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_HANDICAP_COST", iHandicap));
		szBuffer.append(CvWString::format(L" (%+d%%)", GC.getHandicapInfo(kPlayer.getHandicapType()).getUnitCostPercent() - 100)); // K-Mod
	}
	szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_UNIT_COST_4", iCost));
}

void CvGameTextMgr::buildFinanceAwaySupplyString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) {
	if (NO_PLAYER == ePlayer) {
		return;
	}
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);

	int iPaidUnits = 0;
	int iBaseCost = 0;
	int iCost = kPlayer.calculateUnitSupply(iPaidUnits, iBaseCost);
	int iHandicap = iCost - iBaseCost;

	CvWString szHandicap;
	if (iHandicap != 0) {
		FAssertMsg(false, "not all supply costs were accounted for"); // K-Mod (handicap modifier are now rolled into the other costs)
		szHandicap = gDLL->getText("TXT_KEY_FINANCE_ADVISOR_HANDICAP_COST", iHandicap);
	}

	szBuffer.append(NEWLINE);
	szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_SUPPLY_COST", kPlayer.getNumOutsideUnits(), kPlayer.getNumOutsideUnits() - iPaidUnits, iBaseCost, szHandicap.GetCString(), iCost)); // K-Mod
}

void CvGameTextMgr::buildFinanceCityMaintString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) {
	if (NO_PLAYER == ePlayer) {
		return;
	}
	int iDistanceMaint = 0;
	int iColonyMaint = 0;
	int iCorporationMaint = 0;

	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	int iLoop;
	for (CvCity* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop)) {
		if (!pLoopCity->isDisorder() && !pLoopCity->isWeLoveTheKingDay() && (pLoopCity->getPopulation() > 0)) {
			iDistanceMaint += (pLoopCity->calculateDistanceMaintenanceTimes100() * std::max(0, (pLoopCity->getMaintenanceModifier() + 100))) / 100;
			iColonyMaint += (pLoopCity->calculateColonyMaintenanceTimes100() * std::max(0, (pLoopCity->getMaintenanceModifier() + 100))) / 100;
			iCorporationMaint += (pLoopCity->calculateCorporationMaintenanceTimes100() * std::max(0, (pLoopCity->getMaintenanceModifier() + 100))) / 100;
		}
	}
	iDistanceMaint /= 100;
	iColonyMaint /= 100;
	iCorporationMaint /= 100;

	int iNumCityMaint = kPlayer.getTotalMaintenance() - iDistanceMaint - iColonyMaint - iCorporationMaint;

	szBuffer.append(NEWLINE);
	szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_CITY_MAINT_COST", iDistanceMaint, iNumCityMaint, iColonyMaint, iCorporationMaint, kPlayer.getTotalMaintenance()));
}

void CvGameTextMgr::buildFinanceCivicUpkeepString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) {
	if (NO_PLAYER == ePlayer) {
		return;
	}
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	CvWString szCivicOptionCosts;
	for (int iI = 0; iI < GC.getNumCivicOptionInfos(); ++iI) {
		CivicTypes eCivic = kPlayer.getCivics((CivicOptionTypes)iI);
		if (NO_CIVIC != eCivic) {
			CvWString szTemp;
			szTemp.Format(L"%d%c: %s", kPlayer.getSingleCivicUpkeep(eCivic), GC.getCommerceInfo(COMMERCE_GOLD).getChar(), GC.getCivicInfo(eCivic).getDescription());
			szCivicOptionCosts += NEWLINE + szTemp;
		}
	}

	szBuffer.append(NEWLINE);
	szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_CIVIC_UPKEEP_COST", szCivicOptionCosts.GetCString(), kPlayer.getCivicUpkeep()));
}

void CvGameTextMgr::buildFinanceForeignIncomeString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) {
	if (NO_PLAYER == ePlayer) {
		return;
	}
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);

	CvWString szPlayerIncome;
	for (int iI = 0; iI < MAX_PLAYERS; ++iI) {
		const CvPlayer& otherPlayer = GET_PLAYER((PlayerTypes)iI);
		if (otherPlayer.isAlive() && kPlayer.getGoldPerTurnByPlayer((PlayerTypes)iI) != 0) {
			CvWString szTemp;
			szTemp.Format(L"%d%c: %s", kPlayer.getGoldPerTurnByPlayer((PlayerTypes)iI), GC.getCommerceInfo(COMMERCE_GOLD).getChar(), otherPlayer.getCivilizationShortDescription());
			szPlayerIncome += NEWLINE + szTemp;
		}
	}
	if (!szPlayerIncome.empty()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_FOREIGN_INCOME", szPlayerIncome.GetCString(), kPlayer.getGoldPerTurn()));
	}
}

bool CvGameTextMgr::setBuildingAdditionalYieldHelp(CvWStringBuffer& szBuffer, const CvCity& city, YieldTypes eIndex, const CvWString& szStart, bool bStarted) {
	const CvYieldInfo& kYield = GC.getYieldInfo(eIndex);
	const CvCivilizationInfo& kCivilization = GC.getCivilizationInfo(GET_PLAYER(city.getOwnerINLINE()).getCivilizationType());

	for (int iI = 0; iI < GC.getNumBuildingClassInfos(); iI++) {
		BuildingTypes eBuilding = (BuildingTypes)kCivilization.getCivilizationBuildings((BuildingClassTypes)iI);

		if (eBuilding != NO_BUILDING && city.canConstruct(eBuilding, false, true, false)) {
			int iChange = city.getAdditionalYieldByBuilding(eIndex, eBuilding);

			if (iChange != 0) {
				if (!bStarted) {
					szBuffer.append(szStart);
					bStarted = true;
				}

				CvWString szLabel;
				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				setResumableValueChangeHelp(szBuffer, szLabel, L": ", L"", iChange, kYield.getChar(), false, true);
			}
		}
	}

	return bStarted;
}

bool CvGameTextMgr::setBuildingAdditionalCommerceHelp(CvWStringBuffer& szBuffer, const CvCity& city, CommerceTypes eIndex, const CvWString& szStart, bool bStarted) {
	const CvCommerceInfo& kCommerce = GC.getCommerceInfo(eIndex);
	const CvCivilizationInfo& kCivilization = GC.getCivilizationInfo(GET_PLAYER(city.getOwnerINLINE()).getCivilizationType());

	for (int iI = 0; iI < GC.getNumBuildingClassInfos(); iI++) {
		BuildingTypes eBuilding = (BuildingTypes)kCivilization.getCivilizationBuildings((BuildingClassTypes)iI);

		if (eBuilding != NO_BUILDING && city.canConstruct(eBuilding, false, true, false)) {
			int iChange = city.getAdditionalCommerceTimes100ByBuilding(eIndex, eBuilding);

			if (iChange != 0) {
				if (!bStarted) {
					szBuffer.append(szStart);
					bStarted = true;
				}

				CvWString szLabel;
				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				setResumableValueTimes100ChangeHelp(szBuffer, szLabel, L": ", L"", iChange, kCommerce.getChar(), true);
			}
		}
	}

	return bStarted;
}

bool CvGameTextMgr::setBuildingSavedMaintenanceHelp(CvWStringBuffer& szBuffer, const CvCity& city, const CvWString& szStart, bool bStarted) {
	const CvCommerceInfo& kCommerce = GC.getCommerceInfo(COMMERCE_GOLD);
	const CvCivilizationInfo& kCivilization = GC.getCivilizationInfo(GET_PLAYER(city.getOwnerINLINE()).getCivilizationType());

	for (int iI = 0; iI < GC.getNumBuildingClassInfos(); iI++) {
		BuildingTypes eBuilding = (BuildingTypes)kCivilization.getCivilizationBuildings((BuildingClassTypes)iI);

		if (eBuilding != NO_BUILDING && city.canConstruct(eBuilding, false, true, false)) {
			int iChange = city.getSavedMaintenanceTimes100ByBuilding(eBuilding);

			if (iChange != 0) {
				if (!bStarted) {
					szBuffer.append(szStart);
					bStarted = true;
				}

				CvWString szLabel;
				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				setResumableValueTimes100ChangeHelp(szBuffer, szLabel, L": ", L"", iChange, kCommerce.getChar(), true);
			}
		}
	}

	return bStarted;
}

void CvGameTextMgr::setProductionHelp(CvWStringBuffer& szBuffer, CvCity& city) {
	FAssertMsg(NO_PLAYER != city.getOwnerINLINE(), "City must have an owner");

	bool bIsProcess = city.isProductionProcess();
	int iPastOverflow = (bIsProcess ? 0 : city.getOverflowProduction());
	if (iPastOverflow != 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_OVERFLOW", iPastOverflow));
		szBuffer.append(NEWLINE);
	}

	int iFromChops = (city.isProductionProcess() ? 0 : city.getFeatureProduction());
	if (iFromChops != 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_CHOPS", iFromChops));
		szBuffer.append(NEWLINE);
	}

	bool bBuildingAdditionalYield = getOptionBOOL("MiscHover__BuildingAdditionalProduction", true);
	if (city.getCurrentProductionDifference(false, true) == 0 && !bBuildingAdditionalYield) {
		return;
	}

	setYieldHelp(szBuffer, city, YIELD_PRODUCTION);

	int iBaseProduction = city.getBaseYieldRate(YIELD_PRODUCTION) + iPastOverflow + iFromChops;
	int iBaseModifier = city.getBaseYieldRateModifier(YIELD_PRODUCTION);

	UnitTypes eUnit = city.getProductionUnit();
	if (NO_UNIT != eUnit) {
		const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);

		// Domain
		int iDomainMod = city.getDomainProductionModifier((DomainTypes)kUnit.getDomainType());
		if (0 != iDomainMod) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_DOMAIN", iDomainMod, GC.getDomainInfo((DomainTypes)kUnit.getDomainType()).getTextKeyWide()));
			szBuffer.append(NEWLINE);
			iBaseModifier += iDomainMod;
		}

		// Military
		if (kUnit.isMilitaryProduction()) {
			int iMilitaryMod = city.getMilitaryProductionModifier() + GET_PLAYER(city.getOwnerINLINE()).getMilitaryProductionModifier();
			if (0 != iMilitaryMod) {
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_MILITARY", iMilitaryMod));
				szBuffer.append(NEWLINE);
				iBaseModifier += iMilitaryMod;
			}
		}

		// Bonus
		for (int i = 0; i < GC.getNumBonusInfos(); i++) {
			if (city.hasBonus((BonusTypes)i)) {
				int iBonusMod = kUnit.getBonusProductionModifier(i);
				if (0 != iBonusMod) {
					szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_MOD_BONUS", iBonusMod, kUnit.getTextKeyWide(), GC.getBonusInfo((BonusTypes)i).getTextKeyWide()));
					szBuffer.append(NEWLINE);
					iBaseModifier += iBonusMod;
				}
			}
		}

		// Trait
		for (int i = 0; i < GC.getNumTraitInfos(); i++) {
			if (city.hasTrait((TraitTypes)i)) {
				int iTraitMod = kUnit.getProductionTraits(i);

				if (kUnit.getSpecialUnitType() != NO_SPECIALUNIT) {
					iTraitMod += GC.getSpecialUnitInfo((SpecialUnitTypes)kUnit.getSpecialUnitType()).getProductionTraits(i);
				}
				if (0 != iTraitMod) {
					szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_TRAIT", iTraitMod, kUnit.getTextKeyWide(), GC.getTraitInfo((TraitTypes)i).getTextKeyWide()));
					szBuffer.append(NEWLINE);
					iBaseModifier += iTraitMod;
				}
			}
		}

		// Religion
		if (NO_PLAYER != city.getOwnerINLINE() && NO_RELIGION != GET_PLAYER(city.getOwnerINLINE()).getStateReligion()) {
			if (city.isHasReligion(GET_PLAYER(city.getOwnerINLINE()).getStateReligion())) {
				int iReligionMod = GET_PLAYER(city.getOwnerINLINE()).getStateReligionUnitProductionModifier();
				if (0 != iReligionMod) {
					szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_RELIGION", iReligionMod, GC.getReligionInfo(GET_PLAYER(city.getOwnerINLINE()).getStateReligion()).getTextKeyWide()));
					szBuffer.append(NEWLINE);
					iBaseModifier += iReligionMod;
				}
			}
		}
	}

	BuildingTypes eBuilding = city.getProductionBuilding();
	if (NO_BUILDING != eBuilding) {
		const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);

		// Bonus
		for (int i = 0; i < GC.getNumBonusInfos(); i++) {
			if (city.hasBonus((BonusTypes)i)) {
				int iBonusMod = kBuilding.getBonusProductionModifier(i);
				if (0 != iBonusMod) {
					szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_MOD_BONUS", iBonusMod, kBuilding.getTextKeyWide(), GC.getBonusInfo((BonusTypes)i).getTextKeyWide()));
					szBuffer.append(NEWLINE);
					iBaseModifier += iBonusMod;
				}
			}
		}

		// Trait
		for (int i = 0; i < GC.getNumTraitInfos(); i++) {
			if (city.hasTrait((TraitTypes)i)) {
				int iTraitMod = kBuilding.getProductionTraits(i);

				if (kBuilding.getSpecialBuildingType() != NO_SPECIALBUILDING) {
					iTraitMod += GC.getSpecialBuildingInfo((SpecialBuildingTypes)kBuilding.getSpecialBuildingType()).getProductionTraits(i);
				}
				if (0 != iTraitMod) {
					szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_TRAIT", iTraitMod, kBuilding.getTextKeyWide(), GC.getTraitInfo((TraitTypes)i).getTextKeyWide()));
					szBuffer.append(NEWLINE);
					iBaseModifier += iTraitMod;
				}
			}
		}

		// Wonder
		if (isWorldWonderClass((BuildingClassTypes)(GC.getBuildingInfo(eBuilding).getBuildingClassType())) && NO_PLAYER != city.getOwnerINLINE()) {
			int iWonderMod = GET_PLAYER(city.getOwnerINLINE()).getMaxGlobalBuildingProductionModifier();
			if (0 != iWonderMod) {
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_WONDER", iWonderMod));
				szBuffer.append(NEWLINE);
				iBaseModifier += iWonderMod;
			}
		}

		// Team Wonder
		if (isTeamWonderClass((BuildingClassTypes)(GC.getBuildingInfo(eBuilding).getBuildingClassType())) && NO_PLAYER != city.getOwnerINLINE()) {
			int iWonderMod = GET_PLAYER(city.getOwnerINLINE()).getMaxTeamBuildingProductionModifier();
			if (0 != iWonderMod) {
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_TEAM_WONDER", iWonderMod));
				szBuffer.append(NEWLINE);
				iBaseModifier += iWonderMod;
			}
		}

		// National Wonder
		if (isNationalWonderClass((BuildingClassTypes)(GC.getBuildingInfo(eBuilding).getBuildingClassType())) && NO_PLAYER != city.getOwnerINLINE()) {
			int iWonderMod = GET_PLAYER(city.getOwnerINLINE()).getMaxPlayerBuildingProductionModifier();
			if (0 != iWonderMod) {
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_NATIONAL_WONDER", iWonderMod));
				szBuffer.append(NEWLINE);
				iBaseModifier += iWonderMod;
			}
		}

		// Religion
		if (NO_PLAYER != city.getOwnerINLINE() && NO_RELIGION != GET_PLAYER(city.getOwnerINLINE()).getStateReligion()) {
			if (city.isHasReligion(GET_PLAYER(city.getOwnerINLINE()).getStateReligion())) {
				int iReligionMod = GET_PLAYER(city.getOwnerINLINE()).getStateReligionBuildingProductionModifier();
				if (0 != iReligionMod) {
					szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_RELIGION", iReligionMod, GC.getReligionInfo(GET_PLAYER(city.getOwnerINLINE()).getStateReligion()).getTextKeyWide()));
					szBuffer.append(NEWLINE);
					iBaseModifier += iReligionMod;
				}
			}
		}
	}

	ProjectTypes eProject = city.getProductionProject();
	if (NO_PROJECT != eProject) {
		const CvProjectInfo& kProject = GC.getProjectInfo(eProject);

		// Spaceship
		if (kProject.isSpaceship()) {
			int iSpaceshipMod = city.getSpaceProductionModifier();
			if (NO_PLAYER != city.getOwnerINLINE()) {
				iSpaceshipMod += GET_PLAYER(city.getOwnerINLINE()).getSpaceProductionModifier();
			}
			if (0 != iSpaceshipMod) {
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_SPACESHIP", iSpaceshipMod));
				szBuffer.append(NEWLINE);
				iBaseModifier += iSpaceshipMod;
			}
		}

		// Bonus
		for (int i = 0; i < GC.getNumBonusInfos(); i++) {
			if (city.hasBonus((BonusTypes)i)) {
				int iBonusMod = kProject.getBonusProductionModifier(i);
				if (0 != iBonusMod) {
					szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_MOD_BONUS", iBonusMod, kProject.getTextKeyWide(), GC.getBonusInfo((BonusTypes)i).getTextKeyWide()));
					szBuffer.append(NEWLINE);
					iBaseModifier += iBonusMod;
				}
			}
		}
	}

	int iFoodProduction = (city.isFoodProduction() ? std::max(0, (city.getYieldRate(YIELD_FOOD) - city.foodConsumption(true))) : 0);
	if (iFoodProduction > 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_FOOD", iFoodProduction, iFoodProduction));
		szBuffer.append(NEWLINE);
	}

	int iModProduction = iFoodProduction + (iBaseModifier * iBaseProduction) / 100;

	FAssertMsg(iModProduction == city.getCurrentProductionDifference(false, !bIsProcess), "Modified Production does not match actual value");

	szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PROD_FINAL_YIELD", iModProduction));

	if (bBuildingAdditionalYield && city.getOwnerINLINE() == GC.getGame().getActivePlayer()) {
		setBuildingAdditionalYieldHelp(szBuffer, city, YIELD_PRODUCTION, DOUBLE_SEPARATOR);
	}
}


void CvGameTextMgr::parsePlayerTraits(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) {
	bool bFirst = true;

	for (int iTrait = 0; iTrait < GC.getNumTraitInfos(); ++iTrait) {
		if (GET_PLAYER(ePlayer).hasTrait((TraitTypes)iTrait)) {
			if (bFirst) {
				szBuffer.append(L" (");
				bFirst = false;
			} else {
				szBuffer.append(L", ");
			}
			szBuffer.append(GC.getTraitInfo((TraitTypes)iTrait).getDescription());
		}
	}

	if (!bFirst) {
		szBuffer.append(L")");
	}
}

// K-Mod. I've rewritten most of this function.
void CvGameTextMgr::parseLeaderHeadHelp(CvWStringBuffer& szBuffer, PlayerTypes eThisPlayer, PlayerTypes eOtherPlayer) {
	if (eThisPlayer == NO_PLAYER)
		return;

	const CvPlayerAI& kPlayer = GET_PLAYER(eThisPlayer);

	szBuffer.append(CvWString::format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), kPlayer.getName()));

	parsePlayerTraits(szBuffer, eThisPlayer);

	// Some debug info: found-site traits, and AI flavours
	if (gDLL->getChtLvl() > 0 && GC.altKey()) {
		szBuffer.append(CvWString::format(SETCOLR SEPARATOR NEWLINE, TEXT_COLOR("COLOR_LIGHT_GREY")));
		CvPlayerAI::CvFoundSettings kFoundSet(kPlayer, false);

		bool bFirst = true;

#define trait_info(x) do { \
	if (kFoundSet.b##x) \
		{ \
		szBuffer.append(CvWString::format(L"%s"L#x, bFirst? L"" : L", ")); \
		bFirst = false; \
		} \
		} while (0)

		trait_info(Ambitious);
		trait_info(Defensive);
		trait_info(EasyCulture);
		trait_info(Expansive);
		trait_info(Financial);
		trait_info(Seafaring);

#undef trait_info

#define flavour_info(x) do { \
	if (kPlayer.AI_getFlavorValue(FLAVOR_##x)) \
		{ \
		szBuffer.append(CvWString::format(L"%s"L#x L"=%d", bFirst? L"" : L", ", kPlayer.AI_getFlavorValue(FLAVOR_##x))); \
		bFirst = false; \
		} \
		} while (0)

		flavour_info(MILITARY);
		flavour_info(RELIGION);
		flavour_info(PRODUCTION);
		flavour_info(GOLD);
		flavour_info(SCIENCE);
		flavour_info(CULTURE);
		flavour_info(GROWTH);

#undef flavour_info

		szBuffer.append(SEPARATOR ENDCOLR);
	}

	if (eOtherPlayer != NO_PLAYER) {
		const CvTeam& kThisTeam = GET_TEAM(kPlayer.getTeam());
		TeamTypes eOtherTeam = GET_PLAYER(eOtherPlayer).getTeam();

		if (kThisTeam.isHasMet(eOtherTeam)) // K-Mod. Allow the "other relations string" to display even if eOtherPlayer == eThisPlayer. It's useful info.
		{
			getVassalInfoString(szBuffer, eThisPlayer); // K-Mod

			if (eOtherPlayer != eThisPlayer) {
				getAttitudeString(szBuffer, eThisPlayer, eOtherPlayer);
				getActiveDealsString(szBuffer, eThisPlayer, eOtherPlayer);
			}
			getWarWearinessString(szBuffer, eThisPlayer, eOtherPlayer);

			getOtherRelationsString(szBuffer, eThisPlayer, eOtherPlayer);
		}
	} else
		getWarWearinessString(szBuffer, eThisPlayer, NO_PLAYER); // total war weariness
}


void CvGameTextMgr::parseLeaderLineHelp(CvWStringBuffer& szBuffer, PlayerTypes eThisPlayer, PlayerTypes eOtherPlayer) {
	if (NO_PLAYER == eThisPlayer || NO_PLAYER == eOtherPlayer) {
		return;
	}
	const CvTeam& thisTeam = GET_TEAM(GET_PLAYER(eThisPlayer).getTeam());
	const CvTeam& otherTeam = GET_TEAM(GET_PLAYER(eOtherPlayer).getTeam());

	if (thisTeam.getID() == otherTeam.getID()) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_PERMANENT_ALLIANCE"));
		szBuffer.append(NEWLINE);
	} else if (thisTeam.isAtWar(otherTeam.getID())) {
		szBuffer.append(gDLL->getText("TXT_KEY_CONCEPT_WAR"));
		szBuffer.append(NEWLINE);
	} else {
		if (thisTeam.isDefensivePact(otherTeam.getID())) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEFENSIVE_PACT"));
			szBuffer.append(NEWLINE);
		}
		if (thisTeam.isHasEmbassy(otherTeam.getID()) || otherTeam.isHasEmbassy(thisTeam.getID())) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_EMBASSY"));
			szBuffer.append(NEWLINE);
		}
		if (thisTeam.isLimitedBorders(otherTeam.getID()) || otherTeam.isLimitedBorders(thisTeam.getID())) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_LIMITED_BORDERS"));
			szBuffer.append(NEWLINE);
		}
		if (thisTeam.isFreeTradeAgreement(otherTeam.getID()) || otherTeam.isFreeTradeAgreement(thisTeam.getID())) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_FREE_TRADE"));
			szBuffer.append(NEWLINE);
		}
		if (thisTeam.isHasNonAggression(otherTeam.getID()) || otherTeam.isHasNonAggression(thisTeam.getID())) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_NON_AGGRESSION"));
			szBuffer.append(NEWLINE);
		}
		if (thisTeam.isOpenBorders(otherTeam.getID())) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_OPEN_BORDERS"));
			szBuffer.append(NEWLINE);
		}
		if (thisTeam.isVassal(otherTeam.getID())) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_VASSAL"));
			szBuffer.append(NEWLINE);
		}
	}
}


void CvGameTextMgr::getActiveDealsString(CvWStringBuffer& szBuffer, PlayerTypes eThisPlayer, PlayerTypes eOtherPlayer) {
	int iIndex;
	CvDeal* pDeal = GC.getGameINLINE().firstDeal(&iIndex);
	while (NULL != pDeal) {
		if ((pDeal->getFirstPlayer() == eThisPlayer && pDeal->getSecondPlayer() == eOtherPlayer)
			|| (pDeal->getFirstPlayer() == eOtherPlayer && pDeal->getSecondPlayer() == eThisPlayer)) {
			szBuffer.append(NEWLINE);
			szBuffer.append(CvWString::format(L"%c", gDLL->getSymbolID(BULLET_CHAR)));
			getDealString(szBuffer, *pDeal, eThisPlayer);
		}
		pDeal = GC.getGameINLINE().nextDeal(&iIndex);
	}
}

void CvGameTextMgr::getOtherRelationsString(CvWStringBuffer& szString, PlayerTypes eThisPlayer, PlayerTypes eOtherPlayer) {
	if (eThisPlayer == NO_PLAYER || eOtherPlayer == NO_PLAYER) {
		return;
	}
	const CvPlayer& kThisPlayer = GET_PLAYER(eThisPlayer);
	const CvPlayer& kOtherPlayer = GET_PLAYER(eOtherPlayer);

	// Put all war, worst enemy strings on one line
	CvWStringBuffer szWarWithString;
	CvWStringBuffer szWorstEnemyString;
	bool bFirst = true;
	bool bFirst2 = true;
	for (int iTeam = 0; iTeam < MAX_CIV_TEAMS; ++iTeam) {
		CvTeamAI& kTeam = GET_TEAM((TeamTypes)iTeam);
		if (kTeam.isAlive() && !kTeam.isMinorCiv() && iTeam != kThisPlayer.getTeam()) // K-Mod. (show "at war" even for the civ selected.)
		{
			if (kTeam.isHasMet(kOtherPlayer.getTeam()) && kTeam.isHasMet(kThisPlayer.getTeam()) && kTeam.isHasMet(GC.getGameINLINE().getActiveTeam())) {
				if (::atWar((TeamTypes)iTeam, kThisPlayer.getTeam())) {
					setListHelp(szWarWithString, L"", kTeam.getName().GetCString(), L", ", bFirst);
					bFirst = false;
				}

				if (!kTeam.isHuman() && kTeam.AI_getWorstEnemy() == kThisPlayer.getTeam()) {
					setListHelp(szWorstEnemyString, L"", kTeam.getName().GetCString(), L", ", bFirst2);
					bFirst2 = false;
				}
			}
		}
	}

	if (!szWorstEnemyString.isEmpty()) {
		CvWString szTempBuffer;

		szTempBuffer.assign(gDLL->getText(L"TXT_KEY_WORST_ENEMY_OF", szWorstEnemyString));

		szString.append(NEWLINE);
		szString.append(szTempBuffer);
	}
	if (!szWarWithString.isEmpty()) {
		CvWString szTempBuffer;

		szTempBuffer.assign(gDLL->getText(L"TXT_KEY_AT_WAR_WITH", szWarWithString));

		szString.append(NEWLINE);
		szString.append(szTempBuffer);
	}
}

void CvGameTextMgr::buildHintsList(CvWStringBuffer& szBuffer) {
	for (int i = 0; i < GC.getNumHints(); i++) {
		szBuffer.append(CvWString::format(L"%c%s", gDLL->getSymbolID(BULLET_CHAR), GC.getHints(i).getText()));
		szBuffer.append(NEWLINE);
		szBuffer.append(NEWLINE);
	}
}

void CvGameTextMgr::setCommerceHelp(CvWStringBuffer& szBuffer, CvCity& city, CommerceTypes eCommerceType) {
	bool bBuildingAdditionalCommerce = getOptionBOOL("MiscHover__BuildingAdditionalCommerce", true);
	if (NO_COMMERCE == eCommerceType || (0 == city.getCommerceRateTimes100(eCommerceType) && !bBuildingAdditionalCommerce)) {
		return;
	}
	CvCommerceInfo& kCommerce = GC.getCommerceInfo(eCommerceType);

	if (NO_PLAYER == city.getOwnerINLINE()) {
		return;
	}
	const CvPlayer& owner = GET_PLAYER(city.getOwnerINLINE());

	setYieldHelp(szBuffer, city, YIELD_COMMERCE);

	// Slider
	int iBaseCommerceRate = city.getCommerceFromPercent(eCommerceType, city.getYieldRate(YIELD_COMMERCE) * 100);
	CvWString szRate = CvWString::format(L"%d.%02d", iBaseCommerceRate / 100, iBaseCommerceRate % 100);
	szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_SLIDER_PERCENT_FLOAT", owner.getCommercePercent(eCommerceType), city.getYieldRate(YIELD_COMMERCE), szRate.GetCString(), kCommerce.getChar()));
	szBuffer.append(NEWLINE);

	bool bNeedSubtotal = false;

	int iSpecialistCommerce = city.getSpecialistCommerce(eCommerceType) + (city.getSpecialistPopulation() + city.getNumGreatPeople()) * owner.getSpecialistExtraCommerce(eCommerceType);
	if (0 != iSpecialistCommerce) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_SPECIALIST_COMMERCE", iSpecialistCommerce, kCommerce.getChar(), L"TXT_KEY_CONCEPT_SPECIALISTS"));
		szBuffer.append(NEWLINE);
		iBaseCommerceRate += 100 * iSpecialistCommerce;
		bNeedSubtotal = true;
	}

	int iReligionCommerce = city.getReligionCommerce(eCommerceType);
	if (0 != iReligionCommerce) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_RELIGION_COMMERCE", iReligionCommerce, kCommerce.getChar()));
		szBuffer.append(NEWLINE);
		iBaseCommerceRate += 100 * iReligionCommerce;
		bNeedSubtotal = true;
	}

	int iCorporationCommerce = city.getCorporationCommerce(eCommerceType);
	if (0 != iCorporationCommerce) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_CORPORATION_COMMERCE", iCorporationCommerce, kCommerce.getChar()));
		szBuffer.append(NEWLINE);
		iBaseCommerceRate += 100 * iCorporationCommerce;
		bNeedSubtotal = true;
	}

	int iBuildingCommerce = city.getBuildingCommerce(eCommerceType);
	if (0 != iBuildingCommerce) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_BUILDING_COMMERCE", iBuildingCommerce, kCommerce.getChar()));
		szBuffer.append(NEWLINE);
		iBaseCommerceRate += 100 * iBuildingCommerce;
		bNeedSubtotal = true;
	}

	int iFreeCityCommerce = owner.getFreeCityCommerce(eCommerceType);
	if (0 != iFreeCityCommerce) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_FREE_CITY_COMMERCE", iFreeCityCommerce, kCommerce.getChar()));
		szBuffer.append(NEWLINE);
		iBaseCommerceRate += 100 * iFreeCityCommerce;
		bNeedSubtotal = true;
	}

	if (bNeedSubtotal && city.getCommerceRateModifier(eCommerceType) != 0 && getOptionBOOL("MiscHover__BaseCommerce", true)) {
		CvWString szYield = CvWString::format(L"%d.%02d", iBaseCommerceRate / 100, iBaseCommerceRate % 100);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_COMMERCE_SUBTOTAL_YIELD_FLOAT", kCommerce.getTextKeyWide(), szYield.GetCString(), kCommerce.getChar()));
		szBuffer.append(NEWLINE);
	}

	FAssertMsg(city.getBaseCommerceRateTimes100(eCommerceType) == iBaseCommerceRate, "Base Commerce rate does not agree with actual value");

	int iModifier = 100;

	// Buildings
	int iBuildingMod = 0;
	for (BuildingTypes eBuilding = (BuildingTypes)0; eBuilding < GC.getNumBuildingInfos(); eBuilding = (BuildingTypes)(eBuilding + 1)) {
		const CvBuildingInfo& kCommerceBuilding = GC.getBuildingInfo(eBuilding);
		if (city.getNumBuilding(eBuilding) > 0 && !city.isObsoleteBuilding(eBuilding)) {
			for (int iLoop = 0; iLoop < city.getNumBuilding(eBuilding); iLoop++) {
				iBuildingMod += kCommerceBuilding.getCommerceModifier(eCommerceType);
			}
		}
		for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
			const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
			if (kPlayer.isAlive()) {
				if (kPlayer.getTeam() == owner.getTeam()) {
					int iLoop;
					for (CvCity* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop)) {
						if (pLoopCity->getNumBuilding(eBuilding) > 0 && !pLoopCity->isObsoleteBuilding(eBuilding)) {
							for (int iLoop = 0; iLoop < pLoopCity->getNumBuilding(eBuilding); iLoop++) {
								iBuildingMod += kCommerceBuilding.getGlobalCommerceModifier(eCommerceType);
							}
						}
					}
				}
			}
		}
	}
	if (0 != iBuildingMod) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_YIELD_BUILDINGS", iBuildingMod, kCommerce.getChar()));
		szBuffer.append(NEWLINE);
		iModifier += iBuildingMod;
	}


	// Trait
	for (TraitTypes eTrait = (TraitTypes)0; eTrait < GC.getNumTraitInfos(); eTrait = (TraitTypes)(eTrait + 1)) {
		if (city.hasTrait(eTrait)) {
			const CvTraitInfo& kTrait = GC.getTraitInfo(eTrait);
			int iTraitMod = kTrait.getCommerceModifier(eCommerceType);
			if (0 != iTraitMod) {
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_COMMERCE_TRAIT", iTraitMod, kCommerce.getChar(), kTrait.getTextKeyWide()));
				szBuffer.append(NEWLINE);
				iModifier += iTraitMod;
			}
		}
	}

	// Capital
	int iCapitalMod = city.isCapital() ? owner.getCapitalCommerceRateModifier(eCommerceType) : 0;
	if (iCapitalMod != 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_YIELD_CAPITAL", iCapitalMod, kCommerce.getChar()));
		szBuffer.append(NEWLINE);
		iModifier += iCapitalMod;
	}


	// Civics
	int iCivicMod = 0;
	for (CivicOptionTypes eCivicOption = (CivicOptionTypes)0; eCivicOption < GC.getNumCivicOptionInfos(); eCivicOption = (CivicOptionTypes)(eCivicOption + 1)) {
		if (NO_CIVIC != owner.getCivics(eCivicOption)) {
			iCivicMod += GC.getCivicInfo(owner.getCivics(eCivicOption)).getCommerceModifier(eCommerceType);
		}
	}
	if (0 != iCivicMod) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_YIELD_CIVICS", iCivicMod, kCommerce.getChar()));
		szBuffer.append(NEWLINE);
		iModifier += iCivicMod;
	}

	int iModYield = (iModifier * iBaseCommerceRate) / 100;

	int iProductionToCommerce = city.getProductionToCommerceModifier(eCommerceType) * city.getYieldRate(YIELD_PRODUCTION);
	if (0 != iProductionToCommerce) {
		if (iProductionToCommerce % 100 == 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PRODUCTION_TO_COMMERCE", iProductionToCommerce / 100, kCommerce.getChar()));
			szBuffer.append(NEWLINE);
		} else {
			szRate = CvWString::format(L"+%d.%02d", iProductionToCommerce / 100, iProductionToCommerce % 100);
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_PRODUCTION_TO_COMMERCE_FLOAT", szRate.GetCString(), kCommerce.getChar()));
			szBuffer.append(NEWLINE);
		}
		iModYield += iProductionToCommerce;
	}

	if (eCommerceType == COMMERCE_CULTURE && GC.getGameINLINE().isOption(GAMEOPTION_NO_ESPIONAGE)) {
		int iEspionageToCommerce = city.getCommerceRateTimes100(COMMERCE_CULTURE) - iModYield;
		if (0 != iEspionageToCommerce) {
			if (iEspionageToCommerce % 100 == 0) {
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_COMMERCE_TO_COMMERCE", iEspionageToCommerce / 100, kCommerce.getChar(), GC.getCommerceInfo(COMMERCE_ESPIONAGE).getChar()));
				szBuffer.append(NEWLINE);
			} else {
				szRate = CvWString::format(L"+%d.%02d", iEspionageToCommerce / 100, iEspionageToCommerce % 100);
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_COMMERCE_TO_COMMERCE_FLOAT", szRate.GetCString(), kCommerce.getChar(), GC.getCommerceInfo(COMMERCE_ESPIONAGE).getChar()));
				szBuffer.append(NEWLINE);
			}
			iModYield += iEspionageToCommerce;
		}
	}

	FAssertMsg(iModYield == city.getCommerceRateTimes100(eCommerceType), "Commerce yield does not match actual value");

	CvWString szYield = CvWString::format(L"%d.%02d", iModYield / 100, iModYield % 100);
	szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_COMMERCE_FINAL_YIELD_FLOAT", kCommerce.getTextKeyWide(), szYield.GetCString(), kCommerce.getChar()));

	if (bBuildingAdditionalCommerce && city.getOwnerINLINE() == GC.getGame().getActivePlayer()) {
		setBuildingAdditionalCommerceHelp(szBuffer, city, eCommerceType, DOUBLE_SEPARATOR);
	}
}

void CvGameTextMgr::setYieldHelp(CvWStringBuffer& szBuffer, CvCity& city, YieldTypes eYieldType) {
	FAssertMsg(NO_PLAYER != city.getOwnerINLINE(), "City must have an kOwner");

	if (NO_YIELD == eYieldType) {
		return;
	}
	const CvYieldInfo& kYield = GC.getYieldInfo(eYieldType);

	if (NO_PLAYER == city.getOwnerINLINE()) {
		return;
	}
	const CvPlayer& kOwner = GET_PLAYER(city.getOwnerINLINE());

	int iBaseProduction = city.getBaseYieldRate(eYieldType);
	szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_BASE_YIELD", kYield.getTextKeyWide(), iBaseProduction, kYield.getChar()));
	szBuffer.append(NEWLINE);

	int iBaseModifier = 100;

	// Buildings
	int iBuildingMod = 0;
	for (BuildingTypes eBuilding = (BuildingTypes)0; eBuilding < GC.getNumBuildingInfos(); eBuilding = (BuildingTypes)(eBuilding + 1)) {
		const CvBuildingInfo& kYieldBuilding = GC.getBuildingInfo(eBuilding);
		if (city.getNumBuilding(eBuilding) > 0 && !city.isObsoleteBuilding(eBuilding)) {
			for (int iLoop = 0; iLoop < city.getNumBuilding(eBuilding); iLoop++) {
				iBuildingMod += kYieldBuilding.getYieldModifier(eYieldType);
			}
		}
		for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
			const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
			if (kPlayer.isAlive()) {
				if (kPlayer.getTeam() == kOwner.getTeam()) {
					int iLoop;
					for (CvCity* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop)) {
						if (pLoopCity->getNumBuilding(eBuilding) > 0 && !pLoopCity->isObsoleteBuilding(eBuilding)) {
							for (int iLoop = 0; iLoop < pLoopCity->getNumBuilding(eBuilding); iLoop++) {
								iBuildingMod += kYieldBuilding.getGlobalYieldModifier(eYieldType);
							}
						}
					}
				}
			}
		}
	}
	if (NULL != city.area()) {
		iBuildingMod += city.area()->getYieldRateModifier(city.getOwnerINLINE(), eYieldType);
	}
	if (0 != iBuildingMod) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_YIELD_BUILDINGS", iBuildingMod, kYield.getChar()));
		szBuffer.append(NEWLINE);
		iBaseModifier += iBuildingMod;
	}

	// Power
	if (city.isPower()) {
		int iPowerMod = city.getPowerYieldRateModifier(eYieldType);
		if (0 != iPowerMod) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_YIELD_POWER", iPowerMod, kYield.getChar()));
			szBuffer.append(NEWLINE);
			iBaseModifier += iPowerMod;
		}
	}

	// Resources
	int iBonusMod = city.getBonusYieldRateModifier(eYieldType);
	if (0 != iBonusMod) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_YIELD_BONUS", iBonusMod, kYield.getChar()));
		szBuffer.append(NEWLINE);
		iBaseModifier += iBonusMod;
	}

	// Capital
	if (city.isCapital()) {
		int iCapitalMod = kOwner.getCapitalYieldRateModifier(eYieldType);
		if (0 != iCapitalMod) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_YIELD_CAPITAL", iCapitalMod, kYield.getChar()));
			szBuffer.append(NEWLINE);
			iBaseModifier += iCapitalMod;
		}
	}

	// Civics
	int iCivicMod = 0;
	for (CivicOptionTypes eCivicOption = (CivicOptionTypes)0; eCivicOption < GC.getNumCivicOptionInfos(); eCivicOption = (CivicOptionTypes)(eCivicOption + 1)) {
		if (NO_CIVIC != kOwner.getCivics(eCivicOption)) {
			iCivicMod += GC.getCivicInfo(kOwner.getCivics(eCivicOption)).getYieldModifier(eYieldType);
		}
	}
	if (0 != iCivicMod) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_YIELD_CIVICS", iCivicMod, kYield.getChar()));
		szBuffer.append(NEWLINE);
		iBaseModifier += iCivicMod;
	}

	FAssertMsg((iBaseModifier * iBaseProduction) / 100 == city.getYieldRate(eYieldType), "Yield Modifier in setProductionHelp does not agree with actual value");
}

void CvGameTextMgr::setConvertHelp(CvWStringBuffer& szBuffer, PlayerTypes ePlayer, ReligionTypes eReligion) {
	CvWString szReligion = L"TXT_KEY_MISC_NO_STATE_RELIGION";

	if (eReligion != NO_RELIGION) {
		szReligion = GC.getReligionInfo(eReligion).getTextKeyWide();
	}

	szBuffer.assign(gDLL->getText("TXT_KEY_MISC_CANNOT_CONVERT_TO", szReligion.GetCString()));

	if (GET_PLAYER(ePlayer).isAnarchy()) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_WHILE_IN_ANARCHY"));
	} else if (GET_PLAYER(ePlayer).getStateReligion() == eReligion) {
		szBuffer.append(L". ");
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ALREADY_STATE_REL"));
	} else if (GET_PLAYER(ePlayer).getConversionTimer() > 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ANOTHER_REVOLUTION_RECENTLY"));
		szBuffer.append(L". ");
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_WAIT_MORE_TURNS", GET_PLAYER(ePlayer).getConversionTimer()));
	}
}

void CvGameTextMgr::setRevolutionHelp(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) {
	szBuffer.assign(gDLL->getText("TXT_KEY_MISC_CANNOT_CHANGE_CIVICS"));

	if (GET_PLAYER(ePlayer).isAnarchy()) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_WHILE_IN_ANARCHY"));
	} else if (GET_PLAYER(ePlayer).getRevolutionTimer() > 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ANOTHER_REVOLUTION_RECENTLY"));
		szBuffer.append(L" : ");
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_WAIT_MORE_TURNS", GET_PLAYER(ePlayer).getRevolutionTimer()));
	}
}

void CvGameTextMgr::setVassalRevoltHelp(CvWStringBuffer& szBuffer, TeamTypes eMaster, TeamTypes eVassal) {
	if (NO_TEAM == eMaster || NO_TEAM == eVassal) {
		return;
	}

	if (!GET_TEAM(eVassal).isCapitulated()) {
		return;
	}

	if (GET_TEAM(eMaster).isParent(eVassal)) {
		return;
	}

	const CvTeam& kMaster = GET_TEAM(eMaster);
	const CvTeam& kVassal = GET_TEAM(eVassal);

	int iMasterLand = kMaster.getTotalLand(false);
	int iVassalLand = kVassal.getTotalLand(false);
	if (iMasterLand > 0 && GC.getDefineINT("FREE_VASSAL_LAND_PERCENT") >= 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_VASSAL_LAND_STATS", (iVassalLand * 100) / iMasterLand, GC.getDefineINT("FREE_VASSAL_LAND_PERCENT")));
	}

	int iMasterPop = kMaster.getTotalPopulation(false);
	int iVassalPop = kVassal.getTotalPopulation(false);
	if (iMasterPop > 0 && GC.getDefineINT("FREE_VASSAL_POPULATION_PERCENT") >= 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_VASSAL_POPULATION_STATS", (iVassalPop * 100) / iMasterPop, GC.getDefineINT("FREE_VASSAL_POPULATION_PERCENT")));
	}

	if (GC.getDefineINT("VASSAL_REVOLT_OWN_LOSSES_FACTOR") > 0 && kVassal.getVassalPower() > 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_VASSAL_AREA_LOSS", (iVassalLand * 100) / kVassal.getVassalPower(), GC.getDefineINT("VASSAL_REVOLT_OWN_LOSSES_FACTOR")));
	}

	if (GC.getDefineINT("VASSAL_REVOLT_MASTER_LOSSES_FACTOR") > 0 && kVassal.getMasterPower() > 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_MASTER_AREA_LOSS", (iMasterLand * 100) / kVassal.getMasterPower(), GC.getDefineINT("VASSAL_REVOLT_MASTER_LOSSES_FACTOR")));
	}
}

void CvGameTextMgr::parseGreatPeopleHelp(CvWStringBuffer& szBuffer, CvCity& city) {
	int iTotalGreatPeopleUnitProgress;

	if (NO_PLAYER == city.getOwnerINLINE()) {
		return;
	}
	const CvPlayer& kOwner = GET_PLAYER(city.getOwnerINLINE());

	szBuffer.assign(gDLL->getText("TXT_KEY_MISC_GREAT_PERSON", city.getGreatPeopleProgress(), kOwner.greatPeopleThreshold(false)));

	if (city.getGreatPeopleRate() > 0) {
		int iGPPLeft = kOwner.greatPeopleThreshold(false) - city.getGreatPeopleProgress();

		if (iGPPLeft > 0) {
			int iTurnsLeft = iGPPLeft / city.getGreatPeopleRate();

			if (iTurnsLeft * city.getGreatPeopleRate() < iGPPLeft) {
				iTurnsLeft++;
			}

			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("INTERFACE_CITY_TURNS", std::max(1, iTurnsLeft)));
		}
	}

	iTotalGreatPeopleUnitProgress = 0;

	for (UnitTypes eUnit = (UnitTypes)0; eUnit < GC.getNumUnitInfos(); eUnit = (UnitTypes)(eUnit + 1)) {
		iTotalGreatPeopleUnitProgress += city.getGreatPeopleUnitProgress(eUnit);
	}

	if (iTotalGreatPeopleUnitProgress > 0) {
		szBuffer.append(SEPARATOR);
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_PROB"));

		std::vector< std::pair<UnitTypes, int> > aUnitProgress;
		int iTotalTruncated = 0;
		for (UnitTypes eUnit = (UnitTypes)0; eUnit < GC.getNumUnitInfos(); eUnit = (UnitTypes)(eUnit + 1)) {
			int iProgress = ((city.getGreatPeopleUnitProgress(eUnit) * 100) / iTotalGreatPeopleUnitProgress);
			if (iProgress > 0) {
				iTotalTruncated += iProgress;
				aUnitProgress.push_back(std::make_pair(eUnit, iProgress));
			}
		}

		if (iTotalTruncated < 100 && aUnitProgress.size() > 0) {
			aUnitProgress[0].second += 100 - iTotalTruncated;
		}

		for (int iI = 0; iI < (int)aUnitProgress.size(); ++iI) {
			szBuffer.append(CvWString::format(L"%s%s - %d%%", NEWLINE, GC.getUnitInfo(aUnitProgress[iI].first).getDescription(), aUnitProgress[iI].second));
		}
	}

	bool bBuildingAdditionalGreatPeople = getOptionBOOL("MiscHover__BuildingAdditionalGreatPeople", true);
	if (city.getGreatPeopleRate() == 0 && !bBuildingAdditionalGreatPeople) {
		return;
	}

	if (getOptionBOOL("MiscHover__GreatPeopleRateBreakdown", true)) {
		bool bFirst = true;
		int iRate = 0;
		for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
			int iCount = city.getSpecialistCount(eSpecialist) + city.getFreeSpecialistCount(eSpecialist);
			if (iCount > 0) {
				iRate += iCount * GC.getSpecialistInfo(eSpecialist).getGreatPeopleRateChange();
			}
		}
		if (iRate > 0) {
			if (bFirst) {
				szBuffer.append(SEPARATOR);
				bFirst = false;
			}
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_SPECIALIST_COMMERCE", iRate, gDLL->getSymbolID(GREAT_PEOPLE_CHAR), L"TXT_KEY_CONCEPT_SPECIALISTS"));
		}

		iRate = 0;
		for (BuildingTypes eBuilding = (BuildingTypes)0; eBuilding < GC.getNumBuildingInfos(); eBuilding = (BuildingTypes)(eBuilding + 1)) {
			int iCount = city.getNumBuilding(eBuilding);
			if (iCount > 0) {
				iRate += iCount * GC.getBuildingInfo(eBuilding).getGreatPeopleRateChange();
			}
		}
		if (iRate > 0) {
			if (bFirst) {
				szBuffer.append(SEPARATOR);
				bFirst = false;
			}
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_BUILDING_COMMERCE", iRate, gDLL->getSymbolID(GREAT_PEOPLE_CHAR)));
		}
	}

	szBuffer.append(SEPARATOR);
	szBuffer.append(NEWLINE);
	szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_GREATPEOPLE_BASE_RATE", city.getBaseGreatPeopleRate()));
	szBuffer.append(NEWLINE);

	int iModifier = 100;

	// Buildings
	int iBuildingMod = 0;
	for (BuildingTypes eBuilding = (BuildingTypes)0; eBuilding < GC.getNumBuildingInfos(); eBuilding = (BuildingTypes)(eBuilding + 1)) {
		const CvBuildingInfo& infoBuilding = GC.getBuildingInfo(eBuilding);
		if (city.getNumBuilding(eBuilding) > 0 && !city.isObsoleteBuilding(eBuilding)) {
			for (int iLoop = 0; iLoop < city.getNumBuilding(eBuilding); iLoop++) {
				iBuildingMod += infoBuilding.getGreatPeopleRateModifier();
			}
		}
		for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
			const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
			if (kPlayer.isAlive()) {
				if (kPlayer.getTeam() == kOwner.getTeam()) {
					int iLoop;
					for (CvCity* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop)) {
						if (pLoopCity->getNumBuilding(eBuilding) > 0 && !pLoopCity->isObsoleteBuilding(eBuilding)) {
							for (int iLoop = 0; iLoop < pLoopCity->getNumBuilding(eBuilding); iLoop++) {
								iBuildingMod += infoBuilding.getGlobalGreatPeopleRateModifier();
							}
						}
					}
				}
			}
		}
	}
	if (0 != iBuildingMod) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_GREATPEOPLE_BUILDINGS", iBuildingMod));
		szBuffer.append(NEWLINE);
		iModifier += iBuildingMod;
	}

	// Civics
	int iCivicMod = 0;
	for (CivicOptionTypes eCivicOption = (CivicOptionTypes)0; eCivicOption < GC.getNumCivicOptionInfos(); eCivicOption = (CivicOptionTypes)(eCivicOption + 1)) {
		if (NO_CIVIC != kOwner.getCivics(eCivicOption)) {
			iCivicMod += GC.getCivicInfo(kOwner.getCivics(eCivicOption)).getGreatPeopleRateModifier();
			if (kOwner.getStateReligion() != NO_RELIGION && city.isHasReligion(kOwner.getStateReligion())) {
				iCivicMod += GC.getCivicInfo(kOwner.getCivics(eCivicOption)).getStateReligionGreatPeopleRateModifier();
			}
		}
	}
	if (0 != iCivicMod) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_GREATPEOPLE_CIVICS", iCivicMod));
		szBuffer.append(NEWLINE);
		iModifier += iCivicMod;
	}

	// Trait
	for (TraitTypes eTrait = (TraitTypes)0; eTrait < GC.getNumTraitInfos(); eTrait = (TraitTypes)(eTrait + 1)) {
		if (city.hasTrait(eTrait)) {
			const CvTraitInfo& kTrait = GC.getTraitInfo(eTrait);
			int iTraitMod = kTrait.getGreatPeopleRateModifier();
			if (0 != iTraitMod) {
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_GREATPEOPLE_TRAIT", iTraitMod, kTrait.getTextKeyWide()));
				szBuffer.append(NEWLINE);
				iModifier += iTraitMod;
			}
		}
	}

	if (kOwner.isGoldenAge()) {
		int iGoldenAgeMod = GC.getDefineINT("GOLDEN_AGE_GREAT_PEOPLE_MODIFIER");

		if (0 != iGoldenAgeMod) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_GREATPEOPLE_GOLDEN_AGE", iGoldenAgeMod));
			szBuffer.append(NEWLINE);
			iModifier += iGoldenAgeMod;
		}
	}

	int iModGreatPeople = (iModifier * city.getBaseGreatPeopleRate()) / 100;

	FAssertMsg(iModGreatPeople == city.getGreatPeopleRate(), "Great person rate does not match actual value");

	szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_GREATPEOPLE_FINAL", iModGreatPeople));
	if (bBuildingAdditionalGreatPeople && city.getOwnerINLINE() == GC.getGame().getActivePlayer()) {
		setBuildingAdditionalGreatPeopleHelp(szBuffer, city, DOUBLE_SEPARATOR);
	}
}

bool CvGameTextMgr::setBuildingAdditionalGreatPeopleHelp(CvWStringBuffer& szBuffer, const CvCity& city, const CvWString& szStart, bool bStarted) {
	CvWString szLabel;
	const CvCivilizationInfo& kCivilization = GC.getCivilizationInfo(GET_PLAYER(city.getOwnerINLINE()).getCivilizationType());

	for (int iI = 0; iI < GC.getNumBuildingClassInfos(); iI++) {
		BuildingTypes eBuilding = (BuildingTypes)kCivilization.getCivilizationBuildings((BuildingClassTypes)iI);

		if (eBuilding != NO_BUILDING && city.canConstruct(eBuilding, false, true, false)) {
			int iChange = city.getAdditionalGreatPeopleRateByBuilding(eBuilding);

			if (iChange != 0) {
				if (!bStarted) {
					szBuffer.append(szStart);
					bStarted = true;
				}

				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				setResumableValueChangeHelp(szBuffer, szLabel, L": ", L"", iChange, gDLL->getSymbolID(GREAT_PEOPLE_CHAR), false, true);
			}
		}
	}

	return bStarted;
}

void CvGameTextMgr::parseGreatGeneralHelp(CvWStringBuffer& szBuffer, CvPlayer& kPlayer) {
	szBuffer.assign(gDLL->getText("TXT_KEY_MISC_GREAT_GENERAL", kPlayer.getCombatExperience(), kPlayer.greatPeopleThreshold(true)));
}


//------------------------------------------------------------------------------------------------

void CvGameTextMgr::buildCityBillboardIconString(CvWStringBuffer& szBuffer, CvCity* pCity) {
	szBuffer.clear();

	// government center icon
	if (pCity->isGovernmentCenter() && !(pCity->isCapital())) {
		szBuffer.append(CvWString::format(L"%c", gDLL->getSymbolID(SILVER_STAR_CHAR)));
	}

	// happiness, healthiness, superlative icons
	if (pCity->canBeSelected()) {
		if (pCity->angryPopulation() > 0) {
			szBuffer.append(CvWString::format(L"%c", gDLL->getSymbolID(UNHAPPY_CHAR)));
		}

		if (pCity->healthRate() < 0) {
			szBuffer.append(CvWString::format(L"%c", gDLL->getSymbolID(UNHEALTHY_CHAR)));
		}

		if (gDLL->getGraphicOption(GRAPHICOPTION_CITY_DETAIL)) {
			if (GET_PLAYER(pCity->getOwnerINLINE()).getNumCities() > 2) {
				if (pCity->findYieldRateRank(YIELD_PRODUCTION) == 1) {
					szBuffer.append(CvWString::format(L"%c", GC.getYieldInfo(YIELD_PRODUCTION).getChar()));
				}
				if (pCity->findCommerceRateRank(COMMERCE_GOLD) == 1) {
					szBuffer.append(CvWString::format(L"%c", GC.getCommerceInfo(COMMERCE_GOLD).getChar()));
				}
				if (pCity->findCommerceRateRank(COMMERCE_RESEARCH) == 1) {
					szBuffer.append(CvWString::format(L"%c", GC.getCommerceInfo(COMMERCE_RESEARCH).getChar()));
				}
			}
		}

		if (pCity->isConnectedToCapital()) {
			if (GET_PLAYER(pCity->getOwnerINLINE()).countNumCitiesConnectedToCapital() > 1) {
				szBuffer.append(CvWString::format(L"%c", gDLL->getSymbolID(TRADE_CHAR)));
			}
		}
	}

	// religion icons
	for (ReligionTypes eReligion = (ReligionTypes)0; eReligion < GC.getNumReligionInfos(); eReligion = (ReligionTypes)(eReligion + 1)) {
		if (pCity->isHasReligion(eReligion)) {
			if (pCity->isHolyCity(eReligion)) {
				szBuffer.append(CvWString::format(L"%c", GC.getReligionInfo(eReligion).getHolyCityChar()));
			} else {
				szBuffer.append(CvWString::format(L"%c", GC.getReligionInfo(eReligion).getChar()));
			}
		}
	}

	// corporation icons
	for (CorporationTypes eCorporation = (CorporationTypes)0; eCorporation < GC.getNumCorporationInfos(); eCorporation = (CorporationTypes)(eCorporation + 1)) {
		if (pCity->isHeadquarters(eCorporation)) {
			if (pCity->isHasCorporation(eCorporation)) {
				szBuffer.append(CvWString::format(L"%c", GC.getCorporationInfo(eCorporation).getHeadquarterChar()));
			}
		} else {
			if (pCity->isActiveCorporation(eCorporation)) {
				szBuffer.append(CvWString::format(L"%c", GC.getCorporationInfo(eCorporation).getChar()));
			}
		}
	}

	if (pCity->getTeam() == GC.getGameINLINE().getActiveTeam()) {
		if (pCity->isPower()) {
			szBuffer.append(CvWString::format(L"%c", gDLL->getSymbolID(POWER_CHAR)));
		}
	}

	// XXX out this in bottom bar???
	if (pCity->isOccupation()) {
		szBuffer.append(CvWString::format(L" (%c:%d)", gDLL->getSymbolID(OCCUPATION_CHAR), pCity->getOccupationTimer()));
	}

	// defense icon and text
	if (pCity->isVisible(GC.getGameINLINE().getActiveTeam(), true)) {
		int iDefenseModifier = pCity->getDefenseModifier(GC.getGameINLINE().selectionListIgnoreBuildingDefense());

		if (iDefenseModifier != 0) {
			szBuffer.append(CvWString::format(L" %c:%s%d%%", gDLL->getSymbolID(DEFENSE_CHAR), ((iDefenseModifier > 0) ? "+" : ""), iDefenseModifier));
		}
	}
}

void CvGameTextMgr::buildCityBillboardCityNameString(CvWStringBuffer& szBuffer, CvCity* pCity) {
	szBuffer.assign(pCity->getName());

	if (pCity->canBeSelected()) {
		if (gDLL->getGraphicOption(GRAPHICOPTION_CITY_DETAIL)) {
			if (pCity->foodDifference() > 0) {
				int iTurns = pCity->getFoodTurnsLeft();

				if ((iTurns > 1) || !(pCity->AI_isEmphasizeAvoidGrowth())) {
					if (iTurns < MAX_INT) {
						szBuffer.append(CvWString::format(L" (%d)", iTurns));
					}
				}
			} else if (pCity->foodDifference() < 0 && getOptionBOOL("CityBar__StarvationTurns", false)) {
				int iFoodDifference = pCity->foodDifference();
				if (pCity->getFood() + iFoodDifference >= 0) {
					int iTurns = pCity->getFood() / -iFoodDifference + 1;
					szBuffer.append(CvWString::format(L" (%d)", iTurns));
				} else {
					szBuffer.append(L" (!!!)");
				}
			}
		}
	}
}

void CvGameTextMgr::buildCityBillboardProductionString(CvWStringBuffer& szBuffer, CvCity* pCity) {
	if (pCity->getOrderQueueLength() > 0) {
		szBuffer.assign(pCity->getProductionName());

		if (gDLL->getGraphicOption(GRAPHICOPTION_CITY_DETAIL)) {
			int iTurns = pCity->getProductionTurnsLeft();

			if (iTurns < MAX_INT) {
				szBuffer.append(CvWString::format(L" (%d)", iTurns));
			}
		}
	} else {
		szBuffer.clear();
	}
}


void CvGameTextMgr::buildCityBillboardCitySizeString(CvWStringBuffer& szBuffer, CvCity* pCity, const NiColorA& kColor) {
#define CAPARAMS(c) (int)((c).r * 255.0f), (int)((c).g * 255.0f), (int)((c).b * 255.0f), (int)((c).a * 255.0f)
	szBuffer.assign(CvWString::format(SETCOLR L"%d" ENDCOLR, CAPARAMS(kColor), pCity->getPopulation()));
#undef CAPARAMS
}

void CvGameTextMgr::getCityBillboardFoodbarColors(CvCity* pCity, std::vector<NiColorA>& aColors) {
	aColors.resize(NUM_INFOBAR_TYPES);
	aColors[INFOBAR_STORED] = GC.getColorInfo((ColorTypes)(GC.getYieldInfo(YIELD_FOOD).getColorType())).getColor();
	aColors[INFOBAR_RATE] = aColors[INFOBAR_STORED];
	aColors[INFOBAR_RATE].a = 0.5f;
	aColors[INFOBAR_RATE_EXTRA] = GC.getColorInfo((ColorTypes)GC.getInfoTypeForString("COLOR_NEGATIVE_RATE")).getColor();
	aColors[INFOBAR_EMPTY] = GC.getColorInfo((ColorTypes)GC.getInfoTypeForString("COLOR_EMPTY")).getColor();
}

void CvGameTextMgr::getCityBillboardProductionbarColors(CvCity* pCity, std::vector<NiColorA>& aColors) {
	aColors.resize(NUM_INFOBAR_TYPES);
	aColors[INFOBAR_STORED] = GC.getColorInfo((ColorTypes)(GC.getYieldInfo(YIELD_PRODUCTION).getColorType())).getColor();
	aColors[INFOBAR_RATE] = aColors[INFOBAR_STORED];
	aColors[INFOBAR_RATE].a = 0.5f;
	aColors[INFOBAR_RATE_EXTRA] = GC.getColorInfo((ColorTypes)(GC.getYieldInfo(YIELD_FOOD).getColorType())).getColor();
	aColors[INFOBAR_RATE_EXTRA].a = 0.5f;
	aColors[INFOBAR_EMPTY] = GC.getColorInfo((ColorTypes)GC.getInfoTypeForString("COLOR_EMPTY")).getColor();
}


void CvGameTextMgr::setScoreHelp(CvWStringBuffer& szString, PlayerTypes ePlayer) {
	if (NO_PLAYER != ePlayer) {
		CvPlayer& player = GET_PLAYER(ePlayer);

		int iPop = player.getPopScore();
		int iMaxPop = GC.getGameINLINE().getMaxPopulation();
		int iPopScore = 0;
		if (iMaxPop > 0) {
			iPopScore = (GC.getDefineINT("SCORE_POPULATION_FACTOR") * iPop) / iMaxPop;
		}
		int iLand = player.getLandScore();
		int iMaxLand = GC.getGameINLINE().getMaxLand();
		int iLandScore = 0;
		if (iMaxLand > 0) {
			iLandScore = (GC.getDefineINT("SCORE_LAND_FACTOR") * iLand) / iMaxLand;
		}
		int iTech = player.getTechScore();
		int iMaxTech = GC.getGameINLINE().getMaxTech();
		int iTechScore = 0;
		if (iMaxTech > 0) {
			iTechScore = (GC.getDefineINT("SCORE_TECH_FACTOR") * iTech) / iMaxTech;
		}
		int iWonders = player.getWondersScore();
		int iMaxWonders = GC.getGameINLINE().getMaxWonders();
		int iWondersScore = 0;
		if (iMaxWonders > 0) {
			iWondersScore = (GC.getDefineINT("SCORE_WONDER_FACTOR") * iWonders) / iMaxWonders;
		}
		int iTotalScore = iPopScore + iLandScore + iTechScore + iWondersScore;
		int iVictoryScore = player.calculateScore(true, true);
		if (iTotalScore == player.calculateScore()) {
			szString.append(gDLL->getText("TXT_KEY_SCORE_BREAKDOWN", iPopScore, iPop, iMaxPop, iLandScore, iLand, iMaxLand, iTechScore, iTech, iMaxTech, iWondersScore, iWonders, iMaxWonders, iTotalScore, iVictoryScore));
		}
	}
}

void CvGameTextMgr::setEventHelp(CvWStringBuffer& szBuffer, EventTypes eEvent, int iEventTriggeredId, PlayerTypes ePlayer) {
	if (NO_EVENT == eEvent || NO_PLAYER == ePlayer) {
		return;
	}

	const CvEventInfo& kEvent = GC.getEventInfo(eEvent);
	const CvPlayer& kActivePlayer = GET_PLAYER(ePlayer);
	EventTriggeredData* pTriggeredData = kActivePlayer.getEventTriggered(iEventTriggeredId);

	if (NULL == pTriggeredData) {
		return;
	}

	CvCity* pCity = kActivePlayer.getCity(pTriggeredData->m_iCityId);
	CvCity* pOtherPlayerCity = NULL;
	CvPlot* pPlot = GC.getMapINLINE().plot(pTriggeredData->m_iPlotX, pTriggeredData->m_iPlotY);
	CvUnit* pUnit = kActivePlayer.getUnit(pTriggeredData->m_iUnitId);

	if (NO_PLAYER != pTriggeredData->m_eOtherPlayer) {
		pOtherPlayerCity = GET_PLAYER(pTriggeredData->m_eOtherPlayer).getCity(pTriggeredData->m_iOtherPlayerCityId);
	}

	CvWString szCity = gDLL->getText("TXT_KEY_EVENT_THE_CITY");
	if (NULL != pCity && kEvent.isCityEffect()) {
		szCity = pCity->getNameKey();
	} else if (NULL != pOtherPlayerCity && kEvent.isOtherPlayerCityEffect()) {
		szCity = pOtherPlayerCity->getNameKey();
	}

	CvWString szUnit = gDLL->getText("TXT_KEY_EVENT_THE_UNIT");
	if (NULL != pUnit) {
		szUnit = pUnit->getNameKey();
	}

	CvWString szReligion = gDLL->getText("TXT_KEY_EVENT_THE_RELIGION");
	if (NO_RELIGION != pTriggeredData->m_eReligion) {
		szReligion = GC.getReligionInfo(pTriggeredData->m_eReligion).getTextKeyWide();
	}

	eventGoldHelp(szBuffer, eEvent, ePlayer, pTriggeredData->m_eOtherPlayer);

	eventTechHelp(szBuffer, eEvent, kActivePlayer.getBestEventTech(eEvent, pTriggeredData->m_eOtherPlayer), ePlayer, pTriggeredData->m_eOtherPlayer);

	if (NO_PLAYER != pTriggeredData->m_eOtherPlayer && NO_BONUS != kEvent.getBonusGift()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GIFT_BONUS_TO_PLAYER", GC.getBonusInfo((BonusTypes)kEvent.getBonusGift()).getTextKeyWide(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
	}

	if (kEvent.getHappy() != 0) {
		if (NO_PLAYER != pTriggeredData->m_eOtherPlayer) {
			if (kEvent.getHappy() > 0) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HAPPY_FROM_PLAYER", kEvent.getHappy(), kEvent.getHappy(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
			} else {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HAPPY_TO_PLAYER", -kEvent.getHappy(), -kEvent.getHappy(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
			}
		} else {
			if (kEvent.getHappy() > 0) {
				if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect()) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HAPPY_CITY", kEvent.getHappy(), szCity.GetCString()));
				} else {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HAPPY", kEvent.getHappy()));
				}
			} else {
				if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect()) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNHAPPY_CITY", -kEvent.getHappy(), szCity.GetCString()));
				} else {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNHAPPY", -kEvent.getHappy()));
				}
			}
		}
	}

	if (kEvent.getHealth() != 0) {
		if (NO_PLAYER != pTriggeredData->m_eOtherPlayer) {
			if (kEvent.getHealth() > 0) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HEALTH_FROM_PLAYER", kEvent.getHealth(), kEvent.getHealth(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
			} else {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HEALTH_TO_PLAYER", -kEvent.getHealth(), -kEvent.getHealth(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
			}
		} else {
			if (kEvent.getHealth() > 0) {
				if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect()) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HEALTH_CITY", kEvent.getHealth(), szCity.GetCString()));
				} else {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HEALTH", kEvent.getHealth()));
				}
			} else {
				if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect()) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNHEALTH", -kEvent.getHealth(), szCity.GetCString()));
				} else {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNHEALTH_CITY", -kEvent.getHealth()));
				}
			}
		}
	}

	if (kEvent.getHurryAnger() != 0) {
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HURRY_ANGER_CITY", kEvent.getHurryAnger(), szCity.GetCString()));
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HURRY_ANGER", kEvent.getHurryAnger()));
		}
	}

	if (kEvent.getHappyTurns() > 0) {
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_TEMP_HAPPY_CITY", GC.getDefineINT("TEMP_HAPPY"), kEvent.getHappyTurns(), szCity.GetCString()));
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_TEMP_HAPPY", GC.getDefineINT("TEMP_HAPPY"), kEvent.getHappyTurns()));
		}
	}

	if (kEvent.getFood() != 0) {
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FOOD_CITY", kEvent.getFood(), szCity.GetCString()));
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FOOD", kEvent.getFood()));
		}
	}

	if (kEvent.getFoodPercent() != 0) {
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FOOD_PERCENT_CITY", kEvent.getFoodPercent(), szCity.GetCString()));
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FOOD_PERCENT", kEvent.getFoodPercent()));
		}
	}

	if (kEvent.getRevoltTurns() > 0) {
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_REVOLT_TURNS", kEvent.getRevoltTurns(), szCity.GetCString()));
		}
	}

	if (0 != kEvent.getSpaceProductionModifier()) {
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_SPACE_PRODUCTION_CITY", kEvent.getSpaceProductionModifier(), szCity.GetCString()));
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_SPACESHIP_MOD_ALL_CITIES", kEvent.getSpaceProductionModifier()));
		}
	}

	if (kEvent.getMaxPillage() > 0) {
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect()) {
			if (kEvent.getMaxPillage() == kEvent.getMinPillage()) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_PILLAGE_CITY", kEvent.getMinPillage(), szCity.GetCString()));
			} else {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_PILLAGE_RANGE_CITY", kEvent.getMinPillage(), kEvent.getMaxPillage(), szCity.GetCString()));
			}
		} else {
			if (kEvent.getMaxPillage() == kEvent.getMinPillage()) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_PILLAGE", kEvent.getMinPillage()));
			} else {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_PILLAGE_RANGE", kEvent.getMinPillage(), kEvent.getMaxPillage()));
			}
		}
	}

	for (int i = 0; i < GC.getNumSpecialistInfos(); ++i) {
		if (kEvent.getFreeSpecialistCount(i) > 0) {
			if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect()) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FREE_SPECIALIST", kEvent.getFreeSpecialistCount(i), GC.getSpecialistInfo((SpecialistTypes)i).getTextKeyWide(), szCity.GetCString()));
			}
		}
	}

	if (kEvent.getPopulationChange() != 0) {
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_POPULATION_CHANGE_CITY", kEvent.getPopulationChange(), szCity.GetCString()));
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_POPULATION_CHANGE", kEvent.getPopulationChange()));
		}
	}

	if (kEvent.getCulture() != 0) {
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_CULTURE_CITY", kEvent.getCulture(), szCity.GetCString()));
		} else {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_CULTURE", kEvent.getCulture()));
		}
	}

	if (kEvent.getUnitClass() != NO_UNITCLASS) {
		CivilizationTypes eCiv = kActivePlayer.getCivilizationType();
		if (NO_CIVILIZATION != eCiv) {
			UnitTypes eUnit = (UnitTypes)GC.getCivilizationInfo(eCiv).getCivilizationUnits(kEvent.getUnitClass());
			if (eUnit != NO_UNIT) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_BONUS_UNIT", kEvent.getNumUnits(), GC.getUnitInfo(eUnit).getTextKeyWide()));
			}
		}
	}

	if (kEvent.getBuildingClass() != NO_BUILDINGCLASS) {
		CivilizationTypes eCiv = kActivePlayer.getCivilizationType();
		if (NO_CIVILIZATION != eCiv) {
			BuildingTypes eBuilding = (BuildingTypes)GC.getCivilizationInfo(eCiv).getCivilizationBuildings(kEvent.getBuildingClass());
			if (eBuilding != NO_BUILDING) {
				if (kEvent.getBuildingChange() > 0) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_BONUS_BUILDING", GC.getBuildingInfo(eBuilding).getTextKeyWide()));
				} else if (kEvent.getBuildingChange() < 0) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_REMOVE_BUILDING", GC.getBuildingInfo(eBuilding).getTextKeyWide()));
				}
			}
		}
	}

	if (kEvent.getNumBuildingYieldChanges() > 0) {
		CvWStringBuffer szYield;
		for (int iBuildingClass = 0; iBuildingClass < GC.getNumBuildingClassInfos(); ++iBuildingClass) {
			CivilizationTypes eCiv = kActivePlayer.getCivilizationType();
			if (NO_CIVILIZATION != eCiv) {
				BuildingTypes eBuilding = (BuildingTypes)GC.getCivilizationInfo(eCiv).getCivilizationBuildings(iBuildingClass);
				if (eBuilding != NO_BUILDING) {
					int aiYields[NUM_YIELD_TYPES];
					for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield) {
						aiYields[iYield] = kEvent.getBuildingYieldChange(iBuildingClass, iYield);
					}

					szYield.clear();
					setYieldChangeHelp(szYield, L"", L"", L"", aiYields, false, false);
					if (!szYield.isEmpty()) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_EVENT_YIELD_CHANGE_BUILDING", GC.getBuildingInfo(eBuilding).getTextKeyWide(), szYield.getCString()));
					}
				}
			}
		}
	}

	if (kEvent.getNumBuildingCommerceChanges() > 0) {
		CvWStringBuffer szCommerce;
		for (int iBuildingClass = 0; iBuildingClass < GC.getNumBuildingClassInfos(); ++iBuildingClass) {
			CivilizationTypes eCiv = kActivePlayer.getCivilizationType();
			if (NO_CIVILIZATION != eCiv) {
				BuildingTypes eBuilding = (BuildingTypes)GC.getCivilizationInfo(eCiv).getCivilizationBuildings(iBuildingClass);
				if (eBuilding != NO_BUILDING) {
					int aiCommerces[NUM_COMMERCE_TYPES];
					for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce) {
						aiCommerces[iCommerce] = kEvent.getBuildingCommerceChange(iBuildingClass, iCommerce);
					}

					szCommerce.clear();
					setCommerceChangeHelp(szCommerce, L"", L"", L"", aiCommerces, false, false);
					if (!szCommerce.isEmpty()) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_EVENT_YIELD_CHANGE_BUILDING", GC.getBuildingInfo(eBuilding).getTextKeyWide(), szCommerce.getCString()));
					}
				}
			}
		}
	}

	if (kEvent.getNumBuildingHappyChanges() > 0) {
		for (int iBuildingClass = 0; iBuildingClass < GC.getNumBuildingClassInfos(); ++iBuildingClass) {
			CivilizationTypes eCiv = kActivePlayer.getCivilizationType();
			if (NO_CIVILIZATION != eCiv) {
				BuildingTypes eBuilding = (BuildingTypes)GC.getCivilizationInfo(eCiv).getCivilizationBuildings(iBuildingClass);
				if (eBuilding != NO_BUILDING) {
					int iHappy = kEvent.getBuildingHappyChange(iBuildingClass);
					if (iHappy > 0) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HAPPY_BUILDING", GC.getBuildingInfo(eBuilding).getTextKeyWide(), iHappy, gDLL->getSymbolID(HAPPY_CHAR)));
					} else if (iHappy < 0) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HAPPY_BUILDING", GC.getBuildingInfo(eBuilding).getTextKeyWide(), -iHappy, gDLL->getSymbolID(UNHAPPY_CHAR)));
					}
				}
			}
		}
	}

	if (kEvent.getNumBuildingHealthChanges() > 0) {
		for (int iBuildingClass = 0; iBuildingClass < GC.getNumBuildingClassInfos(); ++iBuildingClass) {
			CivilizationTypes eCiv = kActivePlayer.getCivilizationType();
			if (NO_CIVILIZATION != eCiv) {
				BuildingTypes eBuilding = (BuildingTypes)GC.getCivilizationInfo(eCiv).getCivilizationBuildings(iBuildingClass);
				if (eBuilding != NO_BUILDING) {
					int iHealth = kEvent.getBuildingHealthChange(iBuildingClass);
					if (iHealth > 0) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HAPPY_BUILDING", GC.getBuildingInfo(eBuilding).getTextKeyWide(), iHealth, gDLL->getSymbolID(HEALTHY_CHAR)));
					} else if (iHealth < 0) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HAPPY_BUILDING", GC.getBuildingInfo(eBuilding).getTextKeyWide(), -iHealth, gDLL->getSymbolID(UNHEALTHY_CHAR)));
					}
				}
			}
		}
	}

	if (kEvent.getFeatureChange() > 0) {
		if (kEvent.getFeature() != NO_FEATURE) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FEATURE_GROWTH", GC.getFeatureInfo((FeatureTypes)kEvent.getFeature()).getTextKeyWide()));
		}
	} else if (kEvent.getFeatureChange() < 0) {
		if (NULL != pPlot && NO_FEATURE != pPlot->getFeatureType()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FEATURE_REMOVE", GC.getFeatureInfo(pPlot->getFeatureType()).getTextKeyWide()));
		}
	}

	if (kEvent.getImprovementChange() > 0) {
		if (kEvent.getImprovement() != NO_IMPROVEMENT) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_IMPROVEMENT_GROWTH", GC.getImprovementInfo((ImprovementTypes)kEvent.getImprovement()).getTextKeyWide()));
		}
	} else if (kEvent.getImprovementChange() < 0) {
		if (NULL != pPlot && NO_IMPROVEMENT != pPlot->getImprovementType()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_IMPROVEMENT_REMOVE", GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide()));
		}
	}

	if (kEvent.getBonusChange() > 0) {
		if (kEvent.getBonus() != NO_BONUS) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_BONUS_GROWTH", GC.getBonusInfo((BonusTypes)kEvent.getBonus()).getTextKeyWide()));
		}
	} else if (kEvent.getBonusChange() < 0) {
		if (NULL != pPlot && NO_BONUS != pPlot->getBonusType()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_BONUS_REMOVE", GC.getBonusInfo(pPlot->getBonusType()).getTextKeyWide()));
		}
	}

	if (kEvent.getRouteChange() > 0) {
		if (kEvent.getRoute() != NO_ROUTE) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ROUTE_GROWTH", GC.getRouteInfo((RouteTypes)kEvent.getRoute()).getTextKeyWide()));
		}
	} else if (kEvent.getRouteChange() < 0) {
		if (NULL != pPlot && NO_ROUTE != pPlot->getRouteType()) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ROUTE_REMOVE", GC.getRouteInfo(pPlot->getRouteType()).getTextKeyWide()));
		}
	}

	int aiYields[NUM_YIELD_TYPES];
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) {
		aiYields[i] = kEvent.getPlotExtraYield(i);
	}

	CvWStringBuffer szYield;
	setYieldChangeHelp(szYield, L"", L"", L"", aiYields, false, false);
	if (!szYield.isEmpty()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_YIELD_CHANGE_PLOT", szYield.getCString()));
	}

	if (NO_BONUS != kEvent.getBonusRevealed()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_BONUS_REVEALED", GC.getBonusInfo((BonusTypes)kEvent.getBonusRevealed()).getTextKeyWide()));
	}

	if (0 != kEvent.getUnitExperience()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNIT_EXPERIENCE", kEvent.getUnitExperience(), szUnit.GetCString()));
	}

	if (0 != kEvent.isDisbandUnit()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNIT_DISBAND", szUnit.GetCString()));
	}

	if (NO_PROMOTION != kEvent.getUnitPromotion()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNIT_PROMOTION", szUnit.GetCString(), GC.getPromotionInfo((PromotionTypes)kEvent.getUnitPromotion()).getTextKeyWide()));
	}

	for (int i = 0; i < GC.getNumUnitCombatInfos(); ++i) {
		if (NO_PROMOTION != kEvent.getUnitCombatPromotion(i)) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNIT_COMBAT_PROMOTION", GC.getUnitCombatInfo((UnitCombatTypes)i).getTextKeyWide(), GC.getPromotionInfo((PromotionTypes)kEvent.getUnitCombatPromotion(i)).getTextKeyWide()));
		}
	}

	for (int i = 0; i < GC.getNumUnitClassInfos(); ++i) {
		if (NO_PROMOTION != kEvent.getUnitClassPromotion(i)) {
			UnitTypes ePromotedUnit = ((UnitTypes)(GC.getCivilizationInfo(kActivePlayer.getCivilizationType()).getCivilizationUnits(i)));
			if (NO_UNIT != ePromotedUnit) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNIT_CLASS_PROMOTION", GC.getUnitInfo(ePromotedUnit).getTextKeyWide(), GC.getPromotionInfo((PromotionTypes)kEvent.getUnitClassPromotion(i)).getTextKeyWide()));
			}
		}
	}

	if (kEvent.getConvertOwnCities() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_CONVERT_OWN_CITIES", kEvent.getConvertOwnCities(), szReligion.GetCString()));
	}

	if (kEvent.getConvertOtherCities() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_CONVERT_OTHER_CITIES", kEvent.getConvertOtherCities(), szReligion.GetCString()));
	}

	if (NO_PLAYER != pTriggeredData->m_eOtherPlayer) {
		if (kEvent.getAttitudeModifier() > 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ATTITUDE_GOOD", kEvent.getAttitudeModifier(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
		} else if (kEvent.getAttitudeModifier() < 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ATTITUDE_BAD", kEvent.getAttitudeModifier(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
		}
	}

	if (NO_PLAYER != pTriggeredData->m_eOtherPlayer) {
		TeamTypes eWorstEnemy = GET_TEAM(GET_PLAYER(pTriggeredData->m_eOtherPlayer).getTeam()).AI_getWorstEnemy();
		if (NO_TEAM != eWorstEnemy) {
			if (kEvent.getTheirEnemyAttitudeModifier() > 0) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ATTITUDE_GOOD", kEvent.getTheirEnemyAttitudeModifier(), GET_TEAM(eWorstEnemy).getName().GetCString()));
			} else if (kEvent.getTheirEnemyAttitudeModifier() < 0) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ATTITUDE_BAD", kEvent.getTheirEnemyAttitudeModifier(), GET_TEAM(eWorstEnemy).getName().GetCString()));
			}
		}
	}

	if (NO_PLAYER != pTriggeredData->m_eOtherPlayer) {
		if (kEvent.getEspionagePoints() > 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ESPIONAGE_POINTS", kEvent.getEspionagePoints(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
		} else if (kEvent.getEspionagePoints() < 0) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ESPIONAGE_COST", -kEvent.getEspionagePoints()));
		}
	}

	if (kEvent.isGoldenAge()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLDEN_AGE"));
	}

	if (0 != kEvent.getFreeUnitSupport()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FREE_UNIT_SUPPORT", kEvent.getFreeUnitSupport()));
	}

	if (0 != kEvent.getInflationModifier()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_INFLATION_MODIFIER", kEvent.getInflationModifier()));
	}

	if (kEvent.isDeclareWar()) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_DECLARE_WAR", GET_PLAYER(pTriggeredData->m_eOtherPlayer).getCivilizationAdjectiveKey()));
	}

	if (kEvent.getUnitImmobileTurns() > 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_IMMOBILE_UNIT", kEvent.getUnitImmobileTurns(), szUnit.GetCString()));
	}

	if (!CvWString(kEvent.getPythonHelp()).empty()) {
		CvWString szHelp;
		CyArgsList argsList;
		argsList.add(eEvent);
		argsList.add(gDLL->getPythonIFace()->makePythonObject(pTriggeredData));

		gDLL->getPythonIFace()->callFunction(PYRandomEventModule, kEvent.getPythonHelp(), argsList.makeFunctionArgs(), &szHelp);

		szBuffer.append(NEWLINE);
		szBuffer.append(szHelp);
	}

	CvWStringBuffer szTemp;
	for (int i = 0; i < GC.getNumEventInfos(); ++i) {
		if (0 == kEvent.getAdditionalEventTime(i)) {
			if (kEvent.getAdditionalEventChance(i) > 0) {
				if (GET_PLAYER(GC.getGameINLINE().getActivePlayer()).canDoEvent((EventTypes)i, *pTriggeredData)) {
					szTemp.clear();
					setEventHelp(szTemp, (EventTypes)i, iEventTriggeredId, ePlayer);

					if (!szTemp.isEmpty()) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ADDITIONAL_CHANCE", kEvent.getAdditionalEventChance(i), L""));
						szBuffer.append(NEWLINE);
						szBuffer.append(szTemp);
					}
				}
			}
		} else {
			szTemp.clear();
			setEventHelp(szTemp, (EventTypes)i, iEventTriggeredId, ePlayer);

			if (!szTemp.isEmpty()) {
				CvWString szDelay = gDLL->getText("TXT_KEY_EVENT_DELAY_TURNS", (GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getGrowthPercent() * kEvent.getAdditionalEventTime((EventTypes)i)) / 100);

				if (kEvent.getAdditionalEventChance(i) > 0) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ADDITIONAL_CHANCE", kEvent.getAdditionalEventChance(i), szDelay.GetCString()));
				} else {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_DELAY", szDelay.GetCString()));
				}

				szBuffer.append(NEWLINE);
				szBuffer.append(szTemp);
			}
		}
	}

	if (NO_TECH != kEvent.getPrereqTech()) {
		if (!GET_TEAM(kActivePlayer.getTeam()).isHasTech((TechTypes)kEvent.getPrereqTech())) {
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDING_REQUIRES_STRING", GC.getTechInfo((TechTypes)(kEvent.getPrereqTech())).getTextKeyWide()));
		}
	}

	bool done = false;
	while (!done) {
		done = true;
		if (!szBuffer.isEmpty()) {
			const wchar* wideChar = szBuffer.getCString();
			if (wideChar[0] == L'\n') {
				CvWString tempString(&wideChar[1]);
				szBuffer.clear();
				szBuffer.append(tempString);
				done = false;
			}
		}
	}
}

void CvGameTextMgr::eventTechHelp(CvWStringBuffer& szBuffer, EventTypes eEvent, TechTypes eTech, PlayerTypes eActivePlayer, PlayerTypes eOtherPlayer) {
	const CvEventInfo& kEvent = GC.getEventInfo(eEvent);

	if (eTech != NO_TECH) {
		if (100 == kEvent.getTechPercent()) {
			if (NO_PLAYER != eOtherPlayer) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_TECH_GAINED_FROM_PLAYER", GC.getTechInfo(eTech).getTextKeyWide(), GET_PLAYER(eOtherPlayer).getNameKey()));
			} else {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_TECH_GAINED", GC.getTechInfo(eTech).getTextKeyWide()));
			}
		} else if (0 != kEvent.getTechPercent()) {
			CvTeam& kTeam = GET_TEAM(GET_PLAYER(eActivePlayer).getTeam());
			int iBeakers = (kTeam.getResearchCost(eTech) * kEvent.getTechPercent()) / 100;
			if (kEvent.getTechPercent() > 0) {
				iBeakers = std::min(kTeam.getResearchLeft(eTech), iBeakers);
			} else if (kEvent.getTechPercent() < 0) {
				iBeakers = std::max(kTeam.getResearchLeft(eTech) - kTeam.getResearchCost(eTech), iBeakers);
			}

			if (NO_PLAYER != eOtherPlayer) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_TECH_GAINED_FROM_PLAYER_PERCENT", iBeakers, GC.getTechInfo(eTech).getTextKeyWide(), GET_PLAYER(eOtherPlayer).getNameKey()));
			} else {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_TECH_GAINED_PERCENT", iBeakers, GC.getTechInfo(eTech).getTextKeyWide()));
			}
		}
	}
}

void CvGameTextMgr::eventGoldHelp(CvWStringBuffer& szBuffer, EventTypes eEvent, PlayerTypes ePlayer, PlayerTypes eOtherPlayer) {
	const CvEventInfo& kEvent = GC.getEventInfo(eEvent);
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);

	int iGold1 = kPlayer.getEventCost(eEvent, eOtherPlayer, false);
	int iGold2 = kPlayer.getEventCost(eEvent, eOtherPlayer, true);

	if (0 != iGold1 || 0 != iGold2) {
		if (iGold1 == iGold2) {
			if (NO_PLAYER != eOtherPlayer && kEvent.isGoldToPlayer()) {
				if (iGold1 > 0) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_FROM_PLAYER", iGold1, GET_PLAYER(eOtherPlayer).getNameKey()));
				} else {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_TO_PLAYER", -iGold1, GET_PLAYER(eOtherPlayer).getNameKey()));
				}
			} else {
				if (iGold1 > 0) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_GAINED", iGold1));
				} else {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_LOST", -iGold1));
				}
			}
		} else {
			if (NO_PLAYER != eOtherPlayer && kEvent.isGoldToPlayer()) {
				if (iGold1 > 0) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_RANGE_FROM_PLAYER", iGold1, iGold2, GET_PLAYER(eOtherPlayer).getNameKey()));
				} else {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_RANGE_TO_PLAYER", -iGold1, -iGold2, GET_PLAYER(eOtherPlayer).getNameKey()));
				}
			} else {
				if (iGold1 > 0) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_RANGE_GAINED", iGold1, iGold2));
				} else {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_RANGE_LOST", -iGold1, iGold2));
				}
			}
		}
	}
}

void CvGameTextMgr::setTradeRouteHelp(CvWStringBuffer& szBuffer, int iRoute, CvCity* pCity) {
	if (NULL != pCity) {
		CvCity* pOtherCity = pCity->getTradeCity(iRoute);

		if (NULL != pOtherCity) {
			szBuffer.append(pOtherCity->getName());

			int iProfit = pCity->getBaseTradeProfit(pOtherCity);

			szBuffer.append(NEWLINE);
			CvWString szBaseProfit;
			szBaseProfit.Format(L"%d.%02d", iProfit / 100, iProfit % 100);
			szBuffer.append(gDLL->getText("TXT_KEY_TRADE_ROUTE_HELP_BASE", szBaseProfit.GetCString()));

			int iModifier = 100;
			int iNewMod;

			for (int iBuilding = 0; iBuilding < GC.getNumBuildingInfos(); ++iBuilding) {
				if (pCity->getNumActiveBuilding((BuildingTypes)iBuilding) > 0) {
					iNewMod = pCity->getNumActiveBuilding((BuildingTypes)iBuilding) * GC.getBuildingInfo((BuildingTypes)iBuilding).getTradeRouteModifier();
					if (0 != iNewMod) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_TRADE_ROUTE_MOD_BUILDING", GC.getBuildingInfo((BuildingTypes)iBuilding).getTextKeyWide(), iNewMod));
						iModifier += iNewMod;
					}
				}
			}

			iNewMod = pCity->getPopulationTradeModifier();
			if (0 != iNewMod) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_TRADE_ROUTE_MOD_POPULATION", iNewMod));
				iModifier += iNewMod;
			}

			if (pCity->isConnectedToCapital()) {
				iNewMod = GC.getDefineINT("CAPITAL_TRADE_MODIFIER");
				if (0 != iNewMod) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_TRADE_ROUTE_MOD_CAPITAL", iNewMod));
					iModifier += iNewMod;
				}
			}

			if (NULL != pOtherCity) {
				if (pCity->area() != pOtherCity->area()) {
					iNewMod = GC.getDefineINT("OVERSEAS_TRADE_MODIFIER");
					if (0 != iNewMod) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_TRADE_ROUTE_MOD_OVERSEAS", iNewMod));
						iModifier += iNewMod;
					}
				}

				if (pCity->getTeam() != pOtherCity->getTeam()) {
					iNewMod = pCity->getForeignTradeRouteModifier();
					if (0 != iNewMod) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_TRADE_ROUTE_MOD_FOREIGN", iNewMod));
						iModifier += iNewMod;
					}

					iNewMod = pCity->getPeaceTradeModifier(pOtherCity->getTeam());
					if (0 != iNewMod) {
						szBuffer.append(NEWLINE);
						szBuffer.append(gDLL->getText("TXT_KEY_TRADE_ROUTE_MOD_PEACE", iNewMod));
						iModifier += iNewMod;
					}
				}
			}

			FAssert(pCity->totalTradeModifier(pOtherCity) == iModifier);

			iProfit *= iModifier;
			iProfit /= 10000;

			FAssert(iProfit == pCity->calculateTradeProfit(pOtherCity));

			szBuffer.append(SEPARATOR);

			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_TRADE_ROUTE_TOTAL", iProfit));
		}
	}
}

void CvGameTextMgr::setEspionageCostHelp(CvWStringBuffer& szBuffer, EspionageMissionTypes eMission, PlayerTypes eTargetPlayer, const CvPlot* pPlot, int iExtraData, const CvUnit* pSpyUnit) {
	const CvPlayer& kPlayer = GET_PLAYER(GC.getGameINLINE().getActivePlayer());
	const CvEspionageMissionInfo& kMission = GC.getEspionageMissionInfo(eMission);

	int iMissionCost = kPlayer.getEspionageMissionBaseCost(eMission, eTargetPlayer, pPlot, iExtraData, pSpyUnit);

	if (kMission.isDestroyImprovement()) {
		if (NULL != pPlot && NO_IMPROVEMENT != pPlot->getImprovementType()) {
			szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_DESTROY_IMPROVEMENT", GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide()));
			szBuffer.append(NEWLINE);
		}
	}

	if (kMission.getDestroyBuildingCostFactor() > 0) {
		BuildingTypes eTargetBuilding = (BuildingTypes)iExtraData;

		if (NULL != pPlot) {
			CvCity* pCity = pPlot->getPlotCity();

			if (NULL != pCity) {
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_DESTROY_IMPROVEMENT", GC.getBuildingInfo(eTargetBuilding).getTextKeyWide()));
				szBuffer.append(NEWLINE);
			}
		}
	}

	if (kMission.getDestroyProjectCostFactor() > 0) {
		ProjectTypes eTargetProject = (ProjectTypes)iExtraData;

		if (NULL != pPlot) {
			CvCity* pCity = pPlot->getPlotCity();

			if (NULL != pCity) {
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_DESTROY_IMPROVEMENT", GC.getProjectInfo(eTargetProject).getTextKeyWide()));
				szBuffer.append(NEWLINE);
			}
		}
	}

	if (kMission.getDestroyProductionCostFactor() > 0) {
		if (NULL != pPlot) {
			CvCity* pCity = pPlot->getPlotCity();

			if (NULL != pCity) {
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_DESTROY_PRODUCTION", pCity->getProduction()));
				szBuffer.append(NEWLINE);
			}
		}
	}

	if (kMission.getDestroyUnitCostFactor() > 0) {
		if (NO_PLAYER != eTargetPlayer) {
			int iTargetUnitID = iExtraData;

			CvUnit* pUnit = GET_PLAYER(eTargetPlayer).getUnit(iTargetUnitID);

			if (NULL != pUnit) {
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_DESTROY_UNIT", pUnit->getNameKey()));
				szBuffer.append(NEWLINE);
			}
		}
	}

	if (kMission.getBuyUnitCostFactor() > 0) {
		if (NO_PLAYER != eTargetPlayer) {
			int iTargetUnitID = iExtraData;

			CvUnit* pUnit = GET_PLAYER(eTargetPlayer).getUnit(iTargetUnitID);

			if (NULL != pUnit) {
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_BRIBE", pUnit->getNameKey()));
				szBuffer.append(NEWLINE);
			}
		}
	}

	if (kMission.getBuyCityCostFactor() > 0) {
		if (NULL != pPlot) {
			CvCity* pCity = pPlot->getPlotCity();

			if (NULL != pCity) {
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_BRIBE", pCity->getNameKey()));
				szBuffer.append(NEWLINE);
			}
		}
	}

	if (kMission.getCityInsertCultureCostFactor() > 0) {
		if (NULL != pPlot) {
			CvCity* pCity = pPlot->getPlotCity();

			if (NULL != pCity && pPlot->getCulture(GC.getGameINLINE().getActivePlayer()) > 0) {
				int iCultureAmount = kMission.getCityInsertCultureAmountFactor() * pCity->countTotalCultureTimes100();
				iCultureAmount /= 10000;
				iCultureAmount = std::max(1, iCultureAmount);

				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_INSERT_CULTURE", pCity->getNameKey(), iCultureAmount, kMission.getCityInsertCultureAmountFactor()));
				szBuffer.append(NEWLINE);
			}
		}
	}

	if (kMission.getCityPoisonWaterCounter() > 0) {
		if (NULL != pPlot) {
			CvCity* pCity = pPlot->getPlotCity();

			if (NULL != pCity) {
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_POISON", kMission.getCityPoisonWaterCounter(), gDLL->getSymbolID(UNHEALTHY_CHAR), pCity->getNameKey(), kMission.getCityPoisonWaterCounter()));
				szBuffer.append(NEWLINE);
			}
		}
	}

	if (kMission.getCityUnhappinessCounter() > 0) {
		if (NULL != pPlot) {
			CvCity* pCity = pPlot->getPlotCity();

			if (NULL != pCity) {
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_POISON", kMission.getCityUnhappinessCounter(), gDLL->getSymbolID(UNHAPPY_CHAR), pCity->getNameKey(), kMission.getCityUnhappinessCounter()));
				szBuffer.append(NEWLINE);
			}
		}
	}

	if (kMission.getCityRevoltCounter() > 0) {
		if (NULL != pPlot) {
			CvCity* pCity = pPlot->getPlotCity();

			if (NULL != pCity) {
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_REVOLT", pCity->getNameKey(), kMission.getCityRevoltCounter()));
				szBuffer.append(NEWLINE);
			}
		}
	}

	if (kMission.getStealTreasuryTypes() > 0) {
		if (NO_PLAYER != eTargetPlayer) {
			int iNumTotalGold;

			if (NULL != pPlot) {
				CvCity* pCity = pPlot->getPlotCity();

				if (NULL != pCity) {
					iNumTotalGold = kPlayer.getEspionageGoldQuantity(eMission, eTargetPlayer, pCity);
				}
			}

			szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_STEAL_TREASURY", iNumTotalGold, GET_PLAYER(eTargetPlayer).getCivilizationAdjectiveKey()));
			szBuffer.append(NEWLINE);
		}
	}

	if (kMission.getBuyTechCostFactor() > 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_STEAL_TECH", GC.getTechInfo((TechTypes)iExtraData).getTextKeyWide()));
		szBuffer.append(NEWLINE);
	}

	if (kMission.getSwitchCivicCostFactor() > 0) {
		if (NO_PLAYER != eTargetPlayer) {
			szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_SWITCH_CIVIC", GET_PLAYER(eTargetPlayer).getNameKey(), GC.getCivicInfo((CivicTypes)iExtraData).getTextKeyWide()));
			szBuffer.append(NEWLINE);
		}
	}

	if (kMission.getSwitchReligionCostFactor() > 0) {
		if (NO_PLAYER != eTargetPlayer) {
			szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_SWITCH_CIVIC", GET_PLAYER(eTargetPlayer).getNameKey(), GC.getReligionInfo((ReligionTypes)iExtraData).getTextKeyWide()));
			szBuffer.append(NEWLINE);
		}
	}

	if (kMission.getPlayerAnarchyCounter() > 0) {
		if (NO_PLAYER != eTargetPlayer) {
			int iTurns = (kMission.getPlayerAnarchyCounter() * GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getAnarchyPercent()) / 100;
			szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_ANARCHY", GET_PLAYER(eTargetPlayer).getNameKey(), iTurns));
			szBuffer.append(NEWLINE);
		}
	}

	if (kMission.getCounterespionageNumTurns() > 0 && kMission.getCounterespionageMod() > 0) {
		if (NO_PLAYER != eTargetPlayer) {
			int iTurns = (kMission.getCounterespionageNumTurns() * GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getResearchPercent()) / 100;

			szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_HELP_COUNTERESPIONAGE", kMission.getCounterespionageMod(), GET_PLAYER(eTargetPlayer).getCivilizationAdjectiveKey(), iTurns));
			szBuffer.append(NEWLINE);
		}
	}

	szBuffer.append(NEWLINE);
	szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_BASE_COST", iMissionCost));

	if (kPlayer.getEspionageMissionCost(eMission, eTargetPlayer, pPlot, iExtraData, pSpyUnit) > 0) {
		int iModifier = 100;
		int iTempModifier = 0;
		CvCity* pCity = NULL;
		if (NULL != pPlot) {
			pCity = pPlot->getPlotCity();
		}

		if (pCity != NULL && GC.getEspionageMissionInfo(eMission).isTargetsCity()) {
			// City Population
			iTempModifier = (GC.getDefineINT("ESPIONAGE_CITY_POP_EACH_MOD") * (pCity->getPopulation() - 1));
			if (0 != iTempModifier) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_POPULATION_MOD", iTempModifier));
				iModifier *= 100 + iTempModifier;
				iModifier /= 100;
			}

			// Trade Route
			if (pCity->isTradeRoute(kPlayer.getID())) {
				iTempModifier = GC.getDefineINT("ESPIONAGE_CITY_TRADE_ROUTE_MOD");
				if (0 != iTempModifier) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_TRADE_ROUTE_MOD", iTempModifier));
					iModifier *= 100 + iTempModifier;
					iModifier /= 100;
				}
			}

			ReligionTypes eReligion = kPlayer.getStateReligion();
			if (NO_RELIGION != eReligion) {
				iTempModifier = 0;

				if (pCity->isHasReligion(eReligion)) {
					if (GET_PLAYER(eTargetPlayer).getStateReligion() != eReligion) {
						iTempModifier += GC.getDefineINT("ESPIONAGE_CITY_RELIGION_STATE_MOD");
					}

					if (kPlayer.hasHolyCity(eReligion)) {
						iTempModifier += GC.getDefineINT("ESPIONAGE_CITY_HOLY_CITY_MOD");
					}
				}

				if (0 != iTempModifier) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_RELIGION_MOD", iTempModifier));
					iModifier *= 100 + iTempModifier;
					iModifier /= 100;
				}
			}

			iTempModifier = pCity->getEspionageDefenseModifier();
			if (0 != iTempModifier) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_DEFENSE_MOD", iTempModifier));
				iModifier *= 100 + iTempModifier;
				iModifier /= 100;
			}
		}

		if (pPlot != NULL) {
			// K-Mod. Culture Mod. (Based on plot culture rather than city culture.)
			if (eMission == NO_ESPIONAGEMISSION || GC.getEspionageMissionInfo(eMission).isSelectPlot() || GC.getEspionageMissionInfo(eMission).isTargetsCity()) {
				iTempModifier = -(pPlot->getCulture(kPlayer.getID()) * GC.getDefineINT("ESPIONAGE_CULTURE_MULTIPLIER_MOD")) / std::max(1, pPlot->getCulture(eTargetPlayer) + pPlot->getCulture(kPlayer.getID()));
				if (0 != iTempModifier) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_CULTURE_MOD", iTempModifier));
					iModifier *= 100 + iTempModifier;
					iModifier /= 100;
				}
			}

			// Distance mod
			int iDistance = GC.getMap().maxPlotDistance();

			CvCity* pOurCapital = kPlayer.getCapitalCity();
			if (NULL != pOurCapital) {
				if (kMission.isSelectPlot() || kMission.isTargetsCity()) {
					iDistance = plotDistance(pOurCapital->getX_INLINE(), pOurCapital->getY_INLINE(), pPlot->getX_INLINE(), pPlot->getY_INLINE());
				} else {
					CvCity* pTheirCapital = GET_PLAYER(eTargetPlayer).getCapitalCity();
					if (NULL != pTheirCapital) {
						iDistance = plotDistance(pOurCapital->getX_INLINE(), pOurCapital->getY_INLINE(), pTheirCapital->getX_INLINE(), pTheirCapital->getY_INLINE());
					}
				}
			}

			iTempModifier = (iDistance + GC.getMapINLINE().maxPlotDistance()) * GC.getDefineINT("ESPIONAGE_DISTANCE_MULTIPLIER_MOD") / GC.getMapINLINE().maxPlotDistance() - 100;
			if (0 != iTempModifier) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_DISTANCE_MOD", iTempModifier));
				iModifier *= 100 + iTempModifier;
				iModifier /= 100;
			}
		}

		// Spy presence mission cost alteration
		if (NULL != pSpyUnit) {
			iTempModifier = -(pSpyUnit->getFortifyTurns() * GC.getDefineINT("ESPIONAGE_EACH_TURN_UNIT_COST_DECREASE"));
			if (0 != iTempModifier) {
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_SPY_STATIONARY_MOD", iTempModifier));
				iModifier *= 100 + iTempModifier;
				iModifier /= 100;
			}
		}

		// My points VS. Your points to mod cost
		iTempModifier = ::getEspionageModifier(kPlayer.getTeam(), GET_PLAYER(eTargetPlayer).getTeam()) - 100;
		if (0 != iTempModifier) {
			szBuffer.append(SEPARATOR);
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_EP_RATIO_MOD", iTempModifier));
			iModifier *= 100 + iTempModifier;
			iModifier /= 100;
		}

		// Counterespionage Mission Mod
		CvTeam& kTargetTeam = GET_TEAM(GET_PLAYER(eTargetPlayer).getTeam());
		if (kTargetTeam.getCounterespionageModAgainstTeam(kPlayer.getTeam()) > 0) {
			iTempModifier = std::max(-100, kTargetTeam.getCounterespionageModAgainstTeam(kPlayer.getTeam()));
			if (0 != iTempModifier) {
				szBuffer.append(SEPARATOR);
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_COUNTERESPIONAGE_MOD", iTempModifier));
				iModifier *= 100 + iTempModifier;
				iModifier /= 100;
			}
		}

		if (pCity != NULL) {
			if (pCity == GET_PLAYER(pCity->getOwnerINLINE()).getCapitalCity() && kTargetTeam.isHasEmbassy(kPlayer.getTeam())) {
				szBuffer.append(SEPARATOR);
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_EMBASSY_MOD", -GC.getEMBASSY_ESPIONAGE_MISSION_COST_MODIFIER()));

				iModifier *= 100 - GC.getEMBASSY_ESPIONAGE_MISSION_COST_MODIFIER();
				iModifier /= 100;
			}
		}

		FAssert(iModifier == kPlayer.getEspionageMissionCostModifier(eMission, eTargetPlayer, pPlot, iExtraData, pSpyUnit));

		iMissionCost *= iModifier;
		iMissionCost /= 100;

		FAssert(iMissionCost == kPlayer.getEspionageMissionCost(eMission, eTargetPlayer, pPlot, iExtraData, pSpyUnit));

		szBuffer.append(SEPARATOR);

		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_COST_TOTAL", iMissionCost));


		if (NULL != pSpyUnit) {
			int iInterceptChance = (pSpyUnit->getSpyInterceptPercent(GET_PLAYER(eTargetPlayer).getTeam()) * (100 + kMission.getDifficultyMod())) / 100;

			szBuffer.append(NEWLINE);
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_CHANCE_OF_SUCCESS", std::min(100, std::max(0, 100 - iInterceptChance))));
		}
	}
}

void CvGameTextMgr::getTradeScreenTitleIcon(CvString& szButton, CvWidgetDataStruct& widgetData, PlayerTypes ePlayer) {
	szButton.clear();

	ReligionTypes eReligion = GET_PLAYER(ePlayer).getStateReligion();
	if (eReligion != NO_RELIGION) {
		szButton = GC.getReligionInfo(eReligion).getButton();
		widgetData.m_eWidgetType = WIDGET_HELP_RELIGION;
		widgetData.m_iData1 = eReligion;
		widgetData.m_iData2 = -1;
		widgetData.m_bOption = false;
	}
}

void CvGameTextMgr::getTradeScreenIcons(std::vector< std::pair<CvString, CvWidgetDataStruct> >& aIconInfos, PlayerTypes ePlayer) {
	aIconInfos.clear();
	for (int i = 0; i < GC.getNumCivicOptionInfos(); i++) {
		CivicTypes eCivic = GET_PLAYER(ePlayer).getCivics((CivicOptionTypes)i);
		CvWidgetDataStruct widgetData;
		widgetData.m_eWidgetType = WIDGET_PEDIA_JUMP_TO_CIVIC;
		widgetData.m_iData1 = eCivic;
		widgetData.m_iData2 = -1;
		widgetData.m_bOption = false;
		aIconInfos.push_back(std::make_pair(GC.getCivicInfo(eCivic).getButton(), widgetData));
	}

}

void CvGameTextMgr::getTradeScreenHeader(CvWString& szHeader, PlayerTypes ePlayer, PlayerTypes eOtherPlayer, bool bAttitude) {
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	szHeader.Format(L"%s - %s", CvWString(kPlayer.getName()).GetCString(), CvWString(kPlayer.getCivilizationDescription()).GetCString());
	if (bAttitude) {
		szHeader += CvWString::format(L" (%s)", GC.getAttitudeInfo(kPlayer.AI_getAttitude(eOtherPlayer)).getDescription());
	}
}

void CvGameTextMgr::getGlobeLayerName(GlobeLayerTypes eType, int iOption, CvWString& strName) {
	switch (eType) {
	case GLOBE_LAYER_STRATEGY:
		switch (iOption) {
		case 0:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_STRATEGY_VIEW");
			break;
		case 1:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_STRATEGY_NEW_LINE");
			break;
		case 2:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_STRATEGY_NEW_SIGN");
			break;
		case 3:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_STRATEGY_DELETE");
			break;
		case 4:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_STRATEGY_DELETE_LINES");
			break;
		}
		break;
	case GLOBE_LAYER_UNIT:
		switch (iOption) {
		case SHOW_ALL_MILITARY:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_UNITS_ALLMILITARY");
			break;
		case SHOW_TEAM_MILITARY:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_UNITS_TEAMMILITARY");
			break;
		case SHOW_ENEMIES_IN_TERRITORY:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_UNITS_ENEMY_TERRITORY_MILITARY");
			break;
		case SHOW_ENEMIES:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_UNITS_ENEMYMILITARY");
			break;
		case SHOW_PLAYER_DOMESTICS:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_UNITS_DOMESTICS");
			break;
		}
		break;
	case GLOBE_LAYER_RESOURCE:
		switch (iOption) {
		case SHOW_ALL_RESOURCES:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_RESOURCES_EVERYTHING");
			break;
		case SHOW_STRATEGIC_RESOURCES:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_RESOURCES_GENERAL");
			break;
		case SHOW_HAPPY_RESOURCES:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_RESOURCES_LUXURIES");
			break;
		case SHOW_HEALTH_RESOURCES:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_RESOURCES_FOOD");
			break;
		}
		break;
	case GLOBE_LAYER_RELIGION:
		strName = GC.getReligionInfo((ReligionTypes)iOption).getDescription();
		break;
	case GLOBE_LAYER_CULTURE:
	case GLOBE_LAYER_TRADE:
		// these have no sub-options
		strName.clear();
		break;
	}
}

void CvGameTextMgr::getPlotHelp(CvPlot* pMouseOverPlot, CvCity* pCity, CvPlot* pFlagPlot, bool bAlt, CvWStringBuffer& strHelp) {
	if (gDLL->getInterfaceIFace()->isCityScreenUp()) {
		if (pMouseOverPlot != NULL) {
			CvCity* pHeadSelectedCity = gDLL->getInterfaceIFace()->getHeadSelectedCity();
			if (pHeadSelectedCity != NULL) {
				if (pMouseOverPlot->getWorkingCity() == pHeadSelectedCity) {
					if (pMouseOverPlot->isRevealed(GC.getGameINLINE().getActiveTeam(), true)) {
						setPlotHelp(strHelp, pMouseOverPlot);
					}
				}
			}
		}
	} else {
		if (pCity != NULL) {
			setCityBarHelp(strHelp, pCity);
		} else if (pFlagPlot != NULL) {
			setPlotListHelp(strHelp, pFlagPlot, false, true);
		}

		if (strHelp.isEmpty()) {
			if (pMouseOverPlot != NULL) {
				if (pMouseOverPlot == gDLL->getInterfaceIFace()->getGotoPlot() || (bAlt && gDLL->getChtLvl() == 0)) // K-Mod. (alt does something else in cheat mode)
				{
					if (pMouseOverPlot->isActiveVisible(true)) {
						setCombatPlotHelp(strHelp, pMouseOverPlot);
					}
				}
			}
		}

		if (strHelp.isEmpty()) {
			if (pMouseOverPlot != NULL) {
				if (pMouseOverPlot->isRevealed(GC.getGameINLINE().getActiveTeam(), true)) {
					if (pMouseOverPlot->isActiveVisible(true)) {
						setPlotListHelp(strHelp, pMouseOverPlot, true, false);

						if (!strHelp.isEmpty()) {
							strHelp.append(L"\n");
						}
					}

					setPlotHelp(strHelp, pMouseOverPlot);
				}
			}
		}

		InterfaceModeTypes eInterfaceMode = gDLL->getInterfaceIFace()->getInterfaceMode();
		if (eInterfaceMode != INTERFACEMODE_SELECTION) {
			CvWString szTempBuffer;
			szTempBuffer.Format(SETCOLR L"%s" ENDCOLR NEWLINE, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getInterfaceModeInfo(eInterfaceMode).getDescription());

			switch (eInterfaceMode) {
			case INTERFACEMODE_REBASE:
				getRebasePlotHelp(pMouseOverPlot, szTempBuffer);
				break;

			case INTERFACEMODE_NUKE:
				getNukePlotHelp(pMouseOverPlot, szTempBuffer);
				break;

			default:
				break;
			}

			szTempBuffer += strHelp.getCString();
			strHelp.assign(szTempBuffer);
		}
	}
}

void CvGameTextMgr::getRebasePlotHelp(CvPlot* pPlot, CvWString& strHelp) {
	if (NULL != pPlot) {
		CvUnit* pHeadSelectedUnit = gDLL->getInterfaceIFace()->getHeadSelectedUnit();
		if (NULL != pHeadSelectedUnit) {
			if (pPlot->isFriendlyCity(*pHeadSelectedUnit, true)) {
				CvCity* pCity = pPlot->getPlotCity();
				if (NULL != pCity) {
					int iNumUnits = pCity->plot()->countNumAirUnits(GC.getGameINLINE().getActiveTeam());
					bool bFull = (iNumUnits >= pCity->getAirUnitCapacity(GC.getGameINLINE().getActiveTeam()));

					if (bFull) {
						strHelp += CvWString::format(SETCOLR, TEXT_COLOR("COLOR_WARNING_TEXT"));
					}

					strHelp += NEWLINE + gDLL->getText("TXT_KEY_CITY_BAR_AIR_UNIT_CAPACITY", iNumUnits, pCity->getAirUnitCapacity(GC.getGameINLINE().getActiveTeam()));

					if (bFull) {
						strHelp += ENDCOLR;
					}

					strHelp += NEWLINE;
				}
			}
		}
	}
}

void CvGameTextMgr::getNukePlotHelp(CvPlot* pPlot, CvWString& strHelp) {
	if (NULL != pPlot) {
		CvUnit* pHeadSelectedUnit = gDLL->getInterfaceIFace()->getHeadSelectedUnit();

		if (NULL != pHeadSelectedUnit) {
			for (int iI = 0; iI < MAX_TEAMS; iI++) {
				if (pHeadSelectedUnit->isNukeVictim(pPlot, ((TeamTypes)iI))) {
					if (!pHeadSelectedUnit->isEnemy((TeamTypes)iI)) {
						strHelp += NEWLINE + gDLL->getText("TXT_KEY_CANT_NUKE_FRIENDS");
						break;
					}
				}
			}
		}
	}
}

void CvGameTextMgr::getInterfaceCenterText(CvWString& strText) {
	strText.clear();
	if (!gDLL->getInterfaceIFace()->isCityScreenUp()) {
		if (GC.getGameINLINE().getWinner() != NO_TEAM) {
			strText = gDLL->getText("TXT_KEY_MISC_WINS_VICTORY", GET_TEAM(GC.getGameINLINE().getWinner()).getName().GetCString(), GC.getVictoryInfo(GC.getGameINLINE().getVictory()).getTextKeyWide());
		} else if (!(GET_PLAYER(GC.getGameINLINE().getActivePlayer()).isAlive())) {
			strText = gDLL->getText("TXT_KEY_MISC_DEFEAT");
		}
	}
}

void CvGameTextMgr::getTurnTimerText(CvWString& strText) {
	strText.clear();
	if (gDLL->getInterfaceIFace()->getShowInterface() == INTERFACE_SHOW || gDLL->getInterfaceIFace()->getShowInterface() == INTERFACE_ADVANCED_START) {
		if (GC.getGameINLINE().isMPOption(MPOPTION_TURN_TIMER)) {
			// Get number of turn slices remaining until end-of-turn
			int iTurnSlicesRemaining = GC.getGameINLINE().getTurnSlicesRemaining();

			if (iTurnSlicesRemaining > 0) {
				// Get number of seconds remaining
				int iTurnSecondsRemaining = ((int)floorf((float)(iTurnSlicesRemaining - 1) * ((float)gDLL->getMillisecsPerTurn() / 1000.0f)) + 1);
				int iTurnMinutesRemaining = (int)(iTurnSecondsRemaining / 60);
				iTurnSecondsRemaining = (iTurnSecondsRemaining % 60);
				int iTurnHoursRemaining = (int)(iTurnMinutesRemaining / 60);
				iTurnMinutesRemaining = (iTurnMinutesRemaining % 60);

				// Display time remaining
				CvWString szTempBuffer;
				szTempBuffer.Format(L"%d:%02d:%02d", iTurnHoursRemaining, iTurnMinutesRemaining, iTurnSecondsRemaining);
				strText += szTempBuffer;
			} else {
				// Flash zeroes
				if (iTurnSlicesRemaining % 2 == 0) {
					// Display 0
					strText += L"0:00";
				}
			}
		}

		if (GC.getGameINLINE().getGameState() == GAMESTATE_ON) {
			int iMinVictoryTurns = MAX_INT;
			for (int i = 0; i < GC.getNumVictoryInfos(); ++i) {
				TeamTypes eActiveTeam = GC.getGameINLINE().getActiveTeam();
				if (NO_TEAM != eActiveTeam) {
					int iCountdown = GET_TEAM(eActiveTeam).getVictoryCountdown((VictoryTypes)i);
					if (iCountdown > 0 && iCountdown < iMinVictoryTurns) {
						iMinVictoryTurns = iCountdown;
					}
				}
			}

			if (GC.getGameINLINE().isOption(GAMEOPTION_ADVANCED_START) && !GC.getGameINLINE().isOption(GAMEOPTION_ALWAYS_WAR) && GC.getGameINLINE().getElapsedGameTurns() <= GC.getDefineINT("PEACE_TREATY_LENGTH")) {
				if (!strText.empty()) {
					strText += L" -- ";
				}

				strText += gDLL->getText("TXT_KEY_MISC_ADVANCED_START_PEACE_REMAINING", GC.getDefineINT("PEACE_TREATY_LENGTH") - GC.getGameINLINE().getElapsedGameTurns());
			} else if (iMinVictoryTurns < MAX_INT) {
				if (!strText.empty()) {
					strText += L" -- ";
				}

				strText += gDLL->getText("TXT_KEY_MISC_TURNS_LEFT_TO_VICTORY", iMinVictoryTurns);
			} else if (GC.getGameINLINE().getMaxTurns() > 0) {
				if ((GC.getGameINLINE().getElapsedGameTurns() >= (GC.getGameINLINE().getMaxTurns() - 100)) && (GC.getGameINLINE().getElapsedGameTurns() < GC.getGameINLINE().getMaxTurns())) {
					if (!strText.empty()) {
						strText += L" -- ";
					}

					strText += gDLL->getText("TXT_KEY_MISC_TURNS_LEFT", (GC.getGameINLINE().getMaxTurns() - GC.getGameINLINE().getElapsedGameTurns()));
				}
			}
		}
	}
}


void CvGameTextMgr::getFontSymbols(std::vector< std::vector<wchar> >& aacSymbols, std::vector<int>& aiMaxNumRows) {
	aacSymbols.push_back(std::vector<wchar>());
	aiMaxNumRows.push_back(1);
	for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1)) {
		aacSymbols[aacSymbols.size() - 1].push_back((wchar)GC.getYieldInfo(eYield).getChar());
	}

	aacSymbols.push_back(std::vector<wchar>());
	aiMaxNumRows.push_back(2); // There are 26 rows of 25 icons each from the start of religions to the start of the generic symbols, 23 to the beginning of property symbols
	for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
		aacSymbols[aacSymbols.size() - 1].push_back((wchar)GC.getCommerceInfo(eCommerce).getChar());
	}

	aacSymbols.push_back(std::vector<wchar>());
	aiMaxNumRows.push_back(23); // There are 26 rows of 25 icons each from the start of religions to the start of the generic symbols, 23 to the beginning of property symbols
	for (ReligionTypes eReligion = (ReligionTypes)0; eReligion < GC.getNumReligionInfos(); eReligion = (ReligionTypes)(eReligion + 1)) {
		aacSymbols[aacSymbols.size() - 1].push_back((wchar)GC.getReligionInfo(eReligion).getChar());
		aacSymbols[aacSymbols.size() - 1].push_back((wchar)GC.getReligionInfo(eReligion).getHolyCityChar());
	}
	for (CorporationTypes eCorporation = (CorporationTypes)0; eCorporation < GC.getNumCorporationInfos(); eCorporation = (CorporationTypes)(eCorporation + 1)) {
		aacSymbols[aacSymbols.size() - 1].push_back((wchar)GC.getCorporationInfo(eCorporation).getChar());
		aacSymbols[aacSymbols.size() - 1].push_back((wchar)GC.getCorporationInfo(eCorporation).getHeadquarterChar());
	}

	aacSymbols.push_back(std::vector<wchar>());
	aiMaxNumRows.push_back(3); // There are 3 rows of 25 icons each from the start of property symbols to the start of the generic symbols

	aacSymbols.push_back(std::vector<wchar>());
	aiMaxNumRows.push_back(2);
	for (int iI = 0; iI < MAX_NUM_SYMBOLS; iI++) {
		aacSymbols[aacSymbols.size() - 1].push_back((wchar)gDLL->getSymbolID(iI));
	}

	aacSymbols.push_back(std::vector<wchar>());
	aiMaxNumRows.push_back(10);
	for (BonusTypes eBonus = (BonusTypes)0; eBonus < GC.getNumBonusInfos(); eBonus = (BonusTypes)(eBonus + 1)) {
		int iChar = GC.getBonusInfo(eBonus).getChar();
		aacSymbols[aacSymbols.size() - 1].push_back((wchar)iChar);
	}
}

void CvGameTextMgr::assignFontIds(int iFirstSymbolCode, int iPadAmount) {
	int iCurSymbolID = iFirstSymbolCode;  // first symbol code = 8483

	// set yield symbols
	for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1)) {
		GC.getYieldInfo(eYield).setChar(iCurSymbolID);
		++iCurSymbolID;
	}

	do {
		++iCurSymbolID;
	} while (iCurSymbolID % iPadAmount != 0);

	//8500

	// set commerce symbols
	for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
		GC.getCommerceInfo(eCommerce).setChar(iCurSymbolID);
		++iCurSymbolID;
	}

	do {
		++iCurSymbolID;
	} while (iCurSymbolID % iPadAmount != 0);

	//8525
	if (NUM_COMMERCE_TYPES < iPadAmount) {
		do {
			++iCurSymbolID;
		} while (iCurSymbolID % iPadAmount != 0);
	}

	//8550
	for (ReligionTypes eReligion = (ReligionTypes)0; eReligion < GC.getNumReligionInfos(); eReligion = (ReligionTypes)(eReligion + 1)) {
		CvReligionInfo& kReligion = GC.getReligionInfo(eReligion);
		kReligion.setChar(iCurSymbolID);
		++iCurSymbolID;
		kReligion.setHolyCityChar(iCurSymbolID);
		++iCurSymbolID;
	}

	//8826
	for (CorporationTypes eCorporation = (CorporationTypes)0; eCorporation < GC.getNumCorporationInfos(); eCorporation = (CorporationTypes)(eCorporation + 1)) {
		CvCorporationInfo& kCorporation = GC.getCorporationInfo(eCorporation);
		kCorporation.setChar(iCurSymbolID);
		++iCurSymbolID;
		kCorporation.setHeadquarterChar(iCurSymbolID);
		++iCurSymbolID;
	}

	//9102
	do {
		++iCurSymbolID;
	} while (iCurSymbolID % iPadAmount != 0);
	//9125
	if (2 * (GC.getNumReligionInfos() + GC.getNumCorporationInfos()) < iPadAmount) {
		do {
			++iCurSymbolID;
		} while (iCurSymbolID % iPadAmount != 0);
	}

	int iSavePosition = iCurSymbolID;

	// set bonus symbols
	int bonusBaseID = iSavePosition + 125;
	for (BonusTypes eBonus = (BonusTypes)0; eBonus < GC.getNumBonusInfos(); eBonus = (BonusTypes)(eBonus + 1)) {
		CvBonusInfo& kBonus = GC.getBonusInfo(eBonus);
		int bonusID = bonusBaseID + kBonus.getArtInfo()->getFontButtonIndex();
		kBonus.setChar(bonusID);
	}
	// 9206
	iCurSymbolID = iSavePosition + 125;
	iCurSymbolID -= MAX_NUM_SYMBOLS;
	do {
		--iCurSymbolID;
	} while (iCurSymbolID % iPadAmount != 0);

	// modified Sephi
		// set extra symbols
	for (int iI = 0; iI < MAX_NUM_SYMBOLS; iI++) {
		gDLL->setSymbolID(iI, iCurSymbolID);
		++iCurSymbolID;
	}
} // 9226

void CvGameTextMgr::getCityDataForAS(std::vector<CvWBData>& mapCityList, std::vector<CvWBData>& mapBuildingList, std::vector<CvWBData>& mapAutomateList) {
	const CvPlayer& kActivePlayer = GET_PLAYER(GC.getGameINLINE().getActivePlayer());

	CvWString szHelp;
	int iCost = kActivePlayer.getAdvancedStartCityCost(true);
	if (iCost > 0) {
		szHelp = gDLL->getText("TXT_KEY_CITY");
		szHelp += gDLL->getText("TXT_KEY_AS_UNREMOVABLE");
		mapCityList.push_back(CvWBData(0, szHelp, ARTFILEMGR.getInterfaceArtInfo("INTERFACE_BUTTONS_CITYSELECTION")->getPath()));
	}

	iCost = kActivePlayer.getAdvancedStartPopCost(true);
	if (iCost > 0) {
		szHelp = gDLL->getText("TXT_KEY_WB_AS_POPULATION");
		mapCityList.push_back(CvWBData(1, szHelp, ARTFILEMGR.getInterfaceArtInfo("INTERFACE_ANGRYCITIZEN_TEXTURE")->getPath()));
	}

	iCost = kActivePlayer.getAdvancedStartCultureCost(true);
	if (iCost > 0) {
		szHelp = gDLL->getText("TXT_KEY_ADVISOR_CULTURE");
		szHelp += gDLL->getText("TXT_KEY_AS_UNREMOVABLE");
		mapCityList.push_back(CvWBData(2, szHelp, ARTFILEMGR.getInterfaceArtInfo("CULTURE_BUTTON")->getPath()));
	}

	CvWStringBuffer szBuffer;
	for (int i = 0; i < GC.getNumBuildingClassInfos(); i++) {
		BuildingTypes eBuilding = (BuildingTypes)GC.getCivilizationInfo(kActivePlayer.getCivilizationType()).getCivilizationBuildings(i);

		if (eBuilding != NO_BUILDING) {
			if (GC.getBuildingInfo(eBuilding).getFreeStartEra() == NO_ERA || GC.getGameINLINE().getStartEra() < GC.getBuildingInfo(eBuilding).getFreeStartEra()) {
				// Building cost -1 denotes unit which may not be purchased
				iCost = kActivePlayer.getAdvancedStartBuildingCost(eBuilding, true);
				if (iCost > 0) {
					szBuffer.clear();
					setBuildingHelp(szBuffer, eBuilding);
					mapBuildingList.push_back(CvWBData(eBuilding, szBuffer.getCString(), GC.getBuildingInfo(eBuilding).getButton()));
				}
			}
		}
	}

	szHelp = gDLL->getText("TXT_KEY_ACTION_AUTOMATE_BUILD");
	mapAutomateList.push_back(CvWBData(0, szHelp, ARTFILEMGR.getInterfaceArtInfo("INTERFACE_AUTOMATE")->getPath()));
}

void CvGameTextMgr::getUnitDataForAS(std::vector<CvWBData>& mapUnitList) {
	const CvPlayer& kActivePlayer = GET_PLAYER(GC.getGameINLINE().getActivePlayer());

	CvWStringBuffer szBuffer;
	for (int i = 0; i < GC.getNumUnitClassInfos(); i++) {
		UnitTypes eUnit = (UnitTypes)GC.getCivilizationInfo(kActivePlayer.getCivilizationType()).getCivilizationUnits(i);
		if (eUnit != NO_UNIT) {
			// Unit cost -1 denotes unit which may not be purchased
			int iCost = kActivePlayer.getAdvancedStartUnitCost(eUnit, true);
			if (iCost > 0) {
				szBuffer.clear();
				setUnitHelp(szBuffer, eUnit);

				int iMaxUnitsPerCity = GC.getDefineINT("ADVANCED_START_MAX_UNITS_PER_CITY");
				if (iMaxUnitsPerCity >= 0 && GC.getUnitInfo(eUnit).isMilitarySupport()) {
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_WB_AS_MAX_UNITS_PER_CITY", iMaxUnitsPerCity));
				}
				mapUnitList.push_back(CvWBData(eUnit, szBuffer.getCString(), kActivePlayer.getUnitButton(eUnit)));
			}
		}
	}
}

void CvGameTextMgr::getImprovementDataForAS(std::vector<CvWBData>& mapImprovementList, std::vector<CvWBData>& mapRouteList) {
	const CvPlayer& kActivePlayer = GET_PLAYER(GC.getGameINLINE().getActivePlayer());

	for (int i = 0; i < GC.getNumRouteInfos(); i++) {
		RouteTypes eRoute = (RouteTypes)i;
		if (eRoute != NO_ROUTE) {
			// Route cost -1 denotes route which may not be purchased
			int iCost = kActivePlayer.getAdvancedStartRouteCost(eRoute, true);
			if (iCost > 0) {
				mapRouteList.push_back(CvWBData(eRoute, GC.getRouteInfo(eRoute).getDescription(), GC.getRouteInfo(eRoute).getButton()));
			}
		}
	}

	CvWStringBuffer szBuffer;
	for (int i = 0; i < GC.getNumImprovementInfos(); i++) {
		ImprovementTypes eImprovement = (ImprovementTypes)i;
		if (eImprovement != NO_IMPROVEMENT) {
			// Improvement cost -1 denotes Improvement which may not be purchased
			int iCost = kActivePlayer.getAdvancedStartImprovementCost(eImprovement, true);
			if (iCost > 0) {
				szBuffer.clear();
				setImprovementHelp(szBuffer, eImprovement);
				mapImprovementList.push_back(CvWBData(eImprovement, szBuffer.getCString(), GC.getImprovementInfo(eImprovement).getButton()));
			}
		}
	}
}

void CvGameTextMgr::getVisibilityDataForAS(std::vector<CvWBData>& mapVisibilityList) {
	// Unit cost -1 denotes unit which may not be purchased
	int iCost = GET_PLAYER(GC.getGameINLINE().getActivePlayer()).getAdvancedStartVisibilityCost(true);
	if (iCost > 0) {
		CvWString szHelp = gDLL->getText("TXT_KEY_WB_AS_VISIBILITY");
		szHelp += gDLL->getText("TXT_KEY_AS_UNREMOVABLE", iCost);
		mapVisibilityList.push_back(CvWBData(0, szHelp, ARTFILEMGR.getInterfaceArtInfo("INTERFACE_TECH_LOS")->getPath()));
	}
}

void CvGameTextMgr::getTechDataForAS(std::vector<CvWBData>& mapTechList) {
	mapTechList.push_back(CvWBData(0, gDLL->getText("TXT_KEY_WB_AS_TECH"), ARTFILEMGR.getInterfaceArtInfo("INTERFACE_BTN_TECH")->getPath()));
}

void CvGameTextMgr::getUnitDataForWB(std::vector<CvWBData>& mapUnitData) {
	CvWStringBuffer szBuffer;
	for (int i = 0; i < GC.getNumUnitInfos(); i++) {
		szBuffer.clear();
		setUnitHelp(szBuffer, (UnitTypes)i);
		mapUnitData.push_back(CvWBData(i, szBuffer.getCString(), GC.getUnitInfo((UnitTypes)i).getButton()));
	}
}

void CvGameTextMgr::getBuildingDataForWB(bool bStickyButton, std::vector<CvWBData>& mapBuildingData) {
	int iCount = 0;
	if (!bStickyButton) {
		mapBuildingData.push_back(CvWBData(iCount++, GC.getMissionInfo(MISSION_FOUND).getDescription(), GC.getMissionInfo(MISSION_FOUND).getButton()));
	}

	CvWStringBuffer szBuffer;
	for (int i = 0; i < GC.getNumBuildingInfos(); i++) {
		szBuffer.clear();
		setBuildingHelp(szBuffer, (BuildingTypes)i);
		mapBuildingData.push_back(CvWBData(iCount++, szBuffer.getCString(), GC.getBuildingInfo((BuildingTypes)i).getButton()));
	}
}

void CvGameTextMgr::getTerrainDataForWB(std::vector<CvWBData>& mapTerrainData, std::vector<CvWBData>& mapFeatureData, std::vector<CvWBData>& mapPlotData, std::vector<CvWBData>& mapRouteData) {
	CvWStringBuffer szBuffer;

	for (int i = 0; i < GC.getNumTerrainInfos(); i++) {
		if (!GC.getTerrainInfo((TerrainTypes)i).isGraphicalOnly()) {
			szBuffer.clear();
			setTerrainHelp(szBuffer, (TerrainTypes)i);
			mapTerrainData.push_back(CvWBData(i, szBuffer.getCString(), GC.getTerrainInfo((TerrainTypes)i).getButton()));
		}
	}

	for (int i = 0; i < GC.getNumFeatureInfos(); i++) {
		for (int k = 0; k < GC.getFeatureInfo((FeatureTypes)i).getArtInfo()->getNumVarieties(); k++) {
			szBuffer.clear();
			setFeatureHelp(szBuffer, (FeatureTypes)i);
			mapFeatureData.push_back(CvWBData(i + GC.getNumFeatureInfos() * k, szBuffer.getCString(), GC.getFeatureInfo((FeatureTypes)i).getArtInfo()->getVariety(k).getVarietyButton()));
		}
	}

	mapPlotData.push_back(CvWBData(0, gDLL->getText("TXT_KEY_WB_PLOT_TYPE_MOUNTAIN"), ARTFILEMGR.getInterfaceArtInfo("WORLDBUILDER_PLOT_TYPE_MOUNTAIN")->getPath()));
	mapPlotData.push_back(CvWBData(1, gDLL->getText("TXT_KEY_WB_PLOT_TYPE_HILL"), ARTFILEMGR.getInterfaceArtInfo("WORLDBUILDER_PLOT_TYPE_HILL")->getPath()));
	mapPlotData.push_back(CvWBData(2, gDLL->getText("TXT_KEY_WB_PLOT_TYPE_PLAINS"), ARTFILEMGR.getInterfaceArtInfo("WORLDBUILDER_PLOT_TYPE_PLAINS")->getPath()));
	mapPlotData.push_back(CvWBData(3, gDLL->getText("TXT_KEY_WB_PLOT_TYPE_OCEAN"), ARTFILEMGR.getInterfaceArtInfo("WORLDBUILDER_PLOT_TYPE_OCEAN")->getPath()));

	for (int i = 0; i < GC.getNumRouteInfos(); i++) {
		mapRouteData.push_back(CvWBData(i, GC.getRouteInfo((RouteTypes)i).getDescription(), GC.getRouteInfo((RouteTypes)i).getButton()));
	}
	mapRouteData.push_back(CvWBData(GC.getNumRouteInfos(), gDLL->getText("TXT_KEY_WB_RIVER_PLACEMENT"), ARTFILEMGR.getInterfaceArtInfo("WORLDBUILDER_RIVER_PLACEMENT")->getPath()));
}

void CvGameTextMgr::getTerritoryDataForWB(std::vector<CvWBData>& mapTerritoryData) {
	for (int i = 0; i <= MAX_CIV_PLAYERS; i++) {
		CvWString szName = gDLL->getText("TXT_KEY_MAIN_MENU_NONE");
		CvString szButton = GC.getCivilizationInfo(GET_PLAYER(BARBARIAN_PLAYER).getCivilizationType()).getButton();

		if (GET_PLAYER((PlayerTypes)i).isEverAlive()) {
			szName = GET_PLAYER((PlayerTypes)i).getName();
			szButton = GC.getCivilizationInfo(GET_PLAYER((PlayerTypes)i).getCivilizationType()).getButton();
		}
		mapTerritoryData.push_back(CvWBData(i, szName, szButton));
	}
}


void CvGameTextMgr::getTechDataForWB(std::vector<CvWBData>& mapTechData) {
	CvWStringBuffer szBuffer;
	for (int i = 0; i < GC.getNumTechInfos(); i++) {
		szBuffer.clear();
		setTechHelp(szBuffer, (TechTypes)i);
		mapTechData.push_back(CvWBData(i, szBuffer.getCString(), GC.getTechInfo((TechTypes)i).getButton()));
	}
}

void CvGameTextMgr::getPromotionDataForWB(std::vector<CvWBData>& mapPromotionData) {
	CvWStringBuffer szBuffer;
	for (int i = 0; i < GC.getNumPromotionInfos(); i++) {
		szBuffer.clear();
		setPromotionHelp(szBuffer, (PromotionTypes)i, false);
		mapPromotionData.push_back(CvWBData(i, szBuffer.getCString(), GC.getPromotionInfo((PromotionTypes)i).getButton()));
	}
}

void CvGameTextMgr::getBonusDataForWB(std::vector<CvWBData>& mapBonusData) {
	CvWStringBuffer szBuffer;
	for (int i = 0; i < GC.getNumBonusInfos(); i++) {
		szBuffer.clear();
		setBonusHelp(szBuffer, (BonusTypes)i);
		mapBonusData.push_back(CvWBData(i, szBuffer.getCString(), GC.getBonusInfo((BonusTypes)i).getButton()));
	}
}

void CvGameTextMgr::getImprovementDataForWB(std::vector<CvWBData>& mapImprovementData) {
	CvWStringBuffer szBuffer;
	for (int i = 0; i < GC.getNumImprovementInfos(); i++) {
		CvImprovementInfo& kImprovement = GC.getImprovementInfo((ImprovementTypes)i);
		if (!kImprovement.isGraphicalOnly()) {
			szBuffer.clear();
			setImprovementHelp(szBuffer, (ImprovementTypes)i);
			mapImprovementData.push_back(CvWBData(i, szBuffer.getCString(), kImprovement.getButton()));
		}
	}
}

void CvGameTextMgr::getReligionDataForWB(bool bHolyCity, std::vector<CvWBData>& mapReligionData) {
	for (int i = 0; i < GC.getNumReligionInfos(); ++i) {
		const CvReligionInfo& kReligion = GC.getReligionInfo((ReligionTypes)i);
		CvWString strDescription = kReligion.getDescription();
		if (bHolyCity) {
			strDescription = gDLL->getText("TXT_KEY_WB_HOLYCITY", strDescription.GetCString());
		}
		mapReligionData.push_back(CvWBData(i, strDescription, kReligion.getButton()));
	}
}


void CvGameTextMgr::getCorporationDataForWB(bool bHeadquarters, std::vector<CvWBData>& mapCorporationData) {
	for (int i = 0; i < GC.getNumCorporationInfos(); ++i) {
		const CvCorporationInfo& kCorporation = GC.getCorporationInfo((CorporationTypes)i);
		CvWString strDescription = kCorporation.getDescription();
		if (bHeadquarters) {
			strDescription = gDLL->getText("TXT_KEY_CORPORATION_HEADQUARTERS", strDescription.GetCString());
		}
		mapCorporationData.push_back(CvWBData(i, strDescription, kCorporation.getButton()));
	}
}

void CvGameTextMgr::buildUnitRangeUnboundString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isUnitRangeUnbound()) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_UNIT_RANGE_UNBOUND"));
	}
}

void CvGameTextMgr::buildUnitTerritoryUnboundString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isUnitTerritoryUnbound()) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_UNIT_TERRITORY_UNBOUND"));
	}
}

void CvGameTextMgr::buildUnitRangeChangeString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	const CvTechInfo& kTech = GC.getTechInfo(eTech);
	if (kTech.getUnitRangeChange() != 0) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_UNIT_RANGE_CHANGE", kTech.getUnitRangeChange()));
	}
}

void CvGameTextMgr::buildUnitRangePercentageChangeString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	const CvTechInfo& kTech = GC.getTechInfo(eTech);
	if (kTech.getUnitRangePercentChange() != 0) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_UNIT_RANGE_MODIFY", kTech.getUnitRangePercentChange()));
	}
}

/*
	+14 from Worked Tiles
	+2 from Specialists
	+5 from Corporations
	+1 from Buildings
	----------------------- |
	Base Food Produced: 22  |-- only if there are modifiers
	+25% from Buildings     |
	-----------------------
	Total Food Produced: 27
	=======================
	+16 for Population
	+2 for Health
	-----------------------
	Total Food Consumed: 18
	=======================
	Net Food: +9            or
	Net Food for Settler: 9
	=======================
	* Lighthouse: +3
	* Supermarket: +1
*/
void CvGameTextMgr::setFoodHelp(CvWStringBuffer& szBuffer, CvCity& city) {
	FAssertMsg(NO_PLAYER != city.getOwnerINLINE(), "City must have an owner");

	CvYieldInfo& info = GC.getYieldInfo(YIELD_FOOD);

	// Worked Tiles
	int iTileFood = 0;
	for (int i = 0; i < city.getNumCityPlots(); i++) {
		if (city.isWorkingPlot(i)) {
			CvPlot* pPlot = city.getCityIndexPlot(i);

			if (pPlot != NULL) {
				iTileFood += pPlot->getYield(YIELD_FOOD);
			}
		}
	}

	int iBaseRate = 0;
	if (iTileFood != 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_WORKED_TILES_YIELD", iTileFood, info.getChar()));
		iBaseRate += iTileFood;
	}

	// Trade
	bool bNeedSubtotal = false;
	int iTradeFood = city.getTradeYield(YIELD_FOOD);
	if (iTradeFood != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_SPECIALIST_COMMERCE", iTradeFood, info.getChar(), L"TXT_KEY_HEADING_TRADEROUTE_LIST"));
		iBaseRate += iTradeFood;
		bNeedSubtotal = true;
	}

	// Specialists
	int iSpecialistFood = 0;
	for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
		iSpecialistFood += GET_PLAYER(city.getOwnerINLINE()).specialistYield(eSpecialist, YIELD_FOOD) * (city.getSpecialistCount(eSpecialist) + city.getFreeSpecialistCount(eSpecialist));
	}
	if (iSpecialistFood != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_SPECIALIST_COMMERCE", iSpecialistFood, info.getChar(), L"TXT_KEY_CONCEPT_SPECIALISTS"));
		iBaseRate += iSpecialistFood;
		bNeedSubtotal = true;
	}

	// Corporations
	int iCorporationFood = city.getCorporationYield(YIELD_FOOD);
	if (iCorporationFood != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_CORPORATION_COMMERCE", iCorporationFood, info.getChar()));
		iBaseRate += iCorporationFood;
		bNeedSubtotal = true;
	}

	// Buildings
	int iBuildingFood = 0;
	for (BuildingTypes eBuilding = (BuildingTypes)0; eBuilding < GC.getNumBuildingInfos(); eBuilding = (BuildingTypes)(eBuilding + 1)) {
		int iCount = city.getNumActiveBuilding(eBuilding);
		if (iCount > 0) {
			CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);
			iBuildingFood += iCount * (kBuilding.getYieldChange(YIELD_FOOD) + city.getBuildingYieldChange((BuildingClassTypes)kBuilding.getBuildingClassType(), YIELD_FOOD));
		}
	}
	if (iBuildingFood != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_BUILDING_COMMERCE", iBuildingFood, info.getChar()));
		iBaseRate += iBuildingFood;
		bNeedSubtotal = true;
	}

	// Base and modifiers (only if there are modifiers since total is always shown)
	if (city.getBaseYieldRateModifier(YIELD_FOOD) != 100) {
		szBuffer.append(SEPARATOR);
		szBuffer.append(NEWLINE);
		// shows Base Food and lists all modifiers
		setYieldHelp(szBuffer, city, YIELD_FOOD);
	} else {
		szBuffer.append(NEWLINE);
	}

	// Total Produced
	int iBaseModifier = city.getBaseYieldRateModifier(YIELD_FOOD);
	int iRate = iBaseModifier * iBaseRate / 100;
	szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_TOTAL_FOOD_PRODUCED", iRate));

	// ==========================
	szBuffer.append(DOUBLE_SEPARATOR);

	int iFoodConsumed = 0;

	// Eaten
	int iEatenFood = city.getPopulation() * GC.getFOOD_CONSUMPTION_PER_POPULATION();
	if (iEatenFood != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_EATEN_FOOD", iEatenFood));
		iFoodConsumed += iEatenFood;
	}

	// Health
	int iSpoiledFood = -city.healthRate();
	if (iSpoiledFood != 0) {
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_SPOILED_FOOD", iSpoiledFood));
		iFoodConsumed += iSpoiledFood;
	}

	// Total Consumed
	szBuffer.append(NEWLINE);
	szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_TOTAL_FOOD_CONSUMED", iFoodConsumed));

	// ==========================
	szBuffer.append(DOUBLE_SEPARATOR NEWLINE);
	iRate -= iFoodConsumed;

	// Production
	if (city.isFoodProduction() && iRate > 0) {
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_NET_FOOD_PRODUCTION", iRate, city.getProductionNameKey()));
	} else {
		// cannot starve a size 1 city with no food in
		if (iRate < 0 && city.getPopulation() == 1 && city.getFood() == 0) {
			iRate = 0;
		}

		// Net Food
		if (iRate > 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_NET_FOOD_GROW", iRate));
		} else if (iRate < 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_NET_FOOD_SHRINK", iRate));
		} else {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_NET_FOOD_STAGNATE"));
		}
	}

	// ==========================

	if (city.getOwnerINLINE() == GC.getGame().getActivePlayer() && getOptionBOOL("MiscHover__BuildingAdditionalFood", true)) {
		setBuildingAdditionalYieldHelp(szBuffer, city, YIELD_FOOD, DOUBLE_SEPARATOR);
	}
}

/*
	Defense = max(building, culture) + player

	+50% from Buildings
	+10% from Culture (60%)
	+25% from Wonders
	-----------------------
	Total Defense: 85%
	-----------------------
	-37% from Bombardment
	-----------------------
	Net Defense: 48%
	=======================
	* Castle: +50%
	=======================
	+50% from Buildings
	-----------------------
	Total Bombard Defense: 50%
	=======================
	* Castle: +25%
*/
void CvGameTextMgr::setDefenseHelp(CvWStringBuffer& szBuffer, CvCity& city) {
	FAssertMsg(NO_PLAYER != city.getOwnerINLINE(), "City must have an owner");

	bool bFirst = true;

	// Buildings
	int iBuildingDefense = city.getBuildingDefense();
	if (iBuildingDefense != 0) {
		bFirst = false;
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_BUILDING_DEFENSE", iBuildingDefense));
	}

	// Culture
	int iCultureDefense = city.getNaturalDefense();
	if (iCultureDefense != 0) {
		if (!bFirst) {
			szBuffer.append(NEWLINE);
		} else {
			bFirst = false;
		}
		if (iBuildingDefense == 0) {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_CULTURE_DEFENSE", iCultureDefense));
		} else {
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_CULTURE_DEFENSE_PARTIAL", std::max(iCultureDefense, iBuildingDefense) - iBuildingDefense, iCultureDefense));
		}
	}

	// Wonders (or other player sources in mods)
	int iWonderDefense = GET_PLAYER(city.getOwnerINLINE()).getCityDefenseModifier();
	if (iWonderDefense != 0) {
		if (!bFirst) {
			szBuffer.append(NEWLINE);
		} else {
			bFirst = false;
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_WONDER_DEFENSE", iWonderDefense));
	}

	// Total
	int iDefense = city.getTotalDefense(false);
	if (!bFirst) {
		szBuffer.append(SEPARATOR NEWLINE);
	}
	szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_TOTAL_DEFENSE", iDefense));

	// Damage and Net
	int iDamage = GC.getMAX_CITY_DEFENSE_DAMAGE() * city.getDefenseDamage() / GC.getMAX_CITY_DEFENSE_DAMAGE();
	if (iDamage != 0) {
		szBuffer.append(SEPARATOR NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_DEFENSE_DAMAGE", -iDamage));
		szBuffer.append(SEPARATOR NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_NET_DEFENSE", std::max(0, iDefense - iDamage)));
	}

	if (city.getOwnerINLINE() == GC.getGame().getActivePlayer() && getOptionBOOL("MiscHover__BuildingAdditionalDefence", true)) {
		setBuildingAdditionalDefenceHelp(szBuffer, city, SEPARATOR);
	}

	// ==========================
	// Bombardment Defense

	szBuffer.append(DOUBLE_SEPARATOR NEWLINE);
	szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_TOTAL_BOMBARD_DEFENSE", city.getBuildingBombardDefense()));

	if (city.getOwnerINLINE() == GC.getGame().getActivePlayer() && getOptionBOOL("MiscHover__BuildingAdditionalDefence", true)) {
		setBuildingAdditionalBombardDefenceHelp(szBuffer, city, SEPARATOR);
	}

	// ==========================
	// Air Defense - not worth it for just one building
}

bool CvGameTextMgr::setBuildingAdditionalDefenceHelp(CvWStringBuffer& szBuffer, const CvCity& city, const CvWString& szStart, bool bStarted) {
	CvWString szLabel;
	CvCivilizationInfo& kCivilization = GC.getCivilizationInfo(GET_PLAYER(city.getOwnerINLINE()).getCivilizationType());

	for (BuildingClassTypes eBuildingClass = (BuildingClassTypes)0; eBuildingClass < GC.getNumBuildingClassInfos(); eBuildingClass = (BuildingClassTypes)(eBuildingClass + 1)) {
		BuildingTypes eBuilding = (BuildingTypes)kCivilization.getCivilizationBuildings(eBuildingClass);

		if (eBuilding != NO_BUILDING && city.canConstruct(eBuilding, false, true, false)) {
			int iChange = city.getAdditionalDefenseByBuilding(eBuilding);

			if (iChange != 0) {
				if (!bStarted) {
					szBuffer.append(szStart);
					bStarted = true;
				}

				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				setResumableValueChangeHelp(szBuffer, szLabel, L": ", L"", iChange, gDLL->getSymbolID(DEFENSE_CHAR), true, true);
			}
		}
	}

	return bStarted;
}

bool CvGameTextMgr::setBuildingAdditionalBombardDefenceHelp(CvWStringBuffer& szBuffer, const CvCity& city, const CvWString& szStart, bool bStarted) {
	CvWString szLabel;
	CvCivilizationInfo& kCivilization = GC.getCivilizationInfo(GET_PLAYER(city.getOwnerINLINE()).getCivilizationType());

	for (BuildingClassTypes eBuildingClass = (BuildingClassTypes)0; eBuildingClass < GC.getNumBuildingClassInfos(); eBuildingClass = (BuildingClassTypes)(eBuildingClass + 1)) {
		BuildingTypes eBuilding = (BuildingTypes)kCivilization.getCivilizationBuildings(eBuildingClass);

		if (eBuilding != NO_BUILDING && city.canConstruct(eBuilding, false, true, false)) {
			int iChange = city.getAdditionalBombardDefenseByBuilding(eBuilding);

			if (iChange != 0) {
				if (!bStarted) {
					szBuffer.append(szStart);
					bStarted = true;
				}

				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				setResumableValueChangeHelp(szBuffer, szLabel, L": ", L"", iChange, gDLL->getSymbolID(DEFENSE_CHAR), true, true);
			}
		}
	}

	return bStarted;
}

void CvGameTextMgr::buildForestYieldChangeString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	CvWString szTempBuffer;
	if (bList)
		szTempBuffer.Format(L"\n%s", gDLL->getText("TXT_KEY_TECH_CHANGE_FOREST_YIELD").c_str());
	else
		szTempBuffer.Format(L"%s", gDLL->getText("TXT_KEY_TECH_CHANGE_FOREST_YIELD").c_str());

	setYieldChangeHelp(szBuffer, szTempBuffer, L": ", L"", GC.getTechInfo(eTech).getForestPlotYieldChangeArray(), false, bList);
}

void CvGameTextMgr::buildRiverYieldChangeString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	CvWString szTempBuffer;
	if (bList)
		szTempBuffer.Format(L"\n%s", gDLL->getText("TXT_KEY_TECH_CHANGE_RIVER_YIELD").c_str());
	else
		szTempBuffer.Format(L"%s", gDLL->getText("TXT_KEY_TECH_CHANGE_RIVER_YIELD").c_str());

	setYieldChangeHelp(szBuffer, szTempBuffer, L": ", L"", GC.getTechInfo(eTech).getRiverPlotYieldChangeArray(), false, bList);
}

void CvGameTextMgr::buildSeaYieldChangeString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	CvWString szTempBuffer;
	if (bList)
		szTempBuffer.Format(L"\n%s", gDLL->getText("TXT_KEY_TECH_CHANGE_SEA_YIELD").c_str());
	else
		szTempBuffer.Format(L"%s", gDLL->getText("TXT_KEY_TECH_CHANGE_SEA_YIELD").c_str());

	setYieldChangeHelp(szBuffer, szTempBuffer, L": ", L"", GC.getTechInfo(eTech).getSeaPlotYieldChangeArray(), false, bList);
}

void CvGameTextMgr::buildCaptureCitiesString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (bList) {
		szBuffer.append(NEWLINE);
	}
	szBuffer.append(gDLL->getText("TXT_KEY_TECH_CAPTURE_CITIES_HELP"));
}

void CvGameTextMgr::buildObsoleteUnitString(CvWStringBuffer& szBuffer, int iItem, bool bList, bool bPlayerContext) {
	CvWString szTempBuffer;

	if (bList) {
		szBuffer.append(NEWLINE);
	}
	szBuffer.append(gDLL->getText("TXT_KEY_TECH_OBSOLETES", GC.getUnitInfo((UnitTypes)iItem).getTextKeyWide()));
}

void CvGameTextMgr::buildObsoleteBuildString(CvWStringBuffer& szBuffer, int iItem, bool bList, bool bPlayerContext) {
	if (bList) {
		szBuffer.append(NEWLINE);
	}
	szBuffer.append(gDLL->getText("TXT_KEY_TECH_OBSOLETES_BUILD", GC.getBuildInfo((BuildTypes)iItem).getTextKeyWide()));
}

void CvGameTextMgr::buildCanPassPeaksString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isCanPassPeaks() && (!bPlayerContext || !GET_TEAM(GC.getGameINLINE().getActiveTeam()).isCanPassPeaks())) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_CAN_PASS_PEAKS"));
	}
}

void CvGameTextMgr::buildMoveFastPeaksString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isMoveFastPeaks() && (!bPlayerContext || !GET_TEAM(GC.getGameINLINE().getActiveTeam()).isMoveFastPeaks())) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MOVE_FAST_PEAKS"));
	}
}

void CvGameTextMgr::buildCanFoundOnPeaksString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isCanFoundOnPeaks() && (!bPlayerContext || !GET_TEAM(GC.getGameINLINE().getActiveTeam()).isCanFoundOnPeaks())) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_CAN_FOUND_ON_PEAKS"));
	}
}

void CvGameTextMgr::buildEmbassyString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isEmbassyTrading() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isEmbassyTrading()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_EMBASSIES"));
	}
}

void CvGameTextMgr::buildLimitedBordersString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isLimitedBordersTrading() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isLimitedBordersTrading()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_LIMITED_BORDERS"));
	}
}

void CvGameTextMgr::buildFreeTradeAgreementString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isFreeTradeAgreementTrading() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isFreeTradeAgreementTrading()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_FREE_TRADE_AGREEMENT"));
	}
}

void CvGameTextMgr::buildNonAggressionString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	if (GC.getTechInfo(eTech).isNonAggressionTrading() && (!bPlayerContext || !(GET_TEAM(GC.getGameINLINE().getActiveTeam()).isNonAggressionTrading()))) {
		if (bList) {
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ENABLES_NON_AGGRESSION"));
	}
}

void CvGameTextMgr::buildWorldViewRevoltTurnChangeString(CvWStringBuffer& szBuffer, TechTypes eTech, bool bList, bool bPlayerContext) {
	const CvTechInfo& kTech = GC.getTechInfo(eTech);
	for (WorldViewTypes eWorldView = (WorldViewTypes)0; eWorldView < NUM_WORLD_VIEWS; eWorldView = (WorldViewTypes)(eWorldView + 1)) {
		if (kTech.getWorldViewRevoltTurnChange(eWorldView) != 0) {
			if (bList) {
				szBuffer.append(NEWLINE);
			}
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_WV_REVOLT_TURN_CHANGE", GC.getWorldViewInfo(eWorldView).getDescription(), kTech.getWorldViewRevoltTurnChange(eWorldView)));
		}
	}
}
