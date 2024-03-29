// unitAI.cpp

#include "CvGameCoreDLL.h"
#include "CvUnitAI.h"
#include "CvMap.h"
#include "CvArea.h"
#include "CvPlot.h"
#include "CvGlobals.h"
#include "CvGameAI.h"
#include "CvTeamAI.h"
#include "CvPlayerAI.h"
#include "CvGameCoreUtils.h"
#include "CvRandom.h"
#include "CyUnit.h"
#include "CyArgsList.h"
#include "CvDLLPythonIFaceBase.h"
#include "CvInfos.h"
#include "FProfiler.h"
#include "FAStarNode.h"
#include "CvIniOptions.h"

// interface uses
#include "CvDLLInterfaceIFaceBase.h"
#include "CvDLLFAStarIFaceBase.h"

#include "BetterBTSAI.h"

#define FOUND_RANGE				(7)

// Public Functions...

CvUnitAI::CvUnitAI() {
	AI_reset();
}


CvUnitAI::~CvUnitAI() {
	AI_uninit();
}


void CvUnitAI::AI_init(UnitAITypes eUnitAI) {
	AI_reset(eUnitAI);

	//--------------------------------
	// Init other game data
	AI_setBirthmark(GC.getGameINLINE().getSorenRandNum(10000, "AI Unit Birthmark"));

	FAssertMsg(AI_getUnitAIType() != NO_UNITAI, "AI_getUnitAIType() is not expected to be equal with NO_UNITAI");
	area()->changeNumAIUnits(getOwnerINLINE(), AI_getUnitAIType(), 1);
	GET_PLAYER(getOwnerINLINE()).AI_changeNumAIUnits(AI_getUnitAIType(), 1);
}


void CvUnitAI::AI_uninit() {
}


void CvUnitAI::AI_reset(UnitAITypes eUnitAI) {
	AI_uninit();

	m_iBirthmark = 0;

	m_eUnitAIType = eUnitAI;

	m_iAutomatedAbortTurn = -1;
}

// AI_update returns true when we should abort the loop and wait until next slice
bool CvUnitAI::AI_update() {
	PROFILE_FUNC();

	CvUnit* pTransportUnit;

	FAssertMsg(canMove(), "canMove is expected to be true");
	FAssertMsg(isGroupHead(), "isGroupHead is expected to be true"); // XXX is this a good idea???

	// allow python to handle it
	if (GC.getUSE_AI_UNIT_UPDATE_CALLBACK()) // K-Mod. block unused python callbacks
	{
		CyUnit* pyUnit = new CyUnit(this);
		CyArgsList argsList;
		argsList.add(gDLL->getPythonIFace()->makePythonObject(pyUnit));	// pass in unit class
		long lResult = 0;
		gDLL->getPythonIFace()->callFunction(PYGameModule, "AI_unitUpdate", argsList.makeFunctionArgs(), &lResult);
		delete pyUnit;	// python fxn must not hold on to this pointer
		if (lResult == 1) {
			return false;
		}
	}

	if (getDomainType() == DOMAIN_LAND) {
		if (plot()->isWater() && !(canMoveAllTerrain() || plot()->isLandUnitWaterSafe())) {
			getGroup()->pushMission(MISSION_SKIP);
			return false;
		} else {
			pTransportUnit = getTransportUnit();

			if (pTransportUnit != NULL) {
				// K-Mod. Note: transport units with cargo always have their turn before the cargo does - so... well... I've changed the skip condition.
				if (pTransportUnit->getGroup()->headMissionQueueNode() != NULL ||
					(pTransportUnit->getGroup()->AI_getMissionAIPlot() && !atPlot(pTransportUnit->getGroup()->AI_getMissionAIPlot()))) {
					getGroup()->pushMission(MISSION_SKIP);
					return false;
				}
			}
		}
	}

	if (AI_afterAttack()) {
		return false;
	}

	if (getGroup()->isAutomated() && isHuman()) // When the AI fills in for a human player, they should ignore automation
	{
		switch (getGroup()->getAutomateType()) {
		case AUTOMATE_BUILD:
			if (AI_getUnitAIType() == UNITAI_WORKER) {
				AI_workerMove();
			} else if (AI_getUnitAIType() == UNITAI_WORKER_SEA) {
				AI_workerSeaMove();
			} else if (AI_getUnitAIType() == UNITAI_SLAVE) {
				AI_slaveMove();
			} else if (AI_getUnitAIType() == UNITAI_GATHERER) {
				AI_gathererMove();
			} else {
				FAssert(false);
			}
			break;

		case AUTOMATE_NETWORK:
			AI_networkAutomated();
			// XXX else wake up???
			break;

		case AUTOMATE_CITY:
			AI_cityAutomated();
			// XXX else wake up???
			break;

		case AUTOMATE_EXPLORE:
			switch (getDomainType()) {
			case DOMAIN_SEA:
				AI_exploreSeaMove();
				break;

			case DOMAIN_AIR:
				// if we are cargo (on a carrier), hold if the carrier is not done moving yet
				pTransportUnit = getTransportUnit();
				if (pTransportUnit != NULL) {
					if (pTransportUnit->isAutomated() && pTransportUnit->canMove() && pTransportUnit->getGroup()->getActivityType() != ACTIVITY_HOLD) {
						getGroup()->pushMission(MISSION_SKIP);
						break;
					}
				}
				// Have air units explore like AI units do
				AI_exploreAirMove();
				break;

			case DOMAIN_LAND:
				AI_exploreMove();
				break;

			default:
				FAssert(false);
				break;
			}

			// if we have air cargo (we are a carrier), and we done moving, explore with the aircraft as well
			if (hasCargo() && domainCargo() == DOMAIN_AIR && (!canMove() || getGroup()->getActivityType() == ACTIVITY_HOLD)) {
				std::vector<CvUnit*> aCargoUnits;
				getCargoUnits(aCargoUnits);
				for (uint i = 0; i < aCargoUnits.size() && isAutomated(); ++i) {
					CvUnit* pCargoUnit = aCargoUnits[i];
					if (pCargoUnit->getDomainType() == DOMAIN_AIR) {
						if (pCargoUnit->canMove()) {
							pCargoUnit->getGroup()->setAutomateType(AUTOMATE_EXPLORE);
							pCargoUnit->getGroup()->setActivityType(ACTIVITY_AWAKE);
						}
					}
				}
			}
			break;

		case AUTOMATE_RELIGION:
			if (AI_getUnitAIType() == UNITAI_MISSIONARY) {
				AI_missionaryMove();
			}
			break;

		case AUTOMATE_SHADOW:
			FAssertMsg(getShadowUnit() != NULL, "Shadowing Requires a unit to Shadow!");
			AI_shadowMove();
			break;

		case AUTOMATE_ESPIONAGE:
			AI_autoEspionage();
			break;

		case AUTOMATE_PILLAGE:
			AI_autoPillageMove();
			break;

		case AUTOMATE_HUNT:
			AI_searchAndDestroyMove();
			break;

		case AUTOMATE_CITY_DEFENCE:
			AI_cityDefence();
			break;

		case AUTOMATE_BORDER_PATROL:
			AI_borderPatrol();
			break;

		case AUTOMATE_HURRY:
			AI_merchantMove();
			break;

		case AUTOMATE_PIRACY:
			AI_pirateSeaMove();
			break;

			//Yes, these automations do the same thing, but they act differently for different units. 
		case AUTOMATE_AIRBOMB:
		case AUTOMATE_AIRSTRIKE:
			AI_autoAirStrike();
			break;

		case AUTOMATE_AIR_RECON:
			AI_exploreAirMove();
			break;

		case AUTOMATE_PROMOTIONS:
		case AUTOMATE_CANCEL_PROMOTIONS:
		case AUTOMATE_UPGRADING:
		case AUTOMATE_CANCEL_UPGRADING:
			FAssertMsg(false, "SelectionGroup Should Not be Using These Automations!")
				break;

		default:
			FAssert(false);
			break;
		}

		// if no longer automated, then we want to bail
		return !getGroup()->isAutomated();
	} else {
		switch (AI_getUnitAIType()) {
		case UNITAI_UNKNOWN:
			getGroup()->pushMission(MISSION_SKIP);
			break;

		case UNITAI_ANIMAL:
			AI_animalMove();
			break;

		case UNITAI_SETTLE:
			AI_settleMove();
			break;

		case UNITAI_WORKER:
			AI_workerMove();
			break;

		case UNITAI_SLAVE:
			AI_slaveMove();
			break;

		case UNITAI_GATHERER:
			AI_gathererMove();
			break;

		case UNITAI_SLAVER:
			AI_slaverMove();
			break;

		case UNITAI_HUNTER:
			AI_hunterMove();
			break;

		case UNITAI_BARBARIAN:
			AI_barbarianMove();
			break;

		case UNITAI_BARBARIAN_ATTACK_CITY:
			AI_barbarianAttackCityMove();
			break;

		case UNITAI_BARBARIAN_LEADER:
			AI_barbarianLeaderMove();
			break;

		case UNITAI_ATTACK:
			if (isBarbarian()) {
				AI_barbAttackMove();
			} else {
				AI_attackMove();
			}
			break;

		case UNITAI_ATTACK_CITY:
			AI_attackCityMove();
			break;

		case UNITAI_COLLATERAL:
			AI_collateralMove();
			break;

		case UNITAI_PILLAGE:
			AI_pillageMove();
			break;

		case UNITAI_RESERVE:
			AI_reserveMove();
			break;

		case UNITAI_COUNTER:
			AI_counterMove();
			break;

		case UNITAI_PARADROP:
			AI_paratrooperMove();
			break;

		case UNITAI_CITY_DEFENSE:
			AI_cityDefenseMove();
			break;

		case UNITAI_CITY_COUNTER:
		case UNITAI_CITY_SPECIAL:
			AI_cityDefenseExtraMove();
			break;

		case UNITAI_EXPLORE:
			AI_exploreMove();
			break;

		case UNITAI_MISSIONARY:
			AI_missionaryMove();
			break;

		case UNITAI_PROPHET:
			AI_prophetMove();
			break;

		case UNITAI_ARTIST:
			AI_artistMove();
			break;

		case UNITAI_SCIENTIST:
			AI_scientistMove();
			break;

		case UNITAI_GENERAL:
			AI_generalMove();
			break;

		case UNITAI_MERCHANT:
			AI_merchantMove();
			break;

		case UNITAI_ENGINEER:
			AI_engineerMove();
			break;

		case UNITAI_GREAT_SPY:
			AI_greatSpyMove();
			break;

		case UNITAI_JESTER:
			AI_jesterMove();
			break;

		case UNITAI_SPY:
			AI_spyMove();
			break;

		case UNITAI_ICBM:
			AI_ICBMMove();
			break;

		case UNITAI_WORKER_SEA:
			AI_workerSeaMove();
			break;

		case UNITAI_ATTACK_SEA:
			if (isBarbarian()) {
				AI_barbAttackSeaMove();
			} else {
				AI_attackSeaMove();
			}
			break;

		case UNITAI_RESERVE_SEA:
			AI_reserveSeaMove();
			break;

		case UNITAI_ESCORT_SEA:
			AI_escortSeaMove();
			break;

		case UNITAI_EXPLORE_SEA:
			AI_exploreSeaMove();
			break;

		case UNITAI_ASSAULT_SEA:
			AI_assaultSeaMove();
			break;

		case UNITAI_SETTLER_SEA:
			AI_settlerSeaMove();
			break;

		case UNITAI_MISSIONARY_SEA:
			AI_missionarySeaMove();
			break;

		case UNITAI_SPY_SEA:
			AI_spySeaMove();
			break;

		case UNITAI_CARRIER_SEA:
			AI_carrierSeaMove();
			break;

		case UNITAI_MISSILE_CARRIER_SEA:
			AI_missileCarrierSeaMove();
			break;

		case UNITAI_PIRATE_SEA:
			AI_pirateSeaMove();
			break;

		case UNITAI_ATTACK_AIR:
			AI_attackAirMove();
			break;

		case UNITAI_DEFENSE_AIR:
			AI_defenseAirMove();
			break;

		case UNITAI_CARRIER_AIR:
			AI_carrierAirMove();
			break;

		case UNITAI_MISSILE_AIR:
			AI_missileAirMove();
			break;

		case UNITAI_ATTACK_CITY_LEMMING:
			AI_attackCityLemmingMove();
			break;

		default:
			FAssert(false);
			break;
		}
	}

	return false;
}


// Returns true if took an action or should wait to move later...
// K-Mod. I've basically rewriten this function.
// bFirst should be "true" if this is the first unit in the group to use this follow function.
// the point is that there are some calculations and checks in here which only depend on the group, not the unit
// so for efficiency, we should only check them once.
bool CvUnitAI::AI_follow(bool bFirst) {
	FAssert(getDomainType() != DOMAIN_AIR);

	if (AI_followBombard())
		return true;

	if (bFirst && getGroup()->getHeadUnitAI() == UNITAI_ATTACK_CITY) {
		// note: AI_stackAttackCity will check which of our units can attack when comparing stacks;
		// and it will issue the attack order using MOVE_DIRECT ATTACK, which will execute without waiting for the entire group to have movement points.
		if (AI_stackAttackCity()) // automatic threshold
			return true;
	}

	// I've changed attack-follow code so that it will only attack with a single unit, not the whole group.
	if (bFirst && AI_cityAttack(1, 65, 0, true))
		return true;
	if (bFirst) {
		bool bMoveGroup = false; // to large groups to leave some units behind.
		if (getGroup()->getNumUnits() >= 16) {
			int iCanMove = 0;
			CLLNode<IDInfo>* pEntityNode = getGroup()->headUnitNode();
			while (pEntityNode) {
				CvUnit* pLoopUnit = ::getUnit(pEntityNode->m_data);
				pEntityNode = getGroup()->nextUnitNode(pEntityNode);
				iCanMove += (pLoopUnit->canMove() ? 1 : 0);
			}
			bMoveGroup = 5 * iCanMove >= 4 * getGroup()->getNumUnits() || iCanMove >= 20; // if 4/5 of our group can still move.
		}
		if (AI_anyAttack(1, isEnemy(plot()->getTeam()) ? 65 : 70, 0, bMoveGroup ? 0 : 2, true, true))
			return true;
	}
	//

	if (isEnemy(plot()->getTeam())) {
		if (canPillage(plot())) {
			getGroup()->pushMission(MISSION_PILLAGE);
			return true;
		}
	}

	// K-Mod. AI_foundRange is bad AI. It doesn't always found when we want to, and it has the potential to found when we don't!
	// So I've replaced it.
	if (AI_foundFollow())
		return true;

	return false;
}

// K-Mod. This function has been completely rewritten to improve efficiency and intelligence.
void CvUnitAI::AI_upgrade() {
	PROFILE_FUNC();

	// We use this function for autoupgrading units so Humans can use it now
	// FAssert(!isHuman()); 
	FAssert(AI_getUnitAIType() != NO_UNITAI);

	if (!isReadyForUpgrade())
		return;

	const CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());
	const CvCivilizationInfo& kCivInfo = GC.getCivilizationInfo(kPlayer.getCivilizationType());
	UnitAITypes eUnitAI = AI_getUnitAIType();
	CvArea* pArea = area();

	int iBestValue = kPlayer.AI_unitValue(this, eUnitAI, pArea) * 100;
	UnitTypes eBestUnit = NO_UNIT;

	// Note: the original code did two passes, presumably for speed reasons.
	// In the first pass, they checked only units which were flagged with the right unitAI.
	// Then, only if no such units were found, they checked all other units.
	//
	// Now we cache the potential upgrades we can jump straight in
	std::vector<UnitClassTypes> aPotentialUnitClassTypes = GC.getUnitInfo(getUnitType()).getUpgradeUnitClassTypes();
	for (int iIndex = 0; iIndex < (int)aPotentialUnitClassTypes.size(); iIndex++) {
		UnitTypes eLoopUnit = (UnitTypes)kCivInfo.getCivilizationUnits(aPotentialUnitClassTypes[iIndex]);

		if (eLoopUnit != NO_UNIT) {
			int iValue = kPlayer.AI_unitValue(eLoopUnit, eUnitAI, pArea);
			// use a random factor. less than 100, so that the upgrade must be better than the current unit.
			iValue *= 80 + GC.getGameINLINE().getSorenRandNum(21, "AI Upgrade");

			// (believe it or not, AI_unitValue is faster than canUpgrade.)
			if (iValue > iBestValue && canUpgrade(eLoopUnit)) {
				iBestValue = iValue;
				eBestUnit = eLoopUnit;
			}
		}
	}

	if (eBestUnit != NO_UNIT) {
		// K-Mod. Ungroup the unit, so that we don't cause the whole group to miss their turn.
		CvUnit* pUpgradeUnit = upgrade(eBestUnit);
		doDelayedDeath();

		if (pUpgradeUnit != this) {
			CvSelectionGroup* pGroup = pUpgradeUnit->getGroup();
			if (pGroup->getHeadUnit() != pUpgradeUnit) {
				pUpgradeUnit->joinGroup(NULL);
				// indicate that the unit intends to rejoin the old group (although it might not actually do so...)
				pUpgradeUnit->getGroup()->AI_setMissionAI(MISSIONAI_GROUP, 0, pGroup->getHeadUnit());
			}
		}
	}
}


void CvUnitAI::AI_promote() {
	PROFILE_FUNC();

	// K-Mod. A quick check to see if we can rule out all promotions in one hit, before we go through them one by one.
	if (!isPromotionReady())
		return; // can't get any normal promotions. (see CvUnit::canPromote)

	int iBestValue = 0;
	PromotionTypes eBestPromotion = NO_PROMOTION;

	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++) {
		if (canPromote((PromotionTypes)iI, -1)) {
			int iValue = AI_promotionValue((PromotionTypes)iI);

			if (iValue > iBestValue) {
				iBestValue = iValue;
				eBestPromotion = ((PromotionTypes)iI);
			}
		}
	}

	if (eBestPromotion != NO_PROMOTION) {
		promote(eBestPromotion, -1);
		AI_promote();
	}
}


int CvUnitAI::AI_groupFirstVal() {
	switch (AI_getUnitAIType()) {
	case UNITAI_UNKNOWN:
	case UNITAI_ANIMAL:
		FAssert(false);
		break;

	case UNITAI_SETTLE:
		return 22;
		break;

		// Have slaves act before workers as they typically complete a build in one round
		//   so we don't want a worker to waste time staring something that the slave
		//   is going to complete
	case UNITAI_SLAVE:
		return 21;
		break;

	case UNITAI_GATHERER:
	case UNITAI_WORKER:
		return 20;
		break;

	case UNITAI_ATTACK:
	case UNITAI_BARBARIAN:
		if (collateralDamage() > 0) {
			return 15; // was 17
		} else if (withdrawalProbability() > 0) {
			return 14; // was 15
		} else {
			return 13;
		}
		break;

	case UNITAI_BARBARIAN_ATTACK_CITY:
	case UNITAI_ATTACK_CITY:
		if (bombardRate() > 0) {
			return 19;
		} else if (collateralDamage() > 0) {
			return 18;
		} else if (withdrawalProbability() > 0) {
			return 17; // was 16
		} else {
			return 16; // was 14
		}
		break;

	// We want to go after the general attacking units incase we fill our slave slots through their actions
	case UNITAI_SLAVER:
		return 12;
		break;

	case UNITAI_HUNTER:
		return 8;
		break;

	case UNITAI_COLLATERAL:
		return 7;
		break;

	case UNITAI_PILLAGE:
		return 12;
		break;

	case UNITAI_RESERVE:
		return 6;
		break;

	case UNITAI_COUNTER:
		return 5;
		break;

	case UNITAI_CITY_DEFENSE:
		return 3;
		break;

	case UNITAI_CITY_COUNTER:
		return 2;
		break;

	case UNITAI_CITY_SPECIAL:
		return 1;
		break;

	case UNITAI_PARADROP:
		return 4;
		break;

	case UNITAI_EXPLORE:
		return 8;
		break;

	case UNITAI_MISSIONARY:
		return 10;
		break;

	case UNITAI_BARBARIAN_LEADER:
	case UNITAI_PROPHET:
	case UNITAI_ARTIST:
	case UNITAI_SCIENTIST:
	case UNITAI_GENERAL:
	case UNITAI_MERCHANT:
	case UNITAI_ENGINEER:
	case UNITAI_GREAT_SPY: // K-Mod
	case UNITAI_JESTER:
		return 11;
		break;

	case UNITAI_SPY:
		return 9;
		break;

	case UNITAI_ICBM:
		break;

	case UNITAI_WORKER_SEA:
		return 8;
		break;

	case UNITAI_ATTACK_SEA:
		return 3;
		break;

	case UNITAI_RESERVE_SEA:
		return 2;
		break;

	case UNITAI_ESCORT_SEA:
		return 1;
		break;

	case UNITAI_EXPLORE_SEA:
		return 5;
		break;

	case UNITAI_ASSAULT_SEA:
		return 11;
		break;

	case UNITAI_SETTLER_SEA:
		return 9;
		break;

	case UNITAI_MISSIONARY_SEA:
		return 9;
		break;

	case UNITAI_SPY_SEA:
		return 10;
		break;

	case UNITAI_CARRIER_SEA:
		return 7;
		break;

	case UNITAI_MISSILE_CARRIER_SEA:
		return 6;
		break;

	case UNITAI_PIRATE_SEA:
		return 4;
		break;

	case UNITAI_ATTACK_AIR:
	case UNITAI_DEFENSE_AIR:
	case UNITAI_CARRIER_AIR:
	case UNITAI_MISSILE_AIR:
		break;

	case UNITAI_ATTACK_CITY_LEMMING:
		return 1;
		break;

	default:
		FAssert(false);
		break;
	}

	return 0;
}


int CvUnitAI::AI_groupSecondVal() {
	return ((getDomainType() == DOMAIN_AIR) ? airBaseCombatStr() : baseCombatStr());
}


// Returns attack odds out of 100 (the higher, the better...)
// Withdrawal odds included in returned value
int CvUnitAI::AI_attackOdds(const CvPlot* pPlot, bool bPotentialEnemy) const {
	PROFILE_FUNC();

	CvUnit* pDefender = pPlot->getBestDefender(NO_PLAYER, getOwnerINLINE(), this, !bPotentialEnemy, bPotentialEnemy);
	if (pDefender == NULL) {
		return 100;
	}

	// From Lead From Behind by UncutDragon
	if (GC.getLFBEnable() && GC.getLFBUseCombatOdds()) {
		// Combat odds are out of 1000 - we need odds out of 100
		int iOdds = (getCombatOdds(this, pDefender) + 5) / 10;
		iOdds += GET_PLAYER(getOwnerINLINE()).AI_getAttackOddsChange();

		return std::max(1, std::min(iOdds, 99));
	}

	int iOurStrength = ((getDomainType() == DOMAIN_AIR) ? airCurrCombatStr(NULL) : currCombatStr(NULL, NULL));
	int iOurFirepower = ((getDomainType() == DOMAIN_AIR) ? iOurStrength : currFirepower(NULL, NULL));

	if (iOurStrength == 0) {
		return 1;
	}

	int iTheirStrength = pDefender->currCombatStr(pPlot, this);
	int iTheirFirepower = pDefender->currFirepower(pPlot, this);


	FAssert((iOurStrength + iTheirStrength) > 0);
	FAssert((iOurFirepower + iTheirFirepower) > 0);

	int iBaseOdds = (100 * iOurStrength) / (iOurStrength + iTheirStrength);
	if (iBaseOdds == 0) {
		return 1;
	}

	int iStrengthFactor = ((iOurFirepower + iTheirFirepower + 1) / 2);

	int iDamageToUs = std::max(1, ((GC.getCOMBAT_DAMAGE() * (iTheirFirepower + iStrengthFactor)) / (iOurFirepower + iStrengthFactor)));
	int iDamageToThem = std::max(1, ((GC.getCOMBAT_DAMAGE() * (iOurFirepower + iStrengthFactor)) / (iTheirFirepower + iStrengthFactor)));

	int iHitLimitThem = pDefender->maxHitPoints() - combatLimit();

	int iNeededRoundsUs = (std::max(0, pDefender->currHitPoints() - iHitLimitThem) + iDamageToThem - 1) / iDamageToThem;
	int iNeededRoundsThem = (std::max(0, currHitPoints()) + iDamageToUs - 1) / iDamageToUs;

	if (getDomainType() != DOMAIN_AIR) {
		if (!pDefender->immuneToFirstStrikes()) {
			iNeededRoundsUs -= ((iBaseOdds * firstStrikes()) + ((iBaseOdds * chanceFirstStrikes()) / 2)) / 100;
		}
		if (!immuneToFirstStrikes()) {
			iNeededRoundsThem -= (((100 - iBaseOdds) * pDefender->firstStrikes()) + (((100 - iBaseOdds) * pDefender->chanceFirstStrikes()) / 2)) / 100;
		}
		iNeededRoundsUs = std::max(1, iNeededRoundsUs);
		iNeededRoundsThem = std::max(1, iNeededRoundsThem);
	}

	int iRoundsDiff = iNeededRoundsUs - iNeededRoundsThem;
	if (iRoundsDiff > 0) {
		iTheirStrength *= (1 + iRoundsDiff);
	} else {
		iOurStrength *= (1 - iRoundsDiff);
	}

	int iOdds = (((iOurStrength * 100) / (iOurStrength + iTheirStrength)));
	iOdds += ((100 - iOdds) * withdrawalProbability()) / 100;
	iOdds += GET_PLAYER(getOwnerINLINE()).AI_getAttackOddsChange();

	return range(iOdds, 1, 99);
}


// Returns true if the unit found a build for this city...
bool CvUnitAI::AI_bestCityBuild(CvCity* pCity, CvPlot** ppBestPlot, BuildTypes* peBestBuild, CvPlot* pIgnorePlot, CvUnit* pUnit) {
	PROFILE_FUNC();

	int iBestValue = 0;
	BuildTypes eBestBuild = NO_BUILD;
	CvPlot* pBestPlot = NULL;

	// K-Mod. hack: For the AI, I want to use the standard pathfinder, CvUnit::generatePath.
	// but this function is also used to give action recommendations for the player
	// - and for that I do not want to disrupt the standard pathfinder. (because I'm paranoid about OOS bugs.)
	KmodPathFinder alt_finder;
	KmodPathFinder& pathFinder = getGroup()->AI_isControlled() ? CvSelectionGroup::path_finder : alt_finder;
	if (getGroup()->AI_isControlled()) {
		// standard settings. cf. CvUnit::generatePath
		pathFinder.SetSettings(getGroup(), 0);
	} else {
		// like I said - this is only for action recommendations. It can be rough.
		pathFinder.SetSettings(getGroup(), 0, 5, GC.getMOVE_DENOMINATOR());
	}

	for (int iPass = 0; iPass < 2; iPass++) {
		for (int iI = 0; iI < pCity->getNumCityPlots(); iI++) {
			CvPlot* pLoopPlot = plotCity(pCity->getX_INLINE(), pCity->getY_INLINE(), iI);

			if (pLoopPlot && pLoopPlot != pIgnorePlot && pLoopPlot->getWorkingCity() == pCity && AI_plotValid(pLoopPlot)) // K-Mod
			{
				if (pLoopPlot->getImprovementType() == NO_IMPROVEMENT ||
					!GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_SAFE_AUTOMATION) ||
					pLoopPlot->getImprovementType() == GC.getDefineINT("RUINS_IMPROVEMENT")) {
					int iValue = pCity->AI_getBestBuildValue(iI);

					if (iValue > iBestValue) {
						BuildTypes eBuild = pCity->AI_getBestBuild(iI);
						FAssertMsg(eBuild < GC.getNumBuildInfos(), "Invalid Build");

						if (eBuild != NO_BUILD && canBuild(pLoopPlot, eBuild)) // K-Mod
						{
							if (0 == iPass) {
								iBestValue = iValue;
								pBestPlot = pLoopPlot;
								eBestBuild = eBuild;
							} else //if (canBuild(pLoopPlot, eBuild))
							{
								if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
									// K-Mod. basically the same thing, but using pathFinder.
									if (pathFinder.GeneratePath(pLoopPlot)) {
										int iPathTurns = pathFinder.GetPathTurns() + (pathFinder.GetFinalMoves() == 0 ? 1 : 0);
										int iMaxWorkers = iPathTurns > 1 ? 1 : AI_calculatePlotWorkersNeeded(pLoopPlot, eBuild);
										if (pUnit && pUnit->plot()->isCity() && iPathTurns == 1)
											iMaxWorkers += 10;
										if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BUILD, getGroup()) < iMaxWorkers) {
											//XXX this could be improved greatly by
											//looking at the real build time and other factors
											//when deciding whether to stack.
											iValue /= iPathTurns;

											iBestValue = iValue;
											pBestPlot = pLoopPlot;
											eBestBuild = eBuild;
										}
									}
								}
							}
						}
					}
				}
			}
		}

		if (0 == iPass) {
			if (eBestBuild != NO_BUILD) {
				FAssert(pBestPlot != NULL);
				// K-Mod. basically the same thing, but using pathFinder.
				if (pathFinder.GeneratePath(pBestPlot)) {
					int iPathTurns = pathFinder.GetPathTurns() + (pathFinder.GetFinalMoves() == 0 ? 1 : 0);
					int iMaxWorkers = iPathTurns > 1 ? 1 : AI_calculatePlotWorkersNeeded(pBestPlot, eBestBuild);
					if (pUnit && pUnit->plot()->isCity() && iPathTurns == 1)
						iMaxWorkers += 10;
					int iWorkerCount = GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pBestPlot, MISSIONAI_BUILD, getGroup());
					if (iWorkerCount < iMaxWorkers) {
						//Good to go.
						break;
					}
				}
				eBestBuild = NO_BUILD;
				iBestValue = 0;
			}
		}
	}

	if (NO_BUILD != eBestBuild) {
		FAssert(NULL != pBestPlot);
		if (ppBestPlot != NULL) {
			*ppBestPlot = pBestPlot;
		}
		if (peBestBuild != NULL) {
			*peBestBuild = eBestBuild;
		}
	}


	return (NO_BUILD != eBestBuild);
}


bool CvUnitAI::AI_isCityAIType() const {
	return ((AI_getUnitAIType() == UNITAI_CITY_DEFENSE) ||
		(AI_getUnitAIType() == UNITAI_CITY_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_CITY_SPECIAL) ||
		(AI_getUnitAIType() == UNITAI_RESERVE));
}


int CvUnitAI::AI_getBirthmark() const {
	return m_iBirthmark;
}


void CvUnitAI::AI_setBirthmark(int iNewValue) {
	m_iBirthmark = iNewValue;
	if (AI_getUnitAIType() == UNITAI_EXPLORE_SEA) {
		if (GC.getGame().circumnavigationAvailable()) {
			m_iBirthmark -= m_iBirthmark % 4;
			int iExplorerCount = GET_PLAYER(getOwnerINLINE()).AI_getNumAIUnits(UNITAI_EXPLORE_SEA);
			iExplorerCount += getOwnerINLINE() % 4;
			if (GC.getMap().isWrapX()) {
				if ((iExplorerCount % 2) == 1) {
					m_iBirthmark += 1;
				}
			}
			if (GC.getMap().isWrapY()) {
				if (!GC.getMap().isWrapX()) {
					iExplorerCount *= 2;
				}

				if (((iExplorerCount >> 1) % 2) == 1) {
					m_iBirthmark += 2;
				}
			}
		}
	}
}


UnitAITypes CvUnitAI::AI_getUnitAIType() const {
	return m_eUnitAIType;
}


// XXX make sure this gets called...
void CvUnitAI::AI_setUnitAIType(UnitAITypes eNewValue) {
	FAssertMsg(eNewValue != NO_UNITAI, "NewValue is not assigned a valid value");

	if (AI_getUnitAIType() != eNewValue) {
		area()->changeNumAIUnits(getOwnerINLINE(), AI_getUnitAIType(), -1);
		GET_PLAYER(getOwnerINLINE()).AI_changeNumAIUnits(AI_getUnitAIType(), -1);

		m_eUnitAIType = eNewValue;

		area()->changeNumAIUnits(getOwnerINLINE(), AI_getUnitAIType(), 1);
		GET_PLAYER(getOwnerINLINE()).AI_changeNumAIUnits(AI_getUnitAIType(), 1);

		joinGroup(NULL);
	}
}

int CvUnitAI::AI_sacrificeValue(const CvPlot* pPlot) const {
	int iCollateralDamageValue = 0;
	if (pPlot != NULL) {
		int iPossibleTargets = std::min((pPlot->getNumVisibleEnemyDefenders(this) - 1), collateralDamageMaxUnits());

		if (iPossibleTargets > 0) {
			iCollateralDamageValue = collateralDamage();
			iCollateralDamageValue += std::max(0, iCollateralDamageValue - 100);
			iCollateralDamageValue *= iPossibleTargets;
			iCollateralDamageValue /= 5;
		}
	}

	long iValue = 0; // K-Mod. (the int will overflow)
	if (getDomainType() == DOMAIN_AIR) {
		iValue = 128 * (100 + currInterceptionProbability());
		if (m_pUnitInfo->getNukeRange() != -1) {
			iValue += 25000;
		}
		iValue /= m_pUnitInfo->getProductionCost() > 0 ? m_pUnitInfo->getProductionCost() : 180; // K-Mod
		iValue *= (maxHitPoints() - getDamage());
		iValue /= 100;
	} else {
		iValue = 128 * (currEffectiveStr(pPlot, ((pPlot == NULL) ? NULL : this)));
		iValue *= (100 + iCollateralDamageValue);
		iValue /= (100 + cityDefenseModifier());
		iValue *= (100 + withdrawalProbability());
		iValue /= 100; // K-Mod

		// Experience and medics now better handled in LFB
		if (!GC.getLFBEnable()) {
			iValue *= 10; // K-Mod
			iValue /= (10 + getExperience()); // K-Mod - moved from out of the if.
			iValue *= 10;
			iValue /= (10 + getSameTileHeal() + getAdjacentTileHeal());
		}

		// Value units which can't kill units later, also combat limits mean higher survival odds
		iValue *= 100 + 5 * (2 * firstStrikes() + chanceFirstStrikes()) / 2 + (immuneToFirstStrikes() ? 20 : 0) + (combatLimit() < 100 ? 20 : 0);
		iValue /= 100;

		iValue /= m_pUnitInfo->getProductionCost() > 0 ? m_pUnitInfo->getProductionCost() : 180; // K-Mod
	}

	if (GC.getLFBEnable()) {
		// K-Mod. cf. LFBgetValueAdjustedOdds
		iValue *= 1000;
		iValue /= std::max(1, 1000 + 1000 * LFBgetRelativeValueRating() * GC.getLFBAdjustNumerator() / GC.getLFBAdjustDenominator());
		// roughly speaking, the second part of the denominator is the odds adjustment from LFBgetValueAdjustedOdds.
		// It might be more natural to subtract it from the numerator, but then we can't guarantee a positive value.
	}

	return std::min((long)MAX_INT, iValue); // K-Mod
}

// Protected Functions...

// K-Mod - test if we should declare war before moving to the target plot.
// (originally, DOW were made inside the unit movement mechanics. To me, that seems like a really dumb idea.)
bool CvUnitAI::AI_considerDOW(CvPlot* pPlot) {
	CvTeamAI& kOurTeam = GET_TEAM(getTeam());
	TeamTypes ePlotTeam = pPlot->getTeam();

	// Note: We might be a transport ship which ignores borders, but with escorts and cargo who don't ignore borders.
	// So, we should check that the whole group can enter the borders. (There are be faster ways to check, but this is good enough.)
	// If it's an amphibious landing, lets just assume that our cargo will need a DoW!
	if (!getGroup()->canEnterArea(ePlotTeam, pPlot->area(), true) || getGroup()->isAmphibPlot(pPlot)) {
		if (ePlotTeam != NO_TEAM && kOurTeam.AI_isSneakAttackReady(ePlotTeam)) {
			if (kOurTeam.canDeclareWar(ePlotTeam)) {
				if (gUnitLogLevel > 0) logBBAI("    %S declares war on %S with AI_considerDOW (%S - %S).", kOurTeam.getName().GetCString(), GET_TEAM(ePlotTeam).getName().GetCString(), getName(0).GetCString(), GC.getUnitAIInfo(AI_getUnitAIType()).getDescription());
				kOurTeam.declareWar(ePlotTeam, true, NO_WARPLAN);
				getPathFinder().Reset();
				return true;
			}
		}
	}
	return false;
}

// AI_considerPathDOW checks each plot on the path until the end of the turn.
// Sometimes the end plot is in friendly territory, but we need to declare war to actually get there.
// This situation is very rare, but unfortunately we have to check for it every time
// - because otherwise, when it happens, the AI will just get stuck.
bool CvUnitAI::AI_considerPathDOW(CvPlot* pPlot, int iFlags) {
	PROFILE_FUNC();

	if (!(iFlags & MOVE_DECLARE_WAR))
		return false;

	if (!generatePath(pPlot, iFlags, true)) {
		FAssertMsg(false, "AI_considerPathDOW didn't find a path.");
		return false;
	}

	bool bDOW = false;
	FAStarNode* pNode = getPathFinder().GetEndNode(); // TODO: rewrite so that GetEndNode isn't used.
	while (!bDOW && pNode) {
		// we need to check DOW even for moves several turns away - otherwise the actual move mission may fail to find a path.
		// however, I would consider it irresponsible to call this function for multi-move missions.
		// (note: amphibious landings may say 2 turns, even though it is really only 1...)
		FAssert(pNode->m_iData2 <= 1 || (pNode->m_iData2 == 2 && getGroup()->isAmphibPlot(GC.getMapINLINE().plotSorenINLINE(pNode->m_iX, pNode->m_iY))));
		bDOW = AI_considerDOW(GC.getMapINLINE().plotSorenINLINE(pNode->m_iX, pNode->m_iY));
		pNode = pNode->m_pParent;
	}

	return bDOW;
}

void CvUnitAI::AI_animalMove() {
	PROFILE_FUNC();

	if (GC.getGameINLINE().getSorenRandNum(100, "Animal Attack") < GC.getHandicapInfo(GC.getGameINLINE().getHandicapType()).getAnimalAttackProb()) {
		if (AI_anyAttack(1, 0)) {
			return;
		}
	}

	if (AI_heal()) {
		return;
	}

	if (AI_patrol()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_settleMove() {
	PROFILE_FUNC();

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod
	int iMoveFlags = MOVE_NO_ENEMY_TERRITORY; // K-Mod

	if (kOwner.getNumCities() == 0) {
		// RevDCM TODO: What makes sense for rebels here?
		if (canFound(plot())) {
			if (gUnitLogLevel >= 2) {
				logBBAI("    Settler founding in place due to no cities");
			}

			getGroup()->pushMission(MISSION_FOUND);
			return;
		}
	}

	if (kOwner.AI_getAnyPlotDanger(plot())) {
		if (!getGroup()->canDefend() || 100 * kOwner.AI_localAttackStrength(plot(), NO_TEAM) > 80 * getGroup()->AI_sumStrength(0)) {
			// flee
			joinGroup(NULL);
			if (AI_retreatToCity()) {
				return;
			}
		}
	}

	int iAreaBestFoundValue = 0;
	int iOtherBestFoundValue = 0;

	for (int iI = 0; iI < kOwner.AI_getNumCitySites(); iI++) {
		CvPlot* pCitySitePlot = kOwner.AI_getCitySite(iI);
		if ((pCitySitePlot->getArea() == getArea() || canMoveAllTerrain()) && generatePath(pCitySitePlot, iMoveFlags, true)) {
			if (plot() == pCitySitePlot) {
				if (canFound(plot())) {
					if (gUnitLogLevel >= 2) {
						logBBAI("    Settler founding in place since it's at a city site %d, %d", getX_INLINE(), getY_INLINE());
					}

					getGroup()->pushMission(MISSION_FOUND);
					return;
				}
			}
			iAreaBestFoundValue = std::max(iAreaBestFoundValue, pCitySitePlot->getFoundValue(getOwnerINLINE()));

		} else {
			iOtherBestFoundValue = std::max(iOtherBestFoundValue, pCitySitePlot->getFoundValue(getOwnerINLINE()));
		}
	}

	// No new settling of colonies when AI is in financial trouble
	if (plot()->isCity() && (plot()->getOwnerINLINE() == getOwnerINLINE())) {
		if (kOwner.AI_isFinancialTrouble()) {
			iOtherBestFoundValue = 0;
		}
	}


	if ((iAreaBestFoundValue == 0) && (iOtherBestFoundValue == 0)) {
		if ((GC.getGame().getGameTurn() - getGameTurnCreated()) > 20) {
			if (NULL != getTransportUnit()) {
				getTransportUnit()->unloadAll();
			}

			if (NULL == getTransportUnit()) {
				if (kOwner.AI_unitTargetMissionAIs(getGroup()->getHeadUnit(), MISSIONAI_PICKUP) == 0) {
					//may seem wasteful, but settlers confuse the AI.
					scrap();
					return;
				}
			}
		}
	}

	if ((iOtherBestFoundValue * 100) > (iAreaBestFoundValue * 110)) {
		if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, NO_UNITAI, -1, -1, -1, 0, iMoveFlags)) {
				return;
			}
		}
	}

	if (plot()->isCity() && (plot()->getOwnerINLINE() == getOwnerINLINE())) {
		if (kOwner.AI_getAnyPlotDanger(plot())
			&& (GC.getGameINLINE().getMaxCityElimination() > 0)) {
			if (getGroup()->getNumUnits() < 3) {
				getGroup()->pushMission(MISSION_SKIP);
				return;
			}
		}
	}

	if (iAreaBestFoundValue > 0) {
		if (AI_found(iMoveFlags)) {
			return;
		}
	}

	if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
		if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, NO_UNITAI, -1, -1, -1, 0, iMoveFlags)) {
			return;
		}

		// BBAI TODO: Go to a good city (like one with a transport) ...
	}

	// K-Mod: sometimes an unescorted settlers will join up with an escort mid-mission..
	{
		int iLoop;
		for (CvSelectionGroup* pLoopSelectionGroup = kOwner.firstSelectionGroup(&iLoop); pLoopSelectionGroup; pLoopSelectionGroup = kOwner.nextSelectionGroup(&iLoop)) {
			if (pLoopSelectionGroup != getGroup()) {
				if (pLoopSelectionGroup->AI_getMissionAIUnit() == this && pLoopSelectionGroup->AI_getMissionAIType() == MISSIONAI_GROUP) {
					int iPathTurns = MAX_INT;

					generatePath(pLoopSelectionGroup->plot(), iMoveFlags, true, &iPathTurns, 2);
					if (iPathTurns <= 2) {
						CvPlot* pEndTurnPlot = getPathEndTurnPlot();
						if (atPlot(pEndTurnPlot)) {
							pLoopSelectionGroup->mergeIntoGroup(getGroup());
							FAssert(getGroup()->getNumUnits() > 1);
							FAssert(getGroup()->getHeadUnitAI() == UNITAI_SETTLE);
						} else {
							// if we were on our way to a site, keep the current mission plot.
							if (getGroup()->AI_getMissionAIType() == MISSIONAI_FOUND && getGroup()->AI_getMissionAIPlot() != NULL) {
								getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX_INLINE(), pEndTurnPlot->getY_INLINE(), iMoveFlags, false, false, MISSIONAI_FOUND, getGroup()->AI_getMissionAIPlot());
							} else {
								getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX_INLINE(), pEndTurnPlot->getY_INLINE(), iMoveFlags, false, false, MISSIONAI_GROUP, 0, pLoopSelectionGroup->getHeadUnit());
							}
						}
						return;
					}
				}
			}
		}
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_workerMove() {
	PROFILE_FUNC();

	bool bCanRoute = canBuildRoute();
	bool bNextCity = false;

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	// XXX could be trouble...
	if (plot()->getOwnerINLINE() != getOwnerINLINE()) {
		if (AI_retreatToCity()) {
			return;
		}
	}

	if (!isHuman()) {
		if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, 2, -1, -1, 0, MOVE_SAFE_TERRITORY)) {
				return;
			}
		}
	}

	if (!(getGroup()->canDefend())) {
		if (kOwner.AI_isPlotThreatened(plot(), 2)) {
			if (AI_retreatToCity()) // XXX maybe not do this??? could be working productively somewhere else...
			{
				return;
			}
		}
	}

	if (bCanRoute) {
		if (plot()->getOwnerINLINE() == getOwnerINLINE()) // XXX team???
		{
			BonusTypes eNonObsoleteBonus = plot()->getNonObsoleteBonusType(getTeam());
			if (NO_BONUS != eNonObsoleteBonus) {
				if (!(plot()->isConnectedToCapital())) {
					ImprovementTypes eImprovement = plot()->getImprovementType();
					if (kOwner.doesImprovementConnectBonus(eImprovement, eNonObsoleteBonus)) {
						if (AI_connectPlot(plot())) {
							return;
						}
					}
				}
			}
		}
	}

	if (AI_improveBonus()) // K-Mod
	{
		return;
	}

	if (bCanRoute && !isBarbarian()) {
		if (AI_connectCity()) {
			return;
		}
	}

	CvCity* pCity = NULL;
	if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
		pCity = plot()->getPlotCity();
		if (pCity == NULL) {
			pCity = plot()->getWorkingCity();
		}
	}

	if (pCity != NULL) {
		// K-Mod. Note: this worker is currently at pCity, and so we're probably counted in AI_getWorkersHave.
		if (pCity->AI_getWorkersNeeded() > 0 && (plot()->isCity() || pCity->AI_getWorkersHave() - 1 <= (1 + pCity->AI_getWorkersNeeded() * 2) / 3)) {
			if (AI_improveCity(pCity)) {
				return;
			}
		}
	}

	bool bBuildFort = false;

	if (GC.getGame().getSorenRandNum(5, "AI Worker build Fort with Priority")) {
		bool bCanal = false; // K-Mod. The current AI for canals doesn't work anyway; so lets skip it to save time.
		bool bAirbase = false;
		bAirbase = (kOwner.AI_totalUnitAIs(UNITAI_PARADROP) || kOwner.AI_totalUnitAIs(UNITAI_ATTACK_AIR) || kOwner.AI_totalUnitAIs(UNITAI_MISSILE_AIR));

		if (bCanal || bAirbase) {
			if (AI_fortTerritory(bCanal, bAirbase)) {
				return;
			}
		}
		bBuildFort = true;
	}


	if (bCanRoute && isBarbarian()) {
		if (AI_connectCity()) {
			return;
		}
	}

	if ((pCity == NULL) || (pCity->AI_getWorkersNeeded() == 0) || ((pCity->AI_getWorkersHave() > (pCity->AI_getWorkersNeeded() + 1)))) {
		if (AI_nextCityToImprove(pCity)) {
			return;
		}

		bNextCity = true;
	}

	if (pCity != NULL) {
		if (AI_improveCity(pCity)) {
			return;
		}
	}
	// K-Mod. (moved from higher up)
	if (AI_improveLocalPlot(2, pCity))
		return;
	//

	if (!bNextCity) {
		if (AI_nextCityToImprove(pCity)) {
			return;
		}
	}

	if (bCanRoute) {
		if (AI_routeTerritory(true)) {
			return;
		}

		if (AI_connectBonus(false)) {
			return;
		}

		if (AI_routeCity()) {
			return;
		}
	}

	if (AI_irrigateTerritory()) {
		return;
	}

	if (!bBuildFort) {
		bool bCanal = false; // K-Mod. The current AI for canals doesn't work anyway; so lets skip it to save time.
		bool bAirbase = false;
		bAirbase = (kOwner.AI_totalUnitAIs(UNITAI_PARADROP) || kOwner.AI_totalUnitAIs(UNITAI_ATTACK_AIR) || kOwner.AI_totalUnitAIs(UNITAI_MISSILE_AIR));

		if (bCanal || bAirbase) {
			if (AI_fortTerritory(bCanal, bAirbase)) {
				return;
			}
		}
	}

	if (bCanRoute) {
		if (AI_routeTerritory()) {
			return;
		}
	}

	if (!isHuman() || (isAutomated() && GET_TEAM(getTeam()).getAtWarCount(true) == 0)) {
		if (!isHuman() || (getGameTurnCreated() < GC.getGame().getGameTurn())) {
			if (AI_nextCityToImproveAirlift()) {
				return;
			}
		}
		if (!isHuman()) {
			// Fill up boats which already have workers
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_WORKER, -1, -1, -1, -1, MOVE_SAFE_TERRITORY)) {
				return;
			}

			// Avoid filling a galley which has just a settler in it, reduce chances for other ships
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, NO_UNITAI, -1, 2, -1, -1, MOVE_SAFE_TERRITORY)) {
				return;
			}
		}
	}

	if (AI_improveLocalPlot(3, NULL)) {
		return;
	}

	if (!(isHuman()) && (AI_getUnitAIType() == UNITAI_WORKER)) {
		if (GC.getGameINLINE().getElapsedGameTurns() > GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getResearchPercent() / 6) {
			if (kOwner.AI_totalUnitAIs(UNITAI_WORKER) > std::max(GC.getWorldInfo(GC.getMapINLINE().getWorldSize()).getTargetNumCities(), kOwner.getNumCities() * 3 / 2) &&
				area()->getNumAIUnits(getOwnerINLINE(), UNITAI_WORKER) > kOwner.AI_neededWorkers(area()) * 3 / 2) {
				if (kOwner.calculateUnitCost() > 0) {
					scrap();
					return;
				}
			}
		}
	}

	if (AI_retreatToCity(false, true)) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_slaveMove() {
	PROFILE_FUNC();

	bool bCanRoute = canBuildRoute();
	bool bNextCity = false;

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	// XXX could be trouble...
	if (plot()->getOwnerINLINE() != getOwnerINLINE()) {
		if (AI_retreatToCity()) {
			return;
		}
	}

	if (!isHuman()) {
		if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, 2, -1, -1, 0, MOVE_SAFE_TERRITORY)) {
				return;
			}
		}
	}

	if (!(getGroup()->canDefend())) {
		if (kOwner.AI_isPlotThreatened(plot(), 2)) {
			if (AI_retreatToCity()) // XXX maybe not do this??? could be working productively somewhere else...
			{
				return;
			}
		}
	}

	// If the slave is standing on an improvement working a trade bonus that the player doesn't have then create a route for it
	if (bCanRoute) {
		if (plot()->getOwnerINLINE() == getOwnerINLINE()) // XXX team???
		{
			BonusTypes eNonObsoleteBonus = plot()->getNonObsoleteBonusType(getTeam());
			if (NO_BONUS != eNonObsoleteBonus) {
				if (!plot()->isConnectedToCapital()) {
					ImprovementTypes eImprovement = plot()->getImprovementType();
					if (kOwner.doesImprovementConnectBonus(eImprovement, eNonObsoleteBonus)) {
						if (kOwner.getNumTradeableBonuses(eNonObsoleteBonus) == 0) {
							if (AI_connectPlot(plot())) {
								return;
							}
						}
					}
				}
			}
		}
	}

	// Don't do small value improvements
	if (AI_improveBonus(GC.getUnitInfo(m_eUnitType).isSingleBuild()))
		return;

	// Join the city as a resident slave if there is room
	if (AI_join(MAX_INT, true))
		return;

	// Creates improvement for bonuses with a standard value
	if (AI_improveBonus())
		return;

	if (bCanRoute && !isBarbarian()) {
		if (AI_connectCity()) {
			return;
		}
	}

	CvCity* pCity = NULL;
	if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
		pCity = plot()->getPlotCity();
		if (pCity == NULL) {
			pCity = plot()->getWorkingCity();
		}
	}

	if (pCity != NULL) {
		// K-Mod. Note: this worker is currently at pCity, and so we're probably counted in AI_getWorkersHave.
		if (pCity->AI_getWorkersNeeded() > 0 && (plot()->isCity() || pCity->AI_getWorkersHave() - 1 <= (1 + pCity->AI_getWorkersNeeded() * 2) / 3)) {
			if (AI_improveCity(pCity)) {
				return;
			}
		}
	}

	bool bBuildFort = false;

	if (GC.getGame().getSorenRandNum(5, "AI Worker build Fort with Priority")) {
		bool bCanal = false; // K-Mod. The current AI for canals doesn't work anyway; so lets skip it to save time.
		bool bAirbase = false;
		bAirbase = (kOwner.AI_totalUnitAIs(UNITAI_PARADROP) || kOwner.AI_totalUnitAIs(UNITAI_ATTACK_AIR) || kOwner.AI_totalUnitAIs(UNITAI_MISSILE_AIR));

		if (bCanal || bAirbase) {
			if (AI_fortTerritory(bCanal, bAirbase)) {
				return;
			}
		}
		bBuildFort = true;
	}


	if (bCanRoute && isBarbarian()) {
		if (AI_connectCity()) {
			return;
		}
	}

	if (pCity == NULL || pCity->AI_getWorkersNeeded() == 0 || pCity->AI_getWorkersHave() > (pCity->AI_getWorkersNeeded() + 1)) {
		if (AI_nextCityToImprove(pCity)) {
			return;
		}

		bNextCity = true;
	}

	if (pCity != NULL) {
		if (AI_improveCity(pCity)) {
			return;
		}
	}
	if (AI_improveLocalPlot(2, pCity))
		return;
	//

	if (!bNextCity) {
		if (AI_nextCityToImprove(pCity)) {
			return;
		}
	}

	if (bCanRoute) {
		if (AI_routeTerritory(true)) {
			return;
		}

		if (AI_connectBonus(false)) {
			return;
		}

		if (AI_routeCity()) {
			return;
		}
	}

	if (AI_irrigateTerritory()) {
		return;
	}

	if (!bBuildFort) {
		bool bCanal = false;
		bool bAirbase = false;
		bAirbase = (kOwner.AI_totalUnitAIs(UNITAI_PARADROP) || kOwner.AI_totalUnitAIs(UNITAI_ATTACK_AIR) || kOwner.AI_totalUnitAIs(UNITAI_MISSILE_AIR));

		if (bCanal || bAirbase) {
			if (AI_fortTerritory(bCanal, bAirbase)) {
				return;
			}
		}
	}

	if (bCanRoute) {
		if (AI_routeTerritory()) {
			return;
		}
	}

	if (!isHuman() || (isAutomated() && GET_TEAM(getTeam()).getAtWarCount(true) == 0)) {
		if (!isHuman() || (getGameTurnCreated() < GC.getGame().getGameTurn())) {
			if (AI_nextCityToImproveAirlift()) {
				return;
			}
		}
		if (!isHuman()) {
			// Fill up boats which already have workers
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_WORKER, -1, -1, -1, -1, MOVE_SAFE_TERRITORY)) {
				return;
			}
			// Avoid filling a galley which has just a settler in it, reduce chances for other ships
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, NO_UNITAI, -1, 2, -1, -1, MOVE_SAFE_TERRITORY)) {
				return;
			}
		}
	}

	if (AI_improveLocalPlot(3, NULL))
		return;

	if (AI_retreatToCity(false, true))
		return;

	if (AI_handleStranded())
		return;

	if (AI_safety())
		return;

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_slaverMove() {

	PROFILE_FUNC();

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	bool bDanger = (kOwner.AI_getAnyPlotDanger(plot(), 3));

	// If we are in a safe city so offload slaves or heal up
	if (plot()->isCity()) {
		// Our city
		if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
			if (AI_sellSlaves()) {
				return;
			}
		}

		// Team city
		if (plot()->getTeam() == getTeam()) {
			if (AI_heal()) {
				return;
			}
		}
	}

	// Offload any slaves if we need to
	if (getSlaveCountTotal() > 0) {
		if (AI_sellSlaves()) {
			return;
		}
	}

	// A hunting we will go ... look for some easy pickings close by
	if (AI_enslave(3, 100)) {
		return;
	}

	// Take a stim pack
	if (!bDanger) {
		if (AI_heal(30, 1)) {
			return;
		}
	}

	// Shadow any group that is looking for trouble in enemy territory
	if (plot()->getOwnerINLINE() != NO_PLAYER && GET_TEAM(getTeam()).isAtWar(GET_PLAYER(plot()->getOwnerINLINE()).getTeam()) && AI_shadow(UNITAI_ATTACK, 1, -1, false, true, 1)) {
		return;
	}

	// Allow 2 slavers to shadow city breakers
	if (plot()->getOwnerINLINE() != NO_PLAYER && GET_TEAM(getTeam()).isAtWar(GET_PLAYER(plot()->getOwnerINLINE()).getTeam()) && AI_shadow(UNITAI_ATTACK_CITY, 2, -1, false, true, 1)) {
		return;
	}

	// Lets get a bit more adventurous ... but we are cowardly slavers so lets not get too ambitious
	if (AI_enslave(3, 90)) {
		return;
	}

	// Lets add a bit more risk, but not too much
	if (AI_enslave(3, 80)) {
		return;
	}

	if (AI_travelToUpgradeCity()) {
		return;
	}

	// Nothing really to do so lets wander around and hope to stumble on something
	if (AI_slaverExplore(3)) {
		return;
	}

	if (AI_handleStranded()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	// Boring!!
	getGroup()->pushMission(MISSION_SKIP);
	return;
}

// Returns true if a mission was pushed...
// Searches the area defined by iRange for targets to enslave
// Priority is units with no defence, followed by the defender with the best odds of victory under the threshold
// If nothing to enslave then do nothing
bool CvUnitAI::AI_enslave(int iRange, int iOddsThreshold) {
	PROFILE_FUNC();

	if (getMaxSlaves() <= getSlaveCountTotal())
		return false;

	iRange = AI_searchRange(iRange);

	bool bPoachTarget = false;
	bool bPoachTemp = false;
	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	for (int iDX = -iRange; iDX <= iRange; iDX++) {
		for (int iDY = -iRange; iDY <= iRange; iDY++) {
			bool bFoundTarget = false;
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot == NULL || pLoopPlot == plot() || !AI_plotValid(pLoopPlot) || pLoopPlot->isCity())
				continue;

			int iPathTurns;
			if (!generatePath(pLoopPlot, 0, true, &iPathTurns) || (iPathTurns > iRange))
				continue;

			// Use weighting to pick the easiest target
			int iValueWeighting = 100;
			if (pLoopPlot->isVisibleEnemyDefender(this)) {
				int iWeightedOdds = AI_getWeightedOdds(pLoopPlot, true);
				if (iWeightedOdds < iOddsThreshold)
					continue;

				iValueWeighting += iWeightedOdds - iOddsThreshold;
				bFoundTarget = true;
			} else {
				// No defender so see if there are any units that provide a slave
				int iFreeSlaves = getMaxSlaves() - getSlaveCountTotal();

				CLLNode<IDInfo>* pUnitNode = pLoopPlot->headUnitNode();
				while (pUnitNode != NULL && iFreeSlaves) {
					CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
					pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

					if (pLoopUnit->getTeam() == getTeam())
						continue;

					if (pLoopUnit->getSlaveSpecialistType() != NO_SPECIALIST) {
						bPoachTemp = true;
						iFreeSlaves--;
						iValueWeighting *= atWar(getTeam(), pLoopUnit->getTeam()) ? 3 : 2;
						bFoundTarget = true;
					}
				}
			}

			if (pLoopPlot->isRevealedGoody(getTeam())) {
				iValueWeighting += 100000;
				bFoundTarget = true;
			}

			if (!bFoundTarget)
				continue;

			int iValue = (1 + GC.getGameINLINE().getSorenRandNum(10000, "AI Enslave"));
			iValue *= iValueWeighting;
			iValue /= 100;

			// We want to prioritise poaching first
			if (bPoachTarget && !bPoachTemp)
				continue;


			if (iValue > iBestValue) {
				iBestValue = iValue;
				pBestPlot = getPathEndTurnPlot();
				bPoachTarget = bPoachTemp;
			}

		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}

bool CvUnitAI::AI_sellSlaves(bool bForce) {
	bool bSellSlaves = false;

	// cache some data
	int iNumSlaves = getSlaveCountTotal();
	int iMaxSlaves = getMaxSlaves();

	if (bForce || (iNumSlaves == iMaxSlaves)) {
		// We have a full quota of slaves so need to find a city to sell them
		bSellSlaves = true;
	} else {
		CvCity* pCity = plot()->getPlotCity();
		// If we are in one of our city radius and have at least half our slave quota then offload them
		if ((pCity != NULL) && (pCity->getOwnerINLINE() == getOwnerINLINE()) && (pCity->isSlaveMarket()) && (iNumSlaves * 2 >= iMaxSlaves)) {
			bSellSlaves = true;
		}
	}

	if (bSellSlaves) {
		CvPlot* pBestPlot = getBestSlaveMarket();
		if (pBestPlot != NULL) {
			if (atPlot(pBestPlot)) {
				getGroup()->pushMission(MISSION_SELL_SLAVE);
				return true;
			} else {
				getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
				return true;
			}
		}
	}

	return false;
}


bool CvUnitAI::AI_huntRange(int iRange, int iOddsThreshold, bool bStayInBorders, int iMinValue) {
	PROFILE_FUNC();

	int iBestValue = iMinValue;
	CvPlot* pBestPlot = NULL;

	for (int iDX = -(iRange); iDX <= iRange; iDX++) {
		for (int iDY = -(iRange); iDY <= iRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (AI_plotValid(pLoopPlot)) {
					if (!bStayInBorders || (pLoopPlot->getOwnerINLINE() == getOwnerINLINE())) {
						if (pLoopPlot->isVisibleEnemyUnit(this)) {
							int iPathTurns;
							if (!atPlot(pLoopPlot) && canMoveInto(pLoopPlot, true) && generatePath(pLoopPlot, 0, true, &iPathTurns) && (iPathTurns <= iRange)) {
								if (pLoopPlot->getNumVisibleEnemyDefenders(this) <= getGroup()->getNumUnits()) {
									if (pLoopPlot->getNumVisibleAdjacentEnemyDefenders(this) <= ((getGroup()->getNumUnits() * 3) / 2)) {
										int iValue = pLoopPlot->getNumVisiblePotentialEnemyDefenders(this) == 0 ? 100 : AI_getWeightedOdds(pLoopPlot, true);
										if (iValue >= iOddsThreshold) {
											if (iValue > iBestValue) {
												iBestValue = iValue;
												pBestPlot = getPathEndTurnPlot();
												FAssert(!atPlot(pBestPlot));
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_SINGLE_ATTACK, false, false);
		return true;
	}

	return false;
}

void CvUnitAI::AI_barbAttackMove() {
	PROFILE_FUNC();

	if (AI_guardCity(false, true, 1)) {
		return;
	}

	if (plot()->isGoody()) {
		if (AI_anyAttack(1, 90)) {
			return;
		}

		if (plot()->plotCount(PUF_isUnitAIType, UNITAI_ATTACK, -1, getOwnerINLINE()) == 1 && getGroup()->getNumUnits() == 1) {
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
	}

	if (GC.getGameINLINE().getSorenRandNum(2, "AI Barb") == 0) {
		if (AI_pillageRange(1)) {
			return;
		}
	}

	if (AI_anyAttack(1, 20)) {
		return;
	}

	if (GC.getGameINLINE().isOption(GAMEOPTION_RAGING_BARBARIANS)) {
		if (AI_pillageRange(4)) {
			return;
		}

		if (AI_cityAttack(3, 10)) {
			return;
		}

		if (area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE) {
			if (AI_groupMergeRange(UNITAI_ATTACK, 1, true, true, true)) {
				return;
			}

			if (AI_groupMergeRange(UNITAI_ATTACK_CITY, 3, true, true, true)) {
				return;
			}

			if (AI_goToTargetCity(MOVE_ATTACK_STACK, 12)) {
				return;
			}
		}
	} else if (GC.getGameINLINE().getNumCivCities() > (GC.getGameINLINE().countCivPlayersAlive() * 3)) {
		if (AI_cityAttack(1, 15)) {
			return;
		}

		if (AI_pillageRange(3)) {
			return;
		}

		if (AI_cityAttack(2, 10)) {
			return;
		}

		if (area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE) {
			if (AI_groupMergeRange(UNITAI_ATTACK, 1, true, true, true)) {
				return;
			}

			if (AI_groupMergeRange(UNITAI_ATTACK_CITY, 3, true, true, true)) {
				return;
			}

			if (AI_goToTargetCity(MOVE_ATTACK_STACK, 12)) {
				return;
			}
		}
	} else if (GC.getGameINLINE().getNumCivCities() > (GC.getGameINLINE().countCivPlayersAlive() * 2)) {
		if (AI_pillageRange(2)) {
			return;
		}

		if (AI_cityAttack(1, 10)) {
			return;
		}
	}

	if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 1)) {
		return;
	}

	if (AI_heal()) {
		return;
	}

	if (AI_guardCity(false, true, 2)) {
		return;
	}

	if (AI_patrol()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

// This function has been heavily edited by K-Mod
void CvUnitAI::AI_attackMove() {
	PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	bool bDanger = (kOwner.AI_getAnyPlotDanger(plot(), 3));
	bool bLandWar = kOwner.AI_isLandWar(area()); // K-Mod

	// K-Mod note. We'll split the group up later if we need to. (bbai group splitting code deleted.)
	FAssert(getGroup()->countNumUnitAIType(UNITAI_ATTACK_CITY) == 0); // K-Mod. (I'm pretty sure this can't happen.)

	// Attack choking units
	// K-Mod (bbai code deleted)
	if (plot()->getTeam() == getTeam() && (bDanger || area()->getAreaAIType(getTeam()) != AREAAI_NEUTRAL)) {
		if (bDanger && plot()->isCity()) {
			if (AI_leaveAttack(2, 55, 105))
				return;
		} else {
			if (AI_defendTeritory(70, 0, 2, true))
				return;
		}
	}

	// If we are on a spawning improvement pillage it
	if (plot()->getImprovementType() != NO_IMPROVEMENT) {
		CvImprovementInfo& kImprovement = GC.getImprovementInfo(plot()->getImprovementType());
		if (kImprovement.getAnimalSpawnRatePercentage() > 0 || kImprovement.getBarbarianSpawnRatePercentage() > 0) {
			getGroup()->pushMission(MISSION_PILLAGE, -1, -1, 0, false, false, MISSIONAI_PILLAGE, plot());
			return;
		}
	}

	// if Slavers have been picking off our workers then protect them
	if (area()->getSlaveMemoryPerPlayer(getOwnerINLINE())) {
		if (AI_shadow(UNITAI_WORKER, 1, -1, false, true, 3, true)) {
			return;
		}
	}

	//Check if we need any slavers
	if (AI_becomeSlaver())
		return;

	// Do we want to go barbarian
	if (AI_becomeBarbarian())
		return;

	{
		PROFILE("CvUnitAI::AI_attackMove() 1");

		// Guard a city we're in if it needs it
		if (AI_guardCity(true)) {
			return;
		}

		if (!(plot()->isOwned()) || (plot()->getOwnerINLINE() == getOwnerINLINE())) {
			if (area()->getCitiesPerPlayer(getOwnerINLINE()) > kOwner.AI_totalAreaUnitAIs(area(), UNITAI_CITY_DEFENSE)) {
				// Defend colonies in new world
				if (getGroup()->getNumUnits() == 1 ? AI_guardCityMinDefender(true) : AI_guardCity(true, true, 3)) // K-Mod
				{
					return;
				}
			}
		}

		if (AI_heal(30, 1)) {
			return;
		}

		if (AI_omniGroup(UNITAI_SETTLE, 2, -1, false, 0, 3, false, false, false, false, false))
			return;

		if (AI_guardCityAirlift()) {
			return;
		}

		if (AI_guardCity(false, true, 1)) {
			return;
		}

		//join any city attacks in progress
		if (isEnemy(plot()->getTeam())) {
			if (AI_omniGroup(UNITAI_ATTACK_CITY, -1, -1, true, 0, 2, true, false)) {
				return;
			}
		}

		AreaAITypes eAreaAIType = area()->getAreaAIType(getTeam());
		if (plot()->isCity()) {
			if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
				if ((eAreaAIType == AREAAI_ASSAULT) || (eAreaAIType == AREAAI_ASSAULT_ASSIST)) {
					if (AI_offensiveAirlift()) {
						return;
					}
				}
			}
		}

		if (bDanger) {
			if (getGroup()->getNumUnits() > 1 && AI_stackVsStack(3, 110, 65, 0))
				return;

			if (collateralDamage() > 0) {
				if (AI_anyAttack(1, 45, 0, 3)) {
					return;
				}
			}
		}
		// K-Mod (moved from below, and replacing the disabled stuff above)
		if (AI_anyAttack(1, 70)) {
			return;
		}

		if (!noDefensiveBonus()) {
			if (AI_guardCity(false, false)) {
				return;
			}
		}

		if (!bDanger) {
			if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
				bool bAssault = ((eAreaAIType == AREAAI_ASSAULT) || (eAreaAIType == AREAAI_ASSAULT_MASSING) || (eAreaAIType == AREAAI_ASSAULT_ASSIST));
				if (bAssault) {
					if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK_CITY, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 4)) {
						return;
					}
				}

				if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, -1, -1, -1, 1, MOVE_SAFE_TERRITORY, 3)) {
					return;
				}

				if (!bLandWar) {
					// Fill transports before starting new one, but not just full of our unit ai
					if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, 1, -1, -1, 1, MOVE_SAFE_TERRITORY, 4)) {
						return;
					}

					// Pick new transport which has space for other unit ai types to join
					if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, -1, 2, -1, -1, MOVE_SAFE_TERRITORY, 4)) {
						return;
					}
				}

				if (kOwner.AI_unitTargetMissionAIs(this, MISSIONAI_GROUP) > 0) {
					getGroup()->pushMission(MISSION_SKIP);
					return;
				}
			}
		}

		// Allow larger groups if outside territory
		if (getGroup()->getNumUnits() < 3) {
			if (plot()->isOwned() && GET_TEAM(getTeam()).isAtWar(plot()->getTeam())) {
				if (AI_omniGroup(UNITAI_ATTACK, 3, -1, false, 0, 1, true, false, true, false, false)) {
					return;
				}
			}
		}

		if (AI_goody(3)) {
			return;
		}
	}

	{
		PROFILE("CvUnitAI::AI_attackMove() 2");

		if (bDanger) {
			// K-Mod. This block has been rewriten. (original code deleted)

			// slightly more reckless than last time
			if (getGroup()->getNumUnits() > 1 && AI_stackVsStack(3, 90, 40, 0))
				return;

			bool bAggressive = area()->getAreaAIType(getTeam()) != AREAAI_DEFENSIVE || getGroup()->getNumUnits() > 1 || plot()->getTeam() != getTeam();

			if (bAggressive && AI_pillageRange(1, 10))
				return;

			if (plot()->getTeam() == getTeam()) {
				if (AI_defendTeritory(55, 0, 2, true)) {
					return;
				}
			} else if (AI_anyAttack(1, 45)) {
				return;
			}

			if (bAggressive && AI_pillageRange(3, 10)) {
				return;
			}

			if (getGroup()->getNumUnits() < 4 && isEnemy(plot()->getTeam())) {
				if (AI_choke(1)) {
					return;
				}
			}

			if (bAggressive && AI_anyAttack(3, 40))
				return;
		}

		if (!isEnemy(plot()->getTeam())) {
			if (AI_heal()) {
				return;
			}
		}

		if (!plot()->isCity() || plot()->plotCount(PUF_isUnitAIType, UNITAI_CITY_DEFENSE, -1, getOwnerINLINE()) > 0) // K-Mod
		{
			// BBAI TODO: If we're fast, maybe shadow an attack city stack and pillage off of it

			bool bIgnoreFaster = false;
			if (kOwner.AI_isDoStrategy(AI_STRATEGY_LAND_BLITZ)) {
				if (area()->getAreaAIType(getTeam()) != AREAAI_ASSAULT) {
					bIgnoreFaster = true;
				}
			}

			bool bAttackCity = bLandWar && (area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE || (AI_getBirthmark() + GC.getGameINLINE().getGameTurn() / 8) % 5 <= 1);
			if (bAttackCity) {
				// strong merge strategy
				if (AI_omniGroup(UNITAI_ATTACK_CITY, -1, -1, true, 0, 5, true, getGroup()->getNumUnits() < 2, bIgnoreFaster, false, false))
					return;
			} else {
				// weak merge strategy
				if (AI_omniGroup(UNITAI_ATTACK_CITY, -1, 2, true, 0, 5, true, false, bIgnoreFaster, false, false))
					return;
			}

			if (AI_omniGroup(UNITAI_ATTACK, 2, -1, false, 0, 4, true, true, true, true, false)) {
				return;
			}

			if (AI_omniGroup(UNITAI_ATTACK, 1, 1, false, 0, 1, true, true, false, false, false)) {
				return;
			}

			// K-Mod. If we're feeling aggressive, then try to get closer to the enemy.
			if (bAttackCity && getGroup()->getNumUnits() > 1) {
				if (AI_goToTargetCity(0, 12))
					return;
			}
		}

		if (AI_guardCity(false, true, 3)) {
			return;
		}

		if ((kOwner.getNumCities() > 1) && (getGroup()->getNumUnits() == 1)) {
			if (area()->getAreaAIType(getTeam()) != AREAAI_DEFENSIVE) {
				if (area()->getNumUnrevealedTiles(getTeam()) > 0) {
					if (kOwner.AI_areaMissionAIs(area(), MISSIONAI_EXPLORE, getGroup()) < (kOwner.AI_neededExplorers(area()) + 1)) {
						if (AI_exploreRange(3)) {
							return;
						}

						if (AI_explore()) {
							return;
						}
					}
				}
			}
		}

		if (AI_defendTeritory(45, 0, 7)) // K-Mod
		{
			return;
		}

		if (AI_offensiveAirlift()) {
			return;
		}

		if (!bDanger && (area()->getAreaAIType(getTeam()) != AREAAI_DEFENSIVE)) {
			if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
				if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, 1, -1, -1, 1, MOVE_SAFE_TERRITORY, 4)) {
					return;
				}

				if ((GET_TEAM(getTeam()).getAtWarCount(true) > 0) && !(getGroup()->isHasPathToAreaEnemyCity())) {
					if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 4)) {
						return;
					}
				}
			}
		}

		if (getGroup()->getNumUnits() >= 4 && plot()->getTeam() == getTeam()) {
			CvSelectionGroup* pSplitGroup, * pRemainderGroup = NULL;
			pSplitGroup = getGroup()->splitGroup(2, 0, &pRemainderGroup);
			if (pSplitGroup)
				pSplitGroup->pushMission(MISSION_SKIP);
			if (pRemainderGroup) {
				if (pRemainderGroup->AI_isForceSeparate())
					pRemainderGroup->AI_separate();
				else
					pRemainderGroup->pushMission(MISSION_SKIP);
			}
			return;
		}

		if (AI_defend()) {
			return;
		}

		if (AI_travelToUpgradeCity()) {
			return;
		}

		if (AI_handleStranded())
			return;

		if (AI_patrol()) {
			return;
		}

		if (AI_retreatToCity()) {
			return;
		}

		if (AI_safety()) {
			return;
		}
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_paratrooperMove() {
	PROFILE_FUNC();

	bool bHostile = (plot()->isOwned() && isPotentialEnemy(plot()->getTeam()));
	if (!bHostile) {
		if (AI_guardCity(true)) {
			return;
		}

		if (plot()->getTeam() == getTeam()) {
			if (plot()->isCity()) {
				if (AI_heal(30, 1)) {
					return;
				}
			}

			bool bLandWar = GET_PLAYER(getOwnerINLINE()).AI_isLandWar(area()); // K-Mod
			if (!bLandWar) {
				if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, -1, -1, -1, 0, MOVE_SAFE_TERRITORY, 4)) {
					return;
				}
			}
		}

		if (AI_guardCity(false, true, 1)) {
			return;
		}
	}

	if (AI_cityAttack(1, 45)) {
		return;
	}

	if (AI_anyAttack(1, 55)) {
		return;
	}

	if (!bHostile) {
		if (AI_paradrop(getDropRange())) {
			return;
		}

		if (AI_offensiveAirlift()) {
			return;
		}

		if (AI_moveToStagingCity()) {
			return;
		}

		if (AI_guardFort(true)) {
			return;
		}

		if (AI_guardCityAirlift()) {
			return;
		}
	}

	if (AI_pillageRange(1, 15)) {
		return;
	}

	if (bHostile) {
		if (AI_choke(1)) {
			return;
		}
	}

	if (AI_heal()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_defendTeritory(55, 0, 5)) // K-Mod
	{
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

// This function has been heavily edited by K-Mod and by BBAI
void CvUnitAI::AI_attackCityMove() {
	PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	AreaAITypes eAreaAIType = area()->getAreaAIType(getTeam());
	bool bLandWar = !isBarbarian() && kOwner.AI_isLandWar(area()); // K-Mod
	bool bAssault = !isBarbarian() && ((eAreaAIType == AREAAI_ASSAULT) || (eAreaAIType == AREAAI_ASSAULT_ASSIST) || (eAreaAIType == AREAAI_ASSAULT_MASSING));

	bool bTurtle = kOwner.AI_isDoStrategy(AI_STRATEGY_TURTLE);
	bool bAlert1 = kOwner.AI_isDoStrategy(AI_STRATEGY_ALERT1);
	bool bIgnoreFaster = false;
	if (kOwner.AI_isDoStrategy(AI_STRATEGY_LAND_BLITZ)) {
		if (!bAssault && area()->getCitiesPerPlayer(getOwnerINLINE()) > 0) {
			bIgnoreFaster = true;
		}
	}

	bool bInCity = plot()->isCity();

	if (bInCity && plot()->getOwnerINLINE() == getOwnerINLINE()) {
		// force heal if we in our own city and damaged
		// can we remove this or call AI_heal here?
		if ((getGroup()->getNumUnits() == 1) && (getDamage() > 0)) {
			getGroup()->pushMission(MISSION_HEAL);
			return;
		}

		if ((GC.getGame().getGameTurn() - plot()->getPlotCity()->getGameTurnAcquired()) <= 1) {
			CvSelectionGroup* pOldGroup = getGroup();

			pOldGroup->AI_separateNonAI(UNITAI_ATTACK_CITY);

			if (pOldGroup != getGroup()) {
				return;
			}
		}

		if (AI_guardCity(false)) // note. this will eject a unit to defend the city rather then using the whole group
		{
			return;
		}

		if (bAssault) // K-Mod
		{
			if (AI_offensiveAirlift()) {
				return;
			}
		}
	}

	bool bAtWar = isEnemy(plot()->getTeam());

	bool bHuntBarbs = false;
	if (area()->getCitiesPerPlayer(BARBARIAN_PLAYER) > 0 && !isBarbarian()) {
		if ((eAreaAIType != AREAAI_OFFENSIVE) && (eAreaAIType != AREAAI_DEFENSIVE) && !bAlert1 && !bTurtle) {
			bHuntBarbs = true;
		}
	}

	bool bReadyToAttack = false;
	if (!bTurtle) {
		bReadyToAttack = getGroup()->getNumUnits() >= (bHuntBarbs ? 3 : AI_stackOfDoomExtra());
	}

	if (isBarbarian()) {
		bLandWar = (area()->getNumCities() - area()->getCitiesPerPlayer(BARBARIAN_PLAYER) > 0);
		bReadyToAttack = (getGroup()->getNumUnits() >= 3);
	}

	if (bReadyToAttack) {
		// Check that stack has units which can capture cities
		// (K-Mod, I've edited this section to distiguish between 'no capture' and 'combat limit < 100')
		bReadyToAttack = false;
		int iNoCombatLimit = 0;
		int iCityCapture = 0;
		CvSelectionGroup* pGroup = getGroup();

		CLLNode<IDInfo>* pUnitNode = pGroup->headUnitNode();
		while (pUnitNode != NULL && !bReadyToAttack) {
			CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
			pUnitNode = pGroup->nextUnitNode(pUnitNode);

			if (pLoopUnit->canAttack()) // K-Mod
			{
				iCityCapture += pLoopUnit->isNoCapture() ? 0 : 1;
				iNoCombatLimit += pLoopUnit->combatLimit() < 100 ? 0 : 1;

				if ((iCityCapture >= 3 || 2 * iCityCapture > pGroup->getNumUnits()) &&
					(iNoCombatLimit >= 6 || 3 * iNoCombatLimit > pGroup->getNumUnits())) {
					bReadyToAttack = true;
				}
			}
		}
	}

	// K-Mod. Try to be consistent in our usage of move flags, so that we don't cause unnecessary pathfinder resets.
	int iMoveFlags = MOVE_AVOID_ENEMY_WEIGHT_2 | (bReadyToAttack ? MOVE_ATTACK_STACK | MOVE_DECLARE_WAR : 0);

	// Barbarian stacks should be reckless and unpredictable.
	if (isBarbarian()) {
		int iThreshold = GC.getGameINLINE().getSorenRandNum(150, "barb attackCity stackVsStack threshold") + 20;
		if (AI_stackVsStack(1, iThreshold, 0, iMoveFlags))
			return;
	}

	if (AI_omniGroup(UNITAI_ATTACK_CITY, -1, -1, true, iMoveFlags, 0, true, false, bIgnoreFaster)) {
		return;
	}

	CvCity* pTargetCity = NULL;
	if (isBarbarian()) {
		pTargetCity = AI_pickTargetCity(iMoveFlags, 10); // was 12 (K-Mod)
	} else {
		// K-Mod. Try to avoid picking a target city in cases where we clearly aren't ready. (just for efficiency.)
		if (bReadyToAttack || bAtWar || (!plot()->isCity() && getGroup()->getNumUnits() > 1))
			pTargetCity = AI_pickTargetCity(iMoveFlags, MAX_INT, bHuntBarbs);
	}

	bool bTargetTooStrong = false; // K-Mod. This is used to prevent the AI from oscillating between moving to attack moving to pillage.
	int iStepDistToTarget = MAX_INT;
	// Note. I've rearranged some parts of the code below, sometimes without comment.
	if (pTargetCity != NULL) {
		int iAttackRatio = GC.getBBAI_ATTACK_CITY_STACK_RATIO();
		int iAttackRatioSkipBombard = GC.getBBAI_SKIP_BOMBARD_MIN_STACK_RATIO();
		iStepDistToTarget = stepDistance(pTargetCity->getX_INLINE(), pTargetCity->getY_INLINE(), getX_INLINE(), getY_INLINE());

		// K-Mod - I'm going to scale the attack ratio based on our war strategy
		if (isBarbarian()) {
			iAttackRatio = 80;
		} else {
			int iAdjustment = 5;
			iAdjustment += GET_TEAM(getTeam()).AI_getWarPlan(pTargetCity->getTeam()) == WARPLAN_LIMITED ? 10 : 0;
			iAdjustment += kOwner.AI_isDoStrategy(AI_STRATEGY_CRUSH) ? -10 : 0;
			iAdjustment += iAdjustment >= 0 && pTargetCity == area()->getTargetCity(getOwnerINLINE()) ? -10 : 0;
			iAdjustment += range((GET_TEAM(getTeam()).AI_getEnemyPowerPercent(true) - 100) / 12, -10, 0);
			iAdjustment += iStepDistToTarget <= 1 && pTargetCity->isOccupation() ? range(111 - (iAttackRatio + iAdjustment), -10, 0) : 0;
			iAttackRatio += iAdjustment;
			iAttackRatioSkipBombard += iAdjustment;
			FAssert(iAttackRatioSkipBombard >= iAttackRatio);
			FAssert(iAttackRatio >= 100);
		}

		int iComparePostBombard = getGroup()->AI_compareStacks(pTargetCity->plot(), true);
		int iBombardTurns = getGroup()->getBombardTurns(pTargetCity);
		// K-Mod note: AI_compareStacks will try to use the AI memory if it can't see.
		{
			// The defense modifier is counted in AI_compareStacks. So if we add it again, we'd be double counting.
			// I'm going to subtract defence, but unfortunately this will reduce based on the total rather than the base.
			int iDefenseModifier = pTargetCity->getDefenseModifier(false);
			int iReducedModifier = iDefenseModifier;
			iReducedModifier *= std::min(20, iBombardTurns);
			iReducedModifier /= 20;
			int iBase = 210 + (pTargetCity->plot()->isHills() ? GC.getHILLS_EXTRA_DEFENSE() : 0);
			iComparePostBombard *= iBase;
			iComparePostBombard /= std::max(1, iBase + iReducedModifier - iDefenseModifier); // def. mod. < 200. I promise.
			// iBase > 100 is to offset the over-reduction from compounding.
			// With iBase == 200, bombarding a defence bonus of 100% will reduce effective defence by 50%
		}

		bTargetTooStrong = iComparePostBombard < iAttackRatio;

		if (iStepDistToTarget <= 2) {
			// K-Mod. I've rearranged and rewriten most of this section - removing the bbai code.
			if (bTargetTooStrong) {
				if (AI_stackVsStack(2, iAttackRatio, 80, iMoveFlags))
					return;

				FAssert(getDomainType() == DOMAIN_LAND);
				int iOurOffense = kOwner.AI_localAttackStrength(plot(), getTeam(), DOMAIN_LAND, 1, false);
				int iEnemyOffense = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2, false);

				// If in danger, seek defensive ground
				if (4 * iOurOffense < 3 * iEnemyOffense) {
					if (AI_omniGroup(UNITAI_ATTACK_CITY, -1, -1, true, iMoveFlags, 3, true, false, bIgnoreFaster, false, false)) // including smaller groups
						return;

					if (iAttackRatio / 2 > iComparePostBombard && 4 * iEnemyOffense / 5 > kOwner.AI_localDefenceStrength(plot(), getTeam())) {
						// we don't have anywhere near enough attack power, and we are in serious danger.
						// unfortunately, if we are "bReadyToAttack", we'll probably end up coming straight back here...
						if (!bReadyToAttack && AI_retreatToCity())
							return;
					}
					if (getGroup()->AI_getMissionAIType() == MISSIONAI_PILLAGE && plot()->defenseModifier(getTeam(), false) > 0) {
						if (isEnemy(plot()->getTeam()) && canPillage(plot())) {
							getGroup()->pushMission(MISSION_PILLAGE, -1, -1, 0, false, false, MISSIONAI_PILLAGE, plot());
							return;
						}
					}

					if (AI_choke(2, true, iMoveFlags)) {
						return;
					}
				} else {
					if (AI_omniGroup(UNITAI_ATTACK_CITY, -1, -1, true, iMoveFlags, 3, true, false, bIgnoreFaster)) // bigger groups only
						return;

					if (canBombard(plot())) {
						getGroup()->pushMission(MISSION_BOMBARD, -1, -1, 0, false, false, MISSIONAI_ASSAULT, pTargetCity->plot());
						return;
					}

					if (AI_omniGroup(UNITAI_ATTACK_CITY, -1, -1, true, iMoveFlags, 3, true, false, bIgnoreFaster, false, false)) // any size
						return;
				}
			}

			if (iStepDistToTarget == 1) {
				// Consider getting into a better position for attack.
				if (iComparePostBombard < GC.getBBAI_SKIP_BOMBARD_BASE_STACK_RATIO() && // only if we don't already have overwhelming force
					(iComparePostBombard < iAttackRatioSkipBombard ||
						pTargetCity->getDefenseDamage() < GC.getMAX_CITY_DEFENSE_DAMAGE() / 2 ||
						plot()->isRiverCrossing(directionXY(plot(), pTargetCity->plot())))) {
					// Only move into attack position if we have a chance.
					// Without this check, the AI can get stuck alternating between this, and pillage.
					// I've tried to roughly take into account how much our ratio would improve by removing a river penalty.
					if ((getGroup()->canBombard(plot()) && iBombardTurns > 2) ||
						(plot()->isRiverCrossing(directionXY(plot(), pTargetCity->plot())) && 150 * iComparePostBombard >= (150 + GC.getRIVER_ATTACK_MODIFIER()) * iAttackRatio)) {
						if (AI_goToTargetCity(iMoveFlags, 2, pTargetCity))
							return;
					}
					// Note: bombard may skip if stack is powerful enough
					if (AI_bombardCity())
						return;
				} else if (iComparePostBombard >= iAttackRatio && AI_bombardCity()) // we're satisfied with our position already. But we still want to consider bombarding.
					return;

				if (iComparePostBombard >= iAttackRatio) {
					// in position; and no desire to bombard.  So attack!
					if (AI_stackAttackCity(iAttackRatio))
						return;
				}
			}

			if (iComparePostBombard >= iAttackRatio && AI_goToTargetCity(iMoveFlags, 4, pTargetCity))
				return;
		}
	}

	// K-Mod. Lets have some slightly smarter stack vs. stack AI.
	// it would be nice to have some personality effection here...
	// eg. protective leaders have a lower risk threshold.   -- Maybe later.
	// Note. This stackVsStack stuff use to be a bit lower, after the group and the heal stuff.
	if (getGroup()->getNumUnits() > 1) {
		if (bAtWar) // recall that "bAtWar" just means we are in enemy territory.
		{
			// note. if we are 2 steps from the target city, this check here is redundant. (see code above)
			if (AI_stackVsStack(1, 160, 95, iMoveFlags))
				return;
		} else {
			if (eAreaAIType == AREAAI_DEFENSIVE && plot()->getOwnerINLINE() == getOwnerINLINE()) {
				if (AI_stackVsStack(4, 110, 55, iMoveFlags))
					return;
				if (AI_stackVsStack(4, 180, 30, iMoveFlags))
					return;
			} else if (AI_stackVsStack(4, 130, 60, iMoveFlags))
				return;
		}
	}
	//
	// K-Mod. The loading of units for assault needs to be before the following omnigroup - otherwise the units may leave the boat to join their friends.
	if (bAssault && (!pTargetCity || pTargetCity->area() != area())) {
		if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, -1, -1, -1, -1, iMoveFlags, 6)) // was 4 max-turns
		{
			return;
		}
	}

	if (AI_omniGroup(UNITAI_ATTACK_CITY, -1, -1, true, iMoveFlags, 2, true, false, bIgnoreFaster)) {
		return;
	}

	if (AI_heal(30, 1)) {
		return;
	}

	if (AI_anyAttack(1, 60, iMoveFlags | MOVE_SINGLE_ATTACK)) // K-Mod (changed to allow cities, and to only use a single unit, but it is still a questionable move)
	{
		return;
	}

	// K-Mod - replacing some stuff I moved / removed from the BBAI code
	if (pTargetCity && bTargetTooStrong && iStepDistToTarget <= (bReadyToAttack ? 3 : 2)) {
		// Pillage around enemy city
		if (generatePath(pTargetCity->plot(), iMoveFlags, true, 0, 5)) {
			// the above path check is just for efficiency.
			// Otherwise we'd be checking every surrounding tile.
			if (AI_pillageAroundCity(pTargetCity, 11, iMoveFlags, 2)) // was 3 turns
				return;

			if (AI_pillageAroundCity(pTargetCity, 0, iMoveFlags, 4)) // was 5 turns
				return;
		}

		// choke the city.
		if (iStepDistToTarget <= 2 && AI_choke(1, false, iMoveFlags))
			return;

		// if we're already standing right next to the city, then goToTargetCity can fail
		// - and we might end up doing something stupid instead. So try again to choke.
		if (iStepDistToTarget <= 1 && AI_choke(3, false, iMoveFlags))
			return;
	}

	// one more thing. Sometimes a single step can cause the AI to change its target city;
	// and when it changes the target - and so sometimes they can get stuck in a loop where
	// they step towards their target, change their mind, step back to pillage something, ... repeat.
	// Here I've made a kludge to break that cycle:
	if (getGroup()->AI_getMissionAIType() == MISSIONAI_PILLAGE) {
		CvPlot* pMissionPlot = getGroup()->AI_getMissionAIPlot();
		if (pMissionPlot && canPillage(pMissionPlot) && isEnemy(pMissionPlot->getTeam(), pMissionPlot)) {
			if (atPlot(pMissionPlot)) {
				getGroup()->pushMission(MISSION_PILLAGE, -1, -1, 0, false, false, MISSIONAI_PILLAGE, pMissionPlot);
				return;
			}
			if (generatePath(pMissionPlot, iMoveFlags, true, 0, 6)) {
				// the max path turns is arbitrary, but it should be at least as big as the pillage sections higher up.
				CvPlot* pEndTurnPlot = getPathEndTurnPlot();
				FAssert(!atPlot(pEndTurnPlot));
				// warning: this command may attack something. We haven't checked!
				getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX_INLINE(), pEndTurnPlot->getY_INLINE(), iMoveFlags, false, false, MISSIONAI_PILLAGE, pMissionPlot);
				return;
			}
		}
	}

	if (bAtWar && (bTargetTooStrong || getGroup()->getNumUnits() <= 2)) {
		if (AI_pillageRange(3, 11, iMoveFlags)) {
			return;
		}

		if (AI_pillageRange(1, 0, iMoveFlags)) {
			return;
		}
	}

	if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
		if (bReadyToAttack) {
			// K-Mod. If the target city is close, be less likely to wait for backup.
			int iPathTurns = 10;
			int iMaxWaitTurns = 3;
			if (pTargetCity && generatePath(pTargetCity->plot(), iMoveFlags, true, &iPathTurns, iPathTurns))
				iMaxWaitTurns = (iPathTurns + 1) / 3;

			MissionAITypes eMissionAIType = MISSIONAI_GROUP;
			int iJoiners = iMaxWaitTurns > 0 ? kOwner.AI_unitTargetMissionAIs(this, &eMissionAIType, 1, getGroup(), iMaxWaitTurns) : 0;

			if (iJoiners * range(iPathTurns - 1, 2, 5) > getGroup()->getNumUnits()) {
				getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GROUP); // (the mission is just for debug feedback)
				return;
			}
		} else {
			if (bTurtle) {
				if (AI_leaveAttack(1, 51, 100))
					return;

				if (AI_defendTeritory(70, iMoveFlags, 3))
					return;
				if (AI_guardCity(false, true, 7, iMoveFlags)) {
					return;
				}
			} else if (!isBarbarian() && eAreaAIType == AREAAI_DEFENSIVE) {
				// Use smaller attack city stacks on defense
				if (AI_defendTeritory(65, iMoveFlags, 3))
					return;

				if (AI_guardCity(false, true, 3, iMoveFlags)) {
					return;
				}
			}

			int iTargetCount = kOwner.AI_unitTargetMissionAIs(this, MISSIONAI_GROUP);
			if ((iTargetCount * 5) > getGroup()->getNumUnits()) {
				MissionAITypes eMissionAIType = MISSIONAI_GROUP;
				int iJoiners = kOwner.AI_unitTargetMissionAIs(this, &eMissionAIType, 1, getGroup(), 2);

				if ((iJoiners * 5) > getGroup()->getNumUnits()) {
					getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GROUP); // K-Mod (for debug feedback)
					return;
				}

				if (AI_moveToStagingCity()) {
					return;
				}
			}
		}
	}

	if (AI_heal(50, 3)) {
		return;
	}

	if (!bAtWar) {
		if (AI_heal()) {
			return;
		}

		if ((getGroup()->getNumUnits() == 1) && (getTeam() != plot()->getTeam())) {
			if (AI_retreatToCity()) {
				return;
			}
		}
	}

	if (!bReadyToAttack && !noDefensiveBonus()) {
		if (AI_guardCity(false, false, MAX_INT, iMoveFlags)) {
			return;
		}
	}

	bool bAnyWarPlan = (GET_TEAM(getTeam()).getAnyWarPlanCount(true) > 0);

	if (bReadyToAttack) {
		if (isBarbarian()) {
			if (pTargetCity && AI_goToTargetCity(iMoveFlags, 12, pTargetCity)) // target city has already been calculated.
				return;
			if (AI_pillageRange(3, 0, iMoveFlags))
				return;
		} else if (pTargetCity) {
			// Before heading out, check whether to wait to allow unit upgrades
			if (bInCity && plot()->getOwnerINLINE() == getOwnerINLINE()) {
				if (!kOwner.AI_isFinancialTrouble() && !pTargetCity->isBarbarian()) {
					// Check if stack has units which can upgrade
					int iNeedUpgradeCount = 0;

					CLLNode<IDInfo>* pUnitNode = getGroup()->headUnitNode();
					while (pUnitNode != NULL) {
						CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
						pUnitNode = getGroup()->nextUnitNode(pUnitNode);

						if (pLoopUnit->getUpgradeCity(false) != NULL) {
							iNeedUpgradeCount++;

							if (5 * iNeedUpgradeCount > getGroup()->getNumUnits()) // was 8*
							{
								getGroup()->pushMission(MISSION_SKIP);
								return;
							}
						}
					}
				}
			}

			// K-Mod. (original bloated code deleted)
			// Estimate the number of turns required.
			int iPathTurns;
			if (!generatePath(pTargetCity->plot(), iMoveFlags, true, &iPathTurns)) {
				iPathTurns = 100;
			}
			if (!pTargetCity->isBarbarian() || iPathTurns < (bAnyWarPlan ? 7 : 12)) // don't bother with long-distance barb attacks
			{
				// See if we can get there faster by boat..
				if (iPathTurns > 5)// && !pTargetCity->isBarbarian())
				{
					// note: if the only land path to our target happens to go through a tough line of defence...
					// we probably want to take the boat even if our iPathTurns is low.
					// Here's one way to account for that:
					// iPathTurns = std::max(iPathTurns, getPathLastNode()->m_iTotalCost / (2000*GC.getMOVE_DENOMINATOR()));
					// Unfortunately, that "2000"... well I think you know what the problem is. So maybe next time.
					int iLoadTurns = std::max(3, iPathTurns / 3 - 1);
					int iMaxTransportTurns = iPathTurns - iLoadTurns - 2;

					if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, -1, -1, -1, -1, iMoveFlags, iLoadTurns, iMaxTransportTurns))
						return;
				}
				// We have to walk.
				if (AI_goToTargetCity(iMoveFlags, MAX_INT, pTargetCity)) {
					return;
				}

				if (bAnyWarPlan) {
					// We're at war, but we failed to walk to the target. Before we start wigging out, lets just check one more thing...
					if (bTargetTooStrong && iStepDistToTarget == 1) {
						// we're standing outside the city already, but we can't capture it and we can't pillage or choke it.
						// I guess we'll just wait for reinforcements to arrive.
						if (AI_safety())
							return;
						getGroup()->pushMission(MISSION_SKIP);
						return;
					}

					CvCity* pTargetCity = area()->getTargetCity(getOwnerINLINE());

					if (pTargetCity != NULL) {
						// this is a last resort. I don't expect that we'll ever actually need it.
						// (it's a pretty ugly function, so I /hope/ we don't need it.)
						FAssertMsg(false, "AI_attackCityMove is resorting to AI_solveBlockageProblem");
						if (AI_solveBlockageProblem(pTargetCity->plot(), (GET_TEAM(getTeam()).getAtWarCount(true) == 0))) {
							return;
						}
					}
				}
			}
		}
	} else {
		int iTargetCount = kOwner.AI_unitTargetMissionAIs(this, MISSIONAI_GROUP);
		if (6 * iTargetCount > getGroup()->getNumUnits()) {
			MissionAITypes eMissionAIType = MISSIONAI_GROUP;
			int iNearbyJoiners = kOwner.AI_unitTargetMissionAIs(this, &eMissionAIType, 1, getGroup(), 2);

			if (4 * iNearbyJoiners > getGroup()->getNumUnits()) {
				getGroup()->pushMission(MISSION_SKIP);
				return;
			}

			if (AI_safety()) {
				return;
			}
		}

		if ((bombardRate() > 0) && noDefensiveBonus()) {
			// BBAI Notes: Add this stack lead by bombard unit to stack probably not lead by a bombard unit
			// BBAI TODO: Some sense of minimum stack size?  Can have big stack moving 10 turns to merge with tiny stacks
			if (AI_omniGroup(UNITAI_ATTACK_CITY, -1, -1, true, iMoveFlags, 10, true, getGroup()->getNumUnits() < 2, bIgnoreFaster, true, true)) {
				return;
			}
		} else {
			if (AI_omniGroup(UNITAI_ATTACK_CITY, AI_stackOfDoomExtra() * 2, -1, true, iMoveFlags, 10, true, getGroup()->getNumUnits() < 2, bIgnoreFaster, false, true)) {
				return;
			}
		}
	}

	if (plot()->getOwnerINLINE() == getOwnerINLINE() && bLandWar) {
		if ((GET_TEAM(getTeam()).getAtWarCount(true) > 0)) {
			// if no land path to enemy cities, try getting there another way
			if (AI_offensiveAirlift()) {
				return;
			}

			if (pTargetCity == NULL) {
				if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, -1, -1, -1, -1, iMoveFlags, 4)) {
					return;
				}
			}
		}
	}

	if (AI_defendTeritory(70, iMoveFlags, 1, true))
		return;

	if (AI_moveToStagingCity()) {
		return;
	}

	if (AI_offensiveAirlift()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

void CvUnitAI::AI_attackCityLemmingMove() {
	if (AI_cityAttack(1, 80)) {
		return;
	}

	if (AI_bombardCity()) {
		return;
	}

	if (AI_cityAttack(1, 40)) {
		return;
	}

	if (AI_goToTargetCity(MOVE_THROUGH_ENEMY)) {
		return;
	}

	if (AI_anyAttack(1, 70)) {
		return;
	}

	if (AI_anyAttack(1, 0)) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
}


void CvUnitAI::AI_collateralMove() {
	PROFILE_FUNC();

	if (AI_defensiveCollateral(51, 3))
		return;

	if (AI_leaveAttack(1, 30, 100)) // was 20
	{
		return;
	}

	if (AI_guardCity(false, true, 1)) {
		return;
	}

	if (AI_heal(30, 1)) {
		return;
	}

	if (AI_cityAttack(1, 35)) {
		return;
	}

	if (AI_anyAttack(1, 55, 0, 2)) {
		return;
	}

	if (AI_anyAttack(1, 35, 0, 3)) {
		return;
	}

	{
		// count our collateral damage units on this plot
		int iTally = 0;
		CLLNode<IDInfo>* pUnitNode = plot()->headUnitNode();
		while (pUnitNode != NULL) {
			CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);

			if (DOMAIN_LAND == pLoopUnit->getDomainType() && pLoopUnit->getOwner() == getOwner()
				&& pLoopUnit->canMove() && pLoopUnit->collateralDamage() > 0) {
				iTally++;
			}

			pUnitNode = plot()->nextUnitNode(pUnitNode);
		}
		FAssert(iTally > 0);
		FAssert(collateralDamageMaxUnits() > 0);

		int iDangerModifier = 100;
		do {
			int iMinOdds = 80 / (3 + iTally);
			iMinOdds *= 100;
			iMinOdds /= iDangerModifier;
			if (AI_anyAttack(1, iMinOdds, 0, std::min(2 * collateralDamageMaxUnits(), collateralDamageMaxUnits() + iTally - 1))) {
				return;
			}
			// Try again with just half the units, just in case our only problem is that we can't find a big enough target stack.
			iTally = (iTally - 1) / 2;
		} while (iTally > 1);
	}

	if (AI_heal()) {
		return;
	}

	if (AI_anyAttack(2, 55, 0, 3)) {
		return;
	}

	if (AI_cityAttack(2, 50)) {
		return;
	}

	if (area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE) {
		const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());
		// if more than a third of our floating defenders are collateral units, convert this one to city attack
		if (3 * kOwner.AI_totalAreaUnitAIs(area(), UNITAI_COLLATERAL) > kOwner.AI_getTotalFloatingDefenders(area())) {
			if (kOwner.AI_unitValue(this, UNITAI_ATTACK_CITY, area()) > 0) {
				AI_setUnitAIType(UNITAI_ATTACK_CITY);
				return; // no mission pushed.
			}
		}
	}

	if (AI_defendTeritory(55, 0, 6)) // K-Mod
	{
		return;
	}

	if (AI_guardCity(false, true, 8)) // was 3
	{
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

void CvUnitAI::AI_pillageMove() {
	PROFILE_FUNC();

	if (AI_guardCity(false, true, 2)) // was 1
	{
		return;
	}

	if (AI_heal(30, 1)) {
		return;
	}

	// BBAI TODO: Shadow ATTACK_CITY stacks and pillage

	//join any city attacks in progress
	if (plot()->isOwned() && plot()->getOwnerINLINE() != getOwnerINLINE()) {
		if (AI_groupMergeRange(UNITAI_ATTACK_CITY, 1, true, true)) {
			return;
		}
	}

	// K-Mod. Pillage units should focus on pillaging, when possible.
	// note: having 2 moves doesn't necessarily mean we can move & pillage in the same turn, but it's a good enough approximation.
	if (AI_pillageRange(getGroup()->baseMoves() > 1 ? 1 : 0, 11)) {
		return;
	}

	if (AI_anyAttack(1, 65)) {
		return;
	}

	if (!noDefensiveBonus()) {
		if (AI_guardCity(false, false)) {
			return;
		}
	}

	if (AI_pillageRange(3, 11)) {
		return;
	}

	if (AI_choke(1)) {
		return;
	}

	if (AI_pillageRange(1)) {
		return;
	}

	if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
		if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 4)) {
			return;
		}
	}

	if (AI_heal(50, 3)) {
		return;
	}

	if (!isEnemy(plot()->getTeam())) {
		if (AI_heal()) {
			return;
		}
	}

	if (AI_group(UNITAI_PILLAGE, /*iMaxGroup*/ 2, /*iMaxOwnUnitAI*/ 1, -1, /*bIgnoreFaster*/ true, false, false, /*iMaxPath*/ 3)) // K-Mod. (later, I might tell counter units to join up.)
	{
		return;
	}

	if ((area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE) || isEnemy(plot()->getTeam())) {
		if (AI_pillageRange(25, 20)) {
			return;
		}
	}

	if (AI_heal()) {
		return;
	}

	if (AI_guardCity(false, true, 3)) {
		return;
	}

	if (AI_offensiveAirlift()) {
		return;
	}

	if (AI_travelToUpgradeCity()) {
		return;
	}

	if (!isHuman() && plot()->isCoastalLand() && GET_PLAYER(getOwnerINLINE()).AI_unitTargetMissionAIs(this, MISSIONAI_PICKUP) > 0) {
		getGroup()->pushMission(MISSION_SKIP);
		return;
	}

	if (plot()->getTeam() == getTeam() && AI_defendTeritory(55, 0, 3, true))
		return;

	if (AI_handleStranded())
		return;

	if (AI_patrol()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_reserveMove() {
	PROFILE_FUNC();

	if (AI_guardCityOnlyDefender())
		return;

	bool bDanger = (GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(plot(), 3));

	if (plot()->getTeam() == getTeam() && (bDanger || area()->getAreaAIType(getTeam()) != AREAAI_NEUTRAL)) {
		if (bDanger && plot()->isCity()) {
			if (AI_leaveAttack(1, 55, 110))
				return;
		} else {
			if (AI_defendTeritory(65, 0, 2, true))
				return;
		}
	} else {
		if (AI_anyAttack(1, 65))
			return;
	}

	if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
		if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, -1, -1, 1, -1, MOVE_SAFE_TERRITORY)) {
			return;
		}
		if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_WORKER, -1, -1, 1, -1, MOVE_SAFE_TERRITORY)) {
			return;
		}
		if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SLAVE, -1, -1, 1, -1, MOVE_SAFE_TERRITORY)) {
			return;
		}
	}

	if (!bDanger || !plot()->isOwned()) // K-Mod
	{
		if (AI_group(UNITAI_SETTLE, 2, -1, -1, false, false, false, 3, true)) {
			return;
		}
	}

	if (AI_guardCity(true)) {
		return;
	}

	if (!noDefensiveBonus()) {
		if (AI_guardFort(false)) {
			return;
		}
	}

	if (AI_guardCityAirlift()) {
		return;
	}

	if (AI_guardCity(false, true, 2)) // was 1
	{
		return;
	}

	if (AI_guardCitySite()) {
		return;
	}

	if (!noDefensiveBonus()) {
		if (AI_guardFort(true)) {
			return;
		}

		if (AI_guardBonus(15)) {
			return;
		}
	}

	if (AI_heal(30, 1)) {
		return;
	}

	if (bDanger) {
		if (AI_anyAttack(3, 50)) {
			return;
		}
	}

	if (AI_defendTeritory(45, 0, 8)) // K-Mod
	{
		return;
	}

	if (AI_guardCity(false, true)) // K-Mod
	{
		return;
	}

	if (AI_defend()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_counterMove() {
	PROFILE_FUNC();

	// Should never have group lead by counter unit
	if (getGroup()->getNumUnits() > 1) {
		UnitAITypes eGroupAI = getGroup()->getHeadUnitAI();
		if (eGroupAI == AI_getUnitAIType()) {
			if (plot()->isCity() && plot()->getOwnerINLINE() == getOwnerINLINE()) {
				getGroup()->AI_separate(); // will change group
				return;
			}
		}
	}

	if (!(plot()->isOwned())) {
		if (AI_groupMergeRange(UNITAI_SETTLE, 2, true, false, false)) {
			return;
		}
	}

	bool bDanger = GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(plot(), 3);
	if (bDanger && plot()->getTeam() == getTeam()) {
		if (plot()->isCity()) {
			if (AI_leaveAttack(1, 65, 115))
				return;
		} else {
			if (AI_defendTeritory(70, 0, 2, true))
				return;
		}
	}

	if (AI_guardCity(false, true, 2)) // K-Mod
	{
		return;
	}

	if (getSameTileHeal() > 0) {
		if (!canAttack()) {
			// Don't restrict to groups carrying cargo ... does this apply to any units in standard bts anyway?
			if (AI_shadow(UNITAI_ATTACK_CITY, -1, 21, false, false, 4)) {
				return;
			}
		}
	}

	AreaAITypes eAreaAIType = area()->getAreaAIType(getTeam());

	if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
		if (!bDanger) {
			if (plot()->isCity()) {
				if ((eAreaAIType == AREAAI_ASSAULT) || (eAreaAIType == AREAAI_ASSAULT_ASSIST)) {
					if (AI_offensiveAirlift()) {
						return;
					}
				}
			}

			if ((eAreaAIType == AREAAI_ASSAULT) || (eAreaAIType == AREAAI_ASSAULT_ASSIST) || (eAreaAIType == AREAAI_ASSAULT_MASSING)) {
				if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK_CITY, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 4)) {
					return;
				}

				if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 4)) {
					return;
				}
			}
		}
	}

	//join any city attacks in progress
	if (plot()->getOwnerINLINE() != getOwnerINLINE()) {
		if (AI_groupMergeRange(UNITAI_ATTACK_CITY, 1, true, true)) {
			return;
		}
	}

	if (bDanger) {
		if (AI_anyAttack(1, 40)) {
			return;
		}
	}

	bool bIgnoreFasterStacks = false;
	if (GET_PLAYER(getOwnerINLINE()).AI_isDoStrategy(AI_STRATEGY_LAND_BLITZ)) {
		if (area()->getAreaAIType(getTeam()) != AREAAI_ASSAULT) {
			bIgnoreFasterStacks = true;
		}
	}

	if (AI_group(UNITAI_ATTACK_CITY, /*iMaxGroup*/ -1, 2, -1, bIgnoreFasterStacks, /*bIgnoreOwnUnitType*/ true, /*bStackOfDoom*/ true, /*iMaxPath*/ 6)) {
		return;
	}

	bool bFastMovers = (GET_PLAYER(getOwnerINLINE()).AI_isDoStrategy(AI_STRATEGY_FASTMOVERS));
	if (AI_group(UNITAI_ATTACK, /*iMaxGroup*/ 2, -1, -1, bFastMovers, /*bIgnoreOwnUnitType*/ true, /*bStackOfDoom*/ true, /*iMaxPath*/ 5)) {
		return;
	}

	// BBAI TODO: merge with nearby pillage

	if (AI_guardCity(true, true, 5)) // K-Mod
	{
		return;
	}

	if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
		if (!bDanger) {
			if ((eAreaAIType != AREAAI_DEFENSIVE)) {
				if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK_CITY, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 4)) {
					return;
				}

				if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 4)) {
					return;
				}
			}
		}
	}

	if (AI_heal()) {
		return;
	}

	if (AI_offensiveAirlift()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_cityDefenseMove() {
	PROFILE_FUNC();

	bool bDanger = (GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(plot(), 3));

	if (!(plot()->isOwned())) {
		if (AI_group(UNITAI_SETTLE, 1, -1, -1, false, false, false, 2, true)) {
			return;
		}
	}

	if (bDanger) {
		if (AI_leaveAttack(1, 70, 140)) // was ,,175
		{
			return;
		}

		if (AI_chokeDefend()) {
			return;
		}
	}

	if (AI_guardCityBestDefender()) {
		return;
	}

	if (!bDanger) {
		if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, -1, -1, 1, -1, MOVE_SAFE_TERRITORY, 1)) {
				return;
			}
		}
	}

	if (AI_guardCityMinDefender(true)) {
		return;
	}

	if (AI_guardCity(true)) {
		return;
	}

	if (!bDanger) {
		if (AI_group(UNITAI_SETTLE, /*iMaxGroup*/ 1, -1, -1, false, false, false, /*iMaxPath*/ 2, /*bAllowRegrouping*/ true)) {
			return;
		}

		if (AI_group(UNITAI_SETTLE, /*iMaxGroup*/ 2, -1, -1, false, false, false, /*iMaxPath*/ 2, /*bAllowRegrouping*/ true)) {
			return;
		}

		if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, -1, -1, 1, -1, MOVE_SAFE_TERRITORY)) {
				return;
			}
		}
	}

	AreaAITypes eAreaAI = area()->getAreaAIType(getTeam());
	if ((eAreaAI == AREAAI_ASSAULT) || (eAreaAI == AREAAI_ASSAULT_MASSING) || (eAreaAI == AREAAI_ASSAULT_ASSIST)) {
		if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK_CITY, -1, -1, -1, 0, MOVE_SAFE_TERRITORY)) {
			return;
		}
	}

	if ((AI_getBirthmark() % 4) == 0) {
		if (AI_guardFort()) {
			return;
		}
	}

	if (AI_guardCityAirlift()) {
		return;
	}

	if (AI_guardCity(false, true, 1)) {
		return;
	}

	if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
		if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, 3, -1, -1, -1, MOVE_SAFE_TERRITORY)) {
			// will enter here if in danger
			return;
		}
	}

	if (AI_guardCity(false, true)) {
		return;
	}

	if (!isBarbarian() && ((area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE) || (area()->getAreaAIType(getTeam()) == AREAAI_MASSING))) {
		bool bIgnoreFaster = false;
		if (GET_PLAYER(getOwnerINLINE()).AI_isDoStrategy(AI_STRATEGY_LAND_BLITZ)) {
			if (area()->getAreaAIType(getTeam()) != AREAAI_ASSAULT) {
				bIgnoreFaster = true;
			}
		}

		if (AI_group(UNITAI_ATTACK_CITY, -1, 2, 4, bIgnoreFaster)) {
			return;
		}
	}

	if (area()->getAreaAIType(getTeam()) == AREAAI_ASSAULT) {
		if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK_CITY, 2, -1, -1, 1, MOVE_SAFE_TERRITORY)) {
			return;
		}
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_cityDefenseExtraMove() {
	PROFILE_FUNC();

	if (!(plot()->isOwned())) {
		if (AI_group(UNITAI_SETTLE, 1, -1, -1, false, false, false, 1, true)) {
			return;
		}
	}

	if (AI_leaveAttack(2, 55, 150)) {
		return;
	}

	if (AI_chokeDefend()) {
		return;
	}

	if (AI_guardCityBestDefender()) {
		return;
	}

	if (AI_guardCity(true)) {
		return;
	}

	if (AI_group(UNITAI_SETTLE, /*iMaxGroup*/ 1, -1, -1, false, false, false, /*iMaxPath*/ 2, /*bAllowRegrouping*/ true)) {
		return;
	}

	if (AI_group(UNITAI_SETTLE, /*iMaxGroup*/ 2, -1, -1, false, false, false, /*iMaxPath*/ 2, /*bAllowRegrouping*/ true)) {
		return;
	}

	CvCity* pCity = plot()->getPlotCity();
	if ((pCity != NULL) && (pCity->getOwnerINLINE() == getOwnerINLINE())) // XXX check for other team?
	{
		if (plot()->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE(), NO_TEAM, PUF_isUnitAIType, AI_getUnitAIType()) == 1) {
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
	}

	if (AI_guardCityAirlift()) {
		return;
	}

	if (AI_guardCity(false, true, 1)) {
		return;
	}

	if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
		if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, 3, -1, -1, -1, MOVE_SAFE_TERRITORY, 3)) {
			return;
		}
	}

	if (AI_guardCity(false, true)) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_exploreMove() {
	PROFILE_FUNC();

	if (!isHuman() && canAttack()) {
		if (AI_anyAttack(1, 70)) {
			return;
		}
	}

	if (getDamage() > 0) {
		if ((plot()->getFeatureType() == NO_FEATURE) || (GC.getFeatureInfo(plot()->getFeatureType()).getTurnDamage() == 0)) {
			getGroup()->pushMission(MISSION_HEAL);
			return;
		}
	}

	// If we are on a spawning improvement pillage it
	if (plot()->getImprovementType() != NO_IMPROVEMENT) {
		CvImprovementInfo& kImprovement = GC.getImprovementInfo(plot()->getImprovementType());
		if (kImprovement.getAnimalSpawnRatePercentage() > 0 || kImprovement.getBarbarianSpawnRatePercentage() > 0) {
			getGroup()->pushMission(MISSION_PILLAGE, -1, -1, 0, false, false, MISSIONAI_PILLAGE, plot());
			return;
		}
	}

	if (!isHuman()) {
		if (AI_pillageRange(3, 10)) {
			return;
		}

		if (AI_cityAttack(3, 80)) {
			return;
		}
	}

	if (AI_goody(4)) {
		return;
	}

	if (AI_exploreRange(3)) {
		return;
	}

	if (!isHuman()) {
		if (AI_pillageRange(3)) {
			return;
		}
	}

	if (AI_explore()) {
		return;
	}

	if (!isHuman()) {
		if (AI_pillageRange(25)) {
			return;
		}
	}

	if (!isHuman()) {
		if (AI_travelToUpgradeCity()) {
			return;
		}
	}

	if (!isHuman() && (AI_getUnitAIType() == UNITAI_EXPLORE)) {
		if (GET_PLAYER(getOwnerINLINE()).AI_totalAreaUnitAIs(area(), UNITAI_EXPLORE) > GET_PLAYER(getOwnerINLINE()).AI_neededExplorers(area())) {
			if (GET_PLAYER(getOwnerINLINE()).calculateUnitCost() > 0) {
				// K-Mod. Maybe we can still use this unit.
				if (GET_PLAYER(getOwnerINLINE()).AI_unitValue(this, UNITAI_ATTACK, area()) > 0) {
					AI_setUnitAIType(UNITAI_ATTACK);
				} else {
					scrap();
				}
				return;
			}
		}
	}
	if (AI_handleStranded())
		return;

	if (AI_patrol()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_missionaryMove() {
	PROFILE_FUNC();

	// K-Mod. Split up groups of automated missionaries - automate them individually.
	if (getGroup()->getNumUnits() > 1) {
		AutomateTypes eAutomate = getGroup()->getAutomateType();
		FAssert(isHuman() && eAutomate != NO_AUTOMATE);

		CLLNode<IDInfo>* pEntityNode = getGroup()->headUnitNode();
		while (pEntityNode) {
			CvUnit* pLoopUnit = ::getUnit(pEntityNode->m_data);
			pEntityNode = getGroup()->nextUnitNode(pEntityNode);

			if (pLoopUnit->canAutomate(eAutomate)) {
				pLoopUnit->joinGroup(0, true);
				pLoopUnit->automate(eAutomate);
			}
		}
		return;
	}

	if (AI_spreadReligion()) {
		return;
	}

	if (AI_spreadCorporation()) {
		return;
	}

	if (!isHuman() || (isAutomated() && GET_TEAM(getTeam()).getAtWarCount(true) == 0)) {
		if (!isHuman() || (getGameTurnCreated() < GC.getGame().getGameTurn())) {
			if (AI_spreadReligionAirlift()) {
				return;
			}
			if (AI_spreadCorporationAirlift()) {
				return;
			}
		}

		if (!isHuman()) {
			if (AI_load(UNITAI_MISSIONARY_SEA, MISSIONAI_LOAD_SPECIAL, NO_UNITAI, -1, -1, -1, 0, MOVE_NO_ENEMY_TERRITORY)) {
				return;
			}
		}
	}

	if (AI_retreatToCity(false, true)) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_prophetMove() {
	PROFILE_FUNC();

	if (AI_greatPersonMove())
		return;

	if (GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(plot(), 2)) // K-Mod (there are good reasons for saving a great person)
	{
		if (AI_discover()) {
			return;
		}
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_artistMove() {
	PROFILE_FUNC();

	if (AI_greatPersonMove())
		return;

	if (GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(plot(), 2)) {
		if (AI_discover()) {
			return;
		}
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_scientistMove() {
	PROFILE_FUNC();

	if (AI_greatPersonMove())
		return;

	if (GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(plot(), 2)) {
		if (AI_discover()) {
			return;
		}
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_generalMove() {
	PROFILE_FUNC();

	std::vector<UnitAITypes> aeUnitAITypes;
	int iDanger = GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 2);

	bool bOffenseWar = (area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE);


	if (iDanger > 0) {
		aeUnitAITypes.clear();
		aeUnitAITypes.push_back(UNITAI_ATTACK);
		aeUnitAITypes.push_back(UNITAI_COUNTER);
		if (AI_lead(aeUnitAITypes)) {
			return;
		}
	}

	if (AI_construct(1)) {
		return;
	}
	if (AI_join(1)) {
		return;
	}

	if (bOffenseWar && (AI_getBirthmark() % 2 == 0)) {
		aeUnitAITypes.clear();
		aeUnitAITypes.push_back(UNITAI_ATTACK_CITY);
		if (AI_lead(aeUnitAITypes)) {
			return;
		}

		aeUnitAITypes.clear();
		aeUnitAITypes.push_back(UNITAI_ATTACK);
		if (AI_lead(aeUnitAITypes)) {
			return;
		}
	}

	if (AI_join(2)) {
		return;
	}

	if (AI_construct(2)) {
		return;
	}
	if (AI_join(4)) {
		return;
	}

	if (GC.getGameINLINE().getSorenRandNum(3, "AI General Construct") == 0) {
		if (AI_construct()) {
			return;
		}
	}

	if (AI_join()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_merchantMove() {
	PROFILE_FUNC();

	if (AI_greatPersonMove())
		return;

	if (GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(plot(), 2)) // K-Mod (there are good reasons for saving a great person)
	{
		if (AI_discover()) {
			return;
		}
	}

	if (AI_caravan(false)) {
		return;
	}

	if (AI_caravan(true)) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_engineerMove() {
	PROFILE_FUNC();

	if (AI_greatPersonMove())
		return;

	if (GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(plot(), 2)) // K-Mod (there are good reasons for saving a great person)
	{
		if (AI_discover()) {
			return;
		}
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

// K-Mod, the previously missing great spy ai function...
void CvUnitAI::AI_greatSpyMove() {
	if (AI_greatPersonMove())
		return;

	// Note: spies can't be seen, and can't be attacked. So we don't need to worry about retreating to safety.
	FAssert(alwaysInvisible());

	if (area()->getNumCities() > area()->getCitiesPerPlayer(getOwner())) {
		if (AI_reconSpy(5)) {
			return;
		}
	}

	if (AI_handleStranded())
		return;

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

// K-Mod. For most great people, the AI needs to do similar checks and calculations.
// I've made this general function to do those calculations for all types of great people.
bool CvUnitAI::AI_greatPersonMove() {
	const CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());

	enum {
		GP_SLOW,
		GP_DISCOVER,
		GP_GOLDENAGE,
		GP_TRADE,
		GP_CULTURE,
		GP_WORLD_VIEW
	};
	std::vector<std::pair<int, int> > missions; // (value, mission)
	// 1) Add possible missions to the mission vector.
	// 2) Sort them.
	// 3) Attempt to carry out missions, starting with the highest value.

	CvPlot* pBestPlot = NULL;
	SpecialistTypes eBestSpecialist = NO_SPECIALIST;
	BuildingTypes eBestBuilding = NO_BUILDING;
	int iBestValue = 1;
	int iBestPathTurns = MAX_INT; // just used as a tie-breaker.
	int iMoveFlags = alwaysInvisible() ? 0 : MOVE_NO_ENEMY_TERRITORY;
	bool bCanHurry = m_pUnitInfo->getBaseHurry() > 0 || m_pUnitInfo->getHurryMultiplier() > 0;

	int iLoop;
	for (CvCity* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop)) {
		if (pLoopCity->area() == area()) {
			int iPathTurns;
			if (generatePath(pLoopCity->plot(), iMoveFlags, true, &iPathTurns)) {
				// Join
				for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
					if (canJoin(pLoopCity->plot(), eSpecialist)) {
						// Note, specialistValue is roughly 400x the commerce it provides. So /= 4 to make it 100x.
						int iValue = pLoopCity->AI_permanentSpecialistValue(eSpecialist) / 4;
						if (iValue > iBestValue || (iValue == iBestValue && iPathTurns < iBestPathTurns)) {
							iBestValue = iValue;
							pBestPlot = getPathEndTurnPlot();
							eBestSpecialist = eSpecialist;
							eBestBuilding = NO_BUILDING;
						}
					}
				}
				// Construct
				for (BuildingClassTypes eBuildingClass = (BuildingClassTypes)0; eBuildingClass < GC.getNumBuildingClassInfos(); eBuildingClass = (BuildingClassTypes)(eBuildingClass + 1)) {
					BuildingTypes eBuilding = (BuildingTypes)GC.getCivilizationInfo(getCivilizationType()).getCivilizationBuildings(eBuildingClass);

					if (eBuilding != NO_BUILDING) {
						if ((m_pUnitInfo->getForceBuildings(eBuilding) || m_pUnitInfo->getBuildings(eBuilding)) &&
							canConstruct(pLoopCity->plot(), eBuilding)) {
							// Note, building value is roughly 4x the value of the commerce it provides.
							// so we * 25 to match the scale of specialist value.
							int iValue = pLoopCity->AI_buildingValue(eBuilding) * 25;

							if (iValue > iBestValue || (iValue == iBestValue && iPathTurns < iBestPathTurns)) {
								iBestValue = iValue;
								pBestPlot = getPathEndTurnPlot();
								eBestBuilding = eBuilding;
								eBestSpecialist = NO_SPECIALIST;
							}
						} else if (bCanHurry && isWorldWonderClass(eBuildingClass) && pLoopCity->canConstruct(eBuilding)) {
							// maybe we can hurry a wonder...
							int iCost = pLoopCity->getProductionNeeded(eBuilding);
							int iHurryProduction = getMaxHurryProduction(pLoopCity);
							int iProgress = pLoopCity->getBuildingProduction(eBuilding);

							int iProductionRate = iCost > iHurryProduction + iProgress ? pLoopCity->getProductionDifference(iCost, iProgress, pLoopCity->getProductionModifier(eBuilding), false, 0) : 0;
							// note: currently, it is impossible for a building to be "food production".
							// also note that iProductionRate will return 0 if the city is in disorder. This may mess up our great person's decision - but it's a non-trivial problem to fix.

							if (pLoopCity->getProductionBuilding() == eBuilding) {
								iProgress += iProductionRate * iPathTurns;
							}

							FAssert(iHurryProduction > 0);
							int iFraction = 100 * std::min(iHurryProduction, iCost - iProgress) / std::max(1, iCost);

							if (iFraction > 40) // arbitary, and somewhat unneccessary.
							{
								FAssert(iFraction <= 100);
								int iValue = pLoopCity->AI_buildingValue(eBuilding) * 25 * iFraction / 100;

								if (iProgress + iHurryProduction < iCost) {
									// decrease the value, because we might still miss out!
									FAssert(iProductionRate > 0 || pLoopCity->isDisorder());
									iValue *= 12;
									iValue /= 12 + std::min(30, pLoopCity->getProductionTurnsLeft(iCost, iProgress, iProductionRate, iProductionRate));
								}

								if (iValue > iBestValue || (iValue == iBestValue && iPathTurns < iBestPathTurns)) {
									iBestValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									iBestPathTurns = iPathTurns;
									eBestBuilding = eBuilding;
									eBestSpecialist = NO_SPECIALIST;
								}
							}
						}
					}
				}
			} // end safe move possible
		} // end this area
	} // end city loop.

	// Toggle a World View
	WorldViewTypes eBestWorldView = NO_WORLD_VIEW;
	int iBestWorldViewValue = 0;
	for (WorldViewTypes eWorldView = (WorldViewTypes)0; eWorldView < NUM_WORLD_VIEWS; eWorldView = (WorldViewTypes)(eWorldView + 1)) {
		int iValue = kPlayer.AI_worldViewValue(eWorldView);

		if (iValue < 0)
			iValue = -(iValue);

		if (iValue > iBestWorldViewValue) {
			iBestWorldViewValue = iValue;
			eBestWorldView = eWorldView;
		}
	}
	if (eBestWorldView != NO_WORLD_VIEW) {
		missions.push_back(std::pair<int, int>(std::min(iBestWorldViewValue * 1000, MAX_INT), GP_WORLD_VIEW)); // We ramp up this value as it is a non destructive mission
	}

	// Golden age
	int iGoldenAgeValue = 0;
	if (isGoldenAge()) {
		iGoldenAgeValue = GET_PLAYER(getOwnerINLINE()).AI_calculateGoldenAgeValue() / GET_PLAYER(getOwnerINLINE()).unitsRequiredForGoldenAge();
		iGoldenAgeValue *= (75 + kPlayer.AI_getStrategyRand(0) % 51);
		iGoldenAgeValue /= 100;
		missions.push_back(std::pair<int, int>(iGoldenAgeValue, GP_GOLDENAGE));
	}
	//

	// Discover ("bulb tech")
	int iDiscoverValue = 0;
	TechTypes eDiscoverTech = getDiscoveryTech();
	if (eDiscoverTech != NO_TECH) {
		iDiscoverValue = getDiscoverResearch(eDiscoverTech);
		// if this isn't going to immediately help our research, it isn't worth as much.
		if (iDiscoverValue < GET_TEAM(getTeam()).getResearchLeft(eDiscoverTech) && kPlayer.getCurrentResearch() != eDiscoverTech) {
			iDiscoverValue *= 2;
			iDiscoverValue /= 3;
		}
		if (kPlayer.AI_isFirstTech(eDiscoverTech)) // founding religions / free techs / free great people
		{
			iDiscoverValue *= 2;
		}
		// amplify the 'undiscovered' bonus based on how likely we are to try to trade the tech.
		iDiscoverValue *= 100 + (200 - GC.getLeaderHeadInfo(kPlayer.getPersonalityType()).getTechTradeKnownPercent()) * GET_TEAM(getTeam()).AI_knownTechValModifier(eDiscoverTech) / 100;
		iDiscoverValue /= 100;
		if (GET_TEAM(getTeam()).getAnyWarPlanCount(true) || kPlayer.AI_isDoStrategy(AI_STRATEGY_ALERT2)) {
			iDiscoverValue *= (area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE ? 5 : 4);
			iDiscoverValue /= 3;
		}

		iDiscoverValue *= (75 + kPlayer.AI_getStrategyRand(3) % 51);
		iDiscoverValue /= 100;
		missions.push_back(std::pair<int, int>(iDiscoverValue, GP_DISCOVER));
	}

	// SlowValue is meant to be a rough estimation of how much value we'll get from doing the best join / build mission.
	// To give this estimate, I'm going to do a rough personality-based calculation of how many turns to count.
	// Note that "iBestValue" is roughly 100x commerce per turn for our best join or build mission.
	// Also note that the commerce per turn is likely to increase as we improve our city infrastructure and so on.
	int iSlowValue = iBestValue;
	if (iSlowValue > 0) {
		// multiply by the full number of turns remaining
		iSlowValue *= GC.getGameINLINE().getEstimateEndTurn() - GC.getGameINLINE().getGameTurn();

		// construct a modifier based on what victory we might like to aim for with our personality & situation
		const CvLeaderHeadInfo& kLeader = GC.getLeaderHeadInfo(kPlayer.getPersonalityType());
		int iModifier =
			2 * std::max(kLeader.getSpaceVictoryWeight(), kPlayer.AI_isDoVictoryStrategy(AI_VICTORY_SPACE1) ? 35 : 0) +
			1 * std::max(kLeader.getCultureVictoryWeight(), kPlayer.AI_isDoVictoryStrategy(AI_VICTORY_CULTURE1) ? 35 : 0) +
			//0 * kLeader.getDiplomacyVictoryWeight() +
			-1 * std::max(kLeader.getDominationVictoryWeight(), kPlayer.AI_isDoVictoryStrategy(AI_VICTORY_DOMINATION1) ? 35 : 0) +
			-2 * std::max(kLeader.getConquestVictoryWeight(), kPlayer.AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST1) ? 35 : 0);
		// If we're small, then slow & steady progress might be our best hope to keep up. So increase the modifier for small civs. (think avg. cities / our cities)
		iModifier += range(40 * GC.getGameINLINE().getNumCivCities() / std::max(1, GC.getGameINLINE().countCivPlayersAlive() * kPlayer.getNumCities()) - 50, 0, 50);

		// convert the modifier into some percentage of the remaining turns
		iModifier = range(30 + iModifier / 2, 20, 80);
		// apply it
		iSlowValue *= iModifier;
		iSlowValue /= 10000; // (also removing excess factor of 100)

		// half the value if anyone we know is up to stage 4. (including us)
		for (PlayerTypes i = (PlayerTypes)0; i < MAX_CIV_PLAYERS; i = (PlayerTypes)(i + 1)) {
			const CvPlayerAI& kLoopPlayer = GET_PLAYER(i);
			if (kLoopPlayer.isAlive() && kLoopPlayer.AI_isDoVictoryStrategyLevel4() && GET_TEAM(getTeam()).isHasMet(kLoopPlayer.getTeam())) {
				iSlowValue /= 2;
				break; // just once.
			}
		}
		//if (gUnitLogLevel > 2) logBBAI("    %S GP slow modifier: %d, value: %d", GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), range(30 + iModifier/2, 20, 80), iSlowValue);
		iSlowValue *= (75 + kPlayer.AI_getStrategyRand(6) % 51);
		iSlowValue /= 100;
		missions.push_back(std::pair<int, int>(iSlowValue, GP_SLOW));
	}

	// Trade mission
	CvPlot* pBestTradePlot;
	int iTradeValue = AI_tradeMissionValue(pBestTradePlot, iDiscoverValue / 2);
	// make it roughly comparable to research points
	if (pBestTradePlot != NULL) {
		iTradeValue *= kPlayer.AI_commerceWeight(COMMERCE_GOLD);
		iTradeValue /= 100;
		iTradeValue *= kPlayer.AI_averageCommerceMultiplier(COMMERCE_RESEARCH);
		iTradeValue /= kPlayer.AI_averageCommerceMultiplier(COMMERCE_GOLD);
		// gold can be targeted where it is needed, but it's benefits typically aren't instant. (cf AI_knownTechValModifier)
		iTradeValue *= 130;
		iTradeValue /= 100;
		if (getGroup()->AI_getMissionAIType() == MISSIONAI_TRADE && plot()->getOwner() != getOwner()) {
			// if we are part way through a trade mission, prefer not to turn back.
			iTradeValue *= 120;
			iTradeValue /= 100;
		}
		iTradeValue *= (75 + kPlayer.AI_getStrategyRand(9) % 51);
		iTradeValue /= 100;
		missions.push_back(std::pair<int, int>(iTradeValue, GP_TRADE));
	}

	// Great works (culture bomb)
	CvPlot* pBestCulturePlot;
	int iCultureValue = AI_greatWorkValue(pBestCulturePlot, iDiscoverValue / 2);
	if (pBestCulturePlot != 0) {
		missions.push_back(std::pair<int, int>(iCultureValue, GP_CULTURE));
	}

	// Sort the list!
	std::sort(missions.begin(), missions.end(), std::greater<std::pair<int, int> >());
	std::vector<std::pair<int, int> >::iterator it;

	int iChoice = 1;
	int iScoreThreshold = 0;
	for (it = missions.begin(); it != missions.end(); ++it) {
		if (it->first < iScoreThreshold)
			break;

		switch (it->second) {
		case GP_DISCOVER:
			if (canDiscover(plot())) {
				getGroup()->pushMission(MISSION_DISCOVER);
				if (gUnitLogLevel > 2) logBBAI("    %S chooses 'discover' (%S) with their %S (value: %d, choice #%d)", GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), GC.getTechInfo(eDiscoverTech).getDescription(), getName(0).GetCString(), iDiscoverValue, iChoice);
				return true;
			}
			break;

		case GP_TRADE:
			{
				MissionAITypes eOldMission = getGroup()->AI_getMissionAIType(); // just used for the log message below
				if (AI_doTradeMission(pBestTradePlot)) {
					if (gUnitLogLevel > 2) logBBAI("    %S %s 'trade mission' with their %S (value: %d, choice #%d)", GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), eOldMission == MISSIONAI_TRADE ? "continues" : "chooses", getName(0).GetCString(), iTradeValue, iChoice);
					return true;
				}
				break;
			}

		case GP_CULTURE:
			{
				MissionAITypes eOldMission = getGroup()->AI_getMissionAIType(); // just used for the log message below
				if (AI_doGreatWork(pBestCulturePlot)) {
					if (gUnitLogLevel > 2) logBBAI("    %S %s 'great work' with their %S (value: %d, choice #%d)", GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), eOldMission == MISSIONAI_TRADE ? "continues" : "chooses", getName(0).GetCString(), iCultureValue, iChoice);
					return true;
				}
				break;
			}

		case GP_GOLDENAGE:
			if (AI_goldenAge()) {
				if (gUnitLogLevel > 2) logBBAI("    %S chooses 'golden age' with their %S (value: %d, choice #%d)", GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), getName(0).GetCString(), iGoldenAgeValue, iChoice);
				return true;
			} else if (kPlayer.AI_totalUnitAIs(AI_getUnitAIType()) < 2) {
				// Do we want to wait for another great person? How long will it take?
				int iGpThreshold = kPlayer.greatPeopleThreshold();
				int iMinTurns = INT_MAX;
				// unfortunately, it's non-trivial to calculate the GP type probabilies. So I'm leaving it out.
				for (CvCity* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop)) {
					int iGpRate = pLoopCity->getGreatPeopleRate();
					if (iGpRate > 0) {
						int iGpProgress = pLoopCity->getGreatPeopleProgress();
						int iTurns = (iGpThreshold - iGpProgress + iGpRate - 1) / iGpRate;
						if (iTurns < iMinTurns)
							iMinTurns = iTurns;
					}
				}

				if (iMinTurns != INT_MAX) {
					int iRelativeWaitTime = iMinTurns + (GC.getGameINLINE().getGameTurn() - getGameTurnCreated());
					iRelativeWaitTime *= 100;
					iRelativeWaitTime /= GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getVictoryDelayPercent();
					// lets say 1% per turn.
					iScoreThreshold = std::max(iScoreThreshold, it->first * (100 - iRelativeWaitTime) / 100);
				}
			}
			break;

		case GP_SLOW:
			// no dedicated function for this.
			if (pBestPlot != NULL) {
				if (eBestSpecialist != NO_SPECIALIST) {
					if (gUnitLogLevel > 2) logBBAI("    %S %s 'join' with their %S (value: %d, choice #%d)", GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), getGroup()->AI_getMissionAIType() == MISSIONAI_JOIN_CITY ? "continues" : "chooses", getName(0).GetCString(), iSlowValue, iChoice);
					if (atPlot(pBestPlot)) {
						getGroup()->pushMission(MISSION_JOIN, eBestSpecialist);
						return true;
					} else {
						getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), iMoveFlags, false, false, MISSIONAI_JOIN_CITY);
						return true;
					}
				}

				if (eBestBuilding != NO_BUILDING) {
					MissionAITypes eMissionAI = canConstruct(pBestPlot, eBestBuilding) ? MISSIONAI_CONSTRUCT : MISSIONAI_HURRY;

					if (gUnitLogLevel > 2) logBBAI("    %S %s 'build' (%S) with their %S (value: %d, choice #%d)", GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), getGroup()->AI_getMissionAIType() == eMissionAI ? "continues" : "chooses", GC.getBuildingInfo(eBestBuilding).getDescription(), getName(0).GetCString(), iSlowValue, iChoice);
					if (atPlot(pBestPlot)) {
						if (eMissionAI == MISSIONAI_CONSTRUCT) {
							getGroup()->pushMission(MISSION_CONSTRUCT, eBestBuilding);
						} else {
							// switch and hurry.
							CvCity* pCity = pBestPlot->getPlotCity();
							FAssert(pCity);

							if (pCity->getProductionBuilding() != eBestBuilding)
								pCity->pushOrder(ORDER_CONSTRUCT, eBestBuilding);

							if (pCity->getProductionBuilding() == eBestBuilding && canHurry(plot())) {
								getGroup()->pushMission(MISSION_HURRY);
							} else {
								FAssertMsg(false, "great person cannot hurry what it intended to hurry.");
								return false;
							}
						}
						return true;
					} else {
						getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), iMoveFlags, false, false, eMissionAI);
						return true;
					}
				}
			}
			break;

		case GP_WORLD_VIEW:
			{
				MissionAITypes eOldMission = getGroup()->AI_getMissionAIType(); // just used for the log message below
				if (AI_toggleWorldView(eBestWorldView)) {
					if (gUnitLogLevel > 2) logBBAI("    %S %s 'great work' with their %S (value: %d, choice #%d)", GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), eOldMission == MISSIONAI_TRADE ? "continues" : "chooses", getName(0).GetCString(), iBestWorldViewValue, iChoice);
					return true;
				}
			}
			break;

		default:
			FAssertMsg(false, "Unhandled great person mission");
			break;
		}
		iChoice++;
	}
	FAssert(iScoreThreshold > 0);
	if (gUnitLogLevel > 2) logBBAI("    %S chooses 'wait' with their %S (value: %d, dead time: %d)", GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), getName(0).GetCString(), iScoreThreshold, GC.getGameINLINE().getGameTurn() - getGameTurnCreated());
	return false;
}

// Edited heavily for K-Mod
void CvUnitAI::AI_spyMove() {
	PROFILE_FUNC();

	const CvTeamAI& kTeam = GET_TEAM(getTeam());
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	// First, let us finish any missions that we were part way through doing
	{
		CvPlot* pMissionPlot = getGroup()->AI_getMissionAIPlot();
		if (pMissionPlot != NULL) {
			switch (getGroup()->AI_getMissionAIType()) {
			case MISSIONAI_GUARD_SPY:
				if (pMissionPlot->getOwner() == getOwner()) {
					if (atPlot(pMissionPlot)) {
						// stay here for a few turns.
						if (hasMoved()) {
							getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_SPY, pMissionPlot);
							return;
						}
						if (GC.getGame().getSorenRandNum(6, "AI Spy continue guarding") > 0) {
							getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_SPY, pMissionPlot);
							return;
						}
					} else {
						// continue to the destination
						if (generatePath(pMissionPlot, 0, true)) {
							CvPlot* pEndTurnPlot = getPathEndTurnPlot();
							getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX_INLINE(), pEndTurnPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_SPY, pMissionPlot);
							return;
						}
					}
				}
				break;
			case MISSIONAI_ATTACK_SPY:
				if (pMissionPlot->getTeam() != getTeam()) {
					if (!atPlot(pMissionPlot)) {
						// continue to the destination
						if (generatePath(pMissionPlot, 0, true)) {
							CvPlot* pEndTurnPlot = getPathEndTurnPlot();
							getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX_INLINE(), pEndTurnPlot->getY_INLINE(), 0, false, false, MISSIONAI_ATTACK_SPY, pMissionPlot);
							return;
						}
					} else if (hasMoved()) {
						getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY);
						return;
					}
				}
				break;
			case MISSIONAI_EXPLORE:
				break;
			case MISSIONAI_LOAD_SPECIAL:
				if (AI_load(UNITAI_SPY_SEA, MISSIONAI_LOAD_SPECIAL))
					return;

			default:
				break;
			}
		}
	}

	int iSpontaneousChance = 0;
	if (plot()->isOwned() && plot()->getTeam() != getTeam()) {
		switch (GET_PLAYER(getOwnerINLINE()).AI_getAttitude(plot()->getOwnerINLINE())) {
		case ATTITUDE_FURIOUS:
			iSpontaneousChance = 100;
			break;

		case ATTITUDE_ANNOYED:
			iSpontaneousChance = 50;
			break;

		case ATTITUDE_CAUTIOUS:
			iSpontaneousChance = (GC.getGameINLINE().isOption(GAMEOPTION_AGGRESSIVE_AI) ? 30 : 10);
			break;

		case ATTITUDE_PLEASED:
			iSpontaneousChance = (GC.getGameINLINE().isOption(GAMEOPTION_AGGRESSIVE_AI) ? 20 : 0);
			break;

		case ATTITUDE_FRIENDLY:
			iSpontaneousChance = 0;
			break;

		default:
			FAssert(false);
			break;
		}

		WarPlanTypes eWarPlan = kTeam.AI_getWarPlan(plot()->getTeam());
		if (eWarPlan != NO_WARPLAN) {
			if (eWarPlan == WARPLAN_LIMITED) {
				iSpontaneousChance += 50;
			} else {
				iSpontaneousChance += 20;
			}
		}

		if (plot()->isCity()) {
			bool bTargetCity = false;

			// K-Mod note: telling AI_getEnemyPlotStrength to not count defensive bonuses is not what we want.
			// That would make it not count hills and city defence promotions; and instead count collateral damage power.
			// Instead, I'm going to count the defensive bonuses, and then try to approximately remove the city part.
			int iOurPower = kOwner.AI_localAttackStrength(plot(), getTeam(), DOMAIN_LAND, 1, true, true);
			int iEnemyPower = kOwner.AI_localDefenceStrength(plot(), NO_TEAM, DOMAIN_LAND, 0);
			{
				int iBase = 235 + (plot()->isHills() ? GC.getHILLS_EXTRA_DEFENSE() : 0);
				iEnemyPower *= iBase - plot()->getPlotCity()->getDefenseModifier(false);
				iEnemyPower /= iBase;
			}
			// cf. approximation used in AI_attackCityMove. (here we are slightly more pessimistic)

			if (95 * iOurPower > GC.getBBAI_ATTACK_CITY_STACK_RATIO() * iEnemyPower && eWarPlan != NO_WARPLAN) {
				bTargetCity = true;

				if (AI_revoltCitySpy()) {
					return;
				}

				if (GC.getGame().getSorenRandNum(6, "AI Spy Skip Turn") > 0) {
					getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY, plot());
					return;
				}

				if (plot()->plotCount(PUF_isSpy, -1, -1, getOwner()) > 2) {
					if (AI_cityOffenseSpy(5, plot()->getPlotCity())) {
						return;
					}
				}
			}

			// I think this spontaneous thing is bad. I'm leaving it in, but with greatly diminished probability.
			// scale for game speed
			iSpontaneousChance *= 100;
			iSpontaneousChance /= GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getVictoryDelayPercent();
			if (GC.getGameINLINE().getSorenRandNum(1500, "AI Spy Espionage") < iSpontaneousChance) {
				if (AI_espionageSpy()) {
					return;
				}
			}

			if (kOwner.AI_plotTargetMissionAIs(plot(), MISSIONAI_ASSAULT, getGroup()) > 0) {
				bTargetCity = true;

				getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY, plot());
				return;
			}

			if (!bTargetCity) {
				// normal city handling

				if (getFortifyTurns() >= GC.getDefineINT("MAX_FORTIFY_TURNS")) {
					if (AI_espionageSpy()) {
						return;
					}
				} else {
					// If we think we'll get caught soon, then do the mission early.
					int iInterceptChance = getSpyInterceptPercent(plot()->getTeam(), false);
					iInterceptChance *= 100 + (GET_TEAM(getTeam()).isOpenBorders(plot()->getTeam())
						? GC.getDefineINT("ESPIONAGE_SPY_NO_INTRUDE_INTERCEPT_MOD")
						: GC.getDefineINT("ESPIONAGE_SPY_INTERCEPT_MOD"));
					iInterceptChance /= 100;
					if (GC.getGame().getSorenRandNum(100, "AI Spy early attack") < iInterceptChance + getFortifyTurns()) {
						if (AI_espionageSpy())
							return;
					}
				}

				if (GC.getGame().getSorenRandNum(100, "AI Spy Skip Turn") > 5) {
					// don't wait forever
					getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY, plot());
					return;
				}
			}
		}
	}

	// Do with have enough points on anyone for an attack mission to be useful?
	int iAttackChance = 0;
	int iTransportChance = 0;
	{
		int iScale = 100 * (kOwner.getCurrentEra() + 1);
		int iAttackSpies = kOwner.AI_areaMissionAIs(area(), MISSIONAI_ATTACK_SPY);
		int iLocalPoints = 0;
		int iTotalPoints = 0;

		if (kOwner.AI_isDoStrategy(AI_STRATEGY_ESPIONAGE_ECONOMY)) {
			iScale += 50 * kOwner.getCurrentEra() * (kOwner.getCurrentEra() + 1);
		}

		for (int iI = 0; iI < MAX_CIV_TEAMS; iI++) {
			int iPoints = kTeam.getEspionagePointsAgainstTeam((TeamTypes)iI);
			iTotalPoints += iPoints;

			if (iI != getTeam() && GET_TEAM((TeamTypes)iI).isAlive() && kTeam.isHasMet((TeamTypes)iI) &&
				GET_TEAM((TeamTypes)iI).countNumCitiesByArea(area()) > 0) {
				int x = 100 * iPoints + iScale;
				x /= iPoints + (1 + iAttackSpies) * iScale;
				iAttackChance = std::max(iAttackChance, x);

				iLocalPoints += iPoints;
			}
		}
		iAttackChance /= kTeam.getAnyWarPlanCount(true) == 0 ? 3 : 1;
		iAttackChance /= plot()->area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE ? 2 : 1;
		iAttackChance /= (kOwner.AI_isDoVictoryStrategy(AI_VICTORY_SPACE4) || kOwner.AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3)) ? 2 : 1;
		iAttackChance *= GC.getLeaderHeadInfo(kOwner.getPersonalityType()).getEspionageWeight();
		iAttackChance /= 100;
		// scale for game speed
		iAttackChance *= 100;
		iAttackChance /= GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getVictoryDelayPercent();

		iTransportChance = (100 * iTotalPoints - 130 * iLocalPoints) / std::max(1, iTotalPoints);
	}

	if (plot()->getTeam() == getTeam()) {
		if (GC.getGame().getSorenRandNum(100, "AI Spy guard / transport") >= iAttackChance) {
			if (GC.getGame().getSorenRandNum(100, "AI Spy transport") < iTransportChance) {
				if (AI_load(UNITAI_SPY_SEA, MISSIONAI_LOAD_SPECIAL, NO_UNITAI, -1, -1, -1, 0, 0, 8))
					return;
			}

			if (AI_guardSpy(0)) {
				return;
			}
		}

		if (!kOwner.AI_isDoStrategy(AI_STRATEGY_BIG_ESPIONAGE) && GET_TEAM(getTeam()).getAtWarCount(true) > 0 &&
			GC.getGame().getSorenRandNum(100, "AI Spy pillage improvement") < (kOwner.AI_getStrategyRand(5) % 36)) {
			if (AI_bonusOffenseSpy(6)) {
				return;
			}
		} else {
			if (AI_cityOffenseSpy(10)) {
				return;
			}
		}
	}

	if (getGroup()->AI_getMissionAIType() == MISSIONAI_ATTACK_SPY && plot()->getNonObsoleteBonusType(getTeam(), true) != NO_BONUS
		&& plot()->isOwned() && kOwner.AI_isMaliciousEspionageTarget(plot()->getOwner())) {
		// assume this is the target of our destroy improvement mission.
		if (getFortifyTurns() >= GC.getDefineINT("MAX_FORTIFY_TURNS")) {
			if (AI_espionageSpy()) {
				return;
			}
		}

		if (GC.getGame().getSorenRandNum(10, "AI Spy skip turn at improvement") > 0) {
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY, plot());
			return;
		}
	}

	if (area()->getNumCities() > area()->getCitiesPerPlayer(getOwnerINLINE())) {
		if (kOwner.AI_areaMissionAIs(area(), MISSIONAI_RECON_SPY) <= kOwner.AI_areaMissionAIs(area(), MISSIONAI_GUARD_SPY) + 1 &&
			(getGroup()->AI_getMissionAIType() == MISSIONAI_RECON_SPY
				|| GC.getGame().getSorenRandNum(3, "AI Spy Choose Movement") > 0)) {
			if (AI_reconSpy(3)) {
				return;
			}
		} else {
			if (GC.getGame().getSorenRandNum(100, "AI Spy defense (2)") >= iAttackChance) {
				if (AI_guardSpy(0)) {
					return;
				}
			}

			if (AI_cityOffenseSpy(20)) {
				return;
			}
		}
	}

	if (AI_load(UNITAI_SPY_SEA, MISSIONAI_LOAD_SPECIAL))
		return;

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

void CvUnitAI::AI_ICBMMove() {
	PROFILE_FUNC();

	if (airRange() > 0) {
		if (AI_nukeRange(airRange())) {
			return;
		}
	} else if (AI_nuke()) {
		return;
	}

	if (isCargo()) {
		getGroup()->pushMission(MISSION_SKIP);
		return;
	}

	if (airRange() > 0) {
		if (AI_missileLoad(UNITAI_MISSILE_CARRIER_SEA, 2, true)) {
			return;
		}

		if (AI_missileLoad(UNITAI_MISSILE_CARRIER_SEA, 1, false)) {
			return;
		}

		if (AI_getBirthmark() % 3 == 0) {
			if (AI_missileLoad(UNITAI_ATTACK_SEA, 0, false)) {
				return;
			}
		}

		if (AI_airOffensiveCity()) {
			return;
		}
	}

	getGroup()->pushMission(MISSION_SKIP);
}


void CvUnitAI::AI_workerSeaMove() {
	PROFILE_FUNC();

	CvCity* pCity;

	int iI;

	if (!(getGroup()->canDefend())) {
		if (GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(plot())) {
			if (AI_retreatToCity()) {
				return;
			}
		}
	}

	if (AI_improveBonus()) {
		return;
	}

	if (isHuman()) {
		FAssert(isAutomated());
		if (plot()->getBonusType() != NO_BONUS) {
			if ((plot()->getOwnerINLINE() == getOwnerINLINE()) || (!plot()->isOwned())) {
				getGroup()->pushMission(MISSION_SKIP);
				return;
			}
		}

		for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
			CvPlot* pLoopPlot = plotDirection(getX_INLINE(), getY_INLINE(), (DirectionTypes)iI);
			if (pLoopPlot != NULL) {
				if (pLoopPlot->getBonusType() != NO_BONUS) {
					if (pLoopPlot->isValidDomainForLocation(*this)) {
						getGroup()->pushMission(MISSION_SKIP);
						return;
					}
				}
			}
		}
	}

	if (!(isHuman()) && (AI_getUnitAIType() == UNITAI_WORKER_SEA)) {
		pCity = plot()->getPlotCity();

		if (pCity != NULL) {
			if (pCity->getOwnerINLINE() == getOwnerINLINE()) {
				if (pCity->AI_neededSeaWorkers() == 0) {
					if (GC.getGameINLINE().getElapsedGameTurns() > 10) {
						if (GET_PLAYER(getOwnerINLINE()).calculateUnitCost() > 0) {
							scrap();
							return;
						}
					}
				} else {
					//Probably icelocked since it can't perform actions.
					scrap();
					return;
				}
			}
		}
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_barbAttackSeaMove() {
	PROFILE_FUNC();

	if (AI_anyAttack(1, 51)) // safe attack
		return;

	if (AI_pillageRange(1)) // near pillage
		return;

	if (AI_heal())
		return;

	if (AI_anyAttack(1, 30)) // reckless attack
		return;

	if (GC.getGameINLINE().getSorenRandNum(10, "AI barb attack sea pillage") < 4 && AI_pillageRange(3)) // long pillage
		return;

	if (GC.getGameINLINE().getSorenRandNum(16, "AI barb attack sea chase") < 15 && AI_anyAttack(2, 45)) // chase
		return;

	// Barb ships will often hang out for a little while blockading before moving on (BBAI)
	if ((GC.getGame().getGameTurn() + AI_getBirthmark()) % 12 > 5) {
		if (AI_pirateBlockade()) {
			return;
		}
	}

	// (trap checking from BBAI)
	if (GC.getGameINLINE().getSorenRandNum(3, "AI Check trapped") == 0) {
		// If trapped in small hole in ice or around tiny island, disband to allow other units to be generated
		bool bScrap = true;
		int iMaxRange = baseMoves() + 2;
		for (int iDX = -(iMaxRange); iDX <= iMaxRange; iDX++) {
			for (int iDY = -(iMaxRange); iDY <= iMaxRange; iDY++) {
				if (bScrap) {
					CvPlot* pLoopPlot = plotXY(plot()->getX_INLINE(), plot()->getY_INLINE(), iDX, iDY);

					if (pLoopPlot != NULL && AI_plotValid(pLoopPlot)) {
						int iPathTurns;
						if (generatePath(pLoopPlot, 0, true, &iPathTurns)) {
							if (iPathTurns > 1) {
								bScrap = false;
							}
						}
					}
				}
			}
		}

		if (bScrap) {
			scrap();
			return;
		}
	}
	// K-Mod / BBAI end

	if (AI_patrol()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

void CvUnitAI::AI_pirateSeaMove() {
	PROFILE_FUNC();

	// heal in defended, unthreatened forts and cities
	if (plot()->isCity(true) && (GET_PLAYER(getOwnerINLINE()).AI_localDefenceStrength(plot(), getTeam()) > 0) && !(GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(plot(), 2, false))) {
		if (AI_heal()) {
			return;
		}
	}

	if (plot()->isOwned() && (plot()->getTeam() == getTeam())) {
		if (AI_anyAttack(2, 40)) {
			return;
		}

		if (AI_defendTeritory(45, 0, 3, true)) // K-Mod
		{
			return;
		}


		if (((AI_getBirthmark() / 8) % 2) == 0) {
			// Previously code actually blocked grouping
			if (AI_group(UNITAI_PIRATE_SEA, -1, 1, -1, true, false, false, 8)) {
				return;
			}
		}
	} else {
		if (AI_anyAttack(2, 51)) {
			return;
		}
	}


	if (GC.getGame().getSorenRandNum(10, "AI Pirate Explore") == 0) {
		CvArea* pWaterArea = plot()->waterArea();

		if (pWaterArea != NULL) {
			if (pWaterArea->getNumUnrevealedTiles(getTeam()) > 0) {
				if (GET_PLAYER(getOwnerINLINE()).AI_areaMissionAIs(pWaterArea, MISSIONAI_EXPLORE, getGroup()) < (GET_PLAYER(getOwnerINLINE()).AI_neededExplorers(pWaterArea))) {
					if (AI_exploreRange(2)) {
						return;
					}
				}
			}
		}
	}

	if (GC.getGame().getSorenRandNum(11, "AI Pirate Pillage") == 0) {
		if (AI_pillageRange(1)) {
			return;
		}
	}

	//Includes heal and retreat to sea routines.
	if (AI_pirateBlockade()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_attackSeaMove() {
	PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	if (plot()->isCity(true)) {
		int iOurDefense = kOwner.AI_localDefenceStrength(plot(), getTeam(), DOMAIN_LAND, 0);
		int iEnemyOffense = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2);

		if (getDamage() > 0)	// extra risk to leaving when wounded
		{
			iOurDefense *= 2;
		}

		if (iEnemyOffense > iOurDefense / 2) // was 1 vs 1/4
		{
			if (AI_anyAttack(2, 50)) {
				return;
			}

			if (AI_shadow(UNITAI_ASSAULT_SEA, 4, 34, false, true, baseMoves())) {
				return;
			}

			if (AI_defendTeritory(45, 0, 3, true)) // K-Mod
			{
				return;
			}

			if (AI_retreatToCity()) {
				return;
			}

			if (AI_safety()) {
				return;
			}
		}
	}

	if (AI_heal(30, 1)) {
		return;
	}

	if (AI_anyAttack(1, 35)) {
		return;
	}

	if (AI_anyAttack(2, 40)) {
		return;
	}

	if (AI_seaBombardRange(6)) {
		return;
	}

	if (AI_heal(50, 3)) {
		return;
	}

	if (AI_heal()) {
		return;
	}

	// BBAI TODO: Turn this into a function, have docked escort ships do it to
	CvCity* pCity = plot()->getPlotCity();

	if (pCity != NULL) {
		if (pCity->isBlockaded()) {
			// City under blockade
			// Attacker has low odds since anyAttack checks above passed, try to break if sufficient numbers

			int iAttackers = plot()->plotCount(PUF_isUnitAIType, UNITAI_ATTACK_SEA, -1, NO_PLAYER, getTeam(), PUF_isGroupHead, -1, -1);
			int iBlockaders = kOwner.AI_getWaterDanger(plot(), 4);

			if (iAttackers > (iBlockaders + 2)) {
				if (iAttackers > GC.getGameINLINE().getSorenRandNum(2 * iBlockaders + 1, "AI - Break blockade")) {
					// BBAI TODO: Make odds scale by # of blockaders vs number of attackers
					if (AI_anyAttack(1, 15)) {
						return;
					}
				}
			}
		}
	}

	if (AI_group(UNITAI_CARRIER_SEA, /*iMaxGroup*/ 4, 1, -1, true, false, false, /*iMaxPath*/ 5)) {
		return;
	}

	if (AI_group(UNITAI_ATTACK_SEA, /*iMaxGroup*/ 1, -1, -1, true, false, false, /*iMaxPath*/ 3)) {
		return;
	}

	if (!plot()->isOwned() || !isEnemy(plot()->getTeam())) {
		// K-Mod / BBAI. I've changed the order of group / shadow.
		// What I'd really like is to join the assault group if the group needs escorts, but shadow if it doesn't.

		// Get at least one shadow per assault group.
		if (AI_shadow(UNITAI_ASSAULT_SEA, 1, -1, true, false, 4)) {
			return;
		}

		// Allow several attack_sea with large flotillas 
		if (AI_group(UNITAI_ASSAULT_SEA, -1, 4, 4, false, false, false, 4, false, true, false)) {
			return;
		}

		// allow just a couple with small asault teams
		if (AI_group(UNITAI_ASSAULT_SEA, -1, 2, -1, false, false, false, 5, false, true, false)) {
			return;
		}

		// Otherwise, try to shadow.
		if (AI_shadow(UNITAI_ASSAULT_SEA, 4, 34, true, false, 4)) {
			return;
		}

		if (AI_shadow(UNITAI_CARRIER_SEA, 4, 51, true, false, 5)) {
			return;
		}
	}

	if (AI_group(UNITAI_CARRIER_SEA, -1, 1, -1, false, false, false, 10)) {
		return;
	}

	if (plot()->isOwned() && (isEnemy(plot()->getTeam()))) {
		if (AI_blockade()) {
			return;
		}
	}

	if (AI_pillageRange(4)) {
		return;
	}

	if (AI_defendTeritory(40, 0, 8)) // K-Mod
	{
		return;
	}

	if (AI_travelToUpgradeCity()) {
		return;
	}

	if (AI_guardBonus(10))
		return;

	if (AI_getBirthmark() % 2 == 0 && AI_guardCoast()) // I want some attackSea units to just patrol the area.
		return;

	if (AI_patrol()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_reserveSeaMove() {
	PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	if (plot()->isCity(true)) {
		int iOurDefense = kOwner.AI_localDefenceStrength(plot(), getTeam(), DOMAIN_LAND, 0);
		int iEnemyOffense = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2);

		if (getDamage() > 0)	// extra risk to leaving when wounded
		{
			iOurDefense *= 2;
		}

		if (iEnemyOffense > iOurDefense / 2) // was 1 vs 1/4
		{
			if (AI_anyAttack(2, 60)) {
				return;
			}

			if (AI_defendTeritory(45, 0, 3, true)) // K-Mod
			{
				return;
			}

			if (AI_shadow(UNITAI_SETTLER_SEA, 2, -1, false, true, baseMoves())) {
				return;
			}

			if (AI_retreatToCity()) {
				return;
			}

			if (AI_safety()) {
				return;
			}
		}
	}

	if (AI_guardBonus(15)) // K-Mod (note: this will defend seafood when we have exactly 1 of them)
	{
		return;
	}

	if (AI_heal(30, 1)) {
		return;
	}

	if (AI_anyAttack(1, 55)) {
		return;
	}

	if (AI_seaBombardRange(6)) {
		return;
	}

	if (AI_defendTeritory(45, 0, 5)) // K-Mod
	{
		return;
	}

	// Shadow any nearby settler sea transport out at sea
	if (AI_shadow(UNITAI_SETTLER_SEA, 2, -1, false, true, 5)) {
		return;
	}

	if (AI_group(UNITAI_RESERVE_SEA, 1, -1, -1, false, false, false, 8)) {
		return;
	}

	if (bombardRate() > 0) {
		if (AI_shadow(UNITAI_ASSAULT_SEA, 2, 30, true, false, 8)) {
			return;
		}
	}

	if (AI_heal(50, 3)) {
		return;
	}

	if (AI_defendTeritory(45, 0, -1)) // K-Mod
	{
		return;
	}

	if (AI_anyAttack(3, 45)) {
		return;
	}

	if (AI_heal()) {
		return;
	}

	if (!isNeverInvisible()) {
		if (AI_anyAttack(5, 35)) {
			return;
		}
	}

	// Shadow settler transport with cargo 
	if (AI_shadow(UNITAI_SETTLER_SEA, 1, -1, true, false, 10)) {
		return;
	}

	if (AI_travelToUpgradeCity()) {
		return;
	}

	if (AI_guardBonus(10))
		return;

	if (AI_guardCoast())
		return;

	if (AI_patrol()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_escortSeaMove() {
	PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	if (plot()->isCity(true)) //prioritize getting outta there
	{
		int iOurDefense = kOwner.AI_localDefenceStrength(plot(), getTeam(), DOMAIN_LAND, 0);
		int iEnemyOffense = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2);

		if (getDamage() > 0)	// extra risk to leaving when wounded
		{
			iOurDefense *= 2;
		}

		if (iEnemyOffense > iOurDefense / 2) // was 1 vs 1/4
		{
			if (AI_anyAttack(1, 60)) {
				return;
			}

			if (AI_group(UNITAI_ASSAULT_SEA, -1, /*iMaxOwnUnitAI*/ 1, -1, /*bIgnoreFaster*/ true, false, false, /*iMaxPath*/ baseMoves())) {
				return;
			}

			if (AI_retreatToCity()) {
				return;
			}

			if (AI_safety()) {
				return;
			}
		}
	}

	if (AI_heal(30, 1)) {
		return;
	}

	if (AI_anyAttack(1, 55)) {
		return;
	}

	// Galleons can get stuck with this AI type since they don't upgrade to any escort unit
	// Galleon escorts are much less useful once Frigates or later are available
	if (!isHuman() && !isBarbarian()) {
		if (getCargo() > 0 && (GC.getUnitInfo(getUnitType()).getSpecialCargo() == NO_SPECIALUNIT)) {
			//Obsolete?
			int iValue = kOwner.AI_unitValue(this, AI_getUnitAIType(), area());
			int iBestValue = kOwner.AI_bestAreaUnitAIValue(AI_getUnitAIType(), area());

			if (iValue < iBestValue) {
				if (kOwner.AI_unitValue(this, UNITAI_ASSAULT_SEA, area()) > 0) {
					AI_setUnitAIType(UNITAI_ASSAULT_SEA);
					return;
				}

				if (kOwner.AI_unitValue(this, UNITAI_SETTLER_SEA, area()) > 0) {
					AI_setUnitAIType(UNITAI_SETTLER_SEA);
					return;
				}

				scrap();
			}
		}
	}

	if (AI_group(UNITAI_CARRIER_SEA, -1, /*iMaxOwnUnitAI*/ 0, -1, /*bIgnoreFaster*/ true)) {
		return;
	}

	if (AI_group(UNITAI_ASSAULT_SEA, -1, /*iMaxOwnUnitAI*/ 0, -1, /*bIgnoreFaster*/ true, false, false, /*iMaxPath*/ 3)) {
		return;
	}

	if (AI_heal(50, 3)) {
		return;
	}

	if (AI_pillageRange(2)) {
		return;
	}

	if (AI_group(UNITAI_MISSILE_CARRIER_SEA, 1, 1, -1, true)) // K-Mod. (presumably this is what they meant)
	{
		return;
	}

	if (AI_group(UNITAI_ASSAULT_SEA, 1, /*iMaxOwnUnitAI*/ 0, /*iMinUnitAI*/ -1, /*bIgnoreFaster*/ true)) {
		return;
	}

	if (AI_group(UNITAI_ASSAULT_SEA, -1, /*iMaxOwnUnitAI*/ 2, /*iMinUnitAI*/ -1, /*bIgnoreFaster*/ true)) {
		return;
	}

	if (AI_group(UNITAI_CARRIER_SEA, -1, /*iMaxOwnUnitAI*/ 2, /*iMinUnitAI*/ -1, /*bIgnoreFaster*/ true)) {
		return;
	}

	// Group only with large flotillas first
	if (AI_group(UNITAI_ASSAULT_SEA, -1, /*iMaxOwnUnitAI*/ 4, /*iMinUnitAI*/ 3, /*bIgnoreFaster*/ true)) {
		return;
	}

	if (AI_shadow(UNITAI_SETTLER_SEA, 2, -1, false, true, 4)) {
		return;
	}

	if (AI_heal()) {
		return;
	}

	if (AI_travelToUpgradeCity()) {
		return;
	}

	// If nothing else useful to do, escort nearby large flotillas even if they're faster
	// Gives Caravel escorts something to do during the Galleon/pre-Frigate era
	if (AI_group(UNITAI_ASSAULT_SEA, -1, /*iMaxOwnUnitAI*/ 4, /*iMinUnitAI*/ 3, /*bIgnoreFaster*/ false, false, false, 4, false, true)) {
		return;
	}

	if (AI_group(UNITAI_ASSAULT_SEA, -1, /*iMaxOwnUnitAI*/ 2, /*iMinUnitAI*/ -1, /*bIgnoreFaster*/ false, false, false, 1, false, true)) {
		return;
	}

	// K-Mod. We don't want to actually end our turn inside the city...
	if (AI_guardCoast(true))
		return;

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_exploreSeaMove() {
	PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	if (plot()->isCity(true)) //prioritize getting outta there
	{
		int iOurDefense = kOwner.AI_localDefenceStrength(plot(), getTeam(), DOMAIN_LAND, 0);
		int iEnemyOffense = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2);

		if (getDamage() > 0)	// extra risk to leaving when wounded
		{
			iOurDefense *= 2;
		}

		if (iEnemyOffense > iOurDefense / 2) // was 1 vs 1/4
		{
			if (!isHuman()) {
				if (AI_anyAttack(1, 60)) {
					return;
				}
			}

			if (AI_retreatToCity()) {
				return;
			}

			if (AI_safety()) {
				return;
			}
		}
	}

	CvArea* pWaterArea = plot()->waterArea();

	if (!isHuman()) {
		if (AI_anyAttack(1, 60)) {
			return;
		}
	}

	if (!isHuman() && !isBarbarian()) //XXX move some of this into a function? maybe useful elsewhere
	{
		//Obsolete?
		int iValue = kOwner.AI_unitValue(this, AI_getUnitAIType(), area());
		int iBestValue = kOwner.AI_bestAreaUnitAIValue(AI_getUnitAIType(), area());

		if (iValue < iBestValue) {
			//Transform
			if (kOwner.AI_unitValue(this, UNITAI_WORKER_SEA, area()) > 0) {
				AI_setUnitAIType(UNITAI_WORKER_SEA);
				return;
			}

			if (kOwner.AI_unitValue(this, UNITAI_PIRATE_SEA, area()) > 0) {
				AI_setUnitAIType(UNITAI_PIRATE_SEA);
				return;
			}

			if (kOwner.AI_unitValue(this, UNITAI_MISSIONARY_SEA, area()) > 0) {
				AI_setUnitAIType(UNITAI_MISSIONARY_SEA);
				return;
			}

			if (kOwner.AI_unitValue(this, UNITAI_RESERVE_SEA, area()) > 0) {
				AI_setUnitAIType(UNITAI_RESERVE_SEA);
				return;
			}
			scrap();
			return;
		}
	}

	if (getDamage() > 0) {
		if ((plot()->getFeatureType() == NO_FEATURE) || (GC.getFeatureInfo(plot()->getFeatureType()).getTurnDamage() == 0)) {
			getGroup()->pushMission(MISSION_HEAL);
			return;
		}
	}

	if (!isHuman()) {
		if (AI_pillageRange(1)) {
			return;
		}
	}

	if (AI_exploreRange(4)) {
		return;
	}

	if (!isHuman()) {
		if (AI_pillageRange(4)) {
			return;
		}
	}

	if (AI_explore()) {
		return;
	}

	if (!isHuman()) {
		if (AI_pillageRange(25)) {
			return;
		}
	}

	if (!isHuman()) {
		if (AI_travelToUpgradeCity()) {
			return;
		}
	}

	if (!(isHuman()) && (AI_getUnitAIType() == UNITAI_EXPLORE_SEA)) {
		pWaterArea = plot()->waterArea();

		if (pWaterArea != NULL) {
			if (kOwner.AI_totalWaterAreaUnitAIs(pWaterArea, UNITAI_EXPLORE_SEA) > kOwner.AI_neededExplorers(pWaterArea)) {
				if (kOwner.calculateUnitCost() > 0) {
					scrap();
					return;
				}
			}
		}
	}

	if (AI_patrol()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

void CvUnitAI::AI_assaultSeaMove() {
	PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	FAssert(AI_getUnitAIType() == UNITAI_ASSAULT_SEA);

	bool bEmpty = !getGroup()->hasCargo();
	bool bFull = getGroup()->getCargo() > 0 && getGroup()->AI_isFull();

	if (plot()->isCity(true)) {
		int iOurDefense = kOwner.AI_localDefenceStrength(plot(), getTeam(), DOMAIN_LAND, 0);
		int iEnemyOffense = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2);

		if (getDamage() > 0)	// extra risk to leaving when wounded
		{
			iOurDefense *= 2;
		}

		if (iEnemyOffense > iOurDefense / 4) // was 1 vs 1/8
		{
			if (iEnemyOffense > iOurDefense / 2) // was 1 vs 1/4
			{
				if (!bEmpty) {
					getGroup()->unloadAll();
				}

				if (AI_anyAttack(1, 65)) {
					return;
				}

				// Retreat to primary area first
				if (AI_retreatToCity(true)) {
					return;
				}

				if (AI_retreatToCity()) {
					return;
				}

				if (AI_safety()) {
					return;
				}
			}

			if (!bFull && !bEmpty) {
				getGroup()->unloadAll();
				getGroup()->pushMission(MISSION_SKIP);
				return;
			}
		}
	}

	if (bEmpty) {
		if (AI_anyAttack(1, 65)) {
			return;
		}
	}

	bool bReinforce = false;
	bool bAttack = false;
	bool bNoWarPlans = (GET_TEAM(getTeam()).getAnyWarPlanCount(true) == 0);
	bool bAttackBarbarian = false;
	bool bIsBarbarian = isBarbarian();

	// Count forts as cities
	bool bIsCity = plot()->isCity(true);

	// Cargo if already at war
	int iTargetReinforcementSize = (bIsBarbarian ? 2 : AI_stackOfDoomExtra()); // K-Mod. =\

	// Cargo to launch a new invasion
	int iTargetInvasionSize = 2 * iTargetReinforcementSize;

	// K-Mod. If we are already en route for invasion, decrease the threshold.
	// (One reason for this decrease is that the threshold may actually increase midway through the journey. We don't want to turn back because of that!)
	if (getGroup()->AI_getMissionAIType() == MISSIONAI_ASSAULT) {
		iTargetReinforcementSize = iTargetReinforcementSize * 2 / 3;
		iTargetInvasionSize = iTargetInvasionSize * 2 / 3;
	}

	int iCargo = getGroup()->getCargo();
	int iEscorts = getGroup()->countNumUnitAIType(UNITAI_ESCORT_SEA) + getGroup()->countNumUnitAIType(UNITAI_ATTACK_SEA);

	AreaAITypes eAreaAIType = area()->getAreaAIType(getTeam());
	bool bLandWar = !bIsBarbarian && kOwner.AI_isLandWar(area()); // K-Mod

	// Plot danger case handled above
	if (hasCargo() && (getUnitAICargo(UNITAI_SETTLE) > 0 || getUnitAICargo(UNITAI_WORKER) > 0 || getUnitAICargo(UNITAI_SLAVE) > 0)) {
		// Dump inappropriate load at first oppurtunity after pick up
		if (bIsCity && (plot()->getOwnerINLINE() == getOwnerINLINE())) {
			getGroup()->unloadAll();
			iCargo = 0; // K-Mod. I see no need to skip.
		} else {
			if (!isFull()) {
				if (AI_pickupStranded(NO_UNITAI, 1)) {
					return;
				}
			}

			if (AI_retreatToCity(true)) {
				return;
			}

			if (AI_retreatToCity()) {
				return;
			}
		}
	}

	if (bIsCity) {
		CvCity* pCity = plot()->getPlotCity();

		if (pCity != NULL && (plot()->getOwnerINLINE() == getOwnerINLINE())) {
			// split out galleys from stack of ocean capable ships
			if (kOwner.AI_unitImpassableCount(getUnitType()) == 0 && getGroup()->getNumUnits() > 1) {
				if (getGroup()->AI_separateImpassable()) {
					// recalculate cached variables.
					bEmpty = !getGroup()->hasCargo();
					bFull = getGroup()->getCargo() > 0 && getGroup()->AI_isFull();
					iCargo = getGroup()->getCargo();
					iEscorts = getGroup()->countNumUnitAIType(UNITAI_ESCORT_SEA) + getGroup()->countNumUnitAIType(UNITAI_ATTACK_SEA);
				}
			}

			// galleys with upgrade available should get that ASAP
			if (kOwner.AI_unitImpassableCount(getUnitType()) > 0) {
				CvCity* pUpgradeCity = getUpgradeCity(false);
				if (pUpgradeCity != NULL && pUpgradeCity == pCity) {
					// Wait for upgrade, this unit is top upgrade priority
					getGroup()->pushMission(MISSION_SKIP);
					return;
				}
			}
		}

		if ((iCargo > 0)) {
			if (pCity != NULL) {
				if ((GC.getGameINLINE().getGameTurn() - pCity->getGameTurnAcquired()) <= 1) {
					if (pCity->getPreviousOwner() != NO_PLAYER) {
						// Just captured city, probably from naval invasion.  If area targets, drop cargo and leave so as to not to be lost in quick counter attack
						if (GET_TEAM(getTeam()).countEnemyPowerByArea(plot()->area()) > 0) {
							getGroup()->unloadAll();

							if (iEscorts > 2) {
								if (getGroup()->countNumUnitAIType(UNITAI_ESCORT_SEA) > 1 && getGroup()->countNumUnitAIType(UNITAI_ATTACK_SEA) > 0) {
									getGroup()->AI_separateAI(UNITAI_ATTACK_SEA);
									getGroup()->AI_separateAI(UNITAI_RESERVE_SEA);

									iEscorts = getGroup()->countNumUnitAIType(UNITAI_ESCORT_SEA);
								}
							}
							iCargo = getGroup()->getCargo();
						}
					}
				}
			}
		}

		if ((iCargo > 0) && (iEscorts == 0)) {
			if (AI_group(UNITAI_ASSAULT_SEA, -1, -1, -1,/*bIgnoreFaster*/true, false, false,/*iMaxPath*/1, false,/*bCargoOnly*/true, false, MISSIONAI_ASSAULT)) {
				return;
			}

			if (plot()->plotCount(PUF_isUnitAIType, UNITAI_ESCORT_SEA, -1, getOwnerINLINE(), NO_TEAM, PUF_isGroupHead, -1, -1) > 0) {
				// Loaded but with no escort, wait for escorts in plot to join us
				getGroup()->pushMission(MISSION_SKIP);
				return;
			}

			MissionAITypes eMissionAIType = MISSIONAI_GROUP;
			if ((kOwner.AI_unitTargetMissionAIs(this, &eMissionAIType, 1, getGroup(), 3) > 0) || (kOwner.AI_getWaterDanger(plot(), 4, false) > 0)) {
				// Loaded but with no escort, wait for others joining us soon or avoid dangerous waters
				getGroup()->pushMission(MISSION_SKIP);
				return;
			}
		}

		if (bLandWar) {
			if (iCargo > 0) {
				if ((eAreaAIType == AREAAI_DEFENSIVE) || (pCity != NULL && pCity->AI_isDanger())) {
					// Unload cargo when on defense or if small load of troops and can reach enemy city over land (generally less risky)
					getGroup()->unloadAll();
					getGroup()->pushMission(MISSION_SKIP);
					return;
				}
			}

			if (iCargo >= iTargetReinforcementSize) {
				getGroup()->AI_separateEmptyTransports();

				if (!(getGroup()->hasCargo())) {
					// K-Mod. (and I've made a second if iCargo > thing)
					FAssert(getGroup()->getNumUnits() == 1);
					iCargo = 0;
					iEscorts = 0;
				}
			}
			if (iCargo >= iTargetReinforcementSize) {
				// Send ready transports
				if (AI_assaultSeaReinforce(false)) {
					return;
				}

				if (AI_assaultSeaTransport(false, true)) {
					return;
				}
			}
		} else {
			if ((eAreaAIType == AREAAI_ASSAULT)) {
				if (iCargo >= iTargetInvasionSize) {
					bAttack = true;
				}
			}

			if ((eAreaAIType == AREAAI_ASSAULT) || (eAreaAIType == AREAAI_ASSAULT_ASSIST)) {
				if ((bFull && iCargo > cargoSpace()) || (iCargo >= iTargetReinforcementSize)) {
					bReinforce = true;
				}
			}
		}

		// K-Mod, same purpose, different implementation.
		// keep ungrouping escort units until we don't have too many.
		if (!bAttack && !bReinforce && plot()->getTeam() == getTeam()) {
			int iAssaultUnits = getGroup()->countNumUnitAIType(UNITAI_ASSAULT_SEA);
			CLLNode<IDInfo>* pEntityNode = getGroup()->headUnitNode();
			while (iEscorts > 3 && iEscorts > 2 * iAssaultUnits && iEscorts > 2 * iCargo && pEntityNode != NULL) {
				CvUnit* pLoopUnit = ::getUnit(pEntityNode->m_data);
				pEntityNode = getGroup()->nextUnitNode(pEntityNode);
				// (maybe we should adjust this to ungroup "escorts" last?)
				if (!pLoopUnit->hasCargo()) {
					switch (pLoopUnit->AI_getUnitAIType()) {
					case UNITAI_ATTACK_SEA:
					case UNITAI_RESERVE_SEA:
					case UNITAI_ESCORT_SEA:
						pLoopUnit->joinGroup(NULL);
						iEscorts--;
						break;
					default:
						break;
					}
				}
			}
			FAssert(!(iEscorts > 3 && iEscorts > 2 * iAssaultUnits && iEscorts > 2 * iCargo));
		}

		MissionAITypes eMissionAIType = MISSIONAI_GROUP;
		if (kOwner.AI_unitTargetMissionAIs(this, &eMissionAIType, 1, getGroup(), 1) > 0) {
			// Wait for units which are joining our group this turn
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}

		if (!bFull) {
			if (bAttack) {
				eMissionAIType = MISSIONAI_LOAD_ASSAULT;
				if (kOwner.AI_unitTargetMissionAIs(this, &eMissionAIType, 1, getGroup(), 1) > 0) {
					// Wait for cargo which will load this turn
					getGroup()->pushMission(MISSION_SKIP);
					return;
				}
			} else if (kOwner.AI_unitTargetMissionAIs(this, MISSIONAI_LOAD_ASSAULT) > 0) {
				// Wait for cargo which is on the way
				getGroup()->pushMission(MISSION_SKIP);
				return;
			}
		}

		if (!bAttack && !bReinforce) {
			if (iCargo > 0) {
				if (AI_group(UNITAI_ASSAULT_SEA, -1, -1, -1,/*bIgnoreFaster*/true, false, false,/*iMaxPath*/5, false,/*bCargoOnly*/true, false, MISSIONAI_ASSAULT)) {
					return;
				}
			}
		}
	}

	if (!bIsCity) {
		if (iCargo >= iTargetInvasionSize) {
			bAttack = true;
		}

		if ((iCargo >= iTargetReinforcementSize) || (bFull && iCargo > cargoSpace())) {
			bReinforce = true;
		}

		CvPlot* pAdjacentPlot = NULL;
		for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
			pAdjacentPlot = plotDirection(getX_INLINE(), getY_INLINE(), ((DirectionTypes)iI));
			if (pAdjacentPlot != NULL) {
				if (iCargo > 0) {
					CvCity* pAdjacentCity = pAdjacentPlot->getPlotCity();
					if (pAdjacentCity != NULL && pAdjacentCity->getOwner() == getOwnerINLINE() && pAdjacentCity->getPreviousOwner() != NO_PLAYER) {
						if ((GC.getGameINLINE().getGameTurn() - pAdjacentCity->getGameTurnAcquired()) < 5) {
							// If just captured city and we have some cargo, dump units in city
							getGroup()->pushMission(MISSION_MOVE_TO, pAdjacentPlot->getX_INLINE(), pAdjacentPlot->getY_INLINE(), 0, false, false, NO_MISSIONAI, pAdjacentPlot);
							// K-Mod note: this use to use missionAI_assault. I've changed it because assault would suggest to the troops that they should stay onboard.
							return;
						}
					}
				} else {
					if (pAdjacentPlot->isOwned() && isEnemy(pAdjacentPlot->getTeam())) {
						if (pAdjacentPlot->getNumDefenders(getOwnerINLINE()) > 2) {
							// if we just made a dropoff in enemy territory, release sea bombard units to support invaders
							if ((getGroup()->countNumUnitAIType(UNITAI_ATTACK_SEA) + getGroup()->countNumUnitAIType(UNITAI_RESERVE_SEA)) > 0) {
								bool bMissionPushed = false;

								if (AI_seaBombardRange(1)) {
									bMissionPushed = true;
								}

								CvSelectionGroup* pOldGroup = getGroup();

								//Release any Warships to finish the job.
								getGroup()->AI_separateAI(UNITAI_ATTACK_SEA);
								getGroup()->AI_separateAI(UNITAI_RESERVE_SEA);

								// Fixed bug in next line with checking unit type instead of unit AI
								if (pOldGroup == getGroup() && AI_getUnitAIType() == UNITAI_ASSAULT_SEA) {
									// Need to be sure all units can move
									if (getGroup()->canAllMove()) {
										if (AI_retreatToCity(true)) {
											bMissionPushed = true;
										}
									}
								}

								if (bMissionPushed) {
									return;
								}
							}
						}
					}
				}
			}
		}

		if (iCargo > 0) {
			MissionAITypes eMissionAIType = MISSIONAI_GROUP;
			if (kOwner.AI_unitTargetMissionAIs(this, &eMissionAIType, 1, getGroup(), 1) > 0) {
				if (iEscorts < kOwner.AI_getWaterDanger(plot(), 2, false)) {
					// Wait for units which are joining our group this turn (hopefully escorts)
					getGroup()->pushMission(MISSION_SKIP);
					return;
				}
			}
		}
	}

	if (bIsBarbarian) {
		if (getGroup()->isFull() || iCargo > iTargetInvasionSize) {
			if (AI_assaultSeaTransport(false)) {
				return;
			}
		} else {
			if (AI_pickup(UNITAI_ATTACK_CITY, true, 5)) {
				return;
			}

			if (AI_pickup(UNITAI_ATTACK, true, 5)) {
				return;
			}

			if (AI_retreatToCity()) {
				return;
			}

			if (!(getGroup()->getCargo())) {
				AI_barbAttackSeaMove();
				return;
			}

			if (AI_safety()) {
				return;
			}

			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
	} else {
		if (bAttack || bReinforce) {
			if (bIsCity) {
				getGroup()->AI_separateEmptyTransports();
			}

			if (!(getGroup()->hasCargo())) {
				bAttack = bReinforce = false; // K-Mod
				iCargo = 0;
			}
		}
		if (bAttack || bReinforce) // K-Mod
		{
			FAssert(getGroup()->hasCargo());

			//BBAI TODO: Check that group has escorts, otherwise usually wait

			if (bAttack) {
				if (bReinforce && (AI_getBirthmark() % 2 == 0)) {
					if (AI_assaultSeaReinforce()) {
						return;
					}
					bReinforce = false;
				}

				if (AI_assaultSeaTransport()) {
					return;
				}
			}

			// If not enough troops for own invasion, 
			if (bReinforce) {
				if (AI_assaultSeaReinforce()) {
					return;
				}
			}
		}

		if (bNoWarPlans && (iCargo >= iTargetReinforcementSize)) {
			bAttackBarbarian = true;

			getGroup()->AI_separateEmptyTransports();

			if (!(getGroup()->hasCargo())) {
				// this unit was empty group leader
				getGroup()->pushMission(MISSION_SKIP);
				return;
			}

			FAssert(getGroup()->hasCargo());
			if (AI_assaultSeaReinforce(bAttackBarbarian)) {
				return;
			}

			FAssert(getGroup()->hasCargo());
			if (AI_assaultSeaTransport(bAttackBarbarian)) {
				return;
			}
		}
	}

	if ((bFull || bReinforce) && !bAttack) {
		if (AI_omniGroup(UNITAI_ASSAULT_SEA, -1, -1, false, 0, 10, true, true, true, false, false, -1, true, true)) {
			return;
		}
	} else if (!bFull) {
		bool bHasOneLoad = (getGroup()->getCargo() >= cargoSpace());
		bool bHasCargo = getGroup()->hasCargo();

		if (AI_pickup(UNITAI_ATTACK_CITY, !bHasCargo, (bHasOneLoad ? 3 : 7))) {
			return;
		}

		if (AI_pickup(UNITAI_ATTACK, !bHasCargo, (bHasOneLoad ? 3 : 7))) {
			return;
		}

		if (AI_pickup(UNITAI_COUNTER, !bHasCargo, (bHasOneLoad ? 3 : 7))) {
			return;
		}

		if (AI_pickup(UNITAI_ATTACK_CITY, !bHasCargo)) {
			return;
		}

		if (!bHasCargo) {
			if (AI_pickupStranded(UNITAI_ATTACK_CITY)) {
				return;
			}

			if (AI_pickupStranded(UNITAI_ATTACK)) {
				return;
			}

			if (AI_pickupStranded(UNITAI_COUNTER)) {
				return;
			}

			if ((getGroup()->countNumUnitAIType(AI_getUnitAIType()) == 1)) {
				// Try picking up any thing
				if (AI_pickupStranded()) {
					return;
				}
			}
		}
	}

	if (bIsCity) {
		FAssert(iCargo == getGroup()->getCargo());
		if (bLandWar && iCargo > 0) {
			// Enemy units in this player's territory
			if (kOwner.AI_countNumAreaHostileUnits(area(), true, false, false, false) > iCargo / 2) {
				getGroup()->unloadAll();
				getGroup()->pushMission(MISSION_SKIP);
				return;
			}
		}
		// K-Mod. (moved from way higher up)
		if (iCargo == 0 && plot()->getTeam() == getTeam() && getGroup()->getNumUnits() > 1) {
			getGroup()->AI_separate();
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
	}

	if (AI_retreatToCity(true)) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_settlerSeaMove() {
	PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	bool bEmpty = !getGroup()->hasCargo();

	if (plot()->isCity(true)) {
		int iOurDefense = kOwner.AI_localDefenceStrength(plot(), getTeam(), DOMAIN_LAND, 0);
		int iEnemyOffense = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2);

		if (getDamage() > 0)	// extra risk to leaving when wounded
		{
			iOurDefense *= 2;
		}

		if (iEnemyOffense > iOurDefense / 2) // was 1 vs 1/4
		{
			if (bEmpty) {
				if (AI_anyAttack(1, 65)) {
					return;
				}
			}

			// Retreat to primary area first
			if (AI_retreatToCity(true)) {
				return;
			}

			if (AI_retreatToCity()) {
				return;
			}

			if (AI_safety()) {
				return;
			}
		}
	}

	if (bEmpty) {
		if (AI_anyAttack(1, 65)) {
			return;
		}
		if (AI_anyAttack(1, 40)) {
			return;
		}
	}

	int iSettlerCount = getUnitAICargo(UNITAI_SETTLE);
	int iWorkerCount = getUnitAICargo(UNITAI_WORKER);

	if (hasCargo() && (iSettlerCount == 0) && (iWorkerCount == 0)) {
		// Dump troop load at first oppurtunity after pick up
		if (plot()->isCity() && plot()->getOwnerINLINE() == getOwnerINLINE()) {
			getGroup()->unloadAll();
			getGroup()->pushMission(MISSION_SKIP);
			return;
		} else {
			if (!(isFull())) {
				if (AI_pickupStranded(NO_UNITAI, 1)) {
					return;
				}
			}

			if (AI_retreatToCity(true)) {
				return;
			}

			if (AI_retreatToCity()) {
				return;
			}
		}
	}

	// Don't send transport with settler and no defense
	if ((iSettlerCount > 0) && (iSettlerCount + iWorkerCount == cargoSpace())) {
		// No defenders for settler
		if (plot()->isCity() && plot()->getOwnerINLINE() == getOwnerINLINE()) {
			getGroup()->unloadAll();
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
	}

	if ((iSettlerCount > 0) && (isFull() ||
		((getUnitAICargo(UNITAI_CITY_DEFENSE) > 0) &&
			(kOwner.AI_unitTargetMissionAIs(this, MISSIONAI_LOAD_SETTLER) == 0)))) {
		if (AI_settlerSeaTransport()) {
			return;
		}
	} else if ((getTeam() != plot()->getTeam()) && bEmpty) {
		if (AI_pillageRange(3)) {
			return;
		}
	}

	if (plot()->isCity() && plot()->getOwnerINLINE() == getOwnerINLINE() && !hasCargo()) {
		AreaAITypes eAreaAI = area()->getAreaAIType(getTeam());
		if ((eAreaAI == AREAAI_ASSAULT) || (eAreaAI == AREAAI_ASSAULT_MASSING)) {
			CvArea* pWaterArea = plot()->waterArea();
			FAssert(pWaterArea != NULL);
			if (pWaterArea != NULL) {
				if (kOwner.AI_totalWaterAreaUnitAIs(pWaterArea, UNITAI_SETTLER_SEA) > 1) {
					if (kOwner.AI_unitValue(this, UNITAI_ASSAULT_SEA, pWaterArea) > 0) {
						AI_setUnitAIType(UNITAI_ASSAULT_SEA);
						AI_assaultSeaMove();
						return;
					}
				}
			}
		}
	}

	if ((iWorkerCount > 0)
		&& kOwner.AI_unitTargetMissionAIs(this, MISSIONAI_LOAD_SETTLER) == 0) {
		if (isFull() || (iSettlerCount == 0)) {
			if (AI_settlerSeaFerry()) {
				return;
			}
		}
	}

	if (!(getGroup()->hasCargo())) {
		if (AI_pickupStranded(UNITAI_SETTLE)) {
			return;
		}
	}

	if (!(getGroup()->isFull())) {
		if (kOwner.AI_unitTargetMissionAIs(this, MISSIONAI_LOAD_SETTLER) > 0) {
			// Wait for units on the way
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}

		if (iSettlerCount > 0) {
			if (AI_pickup(UNITAI_CITY_DEFENSE)) {
				return;
			}
		} else if (cargoSpace() - 2 >= getCargo() + iWorkerCount) {
			if (AI_pickup(UNITAI_SETTLE, true)) {
				return;
			}
		}
	}

	if (GC.getGame().getGameTurn() - getGameTurnCreated() < 8 && plot()->waterArea() && kOwner.AI_areaMissionAIs(plot()->waterArea(), MISSIONAI_EXPLORE, getGroup()) < kOwner.AI_neededExplorers(plot()->waterArea())) // K-Mod
	{
		if ((plot()->getPlotCity() == NULL) || kOwner.AI_totalAreaUnitAIs(plot()->area(), UNITAI_SETTLE) == 0) {
			if (AI_explore()) {
				return;
			}
		}
	}
	if (!getGroup()->hasCargo()) {
		// Rescue stranded non-settlers
		if (AI_pickupStranded()) {
			return;
		}
	}

	if (cargoSpace() - 2 < getCargo() + iWorkerCount) {
		// If full of workers and not going anywhere, dump them if a settler is available
		if ((iSettlerCount == 0) && (plot()->plotCount(PUF_isAvailableUnitAITypeGroupie, UNITAI_SETTLE, -1, getOwnerINLINE(), NO_TEAM, PUF_isFiniteRange) > 0)) {
			getGroup()->unloadAll();

			if (AI_pickup(UNITAI_SETTLE, true)) {
				return;
			}

			return;
		}
	}

	if (!(getGroup()->isFull())) {
		if (AI_pickup(UNITAI_WORKER)) {
			return;
		}
		if (AI_pickup(UNITAI_SLAVE)) {
			return;
		}
	}

	// Carracks cause problems for transport upgrades, galleys can't upgrade to them and they can't
	// upgrade to galleons.  Scrap galleys, switch unit AI for stuck Carracks.
	if (plot()->isCity() && plot()->getOwnerINLINE() == getOwnerINLINE()) {
		//
		{
			UnitTypes eBestSettlerTransport = NO_UNIT;
			kOwner.AI_bestCityUnitAIValue(AI_getUnitAIType(), NULL, &eBestSettlerTransport);
			if (eBestSettlerTransport != NO_UNIT) {
				if (eBestSettlerTransport != getUnitType() && kOwner.AI_unitImpassableCount(eBestSettlerTransport) == 0) {
					UnitClassTypes ePotentialUpgradeClass = (UnitClassTypes)GC.getUnitInfo(eBestSettlerTransport).getUnitClassType();
					if (!upgradeAvailable(getUnitType(), ePotentialUpgradeClass)) {
						getGroup()->unloadAll();

						if (kOwner.AI_unitImpassableCount(getUnitType()) > 0) {
							scrap();
							return;
						} else {
							CvArea* pWaterArea = plot()->waterArea();
							FAssert(pWaterArea != NULL);
							if (pWaterArea != NULL) {
								if (kOwner.AI_totalUnitAIs(UNITAI_EXPLORE_SEA) == 0) {
									if (kOwner.AI_unitValue(this, UNITAI_EXPLORE_SEA, pWaterArea) > 0) {
										AI_setUnitAIType(UNITAI_EXPLORE_SEA);
										AI_exploreSeaMove();
										return;
									}
								}

								if (kOwner.AI_totalUnitAIs(UNITAI_SPY_SEA) == 0) {
									if (kOwner.AI_unitValue(this, UNITAI_SPY_SEA, area()) > 0) {
										AI_setUnitAIType(UNITAI_SPY_SEA);
										AI_spySeaMove();
										return;
									}
								}

								if (kOwner.AI_totalUnitAIs(UNITAI_MISSIONARY_SEA) == 0) {
									if (kOwner.AI_unitValue(this, UNITAI_MISSIONARY_SEA, area()) > 0) {
										AI_setUnitAIType(UNITAI_MISSIONARY_SEA);
										AI_missionarySeaMove();
										return;
									}
								}

								if (kOwner.AI_unitValue(this, UNITAI_ATTACK_SEA, pWaterArea) > 0) {
									AI_setUnitAIType(UNITAI_ATTACK_SEA);
									AI_attackSeaMove();
									return;
								}
							}
						}
					}
				}
			}
		}
	}

	if (AI_retreatToCity(true)) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_missionarySeaMove() {
	PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	if (plot()->isCity(true)) {
		int iOurDefense = kOwner.AI_localDefenceStrength(plot(), getTeam(), DOMAIN_LAND, 0);
		int iEnemyOffense = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2);

		if (getDamage() > 0)	// extra risk to leaving when wounded
		{
			iOurDefense *= 2;
		}

		if (iEnemyOffense > iOurDefense / 2) // was 1 vs 1/4
		{
			// Retreat to primary area first
			if (AI_retreatToCity(true)) {
				return;
			}

			if (AI_retreatToCity()) {
				return;
			}

			if (AI_safety()) {
				return;
			}
		}
	}

	if (getUnitAICargo(UNITAI_MISSIONARY) > 0) {
		if (AI_specialSeaTransportMissionary()) {
			return;
		}
	} else if (!(getGroup()->hasCargo())) {
		if (AI_pillageRange(4)) {
			return;
		}
	}

	if (!(getGroup()->isFull())) {
		if (kOwner.AI_unitTargetMissionAIs(this, MISSIONAI_LOAD_SPECIAL) > 0) {
			// Wait for units on the way
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
	}

	if (AI_pickup(UNITAI_MISSIONARY, true)) {
		return;
	}

	if (plot()->waterArea() && kOwner.AI_areaMissionAIs(plot()->waterArea(), MISSIONAI_EXPLORE, getGroup()) < kOwner.AI_neededExplorers(plot()->waterArea())) {
		if (AI_explore())
			return;
	}

	if (AI_retreatToCity(true)) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_spySeaMove() {
	PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	CvCity* pCity;

	if (plot()->isCity(true)) {
		int iOurDefense = kOwner.AI_localDefenceStrength(plot(), getTeam(), DOMAIN_LAND, 0);
		int iEnemyOffense = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2);

		if (getDamage() > 0)	// extra risk to leaving when wounded
		{
			iOurDefense *= 2;
		}

		if (iEnemyOffense > iOurDefense / 2) // was 1 vs 1/4
		{
			// Retreat to primary area first
			if (AI_retreatToCity(true)) {
				return;
			}

			if (AI_retreatToCity()) {
				return;
			}

			if (AI_safety()) {
				return;
			}
		}
	}

	if (getUnitAICargo(UNITAI_SPY) > 0) {
		if (AI_specialSeaTransportSpy()) {
			return;
		}

		pCity = plot()->getPlotCity();

		if (pCity != NULL) {
			if (pCity->getOwnerINLINE() == getOwnerINLINE()) {
				getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY, pCity->plot());
				return;
			}
		}
	} else if (!(getGroup()->hasCargo())) {
		if (AI_pillageRange(5)) {
			return;
		}
	}

	if (!(getGroup()->isFull())) {
		if (kOwner.AI_unitTargetMissionAIs(this, MISSIONAI_LOAD_SPECIAL) > 0) {
			// Wait for units on the way
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
	}

	if (AI_pickup(UNITAI_SPY, true)) {
		return;
	}

	if (AI_retreatToCity(true)) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_carrierSeaMove() {
	PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	if (plot()->isCity(true)) {
		int iOurDefense = kOwner.AI_localDefenceStrength(plot(), getTeam(), DOMAIN_LAND, 0);
		int iEnemyOffense = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2);

		if (getDamage() > 0)	// extra risk to leaving when wounded
		{
			iOurDefense *= 2;
		}

		if (iEnemyOffense > iOurDefense / 2) // was 1 vs 1/4
		{
			if (AI_retreatToCity(true)) {
				return;
			}

			if (AI_retreatToCity()) {
				return;
			}

			if (AI_safety()) {
				return;
			}
		}
	}

	if (AI_heal(50)) {
		return;
	}

	if (!isEnemy(plot()->getTeam())) {
		if (kOwner.AI_unitTargetMissionAIs(this, MISSIONAI_GROUP) > 0) {
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
	} else {
		if (AI_seaBombardRange(1)) {
			return;
		}
	}

	if (AI_group(UNITAI_CARRIER_SEA, -1, /*iMaxOwnUnitAI*/ 1)) {
		return;
	}

	if (getGroup()->countNumUnitAIType(UNITAI_ATTACK_SEA) + getGroup()->countNumUnitAIType(UNITAI_ESCORT_SEA) == 0) {
		if (plot()->isCity() && plot()->getOwnerINLINE() == getOwnerINLINE()) {
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
		if (AI_retreatToCity()) {
			return;
		}
	}

	if (getCargo() > 0) {
		if (AI_carrierSeaTransport()) {
			return;
		}

		if (AI_blockade()) {
			return;
		}

		if (AI_shadow(UNITAI_ASSAULT_SEA)) {
			return;
		}
	}

	if (AI_travelToUpgradeCity()) {
		return;
	}

	if (AI_retreatToCity(true)) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_missileCarrierSeaMove() {
	PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	bool bIsStealth = (getInvisibleType() != NO_INVISIBLE);

	if (plot()->isCity(true)) {
		int iOurDefense = kOwner.AI_localDefenceStrength(plot(), getTeam(), DOMAIN_LAND, 0);
		int iEnemyOffense = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2);

		if (getDamage() > 0)	// extra risk to leaving when wounded
		{
			iOurDefense *= 2;
		}

		if (iEnemyOffense > iOurDefense / 2) // was 1 vs 1/4
		{
			if (AI_shadow(UNITAI_ASSAULT_SEA, 1, 50, false, true, baseMoves())) {
				return;
			}

			if (AI_retreatToCity()) {
				return;
			}

			if (AI_safety()) {
				return;
			}
		}
	}

	if (plot()->isCity() && plot()->getTeam() == getTeam()) {
		if (AI_heal()) {
			return;
		}
	}

	if (((plot()->getTeam() != getTeam()) && getGroup()->hasCargo()) || getGroup()->AI_isFull()) {
		if (bIsStealth) {
			if (AI_carrierSeaTransport()) {
				return;
			}
		} else {
			if (AI_shadow(UNITAI_ASSAULT_SEA, 1, 50, true, false, 12)) {
				return;
			}

			if (AI_carrierSeaTransport()) {
				return;
			}
		}
	}

	if (AI_retreatToCity()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
}


void CvUnitAI::AI_attackAirMove() {
	PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	CvCity* pCity = plot()->getPlotCity();

	// Check for sufficient defenders to stay
	int iDefenders = plot()->plotCount(PUF_canDefend, -1, -1, plot()->getOwner());

	int iAttackAirCount = plot()->plotCount(PUF_canAirAttack, -1, -1, NO_PLAYER, getTeam());
	iAttackAirCount += 2 * plot()->plotCount(PUF_isUnitAIType, UNITAI_ICBM, -1, NO_PLAYER, getTeam());

	if (plot()->isCoastalLand(GC.getMIN_WATER_SIZE_FOR_OCEAN())) {
		iDefenders -= 1;
	}

	if (pCity != NULL) {
		if (pCity->getDefenseModifier(true) < 40) {
			iDefenders -= 1;
		}

		if (pCity->getOccupationTimer() > 1) {
			iDefenders -= 1;
		}
	}

	if (iAttackAirCount > iDefenders) {
		if (AI_airOffensiveCity()) {
			return;
		}
	}

	// Check for direct threat to current base
	if (plot()->isCity(true)) {
		int iOurDefense = kOwner.AI_localDefenceStrength(plot(), getTeam(), DOMAIN_LAND, 0);
		int iEnemyOffense = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2);

		if (iEnemyOffense > iOurDefense || iOurDefense == 0) {
			// Too risky, pull back
			if (AI_airOffensiveCity()) {
				return;
			}

			if (canAirDefend()) {
				if (AI_airDefensiveCity()) {
					return;
				}
			}
		} else if (iEnemyOffense > iOurDefense / 3) {
			if (getDamage() == 0) {
				if (collateralDamage() == 0 && canAirDefend()) {
					if (pCity != NULL) {
						// Check for whether city needs this unit to air defend
						if (!(pCity->AI_isAirDefended(true, -1))) {
							getGroup()->pushMission(MISSION_AIRPATROL);
							return;
						}
					}
				}

				// Attack the invaders!
				if (AI_defendBaseAirStrike()) {
					return;
				}

				if (AI_airStrike()) {
					return;
				}

				// If no targets, no sense staying in risky place
				if (AI_airOffensiveCity()) {
					return;
				}

				if (canAirDefend()) {
					if (AI_airDefensiveCity()) {
						return;
					}
				}
			}

			if (healTurns(plot()) > 1) {
				// If very damaged, no sense staying in risky place
				if (AI_airOffensiveCity()) {
					return;
				}

				if (canAirDefend()) {
					if (AI_airDefensiveCity()) {
						return;
					}
				}
			}

		}
	}

	if (getDamage() > 0) {
		getGroup()->pushMission(MISSION_SKIP);
		return;
	}

	CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());
	CvArea* pArea = area();
	int iAttackValue = kPlayer.AI_unitValue(this, UNITAI_ATTACK_AIR, pArea);
	int iCarrierValue = kPlayer.AI_unitValue(this, UNITAI_CARRIER_AIR, pArea);
	if (iCarrierValue > 0) {
		int iCarriers = kPlayer.AI_totalUnitAIs(UNITAI_CARRIER_SEA);
		if (iCarriers > 0) {
			UnitTypes eBestCarrierUnit = NO_UNIT;
			kPlayer.AI_bestAreaUnitAIValue(UNITAI_CARRIER_SEA, NULL, &eBestCarrierUnit);
			if (eBestCarrierUnit != NO_UNIT) {
				int iCarrierAirNeeded = iCarriers * GC.getUnitInfo(eBestCarrierUnit).getCargoSpace();
				if (kPlayer.AI_totalUnitAIs(UNITAI_CARRIER_AIR) < iCarrierAirNeeded) {
					AI_setUnitAIType(UNITAI_CARRIER_AIR);
					getGroup()->pushMission(MISSION_SKIP);
					return;
				}
			}
		}
	}

	int iDefenseValue = kPlayer.AI_unitValue(this, UNITAI_DEFENSE_AIR, pArea);
	if (iDefenseValue > iAttackValue) {
		if (kPlayer.AI_bestAreaUnitAIValue(UNITAI_ATTACK_AIR, pArea) > iAttackValue) {
			AI_setUnitAIType(UNITAI_DEFENSE_AIR);
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
	}

	bool bDefensive = false;
	if (pArea != NULL) {
		bDefensive = pArea->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE;
	}

	if (GC.getGameINLINE().getSorenRandNum(4, "AI Air Attack Move") == 0) {
		// only moves unit in a fort
		if (AI_travelToUpgradeCity()) {
			return;
		}
	}

	if (AI_airStrike()) {
		return;
	}
	// switched probabilities from original bts. If we're on the offense, we don't want to smash up too many improvements...
	// soon they will be _our_ improvements.
	if (GC.getGameINLINE().getSorenRandNum(bDefensive ? 4 : 6, "AI Air Attack Move") == 0) {
		if (AI_airBombPlots()) {
			return;
		}
	}

	if (canAirAttack()) {
		if (AI_airOffensiveCity()) {
			return;
		}
	} else {
		if (canAirDefend()) {
			if (AI_airDefensiveCity()) {
				return;
			}
		}
	}

	// BBAI TODO: Support friendly attacks on common enemies, if low risk?

	if (canAirDefend()) {
		if (bDefensive || GC.getGameINLINE().getSorenRandNum(2, "AI Air Attack Move") == 0) {
			getGroup()->pushMission(MISSION_AIRPATROL);
			return;
		}
	}

	if (canRecon(plot())) {
		if (AI_exploreAir()) {
			return;
		}
	}


	getGroup()->pushMission(MISSION_SKIP);
	return;
}

// This function has been rewritten for K-Mod. (The new version is much simplier.)
void CvUnitAI::AI_defenseAirMove() {
	PROFILE_FUNC();

	if (!plot()->isCity(true)) {
		//FAssertMsg(false, "defenseAir units are expected to stay in cities/forts");
		if (AI_airDefensiveCity())
			return;
	}

	CvCity* pCity = plot()->getPlotCity();

	int iEnemyOffense = GET_PLAYER(getOwnerINLINE()).AI_localAttackStrength(plot(), NO_TEAM);
	int iOurDefense = GET_PLAYER(getOwnerINLINE()).AI_localDefenceStrength(plot(), getTeam());

	if (iEnemyOffense > 2 * iOurDefense || iOurDefense == 0) {
		// Too risky, pull out
		if (AI_airDefensiveCity()) {
			return;
		}
	}

	int iDefNeeded = pCity ? pCity->AI_neededAirDefenders() : 0;
	int iDefHere = plot()->plotCount(PUF_isAirIntercept, -1, -1, NO_PLAYER, getTeam()) - (PUF_isAirIntercept(this, -1, -1) ? 1 : 0);
	FAssert(iDefHere >= 0);

	if (canAirDefend() && iEnemyOffense < iOurDefense && iDefHere < iDefNeeded / 2) {
		getGroup()->pushMission(MISSION_AIRPATROL);
		return;
	}

	if (iEnemyOffense > (getDamage() == 0 ? iOurDefense / 3 : iOurDefense)) {
		// Attack the invaders!
		if (AI_defendBaseAirStrike()) {
			return;
		}
	}

	if (getDamage() > maxHitPoints() / 3) {
		getGroup()->pushMission(MISSION_SKIP);
		return;
	}

	if (iEnemyOffense == 0 && GC.getGameINLINE().getSorenRandNum(4, "AI Air Defense Move") == 0) {
		if (AI_travelToUpgradeCity()) {
			return;
		}
	}

	bool bDefensive = area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE;
	bool bOffensive = area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE;
	bool bTriedAirStrike = false;

	if (!bTriedAirStrike && GC.getGameINLINE().getSorenRandNum(3, "AI_defenseAirMove airstrike") <= (bOffensive ? 1 : 0) - (bDefensive ? 1 : 0)) {
		if (AI_airStrike())
			return;
		bTriedAirStrike = true;
	}

	if (canAirDefend() && iDefHere < iDefNeeded * 2 / 3) {
		getGroup()->pushMission(MISSION_AIRPATROL);
		return;
	}

	if (!bTriedAirStrike && GC.getGameINLINE().getSorenRandNum(3, "AI_defenseAirMove airstrike2") <= (bOffensive ? 1 : 0) - (bDefensive ? 1 : 0)) {
		if (AI_airStrike())
			return;
		bTriedAirStrike = true;
	}

	if (AI_airDefensiveCity()) // check if there's a better city to be in
	{
		return;
	}

	if (canAirDefend() && iDefHere < iDefNeeded) {
		getGroup()->pushMission(MISSION_AIRPATROL);
		return;
	}

	if (canRecon(plot())) {
		if (GC.getGame().getSorenRandNum(bDefensive ? 6 : 3, "AI defensive air recon") == 0) {
			if (AI_exploreAir()) {
				return;
			}
		}
	}

	if (canAirDefend()) {
		getGroup()->pushMission(MISSION_AIRPATROL);
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_carrierAirMove() {
	PROFILE_FUNC();

	// XXX maybe protect land troops?

	if (getDamage() > 0) {
		getGroup()->pushMission(MISSION_SKIP);
		return;
	}

	if (isCargo()) {
		if (canAirDefend()) {
			int iActiveInterceptors = plot()->plotCount(PUF_isAirIntercept, -1, -1, getOwnerINLINE());
			if (GC.getGameINLINE().getSorenRandNum(16, "AI Air Carrier Move") < 4 - std::min(3, iActiveInterceptors)) {
				getGroup()->pushMission(MISSION_AIRPATROL);
				return;
			}
		}
		if (AI_airStrike()) {
			return;
		}
		if (AI_airBombPlots()) {
			return;
		}

		if (AI_travelToUpgradeCity()) {
			return;
		}

		if (canAirDefend()) {
			getGroup()->pushMission(MISSION_AIRPATROL);
			return;
		}
		getGroup()->pushMission(MISSION_SKIP);
		return;
	}

	if (AI_airCarrier()) {
		return;
	}

	if (AI_airDefensiveCity()) {
		return;
	}

	if (canAirDefend()) {
		getGroup()->pushMission(MISSION_AIRPATROL);
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_missileAirMove() {
	PROFILE_FUNC();

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	CvCity* pCity = plot()->getPlotCity();

	// includes forts
	if (!isCargo() && plot()->isCity(true)) {
		int iOurDefense = kOwner.AI_localDefenceStrength(plot(), getTeam(), DOMAIN_LAND, 0);
		int iEnemyOffense = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2);

		if (iEnemyOffense > (iOurDefense / 2) || iOurDefense == 0) {
			if (AI_airOffensiveCity()) {
				return;
			}
		}
	}

	if (isCargo()) {
		int iRand = GC.getGameINLINE().getSorenRandNum(3, "AI Air Missile plot bombing");
		if (iRand != 0) {
			if (AI_airBombPlots()) {
				return;
			}
		}

		if (AI_airStrike()) {
			return;
		}

		if (AI_airBombPlots()) {
			return;
		}

		getGroup()->pushMission(MISSION_SKIP);
		return;
	}

	if (AI_airStrike()) {
		return;
	}

	if (AI_missileLoad(UNITAI_MISSILE_CARRIER_SEA)) {
		return;
	}

	if (AI_missileLoad(UNITAI_RESERVE_SEA, 1)) {
		return;
	}

	if (AI_missileLoad(UNITAI_ATTACK_SEA, 1)) {
		return;
	}

	if (!isCargo()) {
		if (AI_airOffensiveCity()) {
			return;
		}
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_networkAutomated() {
	FAssertMsg(canBuildRoute(), "canBuildRoute is expected to be true");

	if (!(getGroup()->canDefend())) {
		if (GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(plot())) {
			if (AI_retreatToCity()) // XXX maybe not do this??? could be working productively somewhere else...
			{
				return;
			}
		}
	}

	if (AI_improveBonus())
		return;
	// K-Mod end. (I don't think AI_connectBonus() is useful either, but I haven't looked closely enough to remove it.)

	if (AI_connectBonus()) {
		return;
	}

	if (AI_connectCity()) {
		return;
	}

	if (AI_routeTerritory(true)) {
		return;
	}

	if (AI_connectBonus(false)) {
		return;
	}

	if (AI_routeCity()) {
		return;
	}

	if (AI_routeTerritory()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_cityAutomated() {
	if (!(getGroup()->canDefend())) {
		if (GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(plot())) {
			if (AI_retreatToCity()) // XXX maybe not do this??? could be working productively somewhere else...
			{
				return;
			}
		}
	}

	CvCity* pCity = NULL;

	if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
		pCity = plot()->getWorkingCity();
	}

	if (pCity == NULL) {
		pCity = GC.getMapINLINE().findCity(getX_INLINE(), getY_INLINE(), getOwnerINLINE()); // XXX do team???
	}

	if (pCity != NULL) {
		if (AI_improveCity(pCity)) {
			return;
		}
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


// XXX make sure we include any new UnitAITypes...
int CvUnitAI::AI_promotionValue(PromotionTypes ePromotion) {
	const CvPromotionInfo& kPromotion = GC.getPromotionInfo(ePromotion);
	if (kPromotion.isLeader()) {
		// Don't consume the leader as a regular promotion
		return 0;
	}

	int iValue = 0;
	// Worker promotions
	if (AI_getUnitAIType() == UNITAI_WORKER || AI_getUnitAIType() == UNITAI_GATHERER) {
		if (kPromotion.getWorkRateModifier() > 0) {
			iValue += kPromotion.getWorkRateModifier() * 5;
		}

		CvCity* pCity = plot()->getWorkingCity();
		BuildTypes eBestPlotBuild = NO_BUILD;
		FeatureTypes eBestPlotFeature = NO_FEATURE;
		if (pCity) {
			// If we are in a city radius then find the best build/feature combination
			// Only need to check this once
			CvPlot* pBestPlot;
			if (AI_bestCityBuild(pCity, &pBestPlot, &eBestPlotBuild)) {
				eBestPlotFeature = pBestPlot->getFeatureType();
			}
		}
		for (int i = 0; i < kPromotion.getNumBuildLeaveFeatures(); i++) {
			std::pair<int, int> pPair = kPromotion.getBuildLeaveFeature(i);
			BuildTypes eBuild = (BuildTypes)pPair.first;
			FeatureTypes eFeature = (FeatureTypes)pPair.second;

			// Base value
			iValue += 25;

			// If this promotion assists with the best build type then add extra value as this is probably
			//  a common build irrespective of whether the plot has the feature.
			if (eBestPlotBuild == eBuild) {
				iValue += 25;
				// If it is also for the correct feature type then go for it!
				if (eBestPlotFeature == eFeature) {
					iValue += 250;
				}
			}
		}

		return iValue;
	}

	// These promotions only really have value in the pre-settled stage when they shine
	if (kPromotion.getSalvageModifier() != 0) {
		if (!GET_PLAYER(getOwnerINLINE()).isCivSettled()) {
			iValue += 40;
		} else {
			iValue++;
		}
	}

	if (kPromotion.isBlitz()) {
		if ((AI_getUnitAIType() == UNITAI_RESERVE && baseMoves() > 1) ||
			AI_getUnitAIType() == UNITAI_PARADROP) {
			iValue += 10;
		} else {
			iValue += 2;
		}
	}

	if (kPromotion.isAmphib()) {
		if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
			(AI_getUnitAIType() == UNITAI_ATTACK_CITY)) {
			iValue += 5;
		} else {
			iValue++;
		}
	}

	if (kPromotion.isRiver()) {
		if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
			(AI_getUnitAIType() == UNITAI_ATTACK_CITY)) {
			iValue += 5;
		} else {
			iValue++;
		}
	}

	if (kPromotion.isLoyal()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 30;
		} else {
			iValue += 5;
		}
	}

	if (AI_getUnitAIType() == UNITAI_SPY) {
		// If we are already going down the evasion line in promotions then give it extra weight
		iValue += (kPromotion.getSpyEvasionChange() * 2) + getSpyEvasionChanceExtra();
	}

	if (kPromotion.getSpyPreparationModifier()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += kPromotion.getSpyPreparationModifier() * 20;
		}
	}

	if (kPromotion.getSpyPoisonModifier()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyDestroyImprovementChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.isSpyRadiation()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 30;
		}
	}

	if (kPromotion.getSpyDiploPenaltyChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyNukeCityChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpySwitchCivicChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpySwitchReligionChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyDisablePowerChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyEscapeChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 20;
		}
	}

	if (kPromotion.getSpyInterceptChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			// If we are already going down the interception line in promotions then give it extra weight
			iValue += kPromotion.getSpyInterceptChange() + getSpyInterceptChance();
		}
	}

	if (kPromotion.getSpyUnhappyChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyRevoltChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyWarWearinessChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyReligionRemovalChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyCorporationRemovalChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyCultureChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyResearchSabotageChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyDestroyProjectChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyDestroyBuildingChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyDestroyProductionChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyBuyTechChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.getSpyStealTreasuryChange()) {
		if (AI_getUnitAIType() == UNITAI_SPY) {
			iValue += 15;
		}
	}

	if (kPromotion.isEnemyRoute()) {
		if (AI_getUnitAIType() == UNITAI_PILLAGE) {
			iValue += 40;
		} else if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
			(AI_getUnitAIType() == UNITAI_ATTACK_CITY)) {
			iValue += 20;
		} else if (AI_getUnitAIType() == UNITAI_PARADROP) {
			iValue += 10;
		} else {
			iValue += 4;
		}
	}

	if (kPromotion.isAlwaysHeal()) {
		if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
			(AI_getUnitAIType() == UNITAI_HUNTER) ||
			(AI_getUnitAIType() == UNITAI_ATTACK_CITY) ||
			(AI_getUnitAIType() == UNITAI_PILLAGE) ||
			(AI_getUnitAIType() == UNITAI_COUNTER) ||
			(AI_getUnitAIType() == UNITAI_ATTACK_SEA) ||
			(AI_getUnitAIType() == UNITAI_PIRATE_SEA) ||
			(AI_getUnitAIType() == UNITAI_ESCORT_SEA) ||
			(AI_getUnitAIType() == UNITAI_PARADROP)) {
			iValue += 10;
		} else {
			iValue += 8;
		}
	}

	if (kPromotion.isHillsDoubleMove()) {
		if ((AI_getUnitAIType() == UNITAI_EXPLORE) ||
			(AI_getUnitAIType() == UNITAI_HUNTER)) {
			iValue += 20;
		} else {
			iValue += 10;
		}
	}

	if (kPromotion.isCanMovePeaks()) {
		for (TechTypes eTech = (TechTypes)0; eTech < GC.getNumTechInfos(); eTech = (TechTypes)(eTech + 1)) {
			if (!GET_TEAM(getTeam()).isHasTech(eTech)) {
				if (GC.getTechInfo(eTech).isCanPassPeaks()) {
					iValue += 75;
				}
			}
		}
	}

	// We want negative values for this promotion
	int iTemp = -(kPromotion.getEnemyMoraleModifier());
	if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
		(AI_getUnitAIType() == UNITAI_ATTACK_CITY) ||
		(AI_getUnitAIType() == UNITAI_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_CITY_DEFENSE) ||
		(AI_getUnitAIType() == UNITAI_CITY_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_CITY_SPECIAL) ||
		(AI_getUnitAIType() == UNITAI_PARADROP)) {
		iValue += (iTemp * 2);
	} else if ((AI_getUnitAIType() == UNITAI_ATTACK_SEA)
		|| (AI_getUnitAIType() == UNITAI_PIRATE_SEA)
		|| (AI_getUnitAIType() == UNITAI_ASSAULT_SEA)) {
		iValue += (iTemp * 4);
	} else {
		iValue += iTemp;
	}

	iTemp = kPromotion.getExtraMorale();
	if (getMorale() < 100) {
		iValue += iTemp * 4;
	} else if (getMorale() < 250) {
		iValue += iTemp * 2;
	} else if (getMorale() < 500) {
		iValue += iTemp;
	}

	if (kPromotion.isUnitTerritoryUnbound() && getRangeType() == UNITRANGE_TERRITORY) {
		if ((AI_getUnitAIType() == UNITAI_EXPLORE) ||
			(AI_getUnitAIType() == UNITAI_HUNTER)) {
			iValue += 20;
		} else {
			iValue += 10;
		}
	}

	if (kPromotion.isUnitRangeUnbound() && getRangeType() == UNITRANGE_RANGE) {
		if ((AI_getUnitAIType() == UNITAI_EXPLORE) ||
			(AI_getUnitAIType() == UNITAI_HUNTER)) {
			iValue += 20;
		} else {
			iValue += 10;
		}
	}

	if (kPromotion.isImmuneToFirstStrikes() && !immuneToFirstStrikes()) {
		if ((AI_getUnitAIType() == UNITAI_ATTACK_CITY)) {
			iValue += 12;
		} else if ((AI_getUnitAIType() == UNITAI_ATTACK)) {
			iValue += 8;
		} else {
			iValue += 4;
		}
	}

	iTemp = kPromotion.getEnslaveCountChange();
	if (isSlaver() && iTemp > 0) {
		// This line of promotions should be a priority for slavers
		iValue += (iTemp * 50);
	}

	// Generic see invisibles
	iTemp = kPromotion.getNumSeeInvisibleTypes();
	if ((AI_getUnitAIType() == UNITAI_RESERVE) ||
		(AI_getUnitAIType() == UNITAI_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_CITY_DEFENSE) ||
		(AI_getUnitAIType() == UNITAI_CITY_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_CITY_SPECIAL) ||
		(AI_getUnitAIType() == UNITAI_ATTACK)) {
		iValue += (iTemp * 4);
	} else {
		iValue += (iTemp * 2);
	}

	// Anti-slavery specific value if we are in our cultural borders, remember being hit by slavers in this area and don't have slaver visibility
	InvisibleTypes eSlaverInvisibility = (InvisibleTypes)GC.getInfoTypeForString("INVISIBLE_SLAVER");
	for (int iI = 0; iI < kPromotion.getNumSeeInvisibleTypes(); ++iI) {
		if (kPromotion.getSeeInvisibleType(iI) == eSlaverInvisibility) {
			if ((AI_getUnitAIType() == UNITAI_CITY_DEFENSE || AI_getUnitAIType() == UNITAI_ATTACK)
				&& (area()->getSlaveMemoryPerPlayer(getOwnerINLINE()) > 0 && plot()->getOwnerINLINE() == getOwnerINLINE() && plot()->getInvisibleVisibilityCount(getTeam(), eSlaverInvisibility) < 1)) {
				iValue += 50;
			}
		}
	}

	iTemp = kPromotion.getVisibilityChange();
	if ((AI_getUnitAIType() == UNITAI_EXPLORE_SEA) ||
		(AI_getUnitAIType() == UNITAI_EXPLORE) ||
		(AI_getUnitAIType() == UNITAI_HUNTER)) {
		iValue += (iTemp * 40);
	} else if (AI_getUnitAIType() == UNITAI_PIRATE_SEA) {
		iValue += (iTemp * 20);
	} else if (AI_getUnitAIType() == UNITAI_SPY) {
		iValue += (iTemp * 10);
	}

	iTemp = kPromotion.getMovesChange();
	if ((AI_getUnitAIType() == UNITAI_ATTACK_SEA) ||
		(AI_getUnitAIType() == UNITAI_PIRATE_SEA) ||
		(AI_getUnitAIType() == UNITAI_RESERVE_SEA) ||
		(AI_getUnitAIType() == UNITAI_ESCORT_SEA) ||
		(AI_getUnitAIType() == UNITAI_EXPLORE_SEA) ||
		(AI_getUnitAIType() == UNITAI_ASSAULT_SEA) ||
		(AI_getUnitAIType() == UNITAI_SETTLER_SEA) ||
		(AI_getUnitAIType() == UNITAI_HUNTER) ||
		(AI_getUnitAIType() == UNITAI_PILLAGE) ||
		(AI_getUnitAIType() == UNITAI_ATTACK) ||
		(AI_getUnitAIType() == UNITAI_PARADROP)) {
		iValue += (iTemp * 20);
	} else {
		iValue += (iTemp * 4);
	}

	iTemp = kPromotion.getUnitRangeChange();
	if (getRangeType() == UNITRANGE_RANGE) {
		iValue += (iTemp * 20);
	} else {
		iValue += (iTemp * 4);
	}

	iTemp = kPromotion.getUnitRangeModifier() + 100;
	iTemp /= 100;
	if (getRangeType() == UNITRANGE_RANGE) {
		iValue += (iTemp * 20);
	} else {
		iValue += (iTemp * 5);
	}

	iTemp = kPromotion.getMoveDiscountChange();
	if (AI_getUnitAIType() == UNITAI_PILLAGE) {
		iValue += (iTemp * 10);
	} else {
		iValue += (iTemp * 2);
	}

	iTemp = kPromotion.getAirRangeChange();
	if (AI_getUnitAIType() == UNITAI_ATTACK_AIR ||
		AI_getUnitAIType() == UNITAI_CARRIER_AIR) {
		iValue += (iTemp * 20);
	} else if (AI_getUnitAIType() == UNITAI_DEFENSE_AIR) {
		iValue += (iTemp * 10);
	}

	iTemp = kPromotion.getInterceptChange();
	if (AI_getUnitAIType() == UNITAI_DEFENSE_AIR) {
		iValue += (iTemp * 3);
	} else if (AI_getUnitAIType() == UNITAI_CITY_SPECIAL || AI_getUnitAIType() == UNITAI_CARRIER_AIR) {
		iValue += (iTemp * 2);
	} else {
		iValue += (iTemp / 10);
	}

	iTemp = kPromotion.getEvasionChange();
	if (AI_getUnitAIType() == UNITAI_ATTACK_AIR || AI_getUnitAIType() == UNITAI_CARRIER_AIR) {
		iValue += (iTemp * 3);
	} else {
		iValue += (iTemp / 10);
	}

	iTemp = kPromotion.getFirstStrikesChange() * 2;
	iTemp += kPromotion.getChanceFirstStrikesChange();
	if ((AI_getUnitAIType() == UNITAI_RESERVE) ||
		(AI_getUnitAIType() == UNITAI_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_CITY_DEFENSE) ||
		(AI_getUnitAIType() == UNITAI_CITY_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_CITY_SPECIAL) ||
		(AI_getUnitAIType() == UNITAI_SLAVER) ||
		(AI_getUnitAIType() == UNITAI_HUNTER) ||
		(AI_getUnitAIType() == UNITAI_ATTACK)) {
		iTemp *= 8;
		int iExtra = getExtraChanceFirstStrikes() + getExtraFirstStrikes() * 2;
		iTemp *= 100 + iExtra * 15;
		iTemp /= 100;
		iValue += iTemp;
	} else {
		iValue += (iTemp * 5);
	}


	iTemp = kPromotion.getWithdrawalChange();
	if (iTemp != 0) {
		int iExtra = (m_pUnitInfo->getWithdrawalProbability() + (getExtraWithdrawal() * 4));
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		if ((AI_getUnitAIType() == UNITAI_ATTACK_CITY)) {
			iValue += (iTemp * 4) / 3;
		} else if ((AI_getUnitAIType() == UNITAI_COLLATERAL) ||
			(AI_getUnitAIType() == UNITAI_RESERVE) ||
			(AI_getUnitAIType() == UNITAI_HUNTER) ||
			(AI_getUnitAIType() == UNITAI_RESERVE_SEA) ||
			getLeaderUnitType() != NO_UNIT) {
			iValue += iTemp * 1;
		} else {
			iValue += (iTemp / 4);
		}
	}

	iTemp = kPromotion.getCollateralDamageChange();
	if (iTemp != 0) {
		int iExtra = (getExtraCollateralDamage());//collateral has no strong synergy (not like retreat)
		iTemp *= (100 + iExtra);
		iTemp /= 100;

		if (AI_getUnitAIType() == UNITAI_COLLATERAL) {
			iValue += (iTemp * 1);
		} else if (AI_getUnitAIType() == UNITAI_ATTACK_CITY) {
			iValue += ((iTemp * 2) / 3);
		} else {
			iValue += (iTemp / 8);
		}
	}

	iTemp = kPromotion.getBombardRateChange();
	if (AI_getUnitAIType() == UNITAI_ATTACK_CITY) {
		iValue += (iTemp * 2);
	} else {
		iValue += (iTemp / 8);
	}

	iTemp = kPromotion.getEnemyHealChange();
	if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
		(AI_getUnitAIType() == UNITAI_SLAVER) ||
		(AI_getUnitAIType() == UNITAI_PILLAGE) ||
		(AI_getUnitAIType() == UNITAI_ATTACK_SEA) ||
		(AI_getUnitAIType() == UNITAI_PARADROP) ||
		(AI_getUnitAIType() == UNITAI_PIRATE_SEA)) {
		iValue += (iTemp / 4);
	} else {
		iValue += (iTemp / 8);
	}

	iTemp = kPromotion.getNeutralHealChange();
	iValue += (iTemp / 8);

	iTemp = kPromotion.getFriendlyHealChange();
	if ((AI_getUnitAIType() == UNITAI_CITY_DEFENSE) ||
		(AI_getUnitAIType() == UNITAI_CITY_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_CITY_SPECIAL)) {
		iValue += (iTemp / 4);
	} else {
		iValue += (iTemp / 8);
	}

	if (getDamage() > 0 || ((AI_getBirthmark() % 8 == 0) && (AI_getUnitAIType() == UNITAI_COUNTER ||
		AI_getUnitAIType() == UNITAI_PILLAGE ||
		AI_getUnitAIType() == UNITAI_ATTACK_CITY ||
		AI_getUnitAIType() == UNITAI_RESERVE ||
		AI_getUnitAIType() == UNITAI_PIRATE_SEA ||
		AI_getUnitAIType() == UNITAI_RESERVE_SEA ||
		AI_getUnitAIType() == UNITAI_ASSAULT_SEA))) {
		iTemp = kPromotion.getSameTileHealChange() + getSameTileHeal();
		int iExtra = getSameTileHeal();

		iTemp *= (100 + iExtra * 5);
		iTemp /= 100;

		if (iTemp > 0) {
			if (healRate(plot(), false, true) < iTemp) {
				iValue += iTemp * ((getGroup()->getNumUnits() > 4) ? 4 : 2);
			} else {
				iValue += (iTemp / 8);
			}
		}

		iTemp = kPromotion.getAdjacentTileHealChange();
		iExtra = getAdjacentTileHeal();
		iTemp *= (100 + iExtra * 5);
		iTemp /= 100;
		if (getSameTileHeal() >= iTemp) {
			iValue += (iTemp * ((getGroup()->getNumUnits() > 9) ? 4 : 2));
		} else {
			iValue += (iTemp / 4);
		}
	}

	// try to use Warlords to create super-medic units
	if (kPromotion.getAdjacentTileHealChange() > 0 || kPromotion.getSameTileHealChange() > 0) {
		// K-Mod, I've changed the way we work out if we are a leader or not.
		// The original method would break if there was more than one "leader" promotion)
		for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++) {
			if (GC.getPromotionInfo((PromotionTypes)iI).isLeader() && isHasPromotion((PromotionTypes)iI)) {
				iValue += kPromotion.getAdjacentTileHealChange() + kPromotion.getSameTileHealChange();
				break;
			}
		}
	}

	iTemp = kPromotion.getCombatPercent();
	if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
		(AI_getUnitAIType() == UNITAI_SLAVER) ||
		(AI_getUnitAIType() == UNITAI_HUNTER) ||
		(AI_getUnitAIType() == UNITAI_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_CITY_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_ATTACK_SEA) ||
		(AI_getUnitAIType() == UNITAI_PARADROP) ||
		(AI_getUnitAIType() == UNITAI_PIRATE_SEA) ||
		(AI_getUnitAIType() == UNITAI_RESERVE_SEA) ||
		(AI_getUnitAIType() == UNITAI_ESCORT_SEA) ||
		(AI_getUnitAIType() == UNITAI_CARRIER_SEA) ||
		(AI_getUnitAIType() == UNITAI_ATTACK_AIR) ||
		(AI_getUnitAIType() == UNITAI_CARRIER_AIR)) {
		iValue += (iTemp * 2);
	} else {
		iValue += (iTemp * 1);
	}

	iTemp = kPromotion.getCityAttackPercent();
	if (iTemp != 0) {
		if (m_pUnitInfo->getUnitAIType(UNITAI_ATTACK) || m_pUnitInfo->getUnitAIType(UNITAI_ATTACK_CITY) || m_pUnitInfo->getUnitAIType(UNITAI_ATTACK_CITY_LEMMING)) {
			int iExtra = (m_pUnitInfo->getCityAttackModifier() + (getExtraCityAttackPercent() * 2));
			iTemp *= (100 + iExtra);
			iTemp /= 100;
			if (AI_getUnitAIType() == UNITAI_ATTACK_CITY) {
				iValue += (iTemp * 1);
			} else {
				iValue -= iTemp / 4;
			}
		}
	}

	iTemp = kPromotion.getCityDefensePercent();
	if (iTemp != 0) {
		if ((AI_getUnitAIType() == UNITAI_CITY_DEFENSE) ||
			(AI_getUnitAIType() == UNITAI_CITY_SPECIAL)) {
			int iExtra = m_pUnitInfo->getCityDefenseModifier() + (getExtraCityDefensePercent() * 2);
			iValue += ((iTemp * (100 + iExtra)) / 100);
		} else {
			iValue += (iTemp / 4);
		}
	}

	iTemp = kPromotion.getHillsAttackPercent();
	if (iTemp != 0) {
		int iExtra = getExtraHillsAttackPercent();
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;
		if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
			(AI_getUnitAIType() == UNITAI_SLAVER) ||
			(AI_getUnitAIType() == UNITAI_COUNTER)) {
			iValue += (iTemp / 4);
		} else {
			iValue += (iTemp / 16);
		}
	}

	iTemp = kPromotion.getHillsDefensePercent();
	if (iTemp != 0) {
		int iExtra = (m_pUnitInfo->getHillsDefenseModifier() + (getExtraHillsDefensePercent() * 2));
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		if (AI_getUnitAIType() == UNITAI_CITY_DEFENSE) {
			if (plot()->isCity() && plot()->isHills()) {
				iValue += (iTemp * 4) / 3;
			}
		} else if (AI_getUnitAIType() == UNITAI_COUNTER) {
			if (plot()->isHills()) {
				iValue += (iTemp / 4);
			} else {
				iValue++;
			}
		} else {
			iValue += (iTemp / 16);
		}
	}

	iTemp = kPromotion.getRevoltProtection();
	if ((AI_getUnitAIType() == UNITAI_CITY_DEFENSE) ||
		(AI_getUnitAIType() == UNITAI_CITY_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_CITY_SPECIAL)) {
		if (iTemp > 0) {
			PlayerTypes eOwner = plot()->calculateCulturalOwner();
			if (eOwner != NO_PLAYER && GET_PLAYER(eOwner).getTeam() != GET_PLAYER(getOwnerINLINE()).getTeam()) {
				iValue += (iTemp / 2);
			}
		}
	}

	iTemp = kPromotion.getCollateralDamageProtection();
	if ((AI_getUnitAIType() == UNITAI_CITY_DEFENSE) ||
		(AI_getUnitAIType() == UNITAI_CITY_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_CITY_SPECIAL)) {
		iValue += (iTemp / 3);
	} else if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
		(AI_getUnitAIType() == UNITAI_COUNTER)) {
		iValue += (iTemp / 4);
	} else {
		iValue += (iTemp / 8);
	}

	iTemp = kPromotion.getPillageChange();
	if (AI_getUnitAIType() == UNITAI_PILLAGE ||
		AI_getUnitAIType() == UNITAI_ATTACK_SEA ||
		AI_getUnitAIType() == UNITAI_PIRATE_SEA) {
		iValue += (iTemp / 4);
	} else {
		iValue += (iTemp / 16);
	}

	iTemp = kPromotion.getFoundPopChange();
	if (AI_getUnitAIType() == UNITAI_SETTLE) {
		iValue += (iTemp * 8);
	}


	iTemp = kPromotion.getPlunderChange();
	if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
		(AI_getUnitAIType() == UNITAI_SLAVER) ||
		(AI_getUnitAIType() == UNITAI_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_ATTACK_CITY) ||
		(AI_getUnitAIType() == UNITAI_ATTACK_SEA) ||
		(AI_getUnitAIType() == UNITAI_PARADROP) ||
		(AI_getUnitAIType() == UNITAI_PIRATE_SEA) ||
		(AI_getUnitAIType() == UNITAI_RESERVE_SEA) ||
		(AI_getUnitAIType() == UNITAI_ESCORT_SEA) ||
		(AI_getUnitAIType() == UNITAI_CARRIER_SEA)) {
		iValue += (iTemp / 3);
	} else {
		iValue += (iTemp / 12);
	}

	iTemp = kPromotion.getUpgradeDiscount();
	iValue += (iTemp / 16);

	iTemp = kPromotion.getExperiencePercent();
	if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
		(AI_getUnitAIType() == UNITAI_SLAVER) ||
		(AI_getUnitAIType() == UNITAI_ATTACK_SEA) ||
		(AI_getUnitAIType() == UNITAI_PIRATE_SEA) ||
		(AI_getUnitAIType() == UNITAI_RESERVE_SEA) ||
		(AI_getUnitAIType() == UNITAI_ESCORT_SEA) ||
		(AI_getUnitAIType() == UNITAI_CARRIER_SEA) ||
		(AI_getUnitAIType() == UNITAI_MISSILE_CARRIER_SEA)) {
		iValue += (iTemp * 1);
	} else {
		iValue += (iTemp / 2);
	}

	iTemp = kPromotion.getKamikazePercent();
	if (AI_getUnitAIType() == UNITAI_ATTACK_CITY) {
		iValue += (iTemp / 16);
	} else {
		iValue += (iTemp / 64);
	}

	for (TerrainTypes eTerrain = (TerrainTypes)0; eTerrain < GC.getNumTerrainInfos(); eTerrain = (TerrainTypes)(eTerrain + 1)) {
		iTemp = kPromotion.getTerrainAttackPercent(eTerrain);
		if (iTemp != 0) {
			int iExtra = getExtraTerrainAttackPercent(eTerrain);
			iTemp *= (100 + iExtra * 2);
			iTemp /= 100;
			if (AI_getUnitAIType() == UNITAI_ATTACK || AI_getUnitAIType() == UNITAI_COUNTER || AI_getUnitAIType() == UNITAI_SLAVER) {
				iValue += (iTemp / 4);
			} else {
				iValue += (iTemp / 16);
			}
		}

		iTemp = kPromotion.getTerrainDefensePercent(eTerrain);
		if (iTemp != 0) {
			int iExtra = getExtraTerrainDefensePercent(eTerrain);
			iTemp *= (100 + iExtra);
			iTemp /= 100;
			if (AI_getUnitAIType() == UNITAI_COUNTER) {
				if (plot()->getTerrainType() == eTerrain) {
					iValue += (iTemp / 4);
				} else {
					iValue++;
				}
			} else {
				iValue += (iTemp / 16);
			}
		}

		if (kPromotion.getTerrainDoubleMove(eTerrain)) {
			if (AI_getUnitAIType() == UNITAI_EXPLORE) {
				iValue += 20;
			} else if (AI_getUnitAIType() == UNITAI_ATTACK || AI_getUnitAIType() == UNITAI_PILLAGE) {
				iValue += 10;
			} else {
				iValue += 1;
			}
		}
	}

	for (FeatureTypes eFeature = (FeatureTypes)0; eFeature < GC.getNumFeatureInfos(); eFeature = (FeatureTypes)(eFeature + 1)) {
		iTemp = kPromotion.getFeatureAttackPercent(eFeature);
		if (iTemp != 0) {
			int iExtra = getExtraFeatureAttackPercent(eFeature);
			iTemp *= (100 + iExtra * 2);
			iTemp /= 100;
			if (AI_getUnitAIType() == UNITAI_ATTACK || AI_getUnitAIType() == UNITAI_SLAVER || AI_getUnitAIType() == UNITAI_COUNTER) {
				iValue += (iTemp / 4);
			} else {
				iValue += (iTemp / 16);
			}
		}

		iTemp = kPromotion.getFeatureDefensePercent(eFeature);
		if (iTemp != 0) {
			int iExtra = getExtraFeatureDefensePercent(eFeature);
			iTemp *= (100 + iExtra * 2);
			iTemp /= 100;

			if (!noDefensiveBonus()) {
				if (AI_getUnitAIType() == UNITAI_COUNTER) {
					if (plot()->getFeatureType() == eFeature) {
						iValue += (iTemp / 4);
					} else {
						iValue++;
					}
				} else {
					iValue += (iTemp / 16);
				}
			}
		}

		if (kPromotion.getFeatureDoubleMove(eFeature)) {
			if (AI_getUnitAIType() == UNITAI_EXPLORE) {
				iValue += 20;
			} else if (AI_getUnitAIType() == UNITAI_ATTACK || AI_getUnitAIType() == UNITAI_PILLAGE) {
				iValue += 10;
			} else {
				iValue += 1;
			}
		}
	}

	int iOtherCombat = 0;
	int iSameCombat = 0;

	for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
		if (isUnitCombatType(eUnitCombat)) {
			iSameCombat += unitCombatModifier(eUnitCombat);
		} else {
			iOtherCombat += unitCombatModifier(eUnitCombat);
		}
	}

	for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
		iTemp = kPromotion.getUnitCombatModifierPercent(eUnitCombat);
		int iCombatWeight = 0;
		//Fighting their own kind
		if (isUnitCombatType(eUnitCombat)) {
			if (iSameCombat >= iOtherCombat) {
				iCombatWeight = 70;//"axeman takes formation"
			} else {
				iCombatWeight = 30;
			}
		} else {
			//fighting other kinds
			if (unitCombatModifier(eUnitCombat) > 10) {
				iCombatWeight = 70;//"spearman takes formation"
			} else {
				iCombatWeight = 30;
			}
		}

		iCombatWeight *= GET_PLAYER(getOwnerINLINE()).AI_getUnitCombatWeight(eUnitCombat);
		iCombatWeight /= 100;

		if (AI_getUnitAIType() == UNITAI_COUNTER || AI_getUnitAIType() == UNITAI_CITY_COUNTER) {
			iValue += (iTemp * iCombatWeight) / 50;
		} else if (AI_getUnitAIType() == UNITAI_ATTACK || AI_getUnitAIType() == UNITAI_RESERVE) {
			iValue += (iTemp * iCombatWeight) / 100;
		} else {
			iValue += (iTemp * iCombatWeight) / 200;
		}
	}

	for (int iI = 0; iI < NUM_DOMAIN_TYPES; iI++) {
		//WTF? why float and cast to int?
		iTemp = kPromotion.getDomainModifierPercent(iI);
		if (AI_getUnitAIType() == UNITAI_COUNTER) {
			iValue += (iTemp * 1);
		} else if (AI_getUnitAIType() == UNITAI_ATTACK || AI_getUnitAIType() == UNITAI_SLAVER || AI_getUnitAIType() == UNITAI_RESERVE) {
			iValue += (iTemp / 2);
		} else {
			iValue += (iTemp / 8);
		}
	}

	if (iValue > 0) {
		iValue += GC.getGameINLINE().getSorenRandNum(15, "AI Promote");
	}

	return iValue;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_shadow(UnitAITypes eUnitAI, int iMax, int iMaxRatio, bool bWithCargoOnly, bool bOutsideCityOnly, int iMaxPath, bool bIgnoreMoves) {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvUnit* pBestUnit = NULL;

	int iLoop;
	for (CvUnit* pLoopUnit = GET_PLAYER(getOwnerINLINE()).firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = GET_PLAYER(getOwnerINLINE()).nextUnit(&iLoop)) {
		if (pLoopUnit != this) {
			if (AI_plotValid(pLoopUnit->plot())) {
				if (pLoopUnit->isGroupHead()) {
					if (!(pLoopUnit->isCargo())) {
						if (pLoopUnit->AI_getUnitAIType() == eUnitAI) {
							if (bIgnoreMoves || pLoopUnit->getGroup()->baseMoves() <= getGroup()->baseMoves()) {
								if (!bWithCargoOnly || pLoopUnit->getGroup()->hasCargo()) {
									if (bOutsideCityOnly && pLoopUnit->plot()->isCity()) {
										continue;
									}

									int iShadowerCount = GET_PLAYER(getOwnerINLINE()).AI_unitTargetMissionAIs(pLoopUnit, MISSIONAI_SHADOW, getGroup());
									if ((-1 == iMax || iShadowerCount < iMax) && (-1 == iMaxRatio || iShadowerCount == 0 || (100 * iShadowerCount) / std::max(1, pLoopUnit->getGroup()->countNumUnitAIType(eUnitAI)) <= iMaxRatio)) {
										if (!(pLoopUnit->plot()->isVisibleEnemyUnit(this))) {
											int iPathTurns;
											if (generatePath(pLoopUnit->plot(), 0, true, &iPathTurns, iMaxPath)) {
												if (iPathTurns <= iMaxPath) {
													int iValue = 1 + pLoopUnit->getGroup()->getCargo();
													iValue *= 1000;
													iValue /= 1 + iPathTurns;

													if (iValue > iBestValue) {
														iBestValue = iValue;
														pBestUnit = pLoopUnit;
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestUnit != NULL) {
		if (atPlot(pBestUnit->plot())) {
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_SHADOW, NULL, pBestUnit);
			return true;
		} else {
			getGroup()->pushMission(MISSION_MOVE_TO_UNIT, pBestUnit->getOwnerINLINE(), pBestUnit->getID(), 0, false, false, MISSIONAI_SHADOW, NULL, pBestUnit);
			return true;
		}
	}

	return false;
}

// K-Mod. One group function to rule them all.
bool CvUnitAI::AI_omniGroup(UnitAITypes eUnitAI, int iMaxGroup, int iMaxOwnUnitAI, bool bStackOfDoom, int iFlags, int iMaxPath, bool bMergeGroups, bool bSafeOnly, bool bIgnoreFaster, bool bIgnoreOwnUnitType, bool bBiggerOnly, int iMinUnitAI, bool bWithCargoOnly, bool bIgnoreBusyTransports) {
	PROFILE_FUNC();

	iFlags &= ~MOVE_DECLARE_WAR; // Don't consider war when we just want to group

	if (isCargo())
		return false;

	if (!AI_canGroupWithAIType(eUnitAI))
		return false;

	if (getDomainType() == DOMAIN_LAND && !canMoveAllTerrain()) {
		if (area()->getNumAIUnits(getOwnerINLINE(), eUnitAI) == 0) {
			return false;
		}
	}

	int iOurImpassableCount = 0;
	CLLNode<IDInfo>* pUnitNode = getGroup()->headUnitNode();
	while (pUnitNode != NULL) {
		CvUnit* pImpassUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = getGroup()->nextUnitNode(pUnitNode);

		iOurImpassableCount = std::max(iOurImpassableCount, GET_PLAYER(getOwnerINLINE()).AI_unitImpassableCount(pImpassUnit->getUnitType()));
	}

	CvUnit* pBestUnit = NULL;
	int iBestValue = MAX_INT;

	int iLoop;
	CvSelectionGroup* pLoopGroup = NULL;
	for (pLoopGroup = GET_PLAYER(getOwnerINLINE()).firstSelectionGroup(&iLoop); pLoopGroup != NULL; pLoopGroup = GET_PLAYER(getOwnerINLINE()).nextSelectionGroup(&iLoop)) {
		CvUnit* pLoopUnit = pLoopGroup->getHeadUnit();
		if (pLoopUnit == NULL)
			continue;

		CvPlot* pPlot = pLoopUnit->plot();
		if (AI_plotValid(pPlot)) {
			if (iMaxPath != 0 || pPlot == plot()) {
				if (getDomainType() != DOMAIN_LAND || canMoveAllTerrain() || area() == pPlot->area()) {
					if (AI_allowGroup(pLoopUnit, eUnitAI)) {
						// K-Mod. I've restructed this wad of conditions so that it is easier for me to read.
						// ((removed ((heaps) of parentheses) (etc)).)
						// also, I've rearranged the order to be slightly faster for failed checks.
						// Note: the iMaxGroups & OwnUnitAI check is apparently off-by-one. This is for backwards compatibility for the original code.
						if (true
							&& (!bSafeOnly || !isEnemy(pPlot->getTeam()))
							&& (!bWithCargoOnly || pLoopUnit->getGroup()->hasCargo())
							&& (!bBiggerOnly || !bMergeGroups || pLoopGroup->getNumUnits() >= getGroup()->getNumUnits())
							&& (!bIgnoreFaster || pLoopGroup->baseMoves() <= baseMoves())
							&& (!bIgnoreOwnUnitType || pLoopUnit->getUnitType() != getUnitType())
							&& (!bIgnoreBusyTransports || !pLoopGroup->hasCargo() || (pLoopGroup->AI_getMissionAIType() != MISSIONAI_ASSAULT && pLoopGroup->AI_getMissionAIType() != MISSIONAI_REINFORCE))
							&& (iMinUnitAI == -1 || pLoopGroup->countNumUnitAIType(eUnitAI) >= iMinUnitAI)
							&& (iMaxOwnUnitAI == -1 || (bMergeGroups ? std::max(0, getGroup()->countNumUnitAIType(AI_getUnitAIType()) - 1) : 0) + pLoopGroup->countNumUnitAIType(AI_getUnitAIType()) <= iMaxOwnUnitAI + (bStackOfDoom ? AI_stackOfDoomExtra() : 0))
							&& (iMaxGroup == -1 || (bMergeGroups ? getGroup()->getNumUnits() - 1 : 0) + pLoopGroup->getNumUnits() + GET_PLAYER(getOwnerINLINE()).AI_unitTargetMissionAIs(pLoopUnit, MISSIONAI_GROUP, getGroup()) <= iMaxGroup + (bStackOfDoom ? AI_stackOfDoomExtra() : 0))
							&& (pLoopGroup->AI_getMissionAIType() != MISSIONAI_GUARD_CITY || !pLoopGroup->plot()->isCity() || pLoopGroup->plot()->plotCount(PUF_isMissionAIType, MISSIONAI_GUARD_CITY, -1, getOwnerINLINE()) > pLoopGroup->plot()->getPlotCity()->AI_minDefenders())
							) {
							FAssert(!pPlot->isVisibleEnemyUnit(this))
								if (iOurImpassableCount > 0 || AI_getUnitAIType() == UNITAI_ASSAULT_SEA) {
									int iTheirImpassableCount = 0;
									pUnitNode = pLoopGroup->headUnitNode();
									while (pUnitNode != NULL) {
										CvUnit* pImpassUnit = ::getUnit(pUnitNode->m_data);
										pUnitNode = pLoopGroup->nextUnitNode(pUnitNode);

										iTheirImpassableCount = std::max(iTheirImpassableCount, GET_PLAYER(getOwnerINLINE()).AI_unitImpassableCount(pImpassUnit->getUnitType()));
									}

									if (iOurImpassableCount != iTheirImpassableCount) {
										continue;
									}
								}

							int iPathTurns = 0;
							if (atPlot(pPlot) || generatePath(pPlot, iFlags, true, &iPathTurns, iMaxPath)) {
								int iCost = 100 * (iPathTurns * iPathTurns + 1);
								iCost *= 4 + pLoopGroup->getCargo();
								iCost /= 2 + pLoopGroup->getNumUnits();

								if (iCost < iBestValue) {
									iBestValue = iCost;
									pBestUnit = pLoopUnit;
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestUnit != NULL) {
		if (!atPlot(pBestUnit->plot())) {
			if (!bMergeGroups && getGroup()->getNumUnits() > 1)
				joinGroup(NULL); // might as well leave our current group behind, since they won't be merging anyway.
			getGroup()->pushMission(MISSION_MOVE_TO_UNIT, pBestUnit->getOwnerINLINE(), pBestUnit->getID(), iFlags, false, false, MISSIONAI_GROUP, NULL, pBestUnit);
		}
		if (atPlot(pBestUnit->plot())) {
			if (bMergeGroups)
				getGroup()->mergeIntoGroup(pBestUnit->getGroup());
			else
				joinGroup(pBestUnit->getGroup());
		}
		return true;
	}

	return false;
}

// Added new options to aid transport grouping
// Returns true if a group was joined or a mission was pushed...
bool CvUnitAI::AI_group(UnitAITypes eUnitAI, int iMaxGroup, int iMaxOwnUnitAI, int iMinUnitAI, bool bIgnoreFaster, bool bIgnoreOwnUnitType, bool bStackOfDoom, int iMaxPath, bool bAllowRegrouping, bool bWithCargoOnly, bool bInCityOnly, MissionAITypes eIgnoreMissionAIType) {
	// K-Mod. I've completely gutted this function. It's now basically just a wrapper for AI_omniGroup.
	// This is part of the process of phasing the function out.

	// unsupported features:
	FAssert(!bInCityOnly);
	FAssert(eIgnoreMissionAIType == NO_MISSIONAI || (eUnitAI == UNITAI_ASSAULT_SEA && eIgnoreMissionAIType == MISSIONAI_ASSAULT));
	// .. and now the function.

	if (!bAllowRegrouping) {
		if (getGroup()->getNumUnits() > 1) {
			return false;
		}
	}

	return AI_omniGroup(eUnitAI, iMaxGroup, iMaxOwnUnitAI, bStackOfDoom, 0, iMaxPath, true, true, bIgnoreFaster, bIgnoreOwnUnitType, false, iMinUnitAI, bWithCargoOnly, eIgnoreMissionAIType == MISSIONAI_ASSAULT);
}

bool CvUnitAI::AI_groupMergeRange(UnitAITypes eUnitAI, int iMaxRange, bool bBiggerOnly, bool bAllowRegrouping, bool bIgnoreFaster) {
	// K-Mod. I've completely gutted this function. It's now basically just a wrapper for AI_omniGroup.
	// This is part of the process of phasing the function out.

	if (isCargo()) {
		return false;
	}

	if (!bAllowRegrouping) {
		if (getGroup()->getNumUnits() > 1) {
			return false;
		}
	}

	// approximate max path based on range.
	int iMaxPath = 1;
	while (AI_searchRange(iMaxPath) < iMaxRange)
		iMaxPath++;

	return AI_omniGroup(eUnitAI, -1, -1, false, 0, iMaxPath, true, false, bIgnoreFaster, false, bBiggerOnly);
}

// K-Mod
// Look for the nearest suitable transport. Return a pointer to the transport unit.
// (the bulk of this function was moved straight out of AI_load. I've fixed it up a bit, but I didn't write most of it.)
CvUnit* CvUnitAI::AI_findTransport(UnitAITypes eUnitAI, int iFlags, int iMaxPath, UnitAITypes eTransportedUnitAI, int iMinCargo, int iMinCargoSpace, int iMaxCargoSpace, int iMaxCargoOurUnitAI) {
	if (eUnitAI != NO_UNITAI && GET_PLAYER(getOwnerINLINE()).AI_getNumAIUnits(eUnitAI) == 0)
		return NULL;

	int iBestValue = MAX_INT;
	CvUnit* pBestUnit = 0;

	const int iLoadMissionAICount = 4;
	MissionAITypes aeLoadMissionAI[iLoadMissionAICount] = { MISSIONAI_LOAD_ASSAULT, MISSIONAI_LOAD_SETTLER, MISSIONAI_LOAD_SPECIAL, MISSIONAI_ATTACK_SPY };

	int iCurrentGroupSize = getGroup()->getNumUnits();

	int iLoop;
	for (CvUnit* pLoopUnit = GET_PLAYER(getOwnerINLINE()).firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = GET_PLAYER(getOwnerINLINE()).nextUnit(&iLoop)) {
		if (pLoopUnit->cargoSpace() <= 0 || (pLoopUnit->getArea() != getArea() && !pLoopUnit->plot()->isAdjacentToArea(getArea())) || !canLoadUnit(pLoopUnit, pLoopUnit->plot())) // K-Mod
			continue;

		UnitAITypes eLoopUnitAI = pLoopUnit->AI_getUnitAIType();
		if (eUnitAI == NO_UNITAI || eLoopUnitAI == eUnitAI) {
			int iCargoSpaceAvailable = pLoopUnit->cargoSpaceAvailable(getSpecialUnitType(), getDomainType());
			iCargoSpaceAvailable -= GET_PLAYER(getOwnerINLINE()).AI_unitTargetMissionAIs(pLoopUnit, aeLoadMissionAI, iLoadMissionAICount, getGroup());
			if (iCargoSpaceAvailable > 0) {
				if ((eTransportedUnitAI == NO_UNITAI || pLoopUnit->getUnitAICargo(eTransportedUnitAI) > 0) &&
					(iMinCargo == -1 || pLoopUnit->getCargo() >= iMinCargo)) {
					// Use existing count of cargo space available
					if ((iMinCargoSpace == -1) || (iCargoSpaceAvailable >= iMinCargoSpace)) {
						if ((iMaxCargoSpace == -1) || (iCargoSpaceAvailable <= iMaxCargoSpace)) {
							if ((iMaxCargoOurUnitAI == -1) || (pLoopUnit->getUnitAICargo(AI_getUnitAIType()) <= iMaxCargoOurUnitAI)) {
								if (!(pLoopUnit->plot()->isVisibleEnemyUnit(this))) {
									CvPlot* pUnitTargetPlot = pLoopUnit->getGroup()->AI_getMissionAIPlot();
									if (pUnitTargetPlot == NULL || pUnitTargetPlot->getTeam() == getTeam() || (!pUnitTargetPlot->isOwned() || !isPotentialEnemy(pUnitTargetPlot->getTeam(), pUnitTargetPlot))) {
										int iPathTurns = 0;
										if (atPlot(pLoopUnit->plot()) || generatePath(pLoopUnit->plot(), iFlags, true, &iPathTurns, iMaxPath)) {
											// prefer a transport that can hold as much of our group as possible 
											int iValue = 5 * std::max(0, iCurrentGroupSize - iCargoSpaceAvailable) + iPathTurns;

											if (iValue < iBestValue) {
												iBestValue = iValue;
												pBestUnit = pLoopUnit;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return pBestUnit;
}

// Returns true if we loaded onto a transport or a mission was pushed...
bool CvUnitAI::AI_load(UnitAITypes eUnitAI, MissionAITypes eMissionAI, UnitAITypes eTransportedUnitAI, int iMinCargo, int iMinCargoSpace, int iMaxCargoSpace, int iMaxCargoOurUnitAI, int iFlags, int iMaxPath, int iMaxTransportPath) {
	PROFILE_FUNC();

	if (getCargo() > 0) {
		return false;
	}

	if (isCargo()) {
		getGroup()->pushMission(MISSION_SKIP);
		return true;
	}
	CvUnit* pBestUnit = AI_findTransport(eUnitAI, iFlags, iMaxPath, eTransportedUnitAI, iMinCargo, iMinCargoSpace, iMaxCargoSpace, iMaxCargoOurUnitAI);
	if (pBestUnit != NULL && iMaxTransportPath < MAX_INT && (eUnitAI == UNITAI_ASSAULT_SEA || eUnitAI == UNITAI_SPY_SEA)) // K-Mod
	{
		// Can transport reach enemy in requested time
		bool bFoundEnemyPlotInRange = false;
		int iPathTurns;
		int iRange = iMaxTransportPath * pBestUnit->baseMoves();
		CvPlot* pAdjacentPlot = NULL;
		// K-Mod. use a separate pathfinder for the transports, so that we don't reset our current path data.
		KmodPathFinder temp_finder;
		temp_finder.SetSettings(CvPathSettings(pBestUnit->getGroup(), iFlags & MOVE_DECLARE_WAR, iMaxTransportPath, GC.getMOVE_DENOMINATOR()));
		//

		for (int iDX = -iRange; (iDX <= iRange && !bFoundEnemyPlotInRange); iDX++) {
			for (int iDY = -iRange; (iDY <= iRange && !bFoundEnemyPlotInRange); iDY++) {
				CvPlot* pLoopPlot = plotXY(pBestUnit->getX_INLINE(), pBestUnit->getY_INLINE(), iDX, iDY); // K-Mod

				if (pLoopPlot != NULL) {
					if (pLoopPlot->isCoastalLand()) {
						if (pLoopPlot->isOwned()) {
							if (isPotentialEnemy(pLoopPlot->getTeam(), pLoopPlot) && !isBarbarian()) {
								if (pLoopPlot->area()->getCitiesPerPlayer(pLoopPlot->getOwnerINLINE()) > 0) {
									// Transport cannot enter land plot without cargo, so generate path only works properly if
									// land units are already loaded

									for (int iI = 0; (iI < NUM_DIRECTION_TYPES && !bFoundEnemyPlotInRange); iI++) {
										pAdjacentPlot = plotDirection(getX_INLINE(), getY_INLINE(), (DirectionTypes)iI);
										if (pAdjacentPlot != NULL) {
											if (pAdjacentPlot->isWater()) {
												if (temp_finder.GeneratePath(pAdjacentPlot)) {
													iPathTurns = temp_finder.GetPathTurns() + (temp_finder.GetFinalMoves() == 0 ? 1 : 0); // K-Mod

													if (iPathTurns <= iMaxTransportPath) {
														bFoundEnemyPlotInRange = true;
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}

		if (!bFoundEnemyPlotInRange) {
			pBestUnit = NULL;
		}
	}

	if (pBestUnit != NULL) {
		if (atPlot(pBestUnit->plot())) {
			CvSelectionGroup* pRemainderGroup = NULL; // K-Mod renamed from 'pOtherGroup'
			getGroup()->setTransportUnit(pBestUnit, &pRemainderGroup); // XXX is this dangerous (not pushing a mission...) XXX air units?

			// If part of large group loaded, then try to keep loading the rest
			if (eUnitAI == UNITAI_ASSAULT_SEA && eMissionAI == MISSIONAI_LOAD_ASSAULT) {
				if (pRemainderGroup != NULL && pRemainderGroup->getNumUnits() > 0) {
					if (pRemainderGroup->getHeadUnitAI() == AI_getUnitAIType()) {
						if (pRemainderGroup->getHeadUnit()->AI_load(eUnitAI, eMissionAI, eTransportedUnitAI, iMinCargo, iMinCargoSpace, iMaxCargoSpace, iMaxCargoOurUnitAI, iFlags, 0, iMaxTransportPath))
							pRemainderGroup->AI_setForceSeparate(false); // K-Mod
					} else if (eTransportedUnitAI == NO_UNITAI && iMinCargo < 0 && iMinCargoSpace < 0 && iMaxCargoSpace < 0 && iMaxCargoOurUnitAI < 0) {
						if (pRemainderGroup->getHeadUnit()->AI_load(eUnitAI, eMissionAI, NO_UNITAI, -1, -1, -1, -1, iFlags, 0, iMaxTransportPath))
							pRemainderGroup->AI_setForceSeparate(false); // K-Mod
					}
				}
			}
			// K-Mod - just for efficiency, I'll take care of the force separate stuff here.
			if (pRemainderGroup && pRemainderGroup->AI_isForceSeparate())
				pRemainderGroup->AI_separate();

			return true;
		} else {
			// BBAI TODO: To split or not to split?
			// K-Mod. How about we this:
			// Split the group only if it is going to take more than 1 turn to get to the transport.
			if (generatePath(pBestUnit->plot(), iFlags, true, 0, 1)) {
				// only 1 turn. Don't split.
				getGroup()->pushMission(MISSION_MOVE_TO_UNIT, pBestUnit->getOwnerINLINE(), pBestUnit->getID(), iFlags, false, false, eMissionAI, NULL, pBestUnit);
				return true;
			} else {
				// (bbai code. split the group)
				int iCargoSpaceAvailable = pBestUnit->cargoSpaceAvailable(getSpecialUnitType(), getDomainType());
				FAssertMsg(iCargoSpaceAvailable > 0, "best unit has no space");

				// split our group to fit on the transport
				CvSelectionGroup* pRemainderGroup = NULL;
				CvSelectionGroup* pSplitGroup = getGroup()->splitGroup(iCargoSpaceAvailable, this, &pRemainderGroup);
				FAssertMsg(pSplitGroup, "splitGroup failed");
				FAssertMsg(getGroupID() == pSplitGroup->getID(), "splitGroup failed to put head unit in the new group");

				if (pSplitGroup != NULL) {
					CvPlot* pOldPlot = pSplitGroup->plot();
					pSplitGroup->pushMission(MISSION_MOVE_TO_UNIT, pBestUnit->getOwnerINLINE(), pBestUnit->getID(), iFlags, false, false, eMissionAI, NULL, pBestUnit);
					// K-Mod - just for efficiency, I'll take care of the force separate stuff here.
					if (pRemainderGroup && pRemainderGroup->AI_isForceSeparate())
						pRemainderGroup->AI_separate();
					return true;
				}
			}
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_guardCityBestDefender() {
	CvPlot* pPlot = plot();
	CvCity* pCity = pPlot->getPlotCity();

	if (pCity != NULL) {
		if (pCity->getOwnerINLINE() == getOwnerINLINE()) {
			if (pPlot->getBestDefender(getOwnerINLINE()) == this) {
				getGroup()->pushMission(isFortifyable() ? MISSION_FORTIFY : MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_CITY, NULL);
				return true;
			}
		}
	}

	return false;
}

bool CvUnitAI::AI_guardCityOnlyDefender() {
	FAssert(getGroup()->getNumUnits() == 1);

	CvCity* pPlotCity = plot()->getPlotCity();
	if (pPlotCity && pPlotCity->getOwnerINLINE() == getOwnerINLINE()) {
		if (plot()->plotCount(PUF_isMissionAIType, MISSIONAI_GUARD_CITY, -1, getOwnerINLINE()) <= (getGroup()->AI_getMissionAIType() == MISSIONAI_GUARD_CITY ? 1 : 0)) {
			getGroup()->pushMission(isFortifyable() ? MISSION_FORTIFY : MISSION_SKIP, -1, -1, 0, false, false, noDefensiveBonus() ? NO_MISSIONAI : MISSIONAI_GUARD_CITY, 0);
			return true;
		}
	}
	return false;
}

bool CvUnitAI::AI_guardCityMinDefender(bool bSearch) {
	PROFILE_FUNC();

	CvCity* pPlotCity = plot()->getPlotCity();
	if ((pPlotCity != NULL) && (pPlotCity->getOwnerINLINE() == getOwnerINLINE())) {
		// Note. For this check, we only count UNITAI_CITY_DEFENSE. But in the bSearch case, we count all guard_city units.
		int iDefendersHave = plot()->plotCount(PUF_isMissionAIType, MISSIONAI_GUARD_CITY, -1, getOwnerINLINE(), NO_TEAM, AI_getUnitAIType() == UNITAI_CITY_DEFENSE ? PUF_isUnitAIType : 0, UNITAI_CITY_DEFENSE);

		if (getGroup()->AI_getMissionAIType() == MISSIONAI_GUARD_CITY)
			iDefendersHave--;

		if (iDefendersHave < pPlotCity->AI_minDefenders()) {
			if (iDefendersHave <= 1 || GC.getGame().getSorenRandNum(area()->getNumAIUnits(getOwnerINLINE(), UNITAI_CITY_DEFENSE) + 5, "AI shuffle defender") > 1) {
				getGroup()->pushMission(isFortifyable() ? MISSION_FORTIFY : MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_CITY, NULL);
				return true;
			}
		}
	}

	if (bSearch) {
		int iBestValue = 0;
		CvPlot* pBestPlot = NULL;
		CvPlot* pBestGuardPlot = NULL;

		CvCity* pLoopCity;
		int iLoop;

		int iCurrentTurn = GC.getGame().getGameTurn();
		for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
			if (AI_plotValid(pLoopCity->plot())) {
				int iDefendersHave = pLoopCity->plot()->plotCount(PUF_isMissionAIType, MISSIONAI_GUARD_CITY, -1, getOwnerINLINE());
				if (pPlotCity == pLoopCity && getGroup()->AI_getMissionAIType() == MISSIONAI_GUARD_CITY)
					iDefendersHave--;
				int iDefendersNeed = pLoopCity->AI_minDefenders();

				if (iDefendersHave < iDefendersNeed) {
					if (!(pLoopCity->plot()->isVisibleEnemyUnit(this))) {
						iDefendersHave += GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_GUARD_CITY, getGroup());
						if (iDefendersHave < iDefendersNeed + 1) {
							int iPathTurns;
							if (generatePath(pLoopCity->plot(), 0, true, &iPathTurns, 10)) // K-Mod. (also deleted "if (iPathTurns < 10)")
							{
								int iValue = (iDefendersNeed - iDefendersHave) * 10;
								iValue += iDefendersHave <= 0 ? 10 : 0;

								iValue += 2 * pLoopCity->getCultureLevel();
								iValue += pLoopCity->getPopulation() / 3;
								iValue += pLoopCity->isOccupation() ? 8 : 0;
								iValue -= iPathTurns;

								if (iValue > iBestValue) {
									iBestValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									pBestGuardPlot = pLoopCity->plot();
								}
							}
						}
					}
				}
			}
		}
		if (pBestPlot != NULL) {
			if (atPlot(pBestGuardPlot)) {
				FAssert(pBestGuardPlot == pBestPlot);
				getGroup()->pushMission(isFortifyable() ? MISSION_FORTIFY : MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_CITY, NULL);
				return true;
			}
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_CITY, pBestGuardPlot);
			return true;
		}
	}

	return false;
}

// Returns true if a mission was pushed...
// K-Mod. This function was so full of useless cruft and duplicated code and double-counting mistakes...
// I've deleted the bulk of the old code, and rewritten it to be much much simplier - and also better.
bool CvUnitAI::AI_guardCity(bool bLeave, bool bSearch, int iMaxPath, int iFlags) {
	PROFILE_FUNC();

	FAssert(getDomainType() == DOMAIN_LAND);
	FAssert(canDefend());

	CvPlot* pEndTurnPlot = NULL;
	CvPlot* pBestGuardPlot = NULL;


	CvPlot* pPlot = plot();
	CvCity* pCity = pPlot->getPlotCity();

	if (pCity != NULL && pCity->getOwnerINLINE() == getOwnerINLINE()) {
		int iExtra; // additional defenders needed.
		if (bLeave && !pCity->AI_isDanger()) {
			iExtra = -1;
		} else {
			iExtra = bSearch ? 0 : GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(pPlot, 2);
		}

		if (pPlot->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE(), NO_TEAM, AI_isCityAIType() ? PUF_isCityAIType : 0)
			< pCity->AI_neededDefenders() + 1 + iExtra) // +1 because this unit is being counted as a defender.
		{
			// don't bother searching. We're staying here.
			bSearch = false;
			pEndTurnPlot = plot();
			pBestGuardPlot = plot();
		}
	}

	if (bSearch) {
		int iBestValue = 0;

		int iLoop;
		for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
			if (AI_plotValid(pLoopCity->plot())) {
				// BBAI efficiency: check area for land units
				if ((getDomainType() == DOMAIN_LAND) && (pLoopCity->area() != area()) && !(getGroup()->canMoveAllTerrain()))
					continue;

				int iDefendersNeeded = pLoopCity->AI_neededDefenders();
				int iDefendersHave = pLoopCity->plot()->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE(), NO_TEAM, AI_isCityAIType() ? PUF_isCityAIType : 0);
				if (pCity == pLoopCity)
					iDefendersHave -= getGroup()->getNumUnits();
				if (iDefendersHave < iDefendersNeeded) {
					if (!(pLoopCity->plot()->isVisibleEnemyUnit(this))) {
						if ((GC.getGame().getGameTurn() - pLoopCity->getGameTurnAcquired() < 10) || GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_GUARD_CITY, getGroup()) < 2) {
							int iPathTurns;
							if (!atPlot(pLoopCity->plot()) && generatePath(pLoopCity->plot(), iFlags, true, &iPathTurns, iMaxPath)) {
								if (iPathTurns <= iMaxPath) {
									int iValue = 1000 * (1 + iDefendersNeeded - iDefendersHave);
									iValue /= 1 + iPathTurns + iDefendersHave;
									if (iValue > iBestValue) {
										iBestValue = iValue;
										pEndTurnPlot = getPathEndTurnPlot();
										pBestGuardPlot = pLoopCity->plot();
										FAssert(!atPlot(pEndTurnPlot));
										if (iMaxPath == 1 || iBestValue >= 500)
											break; // we found a good city. No need to waste any more time looking.
									}
								}
							}
						}
					}
				}
			}
		}
	}
	if (pEndTurnPlot != NULL && pBestGuardPlot != NULL) {
		CvSelectionGroup* pOldGroup = getGroup();
		CvUnit* pEjectedUnit = getGroup()->AI_ejectBestDefender(pPlot);

		if (!pEjectedUnit) {
			FAssertMsg(false, "AI_ejectBestDefender failed to choose a candidate for AI_guardCity.");
			pEjectedUnit = this;
			if (getGroup()->getNumUnits() > 0)
				joinGroup(0);
		}

		FAssert(pEjectedUnit);

		// If the unit is not suited for defense, do not use MISSIONAI_GUARD_CITY.
		MissionAITypes eMissionAI = pEjectedUnit->noDefensiveBonus() ? NO_MISSIONAI : MISSIONAI_GUARD_CITY;
		if (atPlot(pBestGuardPlot)) {
			pEjectedUnit->getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, eMissionAI, 0);
		} else {
			FAssert(bSearch);
			FAssert(!atPlot(pEndTurnPlot));
			pEjectedUnit->getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX_INLINE(), pEndTurnPlot->getY_INLINE(), iFlags, false, false, eMissionAI, pBestGuardPlot);
		}

		if (pEjectedUnit->getGroup() == pOldGroup || pEjectedUnit == this)
			return true;
		else
			return false;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_guardCityAirlift() {
	PROFILE_FUNC();

	if (getGroup()->getNumUnits() > 1) {
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();
	if (pCity == NULL) {
		return false;
	}

	if (pCity->getMaxAirlift() == 0) {
		return false;
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if (pLoopCity != pCity) {
			if (canAirliftAt(pCity->plot(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE())) {
				if (!(pLoopCity->AI_isDefended((!AI_isCityAIType()) ? pLoopCity->plot()->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE(), NO_TEAM, PUF_isNotCityAIType) : 0)))	// XXX check for other team's units?
				{
					int iValue = pLoopCity->getPopulation();

					if (pLoopCity->AI_isDanger()) {
						iValue *= 2;
					}

					if (iValue > iBestValue) {
						iBestValue = iValue;
						pBestPlot = pLoopCity->plot();
						FAssert(pLoopCity != pCity);
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		getGroup()->pushMission(MISSION_AIRLIFT, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}

// K-Mod.
// This function will use our naval unit to block the coast outside our cities.
bool CvUnitAI::AI_guardCoast(bool bPrimaryOnly, int iFlags, int iMaxPath) {
	PROFILE_FUNC();

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	CvPlot* pBestCityPlot = 0;
	CvPlot* pEndTurnPlot = 0;
	int iBestValue = 0;

	int iLoop;
	for (CvCity* pLoopCity = kOwner.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kOwner.nextCity(&iLoop)) {
		if (!pLoopCity->isCoastal(GC.getMIN_WATER_SIZE_FOR_OCEAN()) || (bPrimaryOnly && !kOwner.AI_isPrimaryArea(pLoopCity->area())))
			continue;

		int iCoastPlots = 0;

		for (DirectionTypes i = (DirectionTypes)0; i < NUM_DIRECTION_TYPES; i = (DirectionTypes)(i + 1)) {
			CvPlot* pAdjacentPlot = plotDirection(pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE(), i);

			if (pAdjacentPlot && pAdjacentPlot->isWater() && pAdjacentPlot->area()->getNumTiles() >= GC.getMIN_WATER_SIZE_FOR_OCEAN())
				iCoastPlots++;
		}

		int iBaseValue = iCoastPlots > 0
			? 1000 * pLoopCity->AI_neededDefenders() / (iCoastPlots + 3) // arbitrary units (AI_cityValue is a bit slower)
			: 0;

		iBaseValue /= kOwner.AI_isLandWar(pLoopCity->area()) ? 2 : 1;

		if (iBaseValue <= iBestValue)
			continue;

		iBaseValue *= 4;
		iBaseValue /= 4 + kOwner.AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_GUARD_COAST, getGroup(), 1);

		if (iBaseValue <= iBestValue)
			continue;

		for (DirectionTypes i = (DirectionTypes)0; i < NUM_DIRECTION_TYPES; i = (DirectionTypes)(i + 1)) {
			CvPlot* pAdjacentPlot = plotDirection(pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE(), i);

			if (!pAdjacentPlot || !pAdjacentPlot->isWater() || pAdjacentPlot->area()->getNumTiles() < GC.getMIN_WATER_SIZE_FOR_OCEAN() || pAdjacentPlot->getTeam() != getTeam())
				continue;

			int iValue = iBaseValue;

			iValue *= 2;
			iValue /= pAdjacentPlot->getBonusType(getTeam()) == NO_BONUS ? 3 : 2;
			iValue *= 3;
			iValue /= std::max(3, (atPlot(pAdjacentPlot) ? 1 - getGroup()->getNumUnits() : 1) + pAdjacentPlot->plotCount(PUF_isMissionAIType, MISSIONAI_GUARD_COAST, -1, getOwnerINLINE()));

			int iPathTurns;
			if (iValue > iBestValue && generatePath(pAdjacentPlot, iFlags, true, &iPathTurns, iMaxPath)) {
				iValue *= 4;
				iValue /= 3 + iPathTurns;

				if (iValue > iBestValue) {
					iBestValue = iValue;
					pBestCityPlot = pLoopCity->plot();
					pEndTurnPlot = getPathEndTurnPlot();
				}
			}
		}
	}

	if (pEndTurnPlot) {
		if (atPlot(pEndTurnPlot)) {
			getGroup()->pushMission(canSeaPatrol(pEndTurnPlot) ? MISSION_SEAPATROL : MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_COAST, pBestCityPlot);
		} else {
			getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX_INLINE(), pEndTurnPlot->getY_INLINE(), iFlags, false, false, MISSIONAI_GUARD_COAST, pBestCityPlot);
		}
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_guardBonus(int iMinValue) {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestGuardPlot = NULL;

	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot)) {
			if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE()) {
				BonusTypes eNonObsoleteBonus = pLoopPlot->getNonObsoleteBonusType(getTeam(), true);

				if (eNonObsoleteBonus != NO_BONUS && pLoopPlot->isValidDomainForAction(*this)) // K-Mod. (boats shouldn't defend forts!)
				{
					int iValue = GET_PLAYER(getOwnerINLINE()).AI_bonusVal(eNonObsoleteBonus, 0); // K-Mod

					iValue += std::max(0, 200 * GC.getBonusInfo(eNonObsoleteBonus).getAIObjective());

					if (pLoopPlot->getPlotGroupConnectedBonus(getOwnerINLINE(), eNonObsoleteBonus) == 1) {
						iValue *= 2;
					}

					if (iValue > iMinValue) {
						if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
							//if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_GUARD_BONUS, getGroup()) == 0)
							iValue *= 2;
							iValue /= 2 + GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_GUARD_BONUS, getGroup());
							if (iValue > iMinValue) {
								int iPathTurns;
								if (generatePath(pLoopPlot, 0, true, &iPathTurns)) {
									iValue *= 1000;

									iValue /= iPathTurns + 4; // was +1

									if (iValue > iBestValue) {
										iBestValue = iValue;
										pBestPlot = getPathEndTurnPlot();
										pBestGuardPlot = pLoopPlot;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestGuardPlot != NULL)) {
		if (atPlot(pBestGuardPlot)) {
			getGroup()->pushMission(canSeaPatrol(pBestGuardPlot) ? MISSION_SEAPATROL : (isFortifyable() ? MISSION_FORTIFY : MISSION_SKIP), -1, -1, 0, false, false, MISSIONAI_GUARD_BONUS, pBestGuardPlot); // K-Mod
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_BONUS, pBestGuardPlot);
			return true;
		}
	}

	return false;
}

int CvUnitAI::AI_getPlotDefendersNeeded(CvPlot* pPlot, int iExtra) {
	int iNeeded = iExtra;
	BonusTypes eNonObsoleteBonus = pPlot->getNonObsoleteBonusType(getTeam());
	if (eNonObsoleteBonus != NO_BONUS) {
		iNeeded += (GET_PLAYER(getOwnerINLINE()).AI_bonusVal(eNonObsoleteBonus, 1) + 10) / 19;
	}

	int iDefense = pPlot->defenseModifier(getTeam(), true);

	iNeeded += (iDefense + 25) / 50;

	if (iNeeded == 0) {
		return 0;
	}

	iNeeded += GET_PLAYER(getOwnerINLINE()).AI_getPlotAirbaseValue(pPlot) / 50;

	int iNumHostiles = 0;
	int iNumPlots = 0;

	int iRange = 2;
	for (int iX = -iRange; iX <= iRange; iX++) {
		for (int iY = -iRange; iY <= iRange; iY++) {
			CvPlot* pLoopPlot = plotXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iX, iY);
			if (pLoopPlot != NULL) {
				iNumHostiles += pLoopPlot->getNumVisibleEnemyDefenders(this);
				if ((pLoopPlot->getTeam() != getTeam()) || pLoopPlot->isCoastalLand()) {
					iNumPlots++;
					if (isEnemy(pLoopPlot->getTeam())) {
						iNumPlots += 4;
					}
				}
			}
		}
	}

	if ((iNumHostiles == 0) && (iNumPlots < 4)) {
		if (iNeeded > 1) {
			iNeeded = 1;
		} else {
			iNeeded = 0;
		}
	}

	return iNeeded;
}

bool CvUnitAI::AI_guardFort(bool bSearch) {
	PROFILE_FUNC();

	if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
		ImprovementTypes eImprovement = plot()->getImprovementType();
		if (eImprovement != NO_IMPROVEMENT) {
			if (GC.getImprovementInfo(eImprovement).isActsAsCity()) {
				if (plot()->plotCount(PUF_isCityAIType, -1, -1, getOwnerINLINE()) <= AI_getPlotDefendersNeeded(plot(), 0)) {
					getGroup()->pushMission(isFortifyable() ? MISSION_FORTIFY : MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_BONUS, plot());
					return true;
				}
			}
		}
	}

	if (!bSearch) {
		return false;
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestGuardPlot = NULL;

	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot) && !atPlot(pLoopPlot)) {
			if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE()) {
				ImprovementTypes eImprovement = pLoopPlot->getImprovementType();
				if (eImprovement != NO_IMPROVEMENT) {
					if (GC.getImprovementInfo(eImprovement).isActsAsCity()) {
						int iValue = AI_getPlotDefendersNeeded(pLoopPlot, 0);

						if (iValue > 0) {
							if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
								if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_GUARD_BONUS, getGroup()) < iValue) {
									int iPathTurns;
									if (generatePath(pLoopPlot, 0, true, &iPathTurns)) {
										iValue *= 1000;

										iValue /= (iPathTurns + 2);

										if (iValue > iBestValue) {
											iBestValue = iValue;
											pBestPlot = getPathEndTurnPlot();
											pBestGuardPlot = pLoopPlot;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestGuardPlot != NULL)) {
		if (atPlot(pBestGuardPlot)) {
			getGroup()->pushMission(isFortifyable() ? MISSION_FORTIFY : MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_BONUS, pBestGuardPlot);
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_BONUS, pBestGuardPlot);
			return true;
		}
	}

	return false;
}
// Returns true if a mission was pushed...
bool CvUnitAI::AI_guardCitySite() {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestGuardPlot = NULL;

	for (int iI = 0; iI < GET_PLAYER(getOwnerINLINE()).AI_getNumCitySites(); iI++) {
		CvPlot* pLoopPlot = GET_PLAYER(getOwnerINLINE()).AI_getCitySite(iI);
		if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_GUARD_CITY, getGroup()) == 0) {
			// K-Mod. I've switched the order of the following two if statements, for efficiency.
			int iValue = pLoopPlot->getFoundValue(getOwnerINLINE());
			if (iValue > iBestValue) {
				int iPathTurns;
				if (generatePath(pLoopPlot, 0, true, &iPathTurns)) {
					iBestValue = iValue;
					pBestPlot = getPathEndTurnPlot();
					pBestGuardPlot = pLoopPlot;
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestGuardPlot != NULL)) {
		if (atPlot(pBestGuardPlot)) {
			getGroup()->pushMission(isFortifyable() ? MISSION_FORTIFY : MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_CITY, pBestGuardPlot);
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_CITY, pBestGuardPlot);
			return true;
		}
	}

	return false;
}



// Returns true if a mission was pushed...
bool CvUnitAI::AI_guardSpy(int iRandomPercent) {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestGuardPlot = NULL;

	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if (AI_plotValid(pLoopCity->plot())) {
			// BBAI efficiency: check area for land units
			if ((getDomainType() == DOMAIN_LAND) && (pLoopCity->area() != area()) && !(getGroup()->canMoveAllTerrain())) {
				continue;
			}

			int iValue = 0;

			if (GET_PLAYER(getOwnerINLINE()).AI_isDoVictoryStrategy(AI_VICTORY_SPACE4)) {
				if (pLoopCity->isCapital()) {
					iValue += 30;
				} else if (pLoopCity->isProductionProject()) {
					iValue += 5;
				}
			}

			if (GET_PLAYER(getOwnerINLINE()).AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3)) {
				if (pLoopCity->getCultureLevel() >= (GC.getNumCultureLevelInfos() - 2)) {
					iValue += 10;
				}
			}

			if (pLoopCity->isProductionUnit()) {
				if (isLimitedUnitClass((UnitClassTypes)(GC.getUnitInfo(pLoopCity->getProductionUnit()).getUnitClassType()))) {
					iValue += 4;
				}
			} else if (pLoopCity->isProductionBuilding()) {
				if (isLimitedWonderClass((BuildingClassTypes)(GC.getBuildingInfo(pLoopCity->getProductionBuilding()).getBuildingClassType()))) {
					iValue += 5;
				}
			} else if (pLoopCity->isProductionProject()) {
				if (isLimitedProject(pLoopCity->getProductionProject())) {
					iValue += 6;
				}
			}

			if (iValue > 0) {
				if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_GUARD_SPY, getGroup()) == 0) {
					int iPathTurns;
					if (generatePath(pLoopCity->plot(), 0, true, &iPathTurns)) {
						iValue *= 100 + GC.getGameINLINE().getSorenRandNum(iRandomPercent, "AI Guard Spy");
						//iValue /= 100;
						iValue /= iPathTurns + 1;

						if (iValue > iBestValue) {
							iBestValue = iValue;
							pBestPlot = getPathEndTurnPlot();
							pBestGuardPlot = pLoopCity->plot();
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestGuardPlot != NULL)) {
		if (atPlot(pBestGuardPlot)) {
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_SPY, pBestGuardPlot);
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_SPY, pBestGuardPlot);
			return true;
		}
	}

	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_chokeDefend() {
	FAssert(AI_isCityAIType());

	// XXX what about amphib invasions?

	CvCity* pCity = plot()->getPlotCity();
	if (pCity != NULL) {
		if (pCity->getOwnerINLINE() == getOwnerINLINE()) {
			if (pCity->AI_neededDefenders() > 1) {
				if (pCity->AI_isDefended(pCity->plot()->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE(), NO_TEAM, PUF_isNotCityAIType))) {
					int iPlotDanger = GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 3);

					if (iPlotDanger <= 4) {
						//if (AI_anyAttack(1, 65, 0, std::max(0, (iPlotDanger - 1))))
						if (AI_anyAttack(1, 65, 0, iPlotDanger > 1 ? 2 : 0)) // K-Mod
						{
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}


// Returns true if a mission was pushed...
// Heals the unit if it's damage is greater than the percent provided
// The damage percent is the damage taken so AI_heal(30) will heal a unit if it is at 69/100 health
bool CvUnitAI::AI_heal(int iDamagePercent, int iMaxPath) {
	PROFILE_FUNC();

	if (plot()->getFeatureType() != NO_FEATURE) {
		if (GC.getFeatureInfo(plot()->getFeatureType()).getTurnDamage() != 0) {
			//Pass through
			//(actively seeking a safe spot may result in unit getting stuck)
			return false;
		}
	}

	CvSelectionGroup* pGroup = getGroup();

	// If we can heal in 1 turn then do it, irrespective of the daamge we have taken
	if (getGroup()->getNumUnits() == 1) {
		// We don't want to hang around whilst being attacked from a fort if we are on our own
		if (plot()->isSubjectToFortAttack())
			return false;

		if (getDamage() > 0) {
			if (plot()->isCity() || (healTurns(plot()) == 1)) {
				if (!(isAlwaysHeal())) {
					getGroup()->pushMission(MISSION_HEAL, -1, -1, 0, false, false, MISSIONAI_HEAL);
					return true;
				}
			}
		}
	}

	iMaxPath = std::max(iMaxPath, 2);

	int iTotalDamage = 0;
	int iTotalHitpoints = 0;
	int iHurtUnitCount = 0;
	std::vector<CvUnit*> aeDamagedUnits;

	// If we are grouped and this routine is called set a minimum threshold
	iDamagePercent = std::max(10, iDamagePercent);
	CLLNode<IDInfo>* pEntityNode = getGroup()->headUnitNode();
	while (pEntityNode != NULL) {
		CvUnit* pLoopUnit = ::getUnit(pEntityNode->m_data);
		FAssert(pLoopUnit != NULL);
		pEntityNode = pGroup->nextUnitNode(pEntityNode);

		int iDamageThreshold = (pLoopUnit->maxHitPoints() * iDamagePercent) / 100;

		if (NO_UNIT != getLeaderUnitType()) {
			iDamageThreshold /= 2;
		}

		if (pLoopUnit->getDamage() > 0) {
			iHurtUnitCount++;
		}
		iTotalDamage += pLoopUnit->getDamage();
		iTotalHitpoints += pLoopUnit->maxHitPoints();


		if (pLoopUnit->getDamage() > iDamageThreshold) {
			if (!(pLoopUnit->hasMoved())) {
				if (!(pLoopUnit->isAlwaysHeal())) {
					if (pLoopUnit->healTurns(pLoopUnit->plot()) <= iMaxPath) {
						aeDamagedUnits.push_back(pLoopUnit);
					}
				}
			}
		}
	}
	if (iHurtUnitCount == 0) {
		return false;
	}

	bool bPushedMission = false;
	if (plot()->isCity() && (plot()->getOwnerINLINE() == getOwnerINLINE())) {
		FAssertMsg(((int)aeDamagedUnits.size()) <= iHurtUnitCount, "damaged units array is larger than our hurt unit count");

		for (unsigned int iI = 0; iI < aeDamagedUnits.size(); iI++) {
			CvUnit* pUnitToHeal = aeDamagedUnits[iI];
			pUnitToHeal->joinGroup(NULL);
			pUnitToHeal->getGroup()->pushMission(MISSION_HEAL, -1, -1, 0, false, false, MISSIONAI_HEAL);

			// note, removing the head unit from a group will force the group to be completely split if non-human
			if (pUnitToHeal == this) {
				bPushedMission = true;
			}

			iHurtUnitCount--;
		}
	}

	if ((iHurtUnitCount * 2) > pGroup->getNumUnits()) {
		FAssertMsg(pGroup->getNumUnits() > 0, "group now has zero units");

		if (AI_moveIntoCity(2)) {
			return true;
		} else if (healRate(plot()) > 10) {
			pGroup->pushMission(MISSION_HEAL, -1, -1, 0, false, false, MISSIONAI_HEAL);
			return true;
		}
	}

	return bPushedMission;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_afterAttack() {
	if (!isMadeAttack()) {
		return false;
	}

	if (!canFight()) {
		return false;
	}

	if (isBlitz()) {
		return false;
	}

	// K-Mod. Large groups may still have important stuff to do!
	if (getGroup()->getNumUnits() > 2)
		return false;

	if (getDomainType() == DOMAIN_LAND) {
		if (AI_guardCity(false, true, 1)) {
			return true;
		}

		// K-Mod. We might be able to capture an undefended city, or at least a worker. (think paratrooper)
		// (note: it's also possible that we are asking our group partner to attack something.)
		// (also, note that AI_anyAttack will favour undefended cities over workers.)
		if (AI_anyAttack(1, 65)) {
			return true;
		}
	}

	if (AI_pillageRange(1)) {
		return true;
	}

	if (AI_retreatToCity(false, false, 1)) {
		return true;
	}

	if (AI_hide()) {
		return true;
	}

	if (AI_goody(1)) {
		return true;
	}

	if (AI_pillageRange(2)) {
		return true;
	}

	if (AI_defend()) {
		return true;
	}

	if (AI_safety()) {
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_goldenAge() {
	if (canGoldenAge(plot())) {
		getGroup()->pushMission(MISSION_GOLDEN_AGE);
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
// This function has been edited for K-Mod
bool CvUnitAI::AI_spreadReligion() {
	PROFILE_FUNC();

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	bool bCultureVictory = kOwner.AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2);

	ReligionTypes eReligion = NO_RELIGION;

	if (kOwner.getStateReligion() != NO_RELIGION) {
		if (m_pUnitInfo->getReligionSpreads(kOwner.getStateReligion()) > 0) {
			eReligion = kOwner.getStateReligion();
		}
	}

	if (eReligion == NO_RELIGION) {
		for (int iI = 0; iI < GC.getNumReligionInfos(); iI++) {
			if (m_pUnitInfo->getReligionSpreads((ReligionTypes)iI) > 0) {
				eReligion = ((ReligionTypes)iI);
				break;
			}
		}
	}

	if (eReligion == NO_RELIGION) {
		return false;
	}

	bool bHasHolyCity = GET_TEAM(getTeam()).hasHolyCity(eReligion);
	bool bHasAnyHolyCity = bHasHolyCity;
	if (!bHasAnyHolyCity) {
		for (int iI = 0; !bHasAnyHolyCity && iI < GC.getNumReligionInfos(); iI++) {
			bHasAnyHolyCity = GET_TEAM(getTeam()).hasHolyCity((ReligionTypes)iI);
		}
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestSpreadPlot = NULL;

	// BBAI TODO: Could also use CvPlayerAI::AI_missionaryValue to determine which player to target ...
	for (int iI = 0; iI < MAX_PLAYERS; iI++) {
		const CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);

		if (kLoopPlayer.isAlive()) {
			int iPlayerMultiplierPercent = 0;

			if (kLoopPlayer.getTeam() != getTeam() && canEnterTerritory(kLoopPlayer.getTeam())) {
				if (bHasHolyCity) {
					iPlayerMultiplierPercent = 100;
					if (!bCultureVictory || (eReligion == kOwner.getStateReligion())) {
						if (kLoopPlayer.getStateReligion() == NO_RELIGION) {
							if (0 == (kLoopPlayer.getNonStateReligionHappiness())) {
								iPlayerMultiplierPercent += 600;
							}
						} else if (kLoopPlayer.getStateReligion() == eReligion) {
							iPlayerMultiplierPercent += 300;
						} else {
							if (kLoopPlayer.hasHolyCity(kLoopPlayer.getStateReligion())) {
								iPlayerMultiplierPercent += 50;
							} else {
								iPlayerMultiplierPercent += 300;
							}
						}

						int iReligionCount = kLoopPlayer.countTotalHasReligion();
						int iCityCount = kLoopPlayer.getNumCities(); // K-Mod!
						//magic formula to produce normalized adjustment factor based on religious infusion
						int iAdjustment = (100 * (iCityCount + 1));
						iAdjustment /= ((iCityCount + 1) + iReligionCount);
						iAdjustment = (((iAdjustment - 25) * 4) / 3);

						iAdjustment = std::max(10, iAdjustment);

						iPlayerMultiplierPercent *= iAdjustment;
						iPlayerMultiplierPercent /= 100;
					}
				}
			} else if (iI == getOwnerINLINE()) {
				iPlayerMultiplierPercent = (bCultureVictory ? 1600 : 400) + (kOwner.getStateReligion() == eReligion ? 100 : 0);
			} else if (bHasHolyCity && kLoopPlayer.getTeam() == getTeam()) {
				iPlayerMultiplierPercent = kLoopPlayer.getStateReligion() == eReligion ? 600 : 300;
			}

			if (iPlayerMultiplierPercent > 0) {
				int iLoop;
				for (CvCity* pLoopCity = kLoopPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kLoopPlayer.nextCity(&iLoop)) {
					if (AI_plotValid(pLoopCity->plot()) && pLoopCity->area() == area()) {
						if (kOwner.AI_deduceCitySite(pLoopCity) && canSpread(pLoopCity->plot(), eReligion)) {
							if (!(pLoopCity->plot()->isVisibleEnemyUnit(this))) {
								if (kOwner.AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_SPREAD, getGroup()) == 0) {
									int iPathTurns;
									if (generatePath(pLoopCity->plot(), MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns)) {
										int iValue = 16 + pLoopCity->getPopulation() * 4; // was 7 +

										iValue *= iPlayerMultiplierPercent;
										iValue /= 100;

										int iCityReligionCount = pLoopCity->getReligionCount();
										int iReligionCountFactor = iCityReligionCount;

										if (kLoopPlayer.getTeam() == kOwner.getTeam()) {
											// count cities with no religion the same as cities with 2 religions
											// prefer a city with exactly 1 religion already
											if (iCityReligionCount == 0) {
												iReligionCountFactor = 2;
											} else if (iCityReligionCount == 1) {
												iValue *= 2;
											}
										} else {
											// absolutely prefer cities with zero religions
											if (iCityReligionCount == 0) {
												iValue *= 2;
											}

											// not our city, so prefer the lowest number of religions (increment so no divide by zero)
											iReligionCountFactor++;
										}

										iValue /= iReligionCountFactor;

										FAssert(iPathTurns > 0);

										bool bForceMove = false;
										if (isHuman()) {
											//If human, prefer to spread to the player where automated from.
											if (plot()->getOwnerINLINE() == pLoopCity->getOwnerINLINE()) {
												iValue *= 10;
												if (pLoopCity->isRevealed(getTeam(), false)) {
													bForceMove = true;
												}
											}
										}

										iValue *= 1000;

										if (iPathTurns > 0)
											iValue /= (iPathTurns + 2);

										if (iValue > iBestValue) {
											iBestValue = iValue;
											pBestPlot = bForceMove ? pLoopCity->plot() : getPathEndTurnPlot();
											pBestSpreadPlot = pLoopCity->plot();
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestSpreadPlot != NULL)) {
		if (atPlot(pBestSpreadPlot)) {
			getGroup()->pushMission(MISSION_SPREAD, eReligion);
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY, false, false, MISSIONAI_SPREAD, pBestSpreadPlot);
			return true;
		}
	}

	return false;
}


// K-Mod: I've basically rewritten this whole function.
// Returns true if a mission was pushed...
bool CvUnitAI::AI_spreadCorporation() {
	PROFILE_FUNC();

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	CorporationTypes eCorporation = NO_CORPORATION;

	for (int iI = 0; iI < GC.getNumCorporationInfos(); ++iI) {
		if (m_pUnitInfo->getCorporationSpreads((CorporationTypes)iI) > 0) {
			eCorporation = ((CorporationTypes)iI);
			break;
		}
	}

	if (NO_CORPORATION == eCorporation || !kOwner.isActiveCorporation(eCorporation)) {
		return false;
	}
	bool bHasHQ = GET_TEAM(getTeam()).hasHeadquarters(eCorporation);

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestSpreadPlot = NULL;

	// first, if we are already doing a spread mission, continue that.
	if (getGroup()->AI_getMissionAIType() == MISSIONAI_SPREAD_CORPORATION) {
		CvPlot* pMissionPlot = getGroup()->AI_getMissionAIPlot();
		if (pMissionPlot != NULL &&
			pMissionPlot->getPlotCity() != NULL &&
			canSpreadCorporation(pMissionPlot, eCorporation, true) && // don't check gold here
			!pMissionPlot->isVisibleEnemyUnit(this) &&
			generatePath(pMissionPlot, MOVE_NO_ENEMY_TERRITORY, true)) {
			pBestPlot = getPathEndTurnPlot();
			pBestSpreadPlot = pMissionPlot;
		}
	}

	if (pBestSpreadPlot == NULL) {
		PlayerTypes eTargetPlayer = NO_PLAYER;

		if (isHuman())
			eTargetPlayer = plot()->isOwned() ? plot()->getOwnerINLINE() : getOwnerINLINE();

		if (eTargetPlayer == NO_PLAYER ||
			GET_PLAYER(eTargetPlayer).isNoCorporations() || GET_PLAYER(eTargetPlayer).isNoForeignCorporations() ||
			GET_PLAYER(eTargetPlayer).countCorporations(eCorporation, area()) >= area()->getCitiesPerPlayer(eTargetPlayer)) {
			if (kOwner.AI_executiveValue(area(), eCorporation, &eTargetPlayer, true) <= 0)
				return false; // corp is not worth spreading in this region.
		}

		for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++) {
			const CvPlayerAI& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
			if (kLoopPlayer.isAlive() && canEnterTerritory(kLoopPlayer.getTeam()) && area()->getCitiesPerPlayer((PlayerTypes)iI) > 0) {
				AttitudeTypes eAttitude = GET_TEAM(getTeam()).AI_getAttitude(kLoopPlayer.getTeam());
				if (iI == eTargetPlayer || getTeam() == kLoopPlayer.getTeam()) {
					int iLoop;
					for (CvCity* pLoopCity = kLoopPlayer.firstCity(&iLoop); pLoopCity; pLoopCity = kLoopPlayer.nextCity(&iLoop)) {
						if (AI_plotValid(pLoopCity->plot()) &&
							pLoopCity->getArea() == getArea() &&
							kOwner.AI_deduceCitySite(pLoopCity) &&
							canSpreadCorporation(pLoopCity->plot(), eCorporation) &&
							!pLoopCity->plot()->isVisibleEnemyUnit(this) &&
							kOwner.AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_SPREAD_CORPORATION, getGroup()) == 0) {
							int iPathTurns;
							if (generatePath(pLoopCity->plot(), MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns)) {
								int iValue = 0;
								iValue += bHasHQ ? 1000 : 0; // we should probably calculate the true HqValue, but I couldn't be bothered right now.
								if (pLoopCity->getTeam() == getTeam()) {
									const CvPlayerAI& kCityOwner = GET_PLAYER(pLoopCity->getOwnerINLINE());
									iValue += kCityOwner.AI_corporationValue(eCorporation, pLoopCity);

									for (CorporationTypes i = (CorporationTypes)0; i < GC.getNumCorporationInfos(); i = (CorporationTypes)(i + 1)) {
										if (pLoopCity->isHasCorporation(i) && GC.getGameINLINE().isCompetingCorporation(i, eCorporation)) {
											iValue -= kCityOwner.AI_corporationValue(i, pLoopCity) + (GET_TEAM(getTeam()).hasHeadquarters(i) ? 1100 : 100);
											// cf. iValue before AI_corporationValue is added.
										}
									}
								}

								if (iValue < 0)
									continue;

								iValue += 10 + pLoopCity->getPopulation() * 2; // bigger are expected to spread corps faster, I guess.

								if (iI == eTargetPlayer)
									iValue *= 2;

								iValue *= 1000;

								iValue /= (iPathTurns + 1);

								if (iValue > iBestValue) {
									iBestValue = iValue;
									pBestPlot = isHuman() ? pLoopCity->plot() : getPathEndTurnPlot();
									pBestSpreadPlot = pLoopCity->plot();
								}
							}
						}
					}
				}
			}
		}
	}

	// (original code deleted)

	if (pBestPlot != NULL && pBestSpreadPlot != NULL) {
		if (atPlot(pBestSpreadPlot)) {
			if (canSpreadCorporation(pBestSpreadPlot, eCorporation)) {
				getGroup()->pushMission(MISSION_SPREAD_CORPORATION, eCorporation);
				return true;
			} else if (GET_PLAYER(getOwnerINLINE()).getGold() < spreadCorporationCost(eCorporation, pBestSpreadPlot->getPlotCity())) {
				// wait for more money
				getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_SPREAD_CORPORATION, pBestSpreadPlot);
				return true;
			}
			// FAssertMsg(false, "AI_spreadCorporation has taken us to a bogus pBestSpreadPlot");
			// this can happen from time to time. For example, when the player loses their only corp resources while the exec is en route.
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY, false, false, MISSIONAI_SPREAD_CORPORATION, pBestSpreadPlot);
			return true;
		}
	}

	return false;
}

bool CvUnitAI::AI_spreadReligionAirlift() {
	PROFILE_FUNC();

	if (getGroup()->getNumUnits() > 1) {
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();
	if (pCity == NULL) {
		return false;
	}

	if (pCity->getMaxAirlift() == 0) {
		return false;
	}

	ReligionTypes eReligion = NO_RELIGION;

	if (eReligion == NO_RELIGION) {
		if (GET_PLAYER(getOwnerINLINE()).getStateReligion() != NO_RELIGION) {
			if (m_pUnitInfo->getReligionSpreads(GET_PLAYER(getOwnerINLINE()).getStateReligion()) > 0) {
				eReligion = GET_PLAYER(getOwnerINLINE()).getStateReligion();
			}
		}
	}

	if (eReligion == NO_RELIGION) {
		for (int iI = 0; iI < GC.getNumReligionInfos(); iI++) {
			if (m_pUnitInfo->getReligionSpreads((ReligionTypes)iI) > 0) {
				eReligion = ((ReligionTypes)iI);
				break;
			}
		}
	}

	if (eReligion == NO_RELIGION) {
		return false;
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	for (int iI = 0; iI < MAX_PLAYERS; iI++) {
		CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
		if (kLoopPlayer.isAlive() && (getTeam() == kLoopPlayer.getTeam())) {
			int iLoop;
			for (CvCity* pLoopCity = kLoopPlayer.firstCity(&iLoop); NULL != pLoopCity; pLoopCity = kLoopPlayer.nextCity(&iLoop)) {
				if (canAirliftAt(pCity->plot(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE())) {
					if (canSpread(pLoopCity->plot(), eReligion)) {
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_SPREAD, getGroup()) == 0) {
							// Don't airlift where there's already one of our unit types (probably just airlifted)
							if (pLoopCity->plot()->plotCount(PUF_isUnitType, getUnitType(), -1, getOwnerINLINE()) > 0) {
								continue;
							}
							int iValue = (7 + (pLoopCity->getPopulation() * 4));

							int iCityReligionCount = pLoopCity->getReligionCount();
							int iReligionCountFactor = iCityReligionCount;

							// count cities with no religion the same as cities with 2 religions
							// prefer a city with exactly 1 religion already
							if (iCityReligionCount == 0) {
								iReligionCountFactor = 2;
							} else if (iCityReligionCount == 1) {
								iValue *= 2;
							}

							iValue /= iReligionCountFactor;
							if (iValue > iBestValue) {
								iBestValue = iValue;
								pBestPlot = pLoopCity->plot();
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		getGroup()->pushMission(MISSION_AIRLIFT, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_SPREAD, pBestPlot);
		return true;
	}

	return false;
}

bool CvUnitAI::AI_spreadCorporationAirlift() {
	PROFILE_FUNC();

	if (getGroup()->getNumUnits() > 1) {
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();

	if (pCity == NULL) {
		return false;
	}

	if (pCity->getMaxAirlift() == 0) {
		return false;
	}

	CorporationTypes eCorporation = NO_CORPORATION;

	for (int iI = 0; iI < GC.getNumCorporationInfos(); ++iI) {
		if (m_pUnitInfo->getCorporationSpreads((CorporationTypes)iI) > 0) {
			eCorporation = ((CorporationTypes)iI);
			break;
		}
	}

	if (NO_CORPORATION == eCorporation) {
		return false;
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	for (int iI = 0; iI < MAX_PLAYERS; iI++) {
		CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
		if (kLoopPlayer.isAlive() && (getTeam() == kLoopPlayer.getTeam())) {
			int iLoop;
			for (CvCity* pLoopCity = kLoopPlayer.firstCity(&iLoop); NULL != pLoopCity; pLoopCity = kLoopPlayer.nextCity(&iLoop)) {
				if (canAirliftAt(pCity->plot(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE())) {
					if (canSpreadCorporation(pLoopCity->plot(), eCorporation)) {
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_SPREAD_CORPORATION, getGroup()) == 0) {
							// Don't airlift where there's already one of our unit types (probably just airlifted)
							if (pLoopCity->plot()->plotCount(PUF_isUnitType, getUnitType(), -1, getOwnerINLINE()) > 0) {
								continue;
							}

							int iValue = (pLoopCity->getPopulation() * 4);

							if (pLoopCity->getOwnerINLINE() == getOwnerINLINE()) {
								iValue *= 4;
							} else {
								iValue *= 3;
							}

							if (iValue > iBestValue) {
								iBestValue = iValue;
								pBestPlot = pLoopCity->plot();
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		getGroup()->pushMission(MISSION_AIRLIFT, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_SPREAD, pBestPlot);
		return true;
	}

	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_discover(bool bThisTurnOnly, bool bFirstResearchOnly) {
	int iPercentWasted = 0;

	if (canDiscover(plot())) {
		TechTypes eDiscoverTech = getDiscoveryTech();
		bool bIsFirstTech = (GET_PLAYER(getOwnerINLINE()).AI_isFirstTech(eDiscoverTech));

		if (bFirstResearchOnly && !bIsFirstTech) {
			return false;
		}

		iPercentWasted = (100 - ((getDiscoverResearch(eDiscoverTech) * 100) / getDiscoverResearch(NO_TECH)));
		FAssert(((iPercentWasted >= 0) && (iPercentWasted <= 100)));


		if (getDiscoverResearch(eDiscoverTech) >= GET_TEAM(getTeam()).getResearchLeft(eDiscoverTech)) {
			if ((iPercentWasted < 51) && bFirstResearchOnly && bIsFirstTech) {
				getGroup()->pushMission(MISSION_DISCOVER);
				return true;
			}

			if (iPercentWasted < (bIsFirstTech ? 31 : 11)) {
				//I need a good way to assess if the tech is actually valuable...
				//but don't have one.
				getGroup()->pushMission(MISSION_DISCOVER);
				return true;
			}
		} else if (bThisTurnOnly) {
			return false;
		}

		if (iPercentWasted <= 11) {
			if (GET_PLAYER(getOwnerINLINE()).getCurrentResearch() == eDiscoverTech) {
				getGroup()->pushMission(MISSION_DISCOVER);
				return true;
			}
		}
	}
	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_lead(std::vector<UnitAITypes>& aeUnitAITypes) {
	PROFILE_FUNC();

	FAssertMsg(!isHuman(), "isHuman did not return false as expected");
	FAssertMsg(AI_getUnitAIType() != NO_UNITAI, "AI_getUnitAIType() is not expected to be equal with NO_UNITAI");
	FAssert(NO_PLAYER != getOwnerINLINE());

	CvPlayer& kOwner = GET_PLAYER(getOwnerINLINE());

	bool bNeedLeader = false;
	for (int iI = 0; iI < MAX_CIV_TEAMS; iI++) {
		CvTeamAI& kLoopTeam = GET_TEAM((TeamTypes)iI);
		if (isEnemy((TeamTypes)iI)) {
			if (kLoopTeam.countNumUnitsByArea(area()) > 0) {
				bNeedLeader = true;
				break;
			}
		}
	}

	CvUnit* pBestUnit = NULL;
	CvPlot* pBestPlot = NULL;

	// AI may use Warlords to create super-medic units
	CvUnit* pBestStrUnit = NULL;
	CvPlot* pBestStrPlot = NULL;

	CvUnit* pBestHealUnit = NULL;
	CvPlot* pBestHealPlot = NULL;

	if (bNeedLeader) {
		int iBestStrength = 0;
		int iBestHealing = 0;
		int iLoop;
		for (CvUnit* pLoopUnit = kOwner.firstUnit(&iLoop); pLoopUnit; pLoopUnit = kOwner.nextUnit(&iLoop)) {
			bool bValid = isWorldUnitClass(pLoopUnit->getUnitClassType());

			if (!bValid) {
				for (uint iI = 0; iI < aeUnitAITypes.size(); iI++) {
					if (pLoopUnit->AI_getUnitAIType() == aeUnitAITypes[iI] || NO_UNITAI == aeUnitAITypes[iI]) {
						bValid = true;
						break;
					}
				}
			}

			if (bValid) {
				if (canLead(pLoopUnit->plot(), pLoopUnit->getID()) > 0) {
					if (AI_plotValid(pLoopUnit->plot())) {
						if (!(pLoopUnit->plot()->isVisibleEnemyUnit(this))) {
							if (pLoopUnit->combatLimit() == 100) {
								if (generatePath(pLoopUnit->plot(), MOVE_AVOID_ENEMY_WEIGHT_3, true)) {
									// pick the unit with the highest current strength
									int iCombatStrength = pLoopUnit->currCombatStr(NULL, NULL);

									iCombatStrength *= 30 + pLoopUnit->getExperience();
									iCombatStrength /= 30;

									if (GC.getUnitClassInfo(pLoopUnit->getUnitClassType()).getMaxGlobalInstances() > -1) {
										iCombatStrength *= 1 + GC.getUnitClassInfo(pLoopUnit->getUnitClassType()).getMaxGlobalInstances();
										iCombatStrength /= std::max(1, GC.getUnitClassInfo(pLoopUnit->getUnitClassType()).getMaxGlobalInstances());
									}

									if (iCombatStrength > iBestStrength) {
										iBestStrength = iCombatStrength;
										pBestStrUnit = pLoopUnit;
										pBestStrPlot = getPathEndTurnPlot();
									}

									// or the unit with the best healing ability
									int iHealing = pLoopUnit->getSameTileHeal() + pLoopUnit->getAdjacentTileHeal();
									if (iHealing > iBestHealing) {
										iBestHealing = iHealing;
										pBestHealUnit = pLoopUnit;
										pBestHealPlot = getPathEndTurnPlot();
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (AI_getBirthmark() % 3 == 0 && pBestHealUnit != NULL) {
		pBestPlot = pBestHealPlot;
		pBestUnit = pBestHealUnit;
	} else {
		pBestPlot = pBestStrPlot;
		pBestUnit = pBestStrUnit;
	}

	if (pBestPlot) {
		if (atPlot(pBestPlot) && pBestUnit) {
			if (gUnitLogLevel > 2) {
				CvWString szString;
				getUnitAIString(szString, pBestUnit->AI_getUnitAIType());

				logBBAI("      Great general %d for %S chooses to lead %S with UNITAI %S", getID(), GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), pBestUnit->getName(0).GetCString(), szString.GetCString());
			}
			getGroup()->pushMission(MISSION_LEAD, pBestUnit->getID());
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_AVOID_ENEMY_WEIGHT_3);
			return true;
		}
	}

	return false;
}

// Returns true if a mission was pushed... 
// iMaxCounts = 1 would mean join a city if there's no existing joined GP of that type.
bool CvUnitAI::AI_join(int iMaxCount, bool bCitySize) {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	SpecialistTypes eBestSpecialist = NO_SPECIALIST;
	int iCount = 0;

	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if ((pLoopCity->area() == area()) && AI_plotValid(pLoopCity->plot())) {
			if (!(pLoopCity->plot()->isVisibleEnemyUnit(this))) {
				if (generatePath(pLoopCity->plot(), MOVE_SAFE_TERRITORY, true)) {
					for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
						bool bDoesJoin = false;
						if (m_pUnitInfo->getGreatPeoples(eSpecialist) || m_pUnitInfo->getSlaveSpecialistType() == eSpecialist) {
							bDoesJoin = true;
						}
						if (bDoesJoin) {
							iCount += pLoopCity->getSpecialistCount(eSpecialist);
							if (bCitySize && (pLoopCity->getPopulation() <= pLoopCity->getSpecialistCount(eSpecialist))) {
								return false;
							} else if (iCount >= iMaxCount) {
								return false;
							}
						}

						if (canJoin(pLoopCity->plot(), eSpecialist)) {
							if (!(GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(pLoopCity->plot(), 2))) {
								int iValue = pLoopCity->AI_permanentSpecialistValue(eSpecialist); // K-Mod
								if (iValue > iBestValue) {
									iBestValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									eBestSpecialist = eSpecialist;
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (eBestSpecialist != NO_SPECIALIST)) {
		if (atPlot(pBestPlot)) {
			getGroup()->pushMission(MISSION_JOIN, eBestSpecialist);
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_SAFE_TERRITORY);
			return true;
		}
	}

	return false;
}

// Returns true if a mission was pushed... 
// iMaxCount = 1 would mean construct only if there are no existing buildings 
//   constructed by this GP type.
bool CvUnitAI::AI_construct(int iMaxCount, int iMaxSingleBuildingCount, int iThreshold) {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestConstructPlot = NULL;
	BuildingTypes eBestBuilding = NO_BUILDING;
	int iCount = 0;

	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if (AI_plotValid(pLoopCity->plot()) && pLoopCity->area() == area()) {
			if (!(pLoopCity->plot()->isVisibleEnemyUnit(this))) {
				for (int iI = 0; iI < GC.getNumBuildingClassInfos(); iI++) {
					BuildingTypes eBuilding = (BuildingTypes)GC.getCivilizationInfo(getCivilizationType()).getCivilizationBuildings(iI);

					if (NO_BUILDING != eBuilding) {
						bool bDoesBuild = false;
						if ((m_pUnitInfo->getForceBuildings(eBuilding))
							|| (m_pUnitInfo->getBuildings(eBuilding))) {
							bDoesBuild = true;
						}

						if (bDoesBuild && (pLoopCity->getNumBuilding(eBuilding) > 0)) {
							iCount++;
							if (iCount >= iMaxCount) {
								return false;
							}
						}

						if (bDoesBuild && GET_PLAYER(getOwnerINLINE()).getBuildingClassCount((BuildingClassTypes)GC.getBuildingInfo(eBuilding).getBuildingClassType()) < iMaxSingleBuildingCount) {
							if (canConstruct(pLoopCity->plot(), eBuilding) && generatePath(pLoopCity->plot(), MOVE_NO_ENEMY_TERRITORY, true)) {
								int iValue = pLoopCity->AI_buildingValue(eBuilding);

								if ((iValue > iThreshold) && (iValue > iBestValue)) {
									iBestValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									pBestConstructPlot = pLoopCity->plot();
									eBestBuilding = eBuilding;
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestConstructPlot != NULL) && (eBestBuilding != NO_BUILDING)) {
		if (atPlot(pBestConstructPlot)) {
			getGroup()->pushMission(MISSION_CONSTRUCT, eBestBuilding);
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY, false, false, MISSIONAI_CONSTRUCT, pBestConstructPlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_switchHurry() {
	CvCity* pCity = plot()->getPlotCity();
	if ((pCity == NULL) || (pCity->getOwnerINLINE() != getOwnerINLINE())) {
		return false;
	}

	int iBestValue = 0;
	BuildingTypes eBestBuilding = NO_BUILDING;

	for (int iI = 0; iI < GC.getNumBuildingClassInfos(); iI++) {
		if (isWorldWonderClass((BuildingClassTypes)iI)) {
			BuildingTypes eBuilding = (BuildingTypes)GC.getCivilizationInfo(getCivilizationType()).getCivilizationBuildings(iI);

			if (NO_BUILDING != eBuilding) {
				if (pCity->canConstruct(eBuilding)) {
					if (pCity->getBuildingProduction(eBuilding) == 0) {
						if (getMaxHurryProduction(pCity) >= pCity->getProductionNeeded(eBuilding)) {
							int iValue = pCity->AI_buildingValue(eBuilding);

							if (iValue > iBestValue) {
								iBestValue = iValue;
								eBestBuilding = eBuilding;
							}
						}
					}
				}
			}
		}
	}

	if (eBestBuilding != NO_BUILDING) {
		pCity->pushOrder(ORDER_CONSTRUCT, eBestBuilding);

		if (pCity->getProductionBuilding() == eBestBuilding) {
			if (canHurry(plot())) {
				getGroup()->pushMission(MISSION_HURRY);
				return true;
			}
		}

		FAssert(false);
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_hurry() {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestHurryPlot = NULL;

	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if ((pLoopCity->area() == area()) && AI_plotValid(pLoopCity->plot())) {
			if (canHurry(pLoopCity->plot())) {
				if (!(pLoopCity->plot()->isVisibleEnemyUnit(this))) {
					if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_HURRY, getGroup()) == 0) {
						int iPathTurns;
						if (generatePath(pLoopCity->plot(), MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns)) {
							bool bHurry = false;

							if (pLoopCity->isProductionBuilding()) {
								if (isWorldWonderClass((BuildingClassTypes)(GC.getBuildingInfo(pLoopCity->getProductionBuilding()).getBuildingClassType()))) {
									bHurry = true;
								}
							}

							if (bHurry) {
								int iTurnsLeft = pLoopCity->getProductionTurnsLeft();

								iTurnsLeft -= iPathTurns;

								if (iTurnsLeft > 8) {
									int iValue = iTurnsLeft;

									if (iValue > iBestValue) {
										iBestValue = iValue;
										pBestPlot = getPathEndTurnPlot();
										pBestHurryPlot = pLoopCity->plot();
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestHurryPlot != NULL)) {
		if (atPlot(pBestHurryPlot)) {
			getGroup()->pushMission(MISSION_HURRY);
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY, false, false, MISSIONAI_HURRY, pBestHurryPlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_offensiveAirlift() {
	PROFILE_FUNC();

	if (getGroup()->getNumUnits() > 1) {
		return false;
	}

	if (area()->getTargetCity(getOwnerINLINE()) != NULL) {
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();

	if (pCity == NULL) {
		return false;
	}

	if (pCity->getMaxAirlift() == 0) {
		return false;
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if (pLoopCity->area() != pCity->area()) {
			if (canAirliftAt(pCity->plot(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE())) {
				CvCity* pTargetCity = pLoopCity->area()->getTargetCity(getOwnerINLINE());

				if (pTargetCity != NULL) {
					if (GET_PLAYER(getOwnerINLINE()).AI_isLandWar(pTargetCity->area()) || pTargetCity->AI_isDanger()) // K-Mod
					{
						int iValue = 10000;

						iValue *= (GET_PLAYER(getOwnerINLINE()).AI_militaryWeight(pLoopCity->area()) + 10);
						iValue /= (GET_PLAYER(getOwnerINLINE()).AI_totalAreaUnitAIs(pLoopCity->area(), AI_getUnitAIType()) + 10);

						iValue += std::max(1, ((GC.getMapINLINE().maxStepDistance() * 2) - GC.getMapINLINE().calculatePathDistance(pLoopCity->plot(), pTargetCity->plot())));

						if (AI_getUnitAIType() == UNITAI_PARADROP) {
							CvCity* pNearestEnemyCity = GC.getMapINLINE().findCity(pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE(), NO_PLAYER, NO_TEAM, false, false, getTeam());

							if (pNearestEnemyCity != NULL) {
								int iDistance = plotDistance(pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE(), pNearestEnemyCity->getX_INLINE(), pNearestEnemyCity->getY_INLINE());
								if (iDistance <= getDropRange()) {
									iValue *= 5;
								}
							}
						}

						if (iValue > iBestValue) {
							iBestValue = iValue;
							pBestPlot = pLoopCity->plot();
							FAssert(pLoopCity != pCity);
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		getGroup()->pushMission(MISSION_AIRLIFT, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_paradrop(int iRange) {
	PROFILE_FUNC();

	if (getGroup()->getNumUnits() > 1) {
		return false;
	}
	int iParatrooperCount = plot()->plotCount(PUF_isUnitAIType, UNITAI_PARADROP, -1, getOwnerINLINE());
	FAssert(iParatrooperCount > 0);

	CvPlot* pPlot = plot();

	if (!canParadrop(pPlot)) {
		return false;
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	int iSearchRange = AI_searchRange(iRange);

	for (int iDX = -iSearchRange; iDX <= iSearchRange; ++iDX) {
		for (int iDY = -iSearchRange; iDY <= iSearchRange; ++iDY) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (isPotentialEnemy(pLoopPlot->getTeam(), pLoopPlot)) {
					if (canParadropAt(pPlot, pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE())) {
						int iValue = 0;

						PlayerTypes eTargetPlayer = pLoopPlot->getOwnerINLINE();
						FAssert(NO_PLAYER != eTargetPlayer);
						// Bonus values for bonuses the AI has are less than 10 for non-strategic resources... since this is
						// in the AI territory they probably have it
						if (NO_BONUS != pLoopPlot->getNonObsoleteBonusType(getTeam())) {
							iValue += std::max(1, GET_PLAYER(eTargetPlayer).AI_bonusVal(pLoopPlot->getBonusType(), 0) - 10);
						}

						for (int i = -1; i <= 1; ++i) {
							for (int j = -1; j <= 1; ++j) {
								CvPlot* pAdjacentPlot = plotXY(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), i, j);
								if (NULL != pAdjacentPlot) {
									CvCity* pAdjacentCity = pAdjacentPlot->getPlotCity();

									if (NULL != pAdjacentCity) {
										if (pAdjacentCity->getOwnerINLINE() == eTargetPlayer) {
											int iAttackerCount = GET_PLAYER(getOwnerINLINE()).AI_adjacentPotentialAttackers(pAdjacentPlot, true);
											int iDefenderCount = pAdjacentPlot->getNumVisibleEnemyDefenders(this);
											iValue += 20 * (AI_attackOdds(pAdjacentPlot, true) - ((50 * iDefenderCount) / (iParatrooperCount + iAttackerCount)));
										}
									}
								}
							}
						}

						if (iValue > 0) {
							iValue += pLoopPlot->defenseModifier(getTeam(), ignoreBuildingDefense());

							CvUnit* pInterceptor = bestInterceptor(pLoopPlot);
							if (NULL != pInterceptor) {
								int iInterceptProb = isSuicide() ? 100 : pInterceptor->currInterceptionProbability();

								iInterceptProb *= std::max(0, (100 - evasionProbability()));
								iInterceptProb /= 100;

								iValue *= std::max(0, 100 - iInterceptProb / 2);
								iValue /= 100;
							}
						}

						if (iValue > iBestValue) {
							iBestValue = iValue;
							pBestPlot = pLoopPlot;

							FAssert(pBestPlot != pPlot);
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		getGroup()->pushMission(MISSION_PARADROP, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_protect(int iOddsThreshold, int iMaxPathTurns, int iFlags) {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE()) {
			if (AI_plotValid(pLoopPlot)) {
				if (pLoopPlot->isVisibleEnemyUnit(this)) {
					if (!atPlot(pLoopPlot)) {
						// BBAI efficiency: Check area for land units
						if ((getDomainType() != DOMAIN_LAND) || (pLoopPlot->area() == area()) || getGroup()->canMoveAllTerrain()) {
							// BBAI efficiency: Most of the time, path will exist and odds will be checked anyway.  When path doesn't exist, checking path
							// takes longer.  Therefore, check odds first.
							int iValue = AI_getWeightedOdds(pLoopPlot, false); // K-Mod
							BonusTypes eBonus = pLoopPlot->getNonObsoleteBonusType(getTeam()); // K-Mod

							if (iValue >= iOddsThreshold && (eBonus != NO_BONUS || iValue * 50 > iBestValue)) // K-Mod
							{
								int iPathTurns;
								if (generatePath(pLoopPlot, iFlags, true, &iPathTurns, iMaxPathTurns)) {
									// BBAI TODO: Other units targeting this already (if path turns > 1 or 0)?
									if (iPathTurns <= iMaxPathTurns) {
										iValue *= 100;

										iValue /= (2 + iPathTurns);

										if (eBonus != NO_BONUS) {
											iValue *= 10 + GET_PLAYER(getOwnerINLINE()).AI_bonusVal(eBonus, 0);
											iValue /= 10;
										}

										if (iValue > iBestValue) {
											iBestValue = iValue;
											pBestPlot = getPathEndTurnPlot();
											FAssert(!atPlot(pBestPlot));
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), iFlags);
		return true;
	}

	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_patrol() {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
		CvPlot* pAdjacentPlot = plotDirection(getX_INLINE(), getY_INLINE(), ((DirectionTypes)iI));

		if (pAdjacentPlot != NULL) {
			if (AI_plotValid(pAdjacentPlot)) {
				if (!(pAdjacentPlot->isVisibleEnemyUnit(this))) {
					if (generatePath(pAdjacentPlot, 0, true)) {
						int iValue = (1 + GC.getGameINLINE().getSorenRandNum(10000, "AI Patrol"));

						if (isAnimal()) {
							if ((pAdjacentPlot->getFeatureType() != NO_FEATURE && getUnitInfo().getFeatureNative(pAdjacentPlot->getFeatureType())) || getUnitInfo().getTerrainNative(pAdjacentPlot->getTerrainType())) {
								iValue += 20000;
							}
						} else if (isBarbarian() || isHiddenNationality()) {
							if (!pAdjacentPlot->isOwned()) {
								iValue += 20000;
							}

							if (!pAdjacentPlot->isAdjacentOwned()) {
								iValue += 10000;
							}
						} else {
							if (pAdjacentPlot->isRevealedGoody(getTeam())) {
								iValue += 100000;
							}

							if (pAdjacentPlot->getOwnerINLINE() == getOwnerINLINE()) {
								iValue += 10000;
							}
						}

						if (iValue > iBestValue) {
							iBestValue = iValue;
							pBestPlot = getPathEndTurnPlot();
							FAssert(!atPlot(pBestPlot));
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_PATROL);
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_defend() {
	PROFILE_FUNC();

	if (AI_defendPlot(plot())) {
		getGroup()->pushMission(MISSION_SKIP);
		return true;
	}

	int iSearchRange = AI_searchRange(1);

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (AI_plotValid(pLoopPlot)) {
					if (AI_defendPlot(pLoopPlot)) {
						if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
							int iPathTurns;
							if (!atPlot(pLoopPlot) && generatePath(pLoopPlot, 0, true, &iPathTurns, 1)) {
								if (iPathTurns <= 1) {
									int iValue = (1 + GC.getGameINLINE().getSorenRandNum(10000, "AI Defend"));

									if (iValue > iBestValue) {
										iBestValue = iValue;
										pBestPlot = pLoopPlot;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		if (!(pBestPlot->isCity()) && (getGroup()->getNumUnits() > 1)) {
			joinGroup(0); // K-Mod. (AI_makeForceSeparate is a complete waste of time here.)
		}

		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_DEFEND);
		return true;
	}

	return false;
}


// This function has been edited for K-Mod
bool CvUnitAI::AI_safety() {
	PROFILE_FUNC();

	int iSearchRange = AI_searchRange(1);

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	bool bEnemyTerritory = isEnemy(plot()->getTeam());
	bool bIgnoreDanger = false;

	do // K-Mod. What's the point of the first pass if it is just ignored? (see break condition at the end)
	{
		for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
			for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
				CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

				if (pLoopPlot && AI_plotValid(pLoopPlot) && !pLoopPlot->isVisibleEnemyUnit(this)) {
					int iPathTurns;
					if (generatePath(pLoopPlot, bIgnoreDanger ? MOVE_IGNORE_DANGER : 0, true, &iPathTurns, 1)) {
						int iCount = 0;

						CLLNode<IDInfo>* pUnitNode = pLoopPlot->headUnitNode();

						while (pUnitNode != NULL) {
							CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
							pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

							if (pLoopUnit->getOwnerINLINE() == getOwnerINLINE()) {
								if (pLoopUnit->canDefend()) {
									CvUnit* pHeadUnit = pLoopUnit->getGroup()->getHeadUnit();
									FAssert(pHeadUnit != NULL);
									FAssert(getGroup()->getHeadUnit() == this);

									if (pHeadUnit != this) {
										if (pHeadUnit->isWaiting() || !(pHeadUnit->canMove())) {
											FAssert(pLoopUnit != this);
											FAssert(pHeadUnit != getGroup()->getHeadUnit());
											iCount++;
										}
									}
								}
							}
						}

						int iValue = (iCount * 100);

						iValue += pLoopPlot->defenseModifier(getTeam(), false);

						iValue += (bEnemyTerritory ? !isEnemy(pLoopPlot->getTeam(), pLoopPlot) : pLoopPlot->getTeam() == getTeam()) ? 30 : 0;
						iValue += pLoopPlot->isValidRoute(this) ? 25 : 0;

						if (atPlot(pLoopPlot)) {
							iValue += 50;
						} else {
							iValue += GC.getGameINLINE().getSorenRandNum(50, "AI Safety");
						}

						if (iValue > iBestValue) {
							iBestValue = iValue;
							pBestPlot = pLoopPlot;
						}
					}
				}
			}
		}
		if (!pBestPlot) {
			if (bIgnoreDanger)
				break; // no suitable plot, even when ignoring danger
			else
				bIgnoreDanger = true; // try harder next time
		}
	} while (!pBestPlot);

	if (pBestPlot != NULL) {
		if (atPlot(pBestPlot)) {
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		} else {
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), bIgnoreDanger ? MOVE_IGNORE_DANGER : 0);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_hide() {
	PROFILE_FUNC();

	if (getInvisibleType() == NO_INVISIBLE) {
		return false;
	}

	int iSearchRange = AI_searchRange(1);

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (AI_plotValid(pLoopPlot)) {
					bool bValid = true;

					for (int iI = 0; iI < MAX_TEAMS; iI++) {
						if (GET_TEAM((TeamTypes)iI).isAlive()) {
							if (pLoopPlot->isInvisibleVisible(((TeamTypes)iI), getInvisibleType())) {
								bValid = false;
								break;
							}
						}
					}

					if (bValid) {
						if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
							int iPathTurns;
							if (generatePath(pLoopPlot, 0, true, &iPathTurns, 1)) {
								if (iPathTurns <= 1) {
									int iCount = 1;

									CLLNode<IDInfo>* pUnitNode = pLoopPlot->headUnitNode();
									while (pUnitNode != NULL) {
										CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
										pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

										if (pLoopUnit->getOwnerINLINE() == getOwnerINLINE()) {
											if (pLoopUnit->canDefend()) {
												CvUnit* pHeadUnit = pLoopUnit->getGroup()->getHeadUnit();
												FAssert(pHeadUnit != NULL);
												FAssert(getGroup()->getHeadUnit() == this);

												if (pHeadUnit != this) {
													if (pHeadUnit->isWaiting() || !(pHeadUnit->canMove())) {
														FAssert(pLoopUnit != this);
														FAssert(pHeadUnit != getGroup()->getHeadUnit());
														iCount++;
													}
												}
											}
										}
									}

									int iValue = (iCount * 100);

									iValue += pLoopPlot->defenseModifier(getTeam(), false);

									if (atPlot(pLoopPlot)) {
										iValue += 50;
									} else {
										iValue += GC.getGameINLINE().getSorenRandNum(50, "AI Hide");
									}

									if (iValue > iBestValue) {
										iBestValue = iValue;
										pBestPlot = pLoopPlot;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		if (atPlot(pBestPlot)) {
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		} else {
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_goody(int iRange) {
	PROFILE_FUNC();

	if (isBarbarian()) {
		return false;
	}

	int iSearchRange = AI_searchRange(iRange);

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (AI_plotValid(pLoopPlot)) {
					if (pLoopPlot->isRevealedGoody(getTeam())) {
						if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
							int iPathTurns;
							if (!atPlot(pLoopPlot) && generatePath(pLoopPlot, 0, true, &iPathTurns, iRange)) {
								if (iPathTurns <= iRange) {
									int iValue = (1 + GC.getGameINLINE().getSorenRandNum(10000, "AI Goody"));

									iValue /= (iPathTurns + 1);

									if (iValue > iBestValue) {
										iBestValue = iValue;
										pBestPlot = getPathEndTurnPlot();
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_explore() {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestExplorePlot = NULL;

	bool bNoContact = (GC.getGameINLINE().countCivTeamsAlive() > GET_TEAM(getTeam()).getHasMetCivCount(true));
	const CvTeam& kTeam = GET_TEAM(getTeam()); // K-Mod

	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		PROFILE("AI_explore 1");

		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot)) {
			int iValue = 0;

			if (pLoopPlot->isRevealedGoody(getTeam())) {
				iValue += 100000;
			}

			if (iValue > 0 || GC.getGameINLINE().getSorenRandNum(4, "AI make explore faster ;)") == 0) {
				if (!(pLoopPlot->isRevealed(getTeam(), false))) {
					iValue += 10000;
				}
				// XXX is this too slow?
				for (int iJ = 0; iJ < NUM_DIRECTION_TYPES; iJ++) {
					PROFILE("AI_explore 2");

					CvPlot* pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), ((DirectionTypes)iJ));

					if (pAdjacentPlot != NULL) {
						if (!(pAdjacentPlot->isRevealed(getTeam(), false))) {
							iValue += 1000;
						}
						// K-Mod. Not only is the original code cheating, it also doesn't help us meet anyone!
						// The goal here is to try to meet teams which we have already seen through map trading.
						if (bNoContact && pAdjacentPlot->getRevealedOwner(kTeam.getID(), false) != NO_PLAYER) // note: revealed team can be set before the plot is actually revealed.
						{
							if (!kTeam.isHasMet(pAdjacentPlot->getRevealedTeam(kTeam.getID(), false)))
								iValue += 100;
						}
					}
				}

				if (iValue > 0) {
					if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_EXPLORE, getGroup(), 3) == 0) {
							int iPathTurns;
							if (!atPlot(pLoopPlot) && generatePath(pLoopPlot, MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns)) {
								iValue += GC.getGameINLINE().getSorenRandNum(250 * abs(xDistance(getX_INLINE(), pLoopPlot->getX_INLINE())) + abs(yDistance(getY_INLINE(), pLoopPlot->getY_INLINE())), "AI explore");

								if (pLoopPlot->isAdjacentToLand()) {
									iValue += 10000;
								}

								if (pLoopPlot->isOwned()) {
									iValue += 5000;
								}

								iValue /= 3 + std::max(1, iPathTurns);

								if (iValue > iBestValue) {
									iBestValue = iValue;
									pBestPlot = pLoopPlot->isRevealedGoody(getTeam()) ? getPathEndTurnPlot() : pLoopPlot;
									pBestExplorePlot = pLoopPlot;
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestExplorePlot != NULL)) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY, false, false, MISSIONAI_EXPLORE, pBestExplorePlot);
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_exploreRange(int iRange) {
	PROFILE_FUNC();

	int iSearchRange = AI_searchRange(iRange);

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestExplorePlot = NULL;

	int iImpassableCount = GET_PLAYER(getOwnerINLINE()).AI_unitImpassableCount(getUnitType());

	const CvTeam& kTeam = GET_TEAM(getTeam()); // K-Mod

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			PROFILE("AI_exploreRange 1");

			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (AI_plotValid(pLoopPlot)) {
					int iValue = 0;

					if (pLoopPlot->isRevealedGoody(getTeam())) {
						iValue += 100000;
					}

					if (!(pLoopPlot->isRevealed(getTeam(), false))) {
						iValue += 10000;
					}

					// K-Mod. Try to meet teams that we have seen through map trading
					if (pLoopPlot->getRevealedOwner(kTeam.getID(), false) != NO_PLAYER && !kTeam.isHasMet(pLoopPlot->getRevealedTeam(kTeam.getID(), false)))
						iValue += 1000;

					for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
						PROFILE("AI_exploreRange 2");

						CvPlot* pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), ((DirectionTypes)iI));

						if (pAdjacentPlot != NULL) {
							if (!(pAdjacentPlot->isRevealed(getTeam(), false))) {
								iValue += 1000;
							}
						}
					}

					if (iValue > 0) {
						if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
							PROFILE("AI_exploreRange 3");

							if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_EXPLORE, getGroup(), 3) == 0) {
								PROFILE("AI_exploreRange 4");

								int iPathTurns;
								if (!atPlot(pLoopPlot) && generatePath(pLoopPlot, MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns, iRange)) {
									if (iPathTurns <= iRange) {
										iValue += GC.getGameINLINE().getSorenRandNum(10000, "AI Explore");

										if (pLoopPlot->isAdjacentToLand()) {
											iValue += 10000;
										}

										if (pLoopPlot->isOwned()) {
											iValue += 5000;
										}

										if (!isHuman()) {
											int iDirectionModifier = 100;

											if (AI_getUnitAIType() == UNITAI_EXPLORE_SEA && iImpassableCount == 0) {
												iDirectionModifier += (50 * (abs(iDX) + abs(iDY))) / iSearchRange;
												if (GC.getGame().circumnavigationAvailable()) {
													if (GC.getMap().isWrapX()) {
														if ((iDX * ((AI_getBirthmark() % 2 == 0) ? 1 : -1)) > 0) {
															iDirectionModifier *= 150 + ((iDX * 100) / iSearchRange);
														} else {
															iDirectionModifier /= 2;
														}
													}
													if (GC.getMap().isWrapY()) {
														if ((iDY * (((AI_getBirthmark() >> 1) % 2 == 0) ? 1 : -1)) > 0) {
															iDirectionModifier *= 150 + ((iDY * 100) / iSearchRange);
														} else {
															iDirectionModifier /= 2;
														}
													}
												}
												iValue *= iDirectionModifier;
												iValue /= 100;
											}
										}

										if (iValue > iBestValue) {
											iBestValue = iValue;
											if (getDomainType() == DOMAIN_LAND) {
												pBestPlot = getPathEndTurnPlot();
											} else {
												pBestPlot = pLoopPlot;
											}
											pBestExplorePlot = pLoopPlot;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestExplorePlot != NULL)) {
		PROFILE("AI_exploreRange 5");

		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY, false, false, MISSIONAI_EXPLORE, pBestExplorePlot);
		return true;
	}

	return false;
}

// Returns target city
// This function has been heavily edited for K-Mod (and I got sick of putting "K-Mod" tags all over the place)
CvCity* CvUnitAI::AI_pickTargetCity(int iFlags, int iMaxPathTurns, bool bHuntBarbs) {
	PROFILE_FUNC();

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	CvCity* pBestCity = NULL;
	int iBestValue = 0;
	int iOurOffence = -1; // We calculate this for the first city only.
	CvUnit* pBestTransport = 0;
	// iLoadTurns < 0 implies we should look for a transport; otherwise, it is the number of turns to reach the transport.
	// Also, we only consider using transports if we aren't in enemy territory.
	int iLoadTurns = isEnemy(plot()->getTeam()) ? MAX_INT : -1;
	KmodPathFinder transport_path;

	CvCity* pTargetCity = area()->getTargetCity(getOwnerINLINE());

	for (int iI = 0; iI < (bHuntBarbs ? MAX_PLAYERS : MAX_CIV_PLAYERS); iI++) {
		if (GET_PLAYER((PlayerTypes)iI).isAlive() && ::isPotentialEnemy(getTeam(), GET_PLAYER((PlayerTypes)iI).getTeam())) {
			int iLoop;
			for (CvCity* pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop)) {
				if (AI_plotValid(pLoopCity->plot()) && (pLoopCity->area() == area())) {
					if (AI_potentialEnemy(GET_PLAYER((PlayerTypes)iI).getTeam(), pLoopCity->plot())) {
						if (kOwner.AI_deduceCitySite(pLoopCity)) {
							// K-Mod. Look for either a direct land path, or a sea transport path.
							int iPathTurns = MAX_INT;
							bool bLandPath = generatePath(pLoopCity->plot(), iFlags, true, &iPathTurns, iMaxPathTurns);

							if (pLoopCity->isCoastal(GC.getMIN_WATER_SIZE_FOR_OCEAN()) && (pBestTransport || iLoadTurns < 0)) {
								// add a random bias in favour of land paths, so that not all stacks try to use boats.
								int iLandBias = AI_getBirthmark() % 6 + (AI_getBirthmark() % (bLandPath ? 3 : 6) ? 6 : 1);
								if (!pBestTransport && iPathTurns > iLandBias + 2) {
									pBestTransport = AI_findTransport(UNITAI_ASSAULT_SEA, iFlags, std::min(iMaxPathTurns, iPathTurns));
									if (pBestTransport) {
										generatePath(pBestTransport->plot(), iFlags, true, &iLoadTurns);
										FAssert(iLoadTurns > 0 && iLoadTurns < MAX_INT);
										iLoadTurns += iLandBias;
										FAssert(iLoadTurns > 0);
									} else
										iLoadTurns = MAX_INT; // just to indicate the we shouldn't look for a transport again.
								}
								int iMaxTransportTurns = std::min(iMaxPathTurns, iPathTurns) - iLoadTurns;

								if (pBestTransport && iMaxTransportTurns > 0) {
									transport_path.SetSettings(pBestTransport->getGroup(), iFlags & MOVE_DECLARE_WAR, iMaxTransportTurns, GC.getMOVE_DENOMINATOR());
									if (transport_path.GeneratePath(pLoopCity->plot())) {
										// faster by boat
										FAssert(transport_path.GetPathTurns() + iLoadTurns <= iPathTurns);
										iPathTurns = transport_path.GetPathTurns() + iLoadTurns;
									}
								}
							}

							if (iPathTurns < iMaxPathTurns) {
								// If city is visible and our force already in position is dominantly powerful or we have a huge force
								// already on the way, pick a different target
								int iEnemyDefence = -1; // used later.
								int iOffenceEnRoute = kOwner.AI_cityTargetStrengthByPath(pLoopCity, getGroup(), iPathTurns);
								if (pLoopCity->isVisible(getTeam(), false)) {
									iEnemyDefence = kOwner.AI_localDefenceStrength(pLoopCity->plot(), NO_TEAM, DOMAIN_LAND, true, iPathTurns > 1 ? 2 : 0);

									if (iPathTurns > 2) {
										int iAttackRatio = ((GC.getMAX_CITY_DEFENSE_DAMAGE() - pLoopCity->getDefenseDamage()) * GC.getBBAI_SKIP_BOMBARD_BASE_STACK_RATIO() + pLoopCity->getDefenseDamage() * GC.getBBAI_SKIP_BOMBARD_MIN_STACK_RATIO()) / std::max(1, GC.getMAX_CITY_DEFENSE_DAMAGE());

										if (100 * iOffenceEnRoute > iAttackRatio * iEnemyDefence) {
											continue;
										}
									}
								}

								if (iOurOffence == -1) {
									// note: with bCheckCanAttack == false, AI_sumStrength should be roughly the same regardless of which city we are targetting.
									// ... except if lots of our units have a hills-attack promotion or something like that.
									iOurOffence = getGroup()->AI_sumStrength(pLoopCity->plot());
								}
								FAssert(iOurOffence > 0);
								int iTotalOffence = iOurOffence + iOffenceEnRoute;

								int iValue = 0;
								if (AI_getUnitAIType() == UNITAI_ATTACK_CITY) //lemming?
								{
									iValue = kOwner.AI_targetCityValue(pLoopCity, false, false);
								} else {
									iValue = kOwner.AI_targetCityValue(pLoopCity, true, true);
								}
								// adjust value based on defensive bonuses
								{
									int iMod =
										std::min(8, getGroup()->getBombardTurns(pLoopCity)) * pLoopCity->getDefenseModifier(false) / 8
										+ (pLoopCity->plot()->isHills() ? GC.getHILLS_EXTRA_DEFENSE() : 0);
									iValue *= std::max(25, 125 - iMod);
									iValue /= 25; // the denominator is arbitrary, and unimportant.
									// note: the value reduction from high defences which are bombardable should not be more than
									// the value reduction from simply having higher iPathTurns.
								}

								// prefer cities which are close to the main target.
								if (pLoopCity == pTargetCity) {
									iValue *= 2;
								} else if (pTargetCity != NULL) {
									int iStepsFromTarget = stepDistance(
										pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE(),
										pTargetCity->getX_INLINE(), pTargetCity->getY_INLINE());

									iValue *= 124 - 2 * std::min(12, iStepsFromTarget);
									iValue /= 100;
								}

								if (area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE) {
									iValue *= 100 + pLoopCity->calculateCulturePercent(getOwnerINLINE()); // was 50
									iValue /= 125; // was 50 (unimportant)
								}

								// boost value if we can see that the city is poorly defended, or if our existing armies need help there
								if (pLoopCity->isVisible(getTeam(), false) && iPathTurns < 6) {
									FAssert(iEnemyDefence != -1);
									if (iOffenceEnRoute > iEnemyDefence / 3 && iOffenceEnRoute < iEnemyDefence) {
										iValue *= 100 + (9 * iTotalOffence > 10 * iEnemyDefence ? 30 : 15);
										iValue /= 100;
									} else if (iOurOffence > iEnemyDefence) {
										// dont' boost it by too much, otherwise human players will exploit us. :(
										int iCap = 100 + 100 * (6 - iPathTurns) / 5;
										iValue *= std::min(iCap, 100 * iOurOffence / std::max(1, iEnemyDefence));
										iValue /= 100;
										// an additional bonus if we're already adjacent
										// (we can afford to be generous with this bonus, because the enemy has no time to bring in reinforcements)
										if (iPathTurns <= 1) {
											iValue *= std::min(300, 150 * iOurOffence / std::max(1, iEnemyDefence));
											iValue /= 100;
										}
									}
								}
								// Reduce the value if we can see, or remember, that the city is well defended.
								// Note. This adjustment can be more heavy handed because it is harder to feign strong defence than weak defence.
								iEnemyDefence = GET_TEAM(getTeam()).AI_getStrengthMemory(pLoopCity->plot());
								if (iEnemyDefence > iTotalOffence) {
									// a more sensitive adjustment than usual (w/ modifier on the denominator), so as not to be too deterred before bombarding.
									iEnemyDefence *= 130;
									iEnemyDefence /= 130 + (bombardRate() > 0 ? pLoopCity->getDefenseModifier(false) : 0);
									WarPlanTypes eWarPlan = GET_TEAM(kOwner.getTeam()).AI_getWarPlan(pLoopCity->getTeam());
									// If we aren't fully committed to the war, then focus on taking easy cities - but try not to be completely predictable.
									bool bCherryPick = eWarPlan == WARPLAN_LIMITED || eWarPlan == WARPLAN_PREPARING_LIMITED || eWarPlan == WARPLAN_DOGPILE;
									bCherryPick = bCherryPick && (AI_unitBirthmarkHash(GC.getGameINLINE().getElapsedGameTurns() / 4) % 4);

									int iBase = bCherryPick ? 100 : 110;
									if (100 * iEnemyDefence > iBase * iTotalOffence) // an uneven comparison, just in case we can get some air support or other help somehow.
									{
										iValue *= bCherryPick
											? std::max(20, (3 * iBase * iTotalOffence - iEnemyDefence) / (2 * iEnemyDefence))
											: std::max(33, iBase * iTotalOffence / iEnemyDefence);
										iValue /= 100;
									}
								}
								// A const-random component, so that the AI doesn't always go for the same city.
								iValue *= 80 + AI_unitPlotHash(pLoopCity->plot()) % 41;
								iValue /= 100;

								iValue *= 1000;

								// If city is minor civ, less interesting
								if (GET_PLAYER(pLoopCity->getOwnerINLINE()).isMinorCiv() || GET_PLAYER(pLoopCity->getOwnerINLINE()).isBarbarian()) {
									iValue /= 3; // K-Mod
								}

								iValue /= 8 + iPathTurns * iPathTurns; // was 4+

								if (iValue > iBestValue) {
									iBestValue = iValue;
									pBestCity = pLoopCity;
								}
							}
						} // end if revealed.
						// K-Mod. If no city in the area is revealed,
						// then assume the AI is able to deduce the position of the closest city.
						else if (iBestValue == 0 && !pLoopCity->isBarbarian() && (!pBestCity ||
							stepDistance(getX_INLINE(), getY_INLINE(), pBestCity->getX_INLINE(), pBestCity->getY_INLINE()) >
							stepDistance(getX_INLINE(), getY_INLINE(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE()))) {
							if (generatePath(pLoopCity->plot(), iFlags, true, 0, iMaxPathTurns))
								pBestCity = pLoopCity;
						}
					}
				}
			}
		}
	}

	return pBestCity;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_goToTargetCity(int iFlags, int iMaxPathTurns, CvCity* pTargetCity) {
	PROFILE_FUNC();

	if (pTargetCity == NULL) {
		pTargetCity = AI_pickTargetCity(iFlags, iMaxPathTurns);
	}

	if (pTargetCity != NULL) {
		PROFILE("CvUnitAI::AI_targetCity plot attack");
		int iBestValue = 0;
		CvPlot* pBestPlot = NULL;
		CvPlot* pEndTurnPlot = NULL;

		if (0 == (iFlags & MOVE_THROUGH_ENEMY)) {
			for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
				CvPlot* pAdjacentPlot = plotDirection(pTargetCity->getX_INLINE(), pTargetCity->getY_INLINE(), ((DirectionTypes)iI));

				if (pAdjacentPlot != NULL) {
					if (AI_plotValid(pAdjacentPlot)) {
						if (!(pAdjacentPlot->isVisibleEnemyUnit(this))) // K-Mod TODO: consider fighting for the best plot.
						{
							int iPathTurns;
							if (generatePath(pAdjacentPlot, iFlags, true, &iPathTurns, iMaxPathTurns)) {
								if (iPathTurns <= iMaxPathTurns) {
									int iValue = std::max(0, (pAdjacentPlot->defenseModifier(getTeam(), false) + 100));

									if (!(pAdjacentPlot->isRiverCrossing(directionXY(pAdjacentPlot, pTargetCity->plot())))) {
										iValue += (12 * -(GC.getRIVER_ATTACK_MODIFIER()));
									}

									if (!isEnemy(pAdjacentPlot->getTeam(), pAdjacentPlot)) {
										iValue += 100;
									}

									if (atPlot(pAdjacentPlot)) {
										iValue += 50;
									}

									iValue = std::max(1, iValue);

									iValue *= 1000;

									iValue /= (iPathTurns + 1);

									if (iValue > iBestValue) {
										iBestValue = iValue;
										pBestPlot = pAdjacentPlot;
										pEndTurnPlot = getPathEndTurnPlot();
									}
								}
							}
						}
					}
				}
			}
		} else {
			pBestPlot = pTargetCity->plot();
			// K-mod. As far as I know, nothing actually uses this flag here.. but that doesn't mean we should let the code be wrong.
			int iPathTurns;
			if (!generatePath(pBestPlot, iFlags, true, &iPathTurns, iMaxPathTurns) || iPathTurns > iMaxPathTurns)
				return false;
			pEndTurnPlot = getPathEndTurnPlot();
		}

		if (pBestPlot != NULL) {
			FAssert(!(pTargetCity->at(pEndTurnPlot)) || 0 != (iFlags & MOVE_THROUGH_ENEMY)); // no suicide missions...
			// K-Mod note: it may be possible for this assert to fail if the city is so weak that ATTACK_STACK_MOVE would choose to just walk through it.
			if (!atPlot(pEndTurnPlot)) {
				if (AI_considerPathDOW(pEndTurnPlot, iFlags)) {
					// regenerate the path, just incase we want to take a different route after the DOW
					// (but don't bother recalculating the best destination)
					// Note. if the best destination happens to be on the border, and have a stack of defenders on it, this will make us attack them.\
					// That's bad. I'll try to fix that in the future.
					if (!generatePath(pBestPlot, iFlags, false))
						return false;
					pEndTurnPlot = getPathEndTurnPlot();
				}
				// I'm going to use MISSIONAI_ASSAULT signal to our spies and other units that we're attacking this city.
				getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX_INLINE(), pEndTurnPlot->getY_INLINE(), iFlags, false, false, MISSIONAI_ASSAULT, pTargetCity->plot());
				return true;
			}
		}
	}

	return false;
}

bool CvUnitAI::AI_pillageAroundCity(CvCity* pTargetCity, int iBonusValueThreshold, int iFlags, int iMaxPathTurns) {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestPillagePlot = NULL;

	if (!isEnemy(pTargetCity->getTeam()) && !getGroup()->AI_isDeclareWar(pTargetCity->plot())) {
		return false;
	}

	for (int iI = 0; iI < pTargetCity->getNumCityPlots(); iI++) {
		CvPlot* pLoopPlot = pTargetCity->getCityIndexPlot(iI);
		if (pLoopPlot != NULL) {
			if (AI_plotValid(pLoopPlot) && !(pLoopPlot->isBarbarian())) {
				if (potentialWarAction(pLoopPlot) && (pLoopPlot->getTeam() == pTargetCity->getTeam())) {
					if (canPillage(pLoopPlot)) {
						if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
							if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_PILLAGE, getGroup()) == 0) {
								int iPathTurns;
								if (generatePath(pLoopPlot, iFlags, true, &iPathTurns, iMaxPathTurns)) {
									if (getPathFinder().GetFinalMoves() == 0) {
										iPathTurns++;
									}

									if (iPathTurns <= iMaxPathTurns) {
										int iValue = AI_pillageValue(pLoopPlot, iBonusValueThreshold);

										iValue *= 1000 + 30 * (pLoopPlot->defenseModifier(getTeam(), false));

										iValue /= std::max(1, iPathTurns); // K-Mod

										// if not at war with this plot owner, then devalue plot if we already inside this owner's borders
										// (because declaring war will pop us some unknown distance away)
										if (!isEnemy(pLoopPlot->getTeam()) && plot()->getTeam() == pLoopPlot->getTeam()) {
											iValue /= 10;
										}

										if (iValue > iBestValue) {
											iBestValue = iValue;
											pBestPlot = getPathEndTurnPlot();
											pBestPillagePlot = pLoopPlot;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestPillagePlot != NULL)) {
		FAssert(getGroup()->AI_isDeclareWar());
		if (AI_considerPathDOW(pBestPlot, iFlags)) {
			int iPathTurns;
			if (!generatePath(pBestPillagePlot, iFlags, true, &iPathTurns))
				return false;
			pBestPlot = getPathEndTurnPlot();
		}

		if (atPlot(pBestPillagePlot)) {
			FAssert(isEnemy(pBestPillagePlot->getTeam())); // K-Mod
			getGroup()->pushMission(MISSION_PILLAGE, -1, -1, 0, false, false, MISSIONAI_PILLAGE, pBestPillagePlot);
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), iFlags, false, false, MISSIONAI_PILLAGE, pBestPillagePlot);
			return true;
		}
	}

	return false;
}

// Returns true if a mission was pushed...
// This function has been completely rewriten (and greatly simplified) for K-Mod
bool CvUnitAI::AI_bombardCity() {
	// check if we need to declare war before bombarding!
	for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
		CvPlot* pLoopPlot = plotDirection(plot()->getX_INLINE(), plot()->getY_INLINE(), ((DirectionTypes)iI));

		if (pLoopPlot != NULL && pLoopPlot->isCity()) {
			AI_considerDOW(pLoopPlot);
			break; // assume there can only be one city adjacent to us.
		}
	}

	if (!canBombard(plot()))
		return false;

	CvCity* pBombardCity = bombardTarget(plot());

	FAssertMsg(pBombardCity != NULL, "BombardCity is not assigned a valid value");

	int iAttackOdds = getGroup()->AI_attackOdds(pBombardCity->plot(), true);
	int iBase = GC.getBBAI_SKIP_BOMBARD_BASE_STACK_RATIO();
	int iMin = GC.getBBAI_SKIP_BOMBARD_MIN_STACK_RATIO();
	int iBombardTurns = getGroup()->getBombardTurns(pBombardCity);
	iBase = (iBase * (GC.getMAX_CITY_DEFENSE_DAMAGE() - pBombardCity->getDefenseDamage()) + iMin * pBombardCity->getDefenseDamage()) / std::max(1, GC.getMAX_CITY_DEFENSE_DAMAGE());
	int iThreshold = (iBase * (100 - iAttackOdds) + (1 + iBombardTurns / 2) * iMin * iAttackOdds) / (100 + (iBombardTurns / 2) * iAttackOdds);
	int iComparison = getGroup()->AI_compareStacks(pBombardCity->plot(), true);

	if (iComparison > iThreshold) {
		if (gUnitLogLevel > 2) logBBAI("      Stack skipping bombard of %S with compare %d, starting odds %d, bombard turns %d, threshold %d", pBombardCity->getName().GetCString(), iComparison, iAttackOdds, iBombardTurns, iThreshold);
		return false;
	}

	getGroup()->pushMission(MISSION_BOMBARD, -1, -1, 0, false, false, MISSIONAI_ASSAULT, pBombardCity->plot()); // K-Mod
	return true;
}

// Returns true if a mission was pushed...
// This function has been been heavily edited for K-Mod.
bool CvUnitAI::AI_cityAttack(int iRange, int iOddsThreshold, int iFlags, bool bFollow) {
	PROFILE_FUNC();

	FAssert(canMove());

	int iSearchRange = bFollow ? 1 : AI_searchRange(iRange);
	bool bDeclareWar = iFlags & MOVE_DECLARE_WAR;

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (AI_plotValid(pLoopPlot)) {
					if (pLoopPlot->isCity() && (bDeclareWar ? AI_potentialEnemy(pLoopPlot->getTeam(), pLoopPlot) : isEnemy(pLoopPlot->getTeam(), pLoopPlot))) {
						int iPathTurns;
						if (!atPlot(pLoopPlot) && (bFollow ? canMoveOrAttackInto(pLoopPlot, bDeclareWar) : generatePath(pLoopPlot, iFlags, true, &iPathTurns, iRange))) {
							int iValue = pLoopPlot->getNumVisiblePotentialEnemyDefenders(this) == 0 ? 100 : AI_getWeightedOdds(pLoopPlot, true);

							if (iValue >= iOddsThreshold) {
								if (iValue > iBestValue) {
									iBestValue = iValue;
									pBestPlot = ((bFollow) ? pLoopPlot : getPathEndTurnPlot());
									FAssert(!atPlot(pBestPlot));
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		if (AI_considerPathDOW(pBestPlot, iFlags)) {
			// after DOW, we might not be able to get to our target this turn... but try anyway.
			if (!generatePath(pBestPlot, iFlags, false))
				return false;
			if (bFollow && pBestPlot != getPathEndTurnPlot())
				return false;
			pBestPlot = getPathEndTurnPlot();
		}
		if (bFollow && pBestPlot->getNumVisiblePotentialEnemyDefenders(this) == 0) {
			FAssert(pBestPlot->getPlotCity() != 0);
			// we need to ungroup this unit so that we can move into the city.
			joinGroup(0);
			bFollow = false;
		}
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), iFlags | (bFollow ? MOVE_DIRECT_ATTACK | MOVE_SINGLE_ATTACK : 0));
		return true;
	}

	return false;
}

// Returns true if a mission was pushed...
// This function has been been writen for K-Mod. (it started getting messy, so I deleted most of the old code)
// bFollow implies AI_follow conditions - ie. not everyone in the group can move, and this unit might not be the group leader.
bool CvUnitAI::AI_anyAttack(int iRange, int iOddsThreshold, int iFlags, int iMinStack, bool bAllowCities, bool bFollow) {
	PROFILE_FUNC();

	FAssert(canMove());

	if (AI_rangeAttack(iRange)) {
		return true;
	}

	int iSearchRange = bFollow ? 1 : AI_searchRange(iRange);
	bool bDeclareWar = iFlags & MOVE_DECLARE_WAR;

	CvPlot* pBestPlot = NULL;

	for (int iDX = -iSearchRange; iDX <= iSearchRange; iDX++) {
		for (int iDY = -iSearchRange; iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot == NULL || !AI_plotValid(pLoopPlot))
				continue;

			if (!bAllowCities && pLoopPlot->isCity())
				continue;

			if (bDeclareWar
				? !pLoopPlot->isVisiblePotentialEnemyUnit(getOwnerINLINE()) && !(pLoopPlot->isCity() && AI_potentialEnemy(pLoopPlot->getPlotCity()->getTeam(), pLoopPlot))
				: !pLoopPlot->isVisibleEnemyUnit(this) && !pLoopPlot->isEnemyCity(*this)) {
				continue;
			}

			int iEnemyDefenders = bDeclareWar ? pLoopPlot->getNumVisiblePotentialEnemyDefenders(this) : pLoopPlot->getNumVisibleEnemyDefenders(this);
			if (iEnemyDefenders < iMinStack)
				continue;

			if (!atPlot(pLoopPlot) && (bFollow ? getGroup()->canMoveOrAttackInto(pLoopPlot, bDeclareWar, true) : generatePath(pLoopPlot, iFlags, true, 0, iRange))) {
				int iOdds = iEnemyDefenders == 0 ? (pLoopPlot->isCity() ? 101 : 100) : AI_getWeightedOdds(pLoopPlot, false); // 101 for cities, because that's a better thing to capture.
				if (iOdds >= iOddsThreshold) {
					iOddsThreshold = iOdds;
					pBestPlot = bFollow ? pLoopPlot : getPathEndTurnPlot();
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		if (AI_considerPathDOW(pBestPlot, iFlags)) {
			// after DOW, we might not be able to get to our target this turn... but try anyway.
			if (!generatePath(pBestPlot, iFlags))
				return false;
			if (bFollow && pBestPlot != getPathEndTurnPlot())
				return false;
			pBestPlot = getPathEndTurnPlot();
		}
		if (bFollow && pBestPlot->getNumVisiblePotentialEnemyDefenders(this) == 0) {
			FAssert(pBestPlot->getPlotCity() != 0);
			// we need to ungroup to capture the undefended unit / city. (because not everyone in our group can move)
			joinGroup(0);
			bFollow = false;
		}
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), iFlags | (bFollow ? MOVE_DIRECT_ATTACK | MOVE_SINGLE_ATTACK : 0));
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_rangeAttack(int iRange) {
	PROFILE_FUNC();

	FAssert(canMove());

	if (!canRangeStrike()) {
		return false;
	}

	int iSearchRange = AI_searchRange(iRange);

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (pLoopPlot->isVisibleEnemyUnit(this)) // K-Mod
				{
					if (!atPlot(pLoopPlot) && canRangeStrikeAt(plot(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE())) {
						int iValue = getGroup()->AI_attackOdds(pLoopPlot, true);

						if (iValue > iBestValue) {
							iBestValue = iValue;
							pBestPlot = pLoopPlot;
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		// K-Mod note: no AI_considerDOW here.
		getGroup()->pushMission(MISSION_RANGE_ATTACK, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0);
		return true;
	}

	return false;
}

// (heavily edited for K-Mod)
bool CvUnitAI::AI_leaveAttack(int iRange, int iOddsThreshold, int iStrengthThreshold) {
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	FAssert(canMove());

	int iSearchRange = iRange;

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	CvCity* pCity = plot()->getPlotCity();

	if ((pCity != NULL) && (pCity->getOwnerINLINE() == getOwnerINLINE())) {
		int iOurDefence = kOwner.AI_localDefenceStrength(plot(), getTeam());
		int iEnemyStrength = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2);
		if (iEnemyStrength > 0) {
			if (iOurDefence * 100 / iEnemyStrength < iStrengthThreshold) {
				// We should only heed to the threshold if we either we have enough defence to hold the city,
				// or we don't have enough attack force to wipe the enemy out.
				// (otherwise, we are better off attacking than defending.)
				if (iEnemyStrength < iOurDefence
					|| kOwner.AI_localAttackStrength(plot(), getTeam(), DOMAIN_LAND, 0, false, false, true)
					< kOwner.AI_localDefenceStrength(plot(), NO_TEAM, DOMAIN_LAND, 2, false))
					return false;
			}
			if (plot()->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE()) <= getGroup()->getNumUnits()) {
				return false;
			}
		}
	}

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);
			if (pLoopPlot == NULL || !AI_plotValid(pLoopPlot))
				continue;

			if (pLoopPlot->isVisibleEnemyDefender(this)) // K-Mod
			{
				if (!atPlot(pLoopPlot) && generatePath(pLoopPlot, 0, true, 0, iRange)) {
					int iValue = AI_getWeightedOdds(pLoopPlot, false); // K-Mod

					if (iValue >= iOddsThreshold) // K-mod
					{
						if (iValue > iBestValue) {
							iBestValue = iValue;
							pBestPlot = getPathEndTurnPlot();
							FAssert(!atPlot(pBestPlot));
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		// K-Mod note: no AI_considerDOW here.
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_COUNTER_ATTACK);
		return true;
	}

	return false;
}

// K-Mod. Defend nearest city against invading attack stacks.
bool CvUnitAI::AI_defensiveCollateral(int iThreshold, int iSearchRange) {
	PROFILE_FUNC();
	FAssert(collateralDamage() > 0);

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	CvPlot* pDefencePlot = 0;

	if (plot()->isCity(false, getTeam()))
		pDefencePlot = plot();
	else {
		int iClosest = MAX_INT;
		for (int iDX = -iSearchRange; iDX <= iSearchRange; iDX++) {
			for (int iDY = -iSearchRange; iDY <= iSearchRange; iDY++) {
				CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

				if (pLoopPlot && pLoopPlot->isCity(false, getTeam())) {
					if (kOwner.AI_getAnyPlotDanger(pLoopPlot)) {
						pDefencePlot = pLoopPlot;
						break;
					}

					int iDist = std::max(std::abs(iDX), std::abs(iDY));
					if (iDist < iClosest) {
						iClosest = iDist;
						pDefencePlot = pLoopPlot;
					}
				}
			}
		}
	}

	if (pDefencePlot == NULL)
		return false;

	int iEnemyAttack = kOwner.AI_localAttackStrength(pDefencePlot, NO_TEAM, getDomainType(), iSearchRange);
	int iOurDefence = kOwner.AI_localDefenceStrength(pDefencePlot, getTeam(), getDomainType(), 0);
	bool bDanger = iEnemyAttack > iOurDefence;

	CvPlot* pBestPlot = NULL;

	for (int iDX = -iSearchRange; iDX <= iSearchRange; iDX++) {
		for (int iDY = -iSearchRange; iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);
			if (pLoopPlot && AI_plotValid(pLoopPlot) && !atPlot(pLoopPlot)) {
				int iEnemies = pLoopPlot->getNumVisibleEnemyDefenders(this);
				int iPathTurns;
				if (iEnemies > 0 && generatePath(pLoopPlot, 0, true, &iPathTurns, 1)) {
					bool bValid = false;
					int iValue = AI_getWeightedOdds(pLoopPlot);

					if (iValue > 0 && iEnemies >= std::min(4, collateralDamageMaxUnits())) {
						int iOurAttack = kOwner.AI_localAttackStrength(pLoopPlot, getTeam(), getDomainType(), iSearchRange, true, true, true);
						int iEnemyDefence = kOwner.AI_localDefenceStrength(pLoopPlot, NO_TEAM, getDomainType(), 0);

						iValue += std::max(0, (bDanger ? 75 : 45) * (3 * iOurAttack - iEnemyDefence) / std::max(1, 3 * iEnemyDefence));
						// note: the scale is choosen to be around +50% when attack == defence, while in danger.
						if (bDanger && std::max(std::abs(iDX), std::abs(iDY)) <= 1) {
							// enemy is ready to attack, and strong enough to win. We might as well hit them.
							iValue += 20;
						}
					}

					if (iValue >= iThreshold) {
						iThreshold = iValue;
						pBestPlot = getPathEndTurnPlot();
					}
				}
			}
		} // dy
	} // dx

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}

// K-Mod.
// bLocal is just to help with the efficiency of this function for short-range checks. It means that we should look only in nearby plots.
// the default (bLocal == false) is to look at every plot on the map!
bool CvUnitAI::AI_defendTeritory(int iThreshold, int iFlags, int iMaxPathTurns, bool bLocal) {
	PROFILE_FUNC();

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	CvPlot* pEndTurnPlot = NULL;
	int iBestValue = 0;

	// I'm going to use a loop equivalent to the above when !bLocal; and a loop in a square around our unit if bLocal.
	int i = 0;
	int iRange = bLocal ? AI_searchRange(iMaxPathTurns) : 0;
	int iPlots = bLocal ? (2 * iRange + 1) * (2 * iRange + 1) : GC.getMapINLINE().numPlotsINLINE();
	if (bLocal && iPlots >= GC.getMapINLINE().numPlotsINLINE()) {
		bLocal = false;
		iRange = 0;
		iPlots = GC.getMapINLINE().numPlotsINLINE();
		// otherwise it's just silly.
	}
	FAssert(!bLocal || iRange > 0);
	while (i < iPlots) {
		CvPlot* pLoopPlot = bLocal
			? plotXY(getX_INLINE(), getY_INLINE(), -iRange + i % (2 * iRange + 1), -iRange + i / (2 * iRange + 1))
			: GC.getMapINLINE().plotByIndexINLINE(i);
		i++; // for next cycle.

		if (pLoopPlot && pLoopPlot->getTeam() == getTeam() && AI_plotValid(pLoopPlot)) {
			if (pLoopPlot->isVisibleEnemyUnit(this)) {
				int iPathTurns;
				if (generatePath(pLoopPlot, iFlags, true, &iPathTurns, iMaxPathTurns)) {
					int iOdds = AI_getWeightedOdds(pLoopPlot);
					int iValue = iOdds;

					if (iOdds > 0 && iOdds < 100 && iThreshold > 0) {
						int iOurAttack = kOwner.AI_localAttackStrength(pLoopPlot, getTeam(), getDomainType(), 2, true, true, true);
						int iEnemyDefence = kOwner.AI_localDefenceStrength(pLoopPlot, NO_TEAM, getDomainType(), 0);

						if (iOurAttack > iEnemyDefence && iEnemyDefence > 0) {
							iValue += 100 * (iOdds + 15) * (iOurAttack - iEnemyDefence) / ((iThreshold + 100) * iEnemyDefence);
						}
					}

					if (iValue >= iThreshold) {
						BonusTypes eBonus = pLoopPlot->getNonObsoleteBonusType(getTeam());
						iValue *= 100 + (eBonus != NO_BONUS ? 3 * kOwner.AI_bonusVal(eBonus, 0) / 2 : 0) + (pLoopPlot->getWorkingCity() ? 20 : 0);

						if (pLoopPlot->getOwnerINLINE() != getOwnerINLINE())
							iValue = 2 * iValue / 3;

						if (iPathTurns > 1)
							iValue /= iPathTurns + 2;

						if (iOdds >= iThreshold)
							iValue = 4 * iValue / 3;

						if (iValue > iBestValue) {
							iBestValue = iValue;
							pEndTurnPlot = getPathEndTurnPlot();
						}
					}
				}
			}
		}
	}

	if (pEndTurnPlot != NULL) {
		FAssert(!atPlot(pEndTurnPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX_INLINE(), pEndTurnPlot->getY_INLINE(), iFlags, false, false, MISSIONAI_DEFEND);
		return true;
	}

	return false;
}

// iAttackThreshold is the minimum ratio for our attack / their defence.
// iRiskThreshold is the minimum ratio for their attack / our defence adjusted for stack size
// note: iSearchRange is /not/ the number of turns. It is the number of steps. iSearchRange < 1 means 'automatic'
// Only 1-turn moves are considered here.
bool CvUnitAI::AI_stackVsStack(int iSearchRange, int iAttackThreshold, int iRiskThreshold, int iFlags) {
	PROFILE_FUNC();

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	if (iSearchRange < 1) {
		iSearchRange = AI_searchRange(1);
	}

	int iOurDefence = getGroup()->AI_sumStrength(0); // not counting defensive bonuses

	CvPlot* pBestPlot = NULL;
	int iBestValue = 0;

	for (int iDX = -iSearchRange; iDX <= iSearchRange; iDX++) {
		for (int iDY = -iSearchRange; iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);
			if (pLoopPlot && AI_plotValid(pLoopPlot) && !atPlot(pLoopPlot)) {
				int iEnemies = pLoopPlot->getNumVisibleEnemyDefenders(this);
				int iPathTurns;
				if (iEnemies > 0 && generatePath(pLoopPlot, iFlags, true, &iPathTurns, 1)) {
					int iEnemyAttack = kOwner.AI_localAttackStrength(pLoopPlot, NO_TEAM, getDomainType(), 0, false);

					int iRiskRatio = 100 * iEnemyAttack / std::max(1, iOurDefence);
					// adjust risk ratio based on the relative numbers of units.
					iRiskRatio *= 50 + 50 * (getGroup()->getNumUnits() + 3) / std::min(iEnemies + 3, getGroup()->getNumUnits() + 3);
					iRiskRatio /= 100;
					//
					if (iRiskRatio < iRiskThreshold)
						continue;

					int iAttackRatio = getGroup()->AI_compareStacks(pLoopPlot, true);
					if (iAttackRatio < iAttackThreshold)
						continue;

					int iValue = iAttackRatio * iRiskRatio;

					if (iValue > iBestValue) {
						iBestValue = iValue;
						pBestPlot = pLoopPlot;
						FAssert(pBestPlot == getPathEndTurnPlot());
					}
				}
			}
		} // dy
	} // dx

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		if (gUnitLogLevel >= 2) {
			logBBAI("    Stack for player %d (%S) uses StackVsStack attack with value %d", getOwner(), GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), iBestValue);
		}
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), iFlags, false, false, MISSIONAI_COUNTER_ATTACK, pBestPlot);
		return true;
	}

	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_blockade() {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestBlockadePlot = NULL;

	int iFlags = MOVE_DECLARE_WAR; // K-Mod

	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot)) {
			if (potentialWarAction(pLoopPlot)) {
				CvCity* pCity = pLoopPlot->getWorkingCity();

				if (pCity != NULL) {
					if (pCity->isCoastal(GC.getMIN_WATER_SIZE_FOR_OCEAN())) {
						if (!(pCity->isBarbarian())) {
							FAssert(isEnemy(pCity->getTeam()) || GET_TEAM(getTeam()).AI_getWarPlan(pCity->getTeam()) != NO_WARPLAN);

							if (!(pLoopPlot->isVisibleEnemyUnit(this)) && canPlunder(pLoopPlot)) {
								if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BLOCKADE, getGroup(), 2) == 0) {
									int iPathTurns;
									if (generatePath(pLoopPlot, iFlags, true, &iPathTurns)) {
										int iValue = 1;

										iValue += std::min(pCity->getPopulation(), pCity->countNumWaterPlots());

										iValue += GET_PLAYER(getOwnerINLINE()).AI_adjacentPotentialAttackers(pCity->plot());

										iValue += (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pCity->plot(), MISSIONAI_ASSAULT, getGroup(), 2) * 3);

										if (canBombard(pLoopPlot)) {
											iValue *= 2;
										}

										iValue *= 1000;

										iValue /= (iPathTurns + 1);

										if (iPathTurns == 1) {
											//Prefer to have movement remaining to Bombard + Plunder
											iValue *= 1 + std::min(2, getPathFinder().GetFinalMoves());
										}

										// if not at war with this plot owner, then devalue plot if we already inside this owner's borders
										// (because declaring war will pop us some unknown distance away)
										if (!isEnemy(pLoopPlot->getTeam()) && plot()->getTeam() == pLoopPlot->getTeam()) {
											iValue /= 10;
										}

										if (iValue > iBestValue) {
											iBestValue = iValue;
											pBestPlot = getPathEndTurnPlot();
											pBestBlockadePlot = pLoopPlot;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestBlockadePlot != NULL)) {
		FAssert(canPlunder(pBestBlockadePlot));
		if (atPlot(pBestBlockadePlot) && !isEnemy(pBestBlockadePlot->getTeam(), pBestBlockadePlot)) {
			AI_considerPathDOW(pBestBlockadePlot, iFlags); // K-Mod
		}

		if (atPlot(pBestBlockadePlot)) {
			if (canBombard(plot())) {
				getGroup()->pushMission(MISSION_BOMBARD, -1, -1, 0, false, false, MISSIONAI_BLOCKADE, pBestBlockadePlot);
			}

			getGroup()->pushMission(MISSION_PLUNDER, -1, -1, 0, true, false, MISSIONAI_BLOCKADE, pBestBlockadePlot); // K-Mod

			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), iFlags, false, false, MISSIONAI_BLOCKADE, pBestBlockadePlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
// K-Mod todo: this function is very slow on large maps. Consider rewriting it!
bool CvUnitAI::AI_pirateBlockade() {
	PROFILE_FUNC();

	std::vector<int> aiDeathZone(GC.getMapINLINE().numPlotsINLINE(), 0);

	bool bIsInDanger = aiDeathZone[GC.getMap().plotNumINLINE(getX_INLINE(), getY_INLINE())] > 0;

	if (!bIsInDanger) {
		if (getDamage() > 0) {
			if (!plot()->isOwned() && !plot()->isAdjacentOwned()) {
				if (AI_retreatToCity(false, false, 1 + getDamage() / 20)) {
					return true;
				}
				getGroup()->pushMission(MISSION_SKIP);
				return true;
			}
		}
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestBlockadePlot = NULL;
	bool bBestIsForceMove = false;
	bool bBestIsMove = false;

	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot)) {
			if (!(pLoopPlot->isVisibleEnemyUnit(this)) && canPlunder(pLoopPlot)) {
				if (GC.getGame().getSorenRandNum(4, "AI Pirate Blockade") == 0) {
					if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BLOCKADE, getGroup(), 3) == 0) {
						int iPathTurns;
						if (generatePath(pLoopPlot, MOVE_AVOID_ENEMY_WEIGHT_3, true, &iPathTurns)) {
							int iBlockadedCount = 0;
							int iPopulationValue = 0;
							int iRange = GC.getDefineINT("SHIP_BLOCKADE_RANGE") - 1;
							for (int iX = -iRange; iX <= iRange; iX++) {
								for (int iY = -iRange; iY <= iRange; iY++) {
									CvPlot* pRangePlot = plotXY(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), iX, iY);
									if (pRangePlot != NULL) {
										bool bPlotBlockaded = false;
										if (pRangePlot->isWater() && pRangePlot->isOwned() && isEnemy(pRangePlot->getTeam(), pLoopPlot)) {
											bPlotBlockaded = true;
											iBlockadedCount += pRangePlot->getBlockadedCount(pRangePlot->getTeam());
										}

										if (!bPlotBlockaded) {
											CvCity* pPlotCity = pRangePlot->getPlotCity();
											if (pPlotCity != NULL) {
												if (isEnemy(pPlotCity->getTeam(), pLoopPlot)) {
													int iCityValue = 3 + pPlotCity->getPopulation();
													iCityValue *= (atWar(getTeam(), pPlotCity->getTeam()) ? 1 : 3);
													if (GET_PLAYER(pPlotCity->getOwnerINLINE()).isNoForeignTrade()) {
														iCityValue /= 2;
													}
													iPopulationValue += iCityValue;

												}
											}
										}
									}
								}
							}
							int iValue = iPopulationValue;

							iValue *= 1000;

							iValue /= 16 + iBlockadedCount;

							bool bMove = getPathFinder().GetPathTurns() == 1 && getPathFinder().GetFinalMoves() > 0;
							if (atPlot(pLoopPlot)) {
								iValue *= 3;
							} else if (bMove) {
								iValue *= 2;
							}

							int iDeath = aiDeathZone[GC.getMap().plotNumINLINE(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE())];

							bool bForceMove = false;
							if (iDeath) {
								iValue /= 10;
							} else if (bIsInDanger && (iPathTurns <= 2) && (0 == iPopulationValue)) {
								if (getPathFinder().GetFinalMoves() == 0) {
									if (!pLoopPlot->isAdjacentOwned()) {
										int iRand = GC.getGame().getSorenRandNum(2500, "AI Pirate Retreat");
										iValue += iRand;
										if (iRand > 1000) {
											iValue += GC.getGame().getSorenRandNum(2500, "AI Pirate Retreat");
											bForceMove = true;
										}
									}
								}
							}

							if (!bForceMove) {
								iValue /= iPathTurns + 1;
							}

							if (iValue > iBestValue) {
								iBestValue = iValue;
								pBestPlot = bForceMove ? pLoopPlot : getPathEndTurnPlot();
								pBestBlockadePlot = pLoopPlot;
								bBestIsForceMove = bForceMove;
								bBestIsMove = bMove;
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestBlockadePlot != NULL)) {
		FAssert(canPlunder(pBestBlockadePlot));

		if (atPlot(pBestBlockadePlot)) {
			getGroup()->pushMission(MISSION_PLUNDER, -1, -1, 0, true, false, MISSIONAI_BLOCKADE, pBestBlockadePlot); // K-Mod
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			if (bBestIsForceMove) {
				getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_AVOID_ENEMY_WEIGHT_3);
				return true;
			} else {
				getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_AVOID_ENEMY_WEIGHT_3, false, false, MISSIONAI_BLOCKADE, pBestBlockadePlot);
				if (bBestIsMove) {
					getGroup()->pushMission(MISSION_PLUNDER, -1, -1, 0, true, false, MISSIONAI_BLOCKADE, pBestBlockadePlot); // K-Mod
				}
				return true;
			}
		}
	}

	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_seaBombardRange(int iMaxRange) {
	PROFILE_FUNC();

	// cached values
	CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());
	CvPlot* pPlot = plot();
	CvSelectionGroup* pGroup = getGroup();

	// can any unit in this group bombard?
	bool bHasBombardUnit = false;
	bool bBombardUnitCanBombardNow = false;
	CLLNode<IDInfo>* pUnitNode = pGroup->headUnitNode();
	while (pUnitNode != NULL && !bBombardUnitCanBombardNow) {
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pGroup->nextUnitNode(pUnitNode);

		if (pLoopUnit->bombardRate() > 0) {
			bHasBombardUnit = true;

			if (pLoopUnit->canMove() && !pLoopUnit->isMadeAttack()) {
				bBombardUnitCanBombardNow = true;
			}
		}
	}

	if (!bHasBombardUnit) {
		return false;
	}

	// best match
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestBombardPlot = NULL;
	int iBestValue = 0;

	// iterate over plots at each range
	for (int iDX = -(iMaxRange); iDX <= iMaxRange; iDX++) {
		for (int iDY = -(iMaxRange); iDY <= iMaxRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL && AI_plotValid(pLoopPlot)) {
				CvCity* pBombardCity = bombardTarget(pLoopPlot);

				// Consider city even if fully bombarded, causes ship to camp outside blockading instead of switching between cities after bombarding to 0
				if (pBombardCity != NULL && isEnemy(pBombardCity->getTeam(), pLoopPlot) && pBombardCity->getDefenseDamage() < GC.getMAX_CITY_DEFENSE_DAMAGE()) {
					int iPathTurns;
					if (generatePath(pLoopPlot, 0, true, &iPathTurns, 1 + iMaxRange / baseMoves())) {
						// Loop construction doesn't guarantee we can get there anytime soon, could be on other side of narrow continent
						if (iPathTurns <= (1 + iMaxRange / baseMoves())) {
							// Check only for supporting our own ground troops first, if none will look for another target
							int iValue = (kPlayer.AI_plotTargetMissionAIs(pBombardCity->plot(), MISSIONAI_ASSAULT, NULL, 2) * 3);
							iValue += (kPlayer.AI_adjacentPotentialAttackers(pBombardCity->plot(), true));

							if (iValue > 0) {
								iValue *= 1000;

								iValue /= (iPathTurns + 1);

								if (iPathTurns == 1) {
									//Prefer to have movement remaining to Bombard + Plunder
									iValue *= 1 + std::min(2, getPathFinder().GetFinalMoves());
								}

								if (iValue > iBestValue) {
									iBestValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									pBestBombardPlot = pLoopPlot;
								}
							}
						}
					}
				}
			}
		}
	}

	// If no troops of ours to support, check for other bombard targets
	if ((pBestPlot == NULL) && (pBestBombardPlot == NULL)) {
		if ((AI_getUnitAIType() != UNITAI_ASSAULT_SEA)) {
			for (int iDX = -(iMaxRange); iDX <= iMaxRange; iDX++) {
				for (int iDY = -(iMaxRange); iDY <= iMaxRange; iDY++) {
					CvPlot* pLoopPlot = plotXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iDX, iDY);

					if (pLoopPlot != NULL && AI_plotValid(pLoopPlot)) {
						CvCity* pBombardCity = bombardTarget(pLoopPlot);

						// Consider city even if fully bombarded, causes ship to camp outside blockading instead of twitching between
						// cities after bombarding to 0
						if (pBombardCity != NULL && isEnemy(pBombardCity->getTeam(), pLoopPlot) && pBombardCity->getTotalDefense(false) > 0) {
							int iPathTurns;
							if (generatePath(pLoopPlot, 0, true, &iPathTurns, 1 + iMaxRange / maxMoves())) {
								// Loop construction doesn't guarantee we can get there anytime soon, could be on other side of narrow continent
								if (iPathTurns <= 1 + iMaxRange / maxMoves()) {
									int iValue = std::min(20, pBombardCity->getDefenseModifier(false) / 2);

									// Inclination to support attacks by others
									if (GET_PLAYER(pBombardCity->getOwnerINLINE()).AI_getAnyPlotDanger(pBombardCity->plot(), 2, false)) {
										iValue += 60;
									}

									// Inclination to bombard a different nearby city to extend the reach of blockade
									if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pBombardCity->plot(), MISSIONAI_BLOCKADE, getGroup(), 3) == 0) {
										iValue += 35 + pBombardCity->getPopulation();
									}

									// Small inclination to bombard area target, not too large so as not to tip our hand
									if (pBombardCity == pBombardCity->area()->getTargetCity(getOwnerINLINE())) {
										iValue += 10;
									}

									if (iValue > 0) {
										iValue *= 1000;

										iValue /= (iPathTurns + 1);

										if (iPathTurns == 1) {
											//Prefer to have movement remaining to Bombard + Plunder
											iValue *= 1 + std::min(2, getPathFinder().GetFinalMoves());
										}

										if (iValue > iBestValue) {
											iBestValue = iValue;
											pBestPlot = getPathEndTurnPlot();
											pBestBombardPlot = pLoopPlot;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestBombardPlot != NULL)) {
		if (atPlot(pBestBombardPlot)) {
			// if we are at the plot from which to bombard, and we have a unit that can bombard this turn, do it
			if (bBombardUnitCanBombardNow && pGroup->canBombard(pBestBombardPlot)) {
				getGroup()->pushMission(MISSION_BOMBARD, -1, -1, 0, false, false, MISSIONAI_BLOCKADE, pBestBombardPlot);

				// if city bombarded enough, wake up any units that were waiting to bombard this city
				CvCity* pBombardCity = bombardTarget(pBestBombardPlot); // is NULL if city cannot be bombarded any more
				if (pBombardCity == NULL || pBombardCity->getDefenseDamage() < ((GC.getMAX_CITY_DEFENSE_DAMAGE() * 5) / 6)) {
					kPlayer.AI_wakePlotTargetMissionAIs(pBestBombardPlot, MISSIONAI_BLOCKADE, getGroup());
				}
			}
			// otherwise, skip until next turn, when we will surely bombard
			else if (canPlunder(pBestBombardPlot)) {
				getGroup()->pushMission(MISSION_PLUNDER, -1, -1, 0, false, false, MISSIONAI_BLOCKADE, pBestBombardPlot);
			} else {
				getGroup()->pushMission(MISSION_SKIP);
			}

			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BLOCKADE, pBestBombardPlot);
			return true;
		}
	}

	return false;
}



// Returns true if a mission was pushed...
bool CvUnitAI::AI_canPillage(CvPlot& kPlot) const {
	if (isEnemy(kPlot.getTeam(), &kPlot)) {
		return true;
	}

	if (!kPlot.isOwned()) {
		bool bPillageUnowned = true;

		for (int iPlayer = 0; iPlayer < MAX_CIV_PLAYERS && bPillageUnowned; ++iPlayer) {
			int iIndx;
			CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iPlayer);
			if (!isEnemy(kLoopPlayer.getTeam(), &kPlot)) {
				for (CvCity* pCity = kLoopPlayer.firstCity(&iIndx); NULL != pCity; pCity = kLoopPlayer.nextCity(&iIndx)) {
					if (kPlot.getPlotGroup((PlayerTypes)iPlayer) == pCity->plot()->getPlotGroup((PlayerTypes)iPlayer)) {
						bPillageUnowned = false;
						break;
					}

				}
			}
		}

		if (bPillageUnowned) {
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_pillageRange(int iRange, int iBonusValueThreshold, bool bCheckCity, bool bWarOnly, bool bPillageBarbarians, bool bIgnoreDanger, int iFlags) {
	PROFILE_FUNC();

	int iSearchRange = AI_searchRange(iRange);

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestPillagePlot = NULL;

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot == NULL)
				continue;

			// Check of the plot meets our requirements, and if so get the value of the pillage
			if (!AI_plotValid(pLoopPlot))
				continue;

			if (!canPillage(pLoopPlot))
				continue;

			if (pLoopPlot->isBarbarian() && !bPillageBarbarians)
				continue;

			// Do we need the pillage action to be an act of war?
			if (bWarOnly && !potentialWarAction(pLoopPlot))
				continue;

			// Do we need a city working the plot
			CvCity* pWorkingCity = pLoopPlot->getWorkingCity();
			if (pWorkingCity == NULL && bCheckCity)
				continue;

			// Are we willing to get attacked
			if (pLoopPlot->isVisibleEnemyUnit(this) || !bIgnoreDanger)
				continue;

			// Don't pillage cities we are targetting, we may want those improvements
			if (pWorkingCity == area()->getTargetCity(getOwnerINLINE()))
				continue;

			// Ignore this plot if someone else is there first
			if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_PILLAGE, getGroup()) != 0)
				continue;

			// Check if we can get to the plot
			int iPathTurns;
			if (!generatePath(pLoopPlot, iFlags, true, &iPathTurns, iRange))
				continue;

			// Now check the range
			if (getPathFinder().GetFinalMoves() == 0)
				iPathTurns++;

			int iValue = AI_pillageValue(pLoopPlot, iBonusValueThreshold);
			iValue *= 1000;
			iValue /= (iPathTurns + 1);

			// if not at war with this plot owner, then devalue plot if we already inside this owner's borders
			// (because declaring war will pop us some unknown distance away)
			if (!isEnemy(pLoopPlot->getTeam()) && plot()->getTeam() == pLoopPlot->getTeam()) {
				iValue /= 10;
			}

			if (iValue > iBestValue) {
				iBestValue = iValue;
				pBestPlot = getPathEndTurnPlot();
				pBestPillagePlot = pLoopPlot;
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestPillagePlot != NULL)) {
		if (atPlot(pBestPillagePlot) && !isEnemy(pBestPillagePlot->getTeam())) {
			// rather than declare war, just find something else to do, since we may already be deep in enemy territory
			return false;
		}

		if (atPlot(pBestPillagePlot)) {
			if (isEnemy(pBestPillagePlot->getTeam())) {
				getGroup()->pushMission(MISSION_PILLAGE, -1, -1, 0, false, false, MISSIONAI_PILLAGE, pBestPillagePlot);
				return true;
			}
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), iFlags, false, false, MISSIONAI_PILLAGE, pBestPillagePlot);
			return true;
		}
	}

	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_found(int iFlags) {
	PROFILE_FUNC();

	int iBestFoundValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestFoundPlot = NULL;

	for (int iI = 0; iI < GET_PLAYER(getOwnerINLINE()).AI_getNumCitySites(); iI++) {
		CvPlot* pCitySitePlot = GET_PLAYER(getOwnerINLINE()).AI_getCitySite(iI);
		if (pCitySitePlot->getArea() == getArea() || canMoveAllTerrain()) {
			if (canFound(pCitySitePlot)) {
				if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pCitySitePlot, MISSIONAI_FOUND, getGroup()) == 0) {
					if (getGroup()->canDefend() || GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pCitySitePlot, MISSIONAI_GUARD_CITY) > 0) {
						int iPathTurns;
						if (generatePath(pCitySitePlot, iFlags, true, &iPathTurns)) {
							if (!pCitySitePlot->isVisible(getTeam(), false) || !pCitySitePlot->isVisibleEnemyUnit(this) || (iPathTurns > 1 && getGroup()->canDefend())) // K-Mod
							{
								int iValue = pCitySitePlot->getFoundValue(getOwnerINLINE());
								iValue *= 1000;
								iValue /= iPathTurns + (getGroup()->canDefend() ? 4 : 1); // K-Mod
								if (iValue > iBestFoundValue) {
									iBestFoundValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									pBestFoundPlot = pCitySitePlot;
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestFoundPlot != NULL)) {
		if (atPlot(pBestFoundPlot)) {
			if (gUnitLogLevel >= 2) {
				logBBAI("    Settler founding at site %d, %d", pBestFoundPlot->getX_INLINE(), pBestFoundPlot->getY_INLINE());
			}
			getGroup()->pushMission(MISSION_FOUND, -1, -1, 0, false, false, MISSIONAI_FOUND, pBestFoundPlot);
			return true;
		} else {
			if (gUnitLogLevel >= 2) {
				logBBAI("    Settler heading for site %d, %d", pBestFoundPlot->getX_INLINE(), pBestFoundPlot->getY_INLINE());
			}
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), iFlags, false, false, MISSIONAI_FOUND, pBestFoundPlot);
			return true;
		}
	}

	return false;
}


// K-Mod: this function simply checks if we are standing at our target destination
// and if we are, we issue the found command and return true.
// I've disabled (badly flawed) AI_foundRange, which was previously used for 'follow' AI.
bool CvUnitAI::AI_foundFollow() {
	if (canFound(plot()) && getGroup()->AI_getMissionAIPlot() == plot() && getGroup()->AI_getMissionAIType() == MISSIONAI_FOUND) {
		if (gUnitLogLevel >= 2) {
			logBBAI("    Settler founding at plot %d, %d (follow)", getX_INLINE(), getY_INLINE());
		}
		getGroup()->pushMission(MISSION_FOUND);
		return true;
	}

	return false;
}

// K-Mod. helper function for AI_assaultSeaTransport. (just to avoid code duplication)
static int estimateAndCacheCityDefence(CvPlayerAI& kPlayer, CvCity* pCity, std::map<CvCity*, int>& city_defence_cache) {
	// calculate the city's defences, or read from the cache if we've already done it.
	std::map<CvCity*, int>::iterator city_it = city_defence_cache.find(pCity);
	int iDefenceStrength = -1;
	if (city_it == city_defence_cache.end()) {
		if (pCity->plot()->isVisible(kPlayer.getTeam(), false)) {
			iDefenceStrength = kPlayer.AI_localDefenceStrength(pCity->plot(), NO_TEAM);
		} else {
			// If we don't have vision of the city, we should try to estimate its strength based the expected number of defenders.
			int iUnitStr = GET_PLAYER(pCity->getOwnerINLINE()).getTypicalUnitValue(UNITAI_CITY_DEFENSE, DOMAIN_LAND) * GC.getGameINLINE().getBestLandUnitCombat() / 100;
			iDefenceStrength = std::max(GET_TEAM(kPlayer.getTeam()).AI_getStrengthMemory(pCity->plot()), pCity->AI_neededDefenders() * iUnitStr);
		}
		city_defence_cache[pCity] = iDefenceStrength;
	} else {
		// use the cached value
		iDefenceStrength = city_it->second;
	}
	return iDefenceStrength;
}

// Returns true if a mission was pushed...
// This fucntion has been mostly rewritten for K-Mod.
bool CvUnitAI::AI_assaultSeaTransport(bool bAttackBarbs, bool bLocal) {
	PROFILE_FUNC();

	FAssert(getGroup()->hasCargo());

	CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	int iLimitedAttackers = 0;
	int iAmphibiousAttackers = 0;
	int iAmphibiousAttackStrength = 0;
	int iLandedAttackStrength = 0;
	int iCollateralDamageScale = estimateCollateralWeight(0, getTeam());
	std::map<CvCity*, int> city_defence_cache;

	std::vector<CvUnit*> aGroupCargo;
	CLLNode<IDInfo>* pUnitNode = plot()->headUnitNode();
	while (pUnitNode != NULL) {
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = plot()->nextUnitNode(pUnitNode);
		CvUnit* pTransport = pLoopUnit->getTransportUnit();
		if (pTransport != NULL && pTransport->getGroup() == getGroup()) {
			aGroupCargo.push_back(pLoopUnit);
			// K-Mod. Gather some data for later...
			iLimitedAttackers += (pLoopUnit->combatLimit() < 100 ? 1 : 0);
			iAmphibiousAttackers += (pLoopUnit->isAmphib() ? 1 : 0);

			// Estimate attack strength, both for landed assaults and amphibious assaults.
			//
			// Unfortunately, we can't use AI_localAttackStrength because that may miscount
			// depending on whether there is another group on this plot and things like that,
			// and we can't use AI_sumStrength because that currently only works for groups.
			// What we have here is a list of cargo units rather than a group.
			if (pLoopUnit->canAttack()) {
				int iUnitStr = pLoopUnit->currEffectiveStr(NULL, NULL);

				iUnitStr *= 100 + 4 * pLoopUnit->firstStrikes() + 2 * pLoopUnit->chanceFirstStrikes();
				iUnitStr /= 100;

				if (pLoopUnit->collateralDamage() > 0)
					iUnitStr += pLoopUnit->baseCombatStr() * iCollateralDamageScale * pLoopUnit->collateralDamage() * pLoopUnit->collateralDamageMaxUnits() / 10000;

				iLandedAttackStrength += iUnitStr;

				if (pLoopUnit->combatLimit() >= 100 && pLoopUnit->canMove() && (!pLoopUnit->isMadeAttack() || pLoopUnit->isBlitz())) {
					if (!pLoopUnit->isAmphib())
						iUnitStr += iUnitStr * GC.getAMPHIB_ATTACK_MODIFIER() / 100;

					iAmphibiousAttackStrength += iUnitStr;
				}
			}
		}
	}

	int iFlags = MOVE_AVOID_ENEMY_WEIGHT_3 | MOVE_DECLARE_WAR; // K-Mod
	int iCargo = getGroup()->getCargo();
	FAssert(iCargo > 0);
	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestAssaultPlot = NULL;

	// K-Mod note: I've restructured and rewritten this section for efficiency, clarity, and sometimes even to improve the AI!
	// Most of the original code has been deleted.
	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (!pLoopPlot->isRevealed(getTeam(), false))
			continue;
		if (!pLoopPlot->isOwned())
			continue;
		if (!bAttackBarbs && pLoopPlot->isBarbarian() && !kOwner.isMinorCiv())
			continue;
		if (!pLoopPlot->isCoastalLand())
			continue;
		if (!isPotentialEnemy(pLoopPlot->getTeam(), pLoopPlot))
			continue;

		// Note: currently these condtions mean we will never land to invade land-locked enemies

		int iTargetCities = pLoopPlot->area()->getCitiesPerPlayer(pLoopPlot->getOwnerINLINE());
		if (iTargetCities == 0)
			continue;

		int iPathTurns;
		if (!generatePath(pLoopPlot, iFlags, true, &iPathTurns))
			continue;

		CvCity* pCity = pLoopPlot->getPlotCity();
		// If the plot can't be seen, then just roughly estimate what the AI might think is there...
		int iEnemyDefenders = (pLoopPlot->isVisible(getTeam(), false) || GET_TEAM(getTeam()).AI_getStrengthMemory(pLoopPlot))
			? pLoopPlot->getNumVisiblePotentialEnemyDefenders(this)
			: (pCity ? pCity->AI_neededDefenders() : 0);

		int iBaseValue = 10 + std::min(9, 3 * iTargetCities);
		int iValueMultiplier = 100;

		// if there are defenders, we should decide whether or not it is worth attacking them amphibiously.
		if (iEnemyDefenders > 0) {
			if ((iLimitedAttackers > 0 || iAmphibiousAttackers < iCargo / 2) && iEnemyDefenders * 3 > 2 * (iCargo - iLimitedAttackers))
				continue;

			int iDefenceStrength = -1;
			if (pCity)
				iDefenceStrength = estimateAndCacheCityDefence(kOwner, pCity, city_defence_cache);
			else
				iDefenceStrength = pLoopPlot->isVisible(getTeam(), false) ? kOwner.AI_localDefenceStrength(pLoopPlot, NO_TEAM) : GET_TEAM(kOwner.getTeam()).AI_getStrengthMemory(pLoopPlot);
			// Note: the amphibious attack modifier is already taken into account by AI_localAttackStrength,
			// but I'm going to apply a similar penality again just to discourage the AI from attacking amphibiously when they don't need to.
			iDefenceStrength -= iDefenceStrength * GC.getAMPHIB_ATTACK_MODIFIER() * (iCargo - iAmphibiousAttackers) / (100 * iCargo);

			if (iAmphibiousAttackStrength * 100 < iDefenceStrength * GC.getBBAI_ATTACK_CITY_STACK_RATIO())
				continue;

			if (pCity == NULL)
				iValueMultiplier = iValueMultiplier * (iAmphibiousAttackStrength - iDefenceStrength * std::min(iCargo - iLimitedAttackers, iEnemyDefenders) / iEnemyDefenders) / iAmphibiousAttackStrength;
		}

		if (pCity == NULL) {
			// consider landing on strategic resources
			iBaseValue += AI_pillageValue(pLoopPlot, 15);

			// prefer to land on a defensive plot, but not with a river between us and the city
			int iModifier = 0;
			if (pCity && pLoopPlot->isRiverCrossing(directionXY(pLoopPlot, pCity->plot())))
				iModifier += GC.getRIVER_ATTACK_MODIFIER() / 10;

			iModifier += pLoopPlot->defenseModifier(getTeam(), false) / 10;

			iValueMultiplier = (100 + iModifier) * iValueMultiplier / 100;

			// Look look for adjacent cities.
			for (DirectionTypes dir = (DirectionTypes)0; dir < NUM_DIRECTION_TYPES; dir = (DirectionTypes)(dir + 1)) {
				CvPlot* pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), dir);
				if (pAdjacentPlot == NULL)
					continue;

				pCity = pAdjacentPlot->getPlotCity();

				if (pCity != NULL) {
					if (pCity->getOwnerINLINE() == pLoopPlot->getOwnerINLINE())
						break;
					else
						pCity = NULL;
				}
			}
		}


		if (pCity != NULL) {
			int iDefenceStrength = estimateAndCacheCityDefence(kOwner, pCity, city_defence_cache);

			FAssert(isPotentialEnemy(pCity->getTeam(), pLoopPlot));
			iBaseValue += kOwner.AI_targetCityValue(pCity, false, false); // maybe false, true?

			if (pCity->plot() == pLoopPlot)
				iValueMultiplier *= pLoopPlot->isVisible(getTeam(), false) ? 5 : 2; // apparently we can take the city amphibiously
			else {
				// prefer to join existing assaults. (maybe we should calculate the actual attack strength here and roll it into the strength comparison modifier below)
				int iModifier = std::min(kOwner.AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_ASSAULT, getGroup()) + kOwner.AI_adjacentPotentialAttackers(pCity->plot()), 2 * iCargo) * 100 / iCargo;
				iValueMultiplier = (100 + iModifier) * iValueMultiplier / 100;

				// Prefer to target cities that we can defeat.
				// However, keep in mind that if we often won't be able to see the city to gauge their defenses.


				if (iDefenceStrength > 0 || pLoopPlot->isVisible(getTeam(), false)) // otherwise, assume we have no idea what's there.
				{
					if (pLoopPlot->isVisible(getTeam(), false))
						iModifier = std::min(100, 125 * iLandedAttackStrength / std::max(1, iDefenceStrength) - 25);
					else
						iModifier = std::min(50, 75 * iLandedAttackStrength / std::max(1, iDefenceStrength) - 25);
				}

				iValueMultiplier = (100 + iModifier) * iValueMultiplier / 100;
			}
		}

		// Continue attacking in area we have already captured cities
		if (pLoopPlot->area()->getCitiesPerPlayer(getOwnerINLINE()) > 0) {
			if (pCity != NULL && (bLocal || pCity->AI_playerCloseness(getOwnerINLINE()) > 5)) {
				iValueMultiplier = iValueMultiplier * 3 / 2;
			}
		} else if (bLocal) {
			iValueMultiplier = iValueMultiplier * 2 / 3;
		}

		// K-Mod note: It would be nice to use the pathfinder again here
		// to make sure we aren't landing a long way from any enemy cities;
		// otherwise the AI might get into a loop of contantly dropping
		// off units which just walk back to the city to be dropped off again.
		// (maybe some other time)

		FAssert(iPathTurns > 0);

		// K-Mod. A bit of randomness is good, but if it's random every turn then it will lead to inconsistent decisions.
		// So.. if we're already en-route somewhere, try to keep going there.
		if (pCity && getGroup()->AI_getMissionAIPlot() && stepDistance(getGroup()->AI_getMissionAIPlot(), pCity->plot()) <= 1) {
			iValueMultiplier *= 150;
			iValueMultiplier /= 100;
		} else if (iPathTurns > 2) {
			iValueMultiplier *= 60 + (AI_unitPlotHash(pLoopPlot, getGroup()->getNumUnits()) % 81);
			iValueMultiplier /= 100;
		}
		int iValue = iBaseValue * iValueMultiplier / (iPathTurns + 2);

		if (iValue > iBestValue) {
			iBestValue = iValue;
			pBestPlot = getPathEndTurnPlot();
			pBestAssaultPlot = pLoopPlot;
		}
	}

	if (pBestPlot != NULL && pBestAssaultPlot != NULL) {
		return AI_transportGoTo(pBestPlot, pBestAssaultPlot, iFlags, MISSIONAI_ASSAULT);
	}

	return false;
}

// Returns true if a mission was pushed...
// This function has been heavily edited and restructured by K-Mod. It includes some bbai changes.
bool CvUnitAI::AI_assaultSeaReinforce(bool bAttackBarbs) {
	PROFILE_FUNC();

	bool bIsAttackCity = (getUnitAICargo(UNITAI_ATTACK_CITY) > 0);

	FAssert(getGroup()->hasCargo());
	FAssert(getGroup()->canAllMove()); // K-Mod (replacing a BBAI check that I'm sure is unnecessary.)

	std::vector<CvUnit*> aGroupCargo;
	CLLNode<IDInfo>* pUnitNode = plot()->headUnitNode();
	while (pUnitNode != NULL) {
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = plot()->nextUnitNode(pUnitNode);
		CvUnit* pTransport = pLoopUnit->getTransportUnit();
		if (pTransport != NULL && pTransport->getGroup() == getGroup()) {
			aGroupCargo.push_back(pLoopUnit);
		}
	}

	// K-Mod note: bCity was used in a few places, but it should not be. If we make a decision based on being in a city, then we'll just change our mind as soon as we leave the city!

	int iCargo = getGroup()->getCargo();
	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestAssaultPlot = NULL;
	CvArea* pWaterArea = plot()->waterArea();
	bool bCanMoveAllTerrain = getGroup()->canMoveAllTerrain();
	int iFlags = MOVE_AVOID_ENEMY_WEIGHT_3; // K-Mod. (no declare war)

	// Loop over nearby plots for groups in enemy territory to reinforce
	int iRange = 2 * getGroup()->baseMoves();
	for (int iDX = -(iRange); iDX <= iRange; iDX++) {
		for (int iDY = -(iRange); iDY <= iRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot && isEnemy(pLoopPlot->getTeam(), pLoopPlot)) {
				if (bCanMoveAllTerrain || (pWaterArea != NULL && pLoopPlot->isAdjacentToArea(pWaterArea))) {
					int iTargetCities = pLoopPlot->area()->getCitiesPerPlayer(pLoopPlot->getOwnerINLINE());

					if (iTargetCities > 0) {
						int iOurFightersHere = pLoopPlot->getNumDefenders(getOwnerINLINE());

						if (iOurFightersHere > 2) {
							int iPathTurns;
							if (generatePath(pLoopPlot, iFlags, true, &iPathTurns, 2)) {
								CvPlot* pEndTurnPlot = getPathEndTurnPlot();

								int iValue = 10 * iTargetCities;
								iValue += 8 * iOurFightersHere;
								iValue += 3 * GET_PLAYER(getOwnerINLINE()).AI_adjacentPotentialAttackers(pLoopPlot);

								iValue *= 100;

								iValue /= (iPathTurns + 1);

								if (iValue > iBestValue) {
									iBestValue = iValue;
									pBestPlot = pEndTurnPlot;
									pBestAssaultPlot = pLoopPlot;
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot && pBestAssaultPlot)
		return AI_transportGoTo(pBestPlot, pBestAssaultPlot, iFlags, MISSIONAI_REINFORCE);

	// Loop over other transport groups, looking for synchronized landing
	int iLoop;
	for (CvSelectionGroup* pLoopSelectionGroup = GET_PLAYER(getOwnerINLINE()).firstSelectionGroup(&iLoop); pLoopSelectionGroup; pLoopSelectionGroup = GET_PLAYER(getOwnerINLINE()).nextSelectionGroup(&iLoop)) {
		if (pLoopSelectionGroup != getGroup()) {
			if (pLoopSelectionGroup->AI_getMissionAIType() == MISSIONAI_ASSAULT && pLoopSelectionGroup->getHeadUnitAI() == UNITAI_ASSAULT_SEA) // K-Mod. (b/c assault is also used for ground units)
			{
				CvPlot* pLoopPlot = pLoopSelectionGroup->AI_getMissionAIPlot();

				if (pLoopPlot && isPotentialEnemy(pLoopPlot->getTeam(), pLoopPlot)) {
					if (bCanMoveAllTerrain || (pWaterArea != NULL && pLoopPlot->isAdjacentToArea(pWaterArea))) {
						int iTargetCities = pLoopPlot->area()->getCitiesPerPlayer(pLoopPlot->getOwnerINLINE());
						if (iTargetCities <= 0)
							continue;

						int iAssaultsHere = pLoopSelectionGroup->getCargo();

						if (iAssaultsHere < 3)
							continue;

						int iPathTurns;
						if (generatePath(pLoopPlot, iFlags, true, &iPathTurns)) {
							CvPlot* pEndTurnPlot = getPathEndTurnPlot();

							int iOtherPathTurns = MAX_INT;
							// K-Mod. Use a different pathfinder, so that we don't clear our path data.
							KmodPathFinder loop_path;
							loop_path.SetSettings(pLoopSelectionGroup, iFlags, iPathTurns);
							if (loop_path.GeneratePath(pLoopPlot)) {
								iOtherPathTurns = loop_path.GetPathTurns(); // (K-Mod note: I'm not convinced the +1 thing is a good idea.)
							} else {
								continue;
							}

							FAssert(iOtherPathTurns <= iPathTurns);
							if (iPathTurns >= iOtherPathTurns + 5)
								continue;

							bool bCanCargoAllUnload = true;
							int iEnemyDefenders = pLoopPlot->getNumVisibleEnemyDefenders(this);
							if (iEnemyDefenders > 0 || pLoopPlot->isCity()) {
								for (uint i = 0; i < aGroupCargo.size(); ++i) {
									CvUnit* pAttacker = aGroupCargo[i];
									if (!pLoopPlot->hasDefender(true, NO_PLAYER, pAttacker->getOwnerINLINE(), pAttacker, true)) {
										bCanCargoAllUnload = false;
										break;
									} else if (pLoopPlot->isCity() && !(pLoopPlot->isVisible(getTeam(), false))) {
										// Artillery can't naval invade, so don't try
										if (pAttacker->combatLimit() < 100) {
											bCanCargoAllUnload = false;
											break;
										}
									}
								}
							}

							int iValue = (iAssaultsHere * 5);
							iValue += iTargetCities * 10;

							iValue *= 100;

							// K-Mod. More consistent randomness, to prevent decisions from oscillating.
							iValue *= 70 + (AI_unitPlotHash(pLoopPlot, getGroup()->getNumUnits()) % 61);
							iValue /= 100;

							iValue /= (iPathTurns + 2);

							if (iValue > iBestValue) {
								iBestValue = iValue;
								pBestPlot = pEndTurnPlot;
								pBestAssaultPlot = pLoopPlot;
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot && pBestAssaultPlot)
		return AI_transportGoTo(pBestPlot, pBestAssaultPlot, iFlags, MISSIONAI_REINFORCE);

	// Reinforce our cities in need
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if (bCanMoveAllTerrain || (pWaterArea != NULL && (pLoopCity->waterArea(true) == pWaterArea || pLoopCity->secondWaterArea() == pWaterArea))) {
			int iValue = 0;
			if (pLoopCity->area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE) {
				iValue = 3;
			} else if (pLoopCity->area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE) {
				iValue = 2;
			} else if (pLoopCity->area()->getAreaAIType(getTeam()) == AREAAI_MASSING) {
				iValue = 1;
			} else if (bAttackBarbs && (pLoopCity->area()->getCitiesPerPlayer(BARBARIAN_PLAYER) > 0)) {
				iValue = 1;
			} else
				continue;

			bool bCityDanger = pLoopCity->AI_isDanger();
			if (pLoopCity->area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE || bCityDanger || (GC.getGameINLINE().getGameTurn() - pLoopCity->getGameTurnAcquired() < 10 && pLoopCity->getPreviousOwner() != NO_PLAYER)) // K-Mod
			{
				int iOurPower = std::max(1, pLoopCity->area()->getPower(getOwnerINLINE()));
				// Enemy power includes barb power
				int iEnemyPower = GET_TEAM(getTeam()).countEnemyPowerByArea(pLoopCity->area());

				// Don't send troops to areas we are dominating already
				// Don't require presence of enemy cities, just a dangerous force
				if (iOurPower < (3 * iEnemyPower)) {
					int iPathTurns;
					if (generatePath(pLoopCity->plot(), iFlags, true, &iPathTurns)) {
						iValue *= 10 * pLoopCity->AI_cityThreat();

						iValue += 20 * GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_ASSAULT, getGroup());

						iValue *= std::min(iEnemyPower, 3 * iOurPower);
						iValue /= iOurPower;

						iValue *= 100;

						// if more than 3 turns to get there, then put some randomness into our preference of distance
						// +/- 33%
						if (iPathTurns > 3) {
							int iPathAdjustment = GC.getGameINLINE().getSorenRandNum(67, "AI Assault Target");

							iPathTurns *= 66 + iPathAdjustment;
							iPathTurns /= 100;
						}

						iValue /= (iPathTurns + 6);

						if (iValue > iBestValue) {
							iBestValue = iValue;
							pBestPlot = getPathEndTurnPlot(); // K-Mod (why did they have that other stuff?)
							pBestAssaultPlot = pLoopCity->plot();
						}
					}
				}
			}
		}
	}

	if (pBestPlot && pBestAssaultPlot)
		return AI_transportGoTo(pBestPlot, pBestAssaultPlot, iFlags, MISSIONAI_REINFORCE);

	// assist master in attacking
	if (GET_TEAM(getTeam()).isAVassal()) {
		TeamTypes eMasterTeam = NO_TEAM;

		for (int iI = 0; iI < MAX_CIV_TEAMS; iI++) {
			if (GET_TEAM(getTeam()).isVassal((TeamTypes)iI)) {
				eMasterTeam = (TeamTypes)iI;
			}
		}

		if ((eMasterTeam != NO_TEAM) && GET_TEAM(getTeam()).isOpenBorders(eMasterTeam)) {
			for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++) {
				if (GET_PLAYER((PlayerTypes)iI).getTeam() == eMasterTeam) {
					for (CvCity* pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop)) {
						if (pLoopCity->area() == area())
							continue; // otherwise we can just walk there. (probably)

						int iValue = 0;
						switch (pLoopCity->area()->getAreaAIType(eMasterTeam)) {
						case AREAAI_OFFENSIVE:
							iValue = 2;
							break;

						case AREAAI_MASSING:
							iValue = 1;
							break;

						default:
							continue; // not an appropriate area.
						}

						if (bCanMoveAllTerrain || (pWaterArea != NULL && (pLoopCity->waterArea(true) == pWaterArea || pLoopCity->secondWaterArea() == pWaterArea))) {
							int iOurPower = std::max(1, pLoopCity->area()->getPower(getOwnerINLINE()));
							iOurPower += GET_TEAM(eMasterTeam).countPowerByArea(pLoopCity->area());
							// Enemy power includes barb power
							int iEnemyPower = GET_TEAM(eMasterTeam).countEnemyPowerByArea(pLoopCity->area());

							// Don't send troops to areas we are dominating already
							// Don't require presence of enemy cities, just a dangerous force
							if (iOurPower < (2 * iEnemyPower)) {
								int iPathTurns;
								if (generatePath(pLoopCity->plot(), iFlags, true, &iPathTurns)) {
									iValue *= pLoopCity->AI_cityThreat();

									iValue += 10 * GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_ASSAULT, getGroup());

									iValue *= std::min(iEnemyPower, 3 * iOurPower);
									iValue /= iOurPower;

									iValue *= 100;

									// if more than 3 turns to get there, then put some randomness into our preference of distance
									// +/- 33%
									if (iPathTurns > 3) {
										int iPathAdjustment = GC.getGameINLINE().getSorenRandNum(67, "AI Assault Target");

										iPathTurns *= 66 + iPathAdjustment;
										iPathTurns /= 100;
									}

									iValue /= (iPathTurns + 1);

									if (iValue > iBestValue) {
										iBestValue = iValue;
										pBestPlot = getPathEndTurnPlot();
										pBestAssaultPlot = pLoopCity->plot();
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot && pBestAssaultPlot)
		return AI_transportGoTo(pBestPlot, pBestAssaultPlot, iFlags, MISSIONAI_REINFORCE);

	return false;
}

// K-Mod. General function for moving assault groups - to reduce code duplication.
bool CvUnitAI::AI_transportGoTo(CvPlot* pEndTurnPlot, CvPlot* pTargetPlot, int iFlags, MissionAITypes eMissionAI) {
	FAssert(pEndTurnPlot && pTargetPlot);
	FAssert(!pEndTurnPlot->isImpassable(getTeam()));

	if (getGroup()->AI_getMissionAIType() != eMissionAI) {
		// Cancel missions of all those coming to join departing transport
		int iLoop = 0;
		CvPlayer& kOwner = GET_PLAYER(getOwnerINLINE());

		for (CvSelectionGroup* pLoopGroup = kOwner.firstSelectionGroup(&iLoop); pLoopGroup != NULL; pLoopGroup = kOwner.nextSelectionGroup(&iLoop)) {
			if (pLoopGroup != getGroup()) {
				if (pLoopGroup->AI_getMissionAIType() == MISSIONAI_GROUP) {
					CvUnit* pMissionUnit = pLoopGroup->AI_getMissionAIUnit();

					if (pMissionUnit && pMissionUnit->getGroup() == getGroup() && !pLoopGroup->isFull()) {
						pLoopGroup->clearMissionQueue();
						pLoopGroup->AI_setMissionAI(NO_MISSIONAI, 0, 0);
					}
				}
			}
		}
	}

	if (atPlot(pTargetPlot)) {
		getGroup()->unloadAll(); // XXX is this dangerous (not pushing a mission...) XXX air units?
		getGroup()->setActivityType(ACTIVITY_AWAKE); // K-Mod
		return true;
	} else {
		if (getGroup()->isAmphibPlot(pTargetPlot)) {
			// If target is actually an amphibious landing from pEndTurnPlot, then set pEndTurnPlot = pTargetPlot so that we can land this turn.
			if (pTargetPlot != pEndTurnPlot && stepDistance(pTargetPlot, pEndTurnPlot) == 1) {
				pEndTurnPlot = pTargetPlot;
			}

			// if our cargo isn't going to be ready to land, just wait.
			if (pTargetPlot == pEndTurnPlot && !getGroup()->canCargoAllMove()) {
				getGroup()->pushMission(MISSION_SKIP, -1, -1, iFlags, false, false, eMissionAI, pTargetPlot);
				return true;
			}
		}

		// declare war if we need to. (Note: AI_considerPathDOW checks for the declare war flag.)
		if (AI_considerPathDOW(pEndTurnPlot, iFlags)) {
			if (!generatePath(pTargetPlot, iFlags, false))
				return false;
			pEndTurnPlot = getPathEndTurnPlot();
		}

		// Group all moveable land units together before landing,
		// this will help the AI to think more clearly about attacking on the next turn.
		if (pEndTurnPlot == pTargetPlot && !pTargetPlot->isWater() && !pTargetPlot->isCity()) {
			CvSelectionGroup* pCargoGroup = NULL;
			CLLNode<IDInfo>* pUnitNode = getGroup()->headUnitNode();

			while (pUnitNode != NULL) {
				CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
				pUnitNode = getGroup()->nextUnitNode(pUnitNode);

				std::vector<CvUnit*> cargo_units;
				pLoopUnit->getCargoUnits(cargo_units);

				for (size_t i = 0; i < cargo_units.size(); i++) {
					if (cargo_units[i]->getGroup() != pCargoGroup &&
						cargo_units[i]->getDomainType() == DOMAIN_LAND && cargo_units[i]->canMove()) {
						if (pCargoGroup) {
							cargo_units[i]->joinGroup(pCargoGroup);
						} else {
							if (!cargo_units[i]->getGroup()->canAllMove()) {
								cargo_units[i]->joinGroup(NULL); // separate from units that can't move.
							}
							pCargoGroup = cargo_units[i]->getGroup();
						}
					}
				}
			}
		}
		//
		getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX_INLINE(), pEndTurnPlot->getY_INLINE(), iFlags, false, false, eMissionAI, pTargetPlot);
		return true;
	}
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_settlerSeaTransport() {
	PROFILE_FUNC();

	FAssert(getCargo() > 0);
	FAssert(getUnitAICargo(UNITAI_SETTLE) > 0);

	if (!getGroup()->canCargoAllMove()) {
		return false;
	}

	//New logic should allow some new tricks like 
	//unloading settlers when a better site opens up locally
	//and delivering settlers
	//to inland sites

	CvArea* pWaterArea = plot()->waterArea();
	FAssertMsg(pWaterArea != NULL, "Ship out of water?");

	CvUnit* pSettlerUnit = NULL;
	CvPlot* pPlot = plot();
	CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();

	while (pUnitNode != NULL) {
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pPlot->nextUnitNode(pUnitNode);

		if (pLoopUnit->getTransportUnit() == this) {
			if (pLoopUnit->AI_getUnitAIType() == UNITAI_SETTLE) {
				pSettlerUnit = pLoopUnit;
				break;
			}
		}
	}

	FAssert(pSettlerUnit != NULL);

	int iAreaBestFoundValue = 0;
	CvPlot* pAreaBestPlot = NULL;

	int iOtherAreaBestFoundValue = 0;
	CvPlot* pOtherAreaBestPlot = NULL;

	KmodPathFinder land_path;
	land_path.SetSettings(pSettlerUnit->getGroup(), MOVE_SAFE_TERRITORY);

	for (int iI = 0; iI < GET_PLAYER(getOwnerINLINE()).AI_getNumCitySites(); iI++) {
		CvPlot* pCitySitePlot = GET_PLAYER(getOwnerINLINE()).AI_getCitySite(iI);
		if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pCitySitePlot, MISSIONAI_FOUND, getGroup()) == 0) {
			int iValue = pCitySitePlot->getFoundValue(getOwnerINLINE());
			//if (pCitySitePlot->getArea() == getArea())
			if (pCitySitePlot->getArea() == getArea() && land_path.GeneratePath(pCitySitePlot)) // K-Mod
			{
				if (iValue > iAreaBestFoundValue) {
					iAreaBestFoundValue = iValue;
					pAreaBestPlot = pCitySitePlot;
				}
			} else {
				if (iValue > iOtherAreaBestFoundValue) {
					iOtherAreaBestFoundValue = iValue;
					pOtherAreaBestPlot = pCitySitePlot;
				}
			}
		}
	}
	if ((0 == iAreaBestFoundValue) && (0 == iOtherAreaBestFoundValue)) {
		return false;
	}

	if (iAreaBestFoundValue > iOtherAreaBestFoundValue) {
		//let the settler walk.
		getGroup()->unloadAll();
		getGroup()->pushMission(MISSION_SKIP);
		return true;
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestFoundPlot = NULL;

	for (int iI = 0; iI < GET_PLAYER(getOwnerINLINE()).AI_getNumCitySites(); iI++) {
		CvPlot* pCitySitePlot = GET_PLAYER(getOwnerINLINE()).AI_getCitySite(iI);
		if (!(pCitySitePlot->isVisibleEnemyUnit(this))) {
			if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pCitySitePlot, MISSIONAI_FOUND, getGroup(), 4) == 0) {
				int iPathTurns;
				// BBAI TODO: Nearby plots too if much shorter (settler walk from there)
				// also, if plots are in area player already has cities, then may not be coastal ... (see Earth 1000 AD map for Inca)
				if (generatePath(pCitySitePlot, 0, true, &iPathTurns)) {
					int iValue = pCitySitePlot->getFoundValue(getOwnerINLINE());
					iValue *= 1000;
					iValue /= (2 + iPathTurns);

					if (iValue > iBestValue) {
						iBestValue = iValue;
						pBestPlot = getPathEndTurnPlot();
						pBestFoundPlot = pCitySitePlot;
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestFoundPlot != NULL)) {
		FAssert(!pBestPlot->isImpassable(getTeam()));

		if ((pBestPlot == pBestFoundPlot) || (stepDistance(pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), pBestFoundPlot->getX_INLINE(), pBestFoundPlot->getY_INLINE()) == 1)) {
			if (atPlot(pBestFoundPlot)) {
				unloadAll(); // XXX is this dangerous (not pushing a mission...) XXX air units?
				getGroup()->setActivityType(ACTIVITY_AWAKE); // K-Mod
				return true;
			} else {
				getGroup()->pushMission(MISSION_MOVE_TO, pBestFoundPlot->getX_INLINE(), pBestFoundPlot->getY_INLINE(), 0, false, false, MISSIONAI_FOUND, pBestFoundPlot);
				return true;
			}
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_FOUND, pBestFoundPlot);
			return true;
		}
	}

	//Try original logic
	//(sometimes new logic breaks)
	pPlot = plot();

	iBestValue = 0;
	pBestPlot = NULL;
	pBestFoundPlot = NULL;

	int iMinFoundValue = GET_PLAYER(getOwnerINLINE()).AI_getMinFoundValue();

	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		//if (pLoopPlot->isCoastalLand())
		// K-Mod. Only consider areas we have explored, and only land if we know there is something we want to settle.
		int iAreaBest; // (currently unused)
		if (pLoopPlot->isCoastalLand() && pLoopPlot->isRevealed(getTeam(), false) && GET_PLAYER(getOwnerINLINE()).AI_getNumAreaCitySites(pLoopPlot->getArea(), iAreaBest) > 0) {
			int iValue = pLoopPlot->getFoundValue(getOwnerINLINE());

			if ((iValue > iBestValue) && (iValue >= iMinFoundValue)) {
				bool bValid = false;

				pUnitNode = pPlot->headUnitNode();

				while (pUnitNode != NULL) {
					CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
					pUnitNode = pPlot->nextUnitNode(pUnitNode);

					if (pLoopUnit->getTransportUnit() == this) {
						if (pLoopUnit->canFound(pLoopPlot)) {
							bValid = true;
							break;
						}
					}
				}

				if (bValid) {
					if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_FOUND, getGroup(), 4) == 0) {
							if (generatePath(pLoopPlot, 0, true)) {
								iBestValue = iValue;
								pBestPlot = getPathEndTurnPlot();
								pBestFoundPlot = pLoopPlot;
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestFoundPlot != NULL)) {
		FAssert(!pBestPlot->isImpassable(getTeam()));

		if ((pBestPlot == pBestFoundPlot) || (stepDistance(pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), pBestFoundPlot->getX_INLINE(), pBestFoundPlot->getY_INLINE()) == 1)) {
			if (atPlot(pBestFoundPlot)) {
				unloadAll(); // XXX is this dangerous (not pushing a mission...) XXX air units?
				getGroup()->setActivityType(ACTIVITY_AWAKE); // K-Mod
				return true;
			} else {
				getGroup()->pushMission(MISSION_MOVE_TO, pBestFoundPlot->getX_INLINE(), pBestFoundPlot->getY_INLINE(), 0, false, false, MISSIONAI_FOUND, pBestFoundPlot);
				return true;
			}
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_FOUND, pBestFoundPlot);
			return true;
		}
	}
	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_settlerSeaFerry() {
	PROFILE_FUNC();

	FAssert(getCargo() > 0);
	FAssert(getUnitAICargo(UNITAI_WORKER) > 0);

	if (!getGroup()->canCargoAllMove()) {
		return false;
	}

	CvArea* pWaterArea = plot()->waterArea();
	FAssertMsg(pWaterArea != NULL, "Ship out of water?");

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		int iValue = pLoopCity->AI_getWorkersNeeded();
		if (iValue > 0) {
			iValue -= GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_FOUND, getGroup());
			if (iValue > 0) {
				int iPathTurns;
				if (generatePath(pLoopCity->plot(), 0, true, &iPathTurns)) {
					iValue += std::max(0, (GET_PLAYER(getOwnerINLINE()).AI_neededWorkers(pLoopCity->area()) - GET_PLAYER(getOwnerINLINE()).AI_totalAreaUnitAIs(pLoopCity->area(), UNITAI_WORKER)));
					iValue *= 1000;
					iValue /= 4 + iPathTurns;
					if (atPlot(pLoopCity->plot())) {
						iValue += 100;
					} else {
						iValue += GC.getGame().getSorenRandNum(100, "AI settler sea ferry");
					}
					if (iValue > iBestValue) {
						iBestValue = iValue;
						pBestPlot = pLoopCity->plot();
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		if (atPlot(pBestPlot)) {
			unloadAll(); // XXX is this dangerous (not pushing a mission...) XXX air units?
			getGroup()->setActivityType(ACTIVITY_AWAKE); // K-Mod
			return true;
		} else {
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_FOUND, pBestPlot);
			return true;
		}
	}
	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_specialSeaTransportMissionary() {
	//PROFILE_FUNC();

	FAssert(getCargo() > 0);
	FAssert(getUnitAICargo(UNITAI_MISSIONARY) > 0);

	if (!getGroup()->canCargoAllMove()) {
		return false;
	}

	CvPlot* pPlot = plot();
	CvUnit* pMissionaryUnit = NULL;

	CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();
	while (pUnitNode != NULL) {
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pPlot->nextUnitNode(pUnitNode);

		if (pLoopUnit->getTransportUnit() == this) {
			if (pLoopUnit->AI_getUnitAIType() == UNITAI_MISSIONARY) {
				pMissionaryUnit = pLoopUnit;
				break;
			}
		}
	}

	if (pMissionaryUnit == NULL) {
		return false;
	}

	bool bExecutive = false;
	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestSpreadPlot = NULL;

	// XXX what about non-coastal cities?
	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (pLoopPlot->isCoastalLand()) {
			CvCity* pCity = pLoopPlot->getPlotCity();

			if (pCity != NULL) {
				int iValue = 0;
				int iCorpValue = 0;

				for (int iJ = 0; iJ < GC.getNumReligionInfos(); iJ++) {
					if (pMissionaryUnit->canSpread(pLoopPlot, ((ReligionTypes)iJ))) {
						if (GET_PLAYER(getOwnerINLINE()).getStateReligion() == ((ReligionTypes)iJ)) {
							iValue += 3;
						}

						if (GET_PLAYER(getOwnerINLINE()).hasHolyCity((ReligionTypes)iJ)) {
							iValue++;
						}
					}
				}

				for (int iJ = 0; iJ < GC.getNumCorporationInfos(); iJ++) {
					if (pMissionaryUnit->canSpreadCorporation(pLoopPlot, ((CorporationTypes)iJ))) {
						if (GET_PLAYER(getOwnerINLINE()).hasHeadquarters((CorporationTypes)iJ)) {
							iCorpValue += 3;
						}
					}
				}

				if (iValue > 0) {
					if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_SPREAD, getGroup()) == 0) {
							int iPathTurns;
							if (generatePath(pLoopPlot, 0, true, &iPathTurns)) {
								iValue *= pCity->getPopulation();

								if (pCity->getOwnerINLINE() == getOwnerINLINE()) {
									iValue *= 4;
								} else if (pCity->getTeam() == getTeam()) {
									iValue *= 3;
								}

								if (pCity->getReligionCount() == 0) {
									iValue *= 2;
								}

								iValue /= (pCity->getReligionCount() + 1);

								FAssert(iPathTurns > 0);

								if (iPathTurns == 1) {
									iValue *= 2;
								}

								iValue *= 1000;

								iValue /= (iPathTurns + 1);

								if (iValue > iBestValue) {
									iBestValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									pBestSpreadPlot = pLoopPlot;
									bExecutive = false;
								}
							}
						}
					}
				}

				if (iCorpValue > 0) {
					if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_SPREAD_CORPORATION, getGroup()) == 0) {
							int iPathTurns;
							if (generatePath(pLoopPlot, 0, true, &iPathTurns)) {
								iCorpValue *= pCity->getPopulation();

								FAssert(iPathTurns > 0);

								if (iPathTurns == 1) {
									iCorpValue *= 2;
								}

								iCorpValue *= 1000;

								iCorpValue /= (iPathTurns + 1);

								if (iCorpValue > iBestValue) {
									iBestValue = iCorpValue;
									pBestPlot = getPathEndTurnPlot();
									pBestSpreadPlot = pLoopPlot;
									bExecutive = true;
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestSpreadPlot != NULL)) {
		FAssert(!pBestPlot->isImpassable(getTeam()) || canMoveImpassable());

		if ((pBestPlot == pBestSpreadPlot) || (stepDistance(pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), pBestSpreadPlot->getX_INLINE(), pBestSpreadPlot->getY_INLINE()) == 1)) {
			if (atPlot(pBestSpreadPlot)) {
				unloadAll(); // XXX is this dangerous (not pushing a mission...) XXX air units?
				getGroup()->setActivityType(ACTIVITY_AWAKE); // K-Mod
				return true;
			} else {
				getGroup()->pushMission(MISSION_MOVE_TO, pBestSpreadPlot->getX_INLINE(), pBestSpreadPlot->getY_INLINE(), 0, false, false, bExecutive ? MISSIONAI_SPREAD_CORPORATION : MISSIONAI_SPREAD, pBestSpreadPlot);
				return true;
			}
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, bExecutive ? MISSIONAI_SPREAD_CORPORATION : MISSIONAI_SPREAD, pBestSpreadPlot);
			return true;
		}
	}

	return false;
}


// The body of this function has been completely deleted and rewriten for K-Mod
bool CvUnitAI::AI_specialSeaTransportSpy() {
	const CvTeamAI& kOurTeam = GET_TEAM(getTeam());
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	std::vector<int> base_value(MAX_CIV_PLAYERS);

	int iTotalPoints = kOurTeam.getTotalUnspentEspionage();

	int iBestValue = 0;
	PlayerTypes eBestTarget = NO_PLAYER;

	for (int i = 0; i < MAX_CIV_PLAYERS; i++) {
		const CvPlayerAI& kLoopPlayer = GET_PLAYER((PlayerTypes)i);

		if (kLoopPlayer.getTeam() == getTeam() || !kOurTeam.isHasMet(kLoopPlayer.getTeam())) {
			base_value[i] = 0;
		} else {
			int iValue = 1000 * kOurTeam.getEspionagePointsAgainstTeam(kLoopPlayer.getTeam()) / std::max(1, iTotalPoints);

			if (kOwner.AI_isMaliciousEspionageTarget((PlayerTypes)i))
				iValue = 3 * iValue / 2;

			if (kOurTeam.isAtWar(kLoopPlayer.getTeam()) && !isInvisible(kLoopPlayer.getTeam(), false)) {
				iValue /= 3; // it might be too risky.
			}

			if (kOurTeam.AI_hasCitiesInPrimaryArea(kLoopPlayer.getTeam()))
				iValue /= 6;

			iValue *= 100 - kOurTeam.AI_getAttitudeWeight(kLoopPlayer.getTeam()) / 2;

			base_value[i] = iValue; // of order 1000 * percentage of espionage. (~20000)

			if (iValue > iBestValue) {
				iBestValue = iValue;
				eBestTarget = (PlayerTypes)i;
			}
		}
	}

	if (eBestTarget == NO_PLAYER)
		return false;

	iBestValue = 0; // was best player value, now it is best plot value
	CvPlot* pTargetPlot = 0;
	CvPlot* pEndTurnPlot = 0;

	for (int i = 0; i < GC.getMapINLINE().numPlotsINLINE(); i++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(i);
		PlayerTypes ePlotOwner = pLoopPlot->getRevealedOwner(getTeam(), false);

		// only consider coast plots, owned by civ teams, with base value greater than the current best
		if (ePlotOwner == NO_PLAYER || ePlotOwner >= MAX_CIV_PLAYERS || !pLoopPlot->isCoastalLand() || iBestValue >= base_value[ePlotOwner] || pLoopPlot->area()->getCitiesPerPlayer(ePlotOwner) == 0)
			continue;

		//FAssert(pLoopPlot->isRevealed(getTeam(), false)); // otherwise, how do we have a revealed owner?
		// Actually, the owner gets revealed when any of the adjacent plots are visable - so this assert is not always true. I think it's fair to consider this plot anyway.

		int iValue = base_value[ePlotOwner];

		iValue *= 2;
		iValue /= 2 + kOwner.AI_totalAreaUnitAIs(pLoopPlot->area(), UNITAI_SPY);

		CvCity* pPlotCity = pLoopPlot->getPlotCity();
		if (pPlotCity && !kOurTeam.isAtWar(GET_PLAYER(ePlotOwner).getTeam())) // don't go directly to cities if we are at war.
		{
			iValue *= 100;
			iValue /= std::max(100, 3 * kOwner.getEspionageMissionCostModifier(NO_ESPIONAGEMISSION, ePlotOwner, pLoopPlot));
		} else {
			iValue /= 5;
		}

		if (GET_PLAYER(ePlotOwner).AI_isDoVictoryStrategyLevel4() && !GET_PLAYER(ePlotOwner).AI_isPrimaryArea(pLoopPlot->area())) {
			iValue /= 4;
		}

		FAssert(iValue <= base_value[ePlotOwner]);
		int iPathTurns;
		if (iValue > iBestValue && generatePath(pLoopPlot, 0, true, &iPathTurns)) {
			iValue *= 10;
			iValue /= 3 + iPathTurns;
			if (iValue > iBestValue) {
				iBestValue = iValue;
				pTargetPlot = pLoopPlot;
				pEndTurnPlot = getPathEndTurnPlot();
			}
		}
	}

	if (pTargetPlot) {
		if (atPlot(pTargetPlot)) {
			getGroup()->unloadAll();
			getGroup()->setActivityType(ACTIVITY_AWAKE);
			return true; // no actual mission pushed, but we need to rethink our next move.
		} else {
			if (canMoveInto(pEndTurnPlot) || getGroup()->canCargoAllMove()) // (without this, we could get into an infinite loop when the cargo isn't ready to move)
			{
				if (gUnitLogLevel > 2 && pTargetPlot->getOwnerINLINE() != NO_PLAYER && generatePath(pTargetPlot, 0, true, 0, 1)) {
					logBBAI("      %S lands sea-spy in %S territory. (%d percent of unspent points)", // apparently it's impossible to actually use a % sign in this Microsoft version of vsnprintf. madness
						kOurTeam.getName().GetCString(), GET_PLAYER(pTargetPlot->getOwnerINLINE()).getCivilizationDescription(0), kOurTeam.getEspionagePointsAgainstTeam(pTargetPlot->getTeam()) * 100 / iTotalPoints);
				}

				getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX_INLINE(), pEndTurnPlot->getY_INLINE(), 0, false, false, MISSIONAI_ATTACK_SPY, pTargetPlot);
				return true;
			} else {
				// need to wait for our cargo to be ready
				getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY, pTargetPlot);
				return true;
			}
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_carrierSeaTransport() {
	PROFILE_FUNC();

	int iMaxAirRange = 0;

	std::vector<CvUnit*> aCargoUnits;
	getCargoUnits(aCargoUnits);
	for (uint i = 0; i < aCargoUnits.size(); ++i) {
		iMaxAirRange = std::max(iMaxAirRange, aCargoUnits[i]->airRange());
	}

	if (iMaxAirRange == 0) {
		return false;
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestCarrierPlot = NULL;

	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot)) {
			if (pLoopPlot->isAdjacentToLand()) {
				if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
					int iValue = 0;

					for (int iDX = -(iMaxAirRange); iDX <= iMaxAirRange; iDX++) {
						for (int iDY = -(iMaxAirRange); iDY <= iMaxAirRange; iDY++) {
							CvPlot* pLoopPlotAir = plotXY(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), iDX, iDY);

							if (pLoopPlotAir != NULL) {
								if (plotDistance(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), pLoopPlotAir->getX_INLINE(), pLoopPlotAir->getY_INLINE()) <= iMaxAirRange) {
									if (!(pLoopPlotAir->isBarbarian())) {
										if (potentialWarAction(pLoopPlotAir)) {
											if (pLoopPlotAir->isCity()) {
												iValue += 3;

												// BBAI: Support invasions
												iValue += (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlotAir, MISSIONAI_ASSAULT, getGroup(), 2) * 6);
											}

											if (pLoopPlotAir->getImprovementType() != NO_IMPROVEMENT) {
												iValue += 2;
											}

											if (plotDistance(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), pLoopPlotAir->getX_INLINE(), pLoopPlotAir->getY_INLINE()) <= iMaxAirRange / 2) {
												// BBAI: Support/air defense for land troops
												iValue += pLoopPlotAir->plotCount(PUF_canDefend, -1, -1, getOwnerINLINE());
											}
										}
									}
								}
							}
						}
					}

					if (iValue > 0) {
						iValue *= 1000;

						for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++) {
							CvPlot* pDirectionPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), (DirectionTypes)iDirection);
							if (pDirectionPlot != NULL) {
								if (pDirectionPlot->isCity() && isEnemy(pDirectionPlot->getTeam(), pLoopPlot)) {
									iValue /= 2;
									break;
								}
							}
						}

						if (iValue > iBestValue) {
							bool bStealth = (getInvisibleType() != NO_INVISIBLE);
							if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_CARRIER, getGroup(), bStealth ? 5 : 3) <= (bStealth ? 0 : 3)) {
								int iPathTurns;
								if (generatePath(pLoopPlot, 0, true, &iPathTurns)) {
									iValue /= (iPathTurns + 1);

									if (iValue > iBestValue) {
										iBestValue = iValue;
										pBestPlot = getPathEndTurnPlot();
										pBestCarrierPlot = pLoopPlot;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestCarrierPlot != NULL)) {
		if (atPlot(pBestCarrierPlot)) {
			if (getGroup()->hasCargo()) {
				CvPlot* pPlot = plot();

				int iNumUnits = pPlot->getNumUnits();

				for (int i = 0; i < iNumUnits; ++i) {
					bool bDone = true;
					CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();
					while (pUnitNode != NULL) {
						CvUnit* pCargoUnit = ::getUnit(pUnitNode->m_data);
						pUnitNode = pPlot->nextUnitNode(pUnitNode);

						if (pCargoUnit->isCargo()) {
							FAssert(pCargoUnit->getTransportUnit() != NULL);
							if (pCargoUnit->getOwnerINLINE() == getOwnerINLINE() && (pCargoUnit->getTransportUnit()->getGroup() == getGroup()) && (pCargoUnit->getDomainType() == DOMAIN_AIR)) {
								if (pCargoUnit->canMove() && pCargoUnit->isGroupHead()) {
									// careful, this might kill the cargo group
									if (pCargoUnit->getGroup()->AI_update()) {
										bDone = false;
										break;
									}
								}
							}
						}
					}

					if (bDone) {
						break;
					}
				}
			}

			if (canPlunder(pBestCarrierPlot)) {
				getGroup()->pushMission(MISSION_PLUNDER, -1, -1, 0, false, false, MISSIONAI_CARRIER, pBestCarrierPlot);
			} else {
				getGroup()->pushMission(MISSION_SKIP);
			}
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_CARRIER, pBestCarrierPlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_connectPlot(CvPlot* pPlot, int iRange) {
	PROFILE_FUNC();

	FAssert(canBuildRoute());

	// BBAI efficiency: check area for land units before generating paths
	if ((getDomainType() == DOMAIN_LAND) && (pPlot->area() != area()) && !(getGroup()->canMoveAllTerrain())) {
		return false;
	}

	if (!(pPlot->isVisibleEnemyUnit(this))) {
		if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pPlot, MISSIONAI_BUILD, getGroup(), iRange) == 0) {
			if (generatePath(pPlot, MOVE_SAFE_TERRITORY, true)) {
				int iLoop;
				for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
					if (!(pPlot->isConnectedTo(pLoopCity))) {
						FAssertMsg(pPlot->getPlotCity() != pLoopCity, "pPlot->getPlotCity() is not expected to be equal with pLoopCity");

						if (plot()->getPlotGroup(getOwnerINLINE()) == pLoopCity->plot()->getPlotGroup(getOwnerINLINE())) {
							getGroup()->pushMission(MISSION_ROUTE_TO, pPlot->getX_INLINE(), pPlot->getY_INLINE(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pPlot);
							return true;
						}
					}
				}

				for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
					// BBAI efficiency: check same area
					if ((pLoopCity->area() != pPlot->area())) {
						continue;
					}

					if (!(pPlot->isConnectedTo(pLoopCity))) {
						FAssertMsg(pPlot->getPlotCity() != pLoopCity, "pPlot->getPlotCity() is not expected to be equal with pLoopCity");

						if (!(pLoopCity->plot()->isVisibleEnemyUnit(this))) {
							if (generatePath(pLoopCity->plot(), MOVE_SAFE_TERRITORY, true)) {
								if (atPlot(pPlot)) // need to test before moving...
								{
									getGroup()->pushMission(MISSION_ROUTE_TO, pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pPlot);
								} else {
									getGroup()->pushMission(MISSION_ROUTE_TO, pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pPlot);
									getGroup()->pushMission(MISSION_ROUTE_TO, pPlot->getX_INLINE(), pPlot->getY_INLINE(), MOVE_SAFE_TERRITORY, true, false, MISSIONAI_BUILD, pPlot); // K-Mod
								}

								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_improveCity(CvCity* pCity) {
	PROFILE_FUNC();

	CvPlot* pBestPlot;
	BuildTypes eBestBuild;
	MissionTypes eMission;
	if (AI_bestCityBuild(pCity, &pBestPlot, &eBestBuild, NULL, this)) {
		FAssertMsg(pBestPlot != NULL, "BestPlot is not assigned a valid value");
		FAssertMsg(eBestBuild != NO_BUILD, "BestBuild is not assigned a valid value");
		FAssertMsg(eBestBuild < GC.getNumBuildInfos(), "BestBuild is assigned a corrupt value");
		if ((plot()->getWorkingCity() != pCity) || (GC.getBuildInfo(eBestBuild).getRoute() != NO_ROUTE)) {
			eMission = MISSION_ROUTE_TO;
		} else {
			eMission = MISSION_MOVE_TO;
			if (pBestPlot && generatePath(pBestPlot) && getPathFinder().GetPathTurns() == 1 && getPathFinder().GetFinalMoves() == 0) {
				if (pBestPlot->getRouteType() != NO_ROUTE) {
					eMission = MISSION_ROUTE_TO;
				}
			} else if (plot()->getRouteType() == NO_ROUTE) {
				int iPlotMoveCost = 0;
				iPlotMoveCost = ((plot()->getFeatureType() == NO_FEATURE) ? GC.getTerrainInfo(plot()->getTerrainType()).getMovementCost() : GC.getFeatureInfo(plot()->getFeatureType()).getMovementCost());

				if (plot()->isHills()) {
					iPlotMoveCost += GC.getHILLS_EXTRA_MOVEMENT();
				}
				if (plot()->isPeak()) {
					iPlotMoveCost += GET_TEAM(getTeam()).isMoveFastPeaks() ? 1 : GC.getPEAK_EXTRA_MOVEMENT();
				}
				if (iPlotMoveCost > 1) {
					eMission = MISSION_ROUTE_TO;
				}
			}
		}

		eBestBuild = AI_betterPlotBuild(pBestPlot, eBestBuild);

		getGroup()->pushMission(eMission, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BUILD, pBestPlot);
		getGroup()->pushMission(MISSION_BUILD, eBestBuild, -1, 0, true, false, MISSIONAI_BUILD, pBestPlot); // K-Mod

		return true;
	}

	return false;
}

bool CvUnitAI::AI_improveLocalPlot(int iRange, CvCity* pIgnoreCity) {

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	BuildTypes eBestBuild = NO_BUILD;

	for (int iX = -iRange; iX <= iRange; iX++) {
		for (int iY = -iRange; iY <= iRange; iY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iX, iY);
			// K-Mod note: I've turn the all-encompassing if blocks into !if continues.
			if (pLoopPlot == NULL || !pLoopPlot->isCityRadius())
				continue;

			CvCity* pCity = pLoopPlot->getWorkingCity();
			if (pCity == NULL || pCity->getOwnerINLINE() != getOwnerINLINE())
				continue;

			if (pIgnoreCity != NULL && pCity == pIgnoreCity)
				continue;

			if (!AI_plotValid(pLoopPlot))
				continue;

			int iIndex = pCity->getCityPlotIndex(pLoopPlot);
			if (iIndex == CITY_HOME_PLOT || pCity->AI_getBestBuild(iIndex) == NO_BUILD)
				continue;

			if (pIgnoreCity != NULL && pCity->AI_getWorkersHave() - (plot()->getWorkingCity() == pCity ? 1 : 0) >= (1 + pCity->AI_getWorkersNeeded() * 2) / 3)
				continue;

			if (!canBuild(pLoopPlot, pCity->AI_getBestBuild(iIndex)))
				continue;

			if (GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_SAFE_AUTOMATION)) {
				if (pLoopPlot->getImprovementType() != NO_IMPROVEMENT && pLoopPlot->getImprovementType() != GC.getDefineINT("RUINS_IMPROVEMENT"))
					continue;
			}

			// K-Mod. I don't think it's a good idea to disallow improvement changes here. So I'm changing it to have a cutoff value instead.
			if (pCity->AI_getBestBuildValue(iIndex) <= 1)
				continue;
			//

			int iValue = pCity->AI_getBestBuildValue(iIndex);
			int iPathTurns;
			if (generatePath(pLoopPlot, 0, true, &iPathTurns)) {
				int iMaxWorkers = 1;
				if (plot() == pLoopPlot) {
					iValue *= 3;
					iValue /= 2;
				} else if (getPathFinder().GetFinalMoves() == 0) {
					iPathTurns++;
				} else if (iPathTurns <= 1) {
					iMaxWorkers = AI_calculatePlotWorkersNeeded(pLoopPlot, pCity->AI_getBestBuild(iIndex));
				}

				if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BUILD, getGroup()) < iMaxWorkers) {
					iValue *= 1000;
					iValue /= 1 + iPathTurns;

					if (iValue > iBestValue) {
						iBestValue = iValue;
						pBestPlot = pLoopPlot;
						eBestBuild = pCity->AI_getBestBuild(iIndex);
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssertMsg(eBestBuild != NO_BUILD, "BestBuild is not assigned a valid value");
		FAssertMsg(eBestBuild < GC.getNumBuildInfos(), "BestBuild is assigned a corrupt value");

		FAssert(pBestPlot->getWorkingCity() != NULL);
		if (NULL != pBestPlot->getWorkingCity()) {
			pBestPlot->getWorkingCity()->AI_changeWorkersHave(+1);

			if (plot()->getWorkingCity() != NULL) {
				plot()->getWorkingCity()->AI_changeWorkersHave(-1);
			}
		}
		MissionTypes eMission = MISSION_MOVE_TO;

		int iPathTurns;
		if (generatePath(pBestPlot, 0, true, &iPathTurns) && getPathFinder().GetPathTurns() == 1 && getPathFinder().GetFinalMoves() == 0) {
			if (pBestPlot->getRouteType() != NO_ROUTE) {
				eMission = MISSION_ROUTE_TO;
			}
		} else if (plot()->getRouteType() == NO_ROUTE) {
			int iPlotMoveCost = 0;
			iPlotMoveCost = ((plot()->getFeatureType() == NO_FEATURE) ? GC.getTerrainInfo(plot()->getTerrainType()).getMovementCost() : GC.getFeatureInfo(plot()->getFeatureType()).getMovementCost());

			if (plot()->isHills()) {
				iPlotMoveCost += GC.getHILLS_EXTRA_MOVEMENT();
			}
			if (plot()->isPeak()) {
				iPlotMoveCost += GET_TEAM(getTeam()).isMoveFastPeaks() ? 1 : GC.getPEAK_EXTRA_MOVEMENT();
			}
			if (iPlotMoveCost > 1) {
				eMission = MISSION_ROUTE_TO;
			}
		}

		eBestBuild = AI_betterPlotBuild(pBestPlot, eBestBuild);

		getGroup()->pushMission(eMission, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BUILD, pBestPlot);
		getGroup()->pushMission(MISSION_BUILD, eBestBuild, -1, 0, true, false, MISSIONAI_BUILD, pBestPlot); // K-Mod
		return true;
	}

	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_nextCityToImprove(CvCity* pCity) {
	PROFILE_FUNC();

	int iBestValue = 0;
	BuildTypes eBestBuild = NO_BUILD;
	CvPlot* pBestPlot = NULL;

	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if (pLoopCity != pCity) {
			// BBAI efficiency: check area for land units before path generation
			if ((getDomainType() == DOMAIN_LAND) && (pLoopCity->area() != area()) && !(getGroup()->canMoveAllTerrain())) {
				continue;
			}

			int iWorkersNeeded = pLoopCity->AI_getWorkersNeeded();
			int iWorkersHave = pLoopCity->AI_getWorkersHave();

			int iValue = std::max(0, iWorkersNeeded - iWorkersHave) * 100;
			iValue += iWorkersNeeded * 10;
			iValue *= (iWorkersNeeded + 1);
			iValue /= (iWorkersHave + 1);

			if (iValue > 0) {
				CvPlot* pPlot = NULL;
				BuildTypes eBuild = NO_BUILD;
				if (AI_bestCityBuild(pLoopCity, &pPlot, &eBuild, NULL, this)) {
					FAssert(pPlot != NULL);
					FAssert(eBuild != NO_BUILD);

					if (AI_plotValid(pPlot)) {
						iValue *= 1000;

						if (pLoopCity->isCapital()) {
							iValue *= 2;
						}

						if (iValue > iBestValue) {
							int iPathTurns;
							if (generatePath(pPlot, 0, true, &iPathTurns)) {
								iValue /= (iPathTurns + 1);

								if (iValue > iBestValue) {
									iBestValue = iValue;
									eBestBuild = eBuild;
									pBestPlot = pPlot;
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssertMsg(eBestBuild != NO_BUILD, "BestBuild is not assigned a valid value");
		FAssertMsg(eBestBuild < GC.getNumBuildInfos(), "BestBuild is assigned a corrupt value");
		if (plot()->getWorkingCity() != NULL) {
			plot()->getWorkingCity()->AI_changeWorkersHave(-1);
		}

		FAssert(pBestPlot->getWorkingCity() != NULL || GC.getBuildInfo(eBestBuild).getImprovement() == NO_IMPROVEMENT);
		if (NULL != pBestPlot->getWorkingCity()) {
			pBestPlot->getWorkingCity()->AI_changeWorkersHave(+1);
		}

		eBestBuild = AI_betterPlotBuild(pBestPlot, eBestBuild);

		getGroup()->pushMission(MISSION_ROUTE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BUILD, pBestPlot);
		getGroup()->pushMission(MISSION_BUILD, eBestBuild, -1, 0, true, false, MISSIONAI_BUILD, pBestPlot); // K-Mod
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_nextCityToImproveAirlift() {
	PROFILE_FUNC();

	if (getGroup()->getNumUnits() > 1) {
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();
	if (pCity == NULL) {
		return false;
	}

	if (pCity->getMaxAirlift() == 0) {
		return false;
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if (pLoopCity != pCity) {
			if (canAirliftAt(pCity->plot(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE())) {
				int iValue = pLoopCity->AI_totalBestBuildValue(pLoopCity->area());

				if (iValue > iBestValue) {
					iBestValue = iValue;
					pBestPlot = pLoopCity->plot();
					FAssert(pLoopCity != pCity);
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		getGroup()->pushMission(MISSION_AIRLIFT, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_irrigateTerritory() {
	PROFILE_FUNC();

	int iBestValue = 0;
	BuildTypes eBestBuild = NO_BUILD;
	CvPlot* pBestPlot = NULL;

	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot)) {
			if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE()) // XXX team???
			{
				if (pLoopPlot->getWorkingCity() == NULL) {
					ImprovementTypes eImprovement = pLoopPlot->getImprovementType();
					if ((eImprovement == NO_IMPROVEMENT) || !(GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_SAFE_AUTOMATION) && !(eImprovement == (GC.getDefineINT("RUINS_IMPROVEMENT"))))) {
						if ((eImprovement == NO_IMPROVEMENT) || !(GC.getImprovementInfo(eImprovement).isCarriesIrrigation())) {
							BonusTypes eNonObsoleteBonus = pLoopPlot->getNonObsoleteBonusType(getTeam());
							if (eImprovement == NO_IMPROVEMENT || eNonObsoleteBonus == NO_BONUS || !GET_PLAYER(getOwnerINLINE()).doesImprovementConnectBonus(eImprovement, eNonObsoleteBonus)) {
								if (pLoopPlot->isIrrigationAvailable(true)) {
									int iBestTempBuildValue = MAX_INT;
									BuildTypes eBestTempBuild = NO_BUILD;

									for (int iJ = 0; iJ < GC.getNumBuildInfos(); iJ++) {
										BuildTypes eBuild = ((BuildTypes)iJ);
										FAssertMsg(eBuild < GC.getNumBuildInfos(), "Invalid Build");

										if (GC.getBuildInfo(eBuild).getImprovement() != NO_IMPROVEMENT) {
											if (GC.getImprovementInfo((ImprovementTypes)(GC.getBuildInfo(eBuild).getImprovement())).isCarriesIrrigation()) {
												if (canBuild(pLoopPlot, eBuild)) {
													int iValue = 10000;

													iValue /= (GC.getBuildInfo(eBuild).getTime() + 1);

													// XXX feature production???

													if (iValue < iBestTempBuildValue) {
														iBestTempBuildValue = iValue;
														eBestTempBuild = eBuild;
													}
												}
											}
										}
									}

									if (eBestTempBuild != NO_BUILD) {
										bool bValid = true;

										if (pLoopPlot->getFeatureType() != NO_FEATURE) {
											if (GC.getBuildInfo(eBestTempBuild).isFeatureRemove(pLoopPlot->getFeatureType())) {
												const CvFeatureInfo& kFeatureInfo = GC.getFeatureInfo(pLoopPlot->getFeatureType());
												if ((GC.getGame().getGwEventTally() >= 0 && kFeatureInfo.getWarmingDefense() > 0) ||
													(GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_LEAVE_FORESTS) && kFeatureInfo.getYieldChange(YIELD_PRODUCTION) > 0)) {
													bValid = false;
												}
											}
										}

										if (bValid) {
											if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
												if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BUILD, getGroup(), 1) == 0) {
													int iPathTurns;
													if (generatePath(pLoopPlot, 0, true, &iPathTurns)) // XXX should this actually be at the top of the loop? (with saved paths and all...)
													{
														int iValue = 10000;

														iValue /= (iPathTurns + 1);

														if (iValue > iBestValue) {
															iBestValue = iValue;
															eBestBuild = eBestTempBuild;
															pBestPlot = pLoopPlot;
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssertMsg(eBestBuild != NO_BUILD, "BestBuild is not assigned a valid value");
		FAssertMsg(eBestBuild < GC.getNumBuildInfos(), "BestBuild is assigned a corrupt value");

		getGroup()->pushMission(MISSION_ROUTE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BUILD, pBestPlot);
		getGroup()->pushMission(MISSION_BUILD, eBestBuild, -1, 0, true, false, MISSIONAI_BUILD, pBestPlot); // K-Mod

		return true;
	}

	return false;
}

bool CvUnitAI::AI_fortTerritory(bool bCanal, bool bAirbase) {
	PROFILE_FUNC();

	// K-Mod. This function currently only handles canals and airbases. So if we want neither, just abort.
	if (!bCanal && !bAirbase)
		return false;

	int iBestValue = 0;
	BuildTypes eBestBuild = NO_BUILD;
	CvPlot* pBestPlot = NULL;

	CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());
	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot)) {
			if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE()) // XXX team???
			{
				if (pLoopPlot->getImprovementType() == NO_IMPROVEMENT) {
					int iValue = 0;
					iValue += bCanal ? kOwner.AI_getPlotCanalValue(pLoopPlot) : 0;
					iValue += bAirbase ? kOwner.AI_getPlotAirbaseValue(pLoopPlot) : 0;

					if (iValue > 0) {
						int iBestTempBuildValue = MAX_INT;
						BuildTypes eBestTempBuild = NO_BUILD;

						// K-Mod note: the following code may choose the improvement poorly if there are multiple fort options to choose from.
						// I don't want to spend time fixing it now, because K-Mod only has one type of fort anyway.
						for (int iJ = 0; iJ < GC.getNumBuildInfos(); iJ++) {
							BuildTypes eBuild = ((BuildTypes)iJ);
							FAssertMsg(eBuild < GC.getNumBuildInfos(), "Invalid Build");

							if (GC.getBuildInfo(eBuild).getImprovement() != NO_IMPROVEMENT) {
								if (GC.getImprovementInfo((ImprovementTypes)(GC.getBuildInfo(eBuild).getImprovement())).isActsAsCity()) {
									if (GC.getImprovementInfo((ImprovementTypes)(GC.getBuildInfo(eBuild).getImprovement())).getDefenseModifier() > 0) {
										if (canBuild(pLoopPlot, eBuild)) {
											iValue = 10000;

											iValue /= (GC.getBuildInfo(eBuild).getTime() + 1);

											if (iValue < iBestTempBuildValue) {
												iBestTempBuildValue = iValue;
												eBestTempBuild = eBuild;
											}
										}
									}
								}
							}
						}

						if (eBestTempBuild != NO_BUILD) {
							if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
								bool bValid = true;

								if (GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_LEAVE_FORESTS)) {
									if (pLoopPlot->getFeatureType() != NO_FEATURE) {
										if (GC.getBuildInfo(eBestTempBuild).isFeatureRemove(pLoopPlot->getFeatureType())) {
											if (GC.getFeatureInfo(pLoopPlot->getFeatureType()).getYieldChange(YIELD_PRODUCTION) > 0) {
												bValid = false;
											}
										}
									}
								}

								if (bValid) {
									if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BUILD, getGroup(), 3) == 0) {
										int iPathTurns;
										if (generatePath(pLoopPlot, 0, true, &iPathTurns)) {
											iValue *= 1000;
											iValue /= (iPathTurns + 1);

											if (iValue > iBestValue) {
												iBestValue = iValue;
												eBestBuild = eBestTempBuild;
												pBestPlot = pLoopPlot;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssertMsg(eBestBuild != NO_BUILD, "BestBuild is not assigned a valid value");
		FAssertMsg(eBestBuild < GC.getNumBuildInfos(), "BestBuild is assigned a corrupt value");

		getGroup()->pushMission(MISSION_ROUTE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BUILD, pBestPlot);
		getGroup()->pushMission(MISSION_BUILD, eBestBuild, -1, 0, true, false, MISSIONAI_BUILD, pBestPlot); // K-Mod

		return true;
	}
	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_improveBonus(bool bSingleBuild) // K-Mod. (all that junk wasn't being used anyway.)
{
	PROFILE_FUNC();

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	bool bBestBuildIsRoute = false;

	int iBestValue = 0;
	int iBestResourceValue = 0;
	BuildTypes eBestBuild = NO_BUILD;
	CvPlot* pBestPlot = NULL;

	bool bCanRoute = canBuildRoute();

	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE() && AI_plotValid(pLoopPlot)) {
			bool bCanImprove = (pLoopPlot->area() == area());
			if (!bCanImprove) {
				if (DOMAIN_SEA == getDomainType() && pLoopPlot->isWater() && plot()->isAdjacentToArea(pLoopPlot->area())) {
					bCanImprove = true;
				}
			}

			if (bCanImprove) {
				BonusTypes eNonObsoleteBonus = pLoopPlot->getNonObsoleteBonusType(getTeam());

				if (eNonObsoleteBonus != NO_BONUS) {
					bool bIsConnected = pLoopPlot->isConnectedToCapital(getOwnerINLINE());
					if ((pLoopPlot->getWorkingCity() != NULL) || (bIsConnected || bCanRoute)) {
						// K-Mod. Simplier, and better.
						bool bDoImprove = true;
						ImprovementTypes eImprovement = pLoopPlot->getImprovementType();
						CvCity* pWorkingCity = pLoopPlot->getWorkingCity();
						BuildTypes eBestTempBuild = NO_BUILD;

						if (eImprovement != NO_IMPROVEMENT && ((kOwner.isOption(PLAYEROPTION_SAFE_AUTOMATION) && eImprovement != GC.getDefineINT("RUINS_IMPROVEMENT")) || kOwner.doesImprovementConnectBonus(eImprovement, eNonObsoleteBonus) || (bSingleBuild && kOwner.hasBonus(eNonObsoleteBonus)))) {
							bDoImprove = false;
						} else if (pWorkingCity) {
							// Let "best build" handle improvement replacements near cities.
							BuildTypes eBuild = pWorkingCity->AI_getBestBuild(plotCityXY(pWorkingCity, pLoopPlot));
							if (eBuild != NO_BUILD && kOwner.doesImprovementConnectBonus((ImprovementTypes)GC.getBuildInfo(eBuild).getImprovement(), eNonObsoleteBonus) && canBuild(pLoopPlot, eBuild)) {
								bDoImprove = true;
								eBestTempBuild = eBuild;
							} else
								bDoImprove = false;
						}

						if (bDoImprove && eBestTempBuild == NO_BUILD) // K-Mod
						{
							int iBestTempBuildValue = MAX_INT; // K-Mod
							for (int iJ = 0; iJ < GC.getNumBuildInfos(); iJ++) {
								BuildTypes eBuild = ((BuildTypes)iJ);

								if (kOwner.doesImprovementConnectBonus((ImprovementTypes)GC.getBuildInfo(eBuild).getImprovement(), eNonObsoleteBonus)) // K-Mod
								{
									if (canBuild(pLoopPlot, eBuild)) {
										if ((pLoopPlot->getFeatureType() == NO_FEATURE) || !GC.getBuildInfo(eBuild).isFeatureRemove(pLoopPlot->getFeatureType()) || !kOwner.isOption(PLAYEROPTION_LEAVE_FORESTS)) {
											int iValue = 10000;

											iValue /= (GC.getBuildInfo(eBuild).getTime() + 1);

											// XXX feature production???

											if (iValue < iBestTempBuildValue) {
												iBestTempBuildValue = iValue;
												eBestTempBuild = eBuild;
											}
										}
									}
								}
							}
						}
						if (eBestTempBuild == NO_BUILD) {
							bDoImprove = false;
						}

						if ((eBestTempBuild != NO_BUILD) || (bCanRoute && !bIsConnected)) {
							int iPathTurns;
							if (generatePath(pLoopPlot, 0, true, &iPathTurns)) {
								int iValue = kOwner.AI_bonusVal(eNonObsoleteBonus, 1);

								if (bDoImprove) {
									eImprovement = (ImprovementTypes)GC.getBuildInfo(eBestTempBuild).getImprovement();
									FAssert(eImprovement != NO_IMPROVEMENT);
									iValue += 5 * pLoopPlot->calculateImprovementYieldChange(eImprovement, YIELD_FOOD, getOwnerINLINE(), false);
									iValue += 5 * pLoopPlot->calculateNatureYield(YIELD_FOOD, getTeam(), (pLoopPlot->getFeatureType() == NO_FEATURE) ? true : GC.getBuildInfo(eBestTempBuild).isFeatureRemove(pLoopPlot->getFeatureType()));
								}

								iValue += std::max(0, 100 * GC.getBonusInfo(eNonObsoleteBonus).getAIObjective());

								if (kOwner.getNumTradeableBonuses(eNonObsoleteBonus) == 0) {
									iValue *= 2;
								}

								int iMaxWorkers = 1;
								if ((eBestTempBuild != NO_BUILD) && (!GC.getBuildInfo(eBestTempBuild).isKill())) {
									//allow teaming.
									iMaxWorkers = AI_calculatePlotWorkersNeeded(pLoopPlot, eBestTempBuild);
									if (getPathFinder().GetFinalMoves() == 0) {
										iMaxWorkers = std::min((iMaxWorkers + 1) / 2, 1 + kOwner.AI_baseBonusVal(eNonObsoleteBonus) / 20);
									}
								}

								if (kOwner.AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BUILD, getGroup()) < iMaxWorkers && (!bDoImprove || (pLoopPlot->getBuildTurnsLeft(eBestTempBuild, 0, 0) > (iPathTurns * 2 - 1)))) {
									if (bDoImprove) {
										iValue *= 1000;

										if (iPathTurns == 1 && getPathFinder().GetFinalMoves() != 0) {
											iValue *= 2;
										}

										iValue /= (iPathTurns + 1);

										if (pLoopPlot->isCityRadius()) {
											iValue *= 2;
										}

										if (iValue > iBestValue) {
											iBestValue = iValue;
											eBestBuild = eBestTempBuild;
											pBestPlot = pLoopPlot;
											bBestBuildIsRoute = false;
											iBestResourceValue = iValue;
										}
									} else if (!bSingleBuild || !kOwner.hasBonus(eNonObsoleteBonus)) {
										FAssert(bCanRoute && !bIsConnected);
										eImprovement = pLoopPlot->getImprovementType();
										if (kOwner.doesImprovementConnectBonus(eImprovement, eNonObsoleteBonus)) {
											iValue *= 1000;
											iValue /= (iPathTurns + 1);

											if (iValue > iBestValue) {
												iBestValue = iValue;
												eBestBuild = NO_BUILD;
												pBestPlot = pLoopPlot;
												bBestBuildIsRoute = true;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		if (eBestBuild != NO_BUILD) {
			FAssertMsg(!bBestBuildIsRoute, "BestBuild should not be a route");
			FAssertMsg(eBestBuild < GC.getNumBuildInfos(), "BestBuild is assigned a corrupt value");

			MissionTypes eBestMission = MISSION_MOVE_TO;
			if (!this->isSlave()) { // don't waste a slave on building a route
				if ((pBestPlot->getWorkingCity() == NULL) || !pBestPlot->getWorkingCity()->isConnectedToCapital()) {
					eBestMission = MISSION_ROUTE_TO;
				} else {
					int iDistance = stepDistance(getX_INLINE(), getY_INLINE(), pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
					int iPathTurns;
					if (generatePath(pBestPlot, 0, false, &iPathTurns)) {
						if (iPathTurns >= iDistance) {
							eBestMission = MISSION_ROUTE_TO;
						}
					}
				}

				eBestBuild = AI_betterPlotBuild(pBestPlot, eBestBuild);
			}

			getGroup()->pushMission(eBestMission, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BUILD, pBestPlot);
			getGroup()->pushMission(MISSION_BUILD, eBestBuild, -1, 0, true, false, MISSIONAI_BUILD, pBestPlot); // K-Mod

			return true;
		} else if (bBestBuildIsRoute) {
			if (AI_connectPlot(pBestPlot)) {
				return true;
			}
		} else {
			FAssert(false);
		}
	}

	return false;
}

//returns true if a mission is pushed
//if eBuild is NO_BUILD, assumes a route is desired.
bool CvUnitAI::AI_improvePlot(CvPlot* pPlot, BuildTypes eBuild) {
	FAssert(pPlot != NULL);

	if (eBuild != NO_BUILD) {
		FAssertMsg(eBuild < GC.getNumBuildInfos(), "BestBuild is assigned a corrupt value");

		eBuild = AI_betterPlotBuild(pPlot, eBuild);
		if (!atPlot(pPlot)) {
			getGroup()->pushMission(MISSION_MOVE_TO, pPlot->getX_INLINE(), pPlot->getY_INLINE(), 0, false, false, MISSIONAI_BUILD, pPlot);
		}
		getGroup()->pushMission(MISSION_BUILD, eBuild, -1, 0, true, false, MISSIONAI_BUILD, pPlot); // K-Mod

		return true;
	} else if (canBuildRoute()) {
		if (AI_connectPlot(pPlot)) {
			return true;
		}
	}

	return false;

}

BuildTypes CvUnitAI::AI_betterPlotBuild(CvPlot* pPlot, BuildTypes eBuild) {
	FAssert(pPlot != NULL);
	FAssert(eBuild != NO_BUILD);
	bool bBuildRoute = false;
	bool bClearFeature = false;

	FeatureTypes eFeature = pPlot->getFeatureType();

	CvBuildInfo& kOriginalBuildInfo = GC.getBuildInfo(eBuild);

	if (kOriginalBuildInfo.getRoute() != NO_ROUTE) {
		return eBuild;
	}

	int iWorkersNeeded = AI_calculatePlotWorkersNeeded(pPlot, eBuild);

	if ((pPlot->getNonObsoleteBonusType(getTeam()) == NO_BONUS) && (pPlot->getWorkingCity() != NULL)) {
		iWorkersNeeded = std::max(1, std::min(iWorkersNeeded, pPlot->getWorkingCity()->AI_getWorkersHave()));
	}

	if (eFeature != NO_FEATURE) {
		CvFeatureInfo& kFeatureInfo = GC.getFeatureInfo(eFeature);
		if (kOriginalBuildInfo.isFeatureRemove(eFeature)) {
			if ((kOriginalBuildInfo.getImprovement() == NO_IMPROVEMENT) || (!pPlot->isBeingWorked() || (kFeatureInfo.getYieldChange(YIELD_FOOD) + kFeatureInfo.getYieldChange(YIELD_PRODUCTION)) <= 0)) {
				bClearFeature = true;
			}
		}

		if ((kFeatureInfo.getMovementCost() > 1) && (iWorkersNeeded > 1)) {
			bBuildRoute = true;
		}
	}

	if (pPlot->getNonObsoleteBonusType(getTeam()) != NO_BONUS) {
		bBuildRoute = true;
	} else if (pPlot->isHills()) {
		if ((GC.getHILLS_EXTRA_MOVEMENT() > 0) && (iWorkersNeeded > 1)) {
			bBuildRoute = true;
		}
	} else if (pPlot->isPeak()) {
		if ((GC.getPEAK_EXTRA_MOVEMENT() > 0) && (iWorkersNeeded > 1)) {
			bBuildRoute = true;
		}
	}

	if (pPlot->getRouteType() != NO_ROUTE) {
		bBuildRoute = false;
	}

	BuildTypes eBestBuild = NO_BUILD;
	int iBestValue = 0;
	for (int iBuild = 0; iBuild < GC.getNumBuildInfos(); iBuild++) {
		BuildTypes eBuild = ((BuildTypes)iBuild);
		CvBuildInfo& kBuildInfo = GC.getBuildInfo(eBuild);


		RouteTypes eRoute = (RouteTypes)kBuildInfo.getRoute();
		if ((bBuildRoute && eRoute != NO_ROUTE) || (bClearFeature && kBuildInfo.isFeatureRemove(eFeature))) {
			if (canBuild(pPlot, eBuild)) {
				int iValue = 10000;

				if (bBuildRoute && (eRoute != NO_ROUTE)) {
					iValue *= (1 + GC.getRouteInfo(eRoute).getValue());
					iValue /= 2;

					if (pPlot->getNonObsoleteBonusType(getTeam()) != NO_BONUS) {
						iValue *= 2;
					}

					if (pPlot->getWorkingCity() != NULL) {
						// Plot cannot be both hills and peak so only one of these will trigger
						iValue *= 2 + iWorkersNeeded + ((pPlot->isHills() && (iWorkersNeeded > 1)) ? 2 * GC.getHILLS_EXTRA_MOVEMENT() : 0);
						iValue *= 2 + iWorkersNeeded + ((pPlot->isPeak() && (iWorkersNeeded > 1)) ? 2 * GC.getPEAK_EXTRA_MOVEMENT() : 0);
						iValue /= 3;
					}
					ImprovementTypes eImprovement = (ImprovementTypes)kOriginalBuildInfo.getImprovement();
					if (eImprovement != NO_IMPROVEMENT) {
						int iRouteMultiplier = ((GC.getImprovementInfo(eImprovement).getRouteYieldChanges(eRoute, YIELD_FOOD)) * 100);
						iRouteMultiplier += ((GC.getImprovementInfo(eImprovement).getRouteYieldChanges(eRoute, YIELD_PRODUCTION)) * 100);
						iRouteMultiplier += ((GC.getImprovementInfo(eImprovement).getRouteYieldChanges(eRoute, YIELD_COMMERCE)) * 60);
						iValue *= 100 + iRouteMultiplier;
						iValue /= 100;
					}

					int iPlotGroupId = -1;
					for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++) {
						CvPlot* pLoopPlot = plotDirection(pPlot->getX_INLINE(), pPlot->getY_INLINE(), (DirectionTypes)iDirection);
						if (pLoopPlot != NULL) {
							if (pPlot->isRiver() || (pLoopPlot->getRouteType() != NO_ROUTE)) {
								CvPlotGroup* pLoopGroup = pLoopPlot->getPlotGroup(getOwnerINLINE());
								if (pLoopGroup != NULL) {
									if (pLoopGroup->getID() != -1) {
										if (pLoopGroup->getID() != iPlotGroupId) {
											//This plot bridges plot groups, so route it.
											iValue *= 4;
											break;
										} else {
											iPlotGroupId = pLoopGroup->getID();
										}
									}
								}
							}
						}
					}
				}

				iValue /= (kBuildInfo.getTime() + 1);

				if (iValue > iBestValue) {
					iBestValue = iValue;
					eBestBuild = eBuild;
				}
			}
		}
	}

	if (eBestBuild == NO_BUILD) {
		return eBuild;
	}
	return eBestBuild;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_connectBonus(bool bTestTrade) {
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	BonusTypes eNonObsoleteBonus;
	int iI;

	// XXX how do we make sure that we can build roads???

	for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot)) {
			if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE()) // XXX team???
			{
				eNonObsoleteBonus = pLoopPlot->getNonObsoleteBonusType(getTeam());

				if (eNonObsoleteBonus != NO_BONUS) {
					if (!(pLoopPlot->isConnectedToCapital())) {
						if (!bTestTrade || GET_PLAYER(getOwnerINLINE()).doesImprovementConnectBonus(pLoopPlot->getImprovementType(), eNonObsoleteBonus)) {
							if (AI_connectPlot(pLoopPlot)) {
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_connectCity() {
	PROFILE_FUNC();

	// XXX how do we make sure that we can build roads???

	CvCity* pLoopCity = plot()->getWorkingCity();
	if (pLoopCity != NULL) {
		if (AI_plotValid(pLoopCity->plot())) {
			if (!(pLoopCity->isConnectedToCapital())) {
				if (AI_connectPlot(pLoopCity->plot(), 1)) {
					return true;
				}
			}
		}
	}

	int iLoop;
	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if (AI_plotValid(pLoopCity->plot())) {
			if (!(pLoopCity->isConnectedToCapital())) {
				if (AI_connectPlot(pLoopCity->plot(), 1)) {
					return true;
				}
			}
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_routeCity() {
	PROFILE_FUNC();

	FAssert(canBuildRoute());

	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if (AI_plotValid(pLoopCity->plot())) {
			// BBAI efficiency: check area for land units before generating path
			if ((getDomainType() == DOMAIN_LAND) && (pLoopCity->area() != area()) && !(getGroup()->canMoveAllTerrain())) {
				continue;
			}

			CvCity* pRouteToCity = pLoopCity->AI_getRouteToCity();
			if (pRouteToCity != NULL) {
				if (!(pLoopCity->plot()->isVisibleEnemyUnit(this))) {
					if (!(pRouteToCity->plot()->isVisibleEnemyUnit(this))) {
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pRouteToCity->plot(), MISSIONAI_BUILD, getGroup()) == 0) {
							if (generatePath(pLoopCity->plot(), MOVE_SAFE_TERRITORY, true)) {
								if (generatePath(pRouteToCity->plot(), MOVE_SAFE_TERRITORY, true)) {
									getGroup()->pushMission(MISSION_ROUTE_TO, pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pRouteToCity->plot());
									getGroup()->pushMission(MISSION_ROUTE_TO, pRouteToCity->getX_INLINE(), pRouteToCity->getY_INLINE(), MOVE_SAFE_TERRITORY, true, false, MISSIONAI_BUILD, pRouteToCity->plot()); // K-Mod

									return true;
								}
							}
						}
					}
				}
			}
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_routeTerritory(bool bImprovementOnly) {
	PROFILE_FUNC();

	FAssert(canBuildRoute());

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot)) {
			if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE()) // XXX team???
			{
				RouteTypes eBestRoute = GET_PLAYER(getOwnerINLINE()).getBestRoute(pLoopPlot);
				if (eBestRoute != NO_ROUTE) {
					if (eBestRoute != pLoopPlot->getRouteType()) {
						bool bValid = false;
						if (bImprovementOnly) {
							bValid = false;

							ImprovementTypes eImprovement = pLoopPlot->getImprovementType();
							if (eImprovement != NO_IMPROVEMENT) {
								for (int iJ = 0; iJ < NUM_YIELD_TYPES; iJ++) {
									if (GC.getImprovementInfo(eImprovement).getRouteYieldChanges(eBestRoute, iJ) > 0) {
										bValid = true;
										break;
									}
								}
							}
						} else {
							bValid = true;
						}

						if (bValid) {
							if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
								if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BUILD, getGroup(), 1) == 0) {
									int iPathTurns;
									if (generatePath(pLoopPlot, MOVE_SAFE_TERRITORY, true, &iPathTurns)) {
										int iValue = 10000;

										iValue /= (iPathTurns + 1);

										if (iValue > iBestValue) {
											iBestValue = iValue;
											pBestPlot = pLoopPlot;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		getGroup()->pushMission(MISSION_ROUTE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pBestPlot);
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_travelToUpgradeCity() {
	PROFILE_FUNC();

	// is there a city which can upgrade us?
	CvCity* pUpgradeCity = getUpgradeCity(/*bSearch*/ true);
	if (pUpgradeCity != NULL) {
		// cache some stuff
		CvPlot* pPlot = plot();
		bool bSeaUnit = (getDomainType() == DOMAIN_SEA);
		bool bCanAirliftUnit = (getDomainType() == DOMAIN_LAND);
		bool bShouldSkipToUpgrade = (getDomainType() != DOMAIN_AIR);

		// if we at the upgrade city, stop, wait to get upgraded
		if (pUpgradeCity->plot() == pPlot) {
			if (!bShouldSkipToUpgrade) {
				return false;
			}

			getGroup()->pushMission(MISSION_SKIP);
			return true;
		}

		if (DOMAIN_AIR == getDomainType()) {
			FAssert(!atPlot(pUpgradeCity->plot()));
			getGroup()->pushMission(MISSION_MOVE_TO, pUpgradeCity->getX_INLINE(), pUpgradeCity->getY_INLINE());
			return true;
		}

		// find the closest city
		CvCity* pClosestCity = pPlot->getPlotCity();
		bool bAtClosestCity = (pClosestCity != NULL);
		if (pClosestCity == NULL) {
			pClosestCity = pPlot->getWorkingCity();
		}
		if (pClosestCity == NULL) {
			pClosestCity = GC.getMapINLINE().findCity(getX_INLINE(), getY_INLINE(), NO_PLAYER, getTeam(), true, bSeaUnit);
		}

		// can we path to the upgrade city?
		int iUpgradeCityPathTurns;
		CvPlot* pThisTurnPlot = NULL;
		bool bCanPathToUpgradeCity = generatePath(pUpgradeCity->plot(), 0, true, &iUpgradeCityPathTurns);
		if (bCanPathToUpgradeCity) {
			pThisTurnPlot = getPathEndTurnPlot();
		}

		// if we close to upgrade city, head there 
		if (NULL != pThisTurnPlot && NULL != pClosestCity && (pClosestCity == pUpgradeCity || iUpgradeCityPathTurns < 4)) {
			FAssert(!atPlot(pThisTurnPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pThisTurnPlot->getX_INLINE(), pThisTurnPlot->getY_INLINE());
			return true;
		}

		// check for better airlift choice
		if (bCanAirliftUnit && NULL != pClosestCity && pClosestCity->getMaxAirlift() > 0) {
			// if we at the closest city, then do the airlift, or wait
			if (bAtClosestCity) {
				// can we do the airlift this turn?
				if (canAirliftAt(pClosestCity->plot(), pUpgradeCity->getX_INLINE(), pUpgradeCity->getY_INLINE())) {
					getGroup()->pushMission(MISSION_AIRLIFT, pUpgradeCity->getX_INLINE(), pUpgradeCity->getY_INLINE());
					return true;
				}
				// wait to do it next turn
				else {
					getGroup()->pushMission(MISSION_SKIP);
					return true;
				}
			}

			int iClosestCityPathTurns;
			CvPlot* pThisTurnPlotForAirlift = NULL;
			bool bCanPathToClosestCity = generatePath(pClosestCity->plot(), 0, true, &iClosestCityPathTurns);
			if (bCanPathToClosestCity) {
				pThisTurnPlotForAirlift = getPathEndTurnPlot();
			}

			// is the closest city closer pathing? If so, move toward closest city
			if (NULL != pThisTurnPlotForAirlift && (!bCanPathToUpgradeCity || iClosestCityPathTurns < iUpgradeCityPathTurns)) {
				FAssert(!atPlot(pThisTurnPlotForAirlift));
				getGroup()->pushMission(MISSION_MOVE_TO, pThisTurnPlotForAirlift->getX_INLINE(), pThisTurnPlotForAirlift->getY_INLINE(), 0, false, false, MISSIONAI_UPGRADE);
				return true;
			}
		}

		// did not have better airlift choice, go ahead and path to the upgrade city
		if (NULL != pThisTurnPlot) {
			FAssert(!atPlot(pThisTurnPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pThisTurnPlot->getX_INLINE(), pThisTurnPlot->getY_INLINE(), 0, false, false, MISSIONAI_UPGRADE);
			return true;
		}
	}

	return false;
}

// Returns true if a mission was pushed...
// K-Mod. The bAirlift parameter now means that airlift cities will be priotised, but other cities are still accepted.
bool CvUnitAI::AI_retreatToCity(bool bPrimary, bool bPrioritiseAirlift, int iMaxPath) {
	PROFILE_FUNC();

	//int iCurrentDanger = GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot());
	int iCurrentDanger = getGroup()->alwaysInvisible() ? 0 : GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot()); // K-Mod

	CvCity* pCity = plot()->getPlotCity();

	if (0 == iCurrentDanger) {
		if (pCity != NULL) {
			if (pCity->getOwnerINLINE() == getOwnerINLINE()) {
				if (!bPrimary || GET_PLAYER(getOwnerINLINE()).AI_isPrimaryArea(pCity->area())) {
					if (!bPrioritiseAirlift || (pCity->getMaxAirlift() > 0)) {
						//if (!(pCity->plot()->isVisibleEnemyUnit(this)))
						{
							getGroup()->pushMission(MISSION_SKIP);
							return true;
						}
					}
				}
			}
		}
	}

	//for (iPass = 0; iPass < 4; iPass++)
	// K-Mod. originally; pass 0 required the dest to have less plot danger unless the unit could fight; pass 1 was just an ordinary move; 
	// pass 2 was a 1 turn move with "ignore plot danger" and pass 3 was the full iMaxPath with ignore plot danger.
	// I've changed it so that if the unit can fight, the pass 0 just skipped (because it's the same as the pass 1)
	// and pass 2 is always skipped because it's a useless test.
	// -- and I've renumbered the passes.
	CvPlot* pBestPlot = NULL;
	int iShortestPath = MAX_INT;

	int iPass;
	for (iPass = (getGroup()->canDefend() ? 1 : 0); iPass < 3; iPass++) {
		int iLoop;
		bool bNeedsAirlift = false;
		for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
			if (!AI_plotValid(pLoopCity->plot()))
				continue;

			if (bPrimary && !GET_PLAYER(getOwnerINLINE()).AI_isPrimaryArea(pLoopCity->area()))
				continue;

			if (bNeedsAirlift && pLoopCity->getMaxAirlift() == 0)
				continue;

			int iPathTurns;
			if (generatePath(pLoopCity->plot(), (iPass >= 2 ? MOVE_IGNORE_DANGER : 0), true, &iPathTurns, iMaxPath)) { // was iPass >= 3
				// Water units can't defend a city
				// Any unthreatened city acceptable on 0th pass, solves problem where sea units
				// would oscillate in and out of threatened city because they had iCurrentDanger = 0
				// on turns outside city

				if (iPass > 0 || GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(pLoopCity->plot()) <= iCurrentDanger) {
					// If this is the first viable air-lift city, then reset iShortestPath.
					if (bPrioritiseAirlift && !bNeedsAirlift && pLoopCity->getMaxAirlift() > 0) {
						bNeedsAirlift = true;
						iShortestPath = MAX_INT;
					}

					if (iPathTurns < iShortestPath) {
						iShortestPath = iPathTurns;
						pBestPlot = getPathEndTurnPlot();
					}
				}
			}
		}

		if (pBestPlot != NULL) {
			break;
		}

		if (getGroup()->alwaysInvisible()) {
			break;
		}
	}

	if (pBestPlot != NULL) {
		if (atPlot(pBestPlot))
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_RETREAT);
		else
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), iPass >= 2 ? MOVE_IGNORE_DANGER : 0, false, false, MISSIONAI_RETREAT); // was iPass >= 3
		return true;
	}

	if (pCity != NULL) {
		if (pCity->getTeam() == getTeam()) {
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_RETREAT);
			return true;
		}
	}

	return false;
}

// K-Mod
// Decide whether or not this group is stranded.
// If they are stranded, try to walk towards the coast.
// If we're on the coast, wait to be rescued!
bool CvUnitAI::AI_handleStranded(int iFlags) {
	PROFILE_FUNC();

	if (isCargo()) {
		// This is possible, in some rare cases, but I'm currently trying to pin down precisely what those cases are.
		FAssertMsg(false, "AI_handleStranded: this unit is already cargo.");
		getGroup()->pushMission(MISSION_SKIP);
		return true;
	}

	if (isHuman())
		return false;

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	// return false if the group is not stranded.
	int iDummy;
	if (area()->getNumAIUnits(getOwnerINLINE(), UNITAI_SETTLE) > 0 && kOwner.AI_getNumAreaCitySites(getArea(), iDummy) > 0) {
		return false;
	}

	if (area()->getNumCities() > 0) {
		if (plot()->getTeam() == getTeam())
			return false;

		if (getGroup()->isHasPathToAreaPlayerCity(getOwnerINLINE(), iFlags)) {
			return false;
		}

		if ((canFight() || isSpy()) && getGroup()->isHasPathToAreaEnemyCity(true, iFlags)) {
			return false;
		}
	}

	// ok.. so the group is standed.
	// Try to get to the coast.
	if (!plot()->isCoastalLand()) {
		// maybe we were already on our way?
		CvPlot* pMissionPlot = 0;
		CvPlot* pEndTurnPlot = 0;
		if (getGroup()->AI_getMissionAIType() == MISSIONAI_STRANDED) {
			pMissionPlot = getGroup()->AI_getMissionAIPlot();
			if (pMissionPlot && pMissionPlot->isCoastalLand() && !pMissionPlot->isVisibleEnemyUnit(this) && generatePath(pMissionPlot, iFlags, true)) {
				// The current mission looks good enough. Don't bother searching for a better option.
				pEndTurnPlot = getPathEndTurnPlot();
			} else {
				// the current mission plot is not suitable. We'll have to search.
				pMissionPlot = 0;
			}
		}
		if (!pMissionPlot) {
			// look for the clostest coastal plot in this area
			int iShortestPath = MAX_INT;

			for (int i = 0; i < GC.getMapINLINE().numPlotsINLINE(); i++) {
				CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(i);

				if (pLoopPlot->getArea() == getArea() && pLoopPlot->isCoastalLand()) {
					// TODO: check that the water isn't blocked by ice.
					int iPathTurns;
					if (generatePath(pLoopPlot, iFlags, true, &iPathTurns, iShortestPath)) {
						FAssert(iPathTurns <= iShortestPath);
						iShortestPath = iPathTurns;
						pEndTurnPlot = getPathEndTurnPlot();
						pMissionPlot = pLoopPlot;
						if (iPathTurns <= 1)
							break;
					}
				}
			}
		}

		if (pMissionPlot) {
			FAssert(pEndTurnPlot);
			getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX_INLINE(), pEndTurnPlot->getY_INLINE(), iFlags, false, false, MISSIONAI_STRANDED, pMissionPlot);
			return true;
		}
	}

	// Hopefully we're on the coast. (but we might not be - if we couldn't find a path to the coast)
	// try to load into a passing boat
	// Calling AI_load will check all of our boats; so before we do that, I'm going to just see if there are any boats on adjacent plots.
	for (int i = NO_DIRECTION; i < NUM_DIRECTION_TYPES; i++) {
		CvPlot* pAdjacentPlot = plotDirection(getX_INLINE(), getY_INLINE(), (DirectionTypes)i);

		if (pAdjacentPlot && canLoad(pAdjacentPlot)) {
			// ok. there is something we can load into - but lets use the (slow) official function to actually issue the load command.
			if (AI_load(NO_UNITAI, NO_MISSIONAI, NO_UNITAI, -1, -1, -1, -1, iFlags, 1))
				return true;
			else // if that didn't do it, nothing will
				break;
		}
	}

	// raise the 'stranded' flag, and wait to be rescued.
	getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_STRANDED, plot());
	return true;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_pickup(UnitAITypes eUnitAI, bool bCountProduction, int iMaxPath) {
	PROFILE_FUNC();

	FAssert(cargoSpace() > 0);
	if (0 == cargoSpace()) {
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();
	if (pCity != NULL) {
		if (pCity->getOwnerINLINE() == getOwnerINLINE()) {
			if ((GC.getGameINLINE().getGameTurn() - pCity->getGameTurnAcquired()) > 15 || (GET_TEAM(getTeam()).countEnemyPowerByArea(pCity->area()) == 0)) {
				bool bConsider = false;

				if (AI_getUnitAIType() == UNITAI_ASSAULT_SEA) {
					// Improve island hopping
					if (pCity->area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE) {
						bConsider = false;
					} else if (eUnitAI == UNITAI_ATTACK_CITY && !(pCity->AI_isDanger())) {
						bConsider = (pCity->plot()->plotCount(PUF_canDefend, -1, -1, getOwnerINLINE(), NO_TEAM, PUF_isDomainType, DOMAIN_LAND) > pCity->AI_neededDefenders());
					} else {
						bConsider = pCity->AI_isDefended(-1);
					}
				} else if (AI_getUnitAIType() == UNITAI_SETTLER_SEA) {
					if (eUnitAI == UNITAI_CITY_DEFENSE) {
						bConsider = (pCity->plot()->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE(), NO_TEAM, PUF_isCityAIType) > 1);
					} else {
						bConsider = true;
					}
				} else {
					bConsider = true;
				}

				if (bConsider) {
					// only count units which are available to load 
					int iCount = pCity->plot()->plotCount(PUF_isAvailableUnitAITypeGroupie, eUnitAI, -1, getOwnerINLINE(), NO_TEAM, PUF_isFiniteRange);

					if (bCountProduction && (pCity->getProductionUnitAI() == eUnitAI)) {
						if (pCity->getProductionTurnsLeft() < 4) {
							CvUnitInfo& kUnitInfo = GC.getUnitInfo(pCity->getProductionUnit());
							if ((kUnitInfo.getDomainType() != DOMAIN_AIR) || kUnitInfo.getAirRange() > 0) {
								iCount++;
							}
						}
					}

					if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pCity->plot(), MISSIONAI_PICKUP, getGroup()) < ((iCount + (cargoSpace() - 1)) / cargoSpace())) {
						getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_PICKUP, pCity->plot());
						return true;
					}
				}
			}
		}
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestPickupPlot = NULL;

	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if (AI_plotValid(pLoopCity->plot())) {
			if ((GC.getGameINLINE().getGameTurn() - pLoopCity->getGameTurnAcquired()) > 15 || (GET_TEAM(getTeam()).countEnemyPowerByArea(pLoopCity->area()) == 0)) {
				bool bConsider = false;

				if (AI_getUnitAIType() == UNITAI_ASSAULT_SEA) {
					if (pLoopCity->area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE) {
						bConsider = false;
					} else if (eUnitAI == UNITAI_ATTACK_CITY && !(pLoopCity->AI_isDanger())) {
						// Improve island hopping
						bConsider = (pLoopCity->plot()->plotCount(PUF_canDefend, -1, -1, getOwnerINLINE(), NO_TEAM, PUF_isDomainType, DOMAIN_LAND) > pLoopCity->AI_neededDefenders());
					} else {
						bConsider = pLoopCity->AI_isDefended(-1);
					}
				} else if (AI_getUnitAIType() == UNITAI_SETTLER_SEA) {
					if (eUnitAI == UNITAI_CITY_DEFENSE) {
						bConsider = (pLoopCity->plot()->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE(), NO_TEAM, PUF_isCityAIType) > 1);
					} else {
						bConsider = true;
					}
				} else {
					bConsider = true;
				}

				if (bConsider) {
					// only count units which are available to load, have had a chance to move since being built
					int iCount = pLoopCity->plot()->plotCount(PUF_isAvailableUnitAITypeGroupie, eUnitAI, -1, getOwnerINLINE(), NO_TEAM, (bCountProduction ? PUF_isFiniteRange : PUF_isFiniteRangeAndNotJustProduced));

					int iValue = iCount * 10;

					if (bCountProduction && (pLoopCity->getProductionUnitAI() == eUnitAI)) {
						CvUnitInfo& kUnitInfo = GC.getUnitInfo(pLoopCity->getProductionUnit());
						if ((kUnitInfo.getDomainType() != DOMAIN_AIR) || kUnitInfo.getAirRange() > 0) {
							iValue++;
							iCount++;
						}
					}

					if (iValue > 0) {
						iValue += pLoopCity->getPopulation();

						if (!(pLoopCity->plot()->isVisibleEnemyUnit(this))) {
							if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_PICKUP, getGroup()) < ((iCount + (cargoSpace() - 1)) / cargoSpace())) {
								if (!(pLoopCity->AI_isDanger())) {
									int iPathTurns;
									if (!atPlot(pLoopCity->plot()) && generatePath(pLoopCity->plot(), MOVE_AVOID_ENEMY_WEIGHT_3, true, &iPathTurns)) {
										if (AI_getUnitAIType() == UNITAI_ASSAULT_SEA) {
											if (pLoopCity->area()->getAreaAIType(getTeam()) == AREAAI_ASSAULT) {
												iValue *= 4;
											} else if (pLoopCity->area()->getAreaAIType(getTeam()) == AREAAI_ASSAULT_ASSIST) {
												iValue *= 2;
											}
										}

										iValue *= 1000;

										iValue /= (iPathTurns + 3);

										if ((iValue > iBestValue) && (iPathTurns <= iMaxPath)) {
											iBestValue = iValue;
											// Do one turn along path, then reevaluate
											// Causes update of destination based on troop movement
											pBestPlot = getPathEndTurnPlot();
											pBestPickupPlot = pLoopCity->plot();

											if (pBestPlot == NULL || atPlot(pBestPlot)) {
												pBestPlot = pBestPickupPlot;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestPickupPlot != NULL)) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_AVOID_ENEMY_WEIGHT_3, false, false, MISSIONAI_PICKUP, pBestPickupPlot);
		return true;
	}

	return false;
}

// Returns true if a mission was pushed...
// (this function has been significantly edited for K-Mod)
bool CvUnitAI::AI_pickupStranded(UnitAITypes eUnitAI, int iMaxPath) {
	PROFILE_FUNC();

	FAssert(cargoSpace() > 0);
	if (0 == cargoSpace()) {
		return false;
	}

	if (isBarbarian()) {
		return false;
	}

	int iBestValue = 0;
	CvUnit* pBestUnit = 0;

	CvPlot* pEndTurnPlot = 0;
	CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());

	int iLoop;
	for (CvSelectionGroup* pLoopGroup = kPlayer.firstSelectionGroup(&iLoop); pLoopGroup != NULL; pLoopGroup = kPlayer.nextSelectionGroup(&iLoop)) {
		if (pLoopGroup->isStranded()) {
			CvUnit* pHeadUnit = pLoopGroup->getHeadUnit();
			if (pHeadUnit == NULL) {
				continue;
			}

			if ((eUnitAI != NO_UNITAI) && (pHeadUnit->AI_getUnitAIType() != eUnitAI)) {
				continue;
			}

			CvPlot* pPickupPlot = pLoopGroup->AI_getMissionAIPlot(); // K-Mod

			if (pPickupPlot == NULL) {
				continue;
			}

			if (!(pPickupPlot->isCoastalLand()) && !canMoveAllTerrain()) {
				continue;
			}

			// Units are stranded, attempt rescue
			int iCount = pLoopGroup->getNumUnits();

			if (1000 * iCount > iBestValue) {
				CvPlot* pTargetPlot = 0;
				int iPathTurns = MAX_INT;

				for (int iI = NO_DIRECTION; iI < NUM_DIRECTION_TYPES; iI++) {
					CvPlot* pAdjacentPlot = plotDirection(pPickupPlot->getX_INLINE(), pPickupPlot->getY_INLINE(), ((DirectionTypes)iI));

					if (pAdjacentPlot && (atPlot(pAdjacentPlot) || canMoveInto(pAdjacentPlot)) && generatePath(pAdjacentPlot, 0, true, &iPathTurns, iMaxPath)) {
						pTargetPlot = getPathEndTurnPlot();
						break;
					}
				}

				if (pTargetPlot) {
					FAssert(iMaxPath < 0 || iPathTurns <= iMaxPath);

					MissionAITypes eMissionAIType = MISSIONAI_PICKUP;
					iCount -= GET_PLAYER(getOwnerINLINE()).AI_unitTargetMissionAIs(pHeadUnit, &eMissionAIType, 1, getGroup(), iPathTurns) * cargoSpace(); // estimate

					int iValue = 1000 * iCount;

					iValue /= (iPathTurns + 1);

					if (iValue > iBestValue) {
						iBestValue = iValue;
						pBestUnit = pHeadUnit;
						pEndTurnPlot = pTargetPlot;
					}
				}
			}
		}
	}

	if (pBestUnit) {
		FAssert(pEndTurnPlot);
		if (atPlot(pEndTurnPlot)) {
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_PICKUP, 0, pBestUnit);
			return true;
		} else {
			FAssert(!atPlot(pBestUnit->plot()));
			getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX_INLINE(), pEndTurnPlot->getY_INLINE(), 0, false, false, MISSIONAI_PICKUP, 0, pBestUnit);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_airOffensiveCity() {
	//PROFILE_FUNC();

	FAssert(canAirAttack() || nukeRange() >= 0);

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		// Limit to cities and forts, true for any city but only this team's forts
		if (pLoopPlot->isCity(true, getTeam())) {
			if (pLoopPlot->getTeam() == getTeam() || (pLoopPlot->isOwned() && GET_TEAM(pLoopPlot->getTeam()).isVassal(getTeam()))) {
				if (atPlot(pLoopPlot) || canMoveInto(pLoopPlot)) {
					int iValue = AI_airOffenseBaseValue(pLoopPlot);

					if (iValue > iBestValue) {
						iBestValue = iValue;
						pBestPlot = pLoopPlot;
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		if (!atPlot(pBestPlot)) {
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_SAFE_TERRITORY);
			return true;
		}
	}

	return false;
}

// Function for ranking the value of a plot as a base for offensive air units
int CvUnitAI::AI_airOffenseBaseValue(CvPlot* pPlot) {
	if (pPlot == NULL || pPlot->area() == NULL) {
		return 0;
	}

	CvCity* pNearestEnemyCity = NULL;
	int iRange = 0;
	int iTempValue = 0;
	int iOurDefense = 0;
	int iOurOffense = 0;
	int iEnemyOffense = 0;
	int iEnemyDefense = 0;
	int iDistance = 0;

	CvPlot* pLoopPlot = NULL;
	CvCity* pCity = pPlot->getPlotCity();

	int iDefenders = pPlot->plotCount(PUF_canDefend, -1, -1, pPlot->getOwner());

	int iAttackAirCount = pPlot->plotCount(PUF_canAirAttack, -1, -1, NO_PLAYER, getTeam());
	iAttackAirCount += 2 * pPlot->plotCount(PUF_isUnitAIType, UNITAI_ICBM, -1, NO_PLAYER, getTeam());
	if (atPlot(pPlot)) {
		iAttackAirCount += canAirAttack() ? -1 : 0;
		iAttackAirCount += (nukeRange() >= 0) ? -2 : 0;
	}

	if (pPlot->isCoastalLand(GC.getMIN_WATER_SIZE_FOR_OCEAN())) {
		iDefenders -= 1;
	}

	if (pCity != NULL) {
		if (pCity->getDefenseModifier(true) < 40) {
			iDefenders -= 1;
		}

		if (pCity->getOccupationTimer() > 1) {
			iDefenders -= 1;
		}
	}

	// Consider threat from nearby enemy territory
	iRange = 1;
	int iBorderDanger = 0;

	for (int iDX = -(iRange); iDX <= iRange; iDX++) {
		for (int iDY = -(iRange); iDY <= iRange; iDY++) {
			pLoopPlot = plotXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (pLoopPlot->area() == pPlot->area() && pLoopPlot->isOwned()) {
					iDistance = stepDistance(pPlot->getX_INLINE(), pPlot->getY_INLINE(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE());
					if (pLoopPlot->getTeam() != getTeam() && !(GET_TEAM(pLoopPlot->getTeam()).isVassal(getTeam()))) {
						if (iDistance == 1) {
							iBorderDanger++;
						}

						if (atWar(pLoopPlot->getTeam(), getTeam())) {
							if (iDistance == 1) {
								iBorderDanger += 2;
							} else if ((iDistance == 2) && (pLoopPlot->isRoute())) {
								iBorderDanger += 2;
							}
						}
					}
				}
			}
		}
	}

	iDefenders -= std::min(2, (iBorderDanger + 1) / 3);

	// Don't put more attack air units on plot than effective land defenders ... too large a risk
	if (iAttackAirCount >= (iDefenders) || iDefenders <= 0) {
		return 0;
	}

	bool bAnyWar = (GET_TEAM(getTeam()).getAnyWarPlanCount(true) > 0);

	int iValue = 0;

	if (bAnyWar) {
		// Don't count assault assist, don't want to weight defending colonial coasts when homeland might be under attack
		bool bAssault = (pPlot->area()->getAreaAIType(getTeam()) == AREAAI_ASSAULT) || (pPlot->area()->getAreaAIType(getTeam()) == AREAAI_ASSAULT_MASSING);

		// Loop over operational range
		iRange = airRange();

		for (int iDX = -(iRange); iDX <= iRange; iDX++) {
			for (int iDY = -(iRange); iDY <= iRange; iDY++) {
				pLoopPlot = plotXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iDX, iDY);

				if ((pLoopPlot != NULL && pLoopPlot->area() != NULL)) {
					iDistance = plotDistance(pPlot->getX_INLINE(), pPlot->getY_INLINE(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE());

					if (iDistance <= iRange) {
						bool bDefensive = pLoopPlot->area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE;
						bool bOffensive = pLoopPlot->area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE;

						// Value system is based around 1 enemy military unit in our territory = 10 pts
						iTempValue = 0;

						if (pLoopPlot->isWater()) {
							if (pLoopPlot->isVisible(getTeam(), false) && !pLoopPlot->area()->isLake()) {
								// Defend ocean
								iTempValue = 1;

								if (pLoopPlot->isOwned()) {
									if (pLoopPlot->getTeam() == getTeam()) {
										iTempValue += 1;
									} else if ((pLoopPlot->getTeam() != getTeam()) && GET_TEAM(getTeam()).AI_getWarPlan(pLoopPlot->getTeam()) != NO_WARPLAN) {
										iTempValue += 1;
									}
								}

								// Low weight for visible ships cause they will probably move
								iTempValue += 2 * pLoopPlot->getNumVisibleEnemyDefenders(this);

								if (bAssault) {
									iTempValue *= 2;
								}
							}
						} else {
							if (!(pLoopPlot->isOwned())) {
								if (iDistance < (iRange - 2)) {
									// Target enemy troops in neutral territory
									iTempValue += 4 * pLoopPlot->getNumVisibleEnemyDefenders(this);
								}
							} else if (pLoopPlot->getTeam() == getTeam()) {
								iTempValue = 0;

								if (iDistance < (iRange - 2)) {
									// Target enemy troops in our territory
									iTempValue += 5 * pLoopPlot->getNumVisibleEnemyDefenders(this);

									if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE()) {
										if (GET_PLAYER(getOwnerINLINE()).AI_isPrimaryArea(pLoopPlot->area())) {
											iTempValue *= 3;
										} else {
											iTempValue *= 2;
										}
									}

									if (bDefensive) {
										iTempValue *= 2;
									}
								}
							} else if ((pLoopPlot->getTeam() != getTeam()) && GET_TEAM(getTeam()).AI_getWarPlan(pLoopPlot->getTeam()) != NO_WARPLAN) {
								// Attack opponents land territory
								iTempValue = 3;

								CvCity* pLoopCity = pLoopPlot->getPlotCity();

								if (pLoopCity != NULL) {
									// Target enemy cities
									iTempValue += (3 * pLoopCity->getPopulation() + 30);

									if (canAirBombAt(pPlot, pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE())) // K-Mod
									{
										iTempValue *= 2;
									}

									if (pLoopPlot->area()->getTargetCity(getOwnerINLINE()) == pLoopCity) {
										iTempValue *= 2;
									}

									if (pLoopCity->AI_isDanger()) {
										// Multiplier for nearby troops, ours, teammate's, and any other enemy of city
										iTempValue *= 3;
									}
								} else {
									if (iDistance < (iRange - 2)) {
										// Support our troops in enemy territory
										iTempValue += 15 * pLoopPlot->getNumDefenders(getOwnerINLINE());

										// Target enemy troops adjacent to our territory
										if (pLoopPlot->isAdjacentTeam(getTeam(), true)) {
											iTempValue += 7 * pLoopPlot->getNumVisibleEnemyDefenders(this);
										}
									}

									// Weight resources
									if (canAirBombAt(pPlot, pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE())) {
										if (pLoopPlot->getBonusType(getTeam()) != NO_BONUS) {
											iTempValue += 8 * std::max(2, GET_PLAYER(pLoopPlot->getOwnerINLINE()).AI_bonusVal(pLoopPlot->getBonusType(getTeam()), 0) / 10);
										}
									}
								}

								if ((pLoopPlot->area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE)) {
									// Extra weight for enemy territory in offensive areas
									iTempValue *= 2;
								}

								if (GET_PLAYER(getOwnerINLINE()).AI_isPrimaryArea(pLoopPlot->area())) {
									iTempValue *= 3;
									iTempValue /= 2;
								}

								if (pLoopPlot->isBarbarian()) {
									iTempValue /= 2;
								}
							}
						}

						iValue += iTempValue;
					}
				}
			}
		}

		// Consider available defense, direct threat to potential base
		iOurDefense = GET_PLAYER(getOwnerINLINE()).AI_localDefenceStrength(pPlot, getTeam(), DOMAIN_LAND, 0);
		iEnemyOffense = GET_PLAYER(getOwnerINLINE()).AI_localAttackStrength(pPlot, NO_TEAM, DOMAIN_LAND, 2);

		if (3 * iEnemyOffense > iOurDefense || iOurDefense == 0) {
			iValue *= iOurDefense;
			iValue /= std::max(1, 3 * iEnemyOffense);
		}

		// Value forts less, they are generally riskier bases
		if (pCity == NULL) {
			iValue *= 2;
			iValue /= 3;
		}
	} else {
		if (pPlot->getOwnerINLINE() != getOwnerINLINE()) {
			// Keep planes at home when not in real wars
			return 0;
		}

		// If no wars, use prior logic with added value to keeping planes safe from sneak attack
		if (pCity != NULL) {
			// K-Mod. Try not to waste airspace which we need for air defenders; but use the needed air defenders as a proxy for good offense placement.
			// AI_cityThreat has arbitrary scale, so it should not be added to population like that.
			// (the rest of this function still needs some work, but this bit was particularly problematic.)
			int iDefNeeded = static_cast<CvCityAI*>(pCity)->AI_neededAirDefenders();
			int iDefHere = pPlot->plotCount(PUF_isAirIntercept, -1, -1, NO_PLAYER, getTeam()) - (atPlot(pPlot) && PUF_isAirIntercept(this, -1, -1) ? 1 : 0);
			int iSpace = pPlot->airUnitSpaceAvailable(getTeam()) + (atPlot(pPlot) ? 1 : 0);
			iValue = pCity->getPopulation() + 20;
			iValue *= std::min(iDefNeeded + 1, iDefHere + iSpace);
			if (iDefNeeded > iSpace + iDefHere) {
				FAssert(iDefNeeded > 0);
				// drop value to zero if we can't even fit half of the air defenders we need here.
				iValue *= 2 * (iSpace + iDefHere) - iDefNeeded;
				iValue /= iDefNeeded;
			}
		} else {
			if (iDefenders > 0) {
				iValue = (pCity != NULL) ? 0 : GET_PLAYER(getOwnerINLINE()).AI_getPlotAirbaseValue(pPlot);
				iValue /= 6;
			}
		}

		iValue += std::min(24, 3 * (iDefenders - iAttackAirCount));

		if (GET_PLAYER(getOwnerINLINE()).AI_isPrimaryArea(pPlot->area())) {
			iValue *= 4;
			iValue /= 3;
		}

		// No real enemies, check for minor civ or barbarian cities where attacks could be supported
		pNearestEnemyCity = GC.getMapINLINE().findCity(pPlot->getX_INLINE(), pPlot->getY_INLINE(), NO_PLAYER, NO_TEAM, false, false, getTeam());

		if (pNearestEnemyCity != NULL) {
			iDistance = plotDistance(pPlot->getX_INLINE(), pPlot->getY_INLINE(), pNearestEnemyCity->getX_INLINE(), pNearestEnemyCity->getY_INLINE());
			if (iDistance > airRange()) {
				iValue /= 10 * (2 + airRange());
			} else {
				iValue /= 2 + iDistance;
			}
		}
	}

	if (pPlot->getOwnerINLINE() == getOwnerINLINE()) {
		// Bases in our territory better than teammate's
		iValue *= 2;
	} else if (pPlot->getTeam() == getTeam()) {
		// Our team's bases are better than vassal plots
		iValue *= 3;
		iValue /= 2;
	}

	return iValue;
}

// Returns true if a mission was pushed...
// Most of this function has been rewritten for K-Mod, using bbai as the base version. (old code deleted.)
bool CvUnitAI::AI_airDefensiveCity() {
	PROFILE_FUNC();

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	FAssert(getDomainType() == DOMAIN_AIR);
	FAssert(canAirDefend());

	if (canAirDefend() && getDamage() == 0) {
		CvCity* pCity = plot()->getPlotCity();

		if (pCity && pCity->getOwnerINLINE() == getOwnerINLINE()) {
			int iExistingAirDefenders = plot()->plotCount(PUF_isAirIntercept, -1, -1, getOwnerINLINE());
			if (PUF_isAirIntercept(this, -1, -1))
				iExistingAirDefenders--;
			int iNeedAirDefenders = pCity->AI_neededAirDefenders();

			if (iExistingAirDefenders < iNeedAirDefenders / 2 && iExistingAirDefenders < 3) {
				// Be willing to defend with a couple of planes even if it means their doom.
				getGroup()->pushMission(MISSION_AIRPATROL);
				return true;
			}

			if (iExistingAirDefenders < iNeedAirDefenders) {
				// Stay if city is threatened or if we're well short of our target, but not if capture is imminent.
				int iEnemyOffense = kOwner.AI_localAttackStrength(plot(), NO_TEAM, DOMAIN_LAND, 2);

				if (iEnemyOffense > 0 || iExistingAirDefenders < iNeedAirDefenders / 2) {
					int iOurDefense = kOwner.AI_localDefenceStrength(plot(), getTeam());
					if (iEnemyOffense < iOurDefense) {
						getGroup()->pushMission(MISSION_AIRPATROL);
						return true;
					}
				}
			}
		}
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	int iLoop;
	for (CvCity* pLoopCity = kOwner.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kOwner.nextCity(&iLoop)) {
		if (!canAirDefend(pLoopCity->plot()))
			continue;

		if (!atPlot(pLoopCity->plot()) && !canMoveInto(pLoopCity->plot()))
			continue;

		bool bCurrentPlot = atPlot(pLoopCity->plot());

		int iExistingAirDefenders = pLoopCity->plot()->plotCount(PUF_isAirIntercept, -1, -1, pLoopCity->getOwnerINLINE());
		if (bCurrentPlot && PUF_isAirIntercept(this, -1, -1))
			iExistingAirDefenders--;
		int iNeedAirDefenders = pLoopCity->AI_neededAirDefenders();
		int iAirSpaceAvailable = pLoopCity->plot()->airUnitSpaceAvailable(kOwner.getTeam()) + (bCurrentPlot ? 1 : 0);

		if (iNeedAirDefenders > iExistingAirDefenders || iAirSpaceAvailable > 1) {
			// K-Mod note: AI_cityThreat is too expensive for this stuff, and it's already taken into account by AI_neededAirDefenders anyway

			int iOurDefense = kOwner.AI_localDefenceStrength(pLoopCity->plot(), getTeam(), DOMAIN_LAND, 0);
			int iEnemyOffense = kOwner.AI_localAttackStrength(pLoopCity->plot(), NO_TEAM, DOMAIN_LAND, 2);

			int iValue = 10 + iAirSpaceAvailable;
			iValue *= 10 * std::max(0, iNeedAirDefenders - iExistingAirDefenders) + 1;

			if (bCurrentPlot && iAirSpaceAvailable > 1)
				iValue = iValue * 4 / 3;

			if (kOwner.AI_isPrimaryArea(pLoopCity->area())) {
				iValue *= 4;
				iValue /= 3;
			}

			if (pLoopCity->getPreviousOwner() != getOwnerINLINE()) {
				iValue *= (GC.getGameINLINE().getGameTurn() - pLoopCity->getGameTurnAcquired() < 20 ? 3 : 4);
				iValue /= 5;
			}

			// Reduce value of endangered city, it may be too late to help
			if (iEnemyOffense > 0) {
				if (iOurDefense == 0)
					iValue = 0;
				else if (iEnemyOffense * 4 > iOurDefense * 3) {
					// note: this will drop to zero when iEnemyOffense = 1.5 * iOurDefence.
					iValue *= 6 * iOurDefense - 4 * iEnemyOffense;
					iValue /= 3 * iOurDefense;
				}
			}

			if (iValue > iBestValue) {
				iBestValue = iValue;
				pBestPlot = pLoopCity->plot();
			}
		}
	}

	if (pBestPlot != NULL && !atPlot(pBestPlot)) {
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_airCarrier() {
	//PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	if (getCargo() > 0) {
		return false;
	}

	if (isCargo()) {
		if (canAirDefend()) {
			getGroup()->pushMission(MISSION_AIRPATROL);
			return true;
		} else {
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		}
	}

	int iBestValue = 0;
	CvUnit* pBestUnit = NULL;

	int iLoop;
	for (CvUnit* pLoopUnit = kOwner.firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = kOwner.nextUnit(&iLoop)) {
		if (canLoadUnit(pLoopUnit, pLoopUnit->plot())) {
			int iValue = 10;

			if (!(pLoopUnit->plot()->isCity())) {
				iValue += 20;
			}

			if (pLoopUnit->plot()->isOwned()) {
				if (isEnemy(pLoopUnit->plot()->getTeam(), pLoopUnit->plot())) {
					iValue += 20;
				}
			} else {
				iValue += 10;
			}

			iValue /= (pLoopUnit->getCargo() + 1);

			if (iValue > iBestValue) {
				iBestValue = iValue;
				pBestUnit = pLoopUnit;
			}
		}
	}

	if (pBestUnit != NULL) {
		if (atPlot(pBestUnit->plot())) {
			setTransportUnit(pBestUnit); // XXX is this dangerous (not pushing a mission...) XXX air units?
			return true;
		} else {
			getGroup()->pushMission(MISSION_MOVE_TO, pBestUnit->getX_INLINE(), pBestUnit->getY_INLINE());
			return true;
		}
	}

	return false;
}

bool CvUnitAI::AI_missileLoad(UnitAITypes eTargetUnitAI, int iMaxOwnUnitAI, bool bStealthOnly) {
	//PROFILE_FUNC();

	CvUnit* pBestUnit = NULL;
	int iBestValue = 0;
	int iLoop;
	for (CvUnit* pLoopUnit = GET_PLAYER(getOwnerINLINE()).firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = GET_PLAYER(getOwnerINLINE()).nextUnit(&iLoop)) {
		if (!bStealthOnly || pLoopUnit->getInvisibleType() != NO_INVISIBLE) {
			if (pLoopUnit->AI_getUnitAIType() == eTargetUnitAI) {
				if ((iMaxOwnUnitAI == -1) || (pLoopUnit->getUnitAICargo(AI_getUnitAIType()) <= iMaxOwnUnitAI)) {
					if (canLoadUnit(pLoopUnit, pLoopUnit->plot())) {
						int iValue = 100;

						iValue += GC.getGame().getSorenRandNum(100, "AI missile load");

						iValue *= 1 + pLoopUnit->getCargo();

						if (iValue > iBestValue) {
							iBestValue = iValue;
							pBestUnit = pLoopUnit;
						}
					}
				}
			}
		}
	}

	if (pBestUnit != NULL) {
		if (atPlot(pBestUnit->plot())) {
			setTransportUnit(pBestUnit); // XXX is this dangerous (not pushing a mission...) XXX air units?
			return true;
		} else {
			getGroup()->pushMission(MISSION_MOVE_TO, pBestUnit->getX_INLINE(), pBestUnit->getY_INLINE());
			setTransportUnit(pBestUnit);
			return true;
		}
	}

	return false;

}


// Returns true if a mission was pushed...
// K-Mod. I'm rewritten this function so that it now considers bombarding city defences and bombing improvements
// as well as air strikes against enemy troops. Also, it now prefers to hit targets that are in our territory.
bool CvUnitAI::AI_airStrike(int iThreshold) {
	PROFILE_FUNC();

	int iSearchRange = airRange();

	int iBestValue = iThreshold + isSuicide() && m_pUnitInfo->getProductionCost() > 0 ? m_pUnitInfo->getProductionCost() * 5 / 6 : 0;
	CvPlot* pBestPlot = NULL;
	bool bBombard = false; // K-Mod. bombard (city / improvement), rather than air strike (damage)

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				int iStrikeValue = 0;
				int iBombValue = 0;
				int iAdjacentAttackers = 0; // (only count adjacent units if we can air-strike)
				int iAssaultEnRoute = pLoopPlot->isCity() ? GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_ASSAULT, getGroup(), 1) : 0;

				// TODO: consider changing the evaluation system so that instead of simply counting units, it counts attack / defence power.

				// air strike (damage)
				if (canMoveInto(pLoopPlot, true)) {
					iAdjacentAttackers += GET_PLAYER(getOwnerINLINE()).AI_adjacentPotentialAttackers(pLoopPlot);
					CvUnit* pDefender = pLoopPlot->getBestDefender(NO_PLAYER, getOwnerINLINE(), this, true);

					FAssert(pDefender != NULL);
					FAssert(pDefender->canDefend());

					int iDamage = airCombatDamage(pDefender);
					int iDefenders = pLoopPlot->getNumVisibleEnemyDefenders(this);

					iStrikeValue = std::max(0, std::min(pDefender->getDamage() + iDamage, airCombatLimit()) - pDefender->getDamage());

					iStrikeValue += iDamage * collateralDamage() * std::min(iDefenders - 1, collateralDamageMaxUnits()) / 200;

					iStrikeValue *= (3 + iAdjacentAttackers + iAssaultEnRoute / 2);
					iStrikeValue /= (iAdjacentAttackers + iAssaultEnRoute > 0 ? 4 : 6) + std::min(iAdjacentAttackers + iAssaultEnRoute / 2, iDefenders) / 2;

					if (pLoopPlot->isCity(true, pDefender->getTeam())) {
						// units heal more easily in a city / fort
						iStrikeValue *= 3;
						iStrikeValue /= 4;
					}
					if (pLoopPlot->isWater() && (iAdjacentAttackers > 0 || pLoopPlot->getTeam() == getTeam())) {
						iStrikeValue *= 3;
					} else if (pLoopPlot->isAdjacentTeam(getTeam())) // prefer defensive strikes
					{
						iStrikeValue *= 2;
					}
				}
				// bombard (destroy improvement / city defences)
				if (canAirBombAt(plot(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE())) {
					if (pLoopPlot->isCity()) {
						const CvCity* pCity = pLoopPlot->getPlotCity();
						iBombValue = std::max(0, std::min(pCity->getDefenseDamage() + airBombCurrRate(), GC.getMAX_CITY_DEFENSE_DAMAGE()) - pCity->getDefenseDamage());
						iBombValue *= iAdjacentAttackers + 2 * iAssaultEnRoute + (area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE ? 5 : 1);
						iBombValue /= 2;
					} else {
						BonusTypes eBonus = pLoopPlot->getNonObsoleteBonusType(getTeam(), true);
						if (eBonus != NO_BONUS && pLoopPlot->isOwned() && canAirBombAt(plot(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE())) {
							iBombValue = GET_PLAYER(pLoopPlot->getOwner()).AI_bonusVal(eBonus, -1);
							iBombValue += GET_PLAYER(pLoopPlot->getOwner()).AI_bonusVal(eBonus, 0);
						}
					}
				}
				// factor in air defenses but try to avoid using bestInterceptor, because that's a slow function.
				if (iBombValue > iBestValue || iStrikeValue > iBestValue) // values only decreased from here on.
				{
					if (isSuicide()) {
						iStrikeValue /= 2;
						iBombValue /= 2;
					} else if (!canAirDefend()) // assume that air defenders are strong.. and that they are willing to fight
					{
						CvUnit* pInterceptor = bestInterceptor(pLoopPlot);

						if (pInterceptor != NULL) {
							int iInterceptProb = pInterceptor->currInterceptionProbability();

							iInterceptProb *= std::max(0, (100 - evasionProbability()));
							iInterceptProb /= 100;

							iStrikeValue *= std::max(0, 100 - iInterceptProb / 2);
							iStrikeValue /= 100;
							iBombValue *= std::max(0, 100 - iInterceptProb / 2);
							iBombValue /= 100;
						}
					}

					if (iStrikeValue > iBestValue || iBombValue > iBestValue) {
						bBombard = iBombValue > iStrikeValue;
						iBestValue = std::max(iBombValue, iStrikeValue);
						pBestPlot = pLoopPlot;
						FAssert(!atPlot(pBestPlot));
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		if (bBombard)
			getGroup()->pushMission(MISSION_AIRBOMB, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		else
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}

// Air strike around base city
// Returns true if a mission was pushed...
bool CvUnitAI::AI_defendBaseAirStrike() {
	PROFILE_FUNC();

	// Only search around base
	int iSearchRange = 2;

	int iBestValue = (isSuicide() && m_pUnitInfo->getProductionCost() > 0) ? (15 * m_pUnitInfo->getProductionCost()) : 0;
	CvPlot* pBestPlot = NULL;

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (canMoveInto(pLoopPlot, true) && !pLoopPlot->isWater()) // Only true of plots this unit can airstrike
				{
					if (plot()->area() == pLoopPlot->area()) {
						int iValue = 0;

						CvUnit* pDefender = pLoopPlot->getBestDefender(NO_PLAYER, getOwnerINLINE(), this, true);

						FAssert(pDefender != NULL);
						FAssert(pDefender->canDefend());

						int iDamage = airCombatDamage(pDefender);

						iValue = std::max(0, (std::min((pDefender->getDamage() + iDamage), airCombatLimit()) - pDefender->getDamage()));

						iValue += ((iDamage * collateralDamage()) * std::min((pLoopPlot->getNumVisibleEnemyDefenders(this) - 1), collateralDamageMaxUnits())) / (2 * 100);

						// Weight towards stronger units
						iValue *= (pDefender->currCombatStr(NULL, NULL, NULL) + 2000);
						iValue /= 2000;

						// Weight towards adjacent stacks
						if (plotDistance(plot()->getX_INLINE(), plot()->getY_INLINE(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE()) == 1) {
							iValue *= 5;
							iValue /= 4;
						}

						CvUnit* pInterceptor = bestInterceptor(pLoopPlot);

						if (pInterceptor != NULL) {
							int iInterceptProb = isSuicide() ? 100 : pInterceptor->currInterceptionProbability();

							iInterceptProb *= std::max(0, (100 - evasionProbability()));
							iInterceptProb /= 100;

							iValue *= std::max(0, 100 - iInterceptProb / 2);
							iValue /= 100;
						}

						if (iValue > iBestValue) {
							iBestValue = iValue;
							pBestPlot = pLoopPlot;
							FAssert(!atPlot(pBestPlot));
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}

bool CvUnitAI::AI_airBombPlots() {
	//PROFILE_FUNC();

	int iSearchRange = airRange();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (!pLoopPlot->isCity() && pLoopPlot->isOwned() && pLoopPlot != plot()) {
					if (canAirBombAt(plot(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE())) {
						int iValue = 0;

						if (pLoopPlot->getBonusType(pLoopPlot->getTeam()) != NO_BONUS) {
							iValue += AI_pillageValue(pLoopPlot, 15);

							iValue += GC.getGameINLINE().getSorenRandNum(10, "AI Air Bomb");
						} else if (isSuicide()) {
							//This should only be reached when the unit is desperate to die
							iValue += AI_pillageValue(pLoopPlot);
							// Guided missiles lean towards destroying resource-producing tiles as opposed to improvements like Towns
							if (pLoopPlot->getBonusType(pLoopPlot->getTeam()) != NO_BONUS) {
								//and even more so if it's a resource
								iValue += GET_PLAYER(pLoopPlot->getOwnerINLINE()).AI_bonusVal(pLoopPlot->getBonusType(pLoopPlot->getTeam()), 0);
							}
						}

						if (iValue > 0) {
							// K-Mod. Try to avoid using bestInterceptor... because that's a slow function.
							if (isSuicide()) {
								iValue /= 2;
							} else if (!canAirDefend()) // assume that air defenders are strong.. and that they are willing to fight
							{
								CvUnit* pInterceptor = bestInterceptor(pLoopPlot);

								if (pInterceptor != NULL) {
									int iInterceptProb = pInterceptor->currInterceptionProbability();

									iInterceptProb *= std::max(0, (100 - evasionProbability()));
									iInterceptProb /= 100;

									iValue *= std::max(0, 100 - iInterceptProb / 2);
									iValue /= 100;
								}
							}

							if (iValue > iBestValue) {
								iBestValue = iValue;
								pBestPlot = pLoopPlot;
								FAssert(!atPlot(pBestPlot));
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		getGroup()->pushMission(MISSION_AIRBOMB, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


bool CvUnitAI::AI_exploreAir() {
	PROFILE_FUNC();

	const CvPlayer& kPlayer = GET_PLAYER(getOwnerINLINE());
	CvPlot* pBestPlot = NULL;
	int iBestValue = 0;

	for (int iI = 0; iI < MAX_PLAYERS; iI++) {
		if (GET_PLAYER((PlayerTypes)iI).isAlive() && !GET_PLAYER((PlayerTypes)iI).isBarbarian()) {
			if (GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam()) {
				int iLoop;
				for (CvCity* pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop)) {
					if (!pLoopCity->isVisible(getTeam(), false)) {
						if (canReconAt(plot(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE())) {
							int iValue = 1 + GC.getGame().getSorenRandNum(15, "AI explore air");
							if (isEnemy(GET_PLAYER((PlayerTypes)iI).getTeam())) {
								iValue += 10;
								iValue += std::min(10, pLoopCity->area()->getNumAIUnits(getOwnerINLINE(), UNITAI_ATTACK_CITY));
								iValue += 10 * kPlayer.AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_ASSAULT);
							}

							iValue *= plotDistance(getX_INLINE(), getY_INLINE(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE());

							if (iValue > iBestValue) {
								iBestValue = iValue;
								pBestPlot = pLoopCity->plot();
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		getGroup()->pushMission(MISSION_RECON, pBestPlot->getX(), pBestPlot->getY());
		return true;
	}

	return false;
}

int CvUnitAI::AI_exploreAirPlotValue(CvPlot* pPlot) {
	int iValue = 0;
	if (pPlot->isVisible(getTeam(), false)) {
		iValue++;

		if (!pPlot->isOwned()) {
			iValue++;
		}

		if (!pPlot->isImpassable(getTeam())) {
			iValue *= 4;

			if (pPlot->isWater() || pPlot->getArea() == getArea()) {
				iValue *= 2;
			}
		}
	}

	return iValue;
}

bool CvUnitAI::AI_exploreAir2() {
	PROFILE_FUNC();

	CvPlayer& kPlayer = GET_PLAYER(getOwnerINLINE());
	CvPlot* pBestPlot = NULL;
	int iBestValue = 0;

	int iSearchRange = airRange();
	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (!pLoopPlot->isVisible(getTeam(), false)) {
					if (canReconAt(plot(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE())) {
						int iValue = AI_exploreAirPlotValue(pLoopPlot);

						for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
							DirectionTypes eDirection = (DirectionTypes)iI;
							CvPlot* pAdjacentPlot = plotDirection(getX_INLINE(), getY_INLINE(), eDirection);
							if (pAdjacentPlot != NULL) {
								if (!pAdjacentPlot->isVisible(getTeam(), false)) {
									iValue += AI_exploreAirPlotValue(pAdjacentPlot);
								}
							}
						}

						iValue += GC.getGame().getSorenRandNum(25, "AI explore air");
						iValue *= std::min(7, plotDistance(getX_INLINE(), getY_INLINE(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE()));

						if (iValue > iBestValue) {
							iBestValue = iValue;
							pBestPlot = pLoopPlot;
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		getGroup()->pushMission(MISSION_RECON, pBestPlot->getX(), pBestPlot->getY());
		return true;
	}

	return false;
}

void CvUnitAI::AI_exploreAirMove() {
	if (AI_exploreAir()) {
		return;
	}

	if (AI_exploreAir2()) {
		return;
	}

	if (canAirDefend()) {
		getGroup()->pushMission(MISSION_AIRPATROL);
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


// Returns true if a mission was pushed...
// This function has been completely rewritten for K-Mod.
bool CvUnitAI::AI_nuke() {
	PROFILE_FUNC();

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());
	const CvTeamAI& kTeam = GET_TEAM(kOwner.getTeam());

	bool bDanger = kOwner.AI_getAnyPlotDanger(plot(), 2); // consider changing this to something smarter
	int iWarRating = kTeam.AI_getWarSuccessRating();
	// iBaseWeight is the civ-independant part of the weight for civilian damage evaluation
	int iBaseWeight = 10;
	iBaseWeight += kOwner.AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST3) || GC.getGameINLINE().isOption(GAMEOPTION_AGGRESSIVE_AI) ? 20 : 0;
	iBaseWeight += kOwner.AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST4) ? 20 : 0;
	iBaseWeight += std::max(0, -iWarRating);
	iBaseWeight -= std::max(0, iWarRating - 50); // don't completely destroy them if we want to keep their land.

	CvPlot* pBestTarget = 0;
	// the initial value of iBestValue is the threshold for action. (cf. units of AI_nukeValue)
	int iBestValue = std::max(0, 4 * getUnitInfo().getProductionCost());
	iBestValue += bDanger || kOwner.AI_isDoStrategy(AI_STRATEGY_DAGGER) ? 20 : 100;
	iBestValue *= std::max(1, kOwner.getNumNukeUnits() + 2 * kOwner.getNumCities());
	iBestValue /= std::max(1, 2 * kOwner.getNumNukeUnits() + (bDanger ? 2 : 1) * kOwner.getNumCities());
	iBestValue *= 150 + iWarRating;
	iBestValue /= 150;

	for (PlayerTypes i = (PlayerTypes)0; i < MAX_CIV_PLAYERS; i = (PlayerTypes)(i + 1)) {
		const CvPlayer& kLoopPlayer = GET_PLAYER(i);
		if (kLoopPlayer.isAlive() && isEnemy(kLoopPlayer.getTeam())) {
			int iDestructionWeight = iBaseWeight - kOwner.AI_getAttitudeWeight(i) / 2 + std::min(60, 5 * kOwner.AI_getMemoryCount(i, MEMORY_NUKED_US) + 2 * kOwner.AI_getMemoryCount(i, MEMORY_NUKED_FRIEND));
			int iLoop;
			for (CvCity* pLoopCity = kLoopPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kLoopPlayer.nextCity(&iLoop)) {
				// note: we could use "AI_deduceCitySite" here, but if we can't see the city, then we can't properly judge its target value anyway.
				if (pLoopCity->isRevealed(getTeam(), false) && canNukeAt(plot(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE())) {
					CvPlot* pTarget;
					int iValue = AI_nukeValue(pLoopCity->plot(), nukeRange(), pTarget, iDestructionWeight);
					iValue /= (kTeam.AI_getWarPlan(pLoopCity->getTeam()) == WARPLAN_LIMITED && iWarRating > -10) ? 2 : 1;

					if (iValue > iBestValue) {
						iBestValue = iValue;
						pBestTarget = pTarget;
					}
				}
			}
		}
	}

	if (pBestTarget) {
		FAssert(canNukeAt(plot(), pBestTarget->getX_INLINE(), pBestTarget->getY_INLINE()));
		getGroup()->pushMission(MISSION_NUKE, pBestTarget->getX_INLINE(), pBestTarget->getY_INLINE());
		return true;
	}

	return false;
}

// this function has been completely rewritten for K-Mod
bool CvUnitAI::AI_nukeRange(int iRange) {
	PROFILE_FUNC();

	int iThresholdValue = 60 + std::max(0, 3 * getUnitInfo().getProductionCost());
	if (!GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(plot(), DANGER_RANGE))
		iThresholdValue = iThresholdValue * 3 / 2;

	CvPlot* pTargetPlot = 0;
	int iNukeValue = AI_nukeValue(plot(), iRange, pTargetPlot);
	if (iNukeValue > iThresholdValue) {
		FAssert(pTargetPlot && canNukeAt(plot(), pTargetPlot->getX_INLINE(), pTargetPlot->getY_INLINE()));
		getGroup()->pushMission(MISSION_NUKE, pTargetPlot->getX_INLINE(), pTargetPlot->getY_INLINE());
		return true;
	}
	return false;
}


// K-Mod. Get the best trade mission value.
// Note. The iThreshold parameter is only there to improve efficiency.
int CvUnitAI::AI_tradeMissionValue(CvPlot*& pBestPlot, int iThreshold) {
	pBestPlot = NULL;

	int iBestValue = 0;
	int iBestPathTurns = INT_MAX;

	if (getUnitInfo().getBaseTrade() <= 0 && getUnitInfo().getTradeMultiplier() <= 0)
		return 0;

	for (int iI = 0; iI < MAX_PLAYERS; iI++) {
		if (GET_PLAYER((PlayerTypes)iI).isAlive()) {
			int iLoop;
			for (CvCity* pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop)) {
				if (AI_plotValid(pLoopCity->plot()) && !pLoopCity->plot()->isVisibleEnemyUnit(this)) {
					int iValue = getTradeGold(pLoopCity->plot());
					int iPathTurns;

					if (iValue >= iThreshold && canTrade(pLoopCity->plot())) {
						if (generatePath(pLoopCity->plot(), MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns)) {
							if (iValue / (4 + iPathTurns) > iBestValue / (4 + iBestPathTurns)) {
								iBestValue = iValue;
								iBestPathTurns = iPathTurns;
								pBestPlot = getPathEndTurnPlot();
								iThreshold = std::max(iThreshold, iBestValue * 4 / (4 + iBestPathTurns));
							}
						}
					}
				}
			}
		}
	}

	return iBestValue;
}

// K-Mod. Move to destination for a trade mission. (pTradePlot is either the target city, or the end-turn plot.)
bool CvUnitAI::AI_doTradeMission(CvPlot* pTradePlot) {
	if (pTradePlot != NULL) {
		if (atPlot(pTradePlot)) {
			FAssert(canTrade(pTradePlot));
			if (canTrade(pTradePlot)) {
				getGroup()->pushMission(MISSION_TRADE);
				return true;
			}
		} else {
			getGroup()->pushMission(MISSION_MOVE_TO, pTradePlot->getX_INLINE(), pTradePlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY, false, false, MISSIONAI_TRADE);
			return true;
		}
	}

	return false;
}

// find the best place to do create a great work (culture bomb)
int CvUnitAI::AI_greatWorkValue(CvPlot*& pBestPlot, int iThreshold) {
	pBestPlot = NULL;

	int iBestValue = 0;
	int iBestPathTurns = INT_MAX;
	int iLoop;

	if (getUnitInfo().getGreatWorkCulture() == 0)
		return 0;

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());
	for (CvCity* pLoopCity = kOwner.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kOwner.nextCity(&iLoop)) {
		if (AI_plotValid(pLoopCity->plot()) && !pLoopCity->plot()->isVisibleEnemyUnit(this)) {
			int iValue = getGreatWorkCulture(pLoopCity->plot()) * kOwner.AI_commerceWeight(COMMERCE_CULTURE, pLoopCity) / 100;
			// commerceWeight takes into account culture pressure and cultural victory strategy.
			// However, it is intended to be used for evaluating steady culture rates rather than bursts.
			// Therefore the culture-pressure side of it is probably under-rated, and the cultural
			// victory part is based on current culture rather than turns to legendary.
			// Also, it doesn't take into account the possibility of flipping enemy cities.
			// ... But it's a good start.
			int iPathTurns;

			if (iValue >= iThreshold && canGreatWork(pLoopCity->plot())) {
				if (generatePath(pLoopCity->plot(), MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns)) {
					if (iValue / (4 + iPathTurns) > iBestValue / (4 + iBestPathTurns)) {
						iBestValue = iValue;
						iBestPathTurns = iPathTurns;
						pBestPlot = getPathEndTurnPlot();
						iThreshold = std::max(iThreshold, iBestValue * 4 / (4 + iBestPathTurns));
					}
				}
			}
		}
	}

	return iBestValue;
}

// create great work if we're at pCulturePlot, otherwise just move towards pCulturePlot.
bool CvUnitAI::AI_doGreatWork(CvPlot* pCulturePlot) {
	if (pCulturePlot != NULL) {
		if (atPlot(pCulturePlot)) {
			FAssert(canGreatWork(pCulturePlot));
			if (canGreatWork(pCulturePlot)) {
				getGroup()->pushMission(MISSION_GREAT_WORK);
				return true;
			}
		} else {
			getGroup()->pushMission(MISSION_MOVE_TO, pCulturePlot->getX_INLINE(), pCulturePlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY, false, false, MISSIONAI_GREAT_WORK);
			return true;
		}
	}

	return false;
}


bool CvUnitAI::AI_infiltrate() {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	if (canInfiltrate(plot())) {
		getGroup()->pushMission(MISSION_INFILTRATE);
		return true;
	}

	for (int iI = 0; iI < MAX_PLAYERS; iI++) {
		if ((GET_PLAYER((PlayerTypes)iI).isAlive()) && GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam()) {
			int iLoop;
			for (CvCity* pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop)) {
				if (canInfiltrate(pLoopCity->plot())) {
					// BBAI efficiency: check area for land units before generating path
					if ((getDomainType() == DOMAIN_LAND) && (pLoopCity->area() != area()) && !(getGroup()->canMoveAllTerrain())) {
						continue;
					}

					int iValue = getEspionagePoints(pLoopCity->plot());

					if (iValue > iBestValue) {
						int iPathTurns;
						if (generatePath(pLoopCity->plot(), 0, true, &iPathTurns)) {
							FAssert(iPathTurns > 0);

							if (getPathFinder().GetFinalMoves() == 0) {
								iPathTurns++;
							}

							iValue /= 1 + iPathTurns;

							if (iValue > iBestValue) {
								iBestValue = iValue;
								pBestPlot = pLoopCity->plot();
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL)) {
		if (atPlot(pBestPlot)) {
			getGroup()->pushMission(MISSION_INFILTRATE);
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
			getGroup()->pushMission(MISSION_INFILTRATE, -1, -1, 0, true); // K-Mod
			return true;
		}
	}

	return false;
}

bool CvUnitAI::AI_reconSpy(int iRange) {
	PROFILE_FUNC();
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestTargetPlot = NULL;
	int iBestValue = 0;

	int iSearchRange = AI_searchRange(iRange);

	for (int iX = -iSearchRange; iX <= iSearchRange; iX++) {
		for (int iY = -iSearchRange; iY <= iSearchRange; iY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iX, iY);
			int iDistance = stepDistance(0, 0, iX, iY);
			if ((iDistance > 0) && (pLoopPlot != NULL) && AI_plotValid(pLoopPlot)) {
				int iValue = 0;
				if (pLoopPlot->getPlotCity() != NULL) {
					iValue += GC.getGameINLINE().getSorenRandNum(2400, "AI Spy Scout City"); // was 4000
				}

				if (pLoopPlot->getBonusType(getTeam()) != NO_BONUS) {
					iValue += GC.getGameINLINE().getSorenRandNum(800, "AI Spy Recon Bonus"); // was 1000
				}

				for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
					CvPlot* pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), ((DirectionTypes)iI));

					if (pAdjacentPlot != NULL) {
						if (!pAdjacentPlot->isRevealed(getTeam(), false)) {
							iValue += 500;
						} else if (!pAdjacentPlot->isVisible(getTeam(), false)) {
							iValue += 200;
						}
					}
				}
				if (pLoopPlot->getTeam() == getTeam())
					iValue /= 4;
				else if (isPotentialEnemy(pLoopPlot->getTeam(), pLoopPlot))
					iValue *= 2;


				if (iValue > 0) {
					int iPathTurns;
					if (generatePath(pLoopPlot, 0, true, &iPathTurns, iRange)) {
						if (iPathTurns <= iRange) {
							// don't give each and every plot in range a value before generating the patch (performance hit)
							iValue += GC.getGameINLINE().getSorenRandNum(250, "AI Spy Scout Best Plot");

							iValue *= iDistance;
							if (iValue > iBestValue) {
								iBestValue = iValue;
								pBestTargetPlot = getPathEndTurnPlot();
								pBestPlot = pLoopPlot;
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestTargetPlot != NULL)) {
		if (atPlot(pBestTargetPlot)) {
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		} else {
			// K-Mod. (skip turn after each step of a recon mission? strange)
			getGroup()->pushMission(MISSION_MOVE_TO, pBestTargetPlot->getX_INLINE(), pBestTargetPlot->getY_INLINE(), 0, false, false, MISSIONAI_RECON_SPY, pBestPlot);
			return true;
		}
	}

	return false;
}

/// \brief Spy decision on whether to cause revolt in besieged city
///
/// Have spy breakdown city defenses if we have troops in position to capture city this turn.
bool CvUnitAI::AI_revoltCitySpy() {
	PROFILE_FUNC();

	CvCity* pCity = plot()->getPlotCity();

	FAssert(pCity != NULL);

	if (pCity == NULL) {
		return false;
	}

	if (!(GET_TEAM(getTeam()).isAtWar(pCity->getTeam()))) {
		return false;
	}

	if (pCity->isDisorder()) {
		return false;
	}

	if (100 * (GC.getMAX_CITY_DEFENSE_DAMAGE() - pCity->getDefenseDamage()) / std::max(1, GC.getMAX_CITY_DEFENSE_DAMAGE()) < 15)
		return false;

	for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission) {
		CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
		if ((kMissionInfo.getCityRevoltCounter() > 0) || (kMissionInfo.getPlayerAnarchyCounter() > 0)) {
			if (GET_PLAYER(getOwnerINLINE()).canDoEspionageMission((EspionageMissionTypes)iMission, pCity->getOwnerINLINE(), pCity->plot(), -1, this)) {
				if (gUnitLogLevel > 2) logBBAI("      %S uses city revolt at %S.", GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), pCity->getName().GetCString());
				getGroup()->pushMission(MISSION_ESPIONAGE, iMission);
				return true;
			}
		}
	}

	return false;
}

// K-Mod, I've moved the pathfinding check out of this function.
int CvUnitAI::AI_getEspionageTargetValue(CvPlot* pPlot) {
	PROFILE_FUNC();

	CvTeamAI& kTeam = GET_TEAM(getTeam());
	int iValue = 0;

	if (pPlot->isOwned() && pPlot->getTeam() != getTeam() && !GET_TEAM(getTeam()).isVassal(pPlot->getTeam())) {
		if (AI_plotValid(pPlot)) {
			CvCity* pCity = pPlot->getPlotCity();
			if (pCity != NULL) {
				iValue += pCity->getPopulation();
				iValue += pCity->plot()->calculateCulturePercent(getOwnerINLINE()) / 8;

				int iRand = GC.getGame().getSorenRandNum(6, "AI spy choose city");
				iValue += iRand * iRand;

				if (area()->getTargetCity(getOwnerINLINE()) == pCity) {
					iValue += 30;
				}

				if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pPlot, MISSIONAI_ASSAULT, getGroup()) > 0) {
					iValue += 30;
				}
				// K-Mod. Dilute the effect of population, and take cost modifiers into account.
				iValue += 10;
				iValue *= 100;
				iValue /= GET_PLAYER(getOwnerINLINE()).getEspionageMissionCostModifier(NO_ESPIONAGEMISSION, pCity->getOwner(), pPlot);
			} else {
				BonusTypes eBonus = pPlot->getNonObsoleteBonusType(getTeam(), true);
				if (eBonus != NO_BONUS) {
					iValue += GET_PLAYER(pPlot->getOwnerINLINE()).AI_baseBonusVal(eBonus) - 10;
				}
			}
		}
	}

	return iValue;
}

// heavily edited for K-Mod
bool CvUnitAI::AI_cityOffenseSpy(int iMaxPath, CvCity* pSkipCity) {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pEndTurnPlot = NULL;

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());
	const CvTeamAI& kTeam = GET_TEAM(getTeam());

	const int iEra = kOwner.getCurrentEra();
	int iBaselinePoints = 50 * iEra * (iEra + 1); // cf the "big espionage" minimum value.
	int iAverageUnspentPoints;
	{
		int iTeamCount = 0;
		int iTotalUnspentPoints = 0;
		for (int iI = 0; iI < MAX_CIV_TEAMS; iI++) {
			const CvTeam& kLoopTeam = GET_TEAM((TeamTypes)iI);
			if (iI != getTeam() && kLoopTeam.isAlive() && !kTeam.isVassal((TeamTypes)iI)) {
				iTotalUnspentPoints += kTeam.getEspionagePointsAgainstTeam((TeamTypes)iI);
				iTeamCount++;
			}
		}
		iAverageUnspentPoints = iTotalUnspentPoints /= std::max(1, iTeamCount);
	}


	for (int iPlayer = 0; iPlayer < MAX_CIV_PLAYERS; ++iPlayer) {
		CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iPlayer);
		if (kLoopPlayer.isAlive() && kLoopPlayer.getTeam() != getTeam() && !GET_TEAM(getTeam()).isVassal(kLoopPlayer.getTeam())) {
			int iTeamWeight = 1000;
			iTeamWeight *= kTeam.getEspionagePointsAgainstTeam(kLoopPlayer.getTeam());
			iTeamWeight /= std::max(1, iAverageUnspentPoints + iBaselinePoints);

			iTeamWeight *= 400 - kTeam.AI_getAttitudeWeight(kLoopPlayer.getTeam());
			iTeamWeight /= 500;

			iTeamWeight *= kTeam.AI_getWarPlan(kLoopPlayer.getTeam()) != NO_WARPLAN ? 2 : 1;
			iTeamWeight *= kTeam.AI_isSneakAttackPreparing(kLoopPlayer.getTeam()) ? 2 : 1;

			iTeamWeight *= kOwner.AI_isMaliciousEspionageTarget((PlayerTypes)iPlayer) ? 3 : 2;
			iTeamWeight /= 2;

			if (iTeamWeight < 200 && GC.getGame().getSorenRandNum(10, "AI team target saving throw") != 0) {
				// low weight. Probably friendly attitude and below average points.
				// don't target this team.
				continue;
			}

			int iLoop;
			for (CvCity* pLoopCity = kLoopPlayer.firstCity(&iLoop); NULL != pLoopCity; pLoopCity = kLoopPlayer.nextCity(&iLoop)) {
				if (pLoopCity == pSkipCity || !kOwner.AI_deduceCitySite(pLoopCity)) {
					continue;
				}

				if (pLoopCity->area() == area() || canMoveAllTerrain()) {
					CvPlot* pLoopPlot = pLoopCity->plot();
					if (AI_plotValid(pLoopPlot)) {
						int iPathTurns;
						if (generatePath(pLoopPlot, 0, true, &iPathTurns, iMaxPath) && iPathTurns <= iMaxPath) {
							int iValue = AI_getEspionageTargetValue(pLoopPlot);

							iValue *= 5;
							iValue /= (5 + GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_ATTACK_SPY, getGroup()));
							iValue *= iTeamWeight;
							if (iValue > iBestValue) {
								iBestValue = iValue;
								pBestPlot = pLoopPlot;
								pEndTurnPlot = getPathEndTurnPlot();
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		if (atPlot(pBestPlot)) {
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY);
		} else {
			FAssert(pEndTurnPlot != NULL);
			getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX(), pEndTurnPlot->getY(), 0, false, false, MISSIONAI_ATTACK_SPY, pBestPlot);
		}
		return true;
	}

	return false;
}

bool CvUnitAI::AI_bonusOffenseSpy(int iRange) {
	PROFILE_FUNC();

	CvPlot* pBestPlot = NULL;
	CvPlot* pEndTurnPlot = NULL;

	int iBestValue = 10;

	int iSearchRange = AI_searchRange(iRange);

	for (int iX = -iSearchRange; iX <= iSearchRange; iX++) {
		for (int iY = -iSearchRange; iY <= iSearchRange; iY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iX, iY);

			if (NULL != pLoopPlot && pLoopPlot->getNonObsoleteBonusType(getTeam(), true) != NO_BONUS) {
				if (pLoopPlot->isOwned() && pLoopPlot->getTeam() != getTeam()) {
					// K-Mod. I think this is only worthwhile when at war...
					//if (kOwner.AI_isMaliciousEspionageTarget(pLoopPlot->getOwner()))
					if (GET_TEAM(getTeam()).isAtWar(pLoopPlot->getTeam())) {
						int iPathTurns;
						if (generatePath(pLoopPlot, 0, true, &iPathTurns, iRange) && iPathTurns <= iRange) {
							int iValue = AI_getEspionageTargetValue(pLoopPlot);
							iValue *= 4;
							iValue /= (4 + GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_ATTACK_SPY, getGroup()));
							if (iValue > iBestValue) {
								iBestValue = iValue;
								pBestPlot = pLoopPlot;
								pEndTurnPlot = getPathEndTurnPlot();
							}
						}

					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		if (atPlot(pBestPlot)) {
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY);
			return true;
		} else {
			FAssert(pEndTurnPlot != NULL);
			getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX(), pEndTurnPlot->getY(), 0, false, false, MISSIONAI_ATTACK_SPY, pBestPlot);
			return true;
		}
	}

	return false;
}

//Returns true if the spy performs espionage.
bool CvUnitAI::AI_espionageSpy() {
	PROFILE_FUNC();

	if (!canEspionage(plot())) {
		return false;
	}

	EspionageMissionTypes eBestMission = NO_ESPIONAGEMISSION;
	CvPlot* pTargetPlot = NULL;
	PlayerTypes eTargetPlayer = NO_PLAYER;
	int iExtraData = -1;

	eBestMission = AI_bestPlotEspionage(eTargetPlayer, pTargetPlot, iExtraData);
	if (NO_ESPIONAGEMISSION == eBestMission) {
		return false;
	}

	if (!GET_PLAYER(getOwnerINLINE()).canDoEspionageMission(eBestMission, eTargetPlayer, pTargetPlot, iExtraData, this)) {
		return false;
	}

	getGroup()->pushMission(MISSION_ESPIONAGE, eBestMission, iExtraData);

	return true;
}

// K-Mod edition. (This use to be a CvPlayerAI:: function.)
EspionageMissionTypes CvUnitAI::AI_bestPlotEspionage(PlayerTypes& eTargetPlayer, CvPlot*& pPlot, int& iData) const {
	PROFILE_FUNC();

	CvPlot* pSpyPlot = plot();
	const CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());
	bool bBigEspionage = kPlayer.AI_isDoStrategy(AI_STRATEGY_BIG_ESPIONAGE);

	FAssert(pSpyPlot != NULL);

	int iSpyValue = 3 * kPlayer.getProductionNeeded(getUnitType()) + 60;
	if (kPlayer.getCapitalCity() != NULL) {
		iSpyValue += stepDistance(getX(), getY(), kPlayer.getCapitalCity()->getX(), kPlayer.getCapitalCity()->getY()) / 2;
	}

	pPlot = NULL;
	iData = -1;

	EspionageMissionTypes eBestMission = NO_ESPIONAGEMISSION;
	int iBestValue = 0;

	int iEspionageRate = kPlayer.getCommerceRate(COMMERCE_ESPIONAGE);

	if (pSpyPlot->isOwned()) {
		TeamTypes eTargetTeam = pSpyPlot->getTeam();

		if (eTargetTeam != getTeam()) {
			int iEspPoints = GET_TEAM(getTeam()).getEspionagePointsAgainstTeam(eTargetTeam);

			// estimate risk cost of losing the spy while trying to escape
			int iBaseIntercept = 0;
			{
				int iTargetTotal = GET_TEAM(eTargetTeam).getEspionagePointsEver();
				int iOurTotal = GET_TEAM(getTeam()).getEspionagePointsEver();
				iBaseIntercept += (GC.getDefineINT("ESPIONAGE_INTERCEPT_SPENDING_MAX") * iTargetTotal) / std::max(1, iTargetTotal + iOurTotal);

				if (GET_TEAM(eTargetTeam).getCounterespionageModAgainstTeam(getTeam()) > 0)
					iBaseIntercept += GC.getDefineINT("ESPIONAGE_INTERCEPT_COUNTERESPIONAGE_MISSION");
			}
			int iEscapeCost = 2 * iSpyValue * iBaseIntercept * (100 + GC.getDefineINT("ESPIONAGE_SPY_MISSION_ESCAPE_MOD")) / 10000;

			// One espionage mission loop to rule them all.
			for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission) {
				CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
				int iTestData = 1;
				if (kMissionInfo.getBuyTechCostFactor() > 0) {
					iTestData = GC.getNumTechInfos();
				} else if (kMissionInfo.getDestroyProjectCostFactor() > 0) {
					iTestData = GC.getNumProjectInfos();
				} else if (kMissionInfo.getDestroyBuildingCostFactor() > 0) {
					iTestData = GC.getNumBuildingInfos();
				} else if (kMissionInfo.getAttitudeModifier() != 0) {
					iTestData = MAX_PLAYERS;
				} else if (kMissionInfo.getDestroyUnitCostFactor() > 0) {
					iTestData = GC.getNumSpecialistInfos();
				}

				// estimate the risk cost of losing the spy.
				int iOverhead = iEscapeCost + iSpyValue * iBaseIntercept * (100 + kMissionInfo.getDifficultyMod()) / 10000;

				for (iTestData--; iTestData >= 0; iTestData--) {
					int iCost = kPlayer.getEspionageMissionCost((EspionageMissionTypes)iMission, pSpyPlot->getOwner(), pSpyPlot, iTestData, this);
					if (iCost < 0 || (iCost <= iEspPoints && !kPlayer.canDoEspionageMission((EspionageMissionTypes)iMission, pSpyPlot->getOwnerINLINE(), pSpyPlot, iTestData, this)))
						continue; // we can't do the mission, and cost is not the limiting factor.

					int iValue = kPlayer.AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, iTestData);
					iValue *= 80 + GC.getGameINLINE().getSorenRandNum(60, "AI best espionage mission");
					iValue /= 100;
					iValue -= iOverhead;
					iValue -= iCost * (bBigEspionage ? 2 : 1) * iCost / std::max(1, iCost + GET_TEAM(getTeam()).getEspionagePointsAgainstTeam(eTargetTeam));

					// If we can't do the mission yet, don't completely give up. It might be worth saving points for.
					if (!kPlayer.canDoEspionageMission((EspionageMissionTypes)iMission, pSpyPlot->getOwner(), pSpyPlot, iTestData, this)) {
						// Is cost is the reason we can't do the mission?
						if (GET_TEAM(getTeam()).isHasTech((TechTypes)kMissionInfo.getTechPrereq())) {
							FAssert(iCost > iEspPoints); // (see condition at the top of the loop)
							// Scale the mission value based on how long we think it will take to get the points.

							int iTurns = (iCost - iEspPoints) / std::max(1, iEspionageRate);
							iTurns *= bBigEspionage ? 1 : 2;
							// The number of turns is approximated (poorly) by assuming our entire esp rate is targeting eTargetTeam.
							iValue *= 3;
							iValue /= iTurns + 3;
							// eg, 1 turn left -> 3/4. 2 turns -> 3/5, 3 turns -> 3/6. Etc.
						} else {
							// Ok. Now it's time to give up. (Even if we're researching the prereq tech right now - too bad.)
							iValue = 0;
						}
					}

					// Block small missions when using "big espionage", unless the mission is really good value.
					if (bBigEspionage
						&& iValue < 50 * kPlayer.getCurrentEra() * (kPlayer.getCurrentEra() + 1) // 100, 300, 600, 1000, 1500, ...
						&& iValue < (kPlayer.AI_isDoStrategy(AI_STRATEGY_ESPIONAGE_ECONOMY) ? 4 : 2) * iCost) {
						iValue = 0;
					}

					if (iValue > iBestValue) {
						iBestValue = iValue;
						eBestMission = (EspionageMissionTypes)iMission;
						eTargetPlayer = pSpyPlot->getOwner();
						pPlot = pSpyPlot;
						iData = iTestData;
					}
				}
			}
		}
	}
	if (gUnitLogLevel > 2 && eBestMission != NO_ESPIONAGEMISSION) {
		// The following assert isn't a problem or a bug. I just want to know when it happens, for testing purposes.
		//FAssertMsg(!kPlayer.AI_isDoStrategy(AI_STRATEGY_ESPIONAGE_ECONOMY) || GC.getEspionageMissionInfo(eBestMission).getBuyTechCostFactor() > 0 || GC.getEspionageMissionInfo(eBestMission).getDestroyProjectCostFactor() > 0, "Potentially wasteful AI use of espionage.");
		logBBAI("      %S chooses %S as their best%s espionage mission (value: %d, cost: %d).", GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), GC.getEspionageMissionInfo(eBestMission).getText(), bBigEspionage ? " (big)" : "", iBestValue, kPlayer.getEspionageMissionCost(eBestMission, eTargetPlayer, pPlot, iData, this));
	}

	return eBestMission;
}

bool CvUnitAI::AI_moveToStagingCity() {
	PROFILE_FUNC();

	CvPlot* pStagingPlot = NULL;
	CvPlot* pEndTurnPlot = NULL;

	int iBestValue = 0;

	int iWarCount = 0;
	TeamTypes eTargetTeam = NO_TEAM;
	CvTeam& kTeam = GET_TEAM(getTeam());
	for (int iI = 0; iI < MAX_TEAMS; iI++) {
		if ((iI != getTeam()) && GET_TEAM((TeamTypes)iI).isAlive()) {
			if (kTeam.AI_isSneakAttackPreparing((TeamTypes)iI)) {
				eTargetTeam = (TeamTypes)iI;
				iWarCount++;
			}
		}
	}

	if (iWarCount > 1) {
		eTargetTeam = NO_TEAM;
	}

	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if ((pLoopCity->area() == area()) && AI_plotValid(pLoopCity->plot())) {
			// BBAI TODO: Need some knowledge of whether this is a good city to attack from ... only get that
			// indirectly from threat.
			int iValue = pLoopCity->AI_cityThreat();

			// Have attack stacks in assault areas move to coastal cities for faster loading
			if ((area()->getAreaAIType(getTeam()) == AREAAI_ASSAULT) || (area()->getAreaAIType(getTeam()) == AREAAI_ASSAULT_MASSING)) {
				CvArea* pWaterArea = pLoopCity->waterArea();
				if (pWaterArea != NULL && GET_TEAM(getTeam()).AI_isWaterAreaRelevant(pWaterArea)) {
					// BBAI TODO:  Need a better way to determine which cities should serve as invasion launch locations

					// Inertia so units don't just chase transports around the map
					iValue = iValue / 2;
					if (pLoopCity->area()->getAreaAIType(getTeam()) == AREAAI_ASSAULT) {
						// If in assault, transports may be at sea ... tend to stay where they left from
						// to speed reinforcement
						iValue += pLoopCity->plot()->plotCount(PUF_isAvailableUnitAITypeGroupie, UNITAI_ATTACK_CITY, -1, getOwnerINLINE());
					}

					// Attraction to cities which are serving as launch/pickup points
					iValue += 3 * pLoopCity->plot()->plotCount(PUF_isUnitAIType, UNITAI_ASSAULT_SEA, -1, getOwnerINLINE());
					iValue += 2 * pLoopCity->plot()->plotCount(PUF_isUnitAIType, UNITAI_ESCORT_SEA, -1, getOwnerINLINE());
					iValue += 5 * GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_PICKUP);
				} else {
					iValue = iValue / 8;
				}
			}

			if (iValue * 200 > iBestValue) {
				int iPathTurns;
				if (generatePath(pLoopCity->plot(), MOVE_AVOID_ENEMY_WEIGHT_3, true, &iPathTurns)) {
					iValue *= 1000;
					iValue /= (5 + iPathTurns);
					if ((pLoopCity->plot() != plot()) && pLoopCity->isVisible(eTargetTeam, false)) {
						iValue /= 2;
					}

					if (iValue > iBestValue) {
						iBestValue = iValue;
						pStagingPlot = pLoopCity->plot();
						pEndTurnPlot = getPathEndTurnPlot();
					}
				}
			}
		}
	}

	if (pStagingPlot != NULL) {
		if (atPlot(pStagingPlot)) {
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		} else {
			getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX_INLINE(), pEndTurnPlot->getY_INLINE(), MOVE_AVOID_ENEMY_WEIGHT_3, false, false, MISSIONAI_GROUP, pStagingPlot); // K-Mod
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed or we should wait for another unit to bombard...
bool CvUnitAI::AI_followBombard() {
	if (canBombard(plot())) {
		getGroup()->pushMission(MISSION_BOMBARD);
		return true;
	}

	// K-Mod note: I've disabled the following code because it seems like a timewaster with very little benefit.
	// The code checks if we are standing next to a city, and then checks if we have any other readyToMove group
	// next to the same city which can bombard... if so, return true.
	// I suppose the point of the code is to block our units from issuing a follow-attack order if we still have
	// some bombarding to do. -- But in my opinion, such checks, if we want them, should be done by the attack code.

	return false;
}


// Returns true if the unit has found a potential enemy...
bool CvUnitAI::AI_potentialEnemy(TeamTypes eTeam, const CvPlot* pPlot) {
	PROFILE_FUNC();

	if (isHiddenNationality()) {
		return true;
	} else if (getGroup()->AI_isDeclareWar(pPlot)) {
		return isPotentialEnemy(eTeam, pPlot);
	} else {
		return isEnemy(eTeam, pPlot);
	}
}


// Returns true if this plot needs some defense...
bool CvUnitAI::AI_defendPlot(CvPlot* pPlot) {
	if (!canDefend(pPlot)) {
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();
	if (pCity != NULL) {
		if (pCity->getOwnerINLINE() == getOwnerINLINE()) {
			if (pCity->AI_isDanger()) {
				return true;
			}
		}
	} else {
		if (pPlot->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE()) <= ((atPlot(pPlot)) ? 1 : 0)) {
			if (pPlot->plotCount(PUF_cannotDefend, -1, -1, getOwnerINLINE()) > 0) {
				return true;
			}
		}
	}

	return false;
}

int CvUnitAI::AI_pillageValue(CvPlot* pPlot, int iBonusValueThreshold) {
	FAssert(canPillage(pPlot) || canAirBombAt(plot(), pPlot->getX_INLINE(), pPlot->getY_INLINE()) || (getGroup()->getCargo() > 0));

	if (!(pPlot->isOwned())) {
		return 0;
	}

	int iValue = 0;

	int iBonusValue = 0;
	BonusTypes eNonObsoleteBonus = pPlot->isRevealed(getTeam(), false) ? pPlot->getNonObsoleteBonusType(pPlot->getTeam(), true) : NO_BONUS; // K-Mod
	if (eNonObsoleteBonus != NO_BONUS) {
		iBonusValue = GET_PLAYER(pPlot->getOwnerINLINE()).AI_bonusVal(eNonObsoleteBonus, 0); // K-Mod
	}

	if (iBonusValueThreshold > 0) {
		if (eNonObsoleteBonus == NO_BONUS) {
			return 0;
		} else if (iBonusValue < iBonusValueThreshold) {
			return 0;
		}
	}

	if (getDomainType() != DOMAIN_AIR) {
		if (pPlot->isRoute()) {
			iValue++;
			if (eNonObsoleteBonus != NO_BONUS) {
				iValue += iBonusValue; // K-Mod. (many more iBonusValues will be added again later anyway)
			}

			for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
				CvPlot* pAdjacentPlot = plotDirection(pPlot->getX_INLINE(), pPlot->getY_INLINE(), (DirectionTypes)iI); // K-Mod, bugfix

				if (pAdjacentPlot != NULL && pAdjacentPlot->getTeam() == pPlot->getTeam()) {
					if (pAdjacentPlot->isCity()) {
						iValue += 10;
					}

					if (!(pAdjacentPlot->isRoute())) {
						if (!pAdjacentPlot->isWater() && !pAdjacentPlot->isImpassable(getTeam())) {
							iValue += 2;
						}
					}
				}
			}
		}
	}

	ImprovementTypes eImprovement = pPlot->getImprovementDuration() > 20 ? pPlot->getImprovementType() : pPlot->getRevealedImprovementType(getTeam(), false);

	if (eImprovement != NO_IMPROVEMENT) {
		if (pPlot->getWorkingCity() != NULL) {
			iValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_FOOD, pPlot->getOwnerINLINE()) * 5);
			iValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_PRODUCTION, pPlot->getOwnerINLINE()) * 4);
			iValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_COMMERCE, pPlot->getOwnerINLINE()) * 3);
		}

		if (getDomainType() != DOMAIN_AIR) {
			iValue += GC.getImprovementInfo(eImprovement).getPillageGold();
		}

		if (eNonObsoleteBonus != NO_BONUS) {
			if (GET_PLAYER(pPlot->getOwnerINLINE()).doesImprovementConnectBonus(eImprovement, eNonObsoleteBonus)) // K-Mod
			{
				int iTempValue = iBonusValue * 4;

				if (pPlot->isConnectedToCapital() && (pPlot->getPlotGroupConnectedBonus(pPlot->getOwnerINLINE(), eNonObsoleteBonus) == 1)) {
					iTempValue *= 2;
				}

				iValue += iTempValue;
			}
		}
	}

	return iValue;
}

// K-Mod.
// Return the value of the best nuke target in the range specified, and set pBestTarget to be the specific target plot.
// The return value is roughly in units of production.
int CvUnitAI::AI_nukeValue(CvPlot* pCenterPlot, int iSearchRange, CvPlot*& pBestTarget, int iCivilianTargetWeight) const {
	PROFILE_FUNC();

	FAssert(pCenterPlot);

	int iMilitaryTargetWeight = 100;

	typedef std::map<CvPlot*, int> plotMap_t;
	plotMap_t affected_plot_values;
	int iBestValue = 0; // note: value is divided by 100 at the end
	pBestTarget = 0;

	for (int iX = -iSearchRange; iX <= iSearchRange; iX++) {
		for (int iY = -iSearchRange; iY <= iSearchRange; iY++) {
			CvPlot* pLoopTarget = plotXY(pCenterPlot, iX, iY);
			if (!pLoopTarget || !canNukeAt(plot(), pLoopTarget->getX_INLINE(), pLoopTarget->getY_INLINE()))
				continue;

			bool bValid = true;
			// Note: canNukeAt checks that we aren't hitting any 3rd party targets, so we don't have to worry about checking that elsewhere

			int iTargetValue = 0;

			for (int jX = -nukeRange(); bValid && jX <= nukeRange(); jX++) {
				for (int jY = -nukeRange(); bValid && jY <= nukeRange(); jY++) {
					CvPlot* pLoopPlot = plotXY(pLoopTarget, jX, jY);
					if (!pLoopPlot)
						continue;

					plotMap_t::iterator plot_it = affected_plot_values.find(pLoopPlot);
					if (plot_it != affected_plot_values.end()) {
						if (plot_it->second == INT_MIN)
							bValid = false;
						else
							iTargetValue += plot_it->second;
						continue;
					}
					// plot evaluation:
					int iPlotValue = 0;

					// value for improvements / bonuses etc.
					if (bValid && pLoopPlot->isOwned()) {
						bool bEnemy = isEnemy(pLoopPlot->getTeam(), pLoopPlot);
						FAssert(bEnemy || pLoopPlot->getTeam() == getTeam()); // it is owned, and we aren't allowed to nuke neutrals; so it is either enemy or ours.

						ImprovementTypes eImprovement = pLoopPlot->getRevealedImprovementType(getTeam(), false);
						BonusTypes eBonus = pLoopPlot->getNonObsoleteBonusType(getTeam());
						const CvPlayerAI& kPlotOwner = GET_PLAYER(pLoopPlot->getOwnerINLINE());

						if (eImprovement != NO_IMPROVEMENT) {
							const CvImprovementInfo& kImprovement = GC.getImprovementInfo(eImprovement);
							if (!kImprovement.isPermanent()) {
								// arbitrary values, sorry.
								iPlotValue += 8 * (bEnemy ? iCivilianTargetWeight : -50);
								if (kImprovement.getImprovementPillage() != NO_IMPROVEMENT) {
									iPlotValue += (kImprovement.getImprovementUpgrade() == NO_IMPROVEMENT ? 32 : 16) * (bEnemy ? iCivilianTargetWeight : -50);
								}
							}
						}
						if (eBonus != NO_BONUS) {
							iPlotValue += 8 * (bEnemy ? iCivilianTargetWeight : -50);
							if (kPlotOwner.doesImprovementConnectBonus(eImprovement, eBonus)) {
								// assume that valueable bonuses are military targets, because the enemy might be using the bonus to build weapons.
								iPlotValue += kPlotOwner.AI_bonusVal(eBonus, 0) * (bEnemy ? iMilitaryTargetWeight : -100);
							}
						}
					}

					// consider military units if the plot is visible. (todo: increase value of military units that we can chase down this turn, maybe.)
					if (bValid && pLoopPlot->isVisible(getTeam(), false)) {
						CLLNode<IDInfo>* pUnitNode = pLoopPlot->headUnitNode();
						while (pUnitNode) {
							CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
							pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

							if (!pLoopUnit->isInvisible(getTeam(), false, true)) // I'm going to allow the AI to cheat here by seeing cargo units. (Human players can usually guess when a ship is loaded...)
							{
								if (pLoopUnit->isEnemy(getTeam(), pLoopPlot)) {
									int iUnitValue = std::max(1, pLoopUnit->getUnitInfo().getProductionCost());
									// decrease the value for wounded units. (it might be nice to only do this if we are in a position to attack with ground forces...)
									int x = 100 * (pLoopUnit->maxHitPoints() - pLoopUnit->currHitPoints()) / std::max(1, pLoopUnit->maxHitPoints());
									iUnitValue -= iUnitValue * x * x / 10000;
									iPlotValue += iMilitaryTargetWeight * iUnitValue;
								} else // non enemy unit
								{
									if (pLoopUnit->getTeam() == getTeam()) {
										// nuking our own units... sometimes acceptable
										int x = pLoopUnit->getUnitInfo().getProductionCost();
										if (x > 0)
											iPlotValue -= iMilitaryTargetWeight * x;
										else
											bValid = false; // assume this is a special unit.
									} else {
										FAssertMsg(false, "3rd party unit being considered for nuking.");
									}
								}
							}
						} // end unit loop
					} // end plot visible

					if (bValid && pLoopPlot->isCity() && pLoopPlot->getPlotCity()->isRevealed(getTeam(), false)) {
						CvCity* pLoopCity = pLoopPlot->getPlotCity(); // I can imagine some cases where this actually isn't pCity. Can you?

						// it might even be one of our own cities, so be careful!
						if (!isEnemy(pLoopCity->getTeam(), pLoopPlot)) {
							bValid = false;
						} else {
							// the values used here are quite arbitrary.
							iPlotValue += iCivilianTargetWeight * 2 * (pLoopCity->getCultureLevel() + 2) * pLoopCity->getPopulation();

							// note, it is possible to see which buildings the city has by looking at the map. This is not secret information.
							for (BuildingTypes i = (BuildingTypes)0; i < GC.getNumBuildingInfos(); i = (BuildingTypes)(i + 1)) {
								if (pLoopCity->getNumRealBuilding(i) > 0) {
									const CvBuildingInfo& kBuildingInfo = GC.getBuildingInfo(i);
									if (!kBuildingInfo.isNukeImmune())
										iPlotValue += iCivilianTargetWeight * pLoopCity->getNumRealBuilding(i) * std::max(0, kBuildingInfo.getProductionCost());
								}
							}

							// if we don't have vision of the city, just assume that there are at least a couple of defenders, and count that into our evaluation.
							if (!pLoopPlot->isVisible(getTeam(), false)) {
								UnitTypes eBasicUnit = pLoopCity->getConscriptUnit();
								int iBasicCost = std::max(10, eBasicUnit != NO_UNIT ? GC.getUnitInfo(eBasicUnit).getProductionCost() : 0);
								int iExpectedUnits = 1 + ((1 + pLoopCity->getCultureLevel()) * pLoopCity->getPopulation() + pLoopCity->getHighestPopulation() / 2) / std::max(1, pLoopCity->getHighestPopulation());

								iPlotValue += iMilitaryTargetWeight * iExpectedUnits * iBasicCost;
							}
						}
					}
					// end of plot evaluation
					affected_plot_values[pLoopPlot] = bValid ? iPlotValue : INT_MIN;
					iTargetValue += iPlotValue;
				}
			}

			if (bValid && iTargetValue > iBestValue) {
				pBestTarget = pLoopTarget;
				iBestValue = iTargetValue;
			}
		}
	}
	return iBestValue / 100;
}


int CvUnitAI::AI_searchRange(int iRange) {
	if (iRange == 0) {
		return 0;
	}

	if (flatMovementCost() || (getDomainType() == DOMAIN_SEA)) {
		return (iRange * baseMoves());
	} else {
		return ((iRange + 1) * (baseMoves() + 1));
	}
}


// XXX at some point test the game with and without this function...
bool CvUnitAI::AI_plotValid(CvPlot* pPlot) {
	PROFILE_FUNC();

	if (m_pUnitInfo->isNoRevealMap() && willRevealByMove(pPlot)) {
		return false;
	}

	switch (getDomainType()) {
	case DOMAIN_SEA:
		if (pPlot->isWater() || canMoveAllTerrain()) {
			return true;
		} else if (pPlot->isFriendlyCity(*this, true) && pPlot->isCoastalLand()) {
			return true;
		}
		break;

	case DOMAIN_AIR:
		FAssert(false);
		break;

	case DOMAIN_LAND:
		if (pPlot->getArea() == getArea() || canMoveAllTerrain() || pPlot->isLandUnitWaterSafe()) {
			return true;
		}
		break;

	case DOMAIN_IMMOBILE:
		FAssert(false);
		break;

	default:
		FAssert(false);
		break;
	}

	return false;
}

// K-Mod. A new odds-adjusting function to replace the seriously flawed CvUnitAI::AI_finalOddsThreshold function.
// (note: I would like to put this in CvSelectionGroupAI ... but - well - I don't need to say it, right?
int CvUnitAI::AI_getWeightedOdds(CvPlot* pPlot, bool bPotentialEnemy) {
	PROFILE_FUNC();
	int iOdds;
	CvUnit* pAttacker = getGroup()->AI_getBestGroupAttacker(pPlot, bPotentialEnemy, iOdds);
	if (!pAttacker)
		return 0;
	CvUnit* pDefender = pPlot->getBestDefender(NO_PLAYER, getOwnerINLINE(), pAttacker, !bPotentialEnemy, bPotentialEnemy);

	if (!pDefender)
		return 100;

	int iAdjustedOdds = iOdds;

	// adjust the values based on the relative production cost of the units.
	{
		int iOurCost = pAttacker->getUnitInfo().getProductionCost();
		int iTheirCost = pDefender->getUnitInfo().getProductionCost();
		if (iOurCost > 0 && iTheirCost > 0 && iOurCost != iTheirCost) {
			int x = iOdds * (100 - iOdds) * 2 / (iOurCost + iTheirCost + 20);
			iAdjustedOdds += x * (iTheirCost - iOurCost) / 100;
		}
	}
	// similarly, adjust based on the LFB value (slightly diluted)
	{
		int iDilution = GC.getLFBBasedOnExperience() + GC.getLFBBasedOnHealer() + ROUND_DIVIDE(10 * GC.getLFBBasedOnExperience() * (GC.getGameINLINE().getCurrentEra() - GC.getGameINLINE().getStartEra() + 1), std::max(1, GC.getNumEraInfos() - GC.getGameINLINE().getStartEra()));
		int iOurValue = pAttacker->LFBgetRelativeValueRating() + iDilution;
		int iTheirValue = pDefender->LFBgetRelativeValueRating() + iDilution;

		int x = iOdds * (100 - iOdds) * 2 / std::max(1, iOurValue + iTheirValue);
		iAdjustedOdds += x * (iTheirValue - iOurValue) / 100;
	}

	// adjust down if the enemy is on a defensive tile - we'd prefer to attack them on open ground.
	if (!pDefender->noDefensiveBonus()) {
		iAdjustedOdds -= (100 - iOdds) * pPlot->defenseModifier(pDefender->getTeam(), false) / (getDomainType() == DOMAIN_SEA ? 100 : 300);
	}

	// adjust the odds up if the enemy is wounded. We want to attack them now before they heal.
	iAdjustedOdds += iOdds * (100 - iOdds) * pDefender->getDamage() / (100 * pDefender->maxHitPoints());
	// adjust the odds down if our attacker is wounded - but only if healing is viable.
	if (pAttacker->isHurt() && pAttacker->healRate(pAttacker->plot()) > 10)
		iAdjustedOdds -= iOdds * (100 - iOdds) * pAttacker->getDamage() / (100 * pAttacker->maxHitPoints());

	// We're extra keen to take cites when we can...
	if (pPlot->isCity() && pPlot->getNumVisiblePotentialEnemyDefenders(pAttacker) == 1) {
		iAdjustedOdds += (100 - iOdds) / 3;
	}

	// one more thing... unfortunately, the sea AI isn't evolved enough to do without something as painful as this...
	if (getDomainType() == DOMAIN_SEA && !getGroup()->hasCargo()) {
		// I'm sorry about this. I really am. I'll try to make it better one day...
		int iDefenders = pPlot->getNumVisiblePotentialEnemyDefenders(pAttacker);
		iAdjustedOdds *= 2 + getGroup()->getNumUnits();
		iAdjustedOdds /= 3 + std::min(iDefenders / 2, getGroup()->getNumUnits());
	}

	return range(iAdjustedOdds, 1, 99);
}

// A simple hash of the unit's birthmark.
// This is to be used for getting a 'random' number which depends on the unit but which does not vary from turn to turn.
unsigned CvUnitAI::AI_unitBirthmarkHash(int iExtra) const {
	unsigned iHash = AI_getBirthmark() + iExtra;
	iHash *= 2654435761; // golden ratio of 2^32;
	return iHash;
}

// another 'random' hash, but which depends on a particular plot
unsigned CvUnitAI::AI_unitPlotHash(const CvPlot* pPlot, int iExtra) const {
	return AI_unitBirthmarkHash(GC.getMapINLINE().plotNumINLINE(pPlot->getX_INLINE(), pPlot->getY_INLINE()) + iExtra);
}

int CvUnitAI::AI_stackOfDoomExtra() const {
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());
	int iFlavourExtra = kOwner.AI_getFlavorValue(FLAVOR_MILITARY) / 2 + (kOwner.AI_getFlavorValue(FLAVOR_MILITARY) > 0 ? 8 : 4);
	int iEra = kOwner.getCurrentEra();
	// 4 base. then rand between 0 and ... (1 or 2 + iEra + flavour * era ratio)
	return AI_getBirthmark() % ((kOwner.AI_isDoStrategy(AI_STRATEGY_CRUSH) ? 2 : 1) + iEra + (iEra + 1) * iFlavourExtra / std::max(1, GC.getNumEraInfos())) + 4;
}

// This function has been significantly modified for K-Mod
bool CvUnitAI::AI_stackAttackCity(int iPowerThreshold) {
	PROFILE_FUNC();
	CvPlot* pCityPlot = NULL;
	int iSearchRange = 1;

	FAssert(canMove());

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL && AI_plotValid(pLoopPlot)) {
				if (pLoopPlot->isCity()) // K-Mod. We want to attack a city. We don't care about guarded forts!
				{
					if (AI_potentialEnemy(pLoopPlot->getTeam(), pLoopPlot)) {
						if (!atPlot(pLoopPlot) && getGroup()->canMoveOrAttackInto(pLoopPlot, true, true)) {
							if (iPowerThreshold < 0) {
								// basic threshold calculation.
								CvCity* pCity = pLoopPlot->getPlotCity();
								// This automatic threshold calculation is used by AI_follow; and so we can't assume this unit is the head of the group.
								// ... But I think it's fair to assume that if our group has any bombard, it the head unit will have it.
								if (getGroup()->getHeadUnit()->bombardRate() > 0) {
									// if we can bombard, then we should do a rough calculation to give us a 'skip bombard' threshold.
									iPowerThreshold = ((GC.getMAX_CITY_DEFENSE_DAMAGE() - pCity->getDefenseDamage()) * GC.getBBAI_SKIP_BOMBARD_BASE_STACK_RATIO() + pCity->getDefenseDamage() * GC.getBBAI_SKIP_BOMBARD_MIN_STACK_RATIO()) / std::max(1, GC.getMAX_CITY_DEFENSE_DAMAGE());
								} else {
									// if we have no bombard ability - just use the minimum threshold
									iPowerThreshold = GC.getBBAI_SKIP_BOMBARD_MIN_STACK_RATIO();
								}
								FAssert(iPowerThreshold >= GC.getBBAI_ATTACK_CITY_STACK_RATIO());
							}

							if (getGroup()->AI_compareStacks(pLoopPlot, true) >= iPowerThreshold) {
								pCityPlot = pLoopPlot;
							}
						}
					}
					break; // there can only be one.
				}
			}
		}
	}

	if (pCityPlot != NULL) {
		if (gUnitLogLevel >= 1 && pCityPlot->getPlotCity() != NULL) {
			logBBAI("    Stack for player %d (%S) decides to attack city %S with stack ratio %d", getOwner(), GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), pCityPlot->getPlotCity()->getName(0).GetCString(), getGroup()->AI_compareStacks(pCityPlot, true));
			logBBAI("    City %S has defense modifier %d, %d with ignore building", pCityPlot->getPlotCity()->getName(0).GetCString(), pCityPlot->getPlotCity()->getDefenseModifier(false), pCityPlot->getPlotCity()->getDefenseModifier(true));
		}

		FAssert(!atPlot(pCityPlot));
		AI_considerDOW(pCityPlot);
		getGroup()->pushMission(MISSION_MOVE_TO, pCityPlot->getX_INLINE(), pCityPlot->getY_INLINE(), pCityPlot->isVisibleEnemyDefender(this) ? MOVE_DIRECT_ATTACK : 0);
		return true;
	}

	return false;
}

bool CvUnitAI::AI_moveIntoCity(int iRange) {
	PROFILE_FUNC();

	FAssert(canMove());

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	if (plot()->isCity()) {
		return false;
	}

	int iSearchRange = AI_searchRange(iRange);

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (AI_plotValid(pLoopPlot) && (!isEnemy(pLoopPlot->getTeam(), pLoopPlot))) {
					if (pLoopPlot->isCity() || (pLoopPlot->isCity(true))) {
						int iPathTurns;
						if (canMoveInto(pLoopPlot, false) && (generatePath(pLoopPlot, 0, true, &iPathTurns, 1) && (iPathTurns <= 1))) {
							int iValue = 1;
							if (pLoopPlot->getPlotCity() != NULL) {
								iValue += pLoopPlot->getPlotCity()->getPopulation();
							}

							if (iValue > iBestValue) {
								iBestValue = iValue;
								pBestPlot = getPathEndTurnPlot();
								FAssert(!atPlot(pBestPlot));
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


bool CvUnitAI::AI_poach() {
	int iBestPoachValue = 0;
	CvPlot* pBestPoachPlot = NULL;
	TeamTypes eBestPoachTeam = NO_TEAM;

	if (!GC.getGameINLINE().isOption(GAMEOPTION_AGGRESSIVE_AI)) {
		return false;
	}

	if (GET_TEAM(getTeam()).getNumMembers() > 1) {
		return false;
	}

	int iNoPoachRoll = GET_PLAYER(getOwnerINLINE()).AI_totalUnitAIs(UNITAI_WORKER);
	iNoPoachRoll += GET_PLAYER(getOwnerINLINE()).getNumCities();
	iNoPoachRoll = std::max(0, (iNoPoachRoll - 1) / 2);
	if (GC.getGameINLINE().getSorenRandNum(iNoPoachRoll, "AI Poach") > 0) {
		return false;
	}

	if (GET_TEAM(getTeam()).getAnyWarPlanCount(true) > 0) {
		return false;
	}

	FAssert(canAttack());



	int iRange = 1;
	//Look for a unit which is non-combat
	//and has a capture unit type
	for (int iX = -iRange; iX <= iRange; iX++) {
		for (int iY = -iRange; iY <= iRange; iY++) {
			if (iX != 0 && iY != 0) {
				CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iX, iY);
				if ((pLoopPlot != NULL) && (pLoopPlot->getTeam() != getTeam()) && pLoopPlot->isVisible(getTeam(), false)) {
					int iPoachCount = 0;
					int iDefenderCount = 0;
					CvUnit* pPoachUnit = NULL;
					CLLNode<IDInfo>* pUnitNode = pLoopPlot->headUnitNode();
					while (pUnitNode != NULL) {
						CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
						pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);
						if ((pLoopUnit->getTeam() != getTeam())
							&& GET_TEAM(getTeam()).canDeclareWar(pLoopUnit->getTeam())) {
							if (!pLoopUnit->canDefend()) {
								if (pLoopUnit->getCaptureUnitType(getCivilizationType()) != NO_UNIT) {
									iPoachCount++;
									pPoachUnit = pLoopUnit;
								}
							} else {
								iDefenderCount++;
							}
						}
					}

					if (pPoachUnit != NULL) {
						if (iDefenderCount == 0) {
							int iValue = iPoachCount * 100;
							iValue -= iNoPoachRoll * 25;
							if (iValue > iBestPoachValue) {
								iBestPoachValue = iValue;
								pBestPoachPlot = pLoopPlot;
								eBestPoachTeam = pPoachUnit->getTeam();
							}
						}
					}
				}
			}
		}
	}

	if (pBestPoachPlot != NULL) {
		//No war roll.
		if (!GET_TEAM(getTeam()).AI_performNoWarRolls(eBestPoachTeam)) {
			GET_TEAM(getTeam()).declareWar(eBestPoachTeam, true, WARPLAN_LIMITED);

			FAssert(!atPlot(pBestPoachPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPoachPlot->getX_INLINE(), pBestPoachPlot->getY_INLINE(), MOVE_DIRECT_ATTACK);
			return true;
		}

	}

	return false;
}

// K-Mod. I've rewriten most of this function.
bool CvUnitAI::AI_choke(int iRange, bool bDefensive, int iFlags) {
	PROFILE_FUNC();

	int iPercentDefensive;
	{
		int iDefCount = 0;
		CLLNode<IDInfo>* pUnitNode = getGroup()->headUnitNode();
		CvUnit* pLoopUnit = NULL;

		while (pUnitNode != NULL) {
			pLoopUnit = ::getUnit(pUnitNode->m_data);
			iDefCount += pLoopUnit->noDefensiveBonus() ? 0 : 1;

			pUnitNode = getGroup()->nextUnitNode(pUnitNode);
		}
		iPercentDefensive = 100 * iDefCount / getGroup()->getNumUnits();
	}

	CvPlot* pBestPlot = 0;
	CvPlot* pEndTurnPlot = 0;
	int iBestValue = 0;
	for (int iX = -iRange; iX <= iRange; iX++) {
		for (int iY = -iRange; iY <= iRange; iY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iX, iY);
			if (pLoopPlot && isEnemy(pLoopPlot->getTeam()) && !pLoopPlot->isVisibleEnemyUnit(this)) {
				int iPathTurns;
				if (pLoopPlot->getWorkingCity() && generatePath(pLoopPlot, iFlags, true, &iPathTurns, iRange)) {
					FAssert(pLoopPlot->getWorkingCity()->getTeam() == pLoopPlot->getTeam());
					int iValue = bDefensive ? pLoopPlot->defenseModifier(getTeam(), false) - 15 : 0; // K-Mod
					if (pLoopPlot->getBonusType(getTeam()) != NO_BONUS) {
						iValue = GET_PLAYER(pLoopPlot->getOwnerINLINE()).AI_bonusVal(pLoopPlot->getBonusType(), 0);
					}

					iValue += pLoopPlot->getYield(YIELD_PRODUCTION) * 9; // was 10
					iValue += pLoopPlot->getYield(YIELD_FOOD) * 12; // was 10
					iValue += pLoopPlot->getYield(YIELD_COMMERCE) * 5;

					if (atPlot(pLoopPlot) && canPillage(pLoopPlot)) {
						iValue += AI_pillageValue(pLoopPlot, 0) / (bDefensive ? 2 : 1);
					}

					if (iValue > 0) {
						iValue *= (bDefensive ? 25 : 50) + iPercentDefensive * pLoopPlot->defenseModifier(getTeam(), false) / 100;

						if (bDefensive) {
							// for defensive, we care a lot about path turns
							iValue *= 10;
							iValue /= std::max(1, iPathTurns);
						} else {
							// otherwise we just want to block as many tiles as possible
							iValue *= 10;
							iValue /= std::max(1, pLoopPlot->getNumDefenders(getOwnerINLINE()) + (pLoopPlot == plot() ? 0 : getGroup()->getNumUnits()));
						}

						if (iValue > iBestValue) {
							pBestPlot = pLoopPlot;
							pEndTurnPlot = getPathEndTurnPlot();
							iBestValue = iValue;
						}
					}
				}
			}
		}
	}
	if (pBestPlot) {
		FAssert(pBestPlot->getWorkingCity());
		CvPlot* pChokedCityPlot = pBestPlot->getWorkingCity()->plot();
		if (atPlot(pBestPlot)) {
			FAssert(atPlot(pEndTurnPlot));
			if (canPillage(plot()))
				getGroup()->pushMission(MISSION_PILLAGE, -1, -1, iFlags, false, false, MISSIONAI_CHOKE, pChokedCityPlot);
			else
				getGroup()->pushMission(MISSION_SKIP, -1, -1, iFlags, false, false, MISSIONAI_CHOKE, pChokedCityPlot);
			return true;
		} else {
			FAssert(!atPlot(pEndTurnPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pEndTurnPlot->getX(), pEndTurnPlot->getY(), iFlags, false, false, MISSIONAI_CHOKE, pChokedCityPlot);
			return true;
		}
	}

	return false;
}

bool CvUnitAI::AI_solveBlockageProblem(CvPlot* pDestPlot, bool bDeclareWar) {
	PROFILE_FUNC();

	FAssert(pDestPlot != NULL);


	if (pDestPlot != NULL) {
		FAStarNode* pStepNode;

		CvPlot* pSourcePlot = plot();

		if (gDLL->getFAStarIFace()->GeneratePath(&GC.getStepFinder(), pSourcePlot->getX_INLINE(), pSourcePlot->getY_INLINE(), pDestPlot->getX_INLINE(), pDestPlot->getY_INLINE(), false, 0, true)) {
			pStepNode = gDLL->getFAStarIFace()->GetLastNode(&GC.getStepFinder());

			while (pStepNode != NULL) {
				CvPlot* pStepPlot = GC.getMapINLINE().plotSorenINLINE(pStepNode->m_iX, pStepNode->m_iY);
				if (canMoveOrAttackInto(pStepPlot) && generatePath(pStepPlot, 0, true)) {
					if (bDeclareWar && pStepNode->m_pPrev != NULL) {
						CvPlot* pPlot = GC.getMapINLINE().plotSorenINLINE(pStepNode->m_pPrev->m_iX, pStepNode->m_pPrev->m_iY);
						if (pPlot->getTeam() != NO_TEAM) {
							if (!canMoveInto(pPlot, true, true)) {
								if (!isPotentialEnemy(pPlot->getTeam(), pPlot)) {
									CvTeamAI& kTeam = GET_TEAM(getTeam());
									if (kTeam.canDeclareWar(pPlot->getTeam())) {
										WarPlanTypes eWarPlan = WARPLAN_LIMITED;
										WarPlanTypes eExistingWarPlan = kTeam.AI_getWarPlan(pDestPlot->getTeam());
										if (eExistingWarPlan != NO_WARPLAN) {
											if ((eExistingWarPlan == WARPLAN_TOTAL) || (eExistingWarPlan == WARPLAN_PREPARING_TOTAL)) {
												eWarPlan = WARPLAN_TOTAL;
											}

											if (!kTeam.isAtWar(pDestPlot->getTeam())) {
												kTeam.AI_setWarPlan(pDestPlot->getTeam(), NO_WARPLAN);
											}
										}
										kTeam.AI_setWarPlan(pPlot->getTeam(), eWarPlan, true);
										//return (AI_targetCity());
										return AI_goToTargetCity(MOVE_AVOID_ENEMY_WEIGHT_2 | MOVE_DECLARE_WAR); // K-Mod / BBAI
									}
								}
							}
						}
					}
					if (pStepPlot->isVisibleEnemyUnit(this)) {
						FAssert(canAttack());
						CvPlot* pBestPlot = pStepPlot;
						//To prevent puppeteering attempt to barge through
						//if quite close
						if (getPathFinder().GetPathTurns() > 3) {
							pBestPlot = getPathEndTurnPlot();
						}

						FAssert(!atPlot(pBestPlot));
						getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_DIRECT_ATTACK);
						return true;
					}
				}
				pStepNode = pStepNode->m_pParent;
			}
		}
	}

	return false;
}

int CvUnitAI::AI_calculatePlotWorkersNeeded(CvPlot* pPlot, BuildTypes eBuild) {
	int iBuildTime = pPlot->getBuildTime(eBuild) - pPlot->getBuildProgress(eBuild);
	int iWorkRate = workRate(true);

	if (iWorkRate <= 0) {
		FAssert(false);
		return 1;
	}
	int iTurns = iBuildTime / iWorkRate;

	if (iBuildTime > (iTurns * iWorkRate)) {
		iTurns++;
	}

	int iNeeded = std::max(1, (iTurns + 2) / 3);

	if (pPlot->getNonObsoleteBonusType(getTeam()) != NO_BONUS) {
		iNeeded *= 2;
	}

	return iNeeded;

}

bool CvUnitAI::AI_canGroupWithAIType(UnitAITypes eUnitAI) const {
	if (eUnitAI != AI_getUnitAIType()) {
		switch (eUnitAI) {
		case (UNITAI_ATTACK_CITY):
			if (plot()->isCity() && (GC.getGame().getGameTurn() - plot()->getPlotCity()->getGameTurnAcquired()) <= 1) {
				return false;
			}
			break;
		default:
			break;
		}
	}

	return true;
}



bool CvUnitAI::AI_allowGroup(const CvUnit* pUnit, UnitAITypes eUnitAI) const {
	CvSelectionGroup* pGroup = pUnit->getGroup();
	CvPlot* pPlot = pUnit->plot();
	UnitRangeTypes eOurUnitRange = getRangeType();
	UnitRangeTypes eTheirRange = pUnit->getRangeType();

	// Never group if we are home bound
	if (eOurUnitRange == UNITRANGE_HOME) {
		return false;
	}

	// Don't group with units with better range
	if (eOurUnitRange < eTheirRange) {
		return false;
	}

	if (pUnit == this) {
		return false;
	}

	if (!pUnit->isGroupHead()) {
		return false;
	}

	if (pGroup == getGroup()) {
		return false;
	}

	if (pUnit->isCargo()) {
		return false;
	}

	if (pUnit->AI_getUnitAIType() != eUnitAI) {
		return false;
	}

	switch (pGroup->AI_getMissionAIType()) {
	case MISSIONAI_GUARD_CITY:
		// do not join groups that are guarding cities
		// intentional fallthrough
	case MISSIONAI_LOAD_SETTLER:
	case MISSIONAI_LOAD_ASSAULT:
	case MISSIONAI_LOAD_SPECIAL:
		// do not join groups that are loading into transports (we might not fit and get stuck in loop forever)
		return false;
		break;
	default:
		break;
	}

	if (pGroup->getActivityType() == ACTIVITY_HEAL) {
		// do not attempt to join groups which are healing this turn
		// (healing is cleared every turn for automated groups, so we know we pushed a heal this turn)
		return false;
	}

	if (!canJoinGroup(pPlot, pGroup)) {
		return false;
	}

	if (eUnitAI == UNITAI_SETTLE) {
		if (eOurUnitRange < UNITRANGE_RANGE || GET_PLAYER(getOwnerINLINE()).AI_getAnyPlotDanger(pPlot, 3)) {
			return false;
		}
	} else if (eUnitAI == UNITAI_ASSAULT_SEA) {
		if (!pGroup->hasCargo()) {
			return false;
		}
	}

	if ((getGroup()->getHeadUnitAI() == UNITAI_CITY_DEFENSE)) {
		if (plot()->isCity() && (plot()->getTeam() == getTeam()) && plot()->getBestDefender(getOwnerINLINE())->getGroup() == getGroup()) {
			return false;
		}
	}

	if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
		CvPlot* pTargetPlot = pGroup->AI_getMissionAIPlot();

		if (pTargetPlot != NULL) {
			if (pTargetPlot->isOwned()) {
				if (isPotentialEnemy(pTargetPlot->getTeam(), pTargetPlot)) {
					//Do not join groups which have debarked on an offensive mission
					return false;
				}
			}
		}
	}

	if (pUnit->getInvisibleType() != NO_INVISIBLE) {
		if (getInvisibleType() == NO_INVISIBLE) {
			return false;
		}
	}

	return true;
}

void CvUnitAI::read(FDataStreamBase* pStream) {
	CvUnit::read(pStream);

	uint uiFlag = 0;
	pStream->Read(&uiFlag);	// flags for expansion

	pStream->Read(&m_iBirthmark);

	pStream->Read((int*)&m_eUnitAIType);
	pStream->Read(&m_iAutomatedAbortTurn);
}


void CvUnitAI::write(FDataStreamBase* pStream) {
	CvUnit::write(pStream);

	uint uiFlag = 0;
	pStream->Write(uiFlag);		// flag for expansion

	pStream->Write(m_iBirthmark);

	pStream->Write(m_eUnitAIType);
	pStream->Write(m_iAutomatedAbortTurn);
}

// Private Functions...

// Lead From Behind, by UncutDragon, edited for K-Mod
void CvUnitAI::LFBgetBetterAttacker(CvUnit** ppAttacker, const CvPlot* pPlot, bool bPotentialEnemy, int& iAIAttackOdds, int& iAttackerValue) {
	CvUnit* pDefender = pPlot->getBestDefender(NO_PLAYER, getOwnerINLINE(), this, !bPotentialEnemy, bPotentialEnemy);

	int iOdds;
	int iValue = LFBgetAttackerRank(pDefender, iOdds);

	// Combat odds are out of 1000, but the AI routines need odds out of 100, and when called from AI_getBestGroupAttacker
	// we return this value. Note that I'm not entirely sure if/how that return value is actually used ... but just in case I
	// want to make sure I'm returning something consistent with what was there before
	int iAIOdds = (iOdds + 5) / 10;
	iAIOdds += GET_PLAYER(getOwnerINLINE()).AI_getAttackOddsChange();
	iAIOdds = std::max(1, std::min(iAIOdds, 99));

	if (collateralDamage() > 0) {
		int iPossibleTargets = std::min((pPlot->getNumVisibleEnemyDefenders(this) - 1), collateralDamageMaxUnits());

		if (iPossibleTargets > 0) {
			iValue *= (100 + ((collateralDamage() * iPossibleTargets) / 5));
			iValue /= 100;
		}
	}

	// Nothing to compare against - we're obviously better
	if (!(*ppAttacker)) {
		(*ppAttacker) = this;
		iAIAttackOdds = iAIOdds;
		iAttackerValue = iValue;
		return;
	}

	// Compare our adjusted value with the current best
	if (iValue > iAttackerValue) {
		(*ppAttacker) = this;
		iAIAttackOdds = iAIOdds;
		iAttackerValue = iValue;
	}
}

bool CvUnitAI::AI_toggleWorldView(WorldViewTypes eWorldView) {
	CvPlayer& kPlayer = GET_PLAYER(getOwnerINLINE());
	kPlayer.changeWorldViewActivatedStatus(eWorldView, !kPlayer.isWorldViewActivated(eWorldView));
	return true;
}

void CvUnitAI::AI_shadowMove() {
	PROFILE_FUNC();

	CvUnit* pTarget = getShadowUnit();
	FAssertMsg(pTarget != NULL, "Should be Shadowing a Unit!");

	if (AI_protectTarget(pTarget)) {
		return;
	}

	if (AI_moveToTarget(pTarget, true)) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

bool CvUnitAI::AI_moveToTarget(CvUnit* pTarget, bool bForce) {
	PROFILE_FUNC();

	if (atPlot(pTarget->plot()))
		return false;

	int iDX, iDY;
	int iSearchRange = baseMoves();
	if (!bForce) {
		for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
			for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
				CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

				if (pLoopPlot == pTarget->plot()) {
					return false;
				}
			}
		}
	}

	int iPathTurns;
	if (generatePath(pTarget->plot(), 0, true, &iPathTurns)) {
		getGroup()->pushMission(MISSION_MOVE_TO, getPathEndTurnPlot()->getX_INLINE(), getPathEndTurnPlot()->getY_INLINE());
		return true;
	}

	return false;
}

bool CvUnitAI::AI_protectTarget(CvUnit* pTarget) {
	PROFILE_FUNC();

	CvPlot* pBestPlot = NULL;

	int iDanger = GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(pTarget->plot(), 1, false);
	//No Danger
	if (iDanger == 0) {
		return false;
	}

	//Lots of Danger, Move Ontop of Target to protect it
	else if (iDanger > getGroup()->getNumUnits()) {
		int iPathTurns;
		if (generatePath(pTarget->plot(), 0, true, &iPathTurns)) {
			getGroup()->pushMission(MISSION_MOVE_TO, getPathEndTurnPlot()->getX_INLINE(), getPathEndTurnPlot()->getY_INLINE());
			return true;
		}
	}

	//Only minimal enemy targets, move to kill them if possible
	else {
		int iBestValue = 0;
		int iSearchRange = baseMoves();;
		for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
			for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
				CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

				if (pLoopPlot != NULL) {
					if (AI_plotValid(pLoopPlot)) {
						{
							if (pLoopPlot->isVisibleEnemyUnit(this)) {
								int iPathTurns;
								if (!atPlot(pLoopPlot) && canMoveInto(pLoopPlot, true) && generatePath(pLoopPlot, 0, true, &iPathTurns) && (iPathTurns <= iSearchRange)) {
									if (pLoopPlot->getNumVisibleEnemyDefenders(this) <= getGroup()->getNumUnits()) {
										if (pLoopPlot->getNumVisibleAdjacentEnemyDefenders(this) <= ((getGroup()->getNumUnits() * 3) / 2)) {
											int iValue = getGroup()->AI_attackOdds(pLoopPlot, true);

											if (iValue >= AI_getWeightedOdds(pLoopPlot, true)) {
												if (iValue > iBestValue) {
													iBestValue = iValue;
													pBestPlot = getPathEndTurnPlot();
													FAssert(!atPlot(pBestPlot));
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	//Not possible to kill enemies, retreat to target
	if (pBestPlot == NULL) {
		int iPathTurns;
		if (atPlot(pTarget->plot())) {
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		} else if (generatePath(pTarget->plot(), 0, true, &iPathTurns)) {
			getGroup()->pushMission(MISSION_MOVE_TO, getPathEndTurnPlot()->getX_INLINE(), getPathEndTurnPlot()->getY_INLINE());
			return true;
		}
	} else {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_DIRECT_ATTACK, false, false);
		return true;
	}

	return false;
}

void CvUnitAI::AI_autoEspionage() {
	PROFILE_FUNC();

	EspionageMissionTypes eBestMission = NO_ESPIONAGEMISSION;
	PlayerTypes	eTargetPlayer = plot()->getOwnerINLINE();
	int iExtraData = -1;

	// always and only the counter espionage mission
	for (EspionageMissionTypes eEspionageMission = (EspionageMissionTypes)0; eEspionageMission < GC.getNumEspionageMissionInfos(); eEspionageMission = (EspionageMissionTypes)(eEspionageMission + 1)) {
		CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo(eEspionageMission);
		if (kMissionInfo.getCounterespionageNumTurns() > 0) {
			eBestMission = eEspionageMission;
			break;
		}
	}

	if (plot()->isOwned() && (plot()->getTeam() != getTeam())) {
		if (plot()->isCity()) {
			// foreign city
			if (NO_ESPIONAGEMISSION != eBestMission) {
				// player level espionage questions
				if (GET_PLAYER(getOwnerINLINE()).canDoEspionageMission(eBestMission, eTargetPlayer, plot(), iExtraData, this)) {
					// conduct the mission
					if (!espionage(eBestMission, iExtraData)) {
						// couldn't do it so wait next turn and try again
						getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY);
					}
					// spy might wander from city to city until the counter espionage timer has lapsed
					return;
				} else {
					// player can't do espionage so wait next turn and try again
					getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY);
					return;
				}
			} else {
				// won't happen unless mission xml is empty
				getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY);
				return;
			}
		}

		// foreign territory but not a city plot
		else {
			// test for interception
			bool bReveal = false;
			if (testSpyIntercepted(eTargetPlayer, false, bReveal, GC.getEspionageMissionInfo(eBestMission).getDifficultyMod())) {
				return;
			}
		}
	}

	// neutral territory, home territory, team territory city or not
	// give the spy a target based on AI_cityOffenseSpy function call
	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvCity* pSkipCity = NULL;

	for (int iPlayer = 0; iPlayer < MAX_CIV_PLAYERS; ++iPlayer) {
		CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iPlayer);
		if (kLoopPlayer.isAlive() && kLoopPlayer.getTeam() != getTeam() && !GET_TEAM(getTeam()).isVassal(kLoopPlayer.getTeam())) {
			int counterEspTurnsLeft = GET_TEAM(getTeam()).getCounterespionageTurnsLeftAgainstTeam(kLoopPlayer.getTeam());
			// Only move to cities where we will run missions
			if (GET_PLAYER(getOwnerINLINE()).AI_getAttitudeWeight((PlayerTypes)iPlayer) < (GC.getGameINLINE().isOption(GAMEOPTION_AGGRESSIVE_AI) ? 51 : 1)
				|| GET_TEAM(getTeam()).AI_getWarPlan(kLoopPlayer.getTeam()) != NO_WARPLAN
				/*|| GET_TEAM(getTeam()).getBestKnownTechScorePercent() < 85*/) {
				if (counterEspTurnsLeft <= 1) {
					int iLoop;
					for (CvCity* pLoopCity = kLoopPlayer.firstCity(&iLoop); NULL != pLoopCity; pLoopCity = kLoopPlayer.nextCity(&iLoop)) {
						if (pLoopCity == pSkipCity) {
							continue;
						}

						if (pLoopCity->area() == area() || canMoveAllTerrain()) {
							CvPlot* pLoopPlot = pLoopCity->plot();
							if (AI_plotValid(pLoopPlot)) {
								int iValue = AI_getEspionageTargetValue(pLoopPlot);
								if (iValue > iBestValue) {
									iBestValue = iValue;
									if (GET_PLAYER(getOwnerINLINE()).canDoEspionageMission(eBestMission, (PlayerTypes)iPlayer, pLoopPlot, iExtraData, this)) {
										pBestPlot = pLoopPlot;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		if (atPlot(pBestPlot)) {
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY);
		} else {
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_ATTACK_SPY);
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY);
		}
		return;
	}

	getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY);
	return;
}

void CvUnitAI::AI_autoPillageMove() {
	PROFILE_FUNC();

	bool bPillageBarbarians = getOptionBOOL("Automations__PillageBarbarians", false);
	bool bIgnoreDanger = getOptionBOOL("Automations__PillageIgnoreDanger", false);

	if (AI_heal(30))
		return;

	// If we can pillage a bonus improvement someone is using in one turn go for it
	// note: having 2 moves doesn't necessarily mean we can move & pillage in the same turn, but it's a good enough approximation.
	if (plot()->isOwned() && plot()->getOwnerINLINE() != getOwnerINLINE())
		if (AI_pillageRange(getGroup()->baseMoves() > 1 ? 1 : 0, 11, true, false, bPillageBarbarians, bIgnoreDanger))
			return;

	// Lets travel a bit wider for a bonus improvement to hit
	if (AI_pillageRange(3, 11, true, false, bPillageBarbarians, bIgnoreDanger))
		return;

	// If there are no bonuses how about some other improvements
	if (AI_pillageRange(3, 0, true, false, bPillageBarbarians, bIgnoreDanger))
		return;

	if (AI_heal(50, 3))
		return;

	if (!isEnemy(plot()->getTeam()))
		if (AI_heal())
			return;

	// Lets check for high value bonus improvements outside of a city range if we are in enemy territory
	if ((area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE) || isEnemy(plot()->getTeam()))
		if (AI_pillageRange(15, 40, false, false))
			return;

	if (AI_heal())
		return;

	// We are bored so lets just go find anything to pillage
	if (AI_pillageRange(25, 0, false, false, bPillageBarbarians, bIgnoreDanger))
		return;

	if (AI_retreatToCity())
		return;

	if (AI_safety())
		return;

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

void CvUnitAI::AI_searchAndDestroyMove() {
	PROFILE_FUNC();

	if (AI_goody(4))
		return;

	if (AI_heal())
		return;

	if (AI_huntRange(1, 20, false, 10))
		return;

	if (AI_huntRange(3, 40, false, 15))
		return;

	if (AI_exploreRange(2))
		return;

	if (AI_huntRange(5, 60, false, 10))
		return;

	if (AI_exploreRange(5))
		return;

	if (AI_patrol())
		return;

	if (AI_retreatToCity())
		return;

	if (AI_safety())
		return;

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

void CvUnitAI::AI_cityDefence() {

	if (AI_returnToBorders())
		return;

	if (AI_guardCityBestDefender())
		return;

	if (AI_guardCityMinDefender(false))
		return;

	if (AI_leaveAttack(2, 50, 100))
		return;

	if (AI_leaveAttack(3, 55, 130))
		return;

	if (AI_guardCity())
		return;

	if (AI_heal())
		return;

	if (AI_retreatToCity())
		return;

	if (AI_safety())
		return;

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

bool CvUnitAI::AI_returnToBorders() {
	PROFILE_FUNC();

	//Allows the unit to be a maximum of 2 tiles from our borders before ordering him back
	if (plot()->getOwnerINLINE() == getOwnerINLINE())
		return false;

	for (DirectionTypes eDirection = (DirectionTypes)0; eDirection < NUM_DIRECTION_TYPES; eDirection = (DirectionTypes)(eDirection + 1)) {
		CvPlot* pAdjacentPlot = plotDirection(getX_INLINE(), getY_INLINE(), eDirection);
		if (pAdjacentPlot != NULL) {
			if (pAdjacentPlot->getOwnerINLINE() == getOwnerINLINE()) {
				return false;
			}
			CvPlot* pAdjacentPlot2;
			for (DirectionTypes eInnerDirection = (DirectionTypes)0; eInnerDirection < NUM_DIRECTION_TYPES; eInnerDirection = (DirectionTypes)(eInnerDirection + 1)) {
				pAdjacentPlot2 = plotDirection(pAdjacentPlot->getX_INLINE(), pAdjacentPlot->getY_INLINE(), eInnerDirection);
				if (pAdjacentPlot2 != NULL) {
					if (pAdjacentPlot2->getOwnerINLINE() == getOwnerINLINE()) {
						return false;
					}
				}
			}
		}
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);
		if (AI_plotValid(pLoopPlot)) {
			if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE()) {
				if (!pLoopPlot->isVisibleEnemyUnit(this)) {

					int iPathTurns;
					if (generatePath(pLoopPlot, 0, true, &iPathTurns)) {
						int iValue = 1000;
						iValue /= (iPathTurns + 1);

						if (iValue > iBestValue) {
							iBestValue = iValue;
							pBestPlot = getPathEndTurnPlot();
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}

void CvUnitAI::AI_borderPatrol() {
	PROFILE_FUNC();

	bool bStayInBorders = getOptionBOOL("Automations__StayInBorders", false);

	if (AI_returnToBorders()) {
		return;
	}

	if (AI_heal()) {
		return;
	}

	if (AI_huntRange(1, 35, bStayInBorders)) {
		return;
	}

	if (AI_huntRange(2, 40, true)) {
		return;
	}

	if (AI_huntRange(2, 50, bStayInBorders)) {
		return;
	}

	if (AI_huntRange(4, 50, true)) {
		return;
	}

	if (AI_huntRange(6, 60, true)) {
		return;
	}

	if (AI_patrolBorders()) {
		return;
	}

	if (AI_huntRange(10, 60, true)) {
		return;
	}

	if (!bStayInBorders) {
		if (AI_patrol()) {
			return;
		}
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

//AI_patrolBorders relys heavily on the units current facing direction to determine where the next
//move should be. For example, units facing north should not turn around south, or any southerly
//direction (southwest, southeast) to patrol, since that would cause them to oscilate. Instead, 
//they should appear to be intelligent, and move around the players borders in a circuit, without
//turning back or leaving the boundries of the cultural borders. This is not in fact the most optimal
//method of patroling, but it produces results that make sense to the average human, which is the actual goal,
//since the AI actually never use this function, only automated human units do.
bool CvUnitAI::AI_patrolBorders() {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;

	int iSearchRange = baseMoves();

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (canMoveInto(pLoopPlot, false, false, true)) {
					DirectionTypes eNewDirection = estimateDirection(plot(), pLoopPlot);
					int iValue = GC.getGameINLINE().getSorenRandNum(10000, "AI Border Patrol");
					if (pLoopPlot->isBorder(true)) {
						iValue += GC.getGameINLINE().getSorenRandNum(10000, "AI Border Patrol");
					} else if (pLoopPlot->isBorder(false)) {
						iValue += GC.getGameINLINE().getSorenRandNum(5000, "AI Border Patrol");
					}
					//Avoid heading backwards, we want to circuit our borders, if possible.
					if (eNewDirection == getOppositeDirection(getFacingDirection(false))) {
						iValue /= 25;
					} else if (isAdjacentDirection(getOppositeDirection(getFacingDirection(false)), eNewDirection)) {
						iValue /= 10;
					}
					if (pLoopPlot->getOwnerINLINE() != getOwnerINLINE()) {
						if (getOptionBOOL("Automations__StayInBorders", false)) {
							iValue = -1;
						} else {
							iValue /= 10;
						}
					}
					if (getDomainType() == DOMAIN_LAND && pLoopPlot->isWater() || getDomainType() == DOMAIN_SEA && !pLoopPlot->isWater()) {
						iValue /= 10;
					}
					if (iValue > iBestValue) {
						iBestValue = iValue;
						pBestPlot = pLoopPlot;
					}
				}
			}
		}
	}
	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}

bool CvUnitAI::AI_caravan(bool bAnyCity) {
	PROFILE_FUNC();

	//Avoid using Great People
	if (getUnitInfo().getProductionCost() < 0)
		return false;

	int iNumCities = GET_PLAYER(getOwnerINLINE()).getNumCities();
	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestHurryPlot = NULL;

	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if ((pLoopCity->area() == area()) && AI_plotValid(pLoopCity->plot())) {
			if (canHurry(pLoopCity->plot())) {
				if (!(pLoopCity->plot()->isVisibleEnemyUnit(this))) {
					int iPathTurns;
					if (generatePath(pLoopCity->plot(), MOVE_SAFE_TERRITORY, true, &iPathTurns)) {
						bool bCandidate = false;

						if (!bAnyCity) {
							// Only check cities where the population is below 2/3 the average
							if (pLoopCity->findPopulationRank() >= ((iNumCities * 2) / 3)) {
								int iPopulation = pLoopCity->getPopulation();
								int iEmpirePop = GET_PLAYER(getOwnerINLINE()).getTotalPopulation();
								int iAvgPop = iEmpirePop / iNumCities;
								if (iPopulation < ((iAvgPop * 2) / 3)) {
									bCandidate = true;
								}
							}
						}

						if (bCandidate || bAnyCity) {
							int iTurnsLeft = pLoopCity->getProductionTurnsLeft();

							iTurnsLeft -= iPathTurns;
							int iMinTurns = 2;
							iMinTurns *= GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getAnarchyPercent();
							iMinTurns /= 100;
							// Best city is the one that will have its production turns reduced the most
							if (iTurnsLeft > iMinTurns) {
								int iValue = iTurnsLeft;

								if (iValue > iBestValue) {
									iBestValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									pBestHurryPlot = pLoopCity->plot();
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestHurryPlot != NULL)) {
		if (atPlot(pBestHurryPlot)) {
			getGroup()->pushMission(MISSION_HURRY);
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_HURRY, pBestHurryPlot);
			return true;
		}
	}

	return false;
}

void CvUnitAI::AI_autoAirStrike() {
	PROFILE_FUNC();

	//Heal
	if (getDamage() > 0) {
		if ((100 * currHitPoints() / maxHitPoints()) < 50) {
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
	}

	// Attack the invaders!
	if (AI_defendBaseAirStrike())
		return;

	//Attack enemies in range
	if (AI_defensiveAirStrike())
		return;

	//Attack anyone
	if (AI_airStrike())
		return;

	if (getOptionBOOL("Automations__AirRebaseUnits")) {
		// If no targets, no sense staying in risky place
		if (AI_airOffensiveCity())
			return;

		if (canAirDefend()) {
			if (AI_airDefensiveCity())
				return;
		}

		if (healTurns(plot()) > 1) {
			// If very damaged, no sense staying in risky place
			if (AI_airOffensiveCity())
				return;

			if (canAirDefend()) {
				if (AI_airDefensiveCity())
					return;
			}
		}
	}

	CvArea* pArea = area();
	if (getOptionBOOL("Automations__AirCanDefend")) {
		CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());
		int iAttackValue = kPlayer.AI_unitValue(this, UNITAI_ATTACK_AIR, pArea);
		int iDefenseValue = kPlayer.AI_unitValue(this, UNITAI_DEFENSE_AIR, pArea);
		if (iDefenseValue > iAttackValue) {
			if (kPlayer.AI_bestAreaUnitAIValue(UNITAI_ATTACK_AIR, pArea) > iAttackValue) {
				AI_setUnitAIType(UNITAI_DEFENSE_AIR);
				getGroup()->pushMission(MISSION_SKIP);
				return;
			}
		}
	}


	bool bDefensive = false;
	if (pArea != NULL) {
		bDefensive = pArea->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE;
	}
	if (getOptionBOOL("Automations__AirRebaseUnits") && getOptionBOOL("Automations__AirCanDefend")) {
		if (GC.getGameINLINE().getSorenRandNum(bDefensive ? 3 : 6, "AI Air Attack Move") == 0) {
			if (AI_defensiveAirStrike())
				return;
		}
	}

	if (getOptionBOOL("Automations__AirCanDefend")) {
		if (GC.getGameINLINE().getSorenRandNum(4, "AI Air Attack Move") == 0) {
			// only moves unit in a fort
			if (AI_travelToUpgradeCity())
				return;
		}
	}

	// Support ground attacks
	if (canAirBomb(NULL)) {
		if (AI_airBombPlots())
			return;
	}

	if (AI_airStrike())
		return;

	if (AI_airBombCities())
		return;

	if (canAirDefend() && getOptionBOOL("Automations__AirCanDefend")) {
		if (bDefensive || GC.getGameINLINE().getSorenRandNum(2, "AI Air Attack Move") == 0) {
			getGroup()->pushMission(MISSION_AIRPATROL);
			return;
		}
	}

	if (canRecon(plot())) {
		if (AI_exploreAir())
			return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

bool CvUnitAI::AI_airBombCities() {
	//PROFILE_FUNC();

	int iSearchRange = airRange();
	int iBestValue = (isSuicide() && m_pUnitInfo->getProductionCost() > 0) ? (5 * m_pUnitInfo->getProductionCost()) / 6 : 0;
	CvPlot* pBestPlot = NULL;

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (canMoveInto(pLoopPlot, true)) {
					int iValue = 0;
					int iPotentialAttackers = pLoopPlot->getNumVisibleEnemyDefenders(this);

					if (iPotentialAttackers > 0) {
						CvUnit* pDefender = pLoopPlot->getBestDefender(NO_PLAYER, getOwnerINLINE(), this, true);

						FAssert(pDefender != NULL);
						FAssert(pDefender->canDefend());

						// XXX factor in air defenses...

						int iDamage = airCombatDamage(pDefender);

						iValue = std::max(0, (std::min((pDefender->getDamage() + iDamage), airCombatLimit()) - pDefender->getDamage()));

						iValue += ((((iDamage * collateralDamage()) / 100) * std::min((pLoopPlot->getNumVisibleEnemyDefenders(this) - 1), collateralDamageMaxUnits())) / 2);

						iValue *= (3 + iPotentialAttackers);
						iValue /= 4;

						CvUnit* pInterceptor = bestInterceptor(pLoopPlot);

						if (pInterceptor != NULL) {
							int iInterceptProb = isSuicide() ? 100 : pInterceptor->currInterceptionProbability();

							iInterceptProb *= std::max(0, (100 - evasionProbability()));
							iInterceptProb /= 100;

							iValue *= std::max(0, 100 - iInterceptProb / 2);
							iValue /= 100;
						}

						if (pLoopPlot->isWater())
							iValue *= 2;

						if (pLoopPlot->isCity())
							iValue *= 2;

						if (iValue > iBestValue) {
							iBestValue = iValue;
							pBestPlot = pLoopPlot;
							FAssert(!atPlot(pBestPlot));
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}

// Air strike focused on weakening enemy stacks threatening our cities
// Returns true if a mission was pushed...
bool CvUnitAI::AI_defensiveAirStrike() {
	PROFILE_FUNC();

	int iSearchRange = airRange();
	int iBestValue = (isSuicide() && m_pUnitInfo->getProductionCost() > 0) ? (60 * m_pUnitInfo->getProductionCost()) : 0;
	CvPlot* pBestPlot = NULL;

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (canMoveInto(pLoopPlot, true)) // Only true of plots this unit can airstrike
				{
					// Only attack enemy land units near our cities
					if (pLoopPlot->isPlayerCityRadius(getOwnerINLINE()) && !pLoopPlot->isWater()) {
						CvCity* pClosestCity = GC.getMapINLINE().findCity(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), getOwnerINLINE(), getTeam(), true, false);

						if (pClosestCity != NULL) {
							// City and pLoopPlot forced to be in same area, check they're still close
							int iStepDist = plotDistance(pClosestCity->getX_INLINE(), pClosestCity->getY_INLINE(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE());

							if (iStepDist < 3) {
								int iValue = 0;

								CvUnit* pDefender = pLoopPlot->getBestDefender(NO_PLAYER, getOwnerINLINE(), this, true);

								FAssert(pDefender != NULL);
								FAssert(pDefender->canDefend());

								int iDamage = airCombatDamage(pDefender);

								iValue = std::max(0, (std::min((pDefender->getDamage() + iDamage), airCombatLimit()) - pDefender->getDamage()));

								iValue += ((((iDamage * collateralDamage()) / 100) * std::min((pLoopPlot->getNumVisibleEnemyDefenders(this) - 1), collateralDamageMaxUnits())) / 2);

								iValue *= GET_PLAYER(getOwnerINLINE()).AI_localAttackStrength(pClosestCity->plot(), NO_TEAM);
								iValue /= std::max(1, GET_PLAYER(getOwnerINLINE()).AI_localAttackStrength(pClosestCity->plot(), getTeam()));

								if (iStepDist == 1) {
									iValue *= 5;
									iValue /= 4;
								}

								CvUnit* pInterceptor = bestInterceptor(pLoopPlot);

								if (pInterceptor != NULL) {
									int iInterceptProb = isSuicide() ? 100 : pInterceptor->currInterceptionProbability();

									iInterceptProb *= std::max(0, (100 - evasionProbability()));
									iInterceptProb /= 100;

									iValue *= std::max(0, 100 - iInterceptProb / 2);
									iValue /= 100;
								}

								if (iValue > iBestValue) {
									iBestValue = iValue;
									pBestPlot = pLoopPlot;
									FAssert(!atPlot(pBestPlot));
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}

bool CvUnitAI::AI_becomeSlaver() {
	CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());
	CvArea* pArea = area();
	bool bLandWar = kPlayer.AI_isLandWar(pArea);
	bool bAssaultAssist = (pArea->getAreaAIType(getTeam()) == AREAAI_ASSAULT_ASSIST);
	bool bAssault = bAssaultAssist || (pArea->getAreaAIType(getTeam()) == AREAAI_ASSAULT) || (pArea->getAreaAIType(getTeam()) == AREAAI_ASSAULT_MASSING);

	if (kPlayer.AI_totalAreaUnitAIs(pArea, UNITAI_SLAVER) < (kPlayer.AI_neededSlavers(pArea, (bLandWar || bAssault)))) {
		getGroup()->pushMission(MISSION_BECOME_SLAVER);
		return true;
	}

	return false;

}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_slaverExplore(int iRange) {
	PROFILE_FUNC();

	int iSearchRange = AI_searchRange(iRange);

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestExplorePlot = NULL;

	int iImpassableCount = GET_PLAYER(getOwnerINLINE()).AI_unitImpassableCount(getUnitType());

	const CvTeam& kTeam = GET_TEAM(getTeam()); // K-Mod

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			PROFILE("AI_slaverExplore 1");

			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (AI_plotValid(pLoopPlot)) {
					int iValue = 0;

					if (pLoopPlot->isRevealedGoody(getTeam())) {
						iValue += 100000;
					}

					if (!(pLoopPlot->isRevealed(getTeam(), false))) {
						iValue += 10000;
					}

					// Try to meet teams that we have seen through map trading
					if (pLoopPlot->getRevealedOwner(kTeam.getID(), false) != NO_PLAYER && !kTeam.isHasMet(pLoopPlot->getRevealedTeam(kTeam.getID(), false)))
						iValue += 1000;

					for (DirectionTypes eDirection = (DirectionTypes)0; eDirection < NUM_DIRECTION_TYPES; eDirection = (DirectionTypes)(eDirection + 1)) {
						PROFILE("AI_exploreRange 2");

						CvPlot* pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), eDirection);

						if (pAdjacentPlot != NULL) {
							if (!(pAdjacentPlot->isRevealed(getTeam(), false))) {
								iValue += 1000;
							}
						}
					}

					// Enemy plots gets priority
					if (pLoopPlot->isOwned()) {
						if (atWar(GET_PLAYER(pLoopPlot->getOwnerINLINE()).getTeam(), getTeam())) {
							iValue += 20000;
						} else if (pLoopPlot->getOwnerINLINE() != getOwnerINLINE()) {
							iValue += 15000;
						}
					}

					if (iValue > 0) {
						if (!(pLoopPlot->isVisibleEnemyUnit(this))) {
							PROFILE("AI_exploreRange 3");

							int iPathTurns;
							if (!atPlot(pLoopPlot) && generatePath(pLoopPlot, MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns, iRange)) {
								if (iPathTurns <= iRange) {
									iValue += GC.getGameINLINE().getSorenRandNum(10000, "AI Explore");

									if (pLoopPlot->isAdjacentToLand()) {
										iValue += 10000;
									}

									if (pLoopPlot->isOwned()) {
										iValue += 5000;
									}

									if (iValue > iBestValue) {
										iBestValue = iValue;
										if (getDomainType() == DOMAIN_LAND) {
											pBestPlot = getPathEndTurnPlot();
										} else {
											pBestPlot = pLoopPlot;
										}
										pBestExplorePlot = pLoopPlot;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestExplorePlot != NULL)) {
		PROFILE("AI_exploreRange 5");

		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY, false, false, MISSIONAI_EXPLORE, pBestExplorePlot);
		return true;
	}

	return false;
}

void CvUnitAI::AI_gathererMove() {
	PROFILE_FUNC();

	bool bCanRoute = canBuildRoute();
	bool bNextCity = false;

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());

	// XXX could be trouble...
	if (plot()->getOwnerINLINE() != getOwnerINLINE()) {
		if (AI_retreatToCity()) {
			return;
		}
	}

	if (!isHuman()) {
		if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, 2, -1, -1, 0, MOVE_SAFE_TERRITORY)) {
				return;
			}
		}
	}

	if (!(getGroup()->canDefend())) {
		if (kOwner.AI_isPlotThreatened(plot(), 2)) {
			if (AI_retreatToCity()) // XXX maybe not do this??? could be working productively somewhere else...
			{
				return;
			}
		}
	}

	if (AI_improveBonus()) // K-Mod
	{
		return;
	}

	if (bCanRoute && !isBarbarian()) {
		if (AI_connectCity()) {
			return;
		}
	}

	if (bCanRoute) {
		if (plot()->getOwnerINLINE() == getOwnerINLINE()) // XXX team???
		{
			BonusTypes eNonObsoleteBonus = plot()->getNonObsoleteBonusType(getTeam());
			if (NO_BONUS != eNonObsoleteBonus) {
				if (!(plot()->isConnectedToCapital())) {
					ImprovementTypes eImprovement = plot()->getImprovementType();
					if (kOwner.doesImprovementConnectBonus(eImprovement, eNonObsoleteBonus)) {
						if (AI_connectPlot(plot())) {
							return;
						}
					}
				}
			}
		}
	}

	CvCity* pCity = NULL;
	if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
		pCity = plot()->getPlotCity();
		if (pCity == NULL) {
			pCity = plot()->getWorkingCity();
		}
	}

	if (pCity != NULL) {
		// K-Mod. Note: this worker is currently at pCity, and so we're probably counted in AI_getWorkersHave.
		if (pCity->AI_getWorkersNeeded() > 0 && (plot()->isCity() || pCity->AI_getWorkersHave() - 1 <= (1 + pCity->AI_getWorkersNeeded() * 2) / 3)) {
			if (AI_improveCity(pCity)) {
				return;
			}
		}
	}

	if (bCanRoute && isBarbarian()) {
		if (AI_connectCity()) {
			return;
		}
	}

	if (getRangeType() > UNITRANGE_HOME) {
		if ((pCity == NULL) || (pCity->AI_getWorkersNeeded() == 0) || ((pCity->AI_getWorkersHave() > (pCity->AI_getWorkersNeeded() + 1)))) {
			if (AI_nextCityToImprove(pCity)) {
				return;
			}

			bNextCity = true;
		}
	}

	if (pCity != NULL) {
		if (AI_improveCity(pCity)) {
			return;
		}
	}
	// K-Mod. (moved from higher up)
	if (AI_improveLocalPlot(2, pCity))
		return;
	//

	if (getRangeType() > UNITRANGE_HOME) {
		if (!bNextCity) {
			if (AI_nextCityToImprove(pCity)) {
				return;
			}
		}
	}

	if (bCanRoute) {
		if (AI_routeTerritory(true)) {
			return;
		}

		if (AI_connectBonus(false)) {
			return;
		}

		if (AI_routeCity()) {
			return;
		}
	}

	if (AI_irrigateTerritory()) {
		return;
	}

	if (bCanRoute) {
		if (AI_routeTerritory()) {
			return;
		}
	}

	if (!isHuman() || (isAutomated() && GET_TEAM(getTeam()).getAtWarCount(true) == 0)) {
		if (!isHuman() || (getGameTurnCreated() < GC.getGame().getGameTurn())) {
			if (AI_nextCityToImproveAirlift()) {
				return;
			}
		}
		if (!isHuman()) {
			// Fill up boats which already have workers
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_WORKER, -1, -1, -1, -1, MOVE_SAFE_TERRITORY)) {
				return;
			}

			// Avoid filling a galley which has just a settler in it, reduce chances for other ships
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, NO_UNITAI, -1, 2, -1, -1, MOVE_SAFE_TERRITORY)) {
				return;
			}
		}
	}

	if (AI_improveLocalPlot(3, NULL)) {
		return;
	}

	if (!(isHuman()) && (AI_getUnitAIType() == UNITAI_WORKER)) {
		if (GC.getGameINLINE().getElapsedGameTurns() > GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getResearchPercent() / 6) {
			if (kOwner.AI_totalUnitAIs(UNITAI_WORKER) > std::max(GC.getWorldInfo(GC.getMapINLINE().getWorldSize()).getTargetNumCities(), kOwner.getNumCities() * 3 / 2) &&
				area()->getNumAIUnits(getOwnerINLINE(), UNITAI_WORKER) > kOwner.AI_neededWorkers(area()) * 3 / 2) {
				if (kOwner.calculateUnitCost() > 0) {
					scrap();
					return;
				}
			}
		}
	}

	if (AI_retreatToCity(false, true)) {
		return;
	}

	if (AI_handleStranded())
		return;

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

bool CvUnitAI::AI_salvageAnimalRange(int iRange, int iOddsThreshold) {
	PROFILE_FUNC();

	iRange = AI_searchRange(iRange);

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	for (int iDX = -iRange; iDX <= iRange; iDX++) {
		for (int iDY = -iRange; iDY <= iRange; iDY++) {
			bool bFoundTarget = false;
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot == NULL || pLoopPlot == plot() || !AI_plotValid(pLoopPlot) || pLoopPlot->isCity())
				continue;

			int iPathTurns;
			if (!generatePath(pLoopPlot, 0, true, &iPathTurns) || (iPathTurns > iRange))
				continue;

			// Use weighting to pick the easiest target
			int iValueWeighting = 100;
			if (pLoopPlot->isVisibleAnimalEnemy(this)) {
				int iWeightedOdds = AI_getWeightedOdds(pLoopPlot, true);
				if (iWeightedOdds < iOddsThreshold)
					continue;

				iValueWeighting += iWeightedOdds - iOddsThreshold;
				bFoundTarget = true;
			}

			// We always want to pop goodies!
			if (pLoopPlot->isRevealedGoody()) {
				iValueWeighting += 100000;
				bFoundTarget = true;
			}

			if (!bFoundTarget)
				continue;

			int iValue = (1 + GC.getGameINLINE().getSorenRandNum(10000, "AI Salvage"));
			iValue *= iValueWeighting;
			iValue /= 100;

			if (iValue > iBestValue) {
				iBestValue = iValue;
				pBestPlot = getPathEndTurnPlot();
			}
		}
	}

	if (pBestPlot != NULL) {
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;

}

// The primary goal of a hunter is to find and kill animal units, avoiding barbarians and enemy players
void CvUnitAI::AI_hunterMove() {
	PROFILE_FUNC();

	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());
	bool bAnyDanger = kOwner.AI_getAnyPlotDanger(plot(), 3);

	if (AI_heal(50)) {
		return;
	}

	if (AI_salvageAnimalRange(3, 90)) {
		return;
	}

	// Take a stim pack
	if (!bAnyDanger) {
		if (AI_heal(30, 3)) {
			return;
		}
	}

	if (AI_salvageAnimalRange(3, 80)) {
		return;
	}

	if (AI_salvageAnimalRange(3, 70)) {
		return;
	}

	if (AI_travelToUpgradeCity()) {
		return;
	}

	// Lets heal up before we start looking for higher risk targets
	if (AI_heal(30)) {
		return;
	}

	if (AI_salvageAnimalRange(3, 50)) {
		return;
	}

	if (AI_salvageAnimalRange(3, 40)) {
		return;
	}

	// Lets just go wandering
	// It will be rare when this routine does not find a valid plot
	if (AI_hunterExplore(3)) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

void CvUnitAI::AI_barbarianLeaderMove() {
	PROFILE_FUNC();

	if (AI_reinforceMilitary()) {
		return;
	}

	if (AI_plunderCity()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

bool CvUnitAI::AI_reinforceMilitary() {
	PROFILE_FUNC();

	// TODO: Really need to have some calculation based on gold reserves and unit costs rather than a random 50/50 toss up
	CvCity* pCity = plot()->getPlotCity();
	int iDecision = 1 + GC.getGameINLINE().getSorenRandNum(100, "Barbarian Leader Mission");
	if (iDecision > 50) {
		if (pCity != NULL && pCity->getOwnerINLINE() == getOwnerINLINE()) {
			getGroup()->pushMission(MISSION_FREE_UNIT_SUPPORT);
			return true;
		}
	}

	bool bCanPlunder = false;
	for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_CIV_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
		const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
		if (kPlayer.isAlive()) {
			if (kPlayer.getTeam() != getTeam()) {
				if (GET_TEAM(getTeam()).isHasMet(kPlayer.getTeam())) {
					if (AI_plotValid(kPlayer.getCapitalCity()->plot())) {
						bCanPlunder = true;
						break;
					}
				}
			}
		}
	}

	if (!bCanPlunder) {
		getGroup()->pushMission(MISSION_FREE_UNIT_SUPPORT);
		return true;
	}

	return false;
}

bool CvUnitAI::AI_plunderCity() {
	PROFILE_FUNC();

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvCity* pBestCity = NULL;

	for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_CIV_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
		const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
		if (kPlayer.isAlive() && kPlayer.getTeam() != getTeam()) {
			if (kPlayer.AI_getAttitude(ePlayer) <= ATTITUDE_ANNOYED) {
				int iLoop;
				for (CvCity* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop)) {
					if (AI_plotValid(pLoopCity->plot())) {
						if (generatePath(pLoopCity->plot(), 0, true)) {

							int iValue = (pLoopCity->getPopulation() * 2);
							iValue += pLoopCity->getYieldRate(YIELD_PRODUCTION);

							if (atPlot(pLoopCity->plot())) {
								iValue *= 4;
								iValue /= 3;
							}

							// AI prefers to Plunder human tribes ...
							if (kPlayer.isHuman()) {
								iValue *= 3;
								iValue /= 2;
							}

							// AI preferes to Plunder tribes which it is at war with ...
							if (GET_TEAM(getTeam()).isAtWar(kPlayer.getTeam())) {
								iValue *= 3;
								iValue /= 2;
							}

							if (iValue > iBestValue) {
								iBestValue = iValue;
								pBestPlot = getPathEndTurnPlot();
								pBestCity = pLoopCity;
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != NULL) && (pBestCity != NULL)) {
		if (atPlot(pBestCity->plot())) {
			if (canPlunderCity(pBestCity->plot())) {
				if (pBestCity->getPopulation() > 4) {
					getGroup()->pushMission(MISSION_PLUNDER_CITY);
					return true;
				}
				if (pBestCity->getProduction() > ((pBestCity->getProductionNeeded() * 3) / 5)) {
					if (pBestCity->isProductionUnit()) {
						if (isLimitedUnitClass((UnitClassTypes)(GC.getUnitInfo(pBestCity->getProductionUnit()).getUnitClassType()))) {
							getGroup()->pushMission(MISSION_PLUNDER_CITY);
							return true;
						}
					} else if (pBestCity->isProductionBuilding()) {
						if (isLimitedWonderClass((BuildingClassTypes)(GC.getBuildingInfo(pBestCity->getProductionBuilding()).getBuildingClassType()))) {
							getGroup()->pushMission(MISSION_PLUNDER_CITY);
							return true;
						}
					} else if (pBestCity->isProductionProject()) {
						if (isLimitedProject(pBestCity->getProductionProject())) {
							getGroup()->pushMission(MISSION_PLUNDER_CITY);
							return true;
						}
					}
				}
			}

			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_ATTACK_SPY, pBestCity->plot());
			return true;
		}
	}

	return false;
}

void CvUnitAI::AI_barbarianMove() {
	PROFILE_FUNC();
	const CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE()); // K-Mod

	bool bDanger = (kOwner.AI_getAnyPlotDanger(plot(), 3));
	bool bLandWar = kOwner.AI_isLandWar(area()); // K-Mod

	// K-Mod note. We'll split the group up later if we need to. (bbai group splitting code deleted.)
	FAssert(getGroup()->countNumUnitAIType(UNITAI_ATTACK_CITY) == 0); // K-Mod. (I'm pretty sure this can't happen.)

	// If we are under 50% health take a stim pack
	if (AI_heal(50)) {
		return;
	}

	// Attack choking units
	// K-Mod (bbai code deleted)
	if (plot()->getTeam() == getTeam() && (bDanger || area()->getAreaAIType(getTeam()) != AREAAI_NEUTRAL)) {
		if (bDanger && plot()->isCity()) {
			if (AI_leaveAttack(2, 55, 105))
				return;
		} else {
			if (AI_defendTeritory(70, 0, 2, true))
				return;
		}
	}

	// If we are on a spawning improvement pillage it
	if (plot()->getImprovementType() != NO_IMPROVEMENT) {
		CvImprovementInfo& kImprovement = GC.getImprovementInfo(plot()->getImprovementType());
		if (kImprovement.getAnimalSpawnRatePercentage() > 0 || kImprovement.getBarbarianSpawnRatePercentage() > 0) {
			getGroup()->pushMission(MISSION_PILLAGE, -1, -1, 0, false, false, MISSIONAI_PILLAGE, plot());
			return;
		}
	}

	{
		PROFILE("CvUnitAI::AI_attackMove() 1");

		// Guard a city we're in if it needs it
		if (AI_guardCity(true)) {
			return;
		}

		if (AI_heal(30, 1)) {
			return;
		}

		//join any city attacks in progress
		if (isEnemy(plot()->getTeam())) {
			if (AI_omniGroup(UNITAI_ATTACK_CITY, -1, -1, true, 0, 2, true, false)) {
				return;
			}
		}

		AreaAITypes eAreaAIType = area()->getAreaAIType(getTeam());
		if (plot()->isCity()) {
			if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
				if ((eAreaAIType == AREAAI_ASSAULT) || (eAreaAIType == AREAAI_ASSAULT_ASSIST)) {
					if (AI_offensiveAirlift()) {
						return;
					}
				}
			}
		}

		if (bDanger) {
			if (getGroup()->getNumUnits() > 1 && AI_stackVsStack(3, 110, 65, 0))
				return;

			if (collateralDamage() > 0) {
				if (AI_anyAttack(1, 45, 0, 3)) {
					return;
				}
			}
		}
		// K-Mod (moved from below, and replacing the disabled stuff above)
		if (AI_anyAttack(1, 70)) {
			return;
		}

		if (!noDefensiveBonus()) {
			if (AI_guardCity(false, false)) {
				return;
			}
		}

		if (!bDanger) {
			if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
				bool bAssault = ((eAreaAIType == AREAAI_ASSAULT) || (eAreaAIType == AREAAI_ASSAULT_MASSING) || (eAreaAIType == AREAAI_ASSAULT_ASSIST));
				if (bAssault) {
					if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK_CITY, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 4)) {
						return;
					}
				}

				if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, -1, -1, -1, 1, MOVE_SAFE_TERRITORY, 3)) {
					return;
				}

				if (!bLandWar) {
					// Fill transports before starting new one, but not just full of our unit ai
					if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, 1, -1, -1, 1, MOVE_SAFE_TERRITORY, 4)) {
						return;
					}

					// Pick new transport which has space for other unit ai types to join
					if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, -1, 2, -1, -1, MOVE_SAFE_TERRITORY, 4)) {
						return;
					}
				}

				if (kOwner.AI_unitTargetMissionAIs(this, MISSIONAI_GROUP) > 0) {
					getGroup()->pushMission(MISSION_SKIP);
					return;
				}
			}
		}

		// Allow larger groups if outside territory
		if (getGroup()->getNumUnits() < 3) {
			if (plot()->isOwned() && GET_TEAM(getTeam()).isAtWar(plot()->getTeam())) {
				if (AI_omniGroup(UNITAI_ATTACK, 3, -1, false, 0, 1, true, false, true, false, false)) {
					return;
				}
			}
		}

		if (AI_goody(3)) {
			return;
		}
	}

	{
		PROFILE("CvUnitAI::AI_attackMove() 2");

		if (bDanger) {
			// K-Mod. This block has been rewriten. (original code deleted)

			// slightly more reckless than last time
			if (getGroup()->getNumUnits() > 1 && AI_stackVsStack(3, 90, 40, 0))
				return;

			bool bAggressive = area()->getAreaAIType(getTeam()) != AREAAI_DEFENSIVE || getGroup()->getNumUnits() > 1 || plot()->getTeam() != getTeam();

			if (bAggressive && AI_pillageRange(1, 10))
				return;

			if (plot()->getTeam() == getTeam()) {
				if (AI_defendTeritory(55, 0, 2, true)) {
					return;
				}
			} else if (AI_anyAttack(1, 45)) {
				return;
			}

			if (bAggressive && AI_pillageRange(3, 10)) {
				return;
			}

			if (getGroup()->getNumUnits() < 4 && isEnemy(plot()->getTeam())) {
				if (AI_choke(1)) {
					return;
				}
			}

			if (bAggressive && AI_anyAttack(3, 40))
				return;
		}

		if (!isEnemy(plot()->getTeam())) {
			if (AI_heal()) {
				return;
			}
		}

		if (!plot()->isCity() || plot()->plotCount(PUF_isUnitAIType, UNITAI_CITY_DEFENSE, -1, getOwnerINLINE()) > 0) // K-Mod
		{
			// BBAI TODO: If we're fast, maybe shadow an attack city stack and pillage off of it

			bool bIgnoreFaster = false;
			if (kOwner.AI_isDoStrategy(AI_STRATEGY_LAND_BLITZ)) {
				if (area()->getAreaAIType(getTeam()) != AREAAI_ASSAULT) {
					bIgnoreFaster = true;
				}
			}

			bool bAttackCity = bLandWar && (area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE || (AI_getBirthmark() + GC.getGameINLINE().getGameTurn() / 8) % 5 <= 1);
			if (bAttackCity) {
				// strong merge strategy
				if (AI_omniGroup(UNITAI_ATTACK_CITY, -1, -1, true, 0, 5, true, getGroup()->getNumUnits() < 2, bIgnoreFaster, false, false))
					return;
			} else {
				// weak merge strategy
				if (AI_omniGroup(UNITAI_ATTACK_CITY, -1, 2, true, 0, 5, true, false, bIgnoreFaster, false, false))
					return;
			}

			if (AI_omniGroup(UNITAI_ATTACK, 2, -1, false, 0, 4, true, true, true, true, false)) {
				return;
			}

			if (AI_omniGroup(UNITAI_ATTACK, 1, 1, false, 0, 1, true, true, false, false, false)) {
				return;
			}

			// K-Mod. If we're feeling aggressive, then try to get closer to the enemy.
			if (bAttackCity && getGroup()->getNumUnits() > 1) {
				if (AI_goToTargetCity(0, 12))
					return;
			}
		}

		if (AI_guardCity(false, true, 3)) {
			return;
		}

		if ((kOwner.getNumCities() > 1) && (getGroup()->getNumUnits() == 1)) {
			if (area()->getAreaAIType(getTeam()) != AREAAI_DEFENSIVE) {
				if (area()->getNumUnrevealedTiles(getTeam()) > 0) {
					if (kOwner.AI_areaMissionAIs(area(), MISSIONAI_EXPLORE, getGroup()) < (kOwner.AI_neededExplorers(area()) + 1)) {
						if (AI_exploreRange(3)) {
							return;
						}

						if (AI_explore()) {
							return;
						}
					}
				}
			}
		}

		if (AI_defendTeritory(45, 0, 7)) // K-Mod
		{
			return;
		}

		if (AI_offensiveAirlift()) {
			return;
		}

		if (!bDanger && (area()->getAreaAIType(getTeam()) != AREAAI_DEFENSIVE)) {
			if (plot()->getOwnerINLINE() == getOwnerINLINE()) {
				if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, 1, -1, -1, 1, MOVE_SAFE_TERRITORY, 4)) {
					return;
				}

				if ((GET_TEAM(getTeam()).getAtWarCount(true) > 0) && !(getGroup()->isHasPathToAreaEnemyCity())) {
					if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 4)) {
						return;
					}
				}
			}
		}

		if (getGroup()->getNumUnits() >= 4 && plot()->getTeam() == getTeam()) {
			CvSelectionGroup* pSplitGroup, * pRemainderGroup = NULL;
			pSplitGroup = getGroup()->splitGroup(2, 0, &pRemainderGroup);
			if (pSplitGroup)
				pSplitGroup->pushMission(MISSION_SKIP);
			if (pRemainderGroup) {
				if (pRemainderGroup->AI_isForceSeparate())
					pRemainderGroup->AI_separate();
				else
					pRemainderGroup->pushMission(MISSION_SKIP);
			}
			return;
		}

		if (AI_defend()) {
			return;
		}

		if (AI_travelToUpgradeCity()) {
			return;
		}

		if (AI_handleStranded())
			return;

		if (AI_patrol()) {
			return;
		}

		if (AI_retreatToCity()) {
			return;
		}

		if (AI_safety()) {
			return;
		}
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

bool CvUnitAI::AI_becomeBarbarian() {
	
	CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());

	// Ifd we can't create barbarians bail early
	if (!kPlayer.isCreateBarbarians())
		return false;

	// If there are no other player civs in the area we don't want barbarians as they cost more than standard units
	CvArea* pArea = area();
	if (pArea->getNumCities() <= pArea->getCitiesPerPlayer(getOwnerINLINE()) + pArea->getCitiesPerPlayer(BARBARIAN_PLAYER))
		return false;

	// If we don't have ready gold then don't convert
	int iSpareFreeBarbUnits = kPlayer.getBarbarianFreeUnits() - kPlayer.getNumBarbarians();
	if (kPlayer.AI_isFinancialTrouble() && iSpareFreeBarbUnits <= 0)
		return false;

	int iAreaEnemies = 0;
	int iAreaNeutrals = 0;
	for (PlayerTypes eLoopPlayer = (PlayerTypes)0; eLoopPlayer < MAX_CIV_PLAYERS; eLoopPlayer = (PlayerTypes)(eLoopPlayer + 1)) {
		const CvPlayer& kLoopPlayer = GET_PLAYER(eLoopPlayer);
		if (kLoopPlayer.isAlive() && (kLoopPlayer.getCapitalCity() != NULL)) {
			// If we can't get to their capital ignore them
			int iPathTurns;
			if (!generatePath(kLoopPlayer.getCapitalCity()->plot(), MOVE_IGNORE_DANGER, false, &iPathTurns, getRange()))
				continue;

			TeamTypes eLoopTeam = kLoopPlayer.getTeam();
			if (eLoopTeam != getTeam() && (pArea->getCitiesPerPlayer(eLoopPlayer) > 0) && GET_TEAM(getTeam()).isHasMet(eLoopTeam)) {
				atWar(eLoopTeam, getTeam()) ? iAreaEnemies++ : iAreaNeutrals++;
			}
		}
	}

	// If we have some spare barb slots and civs to target then convert to grab barb leaders.
	if (iSpareFreeBarbUnits > 0 && iAreaEnemies + iAreaNeutrals > 0) {
		getGroup()->pushMission(MISSION_BECOME_BARBARIAN);
		return true;
	}

	// For neutral only civs we allow 1 barbarian per city, if there are enemies allow for one extra.
	// We don't want too many due to the additional cost which is a drain on our research
	if (iAreaNeutrals && (kPlayer.getNumBarbarians() < pArea->getCitiesPerPlayer(getOwnerINLINE()))) {
		getGroup()->pushMission(MISSION_BECOME_BARBARIAN);
		return true;
	} else if (iAreaEnemies && (kPlayer.getNumBarbarians() <= pArea->getCitiesPerPlayer(getOwnerINLINE()))) {
		getGroup()->pushMission(MISSION_BECOME_BARBARIAN);
		return true;
	}

	return false;

}

// This routine id very unlikely not to find a target plot if the range is of sufficient size
bool CvUnitAI::AI_hunterExplore(int iRange) {
	PROFILE_FUNC();

	int iSearchRange = AI_searchRange(iRange);

	int iBestValue = 0;
	CvPlot* pBestPlot = NULL;
	CvPlot* pBestExplorePlot = NULL;
	std::vector<CvPlot*> vValidPlots;

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++) {
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++) {
			PROFILE("AI_wander 1");

			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (AI_plotValid(pLoopPlot)) {
					int iValue = 0;

					// We are not interested in plots in our own borders
					if (pLoopPlot->getTeam() == getTeam())
						continue;

					vValidPlots.push_back(pLoopPlot);

					if (pLoopPlot->isRevealedGoody(getTeam())) {
						iValue += 100;
					}

					// Look for unexplored plots
					for (DirectionTypes eDirection = (DirectionTypes)0; eDirection < NUM_DIRECTION_TYPES; eDirection = (DirectionTypes)(eDirection + 1)) {
						PROFILE("AI_hunterExplore 2");

						CvPlot* pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), eDirection);

						if (pAdjacentPlot != NULL) {
							if (!pAdjacentPlot->isRevealed(getTeam(), false)) {
								iValue += 10;
							}

							if (pAdjacentPlot->isVisibleAnimalEnemy(this)) {
								iValue += 8;
							}
						}
					}

					if (iValue > 0) {
						if (!pLoopPlot->isVisibleNonAnimalEnemy(this)) {
							PROFILE("AI_hunterExplore 3");

							int iPathTurns;
							if (!atPlot(pLoopPlot) && generatePath(pLoopPlot, MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns, iRange)) {
								if (iPathTurns <= iRange) {
									iValue += GC.getGameINLINE().getSorenRandNum(10000, "AI Explore");

									if (pLoopPlot->isAdjacentToLand()) {
										iValue += 10;
									}

									// We don't want ot go hunting in rival territory as we are not a military unit
									if (pLoopPlot->isOwned()) {
										iValue -= 5;
									}

									if (iValue > iBestValue) {
										iBestValue = iValue;
										if (getDomainType() == DOMAIN_LAND) {
											pBestPlot = getPathEndTurnPlot();
										} else {
											pBestPlot = pLoopPlot;
										}
										pBestExplorePlot = pLoopPlot;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// If we didn't find anything useful then pick a plot at random if we have any
	if (pBestPlot == NULL || pBestExplorePlot == NULL) {
		PROFILE("AI_hunterExplore 5");

		// The first two checks should not fire, but lets put them in for completeness
		if (pBestPlot != NULL) {
			pBestExplorePlot = pBestPlot;
		} else if (pBestExplorePlot!= NULL) {
			pBestPlot = pBestExplorePlot;
		} else if (vValidPlots.size() > 0) {
			std::random_shuffle(vValidPlots.begin(), vValidPlots.end());
			pBestPlot = vValidPlots[0];
			pBestExplorePlot = vValidPlots[0];
		}
	}

	if ((pBestPlot != NULL) && (pBestExplorePlot != NULL)) {
		PROFILE("AI_hunterExplore 5");

		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY, false, false, MISSIONAI_EXPLORE, pBestExplorePlot);
		return true;
	}

	return false;
}

void CvUnitAI::AI_barbarianAttackCityMove() {
	// We don't mind a suicidal attack on a city
	if (AI_cityAttack(3, 20)) {
		return;
	}

	if (AI_targetCity(MOVE_THROUGH_ENEMY)) {
		return;
	}

	if (AI_anyAttack(2, 10)) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
}

bool CvUnitAI::AI_targetCity(int iFlags) {
	PROFILE_FUNC();

	CvCity* pBestCity = NULL;
	CvCity* pTargetCity = area()->getTargetCity(getOwnerINLINE());
	if (pTargetCity != NULL) {
		if (AI_potentialEnemy(pTargetCity->getTeam(), pTargetCity->plot())) {
			if (!atPlot(pTargetCity->plot()) && generatePath(pTargetCity->plot(), iFlags, true)) {
				pBestCity = pTargetCity;
			}
		}
	}

	int iBestValue = 0;
	if (pBestCity == NULL) {
		for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_CIV_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
			const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
			if (kPlayer.isAlive()) {
				int iLoop;
				for (CvCity* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop)) {
					if (AI_plotValid(pLoopCity->plot()) && AI_potentialEnemy(kPlayer.getTeam(), pLoopCity->plot())) {
						int iPathTurns;
						if (!atPlot(pLoopCity->plot()) && generatePath(pLoopCity->plot(), iFlags, true, &iPathTurns)) {
							int iValue = 0;
							if (AI_getUnitAIType() == UNITAI_ATTACK_CITY || AI_getUnitAIType() == UNITAI_BARBARIAN_ATTACK_CITY) {
								iValue = GET_PLAYER(getOwnerINLINE()).AI_targetCityValue(pLoopCity, false, false);
							} else {
								iValue = GET_PLAYER(getOwnerINLINE()).AI_targetCityValue(pLoopCity, true, true);
							}

							iValue *= 1000;

							if ((area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE)) {
								if (pLoopCity->calculateCulturePercent(getOwnerINLINE()) < 75) {
									iValue /= 2;
								}
							}

							 // If city is minor civ, less interesting
							if (GET_PLAYER(pLoopCity->getOwnerINLINE()).isMinorCiv()) {
								iValue /= 2;
							}

							// If stack has poor bombard, direct towards lower defense cities
							iPathTurns += std::min(16, getGroup()->getBombardTurns(pLoopCity)) / 2;
							iValue /= (4 + iPathTurns * iPathTurns);

							if (iValue > iBestValue) {
								iBestValue = iValue;
								pBestCity = pLoopCity;
							}
						}
					}
				}
			}
		}
	}

	if (pBestCity != NULL) {
		iBestValue = 0;
		CvPlot* pBestPlot = NULL;

		if (0 == (iFlags & MOVE_THROUGH_ENEMY)) {
			for (DirectionTypes eDirection = (DirectionTypes)0; eDirection < NUM_DIRECTION_TYPES; eDirection = (DirectionTypes)(eDirection + 1)) {
				CvPlot* pAdjacentPlot = plotDirection(pBestCity->getX_INLINE(), pBestCity->getY_INLINE(), eDirection);

				if (pAdjacentPlot != NULL) {
					if (AI_plotValid(pAdjacentPlot)) {
						if (!pAdjacentPlot->isVisibleEnemyUnit(this)) {
							int iPathTurns;
							if (generatePath(pAdjacentPlot, iFlags, true, &iPathTurns)) {
								int iValue = std::max(0, (pAdjacentPlot->defenseModifier(getTeam(), false) + 100));

								if (!(pAdjacentPlot->isRiverCrossing(directionXY(pAdjacentPlot, pBestCity->plot())))) {
									iValue += (12 * -(GC.getRIVER_ATTACK_MODIFIER()));
								}

								if (!isEnemy(pAdjacentPlot->getTeam(), pAdjacentPlot)) {
									iValue += 100;
								}

								iValue = std::max(1, iValue);

								iValue *= 1000;

								iValue /= (iPathTurns + 1);

								if (iValue > iBestValue) {
									iBestValue = iValue;
									pBestPlot = getPathEndTurnPlot();
								}
							}
						}
					}
				}
			}
		}


		else {
			pBestPlot = pBestCity->plot();
		}

		if (pBestPlot != NULL) {
			FAssert(!pBestCity->at(pBestPlot) || 0 != (iFlags & MOVE_THROUGH_ENEMY)); // no suicide missions...
			if (!atPlot(pBestPlot)) {
				getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), iFlags);
				return true;
			}
		}
	}

	return false;
}


void CvUnitAI::AI_jesterMove() {
	PROFILE_FUNC();

	// We look to pacify cities before the great jest as it is a non-destructive activity
	if (AI_jesterPacify()) {
		return;
	}

	if (AI_greatPersonMove()) {
		return;
	}

	if (AI_greatJest()) {
		return;
	}

	if (AI_retreatToCity()) {
		return;
	}

	if (AI_handleStranded()) {
		return;
	}

	if (AI_safety()) {
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}

// If we have 3 or more cities in the area that are unhappy go to the closest city and perform a great jest
bool CvUnitAI::AI_greatJest() {
	PROFILE_FUNC();

	CvPlot* pBestPlot = NULL;
	CvPlot* pBestGreatJestPlot = NULL;
	int iUnhappyCount = 0;
	int iClosestCityRange = MAX_INT;
	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if (AI_plotValid(pLoopCity->plot())) {
			if (pLoopCity->unhappyLevel() > pLoopCity->happyLevel()) {
				iUnhappyCount++;
			}

			int iPathTurns;
			if (generatePath(pLoopCity->plot(), MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns)) {
				if (canPerformGreatJest(pLoopCity->plot())) {
					if (!(pLoopCity->plot()->isVisibleEnemyUnit(this))) {
						if (iPathTurns < iClosestCityRange) {
							iClosestCityRange = iPathTurns;
							pBestPlot = getPathEndTurnPlot();
							pBestGreatJestPlot = pLoopCity->plot();
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL && iUnhappyCount > 2) {
		if (atPlot(pBestGreatJestPlot)) {
			getGroup()->pushMission(MISSION_GREAT_JEST);
			return true;
		} else {
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY);
			return true;
		}
	}

	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_jesterPacify() {

	int iBestTargetVal = 0;
	CvPlot* pBestPlot = NULL;

	// If we have an occupied city where the occupation time is greater than twice the time to reach it then send along the jester
	int iLoop;
	for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop)) {
		if (AI_plotValid(pLoopCity->plot())) {
			// If we are in a city that would become unhappy if we moved then stay put
			// We check for a slightly smaller population as we don't mind a bit of unhappiness as there are other ways of dealing with that
			if (atPlot(pLoopCity->plot()) && (pLoopCity->unhappyLevel(-1, true) > pLoopCity->happyLevel())) {
				return true;
			}

			// If the city has a high unhappy population then deal with it as a priority
			int iTempAnger = 0;
			if (pLoopCity->angryPopulation() > 0) {
				iTempAnger = pLoopCity->angryPopulation() * 2;
			}

			// Check for occupied cities
			int iTempOccupationTime = 0;
			if (pLoopCity->isOccupation()) {
				iTempOccupationTime = pLoopCity->getOccupationTimer();
			}

			int iPathTurns;
			if (iTempAnger > 0 || iTempOccupationTime > 0) {
				if (generatePath(pLoopCity->plot(), MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns)) {
					if (!(pLoopCity->plot()->isVisibleEnemyUnit(this))) {
						iTempAnger = std::max(0, iTempAnger - iPathTurns);
						iTempOccupationTime = std::max(0, iTempOccupationTime - iPathTurns);

						if (iTempAnger > iBestTargetVal) {
							iBestTargetVal = iTempAnger;
							pBestPlot = getPathEndTurnPlot();
						}

						if (iTempOccupationTime > iBestTargetVal) {
							iBestTargetVal = iTempOccupationTime;
							pBestPlot = getPathEndTurnPlot();
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL) {
		if (atPlot(pBestPlot)) {
			// We are already in the best plot so stay where we are
			return true;
		} else {
			// Move to the best plot
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY);
			return true;
		}
	}

	return false;
}
