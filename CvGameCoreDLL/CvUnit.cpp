// unit.cpp

#include "CvGameCoreDLL.h"
#include "CvUnit.h"
#include "CvArea.h"
#include "CvPlot.h"
#include "CvCity.h"
#include "CvGlobals.h"
#include "CvGameCoreUtils.h"
#include "CvGameAI.h"
#include "CvMap.h"
#include "CvPlayerAI.h"
#include "CvRandom.h"
#include "CvTeamAI.h"
#include "CvGameCoreUtils.h"
#include "CyUnit.h"
#include "CyArgsList.h"
#include "CyPlot.h"
#include "CvDLLEntityIFaceBase.h"
#include "CvDLLInterfaceIFaceBase.h"
#include "CvDLLEngineIFaceBase.h"
#include "CvEventReporter.h"
#include "CvDLLPythonIFaceBase.h"
#include "CvDLLFAStarIFaceBase.h"
#include "CvInfos.h"
#include "FProfiler.h"
#include "CvPopupInfo.h"
#include "CvArtFileMgr.h"

#include "BetterBTSAI.h"

// Public Functions...


CvUnit::CvUnit() {
	m_aiExtraDomainModifier = new int[NUM_DOMAIN_TYPES];

	m_pabHasPromotion = NULL;

	m_paiTerrainDoubleMoveCount = NULL;
	m_paiFeatureDoubleMoveCount = NULL;
	m_paiExtraTerrainAttackPercent = NULL;
	m_paiExtraTerrainDefensePercent = NULL;
	m_paiExtraFeatureAttackPercent = NULL;
	m_paiExtraFeatureDefensePercent = NULL;
	m_paiExtraUnitCombatModifier = NULL;
	m_paiEnslavedCount = NULL;
	m_paiSeeInvisibleCount = NULL;

	CvDLLEntity::createUnitEntity(this);		// create and attach entity to unit

	reset(0, NO_UNIT, NO_PLAYER, true);
}


CvUnit::~CvUnit() {
	if (!gDLL->GetDone() && GC.IsGraphicsInitialized())						// don't need to remove entity when the app is shutting down, or crash can occur
	{
		gDLL->getEntityIFace()->RemoveUnitFromBattle(this);
		CvDLLEntity::removeEntity();		// remove entity from engine
	}

	CvDLLEntity::destroyEntity();			// delete CvUnitEntity and detach from us

	uninit();

	SAFE_DELETE_ARRAY(m_aiExtraDomainModifier);
}

void CvUnit::reloadEntity() {
	//destroy old entity
	if (!gDLL->GetDone() && GC.IsGraphicsInitialized())						// don't need to remove entity when the app is shutting down, or crash can occur
	{
		gDLL->getEntityIFace()->RemoveUnitFromBattle(this);
		CvDLLEntity::removeEntity();		// remove entity from engine
	}

	CvDLLEntity::destroyEntity();			// delete CvUnitEntity and detach from us

	//creat new one
	CvDLLEntity::createUnitEntity(this);		// create and attach entity to unit
	setupGraphical();
}


void CvUnit::init(int iID, UnitTypes eUnit, UnitAITypes eUnitAI, PlayerTypes eOwner, int iX, int iY, DirectionTypes eFacingDirection) {
	FAssert(NO_UNIT != eUnit);

	//--------------------------------
	// Init saved data
	reset(iID, eUnit, eOwner);

	if (eFacingDirection == NO_DIRECTION)
		m_eFacingDirection = DIRECTION_SOUTH;
	else
		m_eFacingDirection = eFacingDirection;

	//--------------------------------
	// Init containers

	//--------------------------------
	// Init pre-setup() data

	// This needs to be before the setXY call as that is the one that sets the units visibility
	//  so we need to have configured the invisibles that it can see beforehand
	for (int i = 0; i < m_pUnitInfo->getNumSeeInvisibleTypes(); i++) {
		changeSeeInvisibleCount((InvisibleTypes)m_pUnitInfo->getSeeInvisibleType(i), 1);
	}

	setXY(iX, iY, false, false);

	//--------------------------------
	// Init non-saved data
	setupGraphical();

	//--------------------------------
	// Init other game data
	plot()->updateCenterUnit();

	plot()->setFlagDirty(true);

	CvPlayer& kOwner = GET_PLAYER(getOwnerINLINE());

	// If this unit is created in one of the owners cities then set that city as its home
	// otherwise set the owners closest city, favouring cities on the same landmass
	if (!m_pUnitInfo->isAnimal()) {
		if (plot()->getPlotCity() != NULL && plot()->getPlotCity()->getOwnerINLINE() == getOwnerINLINE()) {
			setHomeCity(plot()->getPlotCity());
		} else {
			setHomeCity(kOwner.findCity(getX_INLINE(), getY_INLINE(), true));
		}
	}
	int iUnitName = GC.getGameINLINE().getUnitCreatedCount(getUnitType());
	int iNumNames = m_pUnitInfo->getNumUnitNames();
	if (iUnitName < iNumNames) {
		int iOffset = GC.getGameINLINE().getSorenRandNum(iNumNames, "Unit name selection");

		for (int iI = 0; iI < iNumNames; iI++) {
			int iIndex = (iI + iOffset) % iNumNames;
			CvWString szName = gDLL->getText(m_pUnitInfo->getUnitNames(iIndex));
			if (!GC.getGameINLINE().isGreatPersonBorn(szName)) {
				setName(szName);
				GC.getGameINLINE().addGreatPersonBornName(szName);
				break;
			}
		}
	}

	setGameTurnCreated(GC.getGameINLINE().getGameTurn());

	GC.getGameINLINE().incrementUnitCreatedCount(getUnitType());

	GC.getGameINLINE().incrementUnitClassCreatedCount((UnitClassTypes)(m_pUnitInfo->getUnitClassType()));
	GET_TEAM(getTeam()).changeUnitClassCount(((UnitClassTypes)(m_pUnitInfo->getUnitClassType())), 1);
	kOwner.changeUnitClassCount(((UnitClassTypes)(m_pUnitInfo->getUnitClassType())), 1);

	kOwner.changeExtraUnitCost(m_pUnitInfo->getExtraCost());

	if (m_pUnitInfo->getNukeRange() != -1) {
		kOwner.changeNumNukeUnits(1);
	}

	if (m_pUnitInfo->isMilitarySupport()) {
		kOwner.changeNumMilitaryUnits(1);
	}

	if (isSlave()) {
		kOwner.changeNumSlaves(1);
	}

	kOwner.changeAssets(m_pUnitInfo->getAssetValue());

	kOwner.changePower(m_pUnitInfo->getPowerValue());

	for (PromotionTypes ePromotion = (PromotionTypes)0; ePromotion < GC.getNumPromotionInfos(); ePromotion = (PromotionTypes)(ePromotion + 1)) {
		if (m_pUnitInfo->getFreePromotions(ePromotion)) {
			setHasPromotion(ePromotion, true);
		}
	}

	FAssertMsg((GC.getNumTraitInfos() > 0), "GC.getNumTraitInfos() is less than or equal to zero but is expected to be larger than zero in CvUnit::init");
	for (TraitTypes eTrait = (TraitTypes)0; eTrait < GC.getNumTraitInfos(); eTrait = (TraitTypes)(eTrait + 1)) {
		if (kOwner.hasTrait(eTrait)) {
			const CvTraitInfo& kTrait = GC.getTraitInfo(eTrait);
			for (PromotionTypes ePromotion = (PromotionTypes)0; ePromotion < GC.getNumPromotionInfos(); ePromotion = (PromotionTypes)(ePromotion + 1)) {
				if (kTrait.isFreePromotion(ePromotion)) {
					if ((getUnitCombatType() != NO_UNITCOMBAT) && kTrait.isFreePromotionUnitCombat(getUnitCombatType())) {
						setHasPromotion(ePromotion, true);
					}
				}
			}
		}
	}

	if (NO_UNITCOMBAT != getUnitCombatType()) {
		for (PromotionTypes ePromotion = (PromotionTypes)0; ePromotion < GC.getNumPromotionInfos(); ePromotion = (PromotionTypes)(ePromotion + 1)) {
			if (kOwner.isFreePromotion(getUnitCombatType(), ePromotion)) {
				setHasPromotion(ePromotion, true);
			}
		}
	}

	if (NO_UNITCLASS != getUnitClassType()) {
		for (PromotionTypes ePromotion = (PromotionTypes)0; ePromotion < GC.getNumPromotionInfos(); ePromotion = (PromotionTypes)(ePromotion + 1)) {
			if (kOwner.isFreePromotion(getUnitClassType(), ePromotion)) {
				setHasPromotion(ePromotion, true);
			}
		}
	}

	if (getDomainType() == DOMAIN_LAND) {
		if (baseCombatStr() > 0) {
			if ((GC.getGameINLINE().getBestLandUnit() == NO_UNIT) || (baseCombatStr() > GC.getGameINLINE().getBestLandUnitCombat())) {
				GC.getGameINLINE().setBestLandUnit(getUnitType());
			}
		}
	}

	if (getOwnerINLINE() == GC.getGameINLINE().getActivePlayer()) {
		gDLL->getInterfaceIFace()->setDirty(GameData_DIRTY_BIT, true);
	}

	if (isWorldUnitClass((UnitClassTypes)m_pUnitInfo->getUnitClassType())) {
		CvWString szBuffer;
		for (PlayerTypes eLoopPlayer = (PlayerTypes)0; eLoopPlayer < MAX_PLAYERS; eLoopPlayer = (PlayerTypes)(eLoopPlayer + 1)) {
			const CvPlayer& kLoopPlayer = GET_PLAYER(eLoopPlayer);
			if (kLoopPlayer.isAlive()) {
				if (GET_TEAM(getTeam()).isHasMet(kLoopPlayer.getTeam())) {
					szBuffer = gDLL->getText("TXT_KEY_MISC_SOMEONE_CREATED_UNIT", kOwner.getNameKey(), getNameKey());
					gDLL->getInterfaceIFace()->addHumanMessage(eLoopPlayer, false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_WONDER_UNIT_BUILD", MESSAGE_TYPE_MAJOR_EVENT, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_UNIT_TEXT"), getX_INLINE(), getY_INLINE(), true, true);
				} else {
					szBuffer = gDLL->getText("TXT_KEY_MISC_UNKNOWN_CREATED_UNIT", getNameKey());
					gDLL->getInterfaceIFace()->addHumanMessage(eLoopPlayer, false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_WONDER_UNIT_BUILD", MESSAGE_TYPE_MAJOR_EVENT, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_UNIT_TEXT"));
				}
			}
		}

		szBuffer = gDLL->getText("TXT_KEY_MISC_SOMEONE_CREATED_UNIT", kOwner.getNameKey(), getNameKey());
		GC.getGameINLINE().addReplayMessage(REPLAY_MESSAGE_MAJOR_EVENT, getOwnerINLINE(), szBuffer, getX_INLINE(), getY_INLINE(), (ColorTypes)GC.getInfoTypeForString("COLOR_UNIT_TEXT"));
	}

	AI_init(eUnitAI);

	CvEventReporter::getInstance().unitCreated(this);
}


void CvUnit::uninit() {
	SAFE_DELETE_ARRAY(m_pabHasPromotion);

	SAFE_DELETE_ARRAY(m_paiTerrainDoubleMoveCount);
	SAFE_DELETE_ARRAY(m_paiFeatureDoubleMoveCount);
	SAFE_DELETE_ARRAY(m_paiExtraTerrainAttackPercent);
	SAFE_DELETE_ARRAY(m_paiExtraTerrainDefensePercent);
	SAFE_DELETE_ARRAY(m_paiExtraFeatureAttackPercent);
	SAFE_DELETE_ARRAY(m_paiExtraFeatureDefensePercent);
	SAFE_DELETE_ARRAY(m_paiExtraUnitCombatModifier);
	SAFE_DELETE_ARRAY(m_paiEnslavedCount);
	SAFE_DELETE_ARRAY(m_paiSeeInvisibleCount);

	if (m_pSpy != NULL)
		delete m_pSpy;

}


// FUNCTION: reset()
// Initializes data members that are serialized.
void CvUnit::reset(int iID, UnitTypes eUnit, PlayerTypes eOwner, bool bConstructorCall) {
	//--------------------------------
	// Uninit class
	uninit();

	m_iID = iID;
	m_iGroupID = FFreeList::INVALID_INDEX;
	m_iHotKeyNumber = -1;
	m_iX = INVALID_PLOT_COORD;
	m_iY = INVALID_PLOT_COORD;
	m_iLastMoveTurn = 0;
	m_iReconX = INVALID_PLOT_COORD;
	m_iReconY = INVALID_PLOT_COORD;
	m_iGameTurnCreated = 0;
	m_iDamage = 0;
	m_iMoves = 0;
	m_iExperience = 0;
	m_iLevel = 1;
	m_iCargo = 0;
	m_iAttackPlotX = INVALID_PLOT_COORD;
	m_iAttackPlotY = INVALID_PLOT_COORD;
	m_iCombatTimer = 0;
	m_iCombatFirstStrikes = 0;
	m_iFortifyTurns = 0;
	m_iBlitzCount = 0;
	m_iAmphibCount = 0;
	m_iRiverCount = 0;
	m_iEnemyRouteCount = 0;
	m_iAlwaysHealCount = 0;
	m_iHillsDoubleMoveCount = 0;
	m_iImmuneToFirstStrikesCount = 0;
	m_iExtraVisibilityRange = 0;
	m_iExtraMoves = 0;
	m_iExtraMoveDiscount = 0;
	m_iExtraAirRange = 0;
	m_iExtraIntercept = 0;
	m_iExtraEvasion = 0;
	m_iExtraFirstStrikes = 0;
	m_iExtraChanceFirstStrikes = 0;
	m_iExtraWithdrawal = 0;
	m_iExtraCollateralDamage = 0;
	m_iExtraBombardRate = 0;
	m_iExtraEnemyHeal = 0;
	m_iExtraNeutralHeal = 0;
	m_iExtraFriendlyHeal = 0;
	m_iSameTileHeal = 0;
	m_iAdjacentTileHeal = 0;
	m_iExtraCombatPercent = 0;
	m_iExtraCityAttackPercent = 0;
	m_iExtraCityDefensePercent = 0;
	m_iExtraHillsAttackPercent = 0;
	m_iExtraHillsDefensePercent = 0;
	m_iRevoltProtection = 0;
	m_iCollateralDamageProtection = 0;
	m_iPillageChange = 0;
	m_iUpgradeDiscount = 0;
	m_iExperiencePercent = 0;
	m_iKamikazePercent = 0;
	m_eFacingDirection = DIRECTION_SOUTH;
	m_iImmobileTimer = 0;
	m_iExtraRange = 0;
	m_iExtraRangeModifier = 0;
	m_iRangeUnboundCount = 0;
	m_iTerritoryUnboundCount = 0;
	m_iCanMovePeaksCount = 0;
	m_iMaxSlaves = 0;
	m_iSlaveSpecialistType = (NO_UNIT != eUnit) ? ((CvUnitInfo*)&GC.getUnitInfo(eUnit))->getSlaveSpecialistType() : NO_SPECIALIST;
	m_iSlaveControlCount = 0;
	m_iLoyaltyCount = 0;
	m_iWorkRateModifier = 0;
	m_iWeaponStrength = 0;
	m_iAmmunitionStrength = 0;
	m_iSalvageModifier = 0;

	m_bMadeAttack = false;
	m_bMadeInterception = false;
	m_bPromotionReady = false;
	m_bDeathDelay = false;
	m_bCombatFocus = false;
	m_bInfoBarDirty = false;
	m_bBlockading = false;
	m_bAirCombat = false;
	m_bCivicEnabled = true;
	m_bGroupPromotionChanged = false;
	m_bWorldViewEnabled = true;
	m_bAutoPromoting = false;
	m_bAutoUpgrading = false;
	m_bImmobile = false;
	m_bFixedAI = (NO_UNIT != eUnit) ? GC.getUnitInfo(eUnit).isFixedAI() : false;
	m_bAlwaysHostile = (NO_UNIT != eUnit) ? GC.getUnitInfo(eUnit).isAlwaysHostile() : false;
	m_bHiddenNationality = (NO_UNIT != eUnit) ? GC.getUnitInfo(eUnit).isHiddenNationality() : false;

	m_eOwner = eOwner;
	m_eCapturingPlayer = NO_PLAYER;
	m_eUnitType = eUnit;
	m_eDesiredDiscoveryTech = NO_TECH;
	m_pUnitInfo = (NO_UNIT != m_eUnitType) ? &GC.getUnitInfo(m_eUnitType) : NULL;
	m_iBaseCombat = (NO_UNIT != m_eUnitType) ? m_pUnitInfo->getCombat() : 0;
	m_eLeaderUnitType = NO_UNIT;
	m_iCargoCapacity = (NO_UNIT != m_eUnitType) ? m_pUnitInfo->getCargoSpace() : 0;
	m_eUnitCombatType = (NO_UNIT != m_eUnitType) ? (UnitCombatTypes)m_pUnitInfo->getUnitCombatType() : NO_UNITCOMBAT;
	m_pSpy = (m_pUnitInfo && m_pUnitInfo->isSpy()) ? m_pSpy = new CvSpy : NULL;
	if (m_pSpy) m_pSpy->reset();
	if (m_eUnitCombatType == (UnitCombatTypes)GC.getInfoTypeForString("UNITCOMBAT_SLAVER", true)) {
		// We should have a custom unit mesh
		if (GC.getGameINLINE().isSlaverUnitMeshGroupExists(m_eUnitType))
			m_pCustomUnitMeshGroup = GC.getGameINLINE().getSlaverUnitMeshGroup(m_eUnitType);
	}
	m_eInvisible = (NO_UNIT != m_eUnitType) ? (InvisibleTypes)m_pUnitInfo->getInvisibleType() : NO_INVISIBLE;
	m_eWeaponType = NO_WEAPON;
	m_eAmmunitionType = NO_WEAPON;

	m_combatUnit.reset();
	m_transportUnit.reset();
	m_homeCity.reset();
	m_shadowUnit.reset();

	for (DomainTypes eDomain = (DomainTypes)0; eDomain < NUM_DOMAIN_TYPES; eDomain = (DomainTypes)(eDomain + 1)) {
		m_aiExtraDomainModifier[eDomain] = 0;
	}

	m_szName.clear();
	m_szScriptData = "";

	if (!bConstructorCall) {
		FAssertMsg((0 < GC.getNumPromotionInfos()), "GC.getNumPromotionInfos() is not greater than zero but an array is being allocated in CvUnit::reset");
		m_pabHasPromotion = new bool[GC.getNumPromotionInfos()];
		for (PromotionTypes ePromotion = (PromotionTypes)0; ePromotion < GC.getNumPromotionInfos(); ePromotion = (PromotionTypes)(ePromotion + 1)) {
			m_pabHasPromotion[ePromotion] = false;
		}

		FAssertMsg((0 < GC.getNumTerrainInfos()), "GC.getNumTerrainInfos() is not greater than zero but a float array is being allocated in CvUnit::reset");
		m_paiTerrainDoubleMoveCount = new int[GC.getNumTerrainInfos()];
		m_paiExtraTerrainAttackPercent = new int[GC.getNumTerrainInfos()];
		m_paiExtraTerrainDefensePercent = new int[GC.getNumTerrainInfos()];
		for (TerrainTypes eTerrain = (TerrainTypes)0; eTerrain < GC.getNumTerrainInfos(); eTerrain = (TerrainTypes)(eTerrain + 1)) {
			m_paiTerrainDoubleMoveCount[eTerrain] = 0;
			m_paiExtraTerrainAttackPercent[eTerrain] = 0;
			m_paiExtraTerrainDefensePercent[eTerrain] = 0;
		}

		FAssertMsg((0 < GC.getNumFeatureInfos()), "GC.getNumFeatureInfos() is not greater than zero but a float array is being allocated in CvUnit::reset");
		m_paiFeatureDoubleMoveCount = new int[GC.getNumFeatureInfos()];
		m_paiExtraFeatureDefensePercent = new int[GC.getNumFeatureInfos()];
		m_paiExtraFeatureAttackPercent = new int[GC.getNumFeatureInfos()];
		for (FeatureTypes eFeature = (FeatureTypes)0; eFeature < GC.getNumFeatureInfos(); eFeature = (FeatureTypes)(eFeature + 1)) {
			m_paiFeatureDoubleMoveCount[eFeature] = 0;
			m_paiExtraFeatureAttackPercent[eFeature] = 0;
			m_paiExtraFeatureDefensePercent[eFeature] = 0;
		}

		FAssertMsg((0 < GC.getNumSpecialistInfos()), "GC.getNumSpecialistInfos() is not greater than zero but an array is being allocated in CvUnit::reset");
		m_paiEnslavedCount = new int[GC.getNumSpecialistInfos()];
		for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
			m_paiEnslavedCount[eSpecialist] = 0;
		}

		FAssertMsg((0 < GC.getNumUnitCombatInfos()), "GC.getNumUnitCombatInfos() is not greater than zero but an array is being allocated in CvUnit::reset");
		m_paiExtraUnitCombatModifier = new int[GC.getNumUnitCombatInfos()];
		for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
			m_paiExtraUnitCombatModifier[eUnitCombat] = 0;
		}

		FAssertMsg((0 < GC.getNumInvisibleInfos()), "GC.getNumInvisibleInfos() is not greater than zero but an array is being allocated in CvUnit::reset");
		m_paiSeeInvisibleCount = new int[GC.getNumInvisibleInfos()];
		for (InvisibleTypes eInvisible = (InvisibleTypes)0; eInvisible < GC.getNumInvisibleInfos(); eInvisible = (InvisibleTypes)(eInvisible + 1)) {
			m_paiSeeInvisibleCount[eInvisible] = 0;
		}

		m_mmBuildLeavesFeatures.clear();

		AI_reset();
	}
}


//////////////////////////////////////
// graphical only setup
//////////////////////////////////////
void CvUnit::setupGraphical() {
	if (!GC.IsGraphicsInitialized()) {
		return;
	}

	CvDLLEntity::setup();

	if (getGroup()->getActivityType() == ACTIVITY_INTERCEPT) {
		airCircle(true);
	}
}

// We are a new unit ready to convert ourselves from pUnit
void CvUnit::convert(CvUnit* pUnit) {
	CvPlot* pPlot = plot();

	// We need to this first in order to allow any slaver promotions to be carried over
	if (pUnit->getMaxSlaves() > 0 && canBecomeSlaver()) {
		becomeSlaver();
	}

	// We can't assume that all promotions are immedaitely valid as there are some that modify cargo capacity and if the upgrade reduces or removes the
	// cargo capacity then the promotion would not be valid. It may also be the case that there is more than one cargo capacity modifying promotions and
	// the net result may be valid so we don't immediately dismiss a promotion but store it to try again later.
	std::vector<PromotionTypes> vInvalidPromotions;
	for (PromotionTypes ePromotion = (PromotionTypes)0; ePromotion < GC.getNumPromotionInfos(); ePromotion = (PromotionTypes)(ePromotion + 1)) {
		if (pUnit->isHasPromotion(ePromotion) || m_pUnitInfo->getFreePromotions(ePromotion)) {
			if (canAcquirePromotion(ePromotion)) {
				setHasPromotion(ePromotion, true);
			} else if (!isHasPromotion(ePromotion)) {
				vInvalidPromotions.push_back(ePromotion); // store any currently invalid promotions to try later
			}
		}
	}
	bool bAddedPromotion;
	// Keep trying promotions until we can't add any more
	// We could remove any promotions that we can add in this loop from the vector, but that would invalidate the loop and
	//  the check to acquire promotions catches these at the start so it is not worth it given the expected small size of the vector
	do {
		bAddedPromotion = false;
		for (std::vector<PromotionTypes>::iterator it = vInvalidPromotions.begin(); it != vInvalidPromotions.end(); ++it) {
			if (canAcquirePromotion(*it)) {
				setHasPromotion(*it, true);
				bAddedPromotion = true;
			}
		}
	} while (bAddedPromotion);

	// We need to do this after the promotions to ensure that our enslave count is high enough to hold all the slaves
	if (pUnit->getSlaveCountTotal() > 0) {
		for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
			changeSlaveCount(eSpecialist, pUnit->getSlaveCount(eSpecialist));
		}
	}

	setGameTurnCreated(pUnit->getGameTurnCreated());
	setDamage(pUnit->getDamage());
	setMoves(pUnit->getMoves());
	setAutoPromoting(pUnit->isAutoPromoting());
	setAutoUpgrading(pUnit->isAutoUpgrading());
	setFixedAI(pUnit->isFixedAI());
	setHiddenNationality(pUnit->isHiddenNationality());
	setAlwaysHostile(pUnit->isAlwaysHostile());

	setLevel(pUnit->getLevel());
	int iOldModifier = std::max(1, 100 + GET_PLAYER(pUnit->getOwnerINLINE()).getLevelExperienceModifier());
	int iOurModifier = std::max(1, 100 + GET_PLAYER(getOwnerINLINE()).getLevelExperienceModifier());
	setExperience(std::max(0, (pUnit->getExperience() * iOurModifier) / iOldModifier));
	setName(pUnit->getNameNoDesc());
	setLeaderUnitType(pUnit->getLeaderUnitType());

	CvUnit* pTransportUnit = pUnit->getTransportUnit();
	if (pTransportUnit != NULL)
		pUnit->setTransportUnit(NULL);
	setTransportUnit(pTransportUnit);

	std::vector<CvUnit*> aCargoUnits;
	pUnit->getCargoUnits(aCargoUnits);
	for (uint i = 0; i < aCargoUnits.size(); ++i) {
		// Check cargo types and capacity when upgrading transports
		if (cargoSpaceAvailable(aCargoUnits[i]->getSpecialUnitType(), aCargoUnits[i]->getDomainType()) > 0) {
			aCargoUnits[i]->setTransportUnit(this);
		} else {
			aCargoUnits[i]->setTransportUnit(NULL);
			aCargoUnits[i]->jumpToNearestValidPlot();
		}
	}

	pUnit->kill(true);
}


// K-Mod. I've made some structural change to this function, for efficiency, clarity, and sanity.
void CvUnit::kill(bool bDelay, PlayerTypes ePlayer) {
	PROFILE_FUNC();

	CvPlot* pPlot = plot();
	FAssert(pPlot);

	CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();
	while (pUnitNode) {
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pPlot->nextUnitNode(pUnitNode);

		FAssert(pLoopUnit);
		if (pLoopUnit->getTransportUnit() == this) {
			if (pPlot->isValidDomainForLocation(*pLoopUnit)) {
				pLoopUnit->setCapturingPlayer(NO_PLAYER);
			}

			pLoopUnit->kill(false, ePlayer);
		}
	}

	if (ePlayer != NO_PLAYER) {
		CvEventReporter::getInstance().unitKilled(this, ePlayer);

		if (NO_UNIT != getLeaderUnitType()) {
			CvWString szBuffer;
			szBuffer = gDLL->getText("TXT_KEY_MISC_GENERAL_KILLED", getNameKey());
			for (PlayerTypes i = (PlayerTypes)0; i < MAX_PLAYERS; i = (PlayerTypes)(i + 1)) {
				if (GET_PLAYER(i).isAlive()) {
					gDLL->getInterfaceIFace()->addHumanMessage(i, false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_MAJOR_EVENT); // K-Mod (the other sound is not appropriate for most civs receiving the message.)
				}
			}
		}
	}

	finishMoves();

	if (bDelay) {
		startDelayedDeath();
		return;
	}

	if (isMadeAttack() && nukeRange() != -1) {
		CvPlot* pTarget = getAttackPlot();
		if (pTarget) {
			pTarget->nukeExplosion(nukeRange(), this);
			setAttackPlot(NULL, false);
		}
	}

	if (IsSelected()) {
		if (gDLL->getInterfaceIFace()->getLengthSelectionList() == 1) {
			if (!(gDLL->getInterfaceIFace()->isFocused()) && !(gDLL->getInterfaceIFace()->isCitySelection()) && !(gDLL->getInterfaceIFace()->isDiploOrPopupWaiting())) {
				GC.getGameINLINE().updateSelectionList();
			}

			if (IsSelected()) {
				GC.getGameINLINE().cycleSelectionGroups_delayed(1, false);
			} else {
				gDLL->getInterfaceIFace()->setDirty(SelectionCamera_DIRTY_BIT, true);
			}
		}
	}

	gDLL->getInterfaceIFace()->removeFromSelectionList(this);

	// XXX this is NOT a hack, without it, the game crashes.
	gDLL->getEntityIFace()->RemoveUnitFromBattle(this);

	FAssert(!isFighting()); // K-Mod. With simultaneous turns, a unit can be captured while trying to execute an attack order. (eg. a bomber)

	setTransportUnit(NULL);

	setReconPlot(NULL);
	setBlockading(false);

	FAssertMsg(getCombatUnit() == NULL, "The current unit instance's combat unit is expected to be NULL");

	GET_TEAM(getTeam()).changeUnitClassCount((UnitClassTypes)m_pUnitInfo->getUnitClassType(), -1);

	CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());
	kOwner.changeUnitClassCount((UnitClassTypes)m_pUnitInfo->getUnitClassType(), -1);
	kOwner.changeExtraUnitCost(-(m_pUnitInfo->getExtraCost()));

	if (m_pUnitInfo->getNukeRange() != -1) {
		kOwner.changeNumNukeUnits(-1);
	}

	if (m_pUnitInfo->isMilitarySupport()) {
		kOwner.changeNumMilitaryUnits(-1);
	}

	if (isSlave()) {
		kOwner.changeNumSlaves(-1);
	}

	kOwner.changeAssets(-(m_pUnitInfo->getAssetValue()));

	kOwner.changePower(-(m_pUnitInfo->getPowerValue()));

	kOwner.AI_changeNumAIUnits(AI_getUnitAIType(), -1);

	PlayerTypes eOwner = getOwnerINLINE();
	PlayerTypes eCapturingPlayer = getCapturingPlayer();
	UnitTypes eCaptureUnitType = ((eCapturingPlayer != NO_PLAYER) ? getCaptureUnitType(GET_PLAYER(eCapturingPlayer).getCivilizationType()) : NO_UNIT);

	setXY(INVALID_PLOT_COORD, INVALID_PLOT_COORD, true);

	joinGroup(NULL, false, false);

	CvEventReporter::getInstance().unitLost(this);

	kOwner.deleteUnit(getID());

	if ((eCapturingPlayer != NO_PLAYER) && (eCaptureUnitType != NO_UNIT) && !(GET_PLAYER(eCapturingPlayer).isBarbarian())) {
		if (GET_PLAYER(eCapturingPlayer).isHuman() || GET_PLAYER(eCapturingPlayer).AI_captureUnit(eCaptureUnitType, pPlot) || 0 == GC.getDefineINT("AI_CAN_DISBAND_UNITS")) {
			CvUnit* pkCapturedUnit = GET_PLAYER(eCapturingPlayer).initUnit(eCaptureUnitType, pPlot->getX_INLINE(), pPlot->getY_INLINE());

			if (pkCapturedUnit != NULL) {
				CvWString szBuffer;
				szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_CAPTURED_UNIT", GC.getUnitInfo(eCaptureUnitType).getTextKeyWide());
				gDLL->getInterfaceIFace()->addHumanMessage(eCapturingPlayer, true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_UNITCAPTURE", MESSAGE_TYPE_INFO, pkCapturedUnit->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE());

				// Add a captured mission
				if (pPlot->isActiveVisible(false)) // K-Mod
				{
					CvMissionDefinition kMission;
					kMission.setMissionTime(GC.getMissionInfo(MISSION_CAPTURED).getTime() * gDLL->getSecsPerTurn());
					kMission.setUnit(BATTLE_UNIT_ATTACKER, pkCapturedUnit);
					kMission.setUnit(BATTLE_UNIT_DEFENDER, NULL);
					kMission.setPlot(pPlot);
					kMission.setMissionType(MISSION_CAPTURED);
					gDLL->getEntityIFace()->AddMission(&kMission);
				}

				pkCapturedUnit->finishMoves();

				if (!GET_PLAYER(eCapturingPlayer).isHuman()) {
					CvPlot* pPlot = pkCapturedUnit->plot();
					if (pPlot && !pPlot->isCity(false)) {
						if (GET_PLAYER(eCapturingPlayer).AI_getPlotDanger(pPlot) && GC.getDefineINT("AI_CAN_DISBAND_UNITS")) {
							pkCapturedUnit->scrap(); // K-Mod. roughly the same thing, but this is more appropriate.
						}
					}
				}
			}
		}
	}
}


void CvUnit::NotifyEntity(MissionTypes eMission) {
	gDLL->getEntityIFace()->NotifyEntity(getUnitEntity(), eMission);
}


void CvUnit::doTurn() {
	PROFILE("CvUnit::doTurn()")

	FAssertMsg(!isDead(), "isDead did not return false as expected");
	FAssertMsg(getGroup() != NULL, "getGroup() is not expected to be equal with NULL");

	setGroupPromotionChanged(false);
	testPromotionReady();

	// Set the home city if it does not have one, this will be the case for the first units
	//  before the capital is founded
	if (getHomeCity() == NULL) {
		setHomeCity(GET_PLAYER(getOwnerINLINE()).findCity(getX_INLINE(), getY_INLINE()));
	}

	// If we have enable rehoming and the unit is in a city that belongs to the player but is not it's home city check if it should rehome here
	if (GC.getREHOME_PERCENT_CHANCE() > 0 && plot()->isCity() && plot()->getOwnerINLINE() == getOwnerINLINE() && plot()->getPlotCity() != getHomeCity()) {
		if (GC.getGameINLINE().getSorenRandNum(100, "Rehome Unit") < GC.getREHOME_PERCENT_CHANCE()) {
			setHomeCity(plot()->getPlotCity());
		}
	}

	if (isBlockading()) {
		collectBlockadeGold();
	}

	if (isSpy() && isIntruding() && !isCargo()) {
		TeamTypes eTeam = plot()->getTeam();
		if (NO_TEAM != eTeam) {
			bool bReveal = false;
			if (GET_TEAM(getTeam()).isOpenBorders(eTeam)) {
				testSpyIntercepted(plot()->getOwnerINLINE(), false, bReveal, GC.getDefineINT("ESPIONAGE_SPY_NO_INTRUDE_INTERCEPT_MOD"));
			} else {
				testSpyIntercepted(plot()->getOwnerINLINE(), false, bReveal, GC.getDefineINT("ESPIONAGE_SPY_INTERCEPT_MOD"));
			}
		}
	}

	if (baseCombatStr() > 0) {
		FeatureTypes eFeature = plot()->getFeatureType();
		if (NO_FEATURE != eFeature) {
			if (0 != GC.getFeatureInfo(eFeature).getTurnDamage()) {
				changeDamage(GC.getFeatureInfo(eFeature).getTurnDamage(), NO_PLAYER);
			}
		}
	}

	if (hasMoved()) {
		if (isAlwaysHeal()) {
			doHeal();
		}
	} else {
		if (isHurt()) {
			doHeal();
		}

		if (!isCargo()) {
			changeFortifyTurns(1);
		}
	}

	changeImmobileTimer(-1);

	setMadeAttack(false);
	setMadeInterception(false);

	setReconPlot(NULL);

	setMoves(0);

	if (getDesiredDiscoveryTech() != NO_TECH) {
		if (GET_PLAYER(getOwnerINLINE()).canResearch(getDesiredDiscoveryTech())) {
			getGroup()->setActivityType(ACTIVITY_AWAKE);
			discover(getDesiredDiscoveryTech());
			setDesiredDiscoveryTech(NO_TECH);
		}
	}

	if (isAutoUpgrading()) {
		if (GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 3) == 0 || plot()->isCity()) {
			AI_upgrade();
			//Could Delete Unit!!!
		}
	}
}


void CvUnit::updateAirStrike(CvPlot* pPlot, bool bQuick, bool bFinish) {
	bool bVisible = false;

	if (!bFinish) {
		if (isFighting()) {
			return;
		}

		if (!bQuick) {
			bVisible = isCombatVisible(NULL);
		}

		if (!airStrike(pPlot)) {
			return;
		}

		if (bVisible) {
			CvAirMissionDefinition kAirMission;
			kAirMission.setMissionType(MISSION_AIRSTRIKE);
			kAirMission.setUnit(BATTLE_UNIT_ATTACKER, this);
			kAirMission.setUnit(BATTLE_UNIT_DEFENDER, NULL);
			kAirMission.setDamage(BATTLE_UNIT_DEFENDER, 0);
			kAirMission.setDamage(BATTLE_UNIT_ATTACKER, 0);
			kAirMission.setPlot(pPlot);
			setCombatTimer(GC.getMissionInfo(MISSION_AIRSTRIKE).getTime());
			GC.getGameINLINE().incrementTurnTimer(getCombatTimer());
			kAirMission.setMissionTime(getCombatTimer() * gDLL->getSecsPerTurn());

			if (pPlot->isActiveVisible(false)) {
				gDLL->getEntityIFace()->AddMission(&kAirMission);
			}

			return;
		}
	}

	CvUnit* pDefender = getCombatUnit();
	if (pDefender != NULL) {
		pDefender->setCombatUnit(NULL);
	}
	setCombatUnit(NULL);
	setAttackPlot(NULL, false);

	getGroup()->clearMissionQueue();

	if (isSuicide() && !isDead()) {
		kill(true);
	}
}

void CvUnit::resolveAirCombat(CvUnit* pInterceptor, CvPlot* pPlot, CvAirMissionDefinition& kBattle) {
	int iTheirStrength = (DOMAIN_AIR == pInterceptor->getDomainType() ? pInterceptor->airCurrCombatStr(this) : pInterceptor->currCombatStr(NULL, NULL));
	int iOurStrength = (DOMAIN_AIR == getDomainType() ? airCurrCombatStr(pInterceptor) : currCombatStr(NULL, NULL));
	int iTotalStrength = iOurStrength + iTheirStrength;
	if (0 == iTotalStrength) {
		FAssert(false);
		return;
	}

	// For air v air, more rounds and factor in strength for per round damage
	int iOurOdds = (100 * iOurStrength) / std::max(1, iTotalStrength);
	int iMaxRounds = 0;
	int iOurRoundDamage = 0;
	int iTheirRoundDamage = 0;

	// Air v air is more like standard comabt
	// Round damage in this case will now depend on strength and interception probability
	if (GC.getBBAI_AIR_COMBAT() && (DOMAIN_AIR == pInterceptor->getDomainType() && DOMAIN_AIR == getDomainType())) {
		int iBaseDamage = GC.getDefineINT("AIR_COMBAT_DAMAGE");
		int iOurFirepower = ((airMaxCombatStr(pInterceptor) + iOurStrength + 1) / 2);
		int iTheirFirepower = ((pInterceptor->airMaxCombatStr(this) + iTheirStrength + 1) / 2);

		int iStrengthFactor = ((iOurFirepower + iTheirFirepower + 1) / 2);

		int iTheirInterception = std::max(pInterceptor->maxInterceptionProbability(), 2 * GC.getDefineINT("MIN_INTERCEPTION_DAMAGE"));
		int iOurInterception = std::max(maxInterceptionProbability(), 2 * GC.getDefineINT("MIN_INTERCEPTION_DAMAGE"));

		iOurRoundDamage = std::max(1, ((iBaseDamage * (iTheirFirepower + iStrengthFactor) * iTheirInterception) / ((iOurFirepower + iStrengthFactor) * 100)));
		iTheirRoundDamage = std::max(1, ((iBaseDamage * (iOurFirepower + iStrengthFactor) * iOurInterception) / ((iTheirFirepower + iStrengthFactor) * 100)));

		iMaxRounds = 2 * GC.getDefineINT("INTERCEPTION_MAX_ROUNDS") - 1;
	} else {
		iOurRoundDamage = (pInterceptor->currInterceptionProbability() * GC.getDefineINT("MAX_INTERCEPTION_DAMAGE")) / 100;
		iTheirRoundDamage = (currInterceptionProbability() * GC.getDefineINT("MAX_INTERCEPTION_DAMAGE")) / 100;
		if (getDomainType() == DOMAIN_AIR) {
			iTheirRoundDamage = std::max(GC.getDefineINT("MIN_INTERCEPTION_DAMAGE"), iTheirRoundDamage);
		}

		iMaxRounds = GC.getDefineINT("INTERCEPTION_MAX_ROUNDS");
	}

	int iTheirDamage = 0;
	int iOurDamage = 0;

	for (int iRound = 0; iRound < iMaxRounds; ++iRound) {
		if (GC.getGameINLINE().getSorenRandNum(100, "Air combat") < iOurOdds) {
			if (DOMAIN_AIR == pInterceptor->getDomainType()) {
				iTheirDamage += iTheirRoundDamage;
				pInterceptor->changeDamage(iTheirRoundDamage, getOwnerINLINE());
				if (pInterceptor->isDead()) {
					break;
				}
			}
		} else {
			iOurDamage += iOurRoundDamage;
			changeDamage(iOurRoundDamage, pInterceptor->getOwnerINLINE());
			if (isDead()) {
				break;
			}
		}
	}

	if (isDead()) {
		if (iTheirRoundDamage > 0) {
			int iExperience = attackXPValue();
			iExperience = (iExperience * iOurStrength) / std::max(1, iTheirStrength);
			iExperience = range(iExperience, GC.getDefineINT("MIN_EXPERIENCE_PER_COMBAT"), GC.getDefineINT("MAX_EXPERIENCE_PER_COMBAT"));
			pInterceptor->changeExperience(iExperience, maxXPValue(), true, pPlot->getOwnerINLINE() == pInterceptor->getOwnerINLINE(), !isBarbarian());
		}
	} else if (pInterceptor->isDead()) {
		int iExperience = pInterceptor->defenseXPValue();
		iExperience = (iExperience * iTheirStrength) / std::max(1, iOurStrength);
		iExperience = range(iExperience, GC.getDefineINT("MIN_EXPERIENCE_PER_COMBAT"), GC.getDefineINT("MAX_EXPERIENCE_PER_COMBAT"));
		changeExperience(iExperience, pInterceptor->maxXPValue(), true, pPlot->getOwnerINLINE() == getOwnerINLINE(), !pInterceptor->isBarbarian());
	} else if (iOurDamage > 0) {
		if (iTheirRoundDamage > 0) {
			pInterceptor->changeExperience(GC.getDefineINT("EXPERIENCE_FROM_WITHDRAWL"), maxXPValue(), true, pPlot->getOwnerINLINE() == pInterceptor->getOwnerINLINE(), !isBarbarian());
		}
	} else if (iTheirDamage > 0) {
		changeExperience(GC.getDefineINT("EXPERIENCE_FROM_WITHDRAWL"), pInterceptor->maxXPValue(), true, pPlot->getOwnerINLINE() == getOwnerINLINE(), !pInterceptor->isBarbarian());
	}

	kBattle.setDamage(BATTLE_UNIT_ATTACKER, iOurDamage);
	kBattle.setDamage(BATTLE_UNIT_DEFENDER, iTheirDamage);
}


void CvUnit::updateAirCombat(bool bQuick) {
	FAssert(getDomainType() == DOMAIN_AIR || getDropRange() > 0);

	bool bFinish = false;
	if (getCombatTimer() > 0) {
		changeCombatTimer(-1);

		if (getCombatTimer() > 0) {
			return;
		} else {
			bFinish = true;
		}
	}

	CvPlot* pPlot = getAttackPlot();
	if (pPlot == NULL) {
		return;
	}

	CvUnit* pInterceptor = NULL;
	if (bFinish) {
		pInterceptor = getCombatUnit();
	} else {
		pInterceptor = bestInterceptor(pPlot);
	}


	if (pInterceptor == NULL) {
		setAttackPlot(NULL, false);
		setCombatUnit(NULL);

		getGroup()->clearMissionQueue();

		return;
	}

	//check if quick combat
	bool bVisible = false;
	if (!bQuick) {
		bVisible = isCombatVisible(pInterceptor);
	}

	//if not finished and not fighting yet, set up combat damage and mission
	if (!bFinish) {
		if (!isFighting()) {
			// K-Mod. I don't think it matters if the plot we're on is fighting already - but the interceptor needs to be available to fight!
			if (pPlot->isFighting() || pInterceptor->isFighting()) {
				return;
			}

			setMadeAttack(true);

			setCombatUnit(pInterceptor, true);
			pInterceptor->setCombatUnit(this, false);
		}

		FAssertMsg(pInterceptor != NULL, "Defender is not assigned a valid value");

		FAssertMsg(plot()->isFighting(), "Current unit instance plot is not fighting as expected");
		FAssertMsg(pInterceptor->plot()->isFighting(), "pPlot is not fighting as expected");

		CvAirMissionDefinition kAirMission;
		if (DOMAIN_AIR != getDomainType()) {
			kAirMission.setMissionType(MISSION_PARADROP);
		} else {
			kAirMission.setMissionType(MISSION_AIRSTRIKE);
		}
		kAirMission.setUnit(BATTLE_UNIT_ATTACKER, this);
		kAirMission.setUnit(BATTLE_UNIT_DEFENDER, pInterceptor);

		resolveAirCombat(pInterceptor, pPlot, kAirMission);

		if (!bVisible) {
			bFinish = true;
		} else {
			kAirMission.setPlot(pPlot);
			kAirMission.setMissionTime(GC.getMissionInfo(MISSION_AIRSTRIKE).getTime() * gDLL->getSecsPerTurn());
			setCombatTimer(GC.getMissionInfo(MISSION_AIRSTRIKE).getTime());
			GC.getGameINLINE().incrementTurnTimer(getCombatTimer());

			if (pPlot->isActiveVisible(false)) {
				gDLL->getEntityIFace()->AddMission(&kAirMission);
			}
		}

		changeMoves(GC.getMOVE_DENOMINATOR());
		if (DOMAIN_AIR != pInterceptor->getDomainType()) {
			pInterceptor->setMadeInterception(true);
		}

		if (isDead()) {
			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_SHOT_DOWN_ENEMY", pInterceptor->getNameKey(), getNameKey(), getVisualCivAdjective(pInterceptor->getTeam()));
			gDLL->getInterfaceIFace()->addHumanMessage(pInterceptor->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE(), true, true);

			szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_SHOT_DOWN", getNameKey(), pInterceptor->getNameKey());
			gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, pInterceptor->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE());

			if (pPlot->getOwnerINLINE() == pInterceptor->getOwnerINLINE()) {
				pInterceptor->salvage(this);
			}
		} else if (kAirMission.getDamage(BATTLE_UNIT_ATTACKER) > 0) {
			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_HURT_ENEMY_AIR", pInterceptor->getNameKey(), getNameKey(), -(kAirMission.getDamage(BATTLE_UNIT_ATTACKER)), getVisualCivAdjective(pInterceptor->getTeam()));
			gDLL->getInterfaceIFace()->addHumanMessage(pInterceptor->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE(), true, true);

			szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_AIR_UNIT_HURT", getNameKey(), pInterceptor->getNameKey(), -(kAirMission.getDamage(BATTLE_UNIT_ATTACKER)));
			gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, pInterceptor->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE());
		}

		if (pInterceptor->isDead()) {
			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_SHOT_DOWN_ENEMY", getNameKey(), pInterceptor->getNameKey(), pInterceptor->getVisualCivAdjective(getTeam()));
			gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, pInterceptor->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE(), true, true);

			szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_SHOT_DOWN", pInterceptor->getNameKey(), getNameKey());
			gDLL->getInterfaceIFace()->addHumanMessage(pInterceptor->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE());
		} else if (kAirMission.getDamage(BATTLE_UNIT_DEFENDER) > 0) {
			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_DAMAGED_ENEMY_AIR", getNameKey(), pInterceptor->getNameKey(), -(kAirMission.getDamage(BATTLE_UNIT_DEFENDER)), pInterceptor->getVisualCivAdjective(getTeam()));
			gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, pInterceptor->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE(), true, true);

			szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_AIR_UNIT_DAMAGED", pInterceptor->getNameKey(), getNameKey(), -(kAirMission.getDamage(BATTLE_UNIT_DEFENDER)));
			gDLL->getInterfaceIFace()->addHumanMessage(pInterceptor->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE());
		}

		if (0 == kAirMission.getDamage(BATTLE_UNIT_ATTACKER) + kAirMission.getDamage(BATTLE_UNIT_DEFENDER)) {
			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_ABORTED_ENEMY_AIR", pInterceptor->getNameKey(), getNameKey(), getVisualCivAdjective(getTeam()));
			gDLL->getInterfaceIFace()->addHumanMessage(pInterceptor->getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, pInterceptor->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE(), true, true);

			szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_AIR_UNIT_ABORTED", getNameKey(), pInterceptor->getNameKey());
			gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE());
		}
	}

	if (bFinish) {
		setAttackPlot(NULL, false);
		setCombatUnit(NULL);
		pInterceptor->setCombatUnit(NULL);

		if (!isDead() && isSuicide()) {
			kill(true);
		}
	}
}

//#define LOG_COMBAT_OUTCOMES // K-Mod -- this makes the game log the odds and outcomes of every battle, to help verify the accuracy of the odds calculation.

// K-Mod. I've edited this function so that it handles the battle planning internally rather than feeding details back to the caller.
void CvUnit::resolveCombat(CvUnit* pDefender, CvPlot* pPlot, bool bVisible) {
#ifdef LOG_COMBAT_OUTCOMES
	int iLoggedOdds = getCombatOdds(this, pDefender);
	iLoggedOdds += (1000 - iLoggedOdds) * withdrawalProbability() / 100;
#endif

	// K-Mod. Initialize battle info.
	// Note: kBattle is only relevant if we are going to show the battle animation.
	CvBattleDefinition kBattle;
	if (bVisible) {
		kBattle.setUnit(BATTLE_UNIT_ATTACKER, this);
		kBattle.setUnit(BATTLE_UNIT_DEFENDER, pDefender);
		kBattle.setDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_BEGIN, getDamage());
		kBattle.setDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_BEGIN, pDefender->getDamage());
	}
	std::vector<int> combat_log; // positive number for attacker hitting the defender, negative numbers for defender hitting the attacker.

	CombatDetails cdAttackerDetails;
	CombatDetails cdDefenderDetails;

	int iAttackerStrength = currCombatStr(NULL, NULL, &cdAttackerDetails);
	int iAttackerFirepower = currFirepower(NULL, NULL);
	int iDefenderStrength;
	int iAttackerDamage;
	int iDefenderDamage;
	int iDefenderOdds;

	getDefenderCombatValues(*pDefender, pPlot, iAttackerStrength, iAttackerFirepower, iDefenderOdds, iDefenderStrength, iAttackerDamage, iDefenderDamage, &cdDefenderDetails);
	int iAttackerKillOdds = iDefenderOdds * (100 - withdrawalProbability()) / 100;

	int iWinningOdds = getCombatOdds(this, pDefender);

	CombatData combatData;
	combatData.bDefenderWithdrawn = false;
	combatData.bAttackerWithdrawn = false;
	combatData.bAttackerUninjured = true;
	combatData.bAmphibAttack = cdDefenderDetails.iAmphibAttackModifier;
	combatData.bRiverAttack = cdDefenderDetails.iRiverAttackModifier;
	combatData.iAttackerInitialDamage = getDamage();
	combatData.iDefenderInitialDamage = pDefender->getDamage();
	combatData.iWinningOdds = iWinningOdds;

	if (isHuman() || pDefender->isHuman()) {
		//Added ST
		CyArgsList pyArgsCD;
		pyArgsCD.add(gDLL->getPythonIFace()->makePythonObject(&cdAttackerDetails));
		pyArgsCD.add(gDLL->getPythonIFace()->makePythonObject(&cdDefenderDetails));
		pyArgsCD.add(iWinningOdds);
		CvEventReporter::getInstance().genericEvent("combatLogCalc", pyArgsCD.makeFunctionArgs());
	}

	collateralCombat(pPlot, pDefender);

	while (true) {
		if (GC.getGameINLINE().getSorenRandNum(GC.getCOMBAT_DIE_SIDES(), "Combat") < iDefenderOdds) {
			// Defender won a round
			if (getCombatFirstStrikes() == 0) {
				// Attacker trying to withdraw
				if (getDamage() + iAttackerDamage >= maxHitPoints() && GC.getGameINLINE().getSorenRandNum(100, "Withdrawal") < withdrawalProbability()) {
					combatData.bAttackerWithdrawn = true;
					flankingStrikeCombat(pPlot, iAttackerStrength, iAttackerFirepower, iAttackerKillOdds, iDefenderDamage, pDefender);
					changeExperience(GC.getDefineINT("EXPERIENCE_FROM_WITHDRAWL"), pDefender->maxXPValue(), true, pPlot->getOwnerINLINE() == getOwnerINLINE(), !pDefender->isBarbarian(), pDefender->isBarbarianConvert());
					combat_log.push_back(0); // K-Mod
					break;
				}

				changeDamage(iAttackerDamage, pDefender->getOwnerINLINE());
				combatData.bAttackerUninjured = false;
				combat_log.push_back(-iAttackerDamage); // K-Mod

				// K-Mod. (I don't think this stuff is actually used, but I want to do it my way, just in case.)
				if (pDefender->getCombatFirstStrikes() > 0)
					kBattle.addFirstStrikes(BATTLE_UNIT_DEFENDER, 1);

				cdAttackerDetails.iCurrHitPoints = currHitPoints();

				if (isHuman() || pDefender->isHuman()) {
					CyArgsList pyArgs;
					pyArgs.add(gDLL->getPythonIFace()->makePythonObject(&cdAttackerDetails));
					pyArgs.add(gDLL->getPythonIFace()->makePythonObject(&cdDefenderDetails));
					pyArgs.add(1);
					pyArgs.add(iAttackerDamage);
					CvEventReporter::getInstance().genericEvent("combatLogHit", pyArgs.makeFunctionArgs());
				}
			}
			// K-Mod. Track the free-strike misses, to use for choreographing the battle animation.
			else if (bVisible && !combat_log.empty())
				combat_log.push_back(0);
		} else {
			// Defender lost a round
			if (pDefender->getCombatFirstStrikes() == 0) {
				// Combat limit
				if (std::min(GC.getMAX_HIT_POINTS(), pDefender->getDamage() + iDefenderDamage) > combatLimit()) {
					changeExperience(GC.getDefineINT("EXPERIENCE_FROM_WITHDRAWL"), pDefender->maxXPValue(), true, pPlot->getOwnerINLINE() == getOwnerINLINE(), !pDefender->isBarbarian(), isBarbarianConvert());
					combat_log.push_back(combatLimit() - pDefender->getDamage()); // K-Mod
					pDefender->setDamage(combatLimit(), getOwnerINLINE());
					break;
				}

				pDefender->changeDamage(iDefenderDamage, getOwnerINLINE());
				combat_log.push_back(iDefenderDamage); // K-Mod

				if (getCombatFirstStrikes() > 0)
					kBattle.addFirstStrikes(BATTLE_UNIT_ATTACKER, 1);

				cdDefenderDetails.iCurrHitPoints = pDefender->currHitPoints();

				if (isHuman() || pDefender->isHuman()) {
					CyArgsList pyArgs;
					pyArgs.add(gDLL->getPythonIFace()->makePythonObject(&cdAttackerDetails));
					pyArgs.add(gDLL->getPythonIFace()->makePythonObject(&cdDefenderDetails));
					pyArgs.add(0);
					pyArgs.add(iDefenderDamage);
					CvEventReporter::getInstance().genericEvent("combatLogHit", pyArgs.makeFunctionArgs());
				}
			} else if (bVisible && !combat_log.empty())
				combat_log.push_back(0);
		}

		if (getCombatFirstStrikes() > 0) {
			changeCombatFirstStrikes(-1);
		}

		if (pDefender->getCombatFirstStrikes() > 0) {
			pDefender->changeCombatFirstStrikes(-1);
		}

		if (isDead() || pDefender->isDead()) {
			if (isDead()) { //attacker died
				int iExperience = defenseXPValue();
				iExperience = ((iExperience * iAttackerStrength) / iDefenderStrength);
				iExperience = range(iExperience, GC.getDefineINT("MIN_EXPERIENCE_PER_COMBAT"), GC.getDefineINT("MAX_EXPERIENCE_PER_COMBAT"));
				pDefender->changeExperience(iExperience, maxXPValue(), true, pPlot->getOwnerINLINE() == pDefender->getOwnerINLINE(), !isBarbarian(), pDefender->isBarbarianConvert());
			} else { //defender died
				flankingStrikeCombat(pPlot, iAttackerStrength, iAttackerFirepower, iAttackerKillOdds, iDefenderDamage, pDefender);

				int iExperience = pDefender->attackXPValue();
				iExperience = ((iExperience * iDefenderStrength) / iAttackerStrength);
				iExperience = range(iExperience, GC.getDefineINT("MIN_EXPERIENCE_PER_COMBAT"), GC.getDefineINT("MAX_EXPERIENCE_PER_COMBAT"));
				changeExperience(iExperience, pDefender->maxXPValue(), true, pPlot->getOwnerINLINE() == getOwnerINLINE(), !pDefender->isBarbarian(), isBarbarianConvert());
			}

			break;
		}
	}

	// K-Mod. Finalize battle info and start the animation.
	if (bVisible) {
		kBattle.setDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_END, getDamage());
		kBattle.setDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_END, pDefender->getDamage());
		kBattle.setAdvanceSquare(canAdvance(pPlot, 1));

		// note: BATTLE_TIME_RANGED damage is now set inside planBattle; (not that it actually does anything...)

		int iTurns = planBattle(kBattle, combat_log);
		kBattle.setMissionTime(iTurns * gDLL->getSecsPerTurn());
		setCombatTimer(iTurns);

		GC.getGameINLINE().incrementTurnTimer(getCombatTimer()); // additional time for multiplayer turn timer.

		if (pPlot->isActiveVisible(false)) {
			ExecuteMove(0.5f, true);
			gDLL->getEntityIFace()->AddMission(&kBattle);
		}
	}

	doFieldPromotions(&combatData, pDefender, pPlot);

#ifdef LOG_COMBAT_OUTCOMES
	if (!isBarbarian() && !pDefender->isBarbarian()) // don't log barb battles, because they have special rules.
	{
		TCHAR message[20];
		_snprintf(message, 20, "%.2f\t%d\n", (float)iLoggedOdds / 1000, isDead() ? 0 : 1);
		gDLL->logMsg("combat.txt", message, false, false);
	}
#endif
}


void CvUnit::updateCombat(bool bQuick) {
	bool bFinish = false;
	bool bVisible = false;

	if (getCombatTimer() > 0) {
		FAssert(getCombatUnit() && getCombatUnit()->getAttackPlot() == NULL); // K-Mod
		changeCombatTimer(-1);

		if (getCombatTimer() > 0) {
			return;
		} else {
			bFinish = true;
		}
	}

	CvPlot* pPlot = getAttackPlot();
	if (pPlot == NULL) {
		return;
	}

	if (getDomainType() == DOMAIN_AIR) {
		updateAirStrike(pPlot, bQuick, bFinish);
		return;
	}

	CvUnit* pDefender = NULL;
	if (bFinish) {
		pDefender = getCombatUnit();
	} else {
		FAssert(!isFighting());
		if (plot()->isFighting() || pPlot->isFighting()) {
			// K-Mod. we need to wait for our turn to attack - so don't bother looking for a defender yet.
			return;
		}
		pDefender = pPlot->getBestDefender(NO_PLAYER, getOwnerINLINE(), this, true);
	}

	if (pDefender == NULL) {
		setAttackPlot(NULL, false);
		setCombatUnit(NULL);

		if (bFinish) {
			FAssertMsg(false, "Cannot 'finish' combat with NULL defender");
			return;
		} else
			getGroup()->groupMove(pPlot, true, canAdvance(pPlot, 0) ? this : NULL, true);

		getGroup()->clearMissionQueue();

		return;
	}

	//check if quick combat
	if (!bQuick) {
		bVisible = isCombatVisible(pDefender);
	}

	//if not finished and not fighting yet, set up combat damage and mission
	if (!bFinish) {
		if (!isFighting()) {
			if (plot()->isFighting() || pPlot->isFighting()) {
				return;
			}

			setMadeAttack(true);

			//rotate to face plot
			DirectionTypes newDirection = estimateDirection(this->plot(), pDefender->plot());
			if (newDirection != NO_DIRECTION) {
				setFacingDirection(newDirection);
			}

			//rotate enemy to face us
			newDirection = estimateDirection(pDefender->plot(), this->plot());
			if (newDirection != NO_DIRECTION) {
				pDefender->setFacingDirection(newDirection);
			}

			setCombatUnit(pDefender, true);
			pDefender->setCombatUnit(this, false);

			pDefender->setAttackPlot(NULL, false); // K-Mod (to prevent weirdness from simultanious attacks)
			pDefender->getGroup()->clearMissionQueue();

			bool bFocused = (bVisible && isCombatFocus() && gDLL->getInterfaceIFace()->isCombatFocus());

			if (bFocused) {
				DirectionTypes directionType = directionXY(plot(), pPlot);
				//								N			NE				E				SE					S				SW					W				NW
				NiPoint2 directions[8] = { NiPoint2(0, 1), NiPoint2(1, 1), NiPoint2(1, 0), NiPoint2(1, -1), NiPoint2(0, -1), NiPoint2(-1, -1), NiPoint2(-1, 0), NiPoint2(-1, 1) };
				NiPoint3 attackDirection = NiPoint3(directions[directionType].x, directions[directionType].y, 0);
				float plotSize = GC.getPLOT_SIZE();
				NiPoint3 lookAtPoint(plot()->getPoint().x + plotSize / 2 * attackDirection.x, plot()->getPoint().y + plotSize / 2 * attackDirection.y, (plot()->getPoint().z + pPlot->getPoint().z) / 2);
				attackDirection.Unitize();
				gDLL->getInterfaceIFace()->lookAt(lookAtPoint, (((getOwnerINLINE() != GC.getGameINLINE().getActivePlayer()) || gDLL->getGraphicOption(GRAPHICOPTION_NO_COMBAT_ZOOM)) ? CAMERALOOKAT_BATTLE : CAMERALOOKAT_BATTLE_ZOOM_IN), attackDirection);
			} else {
				PlayerTypes eAttacker = getVisualOwner(pDefender->getTeam());
				CvWString szMessage;
				if (BARBARIAN_PLAYER != eAttacker) {
					szMessage = gDLL->getText("TXT_KEY_MISC_YOU_UNITS_UNDER_ATTACK", GET_PLAYER(getOwnerINLINE()).getNameKey());
				} else {
					szMessage = gDLL->getText("TXT_KEY_MISC_YOU_UNITS_UNDER_ATTACK_UNKNOWN");
				}

				gDLL->getInterfaceIFace()->addHumanMessage(pDefender->getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szMessage, "AS2D_COMBAT", MESSAGE_TYPE_DISPLAY_ONLY, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE(), true);
			}
		}

		FAssertMsg(pDefender != NULL, "Defender is not assigned a valid value");

		FAssertMsg(plot()->isFighting(), "Current unit instance plot is not fighting as expected");
		FAssertMsg(pPlot->isFighting(), "pPlot is not fighting as expected");

		if (!pDefender->canDefend()) {
			if (!bVisible) {
				bFinish = true;
			} else {
				CvMissionDefinition kMission;
				kMission.setMissionTime(getCombatTimer() * gDLL->getSecsPerTurn());
				kMission.setMissionType(MISSION_SURRENDER);
				kMission.setUnit(BATTLE_UNIT_ATTACKER, this);
				kMission.setUnit(BATTLE_UNIT_DEFENDER, pDefender);
				kMission.setPlot(pPlot);
				gDLL->getEntityIFace()->AddMission(&kMission);

				// Surrender mission
				setCombatTimer(GC.getMissionInfo(MISSION_SURRENDER).getTime());

				GC.getGameINLINE().incrementTurnTimer(getCombatTimer());
			}

			// Kill them!
			pDefender->setDamage(GC.getMAX_HIT_POINTS());
		} else {
			resolveCombat(pDefender, pPlot, bVisible);

			FAssert(!bVisible || getCombatTimer() > 0);
			if (!bVisible)
				bFinish = true;

			// Note: K-Mod has moved the bulk of this block into resolveCombat.
		}
	}

	if (bFinish) {
		if (bVisible) {
			if (isCombatFocus() && gDLL->getInterfaceIFace()->isCombatFocus()) {
				if (getOwnerINLINE() == GC.getGameINLINE().getActivePlayer()) {
					gDLL->getInterfaceIFace()->releaseLockedCamera();
				}
			}
		}

		//end the combat mission if this code executes first
		gDLL->getEntityIFace()->RemoveUnitFromBattle(this);
		gDLL->getEntityIFace()->RemoveUnitFromBattle(pDefender);
		setAttackPlot(NULL, false);
		setCombatUnit(NULL);
		pDefender->setCombatUnit(NULL);
		NotifyEntity(MISSION_DAMAGE);
		pDefender->NotifyEntity(MISSION_DAMAGE);

		if (isDead()) {
			if (isBarbarian()) {
				GET_PLAYER(pDefender->getOwnerINLINE()).changeWinsVsBarbs(1);
			}

			if (!pDefender->isBarbarian()) {
				pDefender->salvage(this);
			}

			if (!isHiddenNationality() && !pDefender->isHiddenNationality()) {
				GET_TEAM(getTeam()).changeWarWeariness(pDefender->getTeam(), *pPlot, GC.getDefineINT("WW_UNIT_KILLED_ATTACKING"));
				GET_TEAM(pDefender->getTeam()).changeWarWeariness(getTeam(), *pPlot, GC.getDefineINT("WW_KILLED_UNIT_DEFENDING"));
				GET_TEAM(pDefender->getTeam()).AI_changeWarSuccess(getTeam(), GC.getDefineINT("WAR_SUCCESS_DEFENDING"));
			}

			// Display the losers message including the influence driven war culture
			CvWString szBuffer;
			szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_DIED_ATTACKING", getNameKey(), pDefender->getNameKey());
			float fInfluenceRatio = 0.0;
			if (GC.getIDW_ENABLED()) {
				fInfluenceRatio = pDefender->doVictoryInfluence(this, false, false);
				CvWString szTempBuffer;
				szTempBuffer.Format(L" Influence: -%.1f%%", fInfluenceRatio);
				szBuffer += szTempBuffer;
			}
			gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getEraInfo(GC.getGameINLINE().getCurrentEra()).getAudioUnitDefeatScript(), MESSAGE_TYPE_INFO, NULL, (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE());

			// Display the winners message including the influence driven war culture
			if (enslaveUnit(pDefender, this)) {
				szBuffer = gDLL->getText("TXT_KEY_SLAVERY_DEFEND_YOU_ENSLAVED_ENEMY_UNIT", getNameKey());
			} else {
				szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_KILLED_ENEMY_UNIT", pDefender->getNameKey(), getNameKey(), getVisualCivAdjective(pDefender->getTeam()));
			}
			if (GC.getIDW_ENABLED()) {
				CvWString szTempBuffer;
				szTempBuffer.Format(L" Influence: +%.1f%%", fInfluenceRatio);
				szBuffer += szTempBuffer;
			}
			gDLL->getInterfaceIFace()->addHumanMessage(pDefender->getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getEraInfo(GC.getGameINLINE().getCurrentEra()).getAudioUnitVictoryScript(), MESSAGE_TYPE_INFO, NULL, (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE());

			// report event to Python, along with some other key state
			CvEventReporter::getInstance().combatResult(pDefender, this);
		} else if (pDefender->isDead()) {
			if (pDefender->isBarbarian()) {
				GET_PLAYER(getOwnerINLINE()).changeWinsVsBarbs(1);
			}

			if (!isBarbarian()) {
				salvage(pDefender);
			}

			if (!isHiddenNationality() && !pDefender->isHiddenNationality()) {
				GET_TEAM(pDefender->getTeam()).changeWarWeariness(getTeam(), *pPlot, GC.getDefineINT("WW_UNIT_KILLED_DEFENDING"));
				GET_TEAM(getTeam()).changeWarWeariness(pDefender->getTeam(), *pPlot, GC.getDefineINT("WW_KILLED_UNIT_ATTACKING"));
				GET_TEAM(getTeam()).AI_changeWarSuccess(pDefender->getTeam(), GC.getDefineINT("WAR_SUCCESS_ATTACKING"));
			}

			CvWString szBuffer;
			// Check if any slaves have been taken
			if (enslaveUnit(this, pDefender)) {
				szBuffer = gDLL->getText("TXT_KEY_SLAVERY_ATTACK_YOU_ENSLAVED_ENEMY_UNIT", pDefender->getNameKey());
			} else {
				szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_DESTROYED_ENEMY", getNameKey(), pDefender->getNameKey());
			}

			// Process influence driven war victory
			float fInfluenceRatio = 0.0;
			if (GC.getIDW_ENABLED()) {
				fInfluenceRatio = doVictoryInfluence(pDefender, true, false);
				CvWString szTempBuffer;
				szTempBuffer.Format(L" Influence: +%.1f%%", fInfluenceRatio);
				szBuffer += szTempBuffer;
			}
			gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getEraInfo(GC.getGameINLINE().getCurrentEra()).getAudioUnitVictoryScript(), MESSAGE_TYPE_INFO, NULL, (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE());

			if (getVisualOwner(pDefender->getTeam()) != getOwnerINLINE()) {
				szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_WAS_DESTROYED_UNKNOWN", pDefender->getNameKey(), getNameKey());
			} else {
				szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_WAS_DESTROYED", pDefender->getNameKey(), getNameKey(), getVisualCivAdjective(pDefender->getTeam()));
			}

			// Process influence driven war loss
			if (GC.getIDW_ENABLED()) {
				CvWString szTempBuffer;
				szTempBuffer.Format(L" Influence: -%.1f%%", fInfluenceRatio);
				szBuffer += szTempBuffer;
			}
			gDLL->getInterfaceIFace()->addHumanMessage(pDefender->getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getEraInfo(GC.getGameINLINE().getCurrentEra()).getAudioUnitDefeatScript(), MESSAGE_TYPE_INFO, NULL, (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE());

			// report event to Python, along with some other key state
			CvEventReporter::getInstance().combatResult(this, pDefender);

			bool bAdvance = false;

			if (isSuicide()) {
				kill(true);

				pDefender->kill(false);
				pDefender = NULL;
			} else {
				bAdvance = canAdvance(pPlot, ((pDefender->canDefend()) ? 1 : 0));

				if (bAdvance) {
					if (!isNoCapture()) {
						pDefender->setCapturingPlayer(getOwnerINLINE());
					}
				}


				pDefender->kill(false);
				pDefender = NULL;

				if (!bAdvance) {
					changeMoves(std::max(GC.getMOVE_DENOMINATOR(), pPlot->movementCost(this, plot())));
					checkRemoveSelectionAfterAttack();
				}
			}

			if (pPlot->getNumVisibleEnemyDefenders(this) == 0) {
				getGroup()->groupMove(pPlot, true, bAdvance ? this : NULL, true); // K-Mod
			}

			// This is is put before the plot advancement, the unit will always try to walk back
			// to the square that they came from, before advancing.
			getGroup()->clearMissionQueue();
		} else {
			CvWString szBuffer;
			szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_WITHDRAW", getNameKey(), pDefender->getNameKey());
			gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_OUR_WITHDRAWL", MESSAGE_TYPE_INFO, NULL, (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE());
			szBuffer = gDLL->getText("TXT_KEY_MISC_ENEMY_UNIT_WITHDRAW", getNameKey(), pDefender->getNameKey());
			gDLL->getInterfaceIFace()->addHumanMessage(pDefender->getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_THEIR_WITHDRAWL", MESSAGE_TYPE_INFO, NULL, (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE());

			changeMoves(std::max(GC.getMOVE_DENOMINATOR(), pPlot->movementCost(this, plot())));
			checkRemoveSelectionAfterAttack();

			getGroup()->clearMissionQueue();
		}
	}
}

void CvUnit::checkRemoveSelectionAfterAttack() {
	if (!canMove() || !isBlitz()) {
		if (IsSelected()) {
			if (gDLL->getInterfaceIFace()->getLengthSelectionList() > 1) {
				gDLL->getInterfaceIFace()->removeFromSelectionList(this);
			}
		}
	}
}


bool CvUnit::isActionRecommended(int iAction) {
	if (getOwnerINLINE() != GC.getGameINLINE().getActivePlayer()) {
		return false;
	}

	if (GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_NO_UNIT_RECOMMENDATIONS)) {
		return false;
	}

	CyUnit* pyUnit = new CyUnit(this);
	CyArgsList argsList;
	argsList.add(gDLL->getPythonIFace()->makePythonObject(pyUnit));	// pass in unit class
	argsList.add(iAction);
	long lResult = 0;
	gDLL->getPythonIFace()->callFunction(PYGameModule, "isActionRecommended", argsList.makeFunctionArgs(), &lResult);
	delete pyUnit;	// python fxn must not hold on to this pointer 
	if (lResult == 1) {
		return true;
	}

	CvPlot* pPlot = gDLL->getInterfaceIFace()->getGotoPlot();
	if (pPlot == NULL) {
		if (GC.shiftKey()) {
			pPlot = getGroup()->lastMissionPlot();
		}
	}

	if (pPlot == NULL) {
		pPlot = plot();
	}

	MissionTypes eMissionType = (MissionTypes)GC.getActionInfo(iAction).getMissionType();
	if (eMissionType == MISSION_FORTIFY) {
		if (pPlot->isCity(true, getTeam())) {
			if (canDefend(pPlot)) {
				if (pPlot->getNumDefenders(getOwnerINLINE()) < ((atPlot(pPlot)) ? 2 : 1)) {
					return true;
				}
			}
		}
	}

	if (eMissionType == MISSION_HEAL || eMissionType == MISSION_SENTRY_WHILE_HEAL) {
		if (isHurt()) {
			if (!hasMoved()) {
				if ((pPlot->getTeam() == getTeam()) || (healTurns(pPlot) < 4)) {
					return true;
				}
			}
		}
	}

	if (eMissionType == MISSION_FOUND) {
		if (canFound(pPlot)) {
			if (pPlot->isBestAdjacentFound(getOwnerINLINE())) {
				return true;
			}
		}
	}

	if (eMissionType == MISSION_BUILD) {
		if (pPlot->getOwnerINLINE() == getOwnerINLINE()) {
			BuildTypes eBuild = (BuildTypes)GC.getActionInfo(iAction).getMissionData();
			FAssert(eBuild != NO_BUILD);
			FAssertMsg(eBuild < GC.getNumBuildInfos(), "Invalid Build");

			if (canBuild(pPlot, eBuild)) {
				/// K-Mod
				if (pPlot->getBuildProgress(eBuild) > 0)
					return true;

				ImprovementTypes eImprovement = (ImprovementTypes)GC.getBuildInfo(eBuild).getImprovement();
				RouteTypes eRoute = (RouteTypes)(GC.getBuildInfo(eBuild).getRoute());
				BonusTypes eBonus = pPlot->getNonObsoleteBonusType(getTeam()); // K-Mod
				CvCity* pWorkingCity = pPlot->getWorkingCity();
				BuildTypes eBestBuild = NO_BUILD; // K-Mod. (I use this again later.)
				if (pWorkingCity) {
					int iIndex = pWorkingCity->getCityPlotIndex(pPlot);
					FAssert(iIndex != -1); // K-Mod. this use to be an if statement in the release code

					eBestBuild = pWorkingCity->AI_getBestBuild(iIndex);

					if (eBestBuild == eBuild)
						return true;
				}

				if (eImprovement != NO_IMPROVEMENT) {

					if (eBonus != NO_BONUS &&
						!GET_PLAYER(getOwnerINLINE()).doesImprovementConnectBonus(pPlot->getImprovementType(), eBonus) &&
						GET_PLAYER(getOwnerINLINE()).doesImprovementConnectBonus(eImprovement, eBonus) &&
						(eBestBuild == NO_BUILD || !GET_PLAYER(getOwnerINLINE()).doesImprovementConnectBonus((ImprovementTypes)GC.getBuildInfo(eBestBuild).getImprovement(), eBonus))) {
						return true;
					}

					if (pPlot->getImprovementType() == NO_IMPROVEMENT && eBonus == NO_BONUS && pWorkingCity == NULL) {
						if (pPlot->getFeatureType() == NO_FEATURE || !GC.getBuildInfo(eBuild).isFeatureRemove((FeatureTypes)pPlot->getFeatureType())) {
							if (GC.getImprovementInfo(eImprovement).isCarriesIrrigation() && !pPlot->isIrrigated() && pPlot->isIrrigationAvailable(true))
								return true;

							if (pPlot->getFeatureType() != NO_FEATURE && GC.getImprovementInfo(eImprovement).getFeatureGrowthProbability() > 0)
								return true;
						}
					}
				}

				if (eRoute != NO_ROUTE) {
					if (!(pPlot->isRoute())) {
						if (eBonus != NO_BONUS) {
							return true;
						}

						if (pWorkingCity != NULL) {
							if (pPlot->isRiver()) {
								return true;
							}
						}
					}

					ImprovementTypes eFinalImprovement = finalImprovementUpgrade(eImprovement != NO_IMPROVEMENT ? eImprovement : pPlot->getImprovementType());

					if (eFinalImprovement != NO_IMPROVEMENT) {
						const CvImprovementInfo& kFinalImprovement = GC.getImprovementInfo(eFinalImprovement);
						if (kFinalImprovement.getRouteYieldChanges(eRoute, YIELD_FOOD) > 0 || kFinalImprovement.getRouteYieldChanges(eRoute, YIELD_PRODUCTION) > 0 || kFinalImprovement.getRouteYieldChanges(eRoute, YIELD_COMMERCE) > 0) {
							return true;
						}
					}
				}
			}
		}
	}

	if (GC.getActionInfo(iAction).getCommandType() == COMMAND_PROMOTION) {
		return true;
	}

	return false;
}


bool CvUnit::isBetterDefenderThan(const CvUnit* pDefender, const CvUnit* pAttacker, int* pBestDefenderRank) const {
	if (pDefender == NULL) {
		return true;
	}

	TeamTypes eAttackerTeam = NO_TEAM;
	if (NULL != pAttacker) {
		eAttackerTeam = pAttacker->getTeam();
	}

	if (canCoexistWithEnemyUnit(eAttackerTeam)) {
		return false;
	}

	if (!canDefend()) {
		return false;
	}

	if (canDefend() && !(pDefender->canDefend())) {
		return true;
	}

	if (pAttacker) {
		if (isTargetOf(*pAttacker) && !pDefender->isTargetOf(*pAttacker)) {
			return true;
		}

		if (!isTargetOf(*pAttacker) && pDefender->isTargetOf(*pAttacker)) {
			return false;
		}

		if (pAttacker->canAttack(*pDefender) && !pAttacker->canAttack(*this)) {
			return false;
		}

		if (pAttacker->canAttack(*this) && !pAttacker->canAttack(*pDefender)) {
			return true;
		}
	}

	// To cut down on changes to existing code, we just short-circuit the method
	// and this point and call our own version instead
	if (GC.getLFBEnable())
		return LFBisBetterDefenderThan(pDefender, pAttacker, pBestDefenderRank);

	int iOurDefense = currCombatStr(plot(), pAttacker);
	if (::isWorldUnitClass(getUnitClassType())) {
		iOurDefense /= 2;
	}

	if (NULL == pAttacker) {
		if (pDefender->collateralDamage() > 0) {
			iOurDefense *= (100 + pDefender->collateralDamage());
			iOurDefense /= 100;
		}

		if (pDefender->currInterceptionProbability() > 0) {
			iOurDefense *= (100 + pDefender->currInterceptionProbability());
			iOurDefense /= 100;
		}
	} else {
		if (!(pAttacker->immuneToFirstStrikes())) {
			iOurDefense *= 100 + (firstStrikes() * 2 + chanceFirstStrikes()) * GC.getCOMBAT_DAMAGE() * 2 / 5;
			iOurDefense /= 100;
		}

		if (immuneToFirstStrikes()) {
			iOurDefense *= 100 + (pAttacker->firstStrikes() * 2 + pAttacker->chanceFirstStrikes()) * GC.getCOMBAT_DAMAGE() * 2 / 5;
			iOurDefense /= 100;
		}
	}

	int iAssetValue = std::max(1, getUnitInfo().getAssetValue());
	int iCargoAssetValue = 0;
	std::vector<CvUnit*> aCargoUnits;
	getCargoUnits(aCargoUnits);
	for (uint i = 0; i < aCargoUnits.size(); ++i) {
		iCargoAssetValue += aCargoUnits[i]->getUnitInfo().getAssetValue();
	}
	iOurDefense = iOurDefense * iAssetValue / std::max(1, iAssetValue + iCargoAssetValue);

	int iTheirDefense = pDefender->currCombatStr(plot(), pAttacker);
	if (::isWorldUnitClass(pDefender->getUnitClassType())) {
		iTheirDefense /= 2;
	}

	if (NULL == pAttacker) {
		if (collateralDamage() > 0) {
			iTheirDefense *= (100 + collateralDamage());
			iTheirDefense /= 100;
		}

		if (currInterceptionProbability() > 0) {
			iTheirDefense *= (100 + currInterceptionProbability());
			iTheirDefense /= 100;
		}
	} else {
		if (!(pAttacker->immuneToFirstStrikes())) {
			iTheirDefense *= 100 + (pDefender->firstStrikes() * 2 + pDefender->chanceFirstStrikes()) * GC.getCOMBAT_DAMAGE() * 2 / 5;
			iTheirDefense /= 100;
		}

		if (pDefender->immuneToFirstStrikes()) {
			iTheirDefense *= 100 + (pAttacker->firstStrikes() * 2 + pAttacker->chanceFirstStrikes()) * GC.getCOMBAT_DAMAGE() * 2 / 5;
			iTheirDefense /= 100;
		}
	}

	iAssetValue = std::max(1, pDefender->getUnitInfo().getAssetValue());
	iCargoAssetValue = 0;
	pDefender->getCargoUnits(aCargoUnits);
	for (uint i = 0; i < aCargoUnits.size(); ++i) {
		iCargoAssetValue += aCargoUnits[i]->getUnitInfo().getAssetValue();
	}
	iTheirDefense = iTheirDefense * iAssetValue / std::max(1, iAssetValue + iCargoAssetValue);

	if (iOurDefense == iTheirDefense) {
		if (NO_UNIT == getLeaderUnitType() && NO_UNIT != pDefender->getLeaderUnitType()) {
			++iOurDefense;
		} else if (NO_UNIT != getLeaderUnitType() && NO_UNIT == pDefender->getLeaderUnitType()) {
			++iTheirDefense;
		} else if (isBeforeUnitCycle(this, pDefender)) {
			++iOurDefense;
		}
	}

	return (iOurDefense > iTheirDefense);
}

bool CvUnit::canDoCommand(CommandTypes eCommand, int iData1, int iData2, bool bTestVisible, bool bTestBusy) {
	if (bTestBusy && getGroup()->isBusy()) {
		return false;
	}

	switch (eCommand) {
	case COMMAND_PROMOTION:
		if (canPromote((PromotionTypes)iData1, iData2)) {
			return true;
		}
		break;

	case COMMAND_UPGRADE:
		if (canUpgrade(((UnitTypes)iData1), bTestVisible)) {
			return true;
		}
		break;

	case COMMAND_AUTOMATE:
		if (canAutomate((AutomateTypes)iData1)) {
			return true;
		}
		break;

	case COMMAND_WAKE:
		if (!isAutomated() && isWaiting()) {
			return true;
		}
		break;

	case COMMAND_CANCEL:
	case COMMAND_CANCEL_ALL:
		if (!isAutomated() && (getGroup()->getLengthMissionQueue() > 0)) {
			return true;
		}
		break;

	case COMMAND_STOP_AUTOMATION:
		if (isAutomated()) {
			return true;
		}
		break;

	case COMMAND_DELETE:
		if (canScrap()) {
			return true;
		}
		break;

	case COMMAND_GIFT:
		if (canGift(bTestVisible)) {
			return true;
		}
		break;

	case COMMAND_LOAD:
		if (canLoad(plot())) {
			return true;
		}
		break;

	case COMMAND_LOAD_UNIT:
	{
		CvUnit* pUnit = ::getUnit(IDInfo(((PlayerTypes)iData1), iData2));
		if (pUnit != NULL) {
			if (canLoadUnit(pUnit, plot())) {
				return true;
			}
		}
	}
	break;

	case COMMAND_UNLOAD:
		if (canUnload()) {
			return true;
		}
		break;

	case COMMAND_UNLOAD_ALL:
		if (canUnloadAll()) {
			return true;
		}
		break;

	case COMMAND_HOTKEY:
		if (isGroupHead()) {
			return true;
		}
		break;

	default:
		FAssert(false);
		break;
	}

	return false;
}


void CvUnit::doCommand(CommandTypes eCommand, int iData1, int iData2) {
	bool bCycle = false;

	FAssert(getOwnerINLINE() != NO_PLAYER);

	if (canDoCommand(eCommand, iData1, iData2)) {
		switch (eCommand) {
		case COMMAND_PROMOTION:
			promote((PromotionTypes)iData1, iData2);
			break;

		case COMMAND_UPGRADE:
			upgrade((UnitTypes)iData1);
			bCycle = true;
			break;

		case COMMAND_AUTOMATE:
			automate((AutomateTypes)iData1);
			bCycle = true;
			break;

		case COMMAND_WAKE:
			getGroup()->setActivityType(ACTIVITY_AWAKE);
			break;

		case COMMAND_CANCEL:
			getGroup()->popMission();
			break;

		case COMMAND_CANCEL_ALL:
			getGroup()->clearMissionQueue();
			break;

		case COMMAND_STOP_AUTOMATION:
			getGroup()->setAutomateType(NO_AUTOMATE);
			break;

		case COMMAND_DELETE:
			scrap();
			bCycle = true;
			break;

		case COMMAND_GIFT:
			gift();
			bCycle = true;
			break;

		case COMMAND_LOAD:
			load();
			bCycle = true;
			break;

		case COMMAND_LOAD_UNIT:
		{
			CvUnit* pUnit = ::getUnit(IDInfo(((PlayerTypes)iData1), iData2));
			if (pUnit != NULL) {
				loadUnit(pUnit);
				bCycle = true;
			}
		}
		break;

		case COMMAND_UNLOAD:
			unload();
			bCycle = true;
			break;

		case COMMAND_UNLOAD_ALL:
			unloadAll();
			bCycle = true;
			break;

		case COMMAND_HOTKEY:
			setHotKeyNumber(iData1);
			break;

		default:
			FAssert(false);
			break;
		}
	}

	if (bCycle) {
		if (IsSelected()) {
			GC.getGameINLINE().cycleSelectionGroups_delayed(1, false);
		}
	}

	getGroup()->doDelayedDeath();
}

CvPlot* CvUnit::getPathEndTurnPlot() const {
	return getGroup()->getPathEndTurnPlot();
}


bool CvUnit::generatePath(const CvPlot* pToPlot, int iFlags, bool bReuse, int* piPathTurns, int iMaxPath) const {
	return getGroup()->generatePath(plot(), pToPlot, iFlags, bReuse, piPathTurns, iMaxPath);
}

// K-Mod. Return the standard pathfinder, for extracting path information.
KmodPathFinder& CvUnit::getPathFinder() const {
	return CvSelectionGroup::path_finder;
}

bool CvUnit::canEnterTerritory(TeamTypes eTeam, bool bIgnoreRightOfPassage) const {
	if (GET_TEAM(getTeam()).isFriendlyTerritory(eTeam)) {
		return true;
	}

	if (eTeam == NO_TEAM) {
		return true;
	}

	if (isEnemy(eTeam)) {
		return true;
	}

	if (isRivalTerritory()) {
		return true;
	}

	if (alwaysInvisible()) {
		return true;
	}

	if (isHiddenNationality()) {
		return true;
	}

	if (!bIgnoreRightOfPassage) {
		if (GET_TEAM(getTeam()).isOpenBorders(eTeam)) {
			return true;
		}

		if (GET_TEAM(getTeam()).isLimitedBorders(eTeam)) {
			if (isOnlyDefensive() || !canFight()) {
				return true;
			}
		}
	}

	if (!GET_TEAM(eTeam).isAlive()) {
		return true;
	}

	return false;
}


bool CvUnit::canEnterArea(TeamTypes eTeam, const CvArea* pArea, bool bIgnoreRightOfPassage) const {
	if (!canEnterTerritory(eTeam, bIgnoreRightOfPassage)) {
		return false;
	}

	if (isBarbarian() && DOMAIN_LAND == getDomainType()) {
		if (eTeam != NO_TEAM && eTeam != getTeam()) {
			if (pArea && pArea->isBorderObstacle(eTeam)) {
				return false;
			}
		}
	}

	return true;
}


// Returns the ID of the team to declare war against
TeamTypes CvUnit::getDeclareWarMove(const CvPlot* pPlot) const {
	FAssert(isHuman());

	if (getDomainType() != DOMAIN_AIR) {
		TeamTypes eRevealedTeam = pPlot->getRevealedTeam(getTeam(), false);

		if (eRevealedTeam != NO_TEAM) {
			if (!canEnterArea(eRevealedTeam, pPlot->area()) || (getDomainType() == DOMAIN_SEA && !canCargoEnterArea(eRevealedTeam, pPlot->area(), false) && getGroup()->isAmphibPlot(pPlot))) {
				if (GET_TEAM(getTeam()).canDeclareWar(pPlot->getTeam())) {
					return eRevealedTeam;
				}
			}
		} else {
			if (pPlot->isActiveVisible(false)) {
				// K-Mod. Don't give the "declare war" popup unless we need war to move into the plot.
				if (canMoveInto(pPlot, true, true, true, false) && !canMoveInto(pPlot, false, false, false, false)) {
					CvUnit* pUnit = pPlot->plotCheck(PUF_canDeclareWar, getOwnerINLINE(), isAlwaysHostile(pPlot), NO_PLAYER, NO_TEAM, PUF_isVisible, getOwnerINLINE());

					if (pUnit != NULL) {
						return pUnit->getTeam();
					}
				}
			}
		}
	}

	return NO_TEAM;
}

bool CvUnit::willRevealByMove(const CvPlot* pPlot) const {
	int iRange = visibilityRange() + 1;
	for (int i = -iRange; i <= iRange; ++i) {
		for (int j = -iRange; j <= iRange; ++j) {
			CvPlot* pLoopPlot = ::plotXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), i, j);
			if (NULL != pLoopPlot) {
				if (!pLoopPlot->isRevealed(getTeam(), false) && pPlot->canSeePlot(pLoopPlot, getTeam(), visibilityRange(), NO_DIRECTION)) {
					return true;
				}
			}
		}
	}

	return false;
}

// K-Mod. I've rearranged a few things to make the function slightly faster, and added "bAssumeVisible" which signals that we should check for units on the plot regardless of whether we can actually see.
bool CvUnit::canMoveInto(const CvPlot* pPlot, bool bAttack, bool bDeclareWar, bool bIgnoreLoad, bool bAssumeVisible) const {
	PROFILE_FUNC();

	FAssertMsg(pPlot != NULL, "Plot is not assigned a valid value");

	if (atPlot(pPlot)) {
		return false;
	}

	// Cannot move around in unrevealed land freely
	if (m_pUnitInfo->isNoRevealMap() && willRevealByMove(pPlot)) {
		return false;
	}

	if (m_pUnitInfo->isSpy() && GC.getUSE_SPIES_NO_ENTER_BORDERS()) {
		if (pPlot->getOwnerINLINE() != NO_PLAYER && !GET_PLAYER(getOwnerINLINE()).canSpiesEnterBorders(pPlot->getOwnerINLINE())) {
			return false;
		}
	}

	CvArea* pPlotArea = pPlot->area();
	TeamTypes ePlotTeam = pPlot->getTeam();
	bool bCanEnterArea = canEnterArea(ePlotTeam, pPlotArea);
	if (bCanEnterArea) {
		if (pPlot->getFeatureType() != NO_FEATURE) {
			if (m_pUnitInfo->getFeatureImpassable(pPlot->getFeatureType())) {
				TechTypes eTech = (TechTypes)m_pUnitInfo->getFeaturePassableTech(pPlot->getFeatureType());
				if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech)) {
					if (DOMAIN_SEA != getDomainType() || pPlot->getTeam() != getTeam())  // sea units can enter impassable in own cultural borders
					{
						return false;
					}
				}
			}
		}

		if (m_pUnitInfo->getTerrainImpassable(pPlot->getTerrainType())) {
			TechTypes eTech = (TechTypes)m_pUnitInfo->getTerrainPassableTech(pPlot->getTerrainType());
			if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech)) {
				if (DOMAIN_SEA != getDomainType() || pPlot->getTeam() != getTeam())  // sea units can enter impassable in own cultural borders
				{
					if (bIgnoreLoad || !canLoad(pPlot)) {
						return false;
					}
				}
			}
		}
	}

	switch (getDomainType()) {
	case DOMAIN_SEA:
		if (!pPlot->isWater() && !canMoveAllTerrain()) {
			if (!pPlot->isFriendlyCity(*this, true) || !pPlot->isCoastalLand()) {
				return false;
			}
		}
		break;

	case DOMAIN_AIR:
		if (!bAttack) {
			bool bValid = false;

			if (pPlot->isFriendlyCity(*this, true)) {
				bValid = true;

				if (m_pUnitInfo->getAirUnitCap() > 0) {
					if (pPlot->airUnitSpaceAvailable(getTeam()) <= 0) {
						bValid = false;
					}
				}
			}

			if (!bValid) {
				if (bIgnoreLoad || !canLoad(pPlot)) {
					return false;
				}
			}
		}

		break;

	case DOMAIN_LAND:
		if (pPlot->isWater() && !canMoveAllTerrain()) {
			if (!pPlot->isCity() || 0 == GC.getDefineINT("LAND_UNITS_CAN_ATTACK_WATER_CITIES")) {
				if (bIgnoreLoad || plot()->isWater() || !canLoad(pPlot)) // K-Mod. (AI might want to load into a boat on the coast)
				{
					return false;
				}
			}
		}
		break;

	case DOMAIN_IMMOBILE:
		return false;
		break;

	default:
		FAssert(false);
		break;
	}

	if (isAnimal()) {
		if (pPlot->isOwned()) {
			return false;
		}

		if (!bAttack) {
			if (pPlot->getBonusType() != NO_BONUS) {
				return false;
			}

			if (pPlot->getImprovementType() != NO_IMPROVEMENT) {
				return false;
			}

			if (pPlot->getNumUnits() > 0) {
				return false;
			}
		}
	}

	if (isNoCapture()) {
		// K-Mod. Don't let noCapture units attack defenceless cities. (eg. cities with a worker in them)
		if (pPlot->isEnemyCity(*this)) {
			if (!bAttack || !pPlot->isVisibleEnemyDefender(this)) {
				return false;
			}
		}
	}

	// The following change makes it possible to capture defenseless units after having 
	// made a previous attack or paradrop
	if (bAttack) {
		if (isMadeAttack() && !isBlitz() && pPlot->isVisibleEnemyDefender(this)) {
			return false;
		}
	}

	if (getDomainType() == DOMAIN_AIR) {
		if (bAttack) {
			if (!canAirStrike(pPlot)) {
				return false;
			}
		}
	} else {
		if (canAttack()) {
			if (bAttack || !canCoexistWithEnemyUnit(NO_TEAM)) {
				if (bAssumeVisible || pPlot->isVisible(getTeam(), false)) {
					if (pPlot->isVisibleEnemyUnit(this) != bAttack) {
						// K-Mod. I'm not entirely sure I understand what they were trying to do here. But I'm pretty sure it's wrong.
						// I think the rule should be that bAttack means we have to actually fight an enemy unit. Capturing an undefended city doesn't not count.
						// (there is no "isVisiblePotentialEnemyUnit" function, so I just wrote the code directly.)
						if (!bAttack || !bDeclareWar || !pPlot->isVisiblePotentialEnemyUnit(getOwnerINLINE())) {
							return false;
						}
					}
				}
			}

			if (bAttack) {
				// K-Mod. (this is much faster.)
				if (!pPlot->hasDefender(true, NO_PLAYER, getOwnerINLINE(), this, true))
					return false;
			}
		} else {
			if (bAttack) {
				return false;
			}

			if (!canCoexistWithEnemyUnit(NO_TEAM)) {
				if (bAssumeVisible || pPlot->isVisible(getTeam(), false)) // K-Mod
				{
					if (pPlot->isEnemyCity(*this)) {
						return false;
					}

					if (pPlot->isVisibleEnemyUnit(this)) {
						return false;
					}
				}
			}
		}

		if (isHuman()) // (should this be !bAssumeVisible? It's a bit different to the other isHuman() checks)
		{
			ePlotTeam = pPlot->getRevealedTeam(getTeam(), false);
			bCanEnterArea = canEnterArea(ePlotTeam, pPlotArea);
		}

		if (!bCanEnterArea) {
			FAssert(ePlotTeam != NO_TEAM);

			if (!(GET_TEAM(getTeam()).canDeclareWar(ePlotTeam))) {
				return false;
			}

			// K-Mod. Rather than allowing the AI to move in forbidden territory when it is planning war.
			// I'm going to disallow it from doing so when _not_ planning war.
			if (!bDeclareWar) {
				return false;
			} else if (!isHuman()) {
				if (!GET_TEAM(getTeam()).AI_isSneakAttackReady(ePlotTeam) || !getGroup()->AI_isDeclareWar(pPlot)) {
					return false;
				}
			}
		}

		bool bValid = false;
		if (pPlot->isImpassable(getTeam())) {
			CLLNode<IDInfo>* pUnitNode;
			CvUnit* pLoopUnit;

			if (pPlot->isPeak()) {
				//Check the current tile for a mountaineer
				pUnitNode = plot()->headUnitNode();
				while (pUnitNode != NULL) {
					pLoopUnit = ::getUnit(pUnitNode->m_data);
					pUnitNode = plot()->nextUnitNode(pUnitNode);
					if (pLoopUnit->isCanMovePeaks() && pLoopUnit->getTeam() == getTeam()) {
						bValid = true;
						break;
					}
				}

				//Check the impassible tile	for a mountaineer
				if (!bValid) {
					pUnitNode = pPlot->headUnitNode();

					while (pUnitNode != NULL) {
						pLoopUnit = ::getUnit(pUnitNode->m_data);
						pUnitNode = pPlot->nextUnitNode(pUnitNode);
						if (pLoopUnit->isCanMovePeaks() && pLoopUnit->getTeam() == getTeam()) {
							bValid = true;
							break;
						}
					}
				}
			}
			if (!bValid) {
				if (!canMoveImpassable()) {
					return false;
				}
			}
		}

		// Is this outside the units range and not getting closer to the home city
		// This check is done last as the range calculation is more expensive that the other
		CvCity* kHomeCity = getCity(m_homeCity);
		if (kHomeCity != NULL) {
			bool bCheckRange = false;
			switch (getRangeType()) {
			case UNITRANGE_HOME:
			{
				// We need to create a temporary plot object as the canWork function will not take a const plot
				CvPlot* pTempPlot = GC.getMapINLINE().plot(pPlot->getX_INLINE(), pPlot->getY_INLINE());
				bCheckRange = !(kHomeCity->isWorkingPlot(pTempPlot) || kHomeCity->canWork(pTempPlot));
			}
			break;
			case UNITRANGE_TERRITORY:
			case UNITRANGE_RANGE:
				bCheckRange = (pPlot->getOwnerINLINE() != getOwnerINLINE());
				break;
			case UNITRANGE_UNLIMITED:
			default:
				bCheckRange = false;
				break;
			}

			if (bCheckRange) {
				int iTargetRange = plotDistance(kHomeCity->plot(), pPlot);
				int iCurrRange = plotDistance(kHomeCity->plot(), plot());
				int iDiagonal = plotDistance(plot(), pPlot);
				if (iTargetRange > getRange() && (iTargetRange >= iCurrRange || iDiagonal >= iCurrRange)) {
					return false;
				}
			}
		}
	}

	if (GC.getUSE_UNIT_CANNOT_MOVE_INTO_CALLBACK()) {
		// Python Override
		CyArgsList argsList;
		argsList.add(getOwnerINLINE());	// Player ID
		argsList.add(getID());	// Unit ID
		argsList.add(pPlot->getX());	// Plot X
		argsList.add(pPlot->getY());	// Plot Y
		long lResult = 0;
		gDLL->getPythonIFace()->callFunction(PYGameModule, "unitCannotMoveInto", argsList.makeFunctionArgs(), &lResult);

		if (lResult != 0) {
			return false;
		}
	}

	return true;
}


bool CvUnit::canMoveOrAttackInto(const CvPlot* pPlot, bool bDeclareWar) const {
	return (canMoveInto(pPlot, false, bDeclareWar) || canMoveInto(pPlot, true, bDeclareWar));
}



void CvUnit::attack(CvPlot* pPlot, bool bQuick) {
	// Note: this assertion could fail in certain situations involving sea-patrol - Karadoc
	FAssert(canMoveInto(pPlot, true));
	FAssert(getCombatTimer() == 0);

	setAttackPlot(pPlot, false);

	updateCombat(bQuick);
}

void CvUnit::fightInterceptor(const CvPlot* pPlot, bool bQuick) {
	FAssert(getCombatTimer() == 0);

	setAttackPlot(pPlot, true);

	updateAirCombat(bQuick);
}

void CvUnit::attackForDamage(CvUnit* pDefender, int attackerDamageChange, int defenderDamageChange) {
	FAssert(getCombatTimer() == 0);
	FAssert(pDefender != NULL);
	FAssert(!isFighting());

	if (pDefender == NULL) {
		return;
	}

	setAttackPlot(pDefender->plot(), false);

	CvPlot* pPlot = getAttackPlot();
	if (pPlot == NULL) {
		return;
	}

	//rotate to face plot
	DirectionTypes newDirection = estimateDirection(this->plot(), pDefender->plot());
	if (newDirection != NO_DIRECTION) {
		setFacingDirection(newDirection);
	}

	//rotate enemy to face us
	newDirection = estimateDirection(pDefender->plot(), this->plot());
	if (newDirection != NO_DIRECTION) {
		pDefender->setFacingDirection(newDirection);
	}

	//check if quick combat
	bool bVisible = isCombatVisible(pDefender);

	//if not finished and not fighting yet, set up combat damage and mission
	if (!isFighting()) {
		if (plot()->isFighting() || pPlot->isFighting()) {
			return;
		}

		setCombatUnit(pDefender, true);
		pDefender->setCombatUnit(this, false);

		pDefender->getGroup()->clearMissionQueue();

		bool bFocused = (bVisible && isCombatFocus() && gDLL->getInterfaceIFace()->isCombatFocus());

		if (bFocused) {
			DirectionTypes directionType = directionXY(plot(), pPlot);
			//								N			NE				E				SE					S				SW					W				NW
			NiPoint2 directions[8] = { NiPoint2(0, 1), NiPoint2(1, 1), NiPoint2(1, 0), NiPoint2(1, -1), NiPoint2(0, -1), NiPoint2(-1, -1), NiPoint2(-1, 0), NiPoint2(-1, 1) };
			NiPoint3 attackDirection = NiPoint3(directions[directionType].x, directions[directionType].y, 0);
			float plotSize = GC.getPLOT_SIZE();
			NiPoint3 lookAtPoint(plot()->getPoint().x + plotSize / 2 * attackDirection.x, plot()->getPoint().y + plotSize / 2 * attackDirection.y, (plot()->getPoint().z + pPlot->getPoint().z) / 2);
			attackDirection.Unitize();
			gDLL->getInterfaceIFace()->lookAt(lookAtPoint, (((getOwnerINLINE() != GC.getGameINLINE().getActivePlayer()) || gDLL->getGraphicOption(GRAPHICOPTION_NO_COMBAT_ZOOM)) ? CAMERALOOKAT_BATTLE : CAMERALOOKAT_BATTLE_ZOOM_IN), attackDirection);
		} else {
			PlayerTypes eAttacker = getVisualOwner(pDefender->getTeam());
			CvWString szMessage;
			if (BARBARIAN_PLAYER != eAttacker) {
				szMessage = gDLL->getText("TXT_KEY_MISC_YOU_UNITS_UNDER_ATTACK", GET_PLAYER(getOwnerINLINE()).getNameKey());
			} else {
				szMessage = gDLL->getText("TXT_KEY_MISC_YOU_UNITS_UNDER_ATTACK_UNKNOWN");
			}

			gDLL->getInterfaceIFace()->addHumanMessage(pDefender->getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szMessage, "AS2D_COMBAT", MESSAGE_TYPE_DISPLAY_ONLY, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE(), true);
		}
	}

	FAssertMsg(plot()->isFighting(), "Current unit instance plot is not fighting as expected");
	FAssertMsg(pPlot->isFighting(), "pPlot is not fighting as expected");

	//setup battle object
	CvBattleDefinition kBattle;
	kBattle.setUnit(BATTLE_UNIT_ATTACKER, this);
	kBattle.setUnit(BATTLE_UNIT_DEFENDER, pDefender);
	kBattle.setDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_BEGIN, getDamage());
	kBattle.setDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_BEGIN, pDefender->getDamage());

	changeDamage(attackerDamageChange, pDefender->getOwnerINLINE());
	pDefender->changeDamage(defenderDamageChange, getOwnerINLINE());

	if (bVisible) {
		kBattle.setDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_END, getDamage());
		kBattle.setDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_END, pDefender->getDamage());
		kBattle.setAdvanceSquare(canAdvance(pPlot, 1));

		// K-Mod. (the original code looks wrong to me.)
		kBattle.setDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_RANGED, kBattle.getDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_BEGIN));
		kBattle.setDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_RANGED, kBattle.getDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_BEGIN));

		std::vector<int> combat_log;
		combat_log.push_back(defenderDamageChange);
		combat_log.push_back(-attackerDamageChange);
		int iTurns = planBattle(kBattle, combat_log);
		kBattle.setMissionTime(iTurns * gDLL->getSecsPerTurn());
		setCombatTimer(iTurns);

		GC.getGameINLINE().incrementTurnTimer(getCombatTimer());

		if (pPlot->isActiveVisible(false)) {
			ExecuteMove(0.5f, true);
			gDLL->getEntityIFace()->AddMission(&kBattle);
		}
	} else {
		setCombatTimer(1);
	}
}


void CvUnit::move(CvPlot* pPlot, bool bShow) {
	FAssert(canMoveOrAttackInto(pPlot) || isMadeAttack());

	CvPlot* pOldPlot = plot();

	changeMoves(pPlot->movementCost(this, plot()));

	setXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), true, true, bShow && pPlot->isVisibleToWatchingHuman(), bShow);

	//change feature
	FeatureTypes featureType = pPlot->getFeatureType();
	if (featureType != NO_FEATURE) {
		CvString featureString(GC.getFeatureInfo(featureType).getOnUnitChangeTo());
		if (!featureString.IsEmpty()) {
			FeatureTypes newFeatureType = (FeatureTypes)GC.getInfoTypeForString(featureString);
			pPlot->setFeatureType(newFeatureType);
		}
	}

	if (getOwnerINLINE() == GC.getGameINLINE().getActivePlayer()) {
		if (!(pPlot->isOwned())) {
			//spawn birds if trees present - JW
			if (featureType != NO_FEATURE) {
				if (GC.getASyncRand().get(100) < GC.getFeatureInfo(featureType).getEffectProbability()) {
					EffectTypes eEffect = (EffectTypes)GC.getInfoTypeForString(GC.getFeatureInfo(featureType).getEffectType());
					gDLL->getEngineIFace()->TriggerEffect(eEffect, pPlot->getPoint(), (float)(GC.getASyncRand().get(360)));
					gDLL->getInterfaceIFace()->playGeneralSound("AS3D_UN_BIRDS_SCATTER", pPlot->getPoint());
				}
			}
		}
	}

	CvEventReporter::getInstance().unitMove(pPlot, this, pOldPlot);
}

// false if unit is killed
// K-Mod, added bForceMove and bGroup
bool CvUnit::jumpToNearestValidPlot(bool bGroup, bool bForceMove) {
	FAssertMsg(!isAttacking(), "isAttacking did not return false as expected");
	FAssertMsg(!isFighting(), "isFighting did not return false as expected");

	CvCity* pNearestCity = GC.getMapINLINE().findCity(getX_INLINE(), getY_INLINE(), getOwnerINLINE());

	int iBestValue = MAX_INT;
	CvPlot* pBestPlot = NULL;

	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (pLoopPlot->isValidDomainForLocation(*this)) {
			if (canMoveInto(pLoopPlot)) {
				if (canEnterArea(pLoopPlot->getTeam(), pLoopPlot->area()) && !isEnemy(pLoopPlot->getTeam(), pLoopPlot)) {
					FAssertMsg(!atPlot(pLoopPlot), "atPlot(pLoopPlot) did not return false as expected");

					if ((getDomainType() != DOMAIN_AIR) || pLoopPlot->isFriendlyCity(*this, true)) {
						if (pLoopPlot->isRevealed(getTeam(), false)) {
							int iValue = (plotDistance(getX_INLINE(), getY_INLINE(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE()) * 2);
							if (bForceMove && iValue == 0)
								continue;

							if (pNearestCity != NULL) {
								iValue += plotDistance(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), pNearestCity->getX_INLINE(), pNearestCity->getY_INLINE());
							}

							if (getDomainType() == DOMAIN_SEA && !plot()->isWater()) {
								if (!pLoopPlot->isWater() || !pLoopPlot->isAdjacentToArea(area())) {
									iValue *= 3;
								}
							} else {
								if (pLoopPlot->area() != area()) {
									iValue *= 3;
								}
							}

							if (iValue < iBestValue) {
								iBestValue = iValue;
								pBestPlot = pLoopPlot;
							}
						}
					}
				}
			}
		}
	}

	bool bValid = true;
	if (pBestPlot != NULL) {
		// K-Mod. If a unit is bumped, we should clear their mission queue
		if (pBestPlot != plot())
			getGroup()->clearMissionQueue();
		setXY(pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), bGroup);
	} else {
		kill(false);
		bValid = false;
	}

	return bValid;
}


bool CvUnit::canAutomate(AutomateTypes eAutomate) const {
	if (eAutomate == NO_AUTOMATE) {
		return false;
	}

	switch (eAutomate) {
	case AUTOMATE_BUILD:
		if ((AI_getUnitAIType() != UNITAI_WORKER) && (AI_getUnitAIType() != UNITAI_WORKER_SEA) && (AI_getUnitAIType() != UNITAI_SLAVE)) {
			return false;
		}
		break;

	case AUTOMATE_NETWORK:
		if ((AI_getUnitAIType() != UNITAI_WORKER) || (AI_getUnitAIType() != UNITAI_SLAVE) || !canBuildRoute()) {
			return false;
		}
		break;

	case AUTOMATE_CITY:
		if ((AI_getUnitAIType() != UNITAI_WORKER) || (AI_getUnitAIType() != UNITAI_SLAVE)) {
			return false;
		}
		break;

	case AUTOMATE_EXPLORE:
		switch (getDomainType()) {
		case DOMAIN_IMMOBILE:
			return false;
		case DOMAIN_LAND:
			return canFight() || isSpy() || alwaysInvisible();
		case DOMAIN_AIR:
			return canRecon(NULL);
		default: // sea
			return true;
		}
		break;

	case AUTOMATE_RELIGION:
		if (AI_getUnitAIType() != UNITAI_MISSIONARY) {
			return false;
		}
		break;

	case AUTOMATE_SHADOW:
		if (!canShadow()) {
			return false;
		}
		break;

	case AUTOMATE_ESPIONAGE:
		if (!isSpy())
			return false;
		break;

	case AUTOMATE_PILLAGE:
		if (!getUnitInfo().isPillage())
			return false;
		break;

	case AUTOMATE_HUNT:
	case AUTOMATE_CITY_DEFENCE:
	case AUTOMATE_BORDER_PATROL:
		if (!canAttack())
			return false;
		break;

	case AUTOMATE_HURRY:
		if (m_pUnitInfo->getBaseHurry() <= 0)
			return false;

		//Do not give ability to great people
		if (m_pUnitInfo->getProductionCost() < 0)
			return false;
		break;

	case AUTOMATE_PIRACY:
		if (getDomainType() != DOMAIN_SEA)
			return false;
		if (!canAttack())
			return false;
		if (!isHiddenNationality() || !isAlwaysHostile())
			return false;
		break;

	case AUTOMATE_AIRSTRIKE:
		if (getDomainType() != DOMAIN_AIR)
			return false;
		if (!canAirAttack())
			return false;
		//Jets and Fighters can intercept, modders, if you have fighters with 0 interception, feel free to get rid of this check
		if (m_pUnitInfo->getInterceptionProbability() <= 0)
			return false;
		break;

	case AUTOMATE_AIRBOMB:
		if (getDomainType() != DOMAIN_AIR)
			return false;
		if (airBombBaseRate() == 0)
			return false;
		if (canAutomate(AUTOMATE_AIRSTRIKE))
			return false;
		break;

	case AUTOMATE_AIR_RECON:
		if (!canRecon(NULL))
			return false;
		break;

	case AUTOMATE_PROMOTIONS:
		if (!canAcquirePromotionAny())
			return false;
		if (isAutoPromoting())
			return false;
		break;

	case AUTOMATE_CANCEL_PROMOTIONS:
		if (!canAcquirePromotionAny())
			return false;
		if (!isAutoPromoting())
			return false;
		break;

	case AUTOMATE_UPGRADING:
		if (GC.getUnitInfo(getUnitType()).getUpgradeUnitClassTypes().size() == 0)
			return false;
		if (isAutoUpgrading())
			return false;
		break;

	case AUTOMATE_CANCEL_UPGRADING:
		if (GC.getUnitInfo(getUnitType()).getUpgradeUnitClassTypes().size() == 0)
			return false;
		if (!isAutoUpgrading())
			return false;
		break;

	default:
		FAssert(false);
		break;
	}

	return true;
}


void CvUnit::automate(AutomateTypes eAutomate) {
	if (!canAutomate(eAutomate)) {
		return;
	}

	if (eAutomate == AUTOMATE_UPGRADING || eAutomate == AUTOMATE_CANCEL_UPGRADING) {
		CLLNode<IDInfo>* pUnitNode = getGroup()->headUnitNode();
		while (pUnitNode != NULL) {
			CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
			pUnitNode = getGroup()->nextUnitNode(pUnitNode);
			pLoopUnit->setAutoUpgrading((eAutomate == AUTOMATE_UPGRADING));
		}
		if (IsSelected()) {
			gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
		}
		return;
	}

	if (eAutomate == AUTOMATE_PROMOTIONS || eAutomate == AUTOMATE_CANCEL_PROMOTIONS) {
		CLLNode<IDInfo>* pUnitNode = getGroup()->headUnitNode();
		while (pUnitNode != NULL) {
			CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
			pUnitNode = getGroup()->nextUnitNode(pUnitNode);
			pLoopUnit->setAutoPromoting(eAutomate == AUTOMATE_PROMOTIONS);
		}
		if (IsSelected())
			gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);

		return;
	}

	getGroup()->setAutomateType(eAutomate);
	// K-Mod. I'd like for the unit to automate immediately after the command is given, just so that the UI seems responsive. But doing so is potentially problematic.
	if (isGroupHead() && GET_PLAYER(getOwnerINLINE()).isTurnActive() && getGroup()->readyToMove()) {
		// unfortunately, CvSelectionGroup::AI_update can kill the unit, and CvUnit::automate currently isn't allowed to kill the unit.
		// CvUnit::AI_update should be ok; but using it here makes me a bit uncomfortable because it doesn't include all the checks and conditions of the group update.
		// ...too bad.
		AI_update();
	}
}


bool CvUnit::canScrap() const {
	if (plot()->isFighting()) {
		return false;
	}

	return true;
}


void CvUnit::scrap() {
	if (!canScrap()) {
		return;
	}

	kill(true);
}


bool CvUnit::canGift(bool bTestVisible, bool bTestTransport) {
	CvPlot* pPlot = plot();
	if (!(pPlot->isOwned())) {
		return false;
	}

	if (pPlot->getOwnerINLINE() == getOwnerINLINE()) {
		return false;
	}

	if (pPlot->isVisibleEnemyUnit(this)) {
		return false;
	}

	if (pPlot->isVisibleEnemyUnit(pPlot->getOwnerINLINE())) {
		return false;
	}

	CvUnit* pTransport = getTransportUnit();
	if (!pPlot->isValidDomainForLocation(*this) && NULL == pTransport) {
		return false;
	}

	for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); ++iCorp) {
		if (m_pUnitInfo->getCorporationSpreads(iCorp) > 0) {
			return false;
		}
	}

	if (bTestTransport) {
		if (pTransport && pTransport->getTeam() != pPlot->getTeam()) {
			return false;
		}
	}

	if (!bTestVisible) {
		if (GET_TEAM(pPlot->getTeam()).isUnitClassMaxedOut(getUnitClassType(), GET_TEAM(pPlot->getTeam()).getUnitClassMaking(getUnitClassType()))) {
			return false;
		}

		if (GET_PLAYER(pPlot->getOwnerINLINE()).isUnitClassMaxedOut(getUnitClassType(), GET_PLAYER(pPlot->getOwnerINLINE()).getUnitClassMaking(getUnitClassType()))) {
			return false;
		}

		if (!(GET_PLAYER(pPlot->getOwnerINLINE()).AI_acceptUnit(this))) {
			return false;
		}
	}

	return !atWar(pPlot->getTeam(), getTeam());
}


void CvUnit::gift(bool bTestTransport) {
	if (!canGift(false, bTestTransport)) {
		return;
	}

	std::vector<CvUnit*> aCargoUnits;
	getCargoUnits(aCargoUnits);
	for (uint i = 0; i < aCargoUnits.size(); ++i) {
		aCargoUnits[i]->gift(false);
	}

	FAssertMsg(plot()->getOwnerINLINE() != NO_PLAYER, "plot()->getOwnerINLINE() is not expected to be equal with NO_PLAYER");
	CvPlayerAI& kRecievingPlayer = GET_PLAYER(plot()->getOwnerINLINE()); // K-Mod

	CvUnit* pGiftUnit = kRecievingPlayer.initUnit(getUnitType(), getX_INLINE(), getY_INLINE(), AI_getUnitAIType());
	FAssertMsg(pGiftUnit != NULL, "GiftUnit is not assigned a valid value");

	pGiftUnit->convert(this);

	PlayerTypes eOwner = getOwnerINLINE();

	if (pGiftUnit->isGoldenAge()) {
		kRecievingPlayer.AI_changeMemoryCount(eOwner, MEMORY_GIVE_HELP, 1);
	}
	// Note: I'm not currently considering special units with < 0 production cost.
	if (pGiftUnit->isCombat()) {
		int iEffectiveWarRating = plot()->area()->getAreaAIType(kRecievingPlayer.getTeam()) != AREAAI_NEUTRAL
			? GET_TEAM(kRecievingPlayer.getTeam()).AI_getWarSuccessRating()
			: 60 - (kRecievingPlayer.AI_isDoStrategy(AI_STRATEGY_ALERT1) ? 20 : 0) - (kRecievingPlayer.AI_isDoStrategy(AI_STRATEGY_ALERT2) ? 20 : 0);

		int iUnitValue = std::max(0, kRecievingPlayer.AI_unitValue(pGiftUnit, pGiftUnit->AI_getUnitAIType(), plot()->area()));
		int iBestValue = kRecievingPlayer.AI_bestAreaUnitAIValue(pGiftUnit->AI_getUnitAIType(), plot()->area());

		int iGiftValue = pGiftUnit->getUnitInfo().getProductionCost() * 4 * std::min(300, 100 * iUnitValue / std::max(1, iBestValue)) / 100;
		iGiftValue *= 100;
		iGiftValue /= std::max(20, 110 + 3 * iEffectiveWarRating);

		if (iUnitValue <= iBestValue && kRecievingPlayer.AI_unitCostPerMil() > kRecievingPlayer.AI_maxUnitCostPerMil(plot()->area())) {
			iGiftValue /= 2;
		}
		kRecievingPlayer.AI_changePeacetimeGrantValue(eOwner, iGiftValue);
		// TODO: It would nice if there was some way this could also reduce "you refused to help us during war time", and stuff like that.
		//       But I think that would probably require some additional AI memory.
	} else {
		kRecievingPlayer.AI_changePeacetimeGrantValue(eOwner, pGiftUnit->getUnitInfo().getProductionCost() / 2);
	}

	CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_GIFTED_UNIT_TO_YOU", GET_PLAYER(eOwner).getNameKey(), pGiftUnit->getNameKey());
	gDLL->getInterfaceIFace()->addHumanMessage(pGiftUnit->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_UNITGIFTED", MESSAGE_TYPE_INFO, pGiftUnit->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_WHITE"), pGiftUnit->getX_INLINE(), pGiftUnit->getY_INLINE(), true, true);

	// Python Event
	CvEventReporter::getInstance().unitGifted(pGiftUnit, getOwnerINLINE(), plot());
}


bool CvUnit::canLoadUnit(const CvUnit* pUnit, const CvPlot* pPlot) const {
	FAssert(pUnit != NULL);
	FAssert(pPlot != NULL);

	if (pUnit == this) {
		return false;
	}

	if (pUnit->getTeam() != getTeam()) {
		return false;
	}

	if (isCargo() && getTransportUnit() == pUnit) {
		return false;
	}

	if (getCargo() > 0) {
		return false;
	}

	if (pUnit->isCargo()) {
		return false;
	}

	if (!(pUnit->cargoSpaceAvailable(getSpecialUnitType(), getDomainType()))) {
		return false;
	}

	if (!(pUnit->atPlot(pPlot))) {
		return false;
	}

	if (!isHiddenNationality() && pUnit->isHiddenNationality()) {
		return false;
	}

	if (NO_SPECIALUNIT != getSpecialUnitType()) {
		if (GC.getSpecialUnitInfo(getSpecialUnitType()).isCityLoad()) {
			if (!pPlot->isCity(true, getTeam())) {
				return false;
			}
		}
	}

	return true;
}


void CvUnit::loadUnit(CvUnit* pUnit) {
	if (!canLoadUnit(pUnit, plot())) {
		return;
	}

	setTransportUnit(pUnit);
}

bool CvUnit::shouldLoadOnMove(const CvPlot* pPlot) const {
	if (isCargo()) {
		return false;
	}

	switch (getDomainType()) {
	case DOMAIN_LAND:
		if (pPlot->isWater() && !canMoveAllTerrain()) {
			return true;
		}
		break;
	case DOMAIN_AIR:
		if (!pPlot->isFriendlyCity(*this, true)) {
			return true;
		}

		if (m_pUnitInfo->getAirUnitCap() > 0) {
			if (pPlot->airUnitSpaceAvailable(getTeam()) <= 0) {
				return true;
			}
		}
		break;
	default:
		break;
	}

	if (m_pUnitInfo->getTerrainImpassable(pPlot->getTerrainType())) {
		TechTypes eTech = (TechTypes)m_pUnitInfo->getTerrainPassableTech(pPlot->getTerrainType());
		if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech)) {
			return true;
		}
	}

	return false;
}


bool CvUnit::canLoad(const CvPlot* pPlot) const {
	PROFILE_FUNC();

	FAssert(pPlot != NULL);

	CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();
	while (pUnitNode != NULL) {
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pPlot->nextUnitNode(pUnitNode);

		if (canLoadUnit(pLoopUnit, pPlot)) {
			return true;
		}
	}

	return false;
}


void CvUnit::load() {
	if (!canLoad(plot())) {
		return;
	}

	CvPlot* pPlot = plot();

	for (int iPass = 0; iPass < 2; iPass++) {
		CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();

		while (pUnitNode != NULL) {
			CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
			pUnitNode = pPlot->nextUnitNode(pUnitNode);

			if (canLoadUnit(pLoopUnit, pPlot)) {
				if ((iPass == 0) ? (pLoopUnit->getOwnerINLINE() == getOwnerINLINE()) : (pLoopUnit->getTeam() == getTeam())) {
					setTransportUnit(pLoopUnit);
					break;
				}
			}
		}

		if (isCargo()) {
			break;
		}
	}
}


bool CvUnit::canUnload() const {
	const CvPlot& kPlot = *(plot());

	if (getTransportUnit() == NULL) {
		return false;
	}

	if (!kPlot.isValidDomainForLocation(*this)) {
		return false;
	}

	if (getDomainType() == DOMAIN_AIR) {
		if (kPlot.isFriendlyCity(*this, true)) {
			int iNumAirUnits = kPlot.countNumAirUnits(getTeam());
			CvCity* pCity = kPlot.getPlotCity();
			if (NULL != pCity) {
				if (iNumAirUnits >= pCity->getAirUnitCapacity(getTeam())) {
					return false;
				}
			} else {
				if (iNumAirUnits >= GC.getDefineINT("CITY_AIR_UNIT_CAPACITY")) {
					return false;
				}
			}
		}
	}

	return true;
}


void CvUnit::unload() {
	if (!canUnload()) {
		return;
	}

	setTransportUnit(NULL);
}


bool CvUnit::canUnloadAll() const {
	if (getCargo() == 0) {
		return false;
	}

	return true;
}


void CvUnit::unloadAll() {
	if (!canUnloadAll()) {
		return;
	}

	std::vector<CvUnit*> aCargoUnits;
	getCargoUnits(aCargoUnits);
	for (uint i = 0; i < aCargoUnits.size(); ++i) {
		CvUnit* pCargo = aCargoUnits[i];
		if (pCargo->canUnload()) {
			pCargo->setTransportUnit(NULL);
		} else {
			FAssert(isHuman() || pCargo->getDomainType() == DOMAIN_AIR);
			pCargo->getGroup()->setActivityType(ACTIVITY_AWAKE);
		}
	}
}


bool CvUnit::canHold(const CvPlot* pPlot) const {
	return true;
}


bool CvUnit::canSleep(const CvPlot* pPlot) const {
	if (isFortifyable()) {
		return false;
	}

	if (getGroup()->getActivityType() == ACTIVITY_SLEEP) // K-Mod
	{
		return false;
	}

	return true;
}


bool CvUnit::canFortify(const CvPlot* pPlot) const {
	if (!isFortifyable()) {
		return false;
	}

	if (getGroup()->getActivityType() == ACTIVITY_SLEEP) // K-Mod
	{
		return false;
	}

	return true;
}


bool CvUnit::canAirPatrol(const CvPlot* pPlot) const {
	if (getDomainType() != DOMAIN_AIR) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (!canAirDefend(pPlot)) {
		return false;
	}

	if (getGroup()->getActivityType() == ACTIVITY_INTERCEPT) // K-Mod
	{
		return false;
	}

	return true;
}


bool CvUnit::canSeaPatrol(const CvPlot* pPlot) const {
	if (!pPlot->isWater()) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (getDomainType() != DOMAIN_SEA) {
		return false;
	}

	if (!canFight() || isOnlyDefensive()) {
		return false;
	}

	if (getGroup()->getActivityType() == ACTIVITY_PATROL) // K-Mod
	{
		return false;
	}

	return true;
}


void CvUnit::airCircle(bool bStart) {
	if (!GC.IsGraphicsInitialized()) {
		return;
	}

	if ((getDomainType() != DOMAIN_AIR) || (maxInterceptionProbability() == 0)) {
		return;
	}

	//cancel previos missions
	gDLL->getEntityIFace()->RemoveUnitFromBattle(this);

	if (bStart) {
		CvAirMissionDefinition kDefinition;
		kDefinition.setPlot(plot());
		kDefinition.setUnit(BATTLE_UNIT_ATTACKER, this);
		kDefinition.setUnit(BATTLE_UNIT_DEFENDER, NULL);
		kDefinition.setMissionType(MISSION_AIRPATROL);
		kDefinition.setMissionTime(1.0f); // patrol is indefinite - time is ignored

		gDLL->getEntityIFace()->AddMission(&kDefinition);
	}
}


bool CvUnit::canHeal(const CvPlot* pPlot) const {
	if (!isHurt())
		return false;

	if (getGroup()->getActivityType() == ACTIVITY_HEAL) // K-Mod
		return false;

	if (healTurns(pPlot) == MAX_INT)
		return false;

	return true;
}


bool CvUnit::canSentry(const CvPlot* pPlot) const {
	if (!canDefend(pPlot)) {
		return false;
	}

	if (getGroup()->getActivityType() == ACTIVITY_SENTRY) // K-Mod
	{
		return false;
	}

	return true;
}


int CvUnit::healRate(const CvPlot* pPlot, bool bLocation, bool bUnits) const {
	PROFILE_FUNC();

	CvCity* pCity = pPlot->getPlotCity();

	int iTotalHeal = 0;

	if (bLocation) // K-Mod
	{
		if (pPlot->isCity(true, getTeam())) {
			iTotalHeal += GC.getDefineINT("CITY_HEAL_RATE") + (GET_TEAM(getTeam()).isFriendlyTerritory(pPlot->getTeam()) ? getExtraFriendlyHeal() : getExtraNeutralHeal());
			if (pCity && !pCity->isOccupation()) {
				iTotalHeal += pCity->getHealRate();
			}
		} else {
			if (!GET_TEAM(getTeam()).isFriendlyTerritory(pPlot->getTeam())) {
				if (isEnemy(pPlot->getTeam(), pPlot)) {
					iTotalHeal += (GC.getDefineINT("ENEMY_HEAL_RATE") + getExtraEnemyHeal());
				} else {
					iTotalHeal += (GC.getDefineINT("NEUTRAL_HEAL_RATE") + getExtraNeutralHeal());
				}
			} else {
				iTotalHeal += (GC.getDefineINT("FRIENDLY_HEAL_RATE") + getExtraFriendlyHeal());
			}
		}
	}

	if (bUnits) // K-Mod
	{
		// XXX optimize this (save it?)
		int iBestHeal = 0;

		CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();

		while (pUnitNode != NULL) {
			CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
			pUnitNode = pPlot->nextUnitNode(pUnitNode);

			if (pLoopUnit->getTeam() == getTeam()) // XXX what about alliances?
			{
				int iHeal = pLoopUnit->getSameTileHeal();

				if (iHeal > iBestHeal) {
					iBestHeal = iHeal;
				}
			}
		}

		for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
			CvPlot* pLoopPlot = plotDirection(pPlot->getX_INLINE(), pPlot->getY_INLINE(), ((DirectionTypes)iI));

			if (pLoopPlot != NULL) {
				if (pLoopPlot->area() == pPlot->area()) {
					pUnitNode = pLoopPlot->headUnitNode();

					while (pUnitNode != NULL) {
						CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
						pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

						if (pLoopUnit->getTeam() == getTeam()) // XXX what about alliances?
						{
							int iHeal = pLoopUnit->getAdjacentTileHeal();

							if (iHeal > iBestHeal) {
								iBestHeal = iHeal;
							}
						}
					}
				}
			}
		}

		iTotalHeal += iBestHeal;
		// XXX
	}

	return iTotalHeal;
}


int CvUnit::healTurns(const CvPlot* pPlot) const {
	if (!isHurt())
		return 0;

	int iHeal = healRate(pPlot);

	FeatureTypes eFeature = pPlot->getFeatureType();
	if (eFeature != NO_FEATURE) {
		iHeal -= GC.getFeatureInfo(eFeature).getTurnDamage();
	}

	if (iHeal > 0) {
		return (getDamage() + iHeal - 1) / iHeal; // K-Mod (same, but faster)
	} else {
		return MAX_INT;
	}
}


void CvUnit::doHeal() {
	changeDamage(-(healRate(plot())));
}


bool CvUnit::canAirlift(const CvPlot* pPlot) const {
	if (getDomainType() != DOMAIN_LAND) {
		return false;
	}

	if (hasMoved()) {
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();

	if (pCity == NULL) {
		return false;
	}

	if (pCity->getCurrAirlift() >= pCity->getMaxAirlift()) {
		return false;
	}

	if (pCity->getTeam() != getTeam()) {
		return false;
	}

	return true;
}


bool CvUnit::canAirliftAt(const CvPlot* pPlot, int iX, int iY) const {
	if (!canAirlift(pPlot)) {
		return false;
	}

	CvPlot* pTargetPlot = GC.getMapINLINE().plotINLINE(iX, iY);

	// canMoveInto use to be here

	CvCity* pTargetCity = pTargetPlot->getPlotCity();

	if (pTargetCity == NULL) {
		return false;
	}

	if (pTargetCity->isAirliftTargeted()) {
		return false;
	}

	if (pTargetCity->getTeam() != getTeam() && !GET_TEAM(pTargetCity->getTeam()).isVassal(getTeam())) {
		return false;
	}

	if (!canMoveInto(pTargetPlot)) // moved by K-Mod
	{
		return false;
	}

	return true;
}


bool CvUnit::airlift(int iX, int iY) {
	if (!canAirliftAt(plot(), iX, iY)) {
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();
	FAssert(pCity != NULL);
	CvPlot* pTargetPlot = GC.getMapINLINE().plotINLINE(iX, iY);
	FAssert(pTargetPlot != NULL);
	CvCity* pTargetCity = pTargetPlot->getPlotCity();
	FAssert(pTargetCity != NULL);
	FAssert(pCity != pTargetCity);

	pCity->changeCurrAirlift(1);
	if (pTargetCity->getMaxAirlift() == 0) {
		pTargetCity->setAirliftTargeted(true);
	}

	finishMoves();

	AutomateTypes eAuto = getGroup()->getAutomateType(); // K-Mod
	setXY(pTargetPlot->getX_INLINE(), pTargetPlot->getY_INLINE());
	getGroup()->setAutomateType(eAuto); // K-Mod. (automated workers sometimes airlift themselves. They should stay automated.)

	return true;
}


bool CvUnit::isNukeVictim(const CvPlot* pPlot, TeamTypes eTeam) const {
	if (!(GET_TEAM(eTeam).isAlive())) {
		return false;
	}

	if (eTeam == getTeam()) {
		return false;
	}

	for (int iDX = -(nukeRange()); iDX <= nukeRange(); iDX++) {
		for (int iDY = -(nukeRange()); iDY <= nukeRange(); iDY++) {
			CvPlot* pLoopPlot = plotXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (pLoopPlot->getTeam() == eTeam) {
					return true;
				}

				if (pLoopPlot->plotCheck(PUF_isCombatTeam, eTeam, getTeam()) != NULL) {
					return true;
				}
			}
		}
	}

	return false;
}


bool CvUnit::canNuke(const CvPlot* pPlot) const {
	if (nukeRange() == -1) {
		return false;
	}

	return true;
}


bool CvUnit::canNukeAt(const CvPlot* pPlot, int iX, int iY) const {
	if (!canNuke(pPlot)) {
		return false;
	}

	int iDistance = plotDistance(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iX, iY);
	if (iDistance <= nukeRange()) {
		return false;
	}

	if (airRange() > 0 && iDistance > airRange()) {
		return false;
	}

	CvPlot* pTargetPlot = GC.getMapINLINE().plotINLINE(iX, iY);

	for (int iI = 0; iI < MAX_TEAMS; iI++) {
		if (isNukeVictim(pTargetPlot, ((TeamTypes)iI))) {
			if (!isEnemy((TeamTypes)iI, pPlot)) {
				return false;
			}
		}
	}

	return true;
}


bool CvUnit::nuke(int iX, int iY) {
	if (!canNukeAt(plot(), iX, iY)) {
		return false;
	}

	CvPlot* pPlot = GC.getMapINLINE().plotINLINE(iX, iY);

	bool abTeamsAffected[MAX_TEAMS];
	for (int iI = 0; iI < MAX_TEAMS; iI++) {
		abTeamsAffected[iI] = isNukeVictim(pPlot, ((TeamTypes)iI));
	}

	for (int iI = 0; iI < MAX_TEAMS; iI++) {
		if (abTeamsAffected[iI]) {
			if (!isEnemy((TeamTypes)iI)) {
				GET_TEAM(getTeam()).declareWar(((TeamTypes)iI), false, WARPLAN_LIMITED);
			}
		}
	}

	int iBestInterception = 0;
	TeamTypes eBestTeam = NO_TEAM;

	for (int iI = 0; iI < MAX_TEAMS; iI++) {
		if (abTeamsAffected[iI]) {
			if (GET_TEAM((TeamTypes)iI).getNukeInterception() > iBestInterception) {
				iBestInterception = GET_TEAM((TeamTypes)iI).getNukeInterception();
				eBestTeam = ((TeamTypes)iI);
			}
		}
	}

	iBestInterception *= (100 - m_pUnitInfo->getEvasionProbability());
	iBestInterception /= 100;

	setReconPlot(pPlot);

	if (GC.getGameINLINE().getSorenRandNum(100, "Nuke") < iBestInterception) {
		for (int iI = 0; iI < MAX_PLAYERS; iI++) {
			// K-Mod. Only show the message to players who have met the teams involved!
			const CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
			if (kLoopPlayer.isAlive() && GET_TEAM(kLoopPlayer.getTeam()).isHasMet(getTeam()) && GET_TEAM(kLoopPlayer.getTeam()).isHasMet(eBestTeam)) {
				CvWString szBuffer;
				szBuffer = gDLL->getText("TXT_KEY_MISC_NUKE_INTERCEPTED", GET_PLAYER(getOwnerINLINE()).getNameKey(), getNameKey(), GET_TEAM(eBestTeam).getName().GetCString());
				gDLL->getInterfaceIFace()->addHumanMessage(((PlayerTypes)iI), (((PlayerTypes)iI) == getOwnerINLINE()), GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_NUKE_INTERCEPTED", MESSAGE_TYPE_MAJOR_EVENT, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE(), true, true);
			}
		}

		if (pPlot->isActiveVisible(false)) {
			// Nuke entity mission
			CvMissionDefinition kDefiniton;
			kDefiniton.setMissionTime(GC.getMissionInfo(MISSION_NUKE).getTime() * gDLL->getSecsPerTurn());
			kDefiniton.setMissionType(MISSION_NUKE);
			kDefiniton.setPlot(pPlot);
			kDefiniton.setUnit(BATTLE_UNIT_ATTACKER, this);
			kDefiniton.setUnit(BATTLE_UNIT_DEFENDER, this);

			// Add the intercepted mission (defender is not NULL)
			gDLL->getEntityIFace()->AddMission(&kDefiniton);
		}

		kill(true);
		return true; // Intercepted!!! (XXX need special event for this...)
	}

	if (pPlot->isActiveVisible(false)) {
		// Nuke entity mission
		CvMissionDefinition kDefiniton;
		kDefiniton.setMissionTime(GC.getMissionInfo(MISSION_NUKE).getTime() * gDLL->getSecsPerTurn());
		kDefiniton.setMissionType(MISSION_NUKE);
		kDefiniton.setPlot(pPlot);
		kDefiniton.setUnit(BATTLE_UNIT_ATTACKER, this);
		kDefiniton.setUnit(BATTLE_UNIT_DEFENDER, NULL);

		// Add the non-intercepted mission (defender is NULL)
		gDLL->getEntityIFace()->AddMission(&kDefiniton);
	}

	setMadeAttack(true);
	setAttackPlot(pPlot, false);

	for (int iI = 0; iI < MAX_TEAMS; iI++) {
		if (abTeamsAffected[iI]) {
			GET_TEAM((TeamTypes)iI).changeWarWeariness(getTeam(), 100 * GC.getDefineINT("WW_HIT_BY_NUKE"));
			GET_TEAM(getTeam()).changeWarWeariness(((TeamTypes)iI), 100 * GC.getDefineINT("WW_ATTACKED_WITH_NUKE"));
			GET_TEAM(getTeam()).AI_changeWarSuccess(((TeamTypes)iI), GC.getDefineINT("WAR_SUCCESS_NUKE"));
		}
	}

	for (int iI = 0; iI < MAX_TEAMS; iI++) {
		if (GET_TEAM((TeamTypes)iI).isAlive()) {
			if (iI != getTeam()) {
				if (abTeamsAffected[iI]) {
					for (int iJ = 0; iJ < MAX_PLAYERS; iJ++) {
						if (GET_PLAYER((PlayerTypes)iJ).isAlive()) {
							if (GET_PLAYER((PlayerTypes)iJ).getTeam() == ((TeamTypes)iI)) {
								GET_PLAYER((PlayerTypes)iJ).AI_changeMemoryCount(getOwnerINLINE(), MEMORY_NUKED_US, 1);
							}
						}
					}
				} else {
					for (int iJ = 0; iJ < MAX_TEAMS; iJ++) {
						if (GET_TEAM((TeamTypes)iJ).isAlive()) {
							if (abTeamsAffected[iJ]) {
								if (GET_TEAM((TeamTypes)iI).isHasMet((TeamTypes)iJ)) {
									if (GET_TEAM((TeamTypes)iI).AI_getAttitude((TeamTypes)iJ) >= ATTITUDE_CAUTIOUS) {
										for (int iK = 0; iK < MAX_PLAYERS; iK++) {
											if (GET_PLAYER((PlayerTypes)iK).isAlive()) {
												if (GET_PLAYER((PlayerTypes)iK).getTeam() == ((TeamTypes)iI)) {
													GET_PLAYER((PlayerTypes)iK).AI_changeMemoryCount(getOwnerINLINE(), MEMORY_NUKED_FRIEND, 1);
												}
											}
										}
										break;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// XXX some AI should declare war here...

	for (int iI = 0; iI < MAX_PLAYERS; iI++) {
		const CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
		if (kLoopPlayer.isAlive() && GET_TEAM(kLoopPlayer.getTeam()).isHasMet(getTeam())) {
			CvWString szBuffer;
			szBuffer = gDLL->getText("TXT_KEY_MISC_NUKE_LAUNCHED", GET_PLAYER(getOwnerINLINE()).getNameKey(), getNameKey());
			gDLL->getInterfaceIFace()->addHumanMessage(((PlayerTypes)iI), (((PlayerTypes)iI) == getOwnerINLINE()), GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_NUKE_EXPLODES", MESSAGE_TYPE_MAJOR_EVENT, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE(), true, true);
		}
	}

	if (isSuicide()) {
		kill(true);
	}

	return true;
}


bool CvUnit::canRecon(const CvPlot* pPlot) const {
	if (getDomainType() != DOMAIN_AIR) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (airRange() == 0) {
		return false;
	}

	if (m_pUnitInfo->isSuicide()) {
		return false;
	}

	return true;
}



bool CvUnit::canReconAt(const CvPlot* pPlot, int iX, int iY) const {
	if (!canRecon(pPlot)) {
		return false;
	}

	int iDistance = plotDistance(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iX, iY);
	if (iDistance > airRange() || 0 == iDistance) {
		return false;
	}

	return true;
}


bool CvUnit::recon(int iX, int iY) {
	if (!canReconAt(plot(), iX, iY)) {
		return false;
	}

	CvPlot* pPlot = GC.getMapINLINE().plotINLINE(iX, iY);

	setReconPlot(pPlot);

	finishMoves();

	if (pPlot->isActiveVisible(false)) {
		CvAirMissionDefinition kAirMission;
		kAirMission.setMissionType(MISSION_RECON);
		kAirMission.setUnit(BATTLE_UNIT_ATTACKER, this);
		kAirMission.setUnit(BATTLE_UNIT_DEFENDER, NULL);
		kAirMission.setDamage(BATTLE_UNIT_DEFENDER, 0);
		kAirMission.setDamage(BATTLE_UNIT_ATTACKER, 0);
		kAirMission.setPlot(pPlot);
		kAirMission.setMissionTime(GC.getMissionInfo((MissionTypes)MISSION_RECON).getTime() * gDLL->getSecsPerTurn());
		gDLL->getEntityIFace()->AddMission(&kAirMission);
	}

	return true;
}


bool CvUnit::canParadrop(const CvPlot* pPlot) const {
	if (getDropRange() <= 0) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (hasMoved()) {
		return false;
	}

	if (!pPlot->isFriendlyCity(*this, true)) {
		return false;
	}

	return true;
}



bool CvUnit::canParadropAt(const CvPlot* pPlot, int iX, int iY) const {
	if (!canParadrop(pPlot)) {
		return false;
	}

	CvPlot* pTargetPlot = GC.getMapINLINE().plotINLINE(iX, iY);
	if (NULL == pTargetPlot || pTargetPlot == pPlot) {
		return false;
	}

	if (!pTargetPlot->isVisible(getTeam(), false)) {
		return false;
	}

	if (!canMoveInto(pTargetPlot, false, false, true)) {
		return false;
	}

	if (plotDistance(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iX, iY) > getDropRange()) {
		return false;
	}

	if (!canCoexistWithEnemyUnit(NO_TEAM)) {
		if (pTargetPlot->isEnemyCity(*this)) {
			return false;
		}

		if (pTargetPlot->isVisibleEnemyUnit(this)) {
			return false;
		}
	}

	return true;
}


bool CvUnit::paradrop(int iX, int iY) {
	if (!canParadropAt(plot(), iX, iY)) {
		return false;
	}

	CvPlot* pPlot = GC.getMapINLINE().plotINLINE(iX, iY);

	changeMoves(GC.getMOVE_DENOMINATOR() / 2);
	setMadeAttack(true);

	setXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), true); // K-Mod

	//check if intercepted
	if (interceptTest(pPlot)) {
		return true;
	}

	//play paradrop animation by itself
	if (pPlot->isActiveVisible(false)) {
		CvAirMissionDefinition kAirMission;
		kAirMission.setMissionType(MISSION_PARADROP);
		kAirMission.setUnit(BATTLE_UNIT_ATTACKER, this);
		kAirMission.setUnit(BATTLE_UNIT_DEFENDER, NULL);
		kAirMission.setDamage(BATTLE_UNIT_DEFENDER, 0);
		kAirMission.setDamage(BATTLE_UNIT_ATTACKER, 0);
		kAirMission.setPlot(pPlot);
		kAirMission.setMissionTime(GC.getMissionInfo((MissionTypes)MISSION_PARADROP).getTime() * gDLL->getSecsPerTurn());
		gDLL->getEntityIFace()->AddMission(&kAirMission);
	}

	return true;
}


bool CvUnit::canAirBomb(const CvPlot* pPlot) const {
	if (getDomainType() != DOMAIN_AIR) {
		return false;
	}

	if (airBombBaseRate() == 0) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (isMadeAttack()) {
		return false;
	}

	return true;
}


bool CvUnit::canAirBombAt(const CvPlot* pPlot, int iX, int iY) const {
	if (!canAirBomb(pPlot)) {
		return false;
	}

	CvPlot* pTargetPlot = GC.getMapINLINE().plotINLINE(iX, iY);
	if (plotDistance(pPlot->getX_INLINE(), pPlot->getY_INLINE(), pTargetPlot->getX_INLINE(), pTargetPlot->getY_INLINE()) > airRange()) {
		return false;
	}

	if (pTargetPlot->isOwned()) {
		if (!potentialWarAction(pTargetPlot)) {
			return false;
		}
	}

	CvCity* pCity = pTargetPlot->getPlotCity();
	if (pCity != NULL) {
		if (!pCity->isBombardable(this) || !pCity->isRevealed(getTeam(), false)) // K-Mod
		{
			return false;
		}
	} else {
		// K-Mod. Don't allow the player to bomb improvements that they don't know exist.
		ImprovementTypes eActualImprovement = pTargetPlot->getImprovementType();
		ImprovementTypes eRevealedImprovement = pTargetPlot->getRevealedImprovementType(getTeam(), false);

		if (eActualImprovement == NO_IMPROVEMENT || eRevealedImprovement == NO_IMPROVEMENT)
			return false;

		if (GC.getImprovementInfo(eActualImprovement).isPermanent() || GC.getImprovementInfo(eRevealedImprovement).isPermanent())
			return false;

		if (GC.getImprovementInfo(eActualImprovement).getAirBombDefense() == -1 || GC.getImprovementInfo(eRevealedImprovement).getAirBombDefense() == -1)
			return false;
	}

	return true;
}


bool CvUnit::airBomb(int iX, int iY) {
	if (!canAirBombAt(plot(), iX, iY)) {
		return false;
	}

	CvPlot* pPlot = GC.getMapINLINE().plotINLINE(iX, iY);

	if (!isEnemy(pPlot->getTeam())) {
		return false;
	}

	if (interceptTest(pPlot)) {
		return true;
	}

	CvCity* pCity = pPlot->getPlotCity();

	if (pCity != NULL) {
		pCity->changeDefenseModifier(-airBombCurrRate());

		CvWString szBuffer;
		szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_DEFENSES_REDUCED_TO", pCity->getNameKey(), pCity->getDefenseModifier(false), getNameKey());
		gDLL->getInterfaceIFace()->addHumanMessage(pCity->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_BOMBARDED", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pCity->getX_INLINE(), pCity->getY_INLINE(), true, true);

		szBuffer = gDLL->getText("TXT_KEY_MISC_ENEMY_DEFENSES_REDUCED_TO", getNameKey(), pCity->getNameKey(), pCity->getDefenseModifier(false));
		gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_BOMBARD", MESSAGE_TYPE_INFO, NULL, (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pCity->getX_INLINE(), pCity->getY_INLINE());
	} else {
		if (pPlot->getImprovementType() != NO_IMPROVEMENT) {
			if (GC.getGameINLINE().getSorenRandNum(airBombCurrRate(), "Air Bomb - Offense") >=
				GC.getGameINLINE().getSorenRandNum(GC.getImprovementInfo(pPlot->getImprovementType()).getAirBombDefense(), "Air Bomb - Defense")) {
				CvWString szBuffer;
				szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_DESTROYED_IMP", getNameKey(), GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide());
				gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE());

				if (pPlot->isOwned()) {
					szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_IMP_WAS_DESTROYED", GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide(), getNameKey(), getVisualCivAdjective(pPlot->getTeam()));
					gDLL->getInterfaceIFace()->addHumanMessage(pPlot->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE(), true, true);
				}

				pPlot->setImprovementType((ImprovementTypes)(GC.getImprovementInfo(pPlot->getImprovementType()).getImprovementPillage()));
			} else {
				CvWString szBuffer;
				szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_FAIL_DESTROY_IMP", getNameKey(), GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide());
				gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_BOMB_FAILS", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE());
			}
		}
	}

	setReconPlot(pPlot);

	setMadeAttack(true);
	changeMoves(GC.getMOVE_DENOMINATOR());

	if (pPlot->isActiveVisible(false)) {
		CvAirMissionDefinition kAirMission;
		kAirMission.setMissionType(MISSION_AIRBOMB);
		kAirMission.setUnit(BATTLE_UNIT_ATTACKER, this);
		kAirMission.setUnit(BATTLE_UNIT_DEFENDER, NULL);
		kAirMission.setDamage(BATTLE_UNIT_DEFENDER, 0);
		kAirMission.setDamage(BATTLE_UNIT_ATTACKER, 0);
		kAirMission.setPlot(pPlot);
		kAirMission.setMissionTime(GC.getMissionInfo((MissionTypes)MISSION_AIRBOMB).getTime() * gDLL->getSecsPerTurn());

		gDLL->getEntityIFace()->AddMission(&kAirMission);
	}

	if (isSuicide()) {
		kill(true);
	}

	return true;
}


CvCity* CvUnit::bombardTarget(const CvPlot* pPlot) const {
	int iBestValue = MAX_INT;
	CvCity* pBestCity = NULL;

	for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
		CvPlot* pLoopPlot = plotDirection(pPlot->getX_INLINE(), pPlot->getY_INLINE(), ((DirectionTypes)iI));

		if (pLoopPlot != NULL) {
			CvCity* pLoopCity = pLoopPlot->getPlotCity();

			if (pLoopCity != NULL) {
				if (pLoopCity->isBombardable(this)) {
					int iValue = pLoopCity->getDefenseDamage();

					// always prefer cities we are at war with
					if (isEnemy(pLoopCity->getTeam(), pPlot)) {
						iValue *= 128;
					}

					if (iValue < iBestValue) {
						iBestValue = iValue;
						pBestCity = pLoopCity;
					}
				}
			}
		}
	}

	return pBestCity;
}


bool CvUnit::canBombard(const CvPlot* pPlot) const {
	if (bombardRate() <= 0) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (isMadeAttack()) {
		return false;
	}

	if (isCargo()) {
		return false;
	}

	if (bombardTarget(pPlot) == NULL) {
		return false;
	}

	return true;
}


bool CvUnit::bombard() {
	CvPlot* pPlot = plot();
	if (!canBombard(pPlot)) {
		return false;
	}

	CvCity* pBombardCity = bombardTarget(pPlot);
	FAssertMsg(pBombardCity != NULL, "BombardCity is not assigned a valid value");

	CvPlot* pTargetPlot = pBombardCity->plot();

	if (!isEnemy(pTargetPlot->getTeam())) {
		return false;
	}

	int iBombardModifier = 0;
	if (!ignoreBuildingDefense()) {
		iBombardModifier -= pBombardCity->getBuildingBombardDefense();
	}

	pBombardCity->changeDefenseModifier(-(bombardRate() * std::max(0, 100 + iBombardModifier)) / 100);

	setMadeAttack(true);
	changeMoves(GC.getMOVE_DENOMINATOR());

	CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_DEFENSES_IN_CITY_REDUCED_TO", pBombardCity->getNameKey(), pBombardCity->getDefenseModifier(false), GET_PLAYER(getOwnerINLINE()).getNameKey());
	gDLL->getInterfaceIFace()->addHumanMessage(pBombardCity->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_BOMBARDED", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pBombardCity->getX_INLINE(), pBombardCity->getY_INLINE(), true, true);

	szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_REDUCE_CITY_DEFENSES", getNameKey(), pBombardCity->getNameKey(), pBombardCity->getDefenseModifier(false));
	gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_BOMBARD", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pBombardCity->getX_INLINE(), pBombardCity->getY_INLINE());

	if (pPlot->isActiveVisible(false)) {
		CvUnit* pDefender = pBombardCity->plot()->getBestDefender(NO_PLAYER, getOwnerINLINE(), this, true);

		// Bombard entity mission
		CvMissionDefinition kDefiniton;
		kDefiniton.setMissionTime(GC.getMissionInfo(MISSION_BOMBARD).getTime() * gDLL->getSecsPerTurn());
		kDefiniton.setMissionType(MISSION_BOMBARD);
		kDefiniton.setPlot(pBombardCity->plot());
		kDefiniton.setUnit(BATTLE_UNIT_ATTACKER, this);
		kDefiniton.setUnit(BATTLE_UNIT_DEFENDER, pDefender);
		gDLL->getEntityIFace()->AddMission(&kDefiniton);
	}

	return true;
}


bool CvUnit::canPillage(const CvPlot* pPlot) const {
	if (!(m_pUnitInfo->isPillage())) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (pPlot->isCity()) {
		return false;
	}

	if (isCargo()) {
		return false;
	}

	if (pPlot->getImprovementType() == NO_IMPROVEMENT) {
		if (!(pPlot->isRoute())) {
			return false;
		}
	} else {
		if (GC.getImprovementInfo(pPlot->getImprovementType()).isPermanent()) {
			return false;
		}
	}

	if (pPlot->isOwned()) {
		if (!potentialWarAction(pPlot)) {
			if (pPlot->getOwnerINLINE() != getOwnerINLINE() || (pPlot->getImprovementType() == NO_IMPROVEMENT && !pPlot->isRoute())) {
				return false;
			}
		}
	}

	if (!(pPlot->isValidDomainForAction(*this))) {
		return false;
	}

	if (isBarbarian()) {
		if (pPlot->getImprovementType() != NO_IMPROVEMENT) {
			if (GC.getImprovementInfo(pPlot->getImprovementType()).isAnySpawn()) {
				return false;
			}
		}
	}

	return true;
}


bool CvUnit::pillage() {
	CvPlot* pPlot = plot();
	if (!canPillage(pPlot)) {
		return false;
	}

	if (pPlot->isOwned()) {
		// we should not be calling this without declaring war first, so do not declare war here
		if (!isEnemy(pPlot->getTeam(), pPlot)) {
			if (pPlot->getOwnerINLINE() != getOwnerINLINE() || (pPlot->getImprovementType() == NO_IMPROVEMENT && !pPlot->isRoute())) {
				return false;
			}
		}
	}

	if (pPlot->isWater()) {
		CvUnit* pInterceptor = bestSeaPillageInterceptor(this, GC.getCOMBAT_DIE_SIDES() / 2);
		if (NULL != pInterceptor) {
			setMadeAttack(false);

			int iWithdrawal = withdrawalProbability();
			changeExtraWithdrawal(-iWithdrawal); // no withdrawal since we are really the defender
			attack(pInterceptor->plot(), false);
			changeExtraWithdrawal(iWithdrawal);

			return false;
		}
	}

	RouteTypes eTempRoute = NO_ROUTE;
	ImprovementTypes eTempImprovement = NO_IMPROVEMENT;
	if (pPlot->getImprovementType() != NO_IMPROVEMENT) {
		eTempImprovement = pPlot->getImprovementType();

		if (pPlot->getTeam() != getTeam()) {
			// Use python to determine pillage amounts...
			long lPillageGold = -1; // K-Mod
			int iPillageGold = 0;

			if (GC.getUSE_DO_PILLAGE_GOLD_CALLBACK()) // K-Mod. I've writen C to replace the python callback.
			{
				CyPlot* pyPlot = new CyPlot(pPlot);
				CyUnit* pyUnit = new CyUnit(this);

				CyArgsList argsList;
				argsList.add(gDLL->getPythonIFace()->makePythonObject(pyPlot));	// pass in plot class
				argsList.add(gDLL->getPythonIFace()->makePythonObject(pyUnit));	// pass in unit class

				gDLL->getPythonIFace()->callFunction(PYGameModule, "doPillageGold", argsList.makeFunctionArgs(), &lPillageGold);

				delete pyPlot;	// python fxn must not hold on to this pointer 
				delete pyUnit;	// python fxn must not hold on to this pointer 

				iPillageGold = (int)lPillageGold;
			}
			// K-Mod. C version of the original python code
			if (lPillageGold < 0) {
				int iPillageBase = GC.getImprovementInfo((ImprovementTypes)pPlot->getImprovementType()).getPillageGold();
				iPillageGold = 0;
				iPillageGold += GC.getGameINLINE().getSorenRandNum(iPillageBase, "Pillage Gold 1");
				iPillageGold += GC.getGameINLINE().getSorenRandNum(iPillageBase, "Pillage Gold 2");
				iPillageGold += getPillageChange() * iPillageGold / 100;
			}

			if (iPillageGold > 0) {
				// Influence driven war culture change
				float fInfluenceRatio = (atWar(pPlot->getTeam(), getTeam()) && GC.getIDW_PILLAGE_INFLUENCE_ENABLED()) ? doPillageInfluence() : 0.0f;

				GET_PLAYER(getOwnerINLINE()).changeGold(iPillageGold);

				CvWString szBuffer;
				szBuffer = gDLL->getText("TXT_KEY_MISC_PLUNDERED_GOLD_FROM_IMP", iPillageGold, GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide());
				if (fInfluenceRatio > 0.0f) {
					CvWString szInfluence;
					szInfluence.Format(L" Tile influence: +%.1f%%", fInfluenceRatio);
					szBuffer += szInfluence;
				}
				gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE());

				if (pPlot->isOwned()) {
					szBuffer = gDLL->getText("TXT_KEY_MISC_IMP_DESTROYED", GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide(), getNameKey(), getVisualCivAdjective(pPlot->getTeam()));
					if (fInfluenceRatio > 0.0f) {
						CvWString szInfluence;
						szInfluence.Format(L" Tile influence: -%.1f%%", fInfluenceRatio);
						szBuffer += szInfluence;
					}
					gDLL->getInterfaceIFace()->addHumanMessage(pPlot->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE(), true, true);
				}
			}
		}

		pPlot->setImprovementType((ImprovementTypes)(GC.getImprovementInfo(pPlot->getImprovementType()).getImprovementPillage()));
	} else if (pPlot->isRoute()) {
		eTempRoute = pPlot->getRouteType();
		pPlot->setRouteType(NO_ROUTE, true); // XXX downgrade rail???
	}

	changeMoves(GC.getMOVE_DENOMINATOR());

	if (pPlot->isActiveVisible(false)) {
		// Pillage entity mission
		CvMissionDefinition kDefiniton;
		kDefiniton.setMissionTime(GC.getMissionInfo(MISSION_PILLAGE).getTime() * gDLL->getSecsPerTurn());
		kDefiniton.setMissionType(MISSION_PILLAGE);
		kDefiniton.setPlot(pPlot);
		kDefiniton.setUnit(BATTLE_UNIT_ATTACKER, this);
		kDefiniton.setUnit(BATTLE_UNIT_DEFENDER, NULL);
		gDLL->getEntityIFace()->AddMission(&kDefiniton);
	}

	if (eTempImprovement != NO_IMPROVEMENT || eTempRoute != NO_ROUTE) {
		CvEventReporter::getInstance().unitPillage(this, eTempImprovement, eTempRoute, getOwnerINLINE());
	}

	return true;
}


bool CvUnit::canPlunder(const CvPlot* pPlot, bool bTestVisible) const {
	if (getDomainType() != DOMAIN_SEA) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (!m_pUnitInfo->isPillage()) {
		return false;
	}

	if (!pPlot->isWater()) {
		return false;
	}

	if (pPlot->isFreshWater()) {
		return false;
	}

	if (!pPlot->isValidDomainForAction(*this)) {
		return false;
	}

	if (!bTestVisible) {
		if (pPlot->getTeam() == getTeam()) {
			return false;
		}
	}

	return true;
}


bool CvUnit::plunder() {
	CvPlot* pPlot = plot();

	if (!canPlunder(pPlot)) {
		return false;
	}

	setBlockading(true);

	finishMoves();

	return true;
}


void CvUnit::updatePlunder(int iChange, bool bUpdatePlotGroups) {
	int iBlockadeRange = GC.getDefineINT("SHIP_BLOCKADE_RANGE");

	bool bChanged = false;
	for (int i = -iBlockadeRange; i <= iBlockadeRange; ++i) {
		for (int j = -iBlockadeRange; j <= iBlockadeRange; ++j) {
			CvPlot* pLoopPlot = ::plotXY(getX_INLINE(), getY_INLINE(), i, j);

			if (NULL != pLoopPlot && pLoopPlot->isWater() && pLoopPlot->area() == area()) {

				int iPathDist = GC.getMapINLINE().calculatePathDistance(plot(), pLoopPlot);

				if ((iPathDist >= 0) && (iPathDist <= iBlockadeRange + 2)) {
					for (int iTeam = 0; iTeam < MAX_TEAMS; ++iTeam) {
						if (isEnemy((TeamTypes)iTeam)) {
							bool bValid = (iPathDist <= iBlockadeRange);
							if (!bValid && (iChange == -1 && pLoopPlot->getBlockadedCount((TeamTypes)iTeam) > 0)) {
								bValid = true;
							}

							if (bValid) {
								bool bOldTradeNet = false;
								if (!bChanged) {
									bOldTradeNet = pLoopPlot->isTradeNetwork((TeamTypes)iTeam);
								}

								pLoopPlot->changeBlockadedCount((TeamTypes)iTeam, iChange);

								if (!bChanged) {
									bChanged = (bOldTradeNet != pLoopPlot->isTradeNetwork((TeamTypes)iTeam));
								}
							}
						}
					}
				}
			}
		}
	}

	if (bChanged) {
		gDLL->getInterfaceIFace()->setDirty(BlockadedPlots_DIRTY_BIT, true);

		if (bUpdatePlotGroups) {
			GC.getGameINLINE().updatePlotGroups();
		}
	}
}


int CvUnit::sabotageCost(const CvPlot* pPlot) const {
	return GC.getDefineINT("BASE_SPY_SABOTAGE_COST");
}


// XXX compare with destroy prob...
int CvUnit::sabotageProb(const CvPlot* pPlot, ProbabilityTypes eProbStyle) const {
	int iDefenseCount = 0;
	int iCounterSpyCount = 0;
	if (pPlot->isOwned()) {
		iDefenseCount = pPlot->plotCount(PUF_canDefend, -1, -1, NO_PLAYER, pPlot->getTeam());
		iCounterSpyCount = pPlot->plotCount(PUF_isCounterSpy, -1, -1, NO_PLAYER, pPlot->getTeam());

		for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
			CvPlot* pLoopPlot = plotDirection(pPlot->getX_INLINE(), pPlot->getY_INLINE(), ((DirectionTypes)iI));

			if (pLoopPlot != NULL) {
				iCounterSpyCount += pLoopPlot->plotCount(PUF_isCounterSpy, -1, -1, NO_PLAYER, pPlot->getTeam());
			}
		}
	} else {
		iDefenseCount = 0;
		iCounterSpyCount = 0;
	}

	if (eProbStyle == PROBABILITY_HIGH) {
		iCounterSpyCount = 0;
	}

	int iProb = 0;
	iProb += (40 / (iDefenseCount + 1)); // XXX

	if (eProbStyle != PROBABILITY_LOW) {
		iProb += (50 / (iCounterSpyCount + 1)); // XXX
	}

	return iProb;
}


bool CvUnit::canSabotage(const CvPlot* pPlot, bool bTestVisible) const {
	if (!m_pUnitInfo->isSabotage()) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (pPlot->getTeam() == getTeam()) {
		return false;
	}

	if (pPlot->isCity()) {
		return false;
	}

	if (pPlot->getImprovementType() == NO_IMPROVEMENT) {
		return false;
	}

	if (!bTestVisible) {
		if (GET_PLAYER(getOwnerINLINE()).getGold() < sabotageCost(pPlot)) {
			return false;
		}
	}

	return true;
}


bool CvUnit::sabotage() {
	if (!canSabotage(plot())) {
		return false;
	}

	CvPlot* pPlot = plot();
	bool bCaught = (GC.getGameINLINE().getSorenRandNum(100, "Spy: Sabotage") > sabotageProb(pPlot));

	GET_PLAYER(getOwnerINLINE()).changeGold(-(sabotageCost(pPlot)));

	if (!bCaught) {
		pPlot->setImprovementType((ImprovementTypes)(GC.getImprovementInfo(pPlot->getImprovementType()).getImprovementPillage()));

		finishMoves();

		CvCity* pNearestCity = GC.getMapINLINE().findCity(pPlot->getX_INLINE(), pPlot->getY_INLINE(), pPlot->getOwnerINLINE(), NO_TEAM, false);

		if (pNearestCity != NULL) {
			CvWString szBuffer;
			szBuffer = gDLL->getText("TXT_KEY_MISC_SPY_SABOTAGED", getNameKey(), pNearestCity->getNameKey());
			gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_SABOTAGE", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE());

			if (pPlot->isOwned()) {
				szBuffer = gDLL->getText("TXT_KEY_MISC_SABOTAGE_NEAR", pNearestCity->getNameKey());
				gDLL->getInterfaceIFace()->addHumanMessage(pPlot->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_SABOTAGE", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE(), true, true);
			}
		}

		if (pPlot->isActiveVisible(false)) {
			NotifyEntity(MISSION_SABOTAGE);
		}
	} else {
		CvWString szBuffer;
		if (pPlot->isOwned()) {
			szBuffer = gDLL->getText("TXT_KEY_MISC_SPY_CAUGHT_AND_KILLED", GET_PLAYER(getOwnerINLINE()).getCivilizationAdjective(), getNameKey());
			gDLL->getInterfaceIFace()->addHumanMessage(pPlot->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSE", MESSAGE_TYPE_INFO);
		}

		szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_SPY_CAUGHT", getNameKey());
		gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSED", MESSAGE_TYPE_INFO);

		if (plot()->isActiveVisible(false)) {
			NotifyEntity(MISSION_SURRENDER);
		}

		if (pPlot->isOwned()) {
			if (!isEnemy(pPlot->getTeam(), pPlot)) {
				GET_PLAYER(pPlot->getOwnerINLINE()).AI_changeMemoryCount(getOwnerINLINE(), MEMORY_SPY_CAUGHT, 1);
			}
		}

		kill(true, pPlot->getOwnerINLINE());
	}

	return true;
}


int CvUnit::destroyCost(const CvPlot* pPlot) const {
	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) {
		return 0;
	}

	bool bLimited = false;

	if (pCity->isProductionUnit()) {
		bLimited = isLimitedUnitClass((UnitClassTypes)(GC.getUnitInfo(pCity->getProductionUnit()).getUnitClassType()));
	} else if (pCity->isProductionBuilding()) {
		bLimited = isLimitedWonderClass((BuildingClassTypes)(GC.getBuildingInfo(pCity->getProductionBuilding()).getBuildingClassType()));
	} else if (pCity->isProductionProject()) {
		bLimited = isLimitedProject(pCity->getProductionProject());
	}

	return (GC.getDefineINT("BASE_SPY_DESTROY_COST") + (pCity->getProduction() * ((bLimited) ? GC.getDefineINT("SPY_DESTROY_COST_MULTIPLIER_LIMITED") : GC.getDefineINT("SPY_DESTROY_COST_MULTIPLIER"))));
}


int CvUnit::destroyProb(const CvPlot* pPlot, ProbabilityTypes eProbStyle) const {
	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) {
		return 0;
	}

	int iProb = 0; // XXX
	int iDefenseCount = pPlot->plotCount(PUF_canDefend, -1, -1, NO_PLAYER, pPlot->getTeam());
	int iCounterSpyCount = pPlot->plotCount(PUF_isCounterSpy, -1, -1, NO_PLAYER, pPlot->getTeam());

	for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
		CvPlot* pLoopPlot = plotDirection(pPlot->getX_INLINE(), pPlot->getY_INLINE(), ((DirectionTypes)iI));

		if (pLoopPlot != NULL) {
			iCounterSpyCount += pLoopPlot->plotCount(PUF_isCounterSpy, -1, -1, NO_PLAYER, pPlot->getTeam());
		}
	}

	if (eProbStyle == PROBABILITY_HIGH) {
		iCounterSpyCount = 0;
	}

	iProb += (25 / (iDefenseCount + 1)); // XXX

	if (eProbStyle != PROBABILITY_LOW) {
		iProb += (50 / (iCounterSpyCount + 1)); // XXX
	}

	iProb += std::min(25, pCity->getProductionTurnsLeft()); // XXX

	return iProb;
}


bool CvUnit::canDestroy(const CvPlot* pPlot, bool bTestVisible) const {
	if (!m_pUnitInfo->isDestroy()) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (pPlot->getTeam() == getTeam()) {
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) {
		return false;
	}

	if (pCity->getProduction() == 0) {
		return false;
	}

	if (!bTestVisible) {
		if (GET_PLAYER(getOwnerINLINE()).getGold() < destroyCost(pPlot)) {
			return false;
		}
	}

	return true;
}


bool CvUnit::destroy() {
	if (!canDestroy(plot())) {
		return false;
	}

	bool bCaught = (GC.getGameINLINE().getSorenRandNum(100, "Spy: Destroy") > destroyProb(plot()));

	CvCity* pCity = plot()->getPlotCity();
	FAssertMsg(pCity != NULL, "City is not assigned a valid value");

	GET_PLAYER(getOwnerINLINE()).changeGold(-(destroyCost(plot())));

	if (!bCaught) {
		pCity->setProduction(pCity->getProduction() / 2);

		finishMoves();

		CvWString szBuffer;
		szBuffer = gDLL->getText("TXT_KEY_MISC_SPY_DESTROYED_PRODUCTION", getNameKey(), pCity->getProductionNameKey(), pCity->getNameKey());
		gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_DESTROY", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pCity->getX_INLINE(), pCity->getY_INLINE());

		szBuffer = gDLL->getText("TXT_KEY_MISC_CITY_PRODUCTION_DESTROYED", pCity->getProductionNameKey(), pCity->getNameKey());
		gDLL->getInterfaceIFace()->addHumanMessage(pCity->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_DESTROY", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pCity->getX_INLINE(), pCity->getY_INLINE(), true, true);

		if (plot()->isActiveVisible(false)) {
			NotifyEntity(MISSION_DESTROY);
		}
	} else {
		CvWString szBuffer;
		szBuffer = gDLL->getText("TXT_KEY_MISC_SPY_CAUGHT_AND_KILLED", GET_PLAYER(getOwnerINLINE()).getCivilizationAdjective(), getNameKey());
		gDLL->getInterfaceIFace()->addHumanMessage(pCity->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSE", MESSAGE_TYPE_INFO);

		szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_SPY_CAUGHT", getNameKey());
		gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSED", MESSAGE_TYPE_INFO);

		if (plot()->isActiveVisible(false)) {
			NotifyEntity(MISSION_SURRENDER);
		}

		if (!isEnemy(pCity->getTeam())) {
			GET_PLAYER(pCity->getOwnerINLINE()).AI_changeMemoryCount(getOwnerINLINE(), MEMORY_SPY_CAUGHT, 1);
		}

		kill(true, pCity->getOwnerINLINE());
	}

	return true;
}


int CvUnit::stealPlansCost(const CvPlot* pPlot) const {
	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) {
		return 0;
	}

	return (GC.getDefineINT("BASE_SPY_STEAL_PLANS_COST") + ((GET_TEAM(pCity->getTeam()).getTotalLand() + GET_TEAM(pCity->getTeam()).getTotalPopulation()) * GC.getDefineINT("SPY_STEAL_PLANS_COST_MULTIPLIER")));
}


// XXX compare with destroy prob...
int CvUnit::stealPlansProb(const CvPlot* pPlot, ProbabilityTypes eProbStyle) const {
	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) {
		return 0;
	}

	int iProb = ((pCity->isGovernmentCenter()) ? 20 : 0); // XXX
	int iDefenseCount = pPlot->plotCount(PUF_canDefend, -1, -1, NO_PLAYER, pPlot->getTeam());
	int iCounterSpyCount = pPlot->plotCount(PUF_isCounterSpy, -1, -1, NO_PLAYER, pPlot->getTeam());

	for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
		CvPlot* pLoopPlot = plotDirection(pPlot->getX_INLINE(), pPlot->getY_INLINE(), ((DirectionTypes)iI));
		if (pLoopPlot != NULL) {
			iCounterSpyCount += pLoopPlot->plotCount(PUF_isCounterSpy, -1, -1, NO_PLAYER, pPlot->getTeam());
		}
	}

	if (eProbStyle == PROBABILITY_HIGH) {
		iCounterSpyCount = 0;
	}

	iProb += (20 / (iDefenseCount + 1)); // XXX

	if (eProbStyle != PROBABILITY_LOW) {
		iProb += (50 / (iCounterSpyCount + 1)); // XXX
	}

	return iProb;
}


bool CvUnit::canStealPlans(const CvPlot* pPlot, bool bTestVisible) const {
	if (!m_pUnitInfo->isStealPlans()) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (pPlot->getTeam() == getTeam()) {
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) {
		return false;
	}

	if (!bTestVisible) {
		if (GET_PLAYER(getOwnerINLINE()).getGold() < stealPlansCost(pPlot)) {
			return false;
		}
	}

	return true;
}


bool CvUnit::stealPlans() {
	if (!canStealPlans(plot())) {
		return false;
	}

	bool bCaught = (GC.getGameINLINE().getSorenRandNum(100, "Spy: Steal Plans") > stealPlansProb(plot()));

	CvCity* pCity = plot()->getPlotCity();
	FAssertMsg(pCity != NULL, "City is not assigned a valid value");

	GET_PLAYER(getOwnerINLINE()).changeGold(-(stealPlansCost(plot())));

	if (!bCaught) {
		GET_TEAM(getTeam()).changeStolenVisibilityTimer(pCity->getTeam(), 2);

		finishMoves();

		CvWString szBuffer;
		szBuffer = gDLL->getText("TXT_KEY_MISC_SPY_STOLE_PLANS", getNameKey(), pCity->getNameKey());
		gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_STEALPLANS", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pCity->getX_INLINE(), pCity->getY_INLINE());

		szBuffer = gDLL->getText("TXT_KEY_MISC_PLANS_STOLEN", pCity->getNameKey());
		gDLL->getInterfaceIFace()->addHumanMessage(pCity->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_STEALPLANS", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pCity->getX_INLINE(), pCity->getY_INLINE(), true, true);

		if (plot()->isActiveVisible(false)) {
			NotifyEntity(MISSION_STEAL_PLANS);
		}
	} else {
		CvWString szBuffer;
		szBuffer = gDLL->getText("TXT_KEY_MISC_SPY_CAUGHT_AND_KILLED", GET_PLAYER(getOwnerINLINE()).getCivilizationAdjective(), getNameKey());
		gDLL->getInterfaceIFace()->addHumanMessage(pCity->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSE", MESSAGE_TYPE_INFO);

		szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_SPY_CAUGHT", getNameKey());
		gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSED", MESSAGE_TYPE_INFO);

		if (plot()->isActiveVisible(false)) {
			NotifyEntity(MISSION_SURRENDER);
		}

		if (!isEnemy(pCity->getTeam())) {
			GET_PLAYER(pCity->getOwnerINLINE()).AI_changeMemoryCount(getOwnerINLINE(), MEMORY_SPY_CAUGHT, 1);
		}

		kill(true, pCity->getOwnerINLINE());
	}

	return true;
}


bool CvUnit::canFound(const CvPlot* pPlot, bool bTestVisible) const {
	if (!isFound()) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (!GET_PLAYER(getOwnerINLINE()).canFound(pPlot->getX_INLINE(), pPlot->getY_INLINE(), bTestVisible)) {
		return false;
	}

	return true;
}


bool CvUnit::found() {
	if (!canFound(plot())) {
		return false;
	}

	if (GC.getGameINLINE().getActivePlayer() == getOwnerINLINE()) {
		gDLL->getInterfaceIFace()->lookAt(plot()->getPoint(), CAMERALOOKAT_NORMAL);
	}

	GET_PLAYER(getOwnerINLINE()).found(getX_INLINE(), getY_INLINE());

	if (plot()->isActiveVisible(false)) {
		NotifyEntity(MISSION_FOUND);
	}

	kill(true);

	return true;
}


bool CvUnit::canSpread(const CvPlot* pPlot, ReligionTypes eReligion, bool bTestVisible) const {
	if (eReligion == NO_RELIGION) {
		return false;
	}

	if (m_pUnitInfo->getReligionSpreads(eReligion) <= 0) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) {
		return false;
	}

	if (pCity->isHasReligion(eReligion)) {
		return false;
	}

	if (!canEnterArea(pPlot->getTeam(), pPlot->area())) {
		return false;
	}

	if (!bTestVisible) {
		if (pCity->getTeam() != getTeam()) {
			if (GET_PLAYER(pCity->getOwnerINLINE()).isNoNonStateReligionSpread()) {
				if (eReligion != GET_PLAYER(pCity->getOwnerINLINE()).getStateReligion()) {
					return false;
				}
			}
		}
	}

	if (GC.getUSE_USE_CANNOT_SPREAD_RELIGION_CALLBACK()) {
		CyArgsList argsList;
		argsList.add(getOwnerINLINE());
		argsList.add(getID());
		argsList.add((int)eReligion);
		argsList.add(pPlot->getX());
		argsList.add(pPlot->getY());
		long lResult = 0;
		gDLL->getPythonIFace()->callFunction(PYGameModule, "cannotSpreadReligion", argsList.makeFunctionArgs(), &lResult);
		if (lResult > 0) {
			return false;
		}
	}

	return true;
}


bool CvUnit::spread(ReligionTypes eReligion) {
	if (!canSpread(plot(), eReligion)) {
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();
	if (pCity != NULL) {
		// K-Mod. A more dynamic formula
		int iPresentReligions = pCity->getReligionCount();
		int iMissingReligions = GC.getNumReligionInfos() - iPresentReligions;
		int iSpreadProb = iPresentReligions * (m_pUnitInfo->getReligionSpreads(eReligion) + pCity->getPopulation()) + iMissingReligions * std::max(100, 100 - 10 * iPresentReligions);
		iSpreadProb /= GC.getNumReligionInfos();

		bool bSuccess;

		if (GC.getGameINLINE().getSorenRandNum(100, "Unit Spread Religion") < iSpreadProb) {
			pCity->setHasReligion(eReligion, true, true, false);
			bSuccess = true;
		} else {
			// K-Mod. Instead of simply failing, give some chance of removing one of the existing religions.
			std::vector<std::pair<int, ReligionTypes> > rankedReligions;
			int iRandomWeight = GC.getDefineINT("RELIGION_INFLUENCE_RANDOM_WEIGHT");
			for (int iI = 0; iI < GC.getNumReligionInfos(); iI++) {
				if (pCity->isHasReligion((ReligionTypes)iI) || iI == eReligion) {
					if (pCity != GC.getGame().getHolyCity((ReligionTypes)iI)) // holy city can't lose its religion!
					{
						int iInfluence = pCity->getReligionGrip((ReligionTypes)iI);
						iInfluence += GC.getGameINLINE().getSorenRandNum(iRandomWeight, "Religion influence");
						iInfluence += (iI == eReligion) ? m_pUnitInfo->getReligionSpreads(eReligion) / 2 : 0;

						rankedReligions.push_back(std::make_pair(iInfluence, (ReligionTypes)iI));
					}
				}
			}
			std::partial_sort(rankedReligions.begin(), rankedReligions.begin() + 1, rankedReligions.end());
			ReligionTypes eFailedReligion = rankedReligions[0].second;
			if (eFailedReligion == eReligion) {
				CvWString szBuffer;
				szBuffer = gDLL->getText("TXT_KEY_MISC_RELIGION_FAILED_TO_SPREAD", getNameKey(), GC.getReligionInfo(eReligion).getChar(), pCity->getNameKey());
				gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_NOSPREAD", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pCity->getX_INLINE(), pCity->getY_INLINE());
				bSuccess = false;
			} else {
				pCity->setHasReligion(eReligion, true, true, false);
				pCity->setHasReligion(eFailedReligion, false, true, false);
				bSuccess = true;
			}
		}

		// Python Event
		CvEventReporter::getInstance().unitSpreadReligionAttempt(this, eReligion, bSuccess);
	}

	if (plot()->isActiveVisible(false)) {
		NotifyEntity(MISSION_SPREAD);
	}

	kill(true);

	return true;
}


bool CvUnit::canSpreadCorporation(const CvPlot* pPlot, CorporationTypes eCorporation, bool bTestVisible) const {
	if (NO_CORPORATION == eCorporation) {
		return false;
	}

	if (!GET_PLAYER(getOwnerINLINE()).isActiveCorporation(eCorporation)) {
		return false;
	}

	if (m_pUnitInfo->getCorporationSpreads(eCorporation) <= 0) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();
	if (NULL == pCity) {
		return false;
	}

	if (pCity->isHasCorporation(eCorporation)) {
		return false;
	}

	if (!canEnterArea(pPlot->getTeam(), pPlot->area())) {
		return false;
	}

	if (!bTestVisible) {
		if (!GET_PLAYER(pCity->getOwnerINLINE()).isActiveCorporation(eCorporation)) {
			return false;
		}

		for (int iCorporation = 0; iCorporation < GC.getNumCorporationInfos(); ++iCorporation) {
			if (pCity->isHeadquarters((CorporationTypes)iCorporation)) {
				if (GC.getGameINLINE().isCompetingCorporation((CorporationTypes)iCorporation, eCorporation)) {
					return false;
				}
			}
		}

		const CvCorporationInfo& kCorp = GC.getCorporationInfo(eCorporation);
		bool bValid = false;
		for (int i = 0; i < kCorp.getNumPrereqBonuses(); ++i) {
			BonusTypes eBonus = (BonusTypes)kCorp.getPrereqBonus(i);
			if (NO_BONUS != eBonus) {
				if (pCity->hasBonus(eBonus)) {
					bValid = true;
					break;
				}
			}
		}

		if (!bValid) {
			return false;
		}

		if (GET_PLAYER(getOwnerINLINE()).getGold() < spreadCorporationCost(eCorporation, pCity)) {
			return false;
		}
	}

	return true;
}

int CvUnit::spreadCorporationCost(CorporationTypes eCorporation, CvCity* pCity) const {
	int iCost = std::max(0, GC.getCorporationInfo(eCorporation).getSpreadCost() * (100 + GET_PLAYER(getOwnerINLINE()).getInflationRate()));
	iCost /= 100;

	if (NULL != pCity) {
		if (getTeam() != pCity->getTeam() && !GET_TEAM(pCity->getTeam()).isVassal(getTeam())) {
			iCost *= GC.getDefineINT("CORPORATION_FOREIGN_SPREAD_COST_PERCENT");
			iCost /= 100;
		}

		for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); ++iCorp) {
			if (iCorp != eCorporation) {
				if (pCity->isActiveCorporation((CorporationTypes)iCorp)) {
					if (GC.getGameINLINE().isCompetingCorporation(eCorporation, (CorporationTypes)iCorp)) {
						iCost *= 100 + GC.getCorporationInfo((CorporationTypes)iCorp).getSpreadFactor();
						iCost /= 100;
					}
				}
			}
		}
	}

	return iCost;
}

bool CvUnit::spreadCorporation(CorporationTypes eCorporation) {
	if (!canSpreadCorporation(plot(), eCorporation)) {
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();
	if (NULL != pCity) {
		GET_PLAYER(getOwnerINLINE()).changeGold(-spreadCorporationCost(eCorporation, pCity));

		int iSpreadProb = m_pUnitInfo->getCorporationSpreads(eCorporation);

		if (pCity->getTeam() != getTeam()) {
			iSpreadProb /= 2;
		}

		iSpreadProb += (((GC.getNumCorporationInfos() - pCity->getCorporationCount()) * (100 - iSpreadProb)) / GC.getNumCorporationInfos());

		if (GC.getGameINLINE().getSorenRandNum(100, "Unit Spread Corporation") < iSpreadProb) {
			pCity->setHasCorporation(eCorporation, true, true, false);
		} else {
			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_CORPORATION_FAILED_TO_SPREAD", getNameKey(), GC.getCorporationInfo(eCorporation).getChar(), pCity->getNameKey());
			gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_NOSPREAD", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pCity->getX_INLINE(), pCity->getY_INLINE());
		}
	}

	if (plot()->isActiveVisible(false)) {
		NotifyEntity(MISSION_SPREAD_CORPORATION);
	}

	kill(true);

	return true;
}


bool CvUnit::canJoin(const CvPlot* pPlot, SpecialistTypes eSpecialist) const {
	if (eSpecialist == NO_SPECIALIST) {
		return false;
	}

	if (!(m_pUnitInfo->getGreatPeoples(eSpecialist) || (getSlaveSpecialistType() == eSpecialist && isSlave()))) {
		return false;
	}

	if (isSlave() && !canWorkCity(pPlot)) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) {
		return false;
	}

	if (!(pCity->canJoin())) {
		return false;
	}

	if (pCity->getTeam() != getTeam()) {
		return false;
	}

	if (isDelayedDeath()) {
		return false;
	}

	return true;
}


bool CvUnit::join(SpecialistTypes eSpecialist) {
	if (!canJoin(plot(), eSpecialist)) {
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();
	if (pCity != NULL) {
		pCity->changeFreeSpecialistCount(eSpecialist, 1);

		if (isSlave()) {
			pCity->changeSettledSlaveCount(eSpecialist, 1);
			// We need to increase the players slave count here as it will be reduced when we kill the slave
			GET_PLAYER(getOwnerINLINE()).changeNumSlaves(1);
		}
	}

	if (plot()->isActiveVisible(false)) {
		NotifyEntity(MISSION_JOIN);
	}

	kill(true);

	return true;
}


bool CvUnit::canConstruct(const CvPlot* pPlot, BuildingTypes eBuilding, bool bTestVisible) const {
	if (eBuilding == NO_BUILDING) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) {
		return false;
	}

	if (getTeam() != pCity->getTeam()) {
		return false;
	}

	if (pCity->getNumRealBuilding(eBuilding) > 0) {
		return false;
	}

	if (!(m_pUnitInfo->getForceBuildings(eBuilding))) {
		if (!(m_pUnitInfo->getBuildings(eBuilding))) {
			return false;
		}

		if (!(pCity->canConstruct(eBuilding, false, bTestVisible, true))) {
			return false;
		}
	}

	if (isDelayedDeath()) {
		return false;
	}

	return true;
}


bool CvUnit::construct(BuildingTypes eBuilding) {
	if (!canConstruct(plot(), eBuilding)) {
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();
	if (pCity != NULL) {
		pCity->setNumRealBuilding(eBuilding, pCity->getNumRealBuilding(eBuilding) + 1);

		CvEventReporter::getInstance().buildingBuilt(pCity, eBuilding);
	}

	if (plot()->isActiveVisible(false)) {
		NotifyEntity(MISSION_CONSTRUCT);
	}

	kill(true);

	return true;
}


TechTypes CvUnit::getDiscoveryTech() const {
	return ::getDiscoveryTech(getUnitType(), getOwnerINLINE());
}


int CvUnit::getDiscoverResearch(TechTypes eTech) const {
	int iResearch = (m_pUnitInfo->getBaseDiscover() + (m_pUnitInfo->getDiscoverMultiplier() * GET_TEAM(getTeam()).getTotalPopulation()));

	iResearch *= GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getUnitDiscoverPercent();
	iResearch /= 100;

	if (eTech != NO_TECH) {
		iResearch = std::min(GET_TEAM(getTeam()).getResearchLeft(eTech), iResearch);
	}

	return std::max(0, iResearch);
}


bool CvUnit::canDiscover(const CvPlot* pPlot) const {
	TechTypes eTech = getDiscoveryTech();
	if (eTech == NO_TECH) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (getDiscoverResearch(eTech) == 0) {
		return false;
	}

	if (isDelayedDeath()) {
		return false;
	}

	return true;
}

bool CvUnit::discover() {
	return discover(getDiscoveryTech());
}

bool CvUnit::discover(TechTypes eTech) {
	if (!canDiscover(plot())) {
		return false;
	}

	FAssertMsg(eTech != NO_TECH, "Tech is not assigned a valid value");

	GET_TEAM(getTeam()).changeResearchProgress(eTech, getDiscoverResearch(eTech), getOwnerINLINE());

	// K-Mod. If the AI bulbs something, let them reconsider their current research.
	CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());
	if (!kOwner.isHuman() && kOwner.getCurrentResearch() != eTech) {
		kOwner.clearResearchQueue();
	}

	if (plot()->isActiveVisible(false)) {
		NotifyEntity(MISSION_DISCOVER);
	}

	kill(true);

	return true;
}


int CvUnit::getMaxHurryProduction(CvCity* pCity) const {
	int iProduction = (m_pUnitInfo->getBaseHurry() + (m_pUnitInfo->getHurryMultiplier() * pCity->getPopulation()));

	iProduction *= GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getUnitHurryPercent();
	iProduction /= 100;

	return std::max(0, iProduction);
}


int CvUnit::getHurryProduction(const CvPlot* pPlot) const {
	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) {
		return 0;
	}

	int iProduction = getMaxHurryProduction(pCity);

	iProduction = std::min(pCity->productionLeft(), iProduction);

	return std::max(0, iProduction);
}


bool CvUnit::canHurry(const CvPlot* pPlot, bool bTestVisible) const {
	if (isDelayedDeath()) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (getHurryProduction(pPlot) == 0) {
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) {
		return false;
	}

	if (pCity->getProductionTurnsLeft() == 1) {
		return false;
	}

	if (!bTestVisible) {
		if (!(pCity->isProductionBuilding())) {
			return false;
		}
	}

	return true;
}


bool CvUnit::hurry() {
	if (!canHurry(plot())) {
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();
	if (pCity != NULL) {
		pCity->changeProduction(getHurryProduction(plot()));
	}

	if (plot()->isActiveVisible(false)) {
		NotifyEntity(MISSION_HURRY);
	}

	kill(true);

	return true;
}


int CvUnit::getTradeGold(const CvPlot* pPlot) const {
	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) {
		return 0;
	}

	CvCity* pCapitalCity = GET_PLAYER(getOwnerINLINE()).getCapitalCity();
	int iGold = (m_pUnitInfo->getBaseTrade() + (m_pUnitInfo->getTradeMultiplier() * ((pCapitalCity != NULL) ? pCity->calculateTradeProfit(pCapitalCity) : 0)));

	iGold *= GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getUnitTradePercent();
	iGold /= 100;

	//More Gold From Free Trade Agreement Trade Missions
	PlayerTypes eTargetPlayer = pPlot->getOwnerINLINE();
	if (GET_TEAM(getTeam()).isFreeTradeAgreement(GET_PLAYER(eTargetPlayer).getTeam())) {
		iGold *= 100 + GC.getFREE_TRADE_AGREEMENT_TRADE_MODIFIER();
		iGold /= 100;
	}

	//Gold Sound 
	if (plot()->isActiveVisible(false)) {
		gDLL->getInterfaceIFace()->playGeneralSound("AS2D_COINS");
	}

	return std::max(0, iGold);
}


bool CvUnit::canTrade(const CvPlot* pPlot, bool bTestVisible) const {
	if (isDelayedDeath()) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) {
		return false;
	}

	// K-Mod. if (getTradeGold(pPlot) == 0) use to be here. I've moved it to the bottom, for efficiency.

	if (!canEnterArea(pPlot->getTeam(), pPlot->area())) {
		return false;
	}

	if (!bTestVisible) {
		if (pCity->getTeam() == getTeam()) {
			return false;
		}
	}

	if (getTradeGold(pPlot) == 0) {
		return false;
	}

	return true;
}


bool CvUnit::trade() {
	if (!canTrade(plot())) {
		return false;
	}

	GET_PLAYER(getOwnerINLINE()).changeGold(getTradeGold(plot()));

	if (plot()->isActiveVisible(false)) {
		NotifyEntity(MISSION_TRADE);
	}

	kill(true);

	return true;
}


int CvUnit::getGreatWorkCulture(const CvPlot* pPlot) const {
	int iCulture = m_pUnitInfo->getGreatWorkCulture() * (GET_PLAYER(getOwnerINLINE()).getCurrentEra());
	iCulture *= GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getUnitGreatWorkPercent();
	iCulture /= 100;

	return std::max(0, iCulture);
}


bool CvUnit::canGreatWork(const CvPlot* pPlot) const {
	if (isDelayedDeath()) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) {
		return false;
	}

	if (pCity->getOwnerINLINE() != getOwnerINLINE()) {
		return false;
	}

	if (getGreatWorkCulture(pPlot) == 0) {
		return false;
	}

	return true;
}


bool CvUnit::greatWork() {
	if (!canGreatWork(plot())) {
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();
	if (pCity != NULL) {
		pCity->setCultureUpdateTimer(0);
		pCity->setOccupationTimer(0);

		int iCultureToAdd = 100 * getGreatWorkCulture(plot());
		pCity->changeCultureTimes100(getOwnerINLINE(), iCultureToAdd, true, true);
		GET_PLAYER(getOwnerINLINE()).AI_updateCommerceWeights(); // significant culture change may cause signficant weight changes.
	}

	if (plot()->isActiveVisible(false)) {
		NotifyEntity(MISSION_GREAT_WORK);
	}

	kill(true);

	return true;
}


int CvUnit::getEspionagePoints(const CvPlot* pPlot) const {
	int iEspionagePoints = m_pUnitInfo->getEspionagePoints();
	iEspionagePoints *= GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getUnitGreatWorkPercent();
	iEspionagePoints /= 100;

	return std::max(0, iEspionagePoints);
}

bool CvUnit::canInfiltrate(const CvPlot* pPlot, bool bTestVisible) const {
	if (isDelayedDeath()) {
		return false;
	}

	if (GC.getGameINLINE().isOption(GAMEOPTION_NO_ESPIONAGE)) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (getEspionagePoints(NULL) == 0) {
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL || pCity->isBarbarian()) {
		return false;
	}

	if (!bTestVisible) {
		if (NULL != pCity && pCity->getTeam() == getTeam()) {
			return false;
		}
	}

	return true;
}


bool CvUnit::infiltrate() {
	if (!canInfiltrate(plot())) {
		return false;
	}

	int iPoints = getEspionagePoints(NULL);
	GET_TEAM(getTeam()).changeEspionagePointsAgainstTeam(GET_PLAYER(plot()->getOwnerINLINE()).getTeam(), iPoints);
	GET_TEAM(getTeam()).changeEspionagePointsEver(iPoints);

	if (plot()->isActiveVisible(false)) {
		NotifyEntity(MISSION_INFILTRATE);
	}

	kill(true);

	return true;
}


bool CvUnit::canEspionage(const CvPlot* pPlot, bool bTestVisible) const {
	if (isDelayedDeath()) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (!isSpy()) {
		return false;
	}

	if (GC.getGameINLINE().isOption(GAMEOPTION_NO_ESPIONAGE)) {
		return false;
	}

	PlayerTypes ePlotOwner = pPlot->getOwnerINLINE();
	if (NO_PLAYER == ePlotOwner) {
		return false;
	}

	const CvPlayer& kTarget = GET_PLAYER(ePlotOwner);

	if (kTarget.isBarbarian()) {
		return false;
	}

	if (kTarget.getTeam() == getTeam()) {
		return false;
	}

	if (GET_TEAM(getTeam()).isVassal(kTarget.getTeam())) {
		return false;
	}

	if (!bTestVisible) {
		if (isMadeAttack()) {
			return false;
		}

		if (hasMoved()) {
			return false;
		}

		if (kTarget.getTeam() != getTeam() && !isInvisible(kTarget.getTeam(), false)) {
			return false;
		}
	}

	return true;
}

bool CvUnit::espionage(EspionageMissionTypes eMission, int iData) {
	if (!canEspionage(plot())) {
		return false;
	}

	if (NO_ESPIONAGEMISSION == eMission) {
		FAssert(GET_PLAYER(getOwnerINLINE()).isHuman());
		CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_DOESPIONAGE);
		if (NULL != pInfo) {
			gDLL->getInterfaceIFace()->addPopup(pInfo, getOwnerINLINE(), true);
		}
	} else if (GC.getEspionageMissionInfo(eMission).isTwoPhases() && -1 == iData) {
		FAssert(GET_PLAYER(getOwnerINLINE()).isHuman());
		CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_DOESPIONAGE_TARGET);
		if (NULL != pInfo) {
			pInfo->setData1(eMission);
			gDLL->getInterfaceIFace()->addPopup(pInfo, getOwnerINLINE(), true);
		}
	} else {
		PlayerTypes eTargetPlayer = plot()->getOwnerINLINE();

		bool bReveal = false; // Whether the spy reveals their civ

		// Check if the spy is intercepted before they get chance to complete their mission.
		if (testSpyIntercepted(eTargetPlayer, true, bReveal, GC.getEspionageMissionInfo(eMission).getDifficultyMod())) {
			return false;
		}

		// Spy gets to perform the mission, we check whether his identity was revealed at the start as this impacts some of the strings
		//  displayed during the doEspionageMission call ... this is a hack and should be tidied up at some point
		bool bCaught = testSpyIntercepted(eTargetPlayer, true, bReveal, GC.getDefineINT("ESPIONAGE_SPY_MISSION_ESCAPE_MOD"));
		if (GET_PLAYER(getOwnerINLINE()).doEspionageMission(eMission, eTargetPlayer, plot(), iData, this, bReveal)) {
			if (plot()->isActiveVisible(false)) {
				NotifyEntity(MISSION_ESPIONAGE);
			}

			if (!bCaught)
			{
				setFortifyTurns(0);
				setMadeAttack(true);
				finishMoves();

				bool bAutomated = getGroup()->getAutomateType() == AUTOMATE_ESPIONAGE;

				if (!plot()->isCity()) // Actions that aren't in a city such as improvement destruction don't cause the spy to be sent back
				{
					CvCity* pNearestCity = GC.getMapINLINE().findCity(plot()->getX_INLINE(), plot()->getY_INLINE(), plot()->getOwnerINLINE(), NO_TEAM, false);
					CvWString szBuffer = gDLL->getText("TXT_KEY_ESPIONAGE_SPY_SUCCESS_OUTSIDE", getNameKey(), pNearestCity->getNameKey());
					gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_WHITE"), pNearestCity->getX_INLINE(), pNearestCity->getY_INLINE(), true, true);
				} else {
					// Check the best city to return to after our mission
					CvCity* pReturnCity = getClosestSafeCity();

					if (pReturnCity) // Require complete kills game option may be enabled so we can't assume there is a capital city
					{
						setXY(pReturnCity->getX_INLINE(), pReturnCity->getY_INLINE(), false, false, false);

						CvWString szBuffer = gDLL->getText("TXT_KEY_ESPIONAGE_SPY_SUCCESS", getNameKey(), pReturnCity->getNameKey());
						gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_WHITE"), pReturnCity->getX_INLINE(), pReturnCity->getY_INLINE(), true, true);
					}
				}

				if (bAutomated) {
					getGroup()->setAutomateType(AUTOMATE_ESPIONAGE);
				}

				// Give spies experience for successful missions
				awardSpyExperience(GET_PLAYER(eTargetPlayer).getTeam(), GC.getEspionageMissionInfo(eMission).getDifficultyMod());
			}
			if (getTeam() == GC.getGameINLINE().getActiveTeam())
				gDLL->getInterfaceIFace()->setDirty(CityInfo_DIRTY_BIT, true);
			return true;
		}
	}

	return false;
}

bool CvUnit::testSpyIntercepted(PlayerTypes eTargetPlayer, bool bMission, bool& bReveal, int iModifier) {
	CvPlayer& kTargetPlayer = GET_PLAYER(eTargetPlayer);

	if (kTargetPlayer.isBarbarian()) {
		return false;
	}

	if (GC.getGameINLINE().getSorenRandNum(10000, "Spy Interception") >= getSpyInterceptPercent(kTargetPlayer.getTeam(), bMission) * (100 + iModifier)) {
		return false;
	}

	//
	// At this point we know the spy has failed their mission
	CvString szFormatNoReveal;
	CvString szFormatReveal;
	CvString szFormatRevealTurned;
	CvString szFormatYou;

	if (GET_TEAM(kTargetPlayer.getTeam()).getCounterespionageModAgainstTeam(getTeam()) > 0) { // Counterespionage
		szFormatNoReveal = "TXT_KEY_SPY_INTERCEPTED_MISSION";
		szFormatReveal = "TXT_KEY_SPY_INTERCEPTED_MISSION_REVEAL";
		szFormatRevealTurned = "TXT_KEY_SPY_INTERCEPTED_MISSION_REVEAL_TURNED";
		szFormatYou = "TXT_KEY_SPY_YOU_INTERCEPTED_MISSION";
	} else if (plot()->isEspionageCounterSpy(kTargetPlayer.getTeam())) { // Spy presence
		szFormatNoReveal = "TXT_KEY_SPY_INTERCEPTED_SPY";
		szFormatReveal = "TXT_KEY_SPY_INTERCEPTED_SPY_REVEAL";
		szFormatRevealTurned = "TXT_KEY_SPY_INTERCEPTED_SPY_REVEAL_TURNED";
		szFormatYou = "TXT_KEY_SPY_YOU_INTERCEPTED_SPY";
	} else { // Chance
		szFormatNoReveal = "TXT_KEY_SPY_INTERCEPTED";
		szFormatReveal = "TXT_KEY_SPY_INTERCEPTED_REVEAL";
		szFormatRevealTurned = "TXT_KEY_SPY_INTERCEPTED_REVEAL_TURNED";
		szFormatYou = "TXT_KEY_SPY_YOU_INTERCEPTED";
	}

	CvWString szCityName = kTargetPlayer.getCivilizationShortDescription();
	CvCity* pClosestCity = GC.getMapINLINE().findCity(getX_INLINE(), getY_INLINE(), eTargetPlayer, kTargetPlayer.getTeam(), true, false);
	if (pClosestCity != NULL) {
		szCityName = pClosestCity->getName();
	}

	CvWString szBuffer = gDLL->getText(szFormatYou.GetCString(), getNameKey(), kTargetPlayer.getCivilizationAdjectiveKey(), szCityName.GetCString());
	gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSED", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), getX_INLINE(), getY_INLINE(), true, true);

	// Determine if the target identifies the owner of the spy
	bool bDoubleAgent = false;
	if (!isLoyal() && GC.getGameINLINE().getSorenRandNum(100, "Spy Reveal identity") < GC.getDefineINT("ESPIONAGE_SPY_REVEAL_IDENTITY_PERCENT")) {
		bReveal = true;
		// Determine whether the spy can be turned to provide the target a double agent
		bDoubleAgent = GC.getGameINLINE().getSorenRandNum(100, "Spy turning") < GC.getDOUBLE_AGENT_CREATE_CHANCE();

		if (!isEnemy(kTargetPlayer.getTeam())) {
			GET_PLAYER(eTargetPlayer).AI_changeMemoryCount(getOwnerINLINE(), MEMORY_SPY_CAUGHT, 1);
		}

		// If the target turns the agent they may also gain some espionage points against the owner
		int iGainPoints = 0;
		if (bDoubleAgent) {
			iGainPoints = GC.getGameINLINE().getSorenRandNum(200, "Captured Intel");
			szBuffer = gDLL->getText(szFormatRevealTurned.GetCString(), GET_PLAYER(getOwnerINLINE()).getCivilizationAdjectiveKey(), getNameKey(), kTargetPlayer.getCivilizationAdjectiveKey(), szCityName.GetCString());
		}

		gDLL->getInterfaceIFace()->addHumanMessage(eTargetPlayer, true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSE", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), getX_INLINE(), getY_INLINE(), true, true);

		if (iGainPoints > 0) {
			GET_TEAM(kTargetPlayer.getTeam()).changeEspionagePointsAgainstTeam(GET_PLAYER(getOwnerINLINE()).getTeam(), iGainPoints);

			szBuffer = gDLL->getText("TXT_KEY_MISC_DOUBLE_AGENT_INTEL", GET_PLAYER(getOwnerINLINE()).getCivilizationAdjectiveKey(), iGainPoints);
			gDLL->getInterfaceIFace()->addHumanMessage(eTargetPlayer, true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), getX_INLINE(), getY_INLINE(), true, true);
		}

	} else {
		bReveal = false;
		szBuffer = gDLL->getText(szFormatNoReveal.GetCString(), getNameKey(), kTargetPlayer.getCivilizationAdjectiveKey(), szCityName.GetCString());
		gDLL->getInterfaceIFace()->addHumanMessage(eTargetPlayer, true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSE", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), getX_INLINE(), getY_INLINE(), true, true);
	}

	if (plot()->isActiveVisible(false)) {
		NotifyEntity(MISSION_SURRENDER);
	}

	// Give xp to spy who catches spy
	CvUnit* pCounterUnit = plot()->plotCheck(PUF_isCounterSpy, -1, -1, NO_PLAYER, kTargetPlayer.getTeam());
	if (NULL != pCounterUnit) {
		pCounterUnit->changeExperience(1);
		pCounterUnit->testPromotionReady();
	}

	// The spy is intercepted, but they manage to escape before they are interrogated
	if (GC.getGameINLINE().getSorenRandNum(100, "Spy Escape Chance") < getSpyEscapeChance()) {
		setFortifyTurns(0);
		setMadeAttack(true);
		finishMoves();

		// Get the best city to escape to
		CvCity* pEscapeToCity = getClosestSafeCity();
		if (pEscapeToCity) {
			setXY(pEscapeToCity->getX_INLINE(), pEscapeToCity->getY_INLINE(), false, false, false);
		}
		szFormatReveal = "TXT_KEY_SPY_ESCAPED_REVEAL";
		szFormatNoReveal = "TXT_KEY_SPY_ESCAPED";
		if (bReveal)
			szBuffer = gDLL->getText(szFormatReveal.GetCString(), GET_PLAYER(getOwnerINLINE()).getCivilizationAdjectiveKey(), getNameKey(), kTargetPlayer.getCivilizationAdjectiveKey(), szCityName.GetCString());
		else
			szBuffer = gDLL->getText(szFormatNoReveal.GetCString(), getNameKey(), kTargetPlayer.getCivilizationAdjectiveKey(), szCityName.GetCString());
		gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSED", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), getX_INLINE(), getY_INLINE(), true, true);
		gDLL->getInterfaceIFace()->addHumanMessage(eTargetPlayer, true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSE", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), getX_INLINE(), getY_INLINE(), true, true);

		changeExperience(1);
		testPromotionReady();

		return true;
	}

	if (bDoubleAgent)
		kTargetPlayer.turnSpy(this);
	else
		kill(true);

	return true;
}

int CvUnit::getSpyInterceptPercent(TeamTypes eTargetTeam, bool bMission) const {
	FAssert(isSpy());
	FAssert(getTeam() != eTargetTeam);

	int iSuccess = 0;

	// K-Mod. Scale based on the teams' population.
	{
		const CvTeam& kTeam = GET_TEAM(getTeam());
		const CvTeam& kTargetTeam = GET_TEAM(eTargetTeam);

		int iPopScale = 5 * GC.getWorldInfo(GC.getMapINLINE().getWorldSize()).getTargetNumCities();
		int iTargetPoints = 10 * kTargetTeam.getEspionagePointsEver() / std::max(1, iPopScale + kTargetTeam.getTotalPopulation(false));
		int iOurPoints = 10 * kTeam.getEspionagePointsEver() / std::max(1, iPopScale + kTeam.getTotalPopulation(false));
		iSuccess += GC.getDefineINT("ESPIONAGE_INTERCEPT_SPENDING_MAX") * iTargetPoints / std::max(1, iTargetPoints + iOurPoints);
	}

	// Apply evasion chance from the spy
	iSuccess -= getSpyEvasionChance();

	if (plot()->isEspionageCounterSpy(eTargetTeam)) {
		iSuccess += GC.getDefineINT("ESPIONAGE_INTERCEPT_COUNTERSPY");
		// Add intercept attribute of any enemy spies present to chances
		if (plot()->plotCheck(PUF_isCounterSpy, -1, -1, NO_PLAYER, eTargetTeam)) {
			CvUnit* pCounterUnit = plot()->plotCheck(PUF_isCounterSpy, -1, -1, NO_PLAYER, eTargetTeam);
			if (pCounterUnit->getSpyInterceptChanceExtra()) {
				iSuccess += pCounterUnit->getSpyInterceptChance();
			}
		}
	}

	if (GET_TEAM(eTargetTeam).getCounterespionageModAgainstTeam(getTeam()) > 0) {
		iSuccess += GC.getDefineINT("ESPIONAGE_INTERCEPT_COUNTERESPIONAGE_MISSION");
	}

	// K-Mod. I've added the following condition for the recent mission bonus, to make spies less likely to be caught while exploring during peace time.
	if (bMission || atWar(getTeam(), eTargetTeam) || GET_TEAM(eTargetTeam).getCounterespionageModAgainstTeam(getTeam()) > 0 || plot()->isEspionageCounterSpy(eTargetTeam)) // K-Mod
	{
		iSuccess += GC.getDefineINT("ESPIONAGE_INTERCEPT_RECENT_MISSION");
	}

	return std::min(100, std::max(0, iSuccess));
}

bool CvUnit::isIntruding() const {
	TeamTypes eLocalTeam = plot()->getTeam();

	if (NO_TEAM == eLocalTeam || eLocalTeam == getTeam()) {
		return false;
	}

	if (GET_TEAM(eLocalTeam).isVassal(getTeam()) || GET_TEAM(getTeam()).isVassal(eLocalTeam)) {
		return false;
	}

	return true;
}

bool CvUnit::canGoldenAge(const CvPlot* pPlot, bool bTestVisible) const {
	if (!isGoldenAge()) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (!bTestVisible) {
		if (GET_PLAYER(getOwnerINLINE()).unitsRequiredForGoldenAge() > GET_PLAYER(getOwnerINLINE()).unitsGoldenAgeReady()) {
			return false;
		}
	}

	return true;
}


bool CvUnit::goldenAge() {
	if (!canGoldenAge(plot())) {
		return false;
	}

	GET_PLAYER(getOwnerINLINE()).killGoldenAgeUnits(this);

	GET_PLAYER(getOwnerINLINE()).changeGoldenAgeTurns(GET_PLAYER(getOwnerINLINE()).getGoldenAgeLength());
	GET_PLAYER(getOwnerINLINE()).changeNumUnitGoldenAges(1);

	if (plot()->isActiveVisible(false)) {
		NotifyEntity(MISSION_GOLDEN_AGE);
	}

	kill(true);

	return true;
}


bool CvUnit::canBuild(const CvPlot* pPlot, BuildTypes eBuild, bool bTestVisible) const {
	FAssertMsg(eBuild < GC.getNumBuildInfos(), "Index out of bounds");

	if (eBuild == NO_BUILD) {
		return false;
	}

	if (!m_pUnitInfo->getBuilds(eBuild)) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	const CvBuildInfo& kBuild = GC.getBuildInfo(eBuild);
	if (kBuild.getObsoleteTech() != NO_TECH) {
		if (GET_TEAM(getTeam()).isHasTech((TechTypes)(kBuild.getObsoleteTech()))) {
			return false;
		}
	}

	if (!GET_PLAYER(getOwnerINLINE()).canBuild(pPlot, eBuild, false, bTestVisible)) {
		return false;
	}

	if (!pPlot->isValidDomainForAction(*this)) {
		return false;
	}

	return true;
}

// Returns true if build finished...
bool CvUnit::build(BuildTypes eBuild) {
	FAssertMsg(eBuild < GC.getNumBuildInfos(), "Invalid Build");

	if (!canBuild(plot(), eBuild)) {
		return false;
	}

	// Note: notify entity must come before changeBuildProgress - because once the unit is done building,
	// that function will notify the entity to stop building.
	NotifyEntity((MissionTypes)GC.getBuildInfo(eBuild).getMissionType());

	GET_PLAYER(getOwnerINLINE()).changeGold(-(GET_PLAYER(getOwnerINLINE()).getBuildCost(plot(), eBuild)));

	bool bFinished = plot()->changeBuildProgress(eBuild, workRate(false), this);

	finishMoves(); // needs to be after the work has been processed because movesLeft() can affect workRate()...

	if (bFinished) {
		bool bKill = (GC.getBuildInfo(eBuild).isKill() || getUnitInfo().isSingleBuild());
		if (!bKill) {
			int iCost = GC.getBuildInfo(eBuild).getTime();
			int iSpeedModifier = workRate(true);
			if (iCost > 0) {
				changeExperience100(iCost / std::max(1, (2 * iSpeedModifier) / 100));
			}
		}

		if (bKill) {
			kill(true);
		}
	}

	// Python Event
	CvEventReporter::getInstance().unitBuildImprovement(this, eBuild, bFinished);

	return bFinished;
}


bool CvUnit::canPromote(PromotionTypes ePromotion, int iLeaderUnitId) const {
	if (iLeaderUnitId >= 0) {
		if (iLeaderUnitId == getID()) {
			return false;
		}

		// The command is always possible if it's coming from a Warlord unit that gives just experience points
		CvUnit* pWarlord = GET_PLAYER(getOwnerINLINE()).getUnit(iLeaderUnitId);
		if (pWarlord &&
			NO_UNIT != pWarlord->getUnitType() &&
			pWarlord->getUnitInfo().getLeaderExperience() > 0 &&
			NO_PROMOTION == pWarlord->getUnitInfo().getLeaderPromotion() &&
			canAcquirePromotionAny()) {
			return true;
		}
	}

	if (ePromotion == NO_PROMOTION) {
		return false;
	}

	if (!canAcquirePromotion(ePromotion)) {
		return false;
	}

	const CvPromotionInfo& kPromotion = GC.getPromotionInfo(ePromotion);
	if (kPromotion.isLeader()) {
		if (iLeaderUnitId >= 0) {
			CvUnit* pWarlord = GET_PLAYER(getOwnerINLINE()).getUnit(iLeaderUnitId);
			if (pWarlord && NO_UNIT != pWarlord->getUnitType()) {
				return (pWarlord->getUnitInfo().getLeaderPromotion() == ePromotion);
			}
		}
		return false;
	} else {
		if (kPromotion.getPromotionGroup() == 0 && !isPromotionReady()) {
			return false;
		} else if (kPromotion.getPromotionGroup() > 0 && !isHasPromotionGroup(ePromotion) && !isPromotionReady()) {
			return false;
		} else if (kPromotion.getPromotionGroup() > 0 && isHasPromotionGroup(ePromotion) && isHasGroupPromotionChanged()) {
			return false;
		}
	}

	return true;
}

void CvUnit::promote(PromotionTypes ePromotion, int iLeaderUnitId) {
	if (!canPromote(ePromotion, iLeaderUnitId)) {
		return;
	}

	if (iLeaderUnitId >= 0) {
		CvUnit* pWarlord = GET_PLAYER(getOwnerINLINE()).getUnit(iLeaderUnitId);
		if (pWarlord) {
			pWarlord->giveExperience();
			if (!pWarlord->getNameNoDesc().empty()) {
				setName(pWarlord->getNameKey());
			}

			//update graphics models
			m_eLeaderUnitType = pWarlord->getUnitType();
			reloadEntity();
		}
	}

	if (!GC.getPromotionInfo(ePromotion).isLeader() && !isHasPromotionGroup(ePromotion)) {
		changeLevel(1);
		changeDamage(-(getDamage() / 2));
	}

	setHasPromotion(ePromotion, true);

	testPromotionReady();

	CvSelectionGroup::path_finder.Reset(); // K-Mod. (This currently isn't important, because the AI doesn't use promotions mid-turn anyway.)

	if (IsSelected()) {
		gDLL->getInterfaceIFace()->playGeneralSound(GC.getPromotionInfo(ePromotion).getSound());

		gDLL->getInterfaceIFace()->setDirty(UnitInfo_DIRTY_BIT, true);
		gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
		gDLL->getFAStarIFace()->ForceReset(&GC.getInterfacePathFinder()); // K-Mod.
	} else {
		setInfoBarDirty(true);
	}

	CvEventReporter::getInstance().unitPromoted(this, ePromotion);
}

bool CvUnit::lead(int iUnitId) {
	if (!canLead(plot(), iUnitId)) {
		return false;
	}

	PromotionTypes eLeaderPromotion = (PromotionTypes)m_pUnitInfo->getLeaderPromotion();

	if (-1 == iUnitId) {
		CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_LEADUNIT, eLeaderPromotion, getID());
		if (pInfo) {
			gDLL->getInterfaceIFace()->addPopup(pInfo, getOwnerINLINE(), true);
		}
		return false;
	} else {
		CvUnit* pUnit = GET_PLAYER(getOwnerINLINE()).getUnit(iUnitId);

		if (!pUnit || !pUnit->canPromote(eLeaderPromotion, getID()) || isDoubleAgent()) {
			return false;
		}

		pUnit->joinGroup(NULL, true, true);

		pUnit->promote(eLeaderPromotion, getID());

		if (plot()->isActiveVisible(false)) {
			NotifyEntity(MISSION_LEAD);
		}

		kill(true);

		return true;
	}
}


int CvUnit::canLead(const CvPlot* pPlot, int iUnitId) const {
	PROFILE_FUNC();

	if (isDelayedDeath()) {
		return 0;
	}

	if (NO_UNIT == getUnitType()) {
		return 0;
	}

	if (!isEnabled()) {
		return false;
	}

	int iNumUnits = 0;
	CvUnitInfo& kUnitInfo = getUnitInfo();

	if (-1 == iUnitId) {
		CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();
		while (pUnitNode != NULL) {
			CvUnit* pUnit = ::getUnit(pUnitNode->m_data);
			pUnitNode = pPlot->nextUnitNode(pUnitNode);

			if (pUnit && pUnit != this && pUnit->getOwnerINLINE() == getOwnerINLINE() && pUnit->canPromote((PromotionTypes)kUnitInfo.getLeaderPromotion(), getID())) {
				++iNumUnits;
			}
		}
	} else {
		CvUnit* pUnit = GET_PLAYER(getOwnerINLINE()).getUnit(iUnitId);
		if (pUnit && pUnit != this && pUnit->canPromote((PromotionTypes)kUnitInfo.getLeaderPromotion(), getID())) {
			iNumUnits = 1;
		}
	}
	return iNumUnits;
}


int CvUnit::canGiveExperience(const CvPlot* pPlot) const {
	int iNumUnits = 0;

	if (NO_UNIT != getUnitType() && m_pUnitInfo->getLeaderExperience() > 0) {
		CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();
		while (pUnitNode != NULL) {
			CvUnit* pUnit = ::getUnit(pUnitNode->m_data);
			pUnitNode = pPlot->nextUnitNode(pUnitNode);

			if (pUnit && pUnit != this && pUnit->getOwnerINLINE() == getOwnerINLINE() && pUnit->canAcquirePromotionAny()) {
				++iNumUnits;
			}
		}
	}

	return iNumUnits;
}

bool CvUnit::giveExperience() {
	CvPlot* pPlot = plot();

	if (pPlot) {
		int iNumUnits = canGiveExperience(pPlot);
		if (iNumUnits > 0) {
			int iTotalExperience = getStackExperienceToGive(iNumUnits);

			int iMinExperiencePerUnit = iTotalExperience / iNumUnits;
			int iRemainder = iTotalExperience % iNumUnits;

			CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();
			int i = 0;
			while (pUnitNode != NULL) {
				CvUnit* pUnit = ::getUnit(pUnitNode->m_data);
				pUnitNode = pPlot->nextUnitNode(pUnitNode);

				if (pUnit && pUnit != this && pUnit->getOwnerINLINE() == getOwnerINLINE() && pUnit->canAcquirePromotionAny()) {
					pUnit->changeExperience(i < iRemainder ? iMinExperiencePerUnit + 1 : iMinExperiencePerUnit);
					pUnit->testPromotionReady();
				}

				i++;
			}

			return true;
		}
	}

	return false;
}

int CvUnit::getStackExperienceToGive(int iNumUnits) const {
	// K-Mod, +50% is too low as a maximum.
	return (m_pUnitInfo->getLeaderExperience() * (100 + std::min(GC.getDefineINT("WARLORD_MAXIMUM_EXTRA_EXPERIENCE_PERCENT"), (iNumUnits - 1) * GC.getDefineINT("WARLORD_EXTRA_EXPERIENCE_PER_UNIT_PERCENT")))) / 100;
}

int CvUnit::upgradePrice(UnitTypes eUnit) const {
	CyArgsList argsList;
	argsList.add(getOwnerINLINE());
	argsList.add(getID());
	argsList.add((int)eUnit);
	long lResult = 0;

	if (GC.getUSE_UNIT_UPGRADE_PRICE_CALLBACK()) // K-Mod. block unused python callbacks
	{
		gDLL->getPythonIFace()->callFunction(PYGameModule, "getUpgradePriceOverride", argsList.makeFunctionArgs(), &lResult);
		if (lResult >= 0) {
			return lResult;
		}
	}

	if (isBarbarian()) {
		return 0;
	}

	int iPrice = GC.getDefineINT("BASE_UNIT_UPGRADE_COST");

	iPrice += (std::max(0, (GET_PLAYER(getOwnerINLINE()).getProductionNeeded(eUnit) - GET_PLAYER(getOwnerINLINE()).getProductionNeeded(getUnitType()))) * GC.getDefineINT("UNIT_UPGRADE_COST_PER_PRODUCTION"));

	if (!isHuman() && !isBarbarian()) {
		iPrice *= GC.getHandicapInfo(GC.getGameINLINE().getHandicapType()).getAIUnitUpgradePercent();
		iPrice /= 100;

		iPrice *= std::max(0, ((GC.getHandicapInfo(GC.getGameINLINE().getHandicapType()).getAIPerEraModifier() * GET_PLAYER(getOwnerINLINE()).getCurrentEra()) + 100));
		iPrice /= 100;
	}

	iPrice -= (iPrice * getUpgradeDiscount()) / 100;

	return iPrice;
}


bool CvUnit::upgradeAvailable(UnitTypes eFromUnit, UnitClassTypes eToUnitClass, int iCount) const {
	int numUnitClassInfos = GC.getNumUnitClassInfos();

	if (iCount > numUnitClassInfos) {
		return false;
	}

	CvUnitInfo& fromUnitInfo = GC.getUnitInfo(eFromUnit);

	if (fromUnitInfo.getUpgradeUnitClass(eToUnitClass)) {
		return true;
	}

	for (int iI = 0; iI < numUnitClassInfos; iI++) {
		if (fromUnitInfo.getUpgradeUnitClass(iI)) {
			UnitTypes eLoopUnit = ((UnitTypes)(GC.getCivilizationInfo(getCivilizationType()).getCivilizationUnits(iI)));

			if (eLoopUnit != NO_UNIT) {
				if (upgradeAvailable(eLoopUnit, eToUnitClass, (iCount + 1))) {
					return true;
				}
			}
		}
	}

	return false;
}


bool CvUnit::canUpgrade(UnitTypes eUnit, bool bTestVisible) const {
	if (eUnit == NO_UNIT) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (!isReadyForUpgrade()) {
		return false;
	}

	if (!bTestVisible) {
		if (GET_PLAYER(getOwnerINLINE()).getGold() < upgradePrice(eUnit)) {
			return false;
		}
	}

	if (hasUpgrade(eUnit)) {
		return true;
	}

	return false;
}

bool CvUnit::isReadyForUpgrade() const {
	if (!canMove()) {
		return false;
	}

	if (plot()->getTeam() != getTeam()) {
		return false;
	}

	return true;
}

// has upgrade is used to determine if an upgrade is possible,
// it specifically does not check whether the unit can move, whether the current plot is owned, enough gold
// those are checked in canUpgrade()
// does not search all cities, only checks the closest one
bool CvUnit::hasUpgrade(bool bSearch) const {
	return (getUpgradeCity(bSearch) != NULL);
}

// has upgrade is used to determine if an upgrade is possible,
// it specifically does not check whether the unit can move, whether the current plot is owned, enough gold
// those are checked in canUpgrade()
// does not search all cities, only checks the closest one
bool CvUnit::hasUpgrade(UnitTypes eUnit, bool bSearch) const {
	return (getUpgradeCity(eUnit, bSearch) != NULL);
}

// finds the 'best' city which has a valid upgrade for the unit,
// it specifically does not check whether the unit can move, or if the player has enough gold to upgrade
// those are checked in canUpgrade()
// if bSearch is true, it will check every city, if not, it will only check the closest valid city
// NULL result means the upgrade is not possible
CvCity* CvUnit::getUpgradeCity(bool bSearch) const {
	CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());
	UnitAITypes eUnitAI = AI_getUnitAIType();
	CvArea* pArea = area();

	int iCurrentValue = kPlayer.AI_unitValue(this, eUnitAI, pArea);

	int iBestSearchValue = MAX_INT;
	CvCity* pBestUpgradeCity = NULL;

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++) {
		int iNewValue = kPlayer.AI_unitValue(((UnitTypes)iI), eUnitAI, pArea);
		if (iNewValue > iCurrentValue) {
			int iSearchValue;
			CvCity* pUpgradeCity = getUpgradeCity((UnitTypes)iI, bSearch, &iSearchValue);
			if (pUpgradeCity != NULL) {
				// if not searching or close enough, then this match will do
				if (!bSearch || iSearchValue < 16) {
					return pUpgradeCity;
				}

				if (iSearchValue < iBestSearchValue) {
					iBestSearchValue = iSearchValue;
					pBestUpgradeCity = pUpgradeCity;
				}
			}
		}
	}

	return pBestUpgradeCity;
}

// finds the 'best' city which has a valid upgrade for the unit, to eUnit type
// it specifically does not check whether the unit can move, or if the player has enough gold to upgrade
// those are checked in canUpgrade()
// if bSearch is true, it will check every city, if not, it will only check the closest valid city
// if iSearchValue non NULL, then on return it will be the city's proximity value, lower is better
// NULL result means the upgrade is not possible
CvCity* CvUnit::getUpgradeCity(UnitTypes eUnit, bool bSearch, int* iSearchValue) const {
	if (eUnit == NO_UNIT) {
		return NULL;
	}

	CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());
	CvUnitInfo& kUnitInfo = GC.getUnitInfo(eUnit);

	if (GC.getCivilizationInfo(kPlayer.getCivilizationType()).getCivilizationUnits(kUnitInfo.getUnitClassType()) != eUnit) {
		return NULL;
	}

	if (!upgradeAvailable(getUnitType(), ((UnitClassTypes)(kUnitInfo.getUnitClassType())))) {
		return NULL;
	}

	if (kUnitInfo.getCargoSpace() < getCargo()) {
		return NULL;
	}

	if (getCargo() > 0) // K-Mod. (no point looping through everything if there is no cargo anyway.)
	{
		CLLNode<IDInfo>* pUnitNode = plot()->headUnitNode();
		while (pUnitNode != NULL) {
			CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
			pUnitNode = plot()->nextUnitNode(pUnitNode);

			if (pLoopUnit->getTransportUnit() == this) {
				if (kUnitInfo.getSpecialCargo() != NO_SPECIALUNIT) {
					if (kUnitInfo.getSpecialCargo() != pLoopUnit->getSpecialUnitType()) {
						return NULL;
					}
				}

				if (kUnitInfo.getDomainCargo() != NO_DOMAIN) {
					if (kUnitInfo.getDomainCargo() != pLoopUnit->getDomainType()) {
						return NULL;
					}
				}
			}
		}
	}

	// sea units must be built on the coast
	bool bCoastalOnly = (getDomainType() == DOMAIN_SEA);

	// results
	int iBestValue = MAX_INT;
	CvCity* pBestCity = NULL;

	// if search is true, check every city for our team
	if (bSearch) {
		// air units can travel any distance
		bool bIgnoreDistance = (getDomainType() == DOMAIN_AIR);

		TeamTypes eTeam = getTeam();
		int iArea = getArea();
		int iX = getX_INLINE(), iY = getY_INLINE();

		// check every player on our team's cities
		for (int iI = 0; iI < MAX_PLAYERS; iI++) {
			// is this player on our team?
			CvPlayerAI& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
			if (kLoopPlayer.isAlive() && kLoopPlayer.getTeam() == eTeam) {
				int iLoop;
				for (CvCity* pLoopCity = kLoopPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kLoopPlayer.nextCity(&iLoop)) {
					// if coastal only, then make sure we are coast
					CvArea* pWaterArea = NULL;
					if (!bCoastalOnly || ((pWaterArea = pLoopCity->waterArea()) != NULL && !pWaterArea->isLake())) {
						// can this city train this unit?
						if (pLoopCity->canTrain(eUnit, false, false, true)) {
							// if we do not care about distance, then the first match will do
							if (bIgnoreDistance) {
								// if we do not care about distance, then return 1 for value
								if (iSearchValue != NULL) {
									*iSearchValue = 1;
								}

								return pLoopCity;
							}

							int iValue = plotDistance(iX, iY, pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE());

							// if not same area, not as good (lower numbers are better)
							if (iArea != pLoopCity->getArea() && (!bCoastalOnly || iArea != pWaterArea->getID())) {
								iValue *= 16;
							}

							// if we cannot path there, not as good (lower numbers are better)
							if (!generatePath(pLoopCity->plot(), 0, true)) {
								iValue *= 16;
							}

							if (iValue < iBestValue) {
								iBestValue = iValue;
								pBestCity = pLoopCity;
							}
						}
					}
				}
			}
		}
	} else {
		// find the closest city
		CvCity* pClosestCity = GC.getMapINLINE().findCity(getX_INLINE(), getY_INLINE(), NO_PLAYER, getTeam(), true, bCoastalOnly);
		if (pClosestCity != NULL) {
			// if we can train, then return this city (otherwise it will return NULL)
			if (pClosestCity->canTrain(eUnit, false, false, true)) {
				// did not search, always return 1 for search value
				iBestValue = 1;

				pBestCity = pClosestCity;
			}
		}
	}

	// return the best value, if non-NULL
	if (iSearchValue != NULL) {
		*iSearchValue = iBestValue;
	}

	return pBestCity;
}

// Upgrade ourselves into the UnitType if we can
CvUnit* CvUnit::upgrade(UnitTypes eUnit) // K-Mod: this now returns the new unit.
{
	CvUnit* pUpgradeUnit;

	if (!canUpgrade(eUnit)) {
		return this;
	}

	GET_PLAYER(getOwnerINLINE()).changeGold(-(upgradePrice(eUnit)));

	pUpgradeUnit = GET_PLAYER(getOwnerINLINE()).initUnit(eUnit, getX_INLINE(), getY_INLINE(), AI_getUnitAIType());

	FAssertMsg(pUpgradeUnit != NULL, "UpgradeUnit is not assigned a valid value");

	pUpgradeUnit->convert(this);
	pUpgradeUnit->joinGroup(getGroup()); // K-Mod, swapped order with convert. (otherwise units on boats would be ungrouped.)

	pUpgradeUnit->finishMoves();

	if (pUpgradeUnit->getLeaderUnitType() == NO_UNIT) {
		if (pUpgradeUnit->getExperience() > GC.getDefineINT("MAX_EXPERIENCE_AFTER_UPGRADE")) {
			pUpgradeUnit->setExperience(GC.getDefineINT("MAX_EXPERIENCE_AFTER_UPGRADE"));
		}
	}

	if (gUnitLogLevel > 2) {
		CvWString szString;
		getUnitAIString(szString, AI_getUnitAIType());
		logBBAI("    %S spends %d to upgrade %S to %S, unit AI %S", GET_PLAYER(getOwnerINLINE()).getCivilizationDescription(0), upgradePrice(eUnit), getName(0).GetCString(), pUpgradeUnit->getName(0).GetCString(), szString.GetCString());
	}

	return pUpgradeUnit; // K-Mod
}


HandicapTypes CvUnit::getHandicapType() const {
	return GET_PLAYER(getOwnerINLINE()).getHandicapType();
}


CivilizationTypes CvUnit::getCivilizationType() const {
	return GET_PLAYER(getOwnerINLINE()).getCivilizationType();
}

const wchar* CvUnit::getVisualCivAdjective(TeamTypes eForTeam) const {
	if (getVisualOwner(eForTeam) == getOwnerINLINE()) {
		return GC.getCivilizationInfo(getCivilizationType()).getAdjectiveKey();
	}

	return L"";
}

SpecialUnitTypes CvUnit::getSpecialUnitType() const {
	return ((SpecialUnitTypes)(m_pUnitInfo->getSpecialUnitType()));
}


UnitTypes CvUnit::getCaptureUnitType(CivilizationTypes eCivilization) const {
	FAssert(eCivilization != NO_CIVILIZATION);
	return ((m_pUnitInfo->getUnitCaptureClassType() == NO_UNITCLASS) ? NO_UNIT : (UnitTypes)GC.getCivilizationInfo(eCivilization).getCivilizationUnits(m_pUnitInfo->getUnitCaptureClassType()));
}

void CvUnit::setUnitCombatType(UnitCombatTypes eUnitCombat) {
	m_eUnitCombatType = eUnitCombat;
}

UnitCombatTypes CvUnit::getUnitCombatType() const {
	return m_eUnitCombatType;
}

bool CvUnit::isUnitCombatType(UnitCombatTypes eUnitCombat) const {

	if (getUnitCombatType() == eUnitCombat)
		return true;

	// In some case the units combat class does not match its unitinfo class and in
	// those cases we don't want to to check the unitinfo
	if (getUnitCombatType() == m_pUnitInfo->getUnitCombatType())
		return m_pUnitInfo->isSubCombatType(eUnitCombat);

	return false;
}

DomainTypes CvUnit::getDomainType() const {
	return ((DomainTypes)(m_pUnitInfo->getDomainType()));
}


InvisibleTypes CvUnit::getInvisibleType() const {
	return m_eInvisible;
}

int CvUnit::getNumSeeInvisibleTypes() const {
	int iNumSeeInvisibles = 0;
	for (InvisibleTypes eInvisible = (InvisibleTypes)0; eInvisible < GC.getNumInvisibleInfos(); eInvisible = (InvisibleTypes)(eInvisible + 1)) {
		if (isSeeInvisible(eInvisible)) {
			iNumSeeInvisibles++;
		}
	}
	return iNumSeeInvisibles;
}

InvisibleTypes CvUnit::getSeeInvisibleType(int i) const {
	for (InvisibleTypes eLoopInvisible = (InvisibleTypes)0; eLoopInvisible < GC.getNumInvisibleInfos(); eLoopInvisible = (InvisibleTypes)(eLoopInvisible + 1)) {
		if (isSeeInvisible(eLoopInvisible)) {
			return eLoopInvisible;
		}
	}
	return NO_INVISIBLE;
}


int CvUnit::flavorValue(FlavorTypes eFlavor) const {
	return m_pUnitInfo->getFlavorValue(eFlavor);
}


bool CvUnit::isBarbarian() const {
	return GET_PLAYER(getOwnerINLINE()).isBarbarian();
}


bool CvUnit::isHuman() const {
	return GET_PLAYER(getOwnerINLINE()).isHuman();
}


int CvUnit::visibilityRange() const {
	return (GC.getDefineINT("UNIT_VISIBILITY_RANGE") + getExtraVisibilityRange());
}


int CvUnit::baseMoves() const {
	return (m_pUnitInfo->getMoves() + getExtraMoves() + GET_TEAM(getTeam()).getExtraMoves(getDomainType()));
}


int CvUnit::maxMoves() const {
	return (baseMoves() * GC.getMOVE_DENOMINATOR());
}


int CvUnit::movesLeft() const {
	return std::max(0, (maxMoves() - getMoves()));
}


bool CvUnit::canMove() const {
	if (isDead()) {
		return false;
	}

	if (getMoves() >= maxMoves()) {
		return false;
	}

	if (isImmobile()) {
		return false;
	}

	return true;
}


bool CvUnit::hasMoved()	const {
	return (getMoves() > 0);
}


int CvUnit::airRange() const {
	return (m_pUnitInfo->getAirRange() + getExtraAirRange());
}


int CvUnit::nukeRange() const {
	return m_pUnitInfo->getNukeRange();
}


// XXX should this test for coal?
bool CvUnit::canBuildRoute() const {
	int iI;

	for (iI = 0; iI < GC.getNumBuildInfos(); iI++) {
		if (GC.getBuildInfo((BuildTypes)iI).getRoute() != NO_ROUTE) {
			if (m_pUnitInfo->getBuilds(iI)) {
				if (GET_TEAM(getTeam()).isHasTech((TechTypes)(GC.getBuildInfo((BuildTypes)iI).getTechPrereq()))) {
					return true;
				}
			}
		}
	}

	return false;
}

BuildTypes CvUnit::getBuildType() const {
	if (getGroup()->headMissionQueueNode() != NULL) {
		switch (getGroup()->headMissionQueueNode()->m_data.eMissionType) {
		case MISSION_MOVE_TO:
		case MISSION_MOVE_TO_SENTRY:
			break;

		case MISSION_ROUTE_TO:
		{
			BuildTypes eBuild;
			if (getGroup()->getBestBuildRoute(plot(), &eBuild) != NO_ROUTE) {
				return eBuild;
			}
		}
		break;

		case MISSION_MOVE_TO_UNIT:
		case MISSION_SKIP:
		case MISSION_SLEEP:
		case MISSION_FORTIFY:
		case MISSION_PLUNDER:
		case MISSION_AIRPATROL:
		case MISSION_SEAPATROL:
		case MISSION_HEAL:
		case MISSION_SENTRY:
		case MISSION_SENTRY_WHILE_HEAL:
		case MISSION_SENTRY_NAVAL_UNITS:
		case MISSION_SENTRY_LAND_UNITS:
		case MISSION_AIRLIFT:
		case MISSION_NUKE:
		case MISSION_RECON:
		case MISSION_PARADROP:
		case MISSION_AIRBOMB:
		case MISSION_BOMBARD:
		case MISSION_RANGE_ATTACK:
		case MISSION_PILLAGE:
		case MISSION_SABOTAGE:
		case MISSION_DESTROY:
		case MISSION_STEAL_PLANS:
		case MISSION_FOUND:
		case MISSION_SPREAD:
		case MISSION_SPREAD_CORPORATION:
		case MISSION_JOIN:
		case MISSION_CONSTRUCT:
		case MISSION_DISCOVER:
		case MISSION_HURRY:
		case MISSION_TRADE:
		case MISSION_GREAT_WORK:
		case MISSION_INFILTRATE:
		case MISSION_GOLDEN_AGE:
		case MISSION_LEAD:
		case MISSION_ESPIONAGE:
		case MISSION_DIE_ANIMATION:
		case MISSION_UPDATE_WORLD_VIEWS:
		case MISSION_SELL_SLAVE:
		case MISSION_SHADOW:
		case MISSION_WAIT_FOR_TECH:
		case MISSION_BECOME_SLAVER:
		case MISSION_BECOME_BARBARIAN:
			break;

		case MISSION_BUILD:
			return (BuildTypes)getGroup()->headMissionQueueNode()->m_data.iData1;
			break;

		default:
			FAssert(false);
			break;
		}
	}

	return NO_BUILD;
}


int CvUnit::workRate(bool bMax) const {
	if (!bMax) {
		if (!canMove()) {
			return 0;
		}
	}

	int iRate = m_pUnitInfo->getWorkRate() + getWorkRateModifier();

	iRate *= std::max(0, (GET_PLAYER(getOwnerINLINE()).getWorkerSpeedModifier() + 100));
	iRate /= 100;

	if (!isHuman() && !isBarbarian()) {
		iRate *= std::max(0, (GC.getHandicapInfo(GC.getGameINLINE().getHandicapType()).getAIWorkRateModifier() + 100));
		iRate /= 100;
	}

	return iRate;
}


bool CvUnit::isAnimal() const {
	return m_pUnitInfo->isAnimal();
}


bool CvUnit::isNoBadGoodies() const {
	return m_pUnitInfo->isNoBadGoodies();
}


bool CvUnit::isOnlyDefensive() const {
	return m_pUnitInfo->isOnlyDefensive();
}


bool CvUnit::isNoCapture() const {
	return m_pUnitInfo->isNoCapture();
}


bool CvUnit::isRivalTerritory() const {
	return m_pUnitInfo->isRivalTerritory();
}


bool CvUnit::isMilitaryHappiness() const {
	return m_pUnitInfo->isMilitaryHappiness();
}


bool CvUnit::isInvestigate() const {
	return m_pUnitInfo->isInvestigate();
}


bool CvUnit::isCounterSpy() const {
	return m_pUnitInfo->isCounterSpy() || getSpyInterceptChance() > 0;
}


bool CvUnit::isSpy() const {
	return m_pUnitInfo->isSpy();
}


bool CvUnit::isFound() const {
	return m_pUnitInfo->isFound();
}


bool CvUnit::isGoldenAge() const {
	if (isDelayedDeath()) {
		return false;
	}

	return m_pUnitInfo->isGoldenAge();
}

bool CvUnit::canCoexistWithEnemyUnit(TeamTypes eTeam) const {
	if (NO_TEAM == eTeam) {
		if (alwaysInvisible()) {
			return true;
		}

		return false;
	}

	if (isInvisible(eTeam, false)) {
		return true;
	}

	return false;
}

bool CvUnit::isFighting() const {
	return (getCombatUnit() != NULL);
}


bool CvUnit::isAttacking() const {
	return (getAttackPlot() != NULL && !isDelayedDeath());
}


bool CvUnit::isDefending() const {
	return (isFighting() && !isAttacking());
}


bool CvUnit::isCombat() const {
	return (isFighting() || isAttacking());
}


int CvUnit::maxHitPoints() const {
	return GC.getMAX_HIT_POINTS();
}


int CvUnit::currHitPoints()	const {
	return (maxHitPoints() - getDamage());
}


bool CvUnit::isHurt() const {
	return (getDamage() > 0);
}


bool CvUnit::isDead() const {
	return (getDamage() >= maxHitPoints());
}


void CvUnit::setBaseCombatStr(int iCombat) {
	m_iBaseCombat = iCombat;
}

int CvUnit::baseCombatStr() const {
	return m_iBaseCombat + getWeaponStrength() + getAmmunitionStrength();
}


// maxCombatStr can be called in four different configurations
//		pPlot == NULL, pAttacker == NULL for combat when this is the attacker
//		pPlot valid, pAttacker valid for combat when this is the defender
//		pPlot valid, pAttacker == NULL (new case), when this is the defender, attacker unknown
//		pPlot valid, pAttacker == this (new case), when the defender is unknown, but we want to calc approx str
//			note, in this last case, it is expected pCombatDetails == NULL, it does not have to be, but some 
//			values may be unexpectedly reversed in this case (iModifierTotal will be the negative sum)
int CvUnit::maxCombatStr(const CvPlot* pPlot, const CvUnit* pAttacker, CombatDetails* pCombatDetails) const {
	int iCombat;

	FAssertMsg((pPlot == NULL) || (pPlot->getTerrainType() != NO_TERRAIN), "(pPlot == NULL) || (pPlot->getTerrainType() is not expected to be equal with NO_TERRAIN)");

	// handle our new special case
	const	CvPlot* pAttackedPlot = NULL;
	bool	bAttackingUnknownDefender = false;
	if (pAttacker == this) {
		bAttackingUnknownDefender = true;
		pAttackedPlot = pPlot;

		// reset these values, we will fiddle with them below
		pPlot = NULL;
		pAttacker = NULL;
	}
	// otherwise, attack plot is the plot of us (the defender)
	else if (pAttacker != NULL) {
		pAttackedPlot = plot();
	}

	if (pCombatDetails != NULL) {
		pCombatDetails->iExtraCombatPercent = 0;
		pCombatDetails->iAnimalCombatModifierTA = 0;
		pCombatDetails->iAIAnimalCombatModifierTA = 0;
		pCombatDetails->iAnimalCombatModifierAA = 0;
		pCombatDetails->iAIAnimalCombatModifierAA = 0;
		pCombatDetails->iBarbarianCombatModifierTB = 0;
		pCombatDetails->iAIBarbarianCombatModifierTB = 0;
		pCombatDetails->iBarbarianCombatModifierAB = 0;
		pCombatDetails->iAIBarbarianCombatModifierAB = 0;
		pCombatDetails->iPlotDefenseModifier = 0;
		pCombatDetails->iFortifyModifier = 0;
		pCombatDetails->iCityDefenseModifier = 0;
		pCombatDetails->iHillsAttackModifier = 0;
		pCombatDetails->iHillsDefenseModifier = 0;
		pCombatDetails->iFeatureAttackModifier = 0;
		pCombatDetails->iFeatureDefenseModifier = 0;
		pCombatDetails->iTerrainAttackModifier = 0;
		pCombatDetails->iTerrainDefenseModifier = 0;
		pCombatDetails->iCityAttackModifier = 0;
		pCombatDetails->iDomainDefenseModifier = 0;
		pCombatDetails->iCityBarbarianDefenseModifier = 0;
		pCombatDetails->iClassDefenseModifier = 0;
		pCombatDetails->iClassAttackModifier = 0;
		pCombatDetails->iCombatModifierA = 0;
		pCombatDetails->iCombatModifierT = 0;
		pCombatDetails->iDomainModifierA = 0;
		pCombatDetails->iDomainModifierT = 0;
		pCombatDetails->iAnimalCombatModifierA = 0;
		pCombatDetails->iAnimalCombatModifierT = 0;
		pCombatDetails->iRiverAttackModifier = 0;
		pCombatDetails->iAmphibAttackModifier = 0;
		pCombatDetails->iKamikazeModifier = 0;
		pCombatDetails->iModifierTotal = 0;
		pCombatDetails->iBaseCombatStr = 0;
		pCombatDetails->iCombat = 0;
		pCombatDetails->iMaxCombatStr = 0;
		pCombatDetails->iCurrHitPoints = 0;
		pCombatDetails->iMaxHitPoints = 0;
		pCombatDetails->iCurrCombatStr = 0;
		pCombatDetails->eOwner = getOwnerINLINE();
		pCombatDetails->eVisualOwner = getVisualOwner();
		pCombatDetails->sUnitName = getName().GetCString();
	}

	if (baseCombatStr() == 0) {
		return 0;
	}

	int iModifier = 0;
	int iExtraModifier;

	iExtraModifier = getExtraCombatPercent();
	iModifier += iExtraModifier;
	if (pCombatDetails != NULL) {
		pCombatDetails->iExtraCombatPercent = iExtraModifier;
	}

	// do modifiers for animals and barbarians (leaving these out for bAttackingUnknownDefender case)
	if (pAttacker != NULL) {
		if (isAnimal()) {
			if (pAttacker->isHuman()) {
				// K-Mod. Give bonus based on player's difficulty, not game difficulty.
				iExtraModifier = GC.getHandicapInfo(GET_PLAYER(pAttacker->getOwnerINLINE()).getHandicapType()).getAnimalCombatModifier(); // K-Mod
				iModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iAnimalCombatModifierTA = iExtraModifier;
				}
			} else {
				iExtraModifier = GC.getHandicapInfo(GC.getGameINLINE().getHandicapType()).getAIAnimalCombatModifier();
				iModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iAIAnimalCombatModifierTA = iExtraModifier;
				}
			}
		}

		if (pAttacker->isAnimal()) {
			if (isHuman()) {
				iExtraModifier = -GC.getHandicapInfo(GET_PLAYER(getOwnerINLINE()).getHandicapType()).getAnimalCombatModifier(); // K-Mod
				iModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iAnimalCombatModifierAA = iExtraModifier;
				}
			} else {
				iExtraModifier = -GC.getHandicapInfo(GC.getGameINLINE().getHandicapType()).getAIAnimalCombatModifier();
				iModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iAIAnimalCombatModifierAA = iExtraModifier;
				}
			}
		}

		if (isBarbarian()) {
			if (pAttacker->isHuman()) {
				iExtraModifier = GC.getHandicapInfo(GET_PLAYER(pAttacker->getOwnerINLINE()).getHandicapType()).getBarbarianCombatModifier(); // K-Mod
				iModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iBarbarianCombatModifierTB = iExtraModifier;
				}
			} else {
				iExtraModifier = GC.getHandicapInfo(GC.getGameINLINE().getHandicapType()).getAIBarbarianCombatModifier();
				iModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iAIBarbarianCombatModifierTB = iExtraModifier;
				}
			}
		}

		if (pAttacker->isBarbarian()) {
			if (isHuman()) {
				iExtraModifier = -GC.getHandicapInfo(GET_PLAYER(getOwnerINLINE()).getHandicapType()).getBarbarianCombatModifier(); // K-Mod
				iModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iBarbarianCombatModifierAB = iExtraModifier;
				}
			} else {
				iExtraModifier = -GC.getHandicapInfo(GC.getGameINLINE().getHandicapType()).getAIBarbarianCombatModifier();
				iModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iAIBarbarianCombatModifierTB = iExtraModifier;
				}
			}
		}
	}

	// add defensive bonuses (leaving these out for bAttackingUnknownDefender case)
	if (pPlot != NULL) {
		if (!noDefensiveBonus()) {
			// When pAttacker is NULL but pPlot is not, this is a computation for this units defensive value
			// against an unknown attacker.  Always ignoring building defense in this case is a conservative estimate,
			// but causes AI to suicide against castle walls of low culture cities in early game.  Using this units
			// ignoreBuildingDefense does a little better ... in early game it corrects undervalue of castles.  One
			// downside is when medieval unit is defending a walled city against gunpowder.  Here, the over value
			// makes attacker a little more cautious, but with their tech lead it shouldn't matter too much.  Also
			// makes vulnerable units (ships, etc) feel safer in this case and potentially not leave, but ships
			// leave when ratio is pretty low anyway.
			iExtraModifier = pPlot->defenseModifier(getTeam(), (pAttacker != NULL) ? pAttacker->ignoreBuildingDefense() : ignoreBuildingDefense());
			iModifier += iExtraModifier;
			if (pCombatDetails != NULL) {
				pCombatDetails->iPlotDefenseModifier = iExtraModifier;
			}
		}

		iExtraModifier = fortifyModifier();
		iModifier += iExtraModifier;
		if (pCombatDetails != NULL) {
			pCombatDetails->iFortifyModifier = iExtraModifier;
		}

		if (pPlot->isCity(true, getTeam())) {
			iExtraModifier = cityDefenseModifier();
			iModifier += iExtraModifier;
			if (pCombatDetails != NULL) {
				pCombatDetails->iCityDefenseModifier = iExtraModifier;
			}
		}

		if (pPlot->isHills()) {
			iExtraModifier = hillsDefenseModifier();
			iModifier += iExtraModifier;
			if (pCombatDetails != NULL) {
				pCombatDetails->iHillsDefenseModifier = iExtraModifier;
			}
		}

		if (pPlot->getFeatureType() != NO_FEATURE) {
			iExtraModifier = featureDefenseModifier(pPlot->getFeatureType());
			iModifier += iExtraModifier;
			if (pCombatDetails != NULL) {
				pCombatDetails->iFeatureDefenseModifier = iExtraModifier;
			}
		} else {
			iExtraModifier = terrainDefenseModifier(pPlot->getTerrainType());
			iModifier += iExtraModifier;
			if (pCombatDetails != NULL) {
				pCombatDetails->iTerrainDefenseModifier = iExtraModifier;
			}
		}
	}

	// if we are attacking to an plot with an unknown defender, the calc the modifier in reverse
	if (bAttackingUnknownDefender) {
		pAttacker = this;
	}

	// calc attacker bonueses
	if (pAttacker != NULL && pAttackedPlot != NULL) {
		int iTempModifier = 0;

		if (pAttackedPlot->isCity(true, getTeam())) {
			iExtraModifier = -pAttacker->cityAttackModifier();
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL) {
				pCombatDetails->iCityAttackModifier = iExtraModifier;
			}

			if (pAttacker->isBarbarian()) {
				iExtraModifier = GC.getDefineINT("CITY_BARBARIAN_DEFENSE_MODIFIER");
				iTempModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iCityBarbarianDefenseModifier = iExtraModifier;
				}
			}
		}

		if (pAttackedPlot->isHills()) {
			iExtraModifier = -pAttacker->hillsAttackModifier();
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL) {
				pCombatDetails->iHillsAttackModifier = iExtraModifier;
			}
		}

		if (pAttackedPlot->getFeatureType() != NO_FEATURE) {
			iExtraModifier = -pAttacker->featureAttackModifier(pAttackedPlot->getFeatureType());
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL) {
				pCombatDetails->iFeatureAttackModifier = iExtraModifier;
			}
		} else {
			iExtraModifier = -pAttacker->terrainAttackModifier(pAttackedPlot->getTerrainType());
			iModifier += iExtraModifier;
			if (pCombatDetails != NULL) {
				pCombatDetails->iTerrainAttackModifier = iExtraModifier;
			}
		}

		// only compute comparisions if we are the defender with a known attacker
		if (!bAttackingUnknownDefender) {
			FAssertMsg(pAttacker != this, "pAttacker is not expected to be equal with this");

			iExtraModifier = unitClassDefenseModifier(pAttacker->getUnitClassType());
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL) {
				pCombatDetails->iClassDefenseModifier = iExtraModifier;
			}

			iExtraModifier = -pAttacker->unitClassAttackModifier(getUnitClassType());
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL) {
				pCombatDetails->iClassAttackModifier = iExtraModifier;
			}

			if (pAttacker->getUnitCombatType() != NO_UNITCOMBAT) {
				iExtraModifier = unitCombatModifier(pAttacker->getUnitCombatType());
				iTempModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iCombatModifierA = iExtraModifier;
				}
			}
			if (getUnitCombatType() != NO_UNITCOMBAT) {
				iExtraModifier = -pAttacker->unitCombatModifier(getUnitCombatType());
				iTempModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iCombatModifierT = iExtraModifier;
				}
			}

			iExtraModifier = domainModifier(pAttacker->getDomainType());
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL) {
				pCombatDetails->iDomainModifierA = iExtraModifier;
			}

			iExtraModifier = -pAttacker->domainModifier(getDomainType());
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL) {
				pCombatDetails->iDomainModifierT = iExtraModifier;
			}

			if (pAttacker->isAnimal()) {
				iExtraModifier = animalCombatModifier();
				iTempModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iAnimalCombatModifierA = iExtraModifier;
				}
			}

			if (isAnimal()) {
				iExtraModifier = -pAttacker->animalCombatModifier();
				iTempModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iAnimalCombatModifierT = iExtraModifier;
				}
			}
		}

		if (!(pAttacker->isRiver())) {
			if (pAttacker->plot()->isRiverCrossing(directionXY(pAttacker->plot(), pAttackedPlot))) {
				iExtraModifier = -GC.getRIVER_ATTACK_MODIFIER();
				iTempModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iRiverAttackModifier = iExtraModifier;
				}
			}
		}

		if (!(pAttacker->isAmphib())) {
			if (!(pAttackedPlot->isWater()) && pAttacker->plot()->isWater()) {
				iExtraModifier = -GC.getAMPHIB_ATTACK_MODIFIER();
				iTempModifier += iExtraModifier;
				if (pCombatDetails != NULL) {
					pCombatDetails->iAmphibAttackModifier = iExtraModifier;
				}
			}
		}

		if (pAttacker->getKamikazePercent() != 0) {
			iExtraModifier = pAttacker->getKamikazePercent();
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL) {
				pCombatDetails->iKamikazeModifier = iExtraModifier;
			}
		}

		// if we are attacking an unknown defender, then use the reverse of the modifier
		if (bAttackingUnknownDefender) {
			iModifier -= iTempModifier;
		} else {
			iModifier += iTempModifier;
		}
	}

	if (pCombatDetails != NULL) {
		pCombatDetails->iModifierTotal = iModifier;
		pCombatDetails->iBaseCombatStr = baseCombatStr();
	}

	if (iModifier > 0) {
		iCombat = (baseCombatStr() * (iModifier + 100));
	} else {
		iCombat = ((baseCombatStr() * 10000) / (100 - iModifier));
	}

	if (pCombatDetails != NULL) {
		pCombatDetails->iCombat = iCombat;
		pCombatDetails->iMaxCombatStr = std::max(1, iCombat);
		pCombatDetails->iCurrHitPoints = currHitPoints();
		pCombatDetails->iMaxHitPoints = maxHitPoints();
		pCombatDetails->iCurrCombatStr = ((pCombatDetails->iMaxCombatStr * pCombatDetails->iCurrHitPoints) / pCombatDetails->iMaxHitPoints);
	}

	return std::max(1, iCombat);
}


int CvUnit::currCombatStr(const CvPlot* pPlot, const CvUnit* pAttacker, CombatDetails* pCombatDetails) const {
	return ((maxCombatStr(pPlot, pAttacker, pCombatDetails) * currHitPoints()) / maxHitPoints());
}


int CvUnit::currFirepower(const CvPlot* pPlot, const CvUnit* pAttacker) const {
	return ((maxCombatStr(pPlot, pAttacker) + currCombatStr(pPlot, pAttacker) + 1) / 2);
}

// this nomalizes str by firepower, useful for quick odds calcs
// the effect is that a damaged unit will have an effective str lowered by firepower/maxFirepower
// doing the algebra, this means we mulitply by 1/2(1 + currHP)/maxHP = (maxHP + currHP) / (2 * maxHP)
int CvUnit::currEffectiveStr(const CvPlot* pPlot, const CvUnit* pAttacker, CombatDetails* pCombatDetails) const {
	int currStr = currCombatStr(pPlot, pAttacker, pCombatDetails);

	currStr *= (maxHitPoints() + currHitPoints());
	currStr /= (2 * maxHitPoints());

	return currStr;
}

float CvUnit::maxCombatStrFloat(const CvPlot* pPlot, const CvUnit* pAttacker) const {
	return (((float)(maxCombatStr(pPlot, pAttacker))) / 100.0f);
}


float CvUnit::currCombatStrFloat(const CvPlot* pPlot, const CvUnit* pAttacker) const {
	return (((float)(currCombatStr(pPlot, pAttacker))) / 100.0f);
}


bool CvUnit::canFight() const {
	return (baseCombatStr() > 0);
}


bool CvUnit::canAttack() const {
	if (!canFight()) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (isOnlyDefensive()) {
		return false;
	}

	return true;
}
bool CvUnit::canAttack(const CvUnit& defender) const {
	if (!canAttack()) {
		return false;
	}

	if (defender.getDamage() >= combatLimit()) {
		return false;
	}

	// Artillery can't amphibious attack
	if (plot()->isWater() && !defender.plot()->isWater()) {
		if (combatLimit() < 100) {
			return false;
		}
	}

	return true;
}

bool CvUnit::canDefend(const CvPlot* pPlot) const {
	if (pPlot == NULL) {
		pPlot = plot();
	}

	if (!canFight()) {
		return false;
	}

	if (!pPlot->isValidDomainForAction(*this)) {
		if (GC.getDefineINT("LAND_UNITS_CAN_ATTACK_WATER_CITIES") == 0) {
			return false;
		}
	}

	return true;
}


bool CvUnit::canSiege(TeamTypes eTeam) const {
	if (!canDefend()) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (!isEnemy(eTeam)) {
		return false;
	}

	if (!isNeverInvisible()) {
		return false;
	}

	return true;
}


int CvUnit::airBaseCombatStr() const {
	return m_pUnitInfo->getAirCombat();
}


int CvUnit::airMaxCombatStr(const CvUnit* pOther) const {
	if (airBaseCombatStr() == 0) {
		return 0;
	}

	int iModifier = getExtraCombatPercent();

	if (getKamikazePercent() != 0) {
		iModifier += getKamikazePercent();
	}

	if (NULL != pOther) {
		if (pOther->getUnitCombatType() != NO_UNITCOMBAT) {
			iModifier += unitCombatModifier(pOther->getUnitCombatType());
		}

		iModifier += domainModifier(pOther->getDomainType());

		if (pOther->isAnimal()) {
			iModifier += animalCombatModifier();
		}
	}

	int iCombat = 0;
	if (iModifier > 0) {
		iCombat = (airBaseCombatStr() * (iModifier + 100));
	} else {
		iCombat = ((airBaseCombatStr() * 10000) / (100 - iModifier));
	}

	return std::max(1, iCombat);
}


int CvUnit::airCurrCombatStr(const CvUnit* pOther) const {
	return ((airMaxCombatStr(pOther) * currHitPoints()) / maxHitPoints());
}


float CvUnit::airMaxCombatStrFloat(const CvUnit* pOther) const {
	return (((float)(airMaxCombatStr(pOther))) / 100.0f);
}


float CvUnit::airCurrCombatStrFloat(const CvUnit* pOther) const {
	return (((float)(airCurrCombatStr(pOther))) / 100.0f);
}


int CvUnit::combatLimit() const {
	return m_pUnitInfo->getCombatLimit();
}


int CvUnit::airCombatLimit() const {
	return m_pUnitInfo->getAirCombatLimit();
}


bool CvUnit::canAirAttack() const {
	return (airBaseCombatStr() > 0);
}


bool CvUnit::canAirDefend(const CvPlot* pPlot) const {
	if (pPlot == NULL) {
		pPlot = plot();
	}

	if (maxInterceptionProbability() == 0) {
		return false;
	}

	if (getDomainType() != DOMAIN_AIR) {
		// Land units which are cargo cannot intercept
		if (!pPlot->isValidDomainForLocation(*this) || isCargo()) {
			return false;
		}
	}

	return true;
}


int CvUnit::airCombatDamage(const CvUnit* pDefender) const {
	int iOurStrength = airCurrCombatStr(pDefender);
	FAssertMsg(iOurStrength > 0, "Air combat strength is expected to be greater than zero");

	CvPlot* pPlot = pDefender->plot();
	int iTheirStrength = pDefender->maxCombatStr(pPlot, this);

	int iStrengthFactor = ((iOurStrength + iTheirStrength + 1) / 2);

	int iDamage = std::max(1, ((GC.getDefineINT("AIR_COMBAT_DAMAGE") * (iOurStrength + iStrengthFactor)) / (iTheirStrength + iStrengthFactor)));

	CvCity* pCity = pPlot->getPlotCity();
	if (pCity != NULL) {
		iDamage *= std::max(0, (pCity->getAirModifier() + 100));
		iDamage /= 100;
	}

	return iDamage;
}


int CvUnit::rangeCombatDamage(const CvUnit* pDefender) const {
	int iOurStrength = airCurrCombatStr(pDefender);
	FAssertMsg(iOurStrength > 0, "Combat strength is expected to be greater than zero");

	CvPlot* pPlot = pDefender->plot();
	int iTheirStrength = pDefender->maxCombatStr(pPlot, this);

	int iStrengthFactor = ((iOurStrength + iTheirStrength + 1) / 2);

	int iDamage = std::max(1, ((GC.getDefineINT("RANGE_COMBAT_DAMAGE") * (iOurStrength + iStrengthFactor)) / (iTheirStrength + iStrengthFactor)));

	return iDamage;
}


CvUnit* CvUnit::bestInterceptor(const CvPlot* pPlot) const {
	int iBestValue = 0;
	CvUnit* pBestUnit = NULL;

	for (int iI = 0; iI < MAX_PLAYERS; iI++) {
		if (GET_PLAYER((PlayerTypes)iI).isAlive()) {
			if (isEnemy(GET_PLAYER((PlayerTypes)iI).getTeam()) && !isInvisible(GET_PLAYER((PlayerTypes)iI).getTeam(), false, false)) {
				int iLoop;
				for (CvUnit* pLoopUnit = GET_PLAYER((PlayerTypes)iI).firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = GET_PLAYER((PlayerTypes)iI).nextUnit(&iLoop)) {
					if (pLoopUnit->canAirDefend()) {
						if (!pLoopUnit->isMadeInterception()) {
							if ((pLoopUnit->getDomainType() != DOMAIN_AIR) || !(pLoopUnit->hasMoved())) {
								if ((pLoopUnit->getDomainType() != DOMAIN_AIR) || (pLoopUnit->getGroup()->getActivityType() == ACTIVITY_INTERCEPT)) {
									if (plotDistance(pLoopUnit->getX_INLINE(), pLoopUnit->getY_INLINE(), pPlot->getX_INLINE(), pPlot->getY_INLINE()) <= pLoopUnit->airRange()) {
										int iValue = pLoopUnit->currInterceptionProbability();

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

	return pBestUnit;
}


CvUnit* CvUnit::bestSeaPillageInterceptor(CvUnit* pPillager, int iMinOdds) const {
	CvUnit* pBestUnit = NULL;
	int pBestUnitRank = -1;

	for (int iDX = -1; iDX <= 1; ++iDX) {
		for (int iDY = -1; iDY <= 1; ++iDY) {
			CvPlot* pLoopPlot = plotXY(pPillager->getX_INLINE(), pPillager->getY_INLINE(), iDX, iDY);
			if (NULL != pLoopPlot) {
				CLLNode<IDInfo>* pUnitNode = pLoopPlot->headUnitNode();
				while (NULL != pUnitNode) {
					CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
					pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

					if (NULL != pLoopUnit) {
						if (pLoopUnit->area() == pPillager->plot()->area()) {
							if (!pLoopUnit->isInvisible(getTeam(), false)) {
								if (isEnemy(pLoopUnit->getTeam())) {
									if (DOMAIN_SEA == pLoopUnit->getDomainType()) {
										if (ACTIVITY_PATROL == pLoopUnit->getGroup()->getActivityType()) {
											if (NULL == pBestUnit || pLoopUnit->isBetterDefenderThan(pBestUnit, this, &pBestUnitRank)) {
												if (getCombatOdds(pPillager, pLoopUnit) < iMinOdds) {
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

	return pBestUnit;
}


bool CvUnit::isAutomated() const {
	return getGroup()->isAutomated();
}


bool CvUnit::isWaiting() const {
	return getGroup()->isWaiting();
}


bool CvUnit::isFortifyable() const {
	if (!canFight() || noDefensiveBonus() || ((getDomainType() != DOMAIN_LAND) && (getDomainType() != DOMAIN_IMMOBILE))) {
		return false;
	}

	return true;
}


int CvUnit::fortifyModifier() const {
	if (!isFortifyable()) {
		return 0;
	}

	return (getFortifyTurns() * GC.getFORTIFY_MODIFIER_PER_TURN());
}


int CvUnit::experienceNeeded() const {
	if (GC.getUSE_GET_EXPERIENCE_NEEDED_CALLBACK()) // K-Mod. I've writen C to replace the python callback.
	{
		// Use python to determine pillage amounts...
		long lExperienceNeeded;

		lExperienceNeeded = 0;

		CyArgsList argsList;
		argsList.add(getLevel());	// pass in the units level
		argsList.add(getOwnerINLINE());	// pass in the units 

		gDLL->getPythonIFace()->callFunction(PYGameModule, "getExperienceNeeded", argsList.makeFunctionArgs(), &lExperienceNeeded);

		//iExperienceNeeded = (int)lExperienceNeeded;
		lExperienceNeeded = std::min((long)MAX_INT, lExperienceNeeded); // K-Mod

		if (lExperienceNeeded >= 0) // K-Mod
			return (int)lExperienceNeeded;
	}
	// K-Mod. C version of the original python code.
	// Note: python rounds towards negative infinity, but C++ rounds towards 0.
	// So the code needs to be slightly different to achieve the same effect.
	int iExperienceNeeded = getLevel() * getLevel() + 1;

	int iModifier = GET_PLAYER(getOwnerINLINE()).getLevelExperienceModifier();
	if (iModifier != 0)
		iExperienceNeeded = (iExperienceNeeded * (100 + iModifier) + 99) / 100;

	return iExperienceNeeded;
}


int CvUnit::attackXPValue() const {
	return m_pUnitInfo->getXPValueAttack();
}


int CvUnit::defenseXPValue() const {
	return m_pUnitInfo->getXPValueDefense();
}


int CvUnit::maxXPValue() const {
	int iMaxValue;

	iMaxValue = MAX_INT;

	if (isAnimal()) {
		iMaxValue = GC.getGameINLINE().isAnyCivSettled() ? std::min(iMaxValue, GC.getDefineINT("ANIMAL_MAX_XP_VALUE")) : std::min(iMaxValue, GC.getDefineINT("ANIMAL_MAX_XP_VALUE_PRE_SETTLE"));
	}

	if (isBarbarian()) {
		iMaxValue = GC.getGameINLINE().isAnyCivSettled() ? std::min(iMaxValue, GC.getDefineINT("BARBARIAN_MAX_XP_VALUE")) : std::min(iMaxValue, GC.getDefineINT("BARBARIAN_MAX_XP_VALUE_PRE_SETTLE"));
	}

	return iMaxValue;
}


int CvUnit::firstStrikes() const {
	return std::max(0, (m_pUnitInfo->getFirstStrikes() + getExtraFirstStrikes()));
}


int CvUnit::chanceFirstStrikes() const {
	return std::max(0, (m_pUnitInfo->getChanceFirstStrikes() + getExtraChanceFirstStrikes()));
}


int CvUnit::maxFirstStrikes() const {
	return (firstStrikes() + chanceFirstStrikes());
}


bool CvUnit::isRanged() const {
	for (int i = 0; i < getGroupDefinitions(); i++) {
		if (!getArtInfo(i, GET_PLAYER(getOwnerINLINE()).getCurrentEra())->getActAsRanged()) {
			return false;
		}
	}
	return true;
}


bool CvUnit::alwaysInvisible() const {
	return m_pUnitInfo->isInvisible();
}


bool CvUnit::immuneToFirstStrikes() const {
	return (m_pUnitInfo->isFirstStrikeImmune() || (getImmuneToFirstStrikesCount() > 0));
}


bool CvUnit::noDefensiveBonus() const {
	return m_pUnitInfo->isNoDefensiveBonus();
}


bool CvUnit::ignoreBuildingDefense() const {
	return m_pUnitInfo->isIgnoreBuildingDefense();
}


bool CvUnit::canMoveImpassable() const {
	return m_pUnitInfo->isCanMoveImpassable();
}

bool CvUnit::canMoveAllTerrain() const {
	return m_pUnitInfo->isCanMoveAllTerrain();
}

bool CvUnit::flatMovementCost() const {
	return m_pUnitInfo->isFlatMovementCost();
}


bool CvUnit::ignoreTerrainCost() const {
	return m_pUnitInfo->isIgnoreTerrainCost();
}


bool CvUnit::isNeverInvisible() const {
	return (!alwaysInvisible() && (getInvisibleType() == NO_INVISIBLE));
}


bool CvUnit::isInvisible(TeamTypes eTeam, bool bDebug, bool bCheckCargo) const {
	if (bDebug && GC.getGameINLINE().isDebugMode()) {
		return false;
	}

	if (getTeam() == eTeam) {
		return false;
	}

	if (alwaysInvisible()) {
		return true;
	}

	if (bCheckCargo && isCargo()) {
		return true;
	}

	if (getInvisibleType() == NO_INVISIBLE) {
		return false;
	}

	return !(plot()->isInvisibleVisible(eTeam, getInvisibleType()));
}


bool CvUnit::isNukeImmune() const {
	return m_pUnitInfo->isNukeImmune();
}


int CvUnit::maxInterceptionProbability() const {
	return std::max(0, m_pUnitInfo->getInterceptionProbability() + getExtraIntercept());
}


int CvUnit::currInterceptionProbability() const {
	if (getDomainType() != DOMAIN_AIR) {
		return maxInterceptionProbability();
	} else {
		return ((maxInterceptionProbability() * currHitPoints()) / maxHitPoints());
	}
}


int CvUnit::evasionProbability() const {
	return std::max(0, m_pUnitInfo->getEvasionProbability() + getExtraEvasion());
}


int CvUnit::withdrawalProbability() const {
	if (getDomainType() == DOMAIN_LAND && plot()->isWater()) {
		return 0;
	}

	return std::max(0, (m_pUnitInfo->getWithdrawalProbability() + getExtraWithdrawal()));
}


int CvUnit::collateralDamage() const {
	return std::max(0, (m_pUnitInfo->getCollateralDamage()));
}


int CvUnit::collateralDamageLimit() const {
	return std::max(0, m_pUnitInfo->getCollateralDamageLimit() * GC.getMAX_HIT_POINTS() / 100);
}


int CvUnit::collateralDamageMaxUnits() const {
	return std::max(0, m_pUnitInfo->getCollateralDamageMaxUnits());
}


int CvUnit::cityAttackModifier() const {
	return (m_pUnitInfo->getCityAttackModifier() + getExtraCityAttackPercent());
}


int CvUnit::cityDefenseModifier() const {
	return (m_pUnitInfo->getCityDefenseModifier() + getExtraCityDefensePercent());
}


int CvUnit::animalCombatModifier() const {
	return m_pUnitInfo->getAnimalCombatModifier();
}


int CvUnit::hillsAttackModifier() const {
	return (m_pUnitInfo->getHillsAttackModifier() + getExtraHillsAttackPercent());
}


int CvUnit::hillsDefenseModifier() const {
	return (m_pUnitInfo->getHillsDefenseModifier() + getExtraHillsDefensePercent());
}


int CvUnit::terrainAttackModifier(TerrainTypes eTerrain) const {
	FAssertMsg(eTerrain >= 0, "eTerrain is expected to be non-negative (invalid Index)");
	FAssertMsg(eTerrain < GC.getNumTerrainInfos(), "eTerrain is expected to be within maximum bounds (invalid Index)");
	return (m_pUnitInfo->getTerrainAttackModifier(eTerrain) + getExtraTerrainAttackPercent(eTerrain));
}


int CvUnit::terrainDefenseModifier(TerrainTypes eTerrain) const {
	FAssertMsg(eTerrain >= 0, "eTerrain is expected to be non-negative (invalid Index)");
	FAssertMsg(eTerrain < GC.getNumTerrainInfos(), "eTerrain is expected to be within maximum bounds (invalid Index)");
	return (m_pUnitInfo->getTerrainDefenseModifier(eTerrain) + getExtraTerrainDefensePercent(eTerrain));
}


int CvUnit::featureAttackModifier(FeatureTypes eFeature) const {
	FAssertMsg(eFeature >= 0, "eFeature is expected to be non-negative (invalid Index)");
	FAssertMsg(eFeature < GC.getNumFeatureInfos(), "eFeature is expected to be within maximum bounds (invalid Index)");
	return (m_pUnitInfo->getFeatureAttackModifier(eFeature) + getExtraFeatureAttackPercent(eFeature));
}

int CvUnit::featureDefenseModifier(FeatureTypes eFeature) const {
	FAssertMsg(eFeature >= 0, "eFeature is expected to be non-negative (invalid Index)");
	FAssertMsg(eFeature < GC.getNumFeatureInfos(), "eFeature is expected to be within maximum bounds (invalid Index)");
	return (m_pUnitInfo->getFeatureDefenseModifier(eFeature) + getExtraFeatureDefensePercent(eFeature));
}

int CvUnit::unitClassAttackModifier(UnitClassTypes eUnitClass) const {
	FAssertMsg(eUnitClass >= 0, "eUnitClass is expected to be non-negative (invalid Index)");
	FAssertMsg(eUnitClass < GC.getNumUnitClassInfos(), "eUnitClass is expected to be within maximum bounds (invalid Index)");
	return m_pUnitInfo->getUnitClassAttackModifier(eUnitClass);
}


int CvUnit::unitClassDefenseModifier(UnitClassTypes eUnitClass) const {
	FAssertMsg(eUnitClass >= 0, "eUnitClass is expected to be non-negative (invalid Index)");
	FAssertMsg(eUnitClass < GC.getNumUnitClassInfos(), "eUnitClass is expected to be within maximum bounds (invalid Index)");
	return m_pUnitInfo->getUnitClassDefenseModifier(eUnitClass);
}


int CvUnit::unitCombatModifier(UnitCombatTypes eUnitCombat) const {
	FAssertMsg(eUnitCombat >= 0, "eUnitCombat is expected to be non-negative (invalid Index)");
	FAssertMsg(eUnitCombat < GC.getNumUnitCombatInfos(), "eUnitCombat is expected to be within maximum bounds (invalid Index)");
	return (m_pUnitInfo->getUnitCombatModifier(eUnitCombat) + getExtraUnitCombatModifier(eUnitCombat));
}


int CvUnit::domainModifier(DomainTypes eDomain) const {
	FAssertMsg(eDomain >= 0, "eDomain is expected to be non-negative (invalid Index)");
	FAssertMsg(eDomain < NUM_DOMAIN_TYPES, "eDomain is expected to be within maximum bounds (invalid Index)");
	return (m_pUnitInfo->getDomainModifier(eDomain) + getExtraDomainModifier(eDomain));
}


int CvUnit::bombardRate() const {
	return (m_pUnitInfo->getBombardRate() + getExtraBombardRate());
}


int CvUnit::airBombBaseRate() const {
	return m_pUnitInfo->getBombRate();
}


int CvUnit::airBombCurrRate() const {
	return ((airBombBaseRate() * currHitPoints()) / maxHitPoints());
}


SpecialUnitTypes CvUnit::specialCargo() const {
	return ((SpecialUnitTypes)(m_pUnitInfo->getSpecialCargo()));
}


DomainTypes CvUnit::domainCargo() const {
	return ((DomainTypes)(m_pUnitInfo->getDomainCargo()));
}


// Gets the max number of units the transport can carry
int CvUnit::cargoSpace() const {
	return m_iCargoCapacity;
}

void CvUnit::changeCargoSpace(int iChange) {
	if (iChange != 0) {
		m_iCargoCapacity += iChange;
		FAssert(m_iCargoCapacity >= 0);
		setInfoBarDirty(true);
	}
}

bool CvUnit::isFull() const {
	return (getCargo() >= cargoSpace());
}


// Gets the space for specific types of cargo
// Some transport units have limited cargo types and this will return 0 for those if you are not
//  checking the correct cargo type, irrespective of the total free capacity
int CvUnit::cargoSpaceAvailable(SpecialUnitTypes eSpecialCargo, DomainTypes eDomainCargo) const {
	if (specialCargo() != NO_SPECIALUNIT) {
		if (specialCargo() != eSpecialCargo) {
			return 0;
		}
	}

	if (domainCargo() != NO_DOMAIN) {
		if (domainCargo() != eDomainCargo) {
			return 0;
		}
	}

	return std::max(0, (cargoSpace() - getCargo()));
}


bool CvUnit::hasCargo() const {
	return (getCargo() > 0);
}


bool CvUnit::canCargoEnterArea(TeamTypes eTeam, const CvArea* pArea, bool bIgnoreRightOfPassage) const {
	CvPlot* pPlot = plot();

	CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();
	while (pUnitNode != NULL) {
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pPlot->nextUnitNode(pUnitNode);

		if (pLoopUnit->getTransportUnit() == this) {
			if (!pLoopUnit->canEnterArea(eTeam, pArea, bIgnoreRightOfPassage)) {
				return false;
			}
		}
	}

	return true;
}

int CvUnit::getUnitAICargo(UnitAITypes eUnitAI) const {
	int iCount = 0;

	std::vector<CvUnit*> aCargoUnits;
	getCargoUnits(aCargoUnits);
	for (uint i = 0; i < aCargoUnits.size(); ++i) {
		if (aCargoUnits[i]->AI_getUnitAIType() == eUnitAI) {
			++iCount;
		}
	}

	return iCount;
}


int CvUnit::getID() const {
	return m_iID;
}


int CvUnit::getIndex() const {
	return (getID() & FLTA_INDEX_MASK);
}


IDInfo CvUnit::getIDInfo() const {
	IDInfo unit(getOwnerINLINE(), getID());
	return unit;
}


void CvUnit::setID(int iID) {
	m_iID = iID;
}


int CvUnit::getGroupID() const {
	return m_iGroupID;
}


bool CvUnit::isInGroup() const {
	return(getGroupID() != FFreeList::INVALID_INDEX);
}


bool CvUnit::isGroupHead() const // XXX is this used???
{
	return (getGroup()->getHeadUnit() == this);
}


CvSelectionGroup* CvUnit::getGroup() const {
	return GET_PLAYER(getOwnerINLINE()).getSelectionGroup(getGroupID());
}


bool CvUnit::canJoinGroup(const CvPlot* pPlot, CvSelectionGroup* pSelectionGroup) const {
	CvUnit* pHeadUnit;

	// do not allow someone to join a group that is about to be split apart
	// this prevents a case of a never-ending turn
	if (pSelectionGroup->AI_isForceSeparate()) {
		return false;
	}

	if (pSelectionGroup->getOwnerINLINE() == NO_PLAYER) {
		pHeadUnit = pSelectionGroup->getHeadUnit();

		if (pHeadUnit != NULL) {
			if (pHeadUnit->getOwnerINLINE() != getOwnerINLINE()) {
				return false;
			}
		}
	} else {
		if (pSelectionGroup->getOwnerINLINE() != getOwnerINLINE()) {
			return false;
		}
	}

	if (pSelectionGroup->getNumUnits() > 0) {
		if (!(pSelectionGroup->atPlot(pPlot))) {
			return false;
		}

		if (pSelectionGroup->getDomainType() != getDomainType()) {
			return false;
		}
	}

	return true;
}

// K-Mod has edited this function to increase readability and robustness
void CvUnit::joinGroup(CvSelectionGroup* pSelectionGroup, bool bRemoveSelected, bool bRejoin) {
	CvSelectionGroup* pOldSelectionGroup = GET_PLAYER(getOwnerINLINE()).getSelectionGroup(getGroupID());

	if (pOldSelectionGroup && pSelectionGroup == pOldSelectionGroup)
		return; // attempting to join the group we are already in

	CvPlot* pPlot = plot();
	CvSelectionGroup* pNewSelectionGroup = pSelectionGroup;

	if (pNewSelectionGroup == NULL && bRejoin) {
		pNewSelectionGroup = GET_PLAYER(getOwnerINLINE()).addSelectionGroup();
		pNewSelectionGroup->init(pNewSelectionGroup->getID(), getOwnerINLINE());
	}

	if (pNewSelectionGroup == NULL || canJoinGroup(pPlot, pNewSelectionGroup)) {
		if (pOldSelectionGroup != NULL) {
			bool bWasHead = false;
			if (!isHuman()) {
				if (pOldSelectionGroup->getNumUnits() > 1) {
					if (pOldSelectionGroup->getHeadUnit() == this) {
						bWasHead = true;
					}
				}
			}

			pOldSelectionGroup->removeUnit(this);

			// if we were the head, if the head unitAI changed, then force the group to separate (non-humans)
			if (bWasHead) {
				FAssert(pOldSelectionGroup->getHeadUnit() != NULL);
				if (pOldSelectionGroup->getHeadUnit()->AI_getUnitAIType() != AI_getUnitAIType()) {
					pOldSelectionGroup->AI_setForceSeparate();
				}
			}
		}

		if ((pNewSelectionGroup != NULL) && pNewSelectionGroup->addUnit(this, false)) {
			m_iGroupID = pNewSelectionGroup->getID();
		} else {
			m_iGroupID = FFreeList::INVALID_INDEX;
		}

		if (getGroup() != NULL) {
			if (isGroupHead())
				GET_PLAYER(getOwnerINLINE()).updateGroupCycle(getGroup());
			if (getGroup()->getNumUnits() > 1) {
				// For the AI, only wake the group in particular circumstances. This is to avoid AI deadlocks where they just keep grouping and ungroup indefinitely.
				// If the activity type is not changed at all, then that would enable exploits such as adding new units to air patrol groups to bypass the movement conditions.
				if (isHuman()) {
					getGroup()->setAutomateType(NO_AUTOMATE);
					getGroup()->setActivityType(ACTIVITY_AWAKE);
					getGroup()->clearMissionQueue();
					// K-Mod note. the mission queue has to be cleared, because when the shift key is released, the exe automatically sends the autoMission net message.
					// (if the mission queue isn't cleared, the units will immediately begin their message whenever units are added using shift.)
				} else if (getGroup()->AI_getMissionAIType() == MISSIONAI_GROUP || getLastMoveTurn() == GC.getGameINLINE().getTurnSlice())
					getGroup()->setActivityType(ACTIVITY_AWAKE);
				else if (getGroup()->getActivityType() != ACTIVITY_AWAKE)
					getGroup()->setActivityType(ACTIVITY_HOLD); // don't let them cheat.
			}
		}

		if (getTeam() == GC.getGameINLINE().getActiveTeam()) {
			if (pPlot != NULL) {
				pPlot->setFlagDirty(true);
			}
		}

		if (pPlot == gDLL->getInterfaceIFace()->getSelectionPlot()) {
			gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
		}
	}

	if (bRemoveSelected && IsSelected()) {
		gDLL->getInterfaceIFace()->removeFromSelectionList(this);
	}
}


int CvUnit::getHotKeyNumber() {
	return m_iHotKeyNumber;
}


void CvUnit::setHotKeyNumber(int iNewValue) {
	FAssert(getOwnerINLINE() != NO_PLAYER);

	if (getHotKeyNumber() != iNewValue) {
		if (iNewValue != -1) {
			int iLoop;
			for (CvUnit* pLoopUnit = GET_PLAYER(getOwnerINLINE()).firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = GET_PLAYER(getOwnerINLINE()).nextUnit(&iLoop)) {
				if (pLoopUnit->getHotKeyNumber() == iNewValue) {
					pLoopUnit->setHotKeyNumber(-1);
				}
			}
		}

		m_iHotKeyNumber = iNewValue;

		if (IsSelected()) {
			gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
		}
	}
}


int CvUnit::getX() const {
	return m_iX;
}


int CvUnit::getY() const {
	return m_iY;
}


void CvUnit::setXY(int iX, int iY, bool bGroup, bool bUpdate, bool bShow, bool bCheckPlotVisible) {
	FAssert(!at(iX, iY));
	FAssert(!isFighting());
	FAssert((iX == INVALID_PLOT_COORD) || (GC.getMapINLINE().plotINLINE(iX, iY)->getX_INLINE() == iX));
	FAssert((iY == INVALID_PLOT_COORD) || (GC.getMapINLINE().plotINLINE(iX, iY)->getY_INLINE() == iY));

	setBlockading(false);

	// K-Mod. I've adjusted the code to allow cargo units to stay in their groups when possible.
	bShow = bShow && bGroup && !isCargo();
	if (!bGroup)
		joinGroup(0, true);

	CvPlot* pNewPlot = GC.getMapINLINE().plotINLINE(iX, iY);
	if (pNewPlot != NULL) {
		CvUnit* pTransportUnit = getTransportUnit();

		if (pTransportUnit != NULL) {
			if (!(pTransportUnit->atPlot(pNewPlot))) {
				setTransportUnit(NULL);
			}
		}

		CLinkList<IDInfo> oldUnits;
		if (canFight()) {
			oldUnits.clear();

			CLLNode<IDInfo>* pUnitNode = pNewPlot->headUnitNode();
			while (pUnitNode != NULL) {
				oldUnits.insertAtEnd(pUnitNode->m_data);
				pUnitNode = pNewPlot->nextUnitNode(pUnitNode);
			}

			pUnitNode = oldUnits.head();

			while (pUnitNode != NULL) {
				CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
				pUnitNode = oldUnits.next(pUnitNode);

				if (pLoopUnit != NULL) {
					if (isEnemy(pLoopUnit->getTeam(), pNewPlot) || pLoopUnit->isEnemy(getTeam())) {
						if (!pLoopUnit->canCoexistWithEnemyUnit(getTeam())) {
							if (NO_UNITCLASS == pLoopUnit->getUnitInfo().getUnitCaptureClassType() && pLoopUnit->canDefend(pNewPlot)) {
								pLoopUnit->jumpToNearestValidPlot(); // can kill unit
							} else {
								if (!isHiddenNationality() && !pLoopUnit->isHiddenNationality()) {
									GET_TEAM(pLoopUnit->getTeam()).changeWarWeariness(getTeam(), *pNewPlot, GC.getDefineINT("WW_UNIT_CAPTURED"));
									GET_TEAM(getTeam()).changeWarWeariness(pLoopUnit->getTeam(), *pNewPlot, GC.getDefineINT("WW_CAPTURED_UNIT"));
									GET_TEAM(getTeam()).AI_changeWarSuccess(pLoopUnit->getTeam(), GC.getDefineINT("WAR_SUCCESS_UNIT_CAPTURING"));
								}

								// Slavers will enslave any unit on the plot that they have room for, any other unit will ignore them
								if (isSlaver() && enslaveUnit(this, pLoopUnit)) {
									pLoopUnit->kill(false, getOwnerINLINE());
								} else {
									if (!isNoCapture()) {
										pLoopUnit->setCapturingPlayer(getOwnerINLINE());
									}

									pLoopUnit->kill(false, getOwnerINLINE());
								}
							}
						}
					}
				}
			}
		}

		if (pNewPlot->isGoody(getTeam())) {
			GET_PLAYER(getOwnerINLINE()).doGoody(pNewPlot, this);
		}
	}

	CvPlot* pOldPlot = plot();
	if (pOldPlot != NULL) {
		pOldPlot->removeUnit(this, bUpdate && !hasCargo());

		pOldPlot->changeAdjacentSight(getTeam(), visibilityRange(), false, this, true);

		pOldPlot->area()->changeUnitsPerPlayer(getOwnerINLINE(), -1);
		pOldPlot->area()->changePower(getOwnerINLINE(), -(m_pUnitInfo->getPowerValue()));

		if (AI_getUnitAIType() != NO_UNITAI) {
			pOldPlot->area()->changeNumAIUnits(getOwnerINLINE(), AI_getUnitAIType(), -1);
		}

		if (isAnimal()) {
			pOldPlot->area()->changeAnimalsPerPlayer(getOwnerINLINE(), -1);
		}

		if (pOldPlot->getTeam() != getTeam() && (pOldPlot->getTeam() == NO_TEAM || !GET_TEAM(pOldPlot->getTeam()).isVassal(getTeam()))) {
			GET_PLAYER(getOwnerINLINE()).changeNumOutsideUnits(-1);
		}

		setLastMoveTurn(GC.getGameINLINE().getTurnSlice());

		CvCity* pOldCity = pOldPlot->getPlotCity();

		if (pOldCity != NULL) {
			if (isMilitaryHappiness()) {
				pOldCity->changeMilitaryHappinessUnits(-1);
			}
		}

		CvCity* pWorkingCity = pOldPlot->getWorkingCity();

		if (pWorkingCity != NULL) {
			if (canSiege(pWorkingCity->getTeam())) {
				pWorkingCity->AI_setAssignWorkDirty(true);
			}
		}

		if (pOldPlot->isWater()) {
			for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++) {
				CvPlot* pLoopPlot = plotDirection(pOldPlot->getX_INLINE(), pOldPlot->getY_INLINE(), ((DirectionTypes)iI));

				if (pLoopPlot != NULL) {
					if (pLoopPlot->isWater()) {
						pWorkingCity = pLoopPlot->getWorkingCity();

						if (pWorkingCity != NULL) {
							if (canSiege(pWorkingCity->getTeam())) {
								pWorkingCity->AI_setAssignWorkDirty(true);
							}
						}
					}
				}
			}
		}

		if (pOldPlot->isActiveVisible(true)) {
			pOldPlot->updateMinimapColor();
		}

		if (pOldPlot == gDLL->getInterfaceIFace()->getSelectionPlot()) {
			gDLL->getInterfaceIFace()->verifyPlotListColumn();

			gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
		}
	}

	if (pNewPlot != NULL) {
		m_iX = pNewPlot->getX_INLINE();
		m_iY = pNewPlot->getY_INLINE();
	} else {
		m_iX = INVALID_PLOT_COORD;
		m_iY = INVALID_PLOT_COORD;
	}

	FAssertMsg(plot() == pNewPlot, "plot is expected to equal pNewPlot");

	if (pNewPlot != NULL) {
		CvCity* pNewCity = pNewPlot->getPlotCity();
		if (pNewCity != NULL) {
			if (isEnemy(pNewCity->getTeam()) && !canCoexistWithEnemyUnit(pNewCity->getTeam()) && canFight()) {
				GET_TEAM(getTeam()).changeWarWeariness(pNewCity->getTeam(), *pNewPlot, GC.getDefineINT("WW_CAPTURED_CITY"));
				// Double war success if capturing capital city, always a significant blow to enemy
				// pNewCity still points to old city here, hasn't been acquired yet
				GET_TEAM(getTeam()).AI_changeWarSuccess(pNewCity->getTeam(), (pNewCity->isCapital() ? 2 : 1) * GC.getWAR_SUCCESS_CITY_CAPTURING());

				PlayerTypes eNewOwner = GET_PLAYER(getOwnerINLINE()).pickConqueredCityOwner(*pNewCity);

				if (NO_PLAYER != eNewOwner) {
					GET_PLAYER(eNewOwner).acquireCity(pNewCity, true, false, true); // will delete the pointer
					pNewCity = NULL;
				}
			}
		}

		//update facing direction
		if (pOldPlot != NULL) {
			DirectionTypes newDirection = estimateDirection(pOldPlot, pNewPlot);
			if (newDirection != NO_DIRECTION)
				m_eFacingDirection = newDirection;
		}

		setFortifyTurns(0);

		pNewPlot->changeAdjacentSight(getTeam(), visibilityRange(), true, this, true); // needs to be here so that the square is considered visible when we move into it...

		pNewPlot->addUnit(this, bUpdate && !hasCargo());

		pNewPlot->area()->changeUnitsPerPlayer(getOwnerINLINE(), 1);
		pNewPlot->area()->changePower(getOwnerINLINE(), m_pUnitInfo->getPowerValue());

		if (AI_getUnitAIType() != NO_UNITAI) {
			pNewPlot->area()->changeNumAIUnits(getOwnerINLINE(), AI_getUnitAIType(), 1);
		}

		if (isAnimal()) {
			pNewPlot->area()->changeAnimalsPerPlayer(getOwnerINLINE(), 1);
		}

		if (pNewPlot->getTeam() != getTeam() && (pNewPlot->getTeam() == NO_TEAM || !GET_TEAM(pNewPlot->getTeam()).isVassal(getTeam()))) {
			GET_PLAYER(getOwnerINLINE()).changeNumOutsideUnits(1);
		}

		if (shouldLoadOnMove(pNewPlot)) {
			load();
		}

		if (!alwaysInvisible() && !isHiddenNationality()) // K-Mod (just this condition)
		{
			for (int iI = 0; iI < MAX_CIV_TEAMS; iI++) {
				if (GET_TEAM((TeamTypes)iI).isAlive()) {
					if (!isInvisible(((TeamTypes)iI), false)) {
						if (pNewPlot->isVisible((TeamTypes)iI, false)) {
							GET_TEAM((TeamTypes)iI).meet(getTeam(), true);
						}
					}
				}
			}
		}

		pNewCity = pNewPlot->getPlotCity();

		if (pNewCity != NULL) {
			if (isMilitaryHappiness()) {
				pNewCity->changeMilitaryHappinessUnits(1);
			}
		}

		CvCity* pWorkingCity = pNewPlot->getWorkingCity();
		if (pWorkingCity != NULL) {
			if (canSiege(pWorkingCity->getTeam())) {
				pWorkingCity->verifyWorkingPlot(pWorkingCity->getCityPlotIndex(pNewPlot));
			}
		}

		if (pNewPlot->isActiveVisible(true)) {
			pNewPlot->updateMinimapColor();
		}

		if (GC.IsGraphicsInitialized()) {
			//override bShow if check plot visible
			if (bCheckPlotVisible && pNewPlot->isVisibleToWatchingHuman())
				bShow = true;

			if (bShow) {
				QueueMove(pNewPlot);
			} else {
				SetPosition(pNewPlot);
			}
		}

		if (pNewPlot == gDLL->getInterfaceIFace()->getSelectionPlot()) {
			gDLL->getInterfaceIFace()->verifyPlotListColumn();

			gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
		}
	}

	if (pOldPlot != NULL) {
		if (hasCargo()) {
			std::vector<std::pair<PlayerTypes, int> > cargo_groups; // K-Mod. (player, group) pair.
			CLLNode<IDInfo>* pUnitNode = pOldPlot->headUnitNode();
			while (pUnitNode != NULL) {
				CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
				pUnitNode = pOldPlot->nextUnitNode(pUnitNode);

				if (pLoopUnit->getTransportUnit() == this) {
					pLoopUnit->setXY(iX, iY, bGroup, false);
					cargo_groups.push_back(std::make_pair(pLoopUnit->getOwnerINLINE(), pLoopUnit->getGroupID())); // K-Mod
				}
			}
			// If the group of the cargo units we just moved includes units that are not transported
			// by this transport group, then we need to separate them.

			// first remove duplicate group numbers
			std::sort(cargo_groups.begin(), cargo_groups.end());
			cargo_groups.erase(std::unique(cargo_groups.begin(), cargo_groups.end()), cargo_groups.end());

			// now check the units in each group
			for (size_t i = 0; i < cargo_groups.size(); i++) {
				CvSelectionGroup* pCargoGroup = GET_PLAYER(cargo_groups[i].first).getSelectionGroup(cargo_groups[i].second);
				FAssert(pCargoGroup);
				pUnitNode = pCargoGroup->headUnitNode();
				ActivityTypes eOldActivityType = pCargoGroup->getActivityType();

				while (pUnitNode) {
					CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
					pUnitNode = pCargoGroup->nextUnitNode(pUnitNode);

					if (pLoopUnit->getTransportUnit() == NULL ||
						pLoopUnit->getTransportUnit()->getGroup() != getGroup()) {
						pLoopUnit->joinGroup(NULL, true);
						if (eOldActivityType != ACTIVITY_MISSION) {
							pLoopUnit->getGroup()->setActivityType(eOldActivityType);
						}
					}
					// while we're here, update the air-circling animation for fighter-planes
					else if (eOldActivityType == ACTIVITY_INTERCEPT)
						pLoopUnit->airCircle(true);
				}
			}
		}
	}

	if (bUpdate && hasCargo()) {
		if (pOldPlot != NULL) {
			pOldPlot->updateCenterUnit();
			pOldPlot->setFlagDirty(true);
		}

		if (pNewPlot != NULL) {
			pNewPlot->updateCenterUnit();
			pNewPlot->setFlagDirty(true);
		}
	}

	FAssert(pOldPlot != pNewPlot);
	// K-Mod. Only update the group cycle here if we are placing this unit on the map for the first time.
	if (!pOldPlot)
		GET_PLAYER(getOwnerINLINE()).updateGroupCycle(getGroup());

	setInfoBarDirty(true);

	if (IsSelected()) {
		if (isFound()) {
			gDLL->getInterfaceIFace()->setDirty(GlobeLayer_DIRTY_BIT, true);
			gDLL->getEngineIFace()->updateFoundingBorder();
		}

		gDLL->getInterfaceIFace()->setDirty(ColoredPlots_DIRTY_BIT, true);
	}

	//update glow
	if (pNewPlot != NULL) {
		gDLL->getEntityIFace()->updateEnemyGlow(getUnitEntity());
	}

	// report event to Python, along with some other key state
	CvEventReporter::getInstance().unitSetXY(pNewPlot, this);
}


bool CvUnit::at(int iX, int iY) const {
	return((getX_INLINE() == iX) && (getY_INLINE() == iY));
}


bool CvUnit::atPlot(const CvPlot* pPlot) const {
	return (plot() == pPlot);
}


CvPlot* CvUnit::plot() const {
	return GC.getMapINLINE().plotSorenINLINE(getX_INLINE(), getY_INLINE());
}


int CvUnit::getArea() const {
	return plot()->getArea();
}


CvArea* CvUnit::area() const {
	return plot()->area();
}


bool CvUnit::onMap() const {
	return (plot() != NULL);
}


int CvUnit::getLastMoveTurn() const {
	return m_iLastMoveTurn;
}


void CvUnit::setLastMoveTurn(int iNewValue) {
	m_iLastMoveTurn = iNewValue;
	FAssert(getLastMoveTurn() >= 0);
}


CvPlot* CvUnit::getReconPlot() const {
	return GC.getMapINLINE().plotSorenINLINE(m_iReconX, m_iReconY);
}


void CvUnit::setReconPlot(CvPlot* pNewValue) {
	CvPlot* pOldPlot = getReconPlot();
	if (pOldPlot != pNewValue) {
		if (pOldPlot != NULL) {
			pOldPlot->changeAdjacentSight(getTeam(), GC.getDefineINT("RECON_VISIBILITY_RANGE"), false, this, true);
			pOldPlot->changeReconCount(-1); // changeAdjacentSight() tests for getReconCount()
		}

		if (pNewValue == NULL) {
			m_iReconX = INVALID_PLOT_COORD;
			m_iReconY = INVALID_PLOT_COORD;
		} else {
			m_iReconX = pNewValue->getX_INLINE();
			m_iReconY = pNewValue->getY_INLINE();

			pNewValue->changeReconCount(1); // changeAdjacentSight() tests for getReconCount()
			pNewValue->changeAdjacentSight(getTeam(), GC.getDefineINT("RECON_VISIBILITY_RANGE"), true, this, true);
		}
	}
}


int CvUnit::getGameTurnCreated() const {
	return m_iGameTurnCreated;
}


void CvUnit::setGameTurnCreated(int iNewValue) {
	m_iGameTurnCreated = iNewValue;
	FAssert(getGameTurnCreated() >= 0);
}


int CvUnit::getDamage() const {
	return m_iDamage;
}


void CvUnit::setDamage(int iNewValue, PlayerTypes ePlayer, bool bNotifyEntity) {
	int iOldValue = getDamage();

	m_iDamage = range(iNewValue, 0, maxHitPoints());

	FAssertMsg(currHitPoints() >= 0, "currHitPoints() is expected to be non-negative (invalid Index)");

	if (iOldValue != getDamage()) {
		if (GC.getGameINLINE().isFinalInitialized() && bNotifyEntity) {
			NotifyEntity(MISSION_DAMAGE);
		}

		setInfoBarDirty(true);

		if (IsSelected()) {
			gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
		}

		if (plot() == gDLL->getInterfaceIFace()->getSelectionPlot()) {
			gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
		}
	}

	if (isDead()) {
		kill(true, ePlayer);
	}
}


void CvUnit::changeDamage(int iChange, PlayerTypes ePlayer) {
	setDamage((getDamage() + iChange), ePlayer);
}


int CvUnit::getMoves() const {
	return m_iMoves;
}


void CvUnit::setMoves(int iNewValue) {
	if (getMoves() != iNewValue) {
		m_iMoves = iNewValue;

		FAssert(getMoves() >= 0);

		CvPlot* pPlot = plot();
		if (getTeam() == GC.getGameINLINE().getActiveTeam()) {
			if (pPlot != NULL) {
				pPlot->setFlagDirty(true);
			}
		}

		if (IsSelected()) {
			gDLL->getFAStarIFace()->ForceReset(&GC.getInterfacePathFinder());

			gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
		}

		if (pPlot == gDLL->getInterfaceIFace()->getSelectionPlot()) {
			gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
		}
	}
}


void CvUnit::changeMoves(int iChange) {
	setMoves(getMoves() + iChange);
}


void CvUnit::finishMoves() {
	setMoves(maxMoves());
}


int CvUnit::getExperience100() const {
	return m_iExperience;
}

void CvUnit::setExperience100(int iNewValue, int iMax) {
	if ((getExperience100() != iNewValue) && (getExperience100() < ((iMax == -1) ? MAX_INT : iMax))) {
		m_iExperience = std::min(((iMax == -1) ? MAX_INT : iMax), iNewValue);
		FAssert(getExperience100() >= 0);

		if (IsSelected()) {
			gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
		}
	}
}

void CvUnit::changeExperience100(int iChange, int iMax, bool bFromCombat, bool bInBorders, bool bUpdateGlobal, bool bExpFromBarb) {
	int iUnitExperience = iChange;

	if (bFromCombat) {
		CvPlayer& kPlayer = GET_PLAYER(getOwnerINLINE());

		int iCombatExperienceMod = bExpFromBarb ? kPlayer.getBarbarianLeaderRateModifier() : kPlayer.getGreatGeneralRateModifier();

		if (bInBorders && !bExpFromBarb) {
			iCombatExperienceMod += kPlayer.getDomesticGreatGeneralRateModifier() + kPlayer.getExpInBorderModifier();
			iUnitExperience += (iChange * kPlayer.getExpInBorderModifier()) / 100;
		}

		if (bUpdateGlobal) {
			int iChangeTotal = iChange + ((iChange * iCombatExperienceMod) / 100);
			if (bExpFromBarb) {
				kPlayer.changeBarbarianFractionalExperience(iChangeTotal);
			} else {
				kPlayer.changeFractionalCombatExperience(iChangeTotal);
			}
		}

		if (getExperiencePercent() != 0) {
			iUnitExperience *= std::max(0, 100 + getExperiencePercent());
			iUnitExperience /= 100;
		}
	}

	setExperience100((getExperience100() + iUnitExperience), iMax);
}

int CvUnit::getExperience() const {
	return getExperience100() / 100;
}

void CvUnit::setExperience(int iNewValue, int iMax) {
	setExperience100(iNewValue * 100, iMax > 0 && iMax != MAX_INT ? iMax * 100 : -1);
}

void CvUnit::changeExperience(int iChange, int iMax, bool bFromCombat, bool bInBorders, bool bUpdateGlobal, bool bExpFromBarb) {
	changeExperience100(iChange * 100, iMax > 0 && iMax != MAX_INT ? iMax * 100 : -1, bFromCombat, bInBorders, bUpdateGlobal, bExpFromBarb);
}

int CvUnit::getLevel() const {
	return m_iLevel;
}

void CvUnit::setLevel(int iNewValue) {
	if (getLevel() != iNewValue) {
		m_iLevel = iNewValue;
		FAssert(getLevel() >= 0);

		if (getLevel() > GET_PLAYER(getOwnerINLINE()).getHighestUnitLevel()) {
			GET_PLAYER(getOwnerINLINE()).setHighestUnitLevel(getLevel());
		}

		if (IsSelected()) {
			gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
		}
	}
}

void CvUnit::changeLevel(int iChange) {
	setLevel(getLevel() + iChange);
}

// Returns the number of units that the transport has as cargo
int CvUnit::getCargo() const {
	return m_iCargo;
}

void CvUnit::changeCargo(int iChange) {
	m_iCargo += iChange;
	FAssert(getCargo() >= 0);
}

void CvUnit::getCargoUnits(std::vector<CvUnit*>& aUnits) const {
	aUnits.clear();

	if (hasCargo()) {
		CvPlot* pPlot = plot();
		CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();
		while (pUnitNode != NULL) {
			CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
			pUnitNode = pPlot->nextUnitNode(pUnitNode);
			if (pLoopUnit->getTransportUnit() == this) {
				aUnits.push_back(pLoopUnit);
			}
		}
	}

	FAssert(getCargo() == aUnits.size());
}

CvPlot* CvUnit::getAttackPlot() const {
	return GC.getMapINLINE().plotSorenINLINE(m_iAttackPlotX, m_iAttackPlotY);
}


void CvUnit::setAttackPlot(const CvPlot* pNewValue, bool bAirCombat) {
	if (getAttackPlot() != pNewValue) {
		if (pNewValue != NULL) {
			m_iAttackPlotX = pNewValue->getX_INLINE();
			m_iAttackPlotY = pNewValue->getY_INLINE();
		} else {
			m_iAttackPlotX = INVALID_PLOT_COORD;
			m_iAttackPlotY = INVALID_PLOT_COORD;
		}
	}

	m_bAirCombat = bAirCombat;
}

bool CvUnit::isAirCombat() const {
	return m_bAirCombat;
}

int CvUnit::getCombatTimer() const {
	return m_iCombatTimer;
}

void CvUnit::setCombatTimer(int iNewValue) {
	m_iCombatTimer = iNewValue;
	FAssert(getCombatTimer() >= 0);
}

void CvUnit::changeCombatTimer(int iChange) {
	setCombatTimer(getCombatTimer() + iChange);
}

int CvUnit::getCombatFirstStrikes() const {
	return m_iCombatFirstStrikes;
}

void CvUnit::setCombatFirstStrikes(int iNewValue) {
	m_iCombatFirstStrikes = iNewValue;
	FAssert(getCombatFirstStrikes() >= 0);
}

void CvUnit::changeCombatFirstStrikes(int iChange) {
	setCombatFirstStrikes(getCombatFirstStrikes() + iChange);
}

int CvUnit::getFortifyTurns() const {
	return m_iFortifyTurns;
}

void CvUnit::setFortifyTurns(int iNewValue) {
	iNewValue = range(iNewValue, 0, GC.getDefineINT("MAX_FORTIFY_TURNS"));

	if (iNewValue != getFortifyTurns()) {
		m_iFortifyTurns = iNewValue;
		setInfoBarDirty(true);
	}
}

void CvUnit::changeFortifyTurns(int iChange) {
	setFortifyTurns(getFortifyTurns() + iChange);
}

int CvUnit::getBlitzCount() const {
	return m_iBlitzCount;
}

bool CvUnit::isBlitz() const {
	return (getBlitzCount() > 0);
}

void CvUnit::changeBlitzCount(int iChange) {
	m_iBlitzCount += iChange;
	FAssert(getBlitzCount() >= 0);
}

int CvUnit::getAmphibCount() const {
	return m_iAmphibCount;
}

bool CvUnit::isAmphib() const {
	return (getAmphibCount() > 0);
}

void CvUnit::changeAmphibCount(int iChange) {
	m_iAmphibCount += iChange;
	FAssert(getAmphibCount() >= 0);
}

int CvUnit::getRiverCount() const {
	return m_iRiverCount;
}

bool CvUnit::isRiver() const {
	return (getRiverCount() > 0);
}

void CvUnit::changeRiverCount(int iChange) {
	m_iRiverCount += iChange;
	FAssert(getRiverCount() >= 0);
}

int CvUnit::getEnemyRouteCount() const {
	return m_iEnemyRouteCount;
}

bool CvUnit::isEnemyRoute() const {
	return (getEnemyRouteCount() > 0);
}

void CvUnit::changeEnemyRouteCount(int iChange) {
	m_iEnemyRouteCount += iChange;
	FAssert(getEnemyRouteCount() >= 0);
}

int CvUnit::getAlwaysHealCount() const {
	return m_iAlwaysHealCount;
}

bool CvUnit::isAlwaysHeal() const {
	return (getAlwaysHealCount() > 0);
}

void CvUnit::changeAlwaysHealCount(int iChange) {
	m_iAlwaysHealCount += iChange;
	FAssert(getAlwaysHealCount() >= 0);
}

int CvUnit::getHillsDoubleMoveCount() const {
	return m_iHillsDoubleMoveCount;
}

bool CvUnit::isHillsDoubleMove() const {
	return (getHillsDoubleMoveCount() > 0);
}

void CvUnit::changeHillsDoubleMoveCount(int iChange) {
	m_iHillsDoubleMoveCount += iChange;
	FAssert(getHillsDoubleMoveCount() >= 0);
}

int CvUnit::getImmuneToFirstStrikesCount() const {
	return m_iImmuneToFirstStrikesCount;
}

void CvUnit::changeImmuneToFirstStrikesCount(int iChange) {
	m_iImmuneToFirstStrikesCount += iChange;
	FAssert(getImmuneToFirstStrikesCount() >= 0);
}


int CvUnit::getExtraVisibilityRange() const {
	return m_iExtraVisibilityRange;
}

void CvUnit::changeExtraVisibilityRange(int iChange) {
	if (iChange != 0) {
		plot()->changeAdjacentSight(getTeam(), visibilityRange(), false, this, true);

		m_iExtraVisibilityRange += iChange;
		FAssert(getExtraVisibilityRange() >= 0);

		plot()->changeAdjacentSight(getTeam(), visibilityRange(), true, this, true);
	}
}

int CvUnit::getExtraMoves() const {
	return m_iExtraMoves;
}


void CvUnit::changeExtraMoves(int iChange) {
	m_iExtraMoves += iChange;
	// FAssert(getExtraMoves() >= 0); assert is no longer valid as at least one naval promotion reduces unit movement
}


int CvUnit::getExtraMoveDiscount() const {
	return m_iExtraMoveDiscount;
}


void CvUnit::changeExtraMoveDiscount(int iChange) {
	m_iExtraMoveDiscount += iChange;
	FAssert(getExtraMoveDiscount() >= 0);
}

int CvUnit::getExtraAirRange() const {
	return m_iExtraAirRange;
}

void CvUnit::changeExtraAirRange(int iChange) {
	m_iExtraAirRange += iChange;
}

int CvUnit::getExtraIntercept() const {
	return m_iExtraIntercept;
}

void CvUnit::changeExtraIntercept(int iChange) {
	m_iExtraIntercept += iChange;
}

int CvUnit::getExtraEvasion() const {
	return m_iExtraEvasion;
}

void CvUnit::changeExtraEvasion(int iChange) {
	m_iExtraEvasion += iChange;
}

int CvUnit::getExtraFirstStrikes() const {
	return m_iExtraFirstStrikes;
}

void CvUnit::changeExtraFirstStrikes(int iChange) {
	m_iExtraFirstStrikes += iChange;
	FAssert(getExtraFirstStrikes() >= 0);
}

int CvUnit::getExtraChanceFirstStrikes() const {
	return m_iExtraChanceFirstStrikes;
}

void CvUnit::changeExtraChanceFirstStrikes(int iChange) {
	m_iExtraChanceFirstStrikes += iChange;
	FAssert(getExtraChanceFirstStrikes() >= 0);
}


int CvUnit::getExtraWithdrawal() const {
	return m_iExtraWithdrawal;
}


void CvUnit::changeExtraWithdrawal(int iChange) {
	m_iExtraWithdrawal += iChange;
	FAssert(withdrawalProbability() >= 0); // K-Mod. (the 'extra' can be negative during sea-patrol battles.)
}

int CvUnit::getExtraCollateralDamage() const {
	return m_iExtraCollateralDamage;
}

void CvUnit::changeExtraCollateralDamage(int iChange) {
	m_iExtraCollateralDamage += iChange;
	FAssert(getExtraCollateralDamage() >= 0);
}

int CvUnit::getExtraBombardRate() const {
	return m_iExtraBombardRate;
}

void CvUnit::changeExtraBombardRate(int iChange) {
	m_iExtraBombardRate += iChange;
	FAssert(getExtraBombardRate() >= 0);
}

int CvUnit::getExtraEnemyHeal() const {
	return m_iExtraEnemyHeal;
}

void CvUnit::changeExtraEnemyHeal(int iChange) {
	m_iExtraEnemyHeal += iChange;
	FAssert(getExtraEnemyHeal() >= 0);
}

int CvUnit::getExtraNeutralHeal() const {
	return m_iExtraNeutralHeal;
}

void CvUnit::changeExtraNeutralHeal(int iChange) {
	m_iExtraNeutralHeal += iChange;
	FAssert(getExtraNeutralHeal() >= 0);
}

int CvUnit::getExtraFriendlyHeal() const {
	return m_iExtraFriendlyHeal;
}


void CvUnit::changeExtraFriendlyHeal(int iChange) {
	m_iExtraFriendlyHeal += iChange;
	FAssert(getExtraFriendlyHeal() >= 0);
}

int CvUnit::getSameTileHeal() const {
	return m_iSameTileHeal;
}

void CvUnit::changeSameTileHeal(int iChange) {
	m_iSameTileHeal += iChange;
	FAssert(getSameTileHeal() >= 0);
}

int CvUnit::getAdjacentTileHeal() const {
	return m_iAdjacentTileHeal;
}

void CvUnit::changeAdjacentTileHeal(int iChange) {
	m_iAdjacentTileHeal += iChange;
	FAssert(getAdjacentTileHeal() >= 0);
}

int CvUnit::getExtraCombatPercent() const {
	return m_iExtraCombatPercent;
}

void CvUnit::changeExtraCombatPercent(int iChange) {
	if (iChange != 0) {
		m_iExtraCombatPercent += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getExtraCityAttackPercent() const {
	return m_iExtraCityAttackPercent;
}

void CvUnit::changeExtraCityAttackPercent(int iChange) {
	if (iChange != 0) {
		m_iExtraCityAttackPercent += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getExtraCityDefensePercent() const {
	return m_iExtraCityDefensePercent;
}

void CvUnit::changeExtraCityDefensePercent(int iChange) {
	if (iChange != 0) {
		m_iExtraCityDefensePercent += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getExtraHillsAttackPercent() const {
	return m_iExtraHillsAttackPercent;
}

void CvUnit::changeExtraHillsAttackPercent(int iChange) {
	if (iChange != 0) {
		m_iExtraHillsAttackPercent += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getExtraHillsDefensePercent() const {
	return m_iExtraHillsDefensePercent;
}

void CvUnit::changeExtraHillsDefensePercent(int iChange) {
	if (iChange != 0) {
		m_iExtraHillsDefensePercent += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getRevoltProtection() const {
	return m_iRevoltProtection;
}

void CvUnit::changeRevoltProtection(int iChange) {
	if (iChange != 0) {
		m_iRevoltProtection += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getCollateralDamageProtection() const {
	return m_iCollateralDamageProtection;
}

void CvUnit::changeCollateralDamageProtection(int iChange) {
	if (iChange != 0) {
		m_iCollateralDamageProtection += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getPillageChange() const {
	return m_iPillageChange;
}

void CvUnit::changePillageChange(int iChange) {
	if (iChange != 0) {
		m_iPillageChange += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getUpgradeDiscount() const {
	return m_iUpgradeDiscount;
}

void CvUnit::changeUpgradeDiscount(int iChange) {
	if (iChange != 0) {
		m_iUpgradeDiscount += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getExperiencePercent() const {
	return m_iExperiencePercent;
}

void CvUnit::changeExperiencePercent(int iChange) {
	if (iChange != 0) {
		m_iExperiencePercent += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getKamikazePercent() const {
	return m_iKamikazePercent;
}

void CvUnit::changeKamikazePercent(int iChange) {
	if (iChange != 0) {
		m_iKamikazePercent += iChange;

		setInfoBarDirty(true);
	}
}

DirectionTypes CvUnit::getFacingDirection(bool checkLineOfSightProperty) const {
	if (checkLineOfSightProperty) {
		if (m_pUnitInfo->isLineOfSight()) {
			return m_eFacingDirection; //only look in facing direction
		} else {
			return NO_DIRECTION; //look in all directions
		}
	} else {
		return m_eFacingDirection;
	}
}

void CvUnit::setFacingDirection(DirectionTypes eFacingDirection) {
	if (eFacingDirection != m_eFacingDirection) {
		if (m_pUnitInfo->isLineOfSight()) {
			//remove old fog
			plot()->changeAdjacentSight(getTeam(), visibilityRange(), false, this, true);

			//change direction
			m_eFacingDirection = eFacingDirection;

			//clear new fog
			plot()->changeAdjacentSight(getTeam(), visibilityRange(), true, this, true);

			gDLL->getInterfaceIFace()->setDirty(ColoredPlots_DIRTY_BIT, true);
		} else {
			m_eFacingDirection = eFacingDirection;
		}

		//update formation
		NotifyEntity(NO_MISSION);
	}
}

void CvUnit::rotateFacingDirectionClockwise() {
	//change direction
	DirectionTypes eNewDirection = (DirectionTypes)((m_eFacingDirection + 1) % NUM_DIRECTION_TYPES);
	setFacingDirection(eNewDirection);
}

void CvUnit::rotateFacingDirectionCounterClockwise() {
	//change direction
	DirectionTypes eNewDirection = (DirectionTypes)((m_eFacingDirection + NUM_DIRECTION_TYPES - 1) % NUM_DIRECTION_TYPES);
	setFacingDirection(eNewDirection);
}

int CvUnit::getImmobileTimer() const {
	return m_iImmobileTimer;
}

void CvUnit::setImmobileTimer(int iNewValue) {
	if (iNewValue != getImmobileTimer()) {
		m_iImmobileTimer = iNewValue;

		setInfoBarDirty(true);
	}
}

void CvUnit::changeImmobileTimer(int iChange) {
	if (iChange != 0) {
		setImmobileTimer(std::max(0, getImmobileTimer() + iChange));
	}
}

bool CvUnit::isMadeAttack() const {
	return m_bMadeAttack;
}


void CvUnit::setMadeAttack(bool bNewValue) {
	m_bMadeAttack = bNewValue;
}


bool CvUnit::isMadeInterception() const {
	return m_bMadeInterception;
}


void CvUnit::setMadeInterception(bool bNewValue) {
	m_bMadeInterception = bNewValue;
}


bool CvUnit::isPromotionReady() const {
	return m_bPromotionReady;
}


void CvUnit::setPromotionReady(bool bNewValue) {
	if (isPromotionReady() != bNewValue) {
		m_bPromotionReady = bNewValue;

		gDLL->getEntityIFace()->showPromotionGlow(getUnitEntity(), bNewValue);

		if (m_bPromotionReady) {
			if (isAutoPromoting()) {
				AI_promote();
				setPromotionReady(false);
				testPromotionReady();
			} else {
				getGroup()->setAutomateType(NO_AUTOMATE);
				getGroup()->clearMissionQueue();
				getGroup()->setActivityType(ACTIVITY_AWAKE);
			}
		}

		if (IsSelected()) {
			gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
		}
	}
}


void CvUnit::testPromotionReady() {
	setPromotionReady((getExperience() >= experienceNeeded()) && canAcquirePromotionAny());
}


bool CvUnit::isDelayedDeath() const {
	return m_bDeathDelay;
}


void CvUnit::startDelayedDeath() {
	m_bDeathDelay = true;
}


// Returns true if killed...
bool CvUnit::doDelayedDeath() {
	if (m_bDeathDelay && !isFighting()) {
		kill(false);
		return true;
	}

	return false;
}


bool CvUnit::isCombatFocus() const {
	return m_bCombatFocus;
}


bool CvUnit::isInfoBarDirty() const {
	return m_bInfoBarDirty;
}


void CvUnit::setInfoBarDirty(bool bNewValue) {
	m_bInfoBarDirty = bNewValue;
}

bool CvUnit::isBlockading() const {
	return m_bBlockading;
}

void CvUnit::setBlockading(bool bNewValue) {
	if (bNewValue != isBlockading()) {
		m_bBlockading = bNewValue;

		updatePlunder(isBlockading() ? 1 : -1, true);
	}
}

void CvUnit::collectBlockadeGold() {
	if (plot()->getTeam() == getTeam()) {
		return;
	}

	int iBlockadeRange = GC.getDefineINT("SHIP_BLOCKADE_RANGE");

	for (int i = -iBlockadeRange; i <= iBlockadeRange; ++i) {
		for (int j = -iBlockadeRange; j <= iBlockadeRange; ++j) {
			CvPlot* pLoopPlot = ::plotXY(getX_INLINE(), getY_INLINE(), i, j);

			if (NULL != pLoopPlot && pLoopPlot->isRevealed(getTeam(), false)) {
				CvCity* pCity = pLoopPlot->getPlotCity();

				if (NULL != pCity && !pCity->isPlundered() && isEnemy(pCity->getTeam()) && !atWar(pCity->getTeam(), getTeam())) {
					if (pCity->area() == area() || pCity->plot()->isAdjacentToArea(area())) {
						int iGold = pCity->calculateTradeProfit(pCity) * pCity->getTradeRoutes();
						if (iGold > 0) {
							pCity->setPlundered(true);
							GET_PLAYER(getOwnerINLINE()).changeGold(iGold);
							GET_PLAYER(pCity->getOwnerINLINE()).changeGold(-iGold);

							CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_TRADE_ROUTE_PLUNDERED", getNameKey(), pCity->getNameKey(), iGold);
							gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_BUILD_BANK", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), getX_INLINE(), getY_INLINE());

							szBuffer = gDLL->getText("TXT_KEY_MISC_TRADE_ROUTE_PLUNDER", getNameKey(), pCity->getNameKey(), iGold);
							gDLL->getInterfaceIFace()->addHumanMessage(pCity->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_BUILD_BANK", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pCity->getX_INLINE(), pCity->getY_INLINE());
						}
					}
				}
			}
		}
	}
}


PlayerTypes CvUnit::getOwner() const {
	return getOwnerINLINE();
}

PlayerTypes CvUnit::getVisualOwner(TeamTypes eForTeam) const {
	if (NO_TEAM == eForTeam) {
		eForTeam = GC.getGameINLINE().getActiveTeam();
	}

	if (getTeam() != eForTeam && eForTeam != BARBARIAN_TEAM) {
		if (isHiddenNationality()) {
			if (!plot()->isCity(true, getTeam())) {
				return BARBARIAN_PLAYER;
			}
		}
	}

	return getOwnerINLINE();
}


PlayerTypes CvUnit::getCombatOwner(TeamTypes eForTeam, const CvPlot* pPlot) const {
	if (eForTeam != NO_TEAM && getTeam() != eForTeam && eForTeam != BARBARIAN_TEAM) {
		if (isAlwaysHostile(pPlot)) {
			return BARBARIAN_PLAYER;
		}
	}

	return getOwnerINLINE();
}

TeamTypes CvUnit::getTeam() const {
	return GET_PLAYER(getOwnerINLINE()).getTeam();
}


PlayerTypes CvUnit::getCapturingPlayer() const {
	return m_eCapturingPlayer;
}


void CvUnit::setCapturingPlayer(PlayerTypes eNewValue) {
	m_eCapturingPlayer = eNewValue;
}


const UnitTypes CvUnit::getUnitType() const {
	return m_eUnitType;
}

CvUnitInfo& CvUnit::getUnitInfo() const {
	return *m_pUnitInfo;
}


UnitClassTypes CvUnit::getUnitClassType() const {
	return (UnitClassTypes)m_pUnitInfo->getUnitClassType();
}

const UnitTypes CvUnit::getLeaderUnitType() const {
	return m_eLeaderUnitType;
}

void CvUnit::setLeaderUnitType(UnitTypes leaderUnitType) {
	if (m_eLeaderUnitType != leaderUnitType) {
		m_eLeaderUnitType = leaderUnitType;
		reloadEntity();
	}
}

CvUnit* CvUnit::getCombatUnit() const {
	return getUnit(m_combatUnit);
}


void CvUnit::setCombatUnit(CvUnit* pCombatUnit, bool bAttacking) {
	if (isCombatFocus()) {
		gDLL->getInterfaceIFace()->setCombatFocus(false);
	}

	if (pCombatUnit != NULL) {
		if (bAttacking) {
			if (GC.getLogging()) {
				if (gDLL->getChtLvl() > 0) {
					// Log info about this combat...
					char szOut[1024];
					sprintf(szOut, "*** KOMBAT!\n     ATTACKER: Player %d Unit %d (%S's %S), CombatStrength=%d\n     DEFENDER: Player %d Unit %d (%S's %S), CombatStrength=%d\n",
						getOwnerINLINE(), getID(), GET_PLAYER(getOwnerINLINE()).getName(), getName().GetCString(), currCombatStr(NULL, NULL),
						pCombatUnit->getOwnerINLINE(), pCombatUnit->getID(), GET_PLAYER(pCombatUnit->getOwnerINLINE()).getName(), pCombatUnit->getName().GetCString(), pCombatUnit->currCombatStr(pCombatUnit->plot(), this));
					gDLL->messageControlLog(szOut);
				}
			}

			if (showSeigeTower(pCombatUnit)) // K-Mod
			{
				CvDLLEntity::SetSiegeTower(true);
			}
		}

		FAssertMsg(getCombatUnit() == NULL, "Combat Unit is not expected to be assigned");
		FAssertMsg(!(plot()->isFighting()), "(plot()->isFighting()) did not return false as expected");
		m_bCombatFocus = (bAttacking && !(gDLL->getInterfaceIFace()->isFocusedWidget()) && ((getOwnerINLINE() == GC.getGameINLINE().getActivePlayer()) || ((pCombatUnit->getOwnerINLINE() == GC.getGameINLINE().getActivePlayer()) && !(GC.getGameINLINE().isMPOption(MPOPTION_SIMULTANEOUS_TURNS)))));
		m_combatUnit = pCombatUnit->getIDInfo();
		setCombatFirstStrikes((pCombatUnit->immuneToFirstStrikes()) ? 0 : (firstStrikes() + GC.getGameINLINE().getSorenRandNum(chanceFirstStrikes() + 1, "First Strike")));
	} else {
		if (getCombatUnit() != NULL) {
			FAssertMsg(getCombatUnit() != NULL, "getCombatUnit() is not expected to be equal with NULL");
			FAssertMsg(plot()->isFighting(), "plot()->isFighting is expected to be true");
			m_bCombatFocus = false;
			m_combatUnit.reset();
			setCombatFirstStrikes(0);

			if (IsSelected()) {
				gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
			}

			if (plot() == gDLL->getInterfaceIFace()->getSelectionPlot()) {
				gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
			}

			CvDLLEntity::SetSiegeTower(false);
		}
	}

	setCombatTimer(0);
	setInfoBarDirty(true);

	if (isCombatFocus()) {
		gDLL->getInterfaceIFace()->setCombatFocus(true);
	}
}

// K-Mod. Return true if the combat animation should include a seige tower
// (code copied from setCombatUnit, above)
bool CvUnit::showSeigeTower(CvUnit* pDefender) const {
	return getDomainType() == DOMAIN_LAND
		&& !m_pUnitInfo->isIgnoreBuildingDefense()
		&& pDefender->plot()->getPlotCity()
		&& pDefender->plot()->getPlotCity()->getBuildingDefense() > 0
		&& cityAttackModifier() >= GC.getDefineINT("MIN_CITY_ATTACK_MODIFIER_FOR_SIEGE_TOWER");
}

CvUnit* CvUnit::getTransportUnit() const {
	return getUnit(m_transportUnit);
}


bool CvUnit::isCargo() const {
	return (getTransportUnit() != NULL);
}


void CvUnit::setTransportUnit(CvUnit* pTransportUnit) {
	CvUnit* pOldTransportUnit = getTransportUnit();
	if (pOldTransportUnit != pTransportUnit) {
		if (pOldTransportUnit != NULL) {
			pOldTransportUnit->changeCargo(-1);
		}

		if (pTransportUnit != NULL) {
			FAssertMsg(pTransportUnit->cargoSpaceAvailable(getSpecialUnitType(), getDomainType()) > 0, "Cargo space is expected to be available");

			//joinGroup(NULL, true); // Because what if a group of 3 tries to get in a transport which can hold 2...

			if (getGroup()->getNumUnits() > 1) // we could use > cargoSpace, I suppose. But maybe some quirks of game mechanics rely on this group split.
				joinGroup(NULL, true);
			else {
				getGroup()->clearMissionQueue();
				if (IsSelected())
					gDLL->getInterfaceIFace()->removeFromSelectionList(this);
			}
			FAssert(getGroup()->headMissionQueueNode() == 0); // we don't want them jumping off the boat to complete some unfinished mission!

			m_transportUnit = pTransportUnit->getIDInfo();

			if (getDomainType() != DOMAIN_AIR) {
				getGroup()->setActivityType(ACTIVITY_SLEEP);
			}

			if (GC.getGameINLINE().isFinalInitialized()) {
				finishMoves();
			}

			pTransportUnit->changeCargo(1);
			pTransportUnit->getGroup()->setActivityType(ACTIVITY_AWAKE);
		} else {
			m_transportUnit.reset();

			if (getGroup()->getActivityType() != ACTIVITY_MISSION) // K-Mod. (the unit might be trying to walk somewhere.)
				getGroup()->setActivityType(ACTIVITY_AWAKE);
		}
	}
}


int CvUnit::getExtraDomainModifier(DomainTypes eIndex) const {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < NUM_DOMAIN_TYPES, "eIndex is expected to be within maximum bounds (invalid Index)");
	return m_aiExtraDomainModifier[eIndex];
}


void CvUnit::changeExtraDomainModifier(DomainTypes eIndex, int iChange) {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < NUM_DOMAIN_TYPES, "eIndex is expected to be within maximum bounds (invalid Index)");
	m_aiExtraDomainModifier[eIndex] = (m_aiExtraDomainModifier[eIndex] + iChange);
}


const CvWString CvUnit::getName(uint uiForm) const {
	if (m_szName.empty()) {
		return m_pUnitInfo->getDescription(uiForm);
	}
	CvWString szBuffer;
	szBuffer.Format(L"%s (%s)", m_szName.GetCString(), m_pUnitInfo->getDescription(uiForm));

	return szBuffer;
}


const wchar* CvUnit::getNameKey() const {
	if (m_szName.empty()) {
		return m_pUnitInfo->getTextKeyWide();
	} else {
		return m_szName.GetCString();
	}
}


const CvWString& CvUnit::getNameNoDesc() const {
	return m_szName;
}


void CvUnit::setName(CvWString szNewValue) {
	gDLL->stripSpecialCharacters(szNewValue);

	m_szName = szNewValue;

	if (IsSelected()) {
		gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
	}
}


std::string CvUnit::getScriptData() const {
	return m_szScriptData;
}


void CvUnit::setScriptData(std::string szNewValue) {
	m_szScriptData = szNewValue;
}


int CvUnit::getTerrainDoubleMoveCount(TerrainTypes eIndex) const {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumTerrainInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	return m_paiTerrainDoubleMoveCount[eIndex];
}


bool CvUnit::isTerrainDoubleMove(TerrainTypes eIndex) const {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumTerrainInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	return (getTerrainDoubleMoveCount(eIndex) > 0);
}


void CvUnit::changeTerrainDoubleMoveCount(TerrainTypes eIndex, int iChange) {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumTerrainInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	m_paiTerrainDoubleMoveCount[eIndex] = (m_paiTerrainDoubleMoveCount[eIndex] + iChange);
	FAssert(getTerrainDoubleMoveCount(eIndex) >= 0);
}


int CvUnit::getFeatureDoubleMoveCount(FeatureTypes eIndex) const {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumFeatureInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	return m_paiFeatureDoubleMoveCount[eIndex];
}


bool CvUnit::isFeatureDoubleMove(FeatureTypes eIndex) const {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumFeatureInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	return (getFeatureDoubleMoveCount(eIndex) > 0);
}


void CvUnit::changeFeatureDoubleMoveCount(FeatureTypes eIndex, int iChange) {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumFeatureInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	m_paiFeatureDoubleMoveCount[eIndex] = (m_paiFeatureDoubleMoveCount[eIndex] + iChange);
	FAssert(getFeatureDoubleMoveCount(eIndex) >= 0);
}


int CvUnit::getExtraTerrainAttackPercent(TerrainTypes eIndex) const {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumTerrainInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	return m_paiExtraTerrainAttackPercent[eIndex];
}


void CvUnit::changeExtraTerrainAttackPercent(TerrainTypes eIndex, int iChange) {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumTerrainInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");

	if (iChange != 0) {
		m_paiExtraTerrainAttackPercent[eIndex] += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getExtraTerrainDefensePercent(TerrainTypes eIndex) const {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumTerrainInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	return m_paiExtraTerrainDefensePercent[eIndex];
}


void CvUnit::changeExtraTerrainDefensePercent(TerrainTypes eIndex, int iChange) {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumTerrainInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");

	if (iChange != 0) {
		m_paiExtraTerrainDefensePercent[eIndex] += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getExtraFeatureAttackPercent(FeatureTypes eIndex) const {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumFeatureInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	return m_paiExtraFeatureAttackPercent[eIndex];
}


void CvUnit::changeExtraFeatureAttackPercent(FeatureTypes eIndex, int iChange) {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumFeatureInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");

	if (iChange != 0) {
		m_paiExtraFeatureAttackPercent[eIndex] += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getExtraFeatureDefensePercent(FeatureTypes eIndex) const {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumFeatureInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	return m_paiExtraFeatureDefensePercent[eIndex];
}


void CvUnit::changeExtraFeatureDefensePercent(FeatureTypes eIndex, int iChange) {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumFeatureInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");

	if (iChange != 0) {
		m_paiExtraFeatureDefensePercent[eIndex] += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getExtraUnitCombatModifier(UnitCombatTypes eIndex) const {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumUnitCombatInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	return m_paiExtraUnitCombatModifier[eIndex];
}


void CvUnit::changeExtraUnitCombatModifier(UnitCombatTypes eIndex, int iChange) {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumUnitCombatInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	m_paiExtraUnitCombatModifier[eIndex] = (m_paiExtraUnitCombatModifier[eIndex] + iChange);
}

bool CvUnit::canAcquirePromotion(PromotionTypes ePromotion) const {
	FAssertMsg(ePromotion >= 0, "ePromotion is expected to be non-negative (invalid Index)");
	FAssertMsg(ePromotion < GC.getNumPromotionInfos(), "ePromotion is expected to be within maximum bounds (invalid Index)");

	if (isHasPromotion(ePromotion)) {
		return false;
	}

	const CvPromotionInfo& kPromotion = GC.getPromotionInfo(ePromotion);
	if (kPromotion.getPrereqPromotion() != NO_PROMOTION) {
		if (!isHasPromotion((PromotionTypes)(GC.getPromotionInfo(ePromotion).getPrereqPromotion()))) {
			return false;
		}
	}

	if (kPromotion.getNumPrereqOrPromotions() > 0) {
		bool bValid = false;
		for (int iI = 0; iI < kPromotion.getNumPrereqOrPromotions() && !bValid; iI++) {
			if (isHasPromotion((PromotionTypes)kPromotion.getPrereqOrPromotion(iI))) {
				bValid = true;
			}
		}
		if (!bValid) {
			return false;
		}
	}

	if (kPromotion.getTechPrereq() != NO_TECH) {
		if (!(GET_TEAM(getTeam()).isHasTech((TechTypes)(kPromotion.getTechPrereq())))) {
			return false;
		}
	}

	if (kPromotion.getStateReligionPrereq() != NO_RELIGION) {
		if (GET_PLAYER(getOwnerINLINE()).getStateReligion() != kPromotion.getStateReligionPrereq()) {
			return false;
		}
	}

	if (kPromotion.isCityPrereq()) {
		if (!plot()->isCity(false, getTeam())) {
			return false;
		}
	}

	// Check if this would create negative cargo space or reduce the cargo space below the number of current cargo units
	if (kPromotion.getCargoChange()) {
		int iExistingGroupCargoChange = 0;
		// We need to take into account cargo space we would be removing by replacing a promotion in the same promotion group
		if (isHasPromotionGroup(ePromotion)) {
			int iPromotionGroup = kPromotion.getPromotionGroup();
			for (PromotionTypes eLoopPromotion = (PromotionTypes)0; eLoopPromotion < GC.getNumPromotionInfos(); eLoopPromotion = (PromotionTypes)(eLoopPromotion + 1)) {
				CvPromotionInfo& kLoopPromotion = GC.getPromotionInfo(eLoopPromotion);
				if (kLoopPromotion.getPromotionGroup() == iPromotionGroup && isHasPromotion(eLoopPromotion)) {
					iExistingGroupCargoChange = kLoopPromotion.getCargoChange();
					break;
				}
			}
		}
		// If the net reduction in cargo space is greater than the available space block the promotion
		if (cargoSpace() + kPromotion.getCargoChange() - iExistingGroupCargoChange < getCargo()) {
			return false;
		}
	}

	if (!isPromotionValid(ePromotion)) {
		return false;
	}

	return true;
}

bool CvUnit::isPromotionValid(PromotionTypes ePromotion) const {
	if (!::isPromotionValid(ePromotion, this, true)) {
		return false;
	}

	CvPromotionInfo& promotionInfo = GC.getPromotionInfo(ePromotion);

	if (promotionInfo.getWithdrawalChange() + m_pUnitInfo->getWithdrawalProbability() + getExtraWithdrawal() > GC.getDefineINT("MAX_WITHDRAWAL_PROBABILITY")) {
		return false;
	}

	if (promotionInfo.getInterceptChange() + maxInterceptionProbability() > GC.getDefineINT("MAX_INTERCEPTION_PROBABILITY")) {
		return false;
	}

	if (promotionInfo.getEvasionChange() + evasionProbability() > GC.getDefineINT("MAX_EVASION_PROBABILITY")) {
		return false;
	}

	return true;
}


bool CvUnit::canAcquirePromotionAny() const {
	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++) {
		if (canAcquirePromotion((PromotionTypes)iI)) {
			return true;
		}
	}

	return false;
}


bool CvUnit::isHasPromotion(PromotionTypes eIndex) const {
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumPromotionInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	return m_pabHasPromotion[eIndex];
}


// Moved original functionality to setHasPromotionReal to allow for checking for mutually exclusive promotions
void CvUnit::setHasPromotion(PromotionTypes eIndex, bool bNewValue) {

	// If we are adding the promotion and it is part of a group then we need to remove any existing promotions from the same group
	if (bNewValue && !isHasPromotion(eIndex)) {
		int iPromotionGroup = GC.getPromotionInfo(eIndex).getPromotionGroup();
		if (iPromotionGroup > 0) {
			setGroupPromotionChanged(true);
			for (PromotionTypes ePromotion = (PromotionTypes)0; ePromotion < GC.getNumPromotionInfos(); ePromotion = (PromotionTypes)(ePromotion + 1)) {
				if (GC.getPromotionInfo(ePromotion).getPromotionGroup() == iPromotionGroup) {
					if (isHasPromotion(ePromotion)) {
						setHasPromotionReal(ePromotion, false);
					}
				}
			}
		}
	}

	setHasPromotionReal(eIndex, bNewValue);
}

void CvUnit::setHasPromotionReal(PromotionTypes eIndex, bool bNewValue) {
	if (isHasPromotion(eIndex) != bNewValue) {
		m_pabHasPromotion[eIndex] = bNewValue;

		int iChange = isHasPromotion(eIndex) ? 1 : -1;

		const CvPromotionInfo& kPromotion = GC.getPromotionInfo(eIndex);
		changeBlitzCount(kPromotion.isBlitz() ? iChange : 0);
		changeAmphibCount(kPromotion.isAmphib() ? iChange : 0);
		changeRiverCount(kPromotion.isRiver() ? iChange : 0);
		changeEnemyRouteCount(kPromotion.isEnemyRoute() ? iChange : 0);
		changeAlwaysHealCount(kPromotion.isAlwaysHeal() ? iChange : 0);
		changeHillsDoubleMoveCount(kPromotion.isHillsDoubleMove() ? iChange : 0);
		changeImmuneToFirstStrikesCount(kPromotion.isImmuneToFirstStrikes() ? iChange : 0);
		changeRangeUnboundCount(kPromotion.isUnitRangeUnbound() ? iChange : 0);
		changeTerritoryUnboundCount(kPromotion.isUnitTerritoryUnbound() ? iChange : 0);
		changeCanMovePeaksCount(kPromotion.isCanMovePeaks() ? iChange : 0);
		changeLoyaltyCount(kPromotion.isLoyal() ? iChange : 0);
		changeSpyRadiationCount(kPromotion.isSpyRadiation() ? iChange : 0);

		changeExtraVisibilityRange(kPromotion.getVisibilityChange() * iChange);
		changeExtraMoves(kPromotion.getMovesChange() * iChange);
		changeExtraMoveDiscount(kPromotion.getMoveDiscountChange() * iChange);
		changeExtraAirRange(kPromotion.getAirRangeChange() * iChange);
		changeExtraIntercept(kPromotion.getInterceptChange() * iChange);
		changeExtraEvasion(kPromotion.getEvasionChange() * iChange);
		changeExtraFirstStrikes(kPromotion.getFirstStrikesChange() * iChange);
		changeExtraChanceFirstStrikes(kPromotion.getChanceFirstStrikesChange() * iChange);
		changeExtraWithdrawal(kPromotion.getWithdrawalChange() * iChange);
		changeExtraCollateralDamage(kPromotion.getCollateralDamageChange() * iChange);
		changeExtraBombardRate(kPromotion.getBombardRateChange() * iChange);
		changeExtraEnemyHeal(kPromotion.getEnemyHealChange() * iChange);
		changeExtraNeutralHeal(kPromotion.getNeutralHealChange() * iChange);
		changeExtraFriendlyHeal(kPromotion.getFriendlyHealChange() * iChange);
		changeSameTileHeal(kPromotion.getSameTileHealChange() * iChange);
		changeAdjacentTileHeal(kPromotion.getAdjacentTileHealChange() * iChange);
		changeExtraCombatPercent(kPromotion.getCombatPercent() * iChange);
		changeExtraCityAttackPercent(kPromotion.getCityAttackPercent() * iChange);
		changeExtraCityDefensePercent(kPromotion.getCityDefensePercent() * iChange);
		changeExtraHillsAttackPercent(kPromotion.getHillsAttackPercent() * iChange);
		changeExtraHillsDefensePercent(kPromotion.getHillsDefensePercent() * iChange);
		changeRevoltProtection(kPromotion.getRevoltProtection() * iChange);
		changeCollateralDamageProtection(kPromotion.getCollateralDamageProtection() * iChange);
		changePillageChange(kPromotion.getPillageChange() * iChange);
		changeUpgradeDiscount(kPromotion.getUpgradeDiscount() * iChange);
		changeExperiencePercent(kPromotion.getExperiencePercent() * iChange);
		changeKamikazePercent((kPromotion.getKamikazePercent()) * iChange);
		changeCargoSpace(kPromotion.getCargoChange() * iChange);
		changeExtraRange(kPromotion.getUnitRangeChange() * iChange);
		changeExtraRangeModifier(kPromotion.getUnitRangeModifier() * iChange);
		changeMaxSlaves(kPromotion.getEnslaveCountChange() * iChange);
		changeSpyEvasionChanceExtra(kPromotion.getSpyEvasionChange() * iChange);
		changeSpyPreparationModifier(kPromotion.getSpyPreparationModifier() * iChange);
		changeSpyPoisonChangeExtra(kPromotion.getSpyPoisonModifier() * iChange);
		changeSpyDestroyImprovementChange(kPromotion.getSpyDestroyImprovementChange() * iChange);
		changeSpyDiplomacyPenalty(kPromotion.getSpyDiploPenaltyChange() * iChange);
		changeSpyNukeCityChange(kPromotion.getSpyNukeCityChange() * iChange);
		changeSpySwitchCivicChange(kPromotion.getSpySwitchCivicChange() * iChange);
		changeSpySwitchReligionChange(kPromotion.getSpySwitchReligionChange() * iChange);
		changeSpyDisablePowerChange(kPromotion.getSpyDisablePowerChange() * iChange);
		changeSpyEscapeChanceExtra(kPromotion.getSpyEscapeChange() * iChange);
		changeSpyInterceptChanceExtra(kPromotion.getSpyInterceptChange() * iChange);
		changeSpyUnhappyChange(kPromotion.getSpyUnhappyChange() * iChange);
		changeSpyRevoltChange(kPromotion.getSpyRevoltChange() * iChange);
		changeSpyWarWearinessChange(kPromotion.getSpyWarWearinessChange() * iChange);
		changeSpyReligionRemovalChange(kPromotion.getSpyReligionRemovalChange() * iChange);
		changeSpyCorporationRemovalChange(kPromotion.getSpyCorporationRemovalChange() * iChange);
		changeSpyCultureChange(kPromotion.getSpyCultureChange() * iChange);
		changeSpyResearchSabotageChange(kPromotion.getSpyResearchSabotageChange() * iChange);
		changeSpyDestroyProjectChange(kPromotion.getSpyDestroyProjectChange() * iChange);
		changeSpyDestroyBuildingChange(kPromotion.getSpyDestroyBuildingChange() * iChange);
		changeSpyDestroyProductionChange(kPromotion.getSpyDestroyProductionChange() * iChange);
		changeSpyBuyTechChange(kPromotion.getSpyBuyTechChange() * iChange);
		changeSpyStealTreasuryChange(kPromotion.getSpyStealTreasuryChange() * iChange);
		changeWorkRateModifier(kPromotion.getWorkRateModifier() * iChange);
		changeSalvageModifier(kPromotion.getSalvageModifier() * iChange);

		for (TerrainTypes eTerrain = (TerrainTypes)0; eTerrain < GC.getNumTerrainInfos(); eTerrain = (TerrainTypes)(eTerrain + 1)) {
			changeExtraTerrainAttackPercent(eTerrain, kPromotion.getTerrainAttackPercent(eTerrain) * iChange);
			changeExtraTerrainDefensePercent(eTerrain, kPromotion.getTerrainDefensePercent(eTerrain) * iChange);
			changeTerrainDoubleMoveCount(eTerrain, kPromotion.getTerrainDoubleMove(eTerrain) ? iChange : 0);
		}

		for (FeatureTypes eFeature = (FeatureTypes)0; eFeature < GC.getNumFeatureInfos(); eFeature = (FeatureTypes)(eFeature + 1)) {
			changeExtraFeatureAttackPercent(eFeature, kPromotion.getFeatureAttackPercent(eFeature) * iChange);
			changeExtraFeatureDefensePercent(eFeature, kPromotion.getFeatureDefensePercent(eFeature) * iChange);
			changeFeatureDoubleMoveCount(eFeature, kPromotion.getFeatureDoubleMove(eFeature) ? iChange : 0);
		}

		for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
			changeExtraUnitCombatModifier(eUnitCombat, kPromotion.getUnitCombatModifierPercent(eUnitCombat) * iChange);
		}

		for (DomainTypes eDomain = (DomainTypes)0; eDomain < NUM_DOMAIN_TYPES; eDomain = (DomainTypes)(eDomain + 1)) {
			changeExtraDomainModifier(eDomain, kPromotion.getDomainModifierPercent(eDomain) * iChange);
		}

		for (int i = 0; i < kPromotion.getNumBuildLeaveFeatures(); i++) {
			std::pair<int, int> pPair = kPromotion.getBuildLeaveFeature(i);
			changeBuildLeaveFeatureCount((BuildTypes)pPair.first, (FeatureTypes)pPair.second, iChange);
		}

		std::vector<InvisibleTypes> vInvisibilityTypes;
		for (int i = 0; i < kPromotion.getNumSeeInvisibleTypes(); i++) {
			changeSeeInvisibleCount((InvisibleTypes)kPromotion.getSeeInvisibleType(i), iChange);
			vInvisibilityTypes.push_back((InvisibleTypes)kPromotion.getSeeInvisibleType(i));
		}
		if (vInvisibilityTypes.size() > 0) {
			plot()->changeAdjacentSight(getTeam(), visibilityRange(), (iChange == 1), this, true, vInvisibilityTypes);
		}
		vInvisibilityTypes.clear();

		if (IsSelected()) {
			gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
			gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
		}

		//update graphics
		gDLL->getEntityIFace()->updatePromotionLayers(getUnitEntity());
	}
}


int CvUnit::getSubUnitCount() const {
	return m_pCustomUnitMeshGroup ? m_pCustomUnitMeshGroup->getGroupSize() : m_pUnitInfo->getGroupSize();
}


int CvUnit::getSubUnitsAlive() const {
	return getSubUnitsAlive(getDamage());
}


int CvUnit::getSubUnitsAlive(int iDamage) const {
	if (iDamage >= maxHitPoints()) {
		return 0;
	} else {
		return std::max(1, (((getGroupSize() * (maxHitPoints() - iDamage)) + (maxHitPoints() / ((getGroupSize() * 2) + 1))) / maxHitPoints()));
	}
}
// returns true if unit can initiate a war action with plot (possibly by declaring war)
bool CvUnit::potentialWarAction(const CvPlot* pPlot) const {
	TeamTypes ePlotTeam = pPlot->getTeam();
	TeamTypes eUnitTeam = getTeam();

	if (ePlotTeam == NO_TEAM) {
		return false;
	}

	if (isEnemy(ePlotTeam, pPlot)) {
		return true;
	}

	if (getGroup()->AI_isDeclareWar(pPlot) && GET_TEAM(eUnitTeam).AI_getWarPlan(ePlotTeam) != NO_WARPLAN) {
		return true;
	}

	return false;
}

void CvUnit::read(FDataStreamBase* pStream) {
	// Init data before load
	reset();

	uint uiFlag = 0;
	pStream->Read(&uiFlag);	// flags for expansion

	pStream->Read(&m_iID);
	pStream->Read(&m_iGroupID);
	pStream->Read(&m_iHotKeyNumber);
	pStream->Read(&m_iX);
	pStream->Read(&m_iY);
	pStream->Read(&m_iLastMoveTurn);
	pStream->Read(&m_iReconX);
	pStream->Read(&m_iReconY);
	pStream->Read(&m_iGameTurnCreated);
	pStream->Read(&m_iDamage);
	pStream->Read(&m_iMoves);
	pStream->Read(&m_iExperience);
	pStream->Read(&m_iLevel);
	pStream->Read(&m_iCargo);
	pStream->Read(&m_iCargoCapacity);
	pStream->Read(&m_iAttackPlotX);
	pStream->Read(&m_iAttackPlotY);
	pStream->Read(&m_iCombatTimer);
	pStream->Read(&m_iCombatFirstStrikes);
	if (uiFlag < 2) {
		int iCombatDamage;
		pStream->Read(&iCombatDamage);
	}
	pStream->Read(&m_iFortifyTurns);
	pStream->Read(&m_iBlitzCount);
	pStream->Read(&m_iAmphibCount);
	pStream->Read(&m_iRiverCount);
	pStream->Read(&m_iEnemyRouteCount);
	pStream->Read(&m_iAlwaysHealCount);
	pStream->Read(&m_iHillsDoubleMoveCount);
	pStream->Read(&m_iImmuneToFirstStrikesCount);
	pStream->Read(&m_iExtraVisibilityRange);
	pStream->Read(&m_iExtraMoves);
	pStream->Read(&m_iExtraMoveDiscount);
	pStream->Read(&m_iExtraAirRange);
	pStream->Read(&m_iExtraIntercept);
	pStream->Read(&m_iExtraEvasion);
	pStream->Read(&m_iExtraFirstStrikes);
	pStream->Read(&m_iExtraChanceFirstStrikes);
	pStream->Read(&m_iExtraWithdrawal);
	pStream->Read(&m_iExtraCollateralDamage);
	pStream->Read(&m_iExtraBombardRate);
	pStream->Read(&m_iExtraEnemyHeal);
	pStream->Read(&m_iExtraNeutralHeal);
	pStream->Read(&m_iExtraFriendlyHeal);
	pStream->Read(&m_iSameTileHeal);
	pStream->Read(&m_iAdjacentTileHeal);
	pStream->Read(&m_iExtraCombatPercent);
	pStream->Read(&m_iExtraCityAttackPercent);
	pStream->Read(&m_iExtraCityDefensePercent);
	pStream->Read(&m_iExtraHillsAttackPercent);
	pStream->Read(&m_iExtraHillsDefensePercent);
	pStream->Read(&m_iRevoltProtection);
	pStream->Read(&m_iCollateralDamageProtection);
	pStream->Read(&m_iPillageChange);
	pStream->Read(&m_iUpgradeDiscount);
	pStream->Read(&m_iExperiencePercent);
	pStream->Read(&m_iKamikazePercent);
	pStream->Read(&m_iBaseCombat);
	pStream->Read((int*)&m_eFacingDirection);
	pStream->Read(&m_iImmobileTimer);
	pStream->Read(&m_iExtraRange);
	pStream->Read(&m_iExtraRangeModifier);
	pStream->Read(&m_iRangeUnboundCount);
	pStream->Read(&m_iTerritoryUnboundCount);
	pStream->Read(&m_iCanMovePeaksCount);
	pStream->Read(&m_iMaxSlaves);
	pStream->Read(&m_iSlaveSpecialistType);
	pStream->Read(&m_iSlaveControlCount);
	pStream->Read(&m_iLoyaltyCount);
	pStream->Read(&m_iWorkRateModifier);
	pStream->Read(&m_iSalvageModifier);

	pStream->Read(&m_bMadeAttack);
	pStream->Read(&m_bMadeInterception);
	pStream->Read(&m_bPromotionReady);
	pStream->Read(&m_bDeathDelay);
	pStream->Read(&m_bCombatFocus);
	// m_bInfoBarDirty not saved...
	pStream->Read(&m_bBlockading);
	if (uiFlag > 0) {
		pStream->Read(&m_bAirCombat);
	}
	pStream->Read(&m_bCivicEnabled);
	pStream->Read(&m_bGroupPromotionChanged);
	pStream->Read(&m_bWorldViewEnabled);
	pStream->Read(&m_bAutoPromoting);
	pStream->Read(&m_bAutoUpgrading);
	pStream->Read(&m_bImmobile);
	pStream->Read(&m_bFixedAI);
	pStream->Read(&m_bHiddenNationality);
	pStream->Read(&m_bAlwaysHostile);

	pStream->Read((int*)&m_eOwner);
	pStream->Read((int*)&m_eCapturingPlayer);
	pStream->Read((int*)&m_eUnitType);
	FAssert(NO_UNIT != m_eUnitType);
	m_pUnitInfo = (NO_UNIT != m_eUnitType) ? &GC.getUnitInfo(m_eUnitType) : NULL;
	pStream->Read((int*)&m_eLeaderUnitType);
	pStream->Read((int*)&m_eDesiredDiscoveryTech);
	pStream->Read((int*)&m_eUnitCombatType);
	pStream->Read((int*)&m_eInvisible);
	pStream->Read((int*)&m_eWeaponType);
	m_iWeaponStrength = m_eWeaponType != NO_WEAPON ? GC.getWeaponInfo(m_eWeaponType).getStrength() : 0;
	pStream->Read((int*)&m_eAmmunitionType);
	m_iAmmunitionStrength = m_eAmmunitionType != NO_WEAPON ? GC.getWeaponInfo(m_eAmmunitionType).getStrength() : 0;

	pStream->Read((int*)&m_combatUnit.eOwner);
	pStream->Read(&m_combatUnit.iID);
	pStream->Read((int*)&m_transportUnit.eOwner);
	pStream->Read(&m_transportUnit.iID);
	pStream->Read((int*)&m_homeCity.eOwner);
	pStream->Read(&m_homeCity.iID);
	pStream->Read((int*)&m_shadowUnit.eOwner);
	pStream->Read(&m_shadowUnit.iID);

	pStream->Read(NUM_DOMAIN_TYPES, m_aiExtraDomainModifier);

	pStream->ReadString(m_szName);
	pStream->ReadString(m_szScriptData);

	pStream->Read(GC.getNumPromotionInfos(), m_pabHasPromotion);

	pStream->Read(GC.getNumTerrainInfos(), m_paiTerrainDoubleMoveCount);
	pStream->Read(GC.getNumFeatureInfos(), m_paiFeatureDoubleMoveCount);
	pStream->Read(GC.getNumTerrainInfos(), m_paiExtraTerrainAttackPercent);
	pStream->Read(GC.getNumTerrainInfos(), m_paiExtraTerrainDefensePercent);
	pStream->Read(GC.getNumFeatureInfos(), m_paiExtraFeatureAttackPercent);
	pStream->Read(GC.getNumFeatureInfos(), m_paiExtraFeatureDefensePercent);
	pStream->Read(GC.getNumUnitCombatInfos(), m_paiExtraUnitCombatModifier);
	pStream->Read(GC.getNumSpecialistInfos(), m_paiEnslavedCount);
	pStream->Read(GC.getNumInvisibleInfos(), m_paiSeeInvisibleCount);

	int iOuterMapCount;
	pStream->Read(&iOuterMapCount);
	for (int i = 0; i < iOuterMapCount; i++) {
		BuildTypes eBuild;
		pStream->Read((int*)&eBuild);

		std::map <FeatureTypes, int> mFeatureCount;
		int iInnerMapCount;
		pStream->Read(&iInnerMapCount);
		for (int j = 0; j < iInnerMapCount; j++) {
			FeatureTypes eFeature;
			pStream->Read((int*)&eFeature);
			int iCount;
			pStream->Read(&iCount);
			mFeatureCount[eFeature] = iCount;
		}
		m_mmBuildLeavesFeatures[eBuild] = mFeatureCount;
	}

	m_pSpy = (m_pUnitInfo && m_pUnitInfo->isSpy()) ? m_pSpy = new CvSpy : NULL;
	if (m_pSpy) m_pSpy->read(pStream);

	if (isSlaver())
		setSlaverGraphics();

	if (isBarbarianConvert())
		setBarbarianGraphics();
}


void CvUnit::write(FDataStreamBase* pStream) {
	uint uiFlag = 3;
	pStream->Write(uiFlag);		// flag for expansion

	pStream->Write(m_iID);
	pStream->Write(m_iGroupID);
	pStream->Write(m_iHotKeyNumber);
	pStream->Write(m_iX);
	pStream->Write(m_iY);
	pStream->Write(m_iLastMoveTurn);
	pStream->Write(m_iReconX);
	pStream->Write(m_iReconY);
	pStream->Write(m_iGameTurnCreated);
	pStream->Write(m_iDamage);
	pStream->Write(m_iMoves);
	pStream->Write(m_iExperience);
	pStream->Write(m_iLevel);
	pStream->Write(m_iCargo);
	pStream->Write(m_iCargoCapacity);
	pStream->Write(m_iAttackPlotX);
	pStream->Write(m_iAttackPlotY);
	pStream->Write(m_iCombatTimer);
	pStream->Write(m_iCombatFirstStrikes);
	pStream->Write(m_iFortifyTurns);
	pStream->Write(m_iBlitzCount);
	pStream->Write(m_iAmphibCount);
	pStream->Write(m_iRiverCount);
	pStream->Write(m_iEnemyRouteCount);
	pStream->Write(m_iAlwaysHealCount);
	pStream->Write(m_iHillsDoubleMoveCount);
	pStream->Write(m_iImmuneToFirstStrikesCount);
	pStream->Write(m_iExtraVisibilityRange);
	pStream->Write(m_iExtraMoves);
	pStream->Write(m_iExtraMoveDiscount);
	pStream->Write(m_iExtraAirRange);
	pStream->Write(m_iExtraIntercept);
	pStream->Write(m_iExtraEvasion);
	pStream->Write(m_iExtraFirstStrikes);
	pStream->Write(m_iExtraChanceFirstStrikes);
	pStream->Write(m_iExtraWithdrawal);
	pStream->Write(m_iExtraCollateralDamage);
	pStream->Write(m_iExtraBombardRate);
	pStream->Write(m_iExtraEnemyHeal);
	pStream->Write(m_iExtraNeutralHeal);
	pStream->Write(m_iExtraFriendlyHeal);
	pStream->Write(m_iSameTileHeal);
	pStream->Write(m_iAdjacentTileHeal);
	pStream->Write(m_iExtraCombatPercent);
	pStream->Write(m_iExtraCityAttackPercent);
	pStream->Write(m_iExtraCityDefensePercent);
	pStream->Write(m_iExtraHillsAttackPercent);
	pStream->Write(m_iExtraHillsDefensePercent);
	pStream->Write(m_iRevoltProtection);
	pStream->Write(m_iCollateralDamageProtection);
	pStream->Write(m_iPillageChange);
	pStream->Write(m_iUpgradeDiscount);
	pStream->Write(m_iExperiencePercent);
	pStream->Write(m_iKamikazePercent);
	pStream->Write(m_iBaseCombat);
	pStream->Write(m_eFacingDirection);
	pStream->Write(m_iImmobileTimer);
	pStream->Write(m_iExtraRange);
	pStream->Write(m_iExtraRangeModifier);
	pStream->Write(m_iRangeUnboundCount);
	pStream->Write(m_iTerritoryUnboundCount);
	pStream->Write(m_iCanMovePeaksCount);
	pStream->Write(m_iMaxSlaves);
	pStream->Write(m_iSlaveSpecialistType);
	pStream->Write(m_iSlaveControlCount);
	pStream->Write(m_iLoyaltyCount);
	pStream->Write(m_iWorkRateModifier);
	pStream->Write(m_iSalvageModifier);

	pStream->Write(m_bMadeAttack);
	pStream->Write(m_bMadeInterception);
	pStream->Write(m_bPromotionReady);
	pStream->Write(m_bDeathDelay);
	pStream->Write(m_bCombatFocus);
	// m_bInfoBarDirty not saved...
	pStream->Write(m_bBlockading);
	pStream->Write(m_bAirCombat);
	pStream->Write(m_bCivicEnabled);
	pStream->Write(m_bGroupPromotionChanged);
	pStream->Write(m_bWorldViewEnabled);
	pStream->Write(m_bAutoPromoting);
	pStream->Write(m_bAutoUpgrading);
	pStream->Write(m_bImmobile);
	pStream->Write(m_bFixedAI);
	pStream->Write(m_bHiddenNationality);
	pStream->Write(m_bAlwaysHostile);

	pStream->Write(m_eOwner);
	pStream->Write(m_eCapturingPlayer);
	pStream->Write(m_eUnitType);
	pStream->Write(m_eLeaderUnitType);
	pStream->Write(m_eDesiredDiscoveryTech);
	pStream->Write(m_eUnitCombatType);
	pStream->Write(m_eInvisible);
	pStream->Write(m_eWeaponType);
	pStream->Write(m_eAmmunitionType);

	pStream->Write(m_combatUnit.eOwner);
	pStream->Write(m_combatUnit.iID);
	pStream->Write(m_transportUnit.eOwner);
	pStream->Write(m_transportUnit.iID);
	pStream->Write(m_homeCity.eOwner);
	pStream->Write(m_homeCity.iID);
	pStream->Write(m_shadowUnit.eOwner);
	pStream->Write(m_shadowUnit.iID);

	pStream->Write(NUM_DOMAIN_TYPES, m_aiExtraDomainModifier);

	pStream->WriteString(m_szName);
	pStream->WriteString(m_szScriptData);

	pStream->Write(GC.getNumPromotionInfos(), m_pabHasPromotion);

	pStream->Write(GC.getNumTerrainInfos(), m_paiTerrainDoubleMoveCount);
	pStream->Write(GC.getNumFeatureInfos(), m_paiFeatureDoubleMoveCount);
	pStream->Write(GC.getNumTerrainInfos(), m_paiExtraTerrainAttackPercent);
	pStream->Write(GC.getNumTerrainInfos(), m_paiExtraTerrainDefensePercent);
	pStream->Write(GC.getNumFeatureInfos(), m_paiExtraFeatureAttackPercent);
	pStream->Write(GC.getNumFeatureInfos(), m_paiExtraFeatureDefensePercent);
	pStream->Write(GC.getNumUnitCombatInfos(), m_paiExtraUnitCombatModifier);
	pStream->Write(GC.getNumSpecialistInfos(), m_paiEnslavedCount);
	pStream->Write(GC.getNumInvisibleInfos(), m_paiSeeInvisibleCount);

	pStream->Write(m_mmBuildLeavesFeatures.size());
	for (std::map<BuildTypes, std::map< FeatureTypes, int> >::iterator itB = m_mmBuildLeavesFeatures.begin(); itB != m_mmBuildLeavesFeatures.end(); itB++) {
		pStream->Write(itB->first);
		pStream->Write((itB->second).size());
		for (std::map< FeatureTypes, int>::iterator itF = (itB->second).begin(); itF != (itB->second).end(); itF++) {
			pStream->Write(itF->first);
			pStream->Write(itF->second);
		}
	}

	if (m_pSpy) m_pSpy->write(pStream);

}

// Protected Functions...

bool CvUnit::canAdvance(const CvPlot* pPlot, int iThreshold) const {
	FAssert(canFight());
	FAssert(!(isAnimal() && pPlot->isCity()));
	FAssert(getDomainType() != DOMAIN_AIR);
	FAssert(getDomainType() != DOMAIN_IMMOBILE);

	if (pPlot->getNumVisibleEnemyDefenders(this) > iThreshold) {
		return false;
	}

	if (isNoCapture()) {
		if (pPlot->isEnemyCity(*this)) {
			return false;
		}
	}

	return true;
}

// K-Mod, I've rewritten this function just to make it a bit easier to understand, a bit more efficient, and a bit more robust.
// For example, the original code used a std::map<CvUnit*, int>; if the random number in the map turned out to be the same, it could potentially have led to OOS.
// The actual game mechanics are only very slightly changed. (I've removed the targets cap of "visible units - 1". That seemed like a silly limitation.)
void CvUnit::collateralCombat(const CvPlot* pPlot, CvUnit* pSkipUnit) {
	std::vector<std::pair<int, IDInfo> > targetUnits;

	int iCollateralStrength = (getDomainType() == DOMAIN_AIR ? airBaseCombatStr() : baseCombatStr()) * collateralDamage() / 100;

	if (iCollateralStrength == 0 && getExtraCollateralDamage() == 0)
		return;

	CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();
	while (pUnitNode != NULL) {
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pPlot->nextUnitNode(pUnitNode);

		if (pLoopUnit != pSkipUnit && isEnemy(pLoopUnit->getTeam(), pPlot) && pLoopUnit->canDefend() && !pLoopUnit->isInvisible(getTeam(), false)) {
			// This value thing is a bit bork. It's directly from the original code...
			int iValue = (1 + GC.getGameINLINE().getSorenRandNum(10000, "Collateral Damage"));
			iValue *= pLoopUnit->currHitPoints();

			targetUnits.push_back(std::make_pair(iValue, pLoopUnit->getIDInfo()));
		}
	}

	CvCity* pCity = NULL;
	if (getDomainType() == DOMAIN_AIR) {
		pCity = pPlot->getPlotCity();
	}

	int iPossibleTargets = std::min((int)targetUnits.size(), collateralDamageMaxUnits());
	std::partial_sort(targetUnits.begin(), targetUnits.begin() + iPossibleTargets, targetUnits.end(), std::greater<std::pair<int, IDInfo> >());

	int iDamageCount = 0;

	for (int i = 0; i < iPossibleTargets; i++) {
		CvUnit* pTargetUnit = ::getUnit(targetUnits[i].second);
		FAssert(pTargetUnit);

		if (NO_UNITCOMBAT == getUnitCombatType() || !pTargetUnit->getUnitInfo().getUnitCombatCollateralImmune(getUnitCombatType())) {
			int iTheirStrength = pTargetUnit->baseCombatStr();

			int iStrengthFactor = ((iCollateralStrength + iTheirStrength + 1) / 2);

			int iCollateralDamage = (GC.getDefineINT("COLLATERAL_COMBAT_DAMAGE") * (iCollateralStrength + iStrengthFactor)) / (iTheirStrength + iStrengthFactor);

			iCollateralDamage *= 100 + getExtraCollateralDamage();

			iCollateralDamage *= std::max(0, 100 - pTargetUnit->getCollateralDamageProtection());
			iCollateralDamage /= 100;

			if (pCity != NULL) {
				iCollateralDamage *= 100 + pCity->getAirModifier();
				iCollateralDamage /= 100;
			}

			iCollateralDamage = std::max(0, iCollateralDamage / 100);

			int iMaxDamage = std::min(collateralDamageLimit(), (collateralDamageLimit() * (iCollateralStrength + iStrengthFactor)) / (iTheirStrength + iStrengthFactor));
			int iUnitDamage = std::max(pTargetUnit->getDamage(), std::min(pTargetUnit->getDamage() + iCollateralDamage, iMaxDamage));

			if (pTargetUnit->getDamage() != iUnitDamage) {
				pTargetUnit->setDamage(iUnitDamage, getOwnerINLINE());
				iDamageCount++;
			}
		}
	}

	if (iDamageCount > 0) {
		CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_SUFFER_COL_DMG", iDamageCount);
		gDLL->getInterfaceIFace()->addHumanMessage(pSkipUnit->getOwnerINLINE(), (pSkipUnit->getDomainType() != DOMAIN_AIR), GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_COLLATERAL", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pSkipUnit->getX_INLINE(), pSkipUnit->getY_INLINE(), true, true);

		szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_INFLICT_COL_DMG", getNameKey(), iDamageCount);
		gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_COLLATERAL", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pSkipUnit->getX_INLINE(), pSkipUnit->getY_INLINE());
	}
}


void CvUnit::flankingStrikeCombat(const CvPlot* pPlot, int iAttackerStrength, int iAttackerFirepower, int iDefenderOdds, int iDefenderDamage, CvUnit* pSkipUnit) {
	if (pPlot->isCity(true, pSkipUnit->getTeam())) {
		return;
	}

	CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();

	std::vector< std::pair<CvUnit*, int> > listFlankedUnits;
	while (NULL != pUnitNode) {
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pPlot->nextUnitNode(pUnitNode);

		if (pLoopUnit != pSkipUnit) {
			if (!pLoopUnit->isDead() && isEnemy(pLoopUnit->getTeam(), pPlot)) {
				if (!(pLoopUnit->isInvisible(getTeam(), false))) {
					if (pLoopUnit->canDefend()) {
						int iFlankingStrength = m_pUnitInfo->getFlankingStrikeUnitClass(pLoopUnit->getUnitClassType());

						if (iFlankingStrength > 0) {
							int iFlankedDefenderStrength;
							int iFlankedDefenderOdds;
							int iAttackerDamage;
							int iFlankedDefenderDamage;

							getDefenderCombatValues(*pLoopUnit, pPlot, iAttackerStrength, iAttackerFirepower, iFlankedDefenderOdds, iFlankedDefenderStrength, iAttackerDamage, iFlankedDefenderDamage);

							if (GC.getGameINLINE().getSorenRandNum(GC.getCOMBAT_DIE_SIDES(), "Flanking Combat") >= iDefenderOdds) {
								int iCollateralDamage = (iFlankingStrength * iDefenderDamage) / 100;
								int iUnitDamage = std::max(pLoopUnit->getDamage(), std::min(pLoopUnit->getDamage() + iCollateralDamage, collateralDamageLimit()));

								if (pLoopUnit->getDamage() != iUnitDamage) {
									listFlankedUnits.push_back(std::make_pair(pLoopUnit, iUnitDamage));
								}
							}
						}
					}
				}
			}
		}
	}

	int iNumUnitsHit = std::min((int)listFlankedUnits.size(), collateralDamageMaxUnits());

	for (int i = 0; i < iNumUnitsHit; ++i) {
		int iIndexHit = GC.getGameINLINE().getSorenRandNum(listFlankedUnits.size(), "Pick Flanked Unit");
		CvUnit* pUnit = listFlankedUnits[iIndexHit].first;
		int iDamage = listFlankedUnits[iIndexHit].second;
		pUnit->setDamage(iDamage, getOwnerINLINE());
		if (pUnit->isDead()) {
			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_KILLED_UNIT_BY_FLANKING", getNameKey(), pUnit->getNameKey(), pUnit->getVisualCivAdjective(getTeam()));
			gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getEraInfo(GC.getGameINLINE().getCurrentEra()).getAudioUnitVictoryScript(), MESSAGE_TYPE_INFO, NULL, (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE());
			szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_UNIT_DIED_BY_FLANKING", pUnit->getNameKey(), getNameKey(), getVisualCivAdjective(pUnit->getTeam()));
			gDLL->getInterfaceIFace()->addHumanMessage(pUnit->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getEraInfo(GC.getGameINLINE().getCurrentEra()).getAudioUnitDefeatScript(), MESSAGE_TYPE_INFO, NULL, (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE());

			pUnit->kill(false);
		}

		listFlankedUnits.erase(std::remove(listFlankedUnits.begin(), listFlankedUnits.end(), listFlankedUnits[iIndexHit]));
	}

	if (iNumUnitsHit > 0) {
		CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_DAMAGED_UNITS_BY_FLANKING", getNameKey(), iNumUnitsHit);
		gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getEraInfo(GC.getGameINLINE().getCurrentEra()).getAudioUnitVictoryScript(), MESSAGE_TYPE_INFO, NULL, (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE());

		if (NULL != pSkipUnit) {
			szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_UNITS_DAMAGED_BY_FLANKING", getNameKey(), iNumUnitsHit);
			gDLL->getInterfaceIFace()->addHumanMessage(pSkipUnit->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getEraInfo(GC.getGameINLINE().getCurrentEra()).getAudioUnitDefeatScript(), MESSAGE_TYPE_INFO, NULL, (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE());
		}
	}
}


// Returns true if we were intercepted...
bool CvUnit::interceptTest(const CvPlot* pPlot) {
	if (GC.getGameINLINE().getSorenRandNum(100, "Evasion Rand") >= evasionProbability()) {
		CvUnit* pInterceptor = bestInterceptor(pPlot);

		if (pInterceptor != NULL) {
			if (GC.getGameINLINE().getSorenRandNum(100, "Intercept Rand (Air)") < pInterceptor->currInterceptionProbability()) {
				fightInterceptor(pPlot, false);

				return true;
			}
		}
	}

	return false;
}


CvUnit* CvUnit::airStrikeTarget(const CvPlot* pPlot) const {
	CvUnit* pDefender = pPlot->getBestDefender(NO_PLAYER, getOwnerINLINE(), this, true);
	if (pDefender != NULL) {
		if (!pDefender->isDead()) {
			if (pDefender->canDefend()) {
				return pDefender;
			}
		}
	}

	return NULL;
}


bool CvUnit::canAirStrike(const CvPlot* pPlot) const {
	if (getDomainType() != DOMAIN_AIR) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (!canAirAttack()) {
		return false;
	}

	if (pPlot == plot()) {
		return false;
	}

	if (!pPlot->isVisible(getTeam(), false)) {
		return false;
	}

	if (plotDistance(getX_INLINE(), getY_INLINE(), pPlot->getX_INLINE(), pPlot->getY_INLINE()) > airRange()) {
		return false;
	}

	if (airStrikeTarget(pPlot) == NULL) {
		return false;
	}

	return true;
}


bool CvUnit::airStrike(CvPlot* pPlot) {
	if (!canAirStrike(pPlot)) {
		return false;
	}

	if (interceptTest(pPlot)) {
		return false;
	}

	CvUnit* pDefender = airStrikeTarget(pPlot);

	FAssert(pDefender != NULL);
	FAssert(pDefender->canDefend());

	setReconPlot(pPlot);

	setMadeAttack(true);
	changeMoves(GC.getMOVE_DENOMINATOR());

	int iDamage = airCombatDamage(pDefender);

	int iUnitDamage = std::max(pDefender->getDamage(), std::min((pDefender->getDamage() + iDamage), airCombatLimit()));

	CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_ARE_ATTACKED_BY_AIR", pDefender->getNameKey(), getNameKey(), -(((iUnitDamage - pDefender->getDamage()) * 100) / pDefender->maxHitPoints()));
	gDLL->getInterfaceIFace()->addHumanMessage(pDefender->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_AIR_ATTACK", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE(), true, true);

	szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_ATTACK_BY_AIR", getNameKey(), pDefender->getNameKey(), -(((iUnitDamage - pDefender->getDamage()) * 100) / pDefender->maxHitPoints()));
	gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_AIR_ATTACKED", MESSAGE_TYPE_INFO, pDefender->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE());

	collateralCombat(pPlot, pDefender);

	pDefender->setDamage(iUnitDamage, getOwnerINLINE());

	return true;
}

bool CvUnit::canRangeStrike() const {
	if (getDomainType() == DOMAIN_AIR) {
		return false;
	}

	if (!isEnabled()) {
		return false;
	}

	if (airRange() <= 0) {
		return false;
	}

	if (airBaseCombatStr() <= 0) {
		return false;
	}

	if (!canFight()) {
		return false;
	}

	if (isMadeAttack() && !isBlitz()) {
		return false;
	}

	if (!canMove() && getMoves() > 0) {
		return false;
	}

	return true;
}

bool CvUnit::canRangeStrikeAt(const CvPlot* pPlot, int iX, int iY) const {
	if (!canRangeStrike()) {
		return false;
	}

	CvPlot* pTargetPlot = GC.getMapINLINE().plotINLINE(iX, iY);

	if (NULL == pTargetPlot) {
		return false;
	}

	if (!pPlot->isVisible(getTeam(), false)) {
		return false;
	}

	// Need to check target plot too
	if (!pTargetPlot->isVisible(getTeam(), false)) {
		return false;
	}

	if (plotDistance(pPlot->getX_INLINE(), pPlot->getY_INLINE(), pTargetPlot->getX_INLINE(), pTargetPlot->getY_INLINE()) > airRange()) {
		return false;
	}

	CvUnit* pDefender = airStrikeTarget(pTargetPlot);
	if (NULL == pDefender) {
		return false;
	}

	if (!pPlot->canSeePlot(pTargetPlot, getTeam(), airRange(), getFacingDirection(true))) {
		return false;
	}

	return true;
}


bool CvUnit::rangeStrike(int iX, int iY) {
	CvPlot* pPlot = GC.getMapINLINE().plot(iX, iY);
	if (NULL == pPlot) {
		return false;
	}

	if (!canRangeStrikeAt(plot(), iX, iY)) {
		return false;
	}

	CvUnit* pDefender = airStrikeTarget(pPlot);

	FAssert(pDefender != NULL);
	FAssert(pDefender->canDefend());

	if (GC.getDefineINT("RANGED_ATTACKS_USE_MOVES") == 0) {
		setMadeAttack(true);
	}
	changeMoves(GC.getMOVE_DENOMINATOR());

	int iDamage = rangeCombatDamage(pDefender);

	int iUnitDamage = std::max(pDefender->getDamage(), std::min((pDefender->getDamage() + iDamage), airCombatLimit()));

	CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_ARE_ATTACKED_BY_AIR", pDefender->getNameKey(), getNameKey(), -(((iUnitDamage - pDefender->getDamage()) * 100) / pDefender->maxHitPoints()));
	//red icon over attacking unit
	gDLL->getInterfaceIFace()->addHumanMessage(pDefender->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_COMBAT", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), this->getX_INLINE(), this->getY_INLINE(), true, true);
	//white icon over defending unit
	gDLL->getInterfaceIFace()->addHumanMessage(pDefender->getOwnerINLINE(), false, 0, L"", "AS2D_COMBAT", MESSAGE_TYPE_DISPLAY_ONLY, pDefender->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_WHITE"), pDefender->getX_INLINE(), pDefender->getY_INLINE(), true, true);

	szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_ATTACK_BY_AIR", getNameKey(), pDefender->getNameKey(), -(((iUnitDamage - pDefender->getDamage()) * 100) / pDefender->maxHitPoints()));
	gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_COMBAT", MESSAGE_TYPE_INFO, pDefender->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE());

	collateralCombat(pPlot, pDefender);

	//set damage but don't update entity damage visibility
	pDefender->setDamage(iUnitDamage, getOwnerINLINE(), false);

	if (pPlot->isActiveVisible(false)) {
		// Range strike entity mission
		CvMissionDefinition kDefiniton;
		kDefiniton.setMissionTime(GC.getMissionInfo(MISSION_RANGE_ATTACK).getTime() * gDLL->getSecsPerTurn());
		kDefiniton.setMissionType(MISSION_RANGE_ATTACK);
		kDefiniton.setPlot(pDefender->plot());
		kDefiniton.setUnit(BATTLE_UNIT_ATTACKER, this);
		kDefiniton.setUnit(BATTLE_UNIT_DEFENDER, pDefender);
		gDLL->getEntityIFace()->AddMission(&kDefiniton);

		// mission timer is not used like this in any other part of code, so it might cause OOS
		// issues ... at worst I think unit dies before animation is complete, so no real
		// harm in commenting it out.
	}

	return true;
}

//------------------------------------------------------------------------------------------------
// FUNCTION:    CvUnit::planBattle
//! \brief      Determines in general how a battle will progress.
//!
//!				Note that the outcome of the battle is not determined here. This function plans
//!				how many sub-units die and in which 'rounds' of battle.
//! \param      kBattle The battle definition, which receives the battle plan.
//! \param		combat_log The order and amplitude of damage taken by the units (K-Mod)
//! \retval     The number of game turns that the battle should be given.
//------------------------------------------------------------------------------------------------

// Rewriten for K-Mod!
int CvUnit::planBattle(CvBattleDefinition& kBattle, const std::vector<int>& combat_log_argument) const {
	const int BATTLE_TURNS_SETUP = 4;
	const int BATTLE_TURNS_ENDING = 4;
	const int BATTLE_TURNS_MELEE = 6;
	const int BATTLE_TURNS_RANGED = 6;
	const int BATTLE_TURN_RECHECK = 4;

	CvUnit* pAttackUnit = kBattle.getUnit(BATTLE_UNIT_ATTACKER);
	CvUnit* pDefenceUnit = kBattle.getUnit(BATTLE_UNIT_DEFENDER);

	int iAttackerDamage = kBattle.getDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_BEGIN);
	int iDefenderDamage = kBattle.getDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_BEGIN);

	int iAttackerUnits = pAttackUnit->getSubUnitsAlive(iAttackerDamage);
	int iDefenderUnits = pDefenceUnit->getSubUnitsAlive(iDefenderDamage);
	int iAttackerUnitsKilled = 0;
	int iDefenderUnitsKilled = 0;

	// some hackery to ensure that we don't have to deal with an empty combat log...
	const std::vector<int> dummy_log(1, 0);
	const std::vector<int>& combat_log = combat_log_argument.size() == 0 ? dummy_log : combat_log_argument;
	FAssert(combat_log.size() > 0);
	// now we can just use 'combat_log' without having to worry about it being empty.

	//
	kBattle.setNumRangedRounds(0);
	kBattle.setNumMeleeRounds(0);
	kBattle.setDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_RANGED, iAttackerDamage);
	kBattle.setDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_RANGED, iDefenderDamage);
	kBattle.clearBattleRounds();
	bool bRanged = true;

	const static int iStandardNumRounds = GC.getDefineINT("STANDARD_BATTLE_ANIMATION_ROUNDS", 6);

	int iTotalBattleRounds = (iStandardNumRounds * (int)combat_log.size() * GC.getCOMBAT_DAMAGE() + GC.getMAX_HIT_POINTS()) / (2 * GC.getMAX_HIT_POINTS());

	// Reduce number of rounds if both units have groupSize == 1, because nothing much happens in those battles.
	if (getGroupSize() == 1 && getGroupSize() == 1)
		iTotalBattleRounds = (2 * iTotalBattleRounds + 1) / 3;

	// apparently, there is a hardcoded minimum of 2 rounds. (game will crash if there less than 2 rounds.)
	iTotalBattleRounds = range(iTotalBattleRounds, 2, 10);

	int iBattleRound = 0;
	for (int i = 0; i < (int)combat_log.size(); i++) {
		// The combat animator can't handle rounds where there are more deaths on one side than survivers on the other.
		// therefore, we must sometimes end the round early to avoid violating that rule.
		if (combat_log[i] > 0) {
			iDefenderDamage = std::min(GC.getMAX_HIT_POINTS(), iDefenderDamage + combat_log[i]);
			iDefenderUnitsKilled = std::min(iAttackerUnits - iAttackerUnitsKilled, iDefenderUnits - pDefenceUnit->getSubUnitsAlive(iDefenderDamage));
			bRanged = bRanged && pAttackUnit->isRanged();
			if (bRanged)
				kBattle.addDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_RANGED, combat_log[i]); // I'm not sure if this is actually used for the animation...
		} else if (combat_log[i] < 0) {
			iAttackerDamage = std::min(GC.getMAX_HIT_POINTS(), iAttackerDamage - combat_log[i]);
			iAttackerUnitsKilled = std::min(iDefenderUnits - iDefenderUnitsKilled, iAttackerUnits - pAttackUnit->getSubUnitsAlive(iAttackerDamage));
			bRanged = bRanged && pDefenceUnit->isRanged();
			if (bRanged)
				kBattle.addDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_RANGED, combat_log[i]);
		}

		// Sometimes we may need to end more than one round at a time...
		bool bNextRound = false;
		do {
			bNextRound = iBattleRound < (i + 1)* iTotalBattleRounds / (int)combat_log.size();

			// force the round to end if we are already at the deaths limit for whomever will take the next damage.
			if (!bNextRound && (iDefenderUnitsKilled > 0 || iAttackerUnitsKilled > 0)) {
				if (i + 1 == combat_log.size() ||
					iDefenderUnitsKilled >= iAttackerUnits - iAttackerUnitsKilled && combat_log[i] > 0 ||
					iAttackerUnitsKilled >= iDefenderUnits - iDefenderUnitsKilled && combat_log[i] < 0) {
					bNextRound = true;
				}
			}

			if (bNextRound) {
				if (bRanged)
					kBattle.addNumRangedRounds(1);
				else
					kBattle.addNumMeleeRounds(1);

				//
				CvBattleRound kRound;

				kRound.setRangedRound(bRanged);
				kRound.setWaveSize(computeWaveSize(bRanged, iAttackerUnits, iDefenderUnits));

				iAttackerUnits -= iAttackerUnitsKilled;
				iDefenderUnits -= iDefenderUnitsKilled;

				kRound.setNumAlive(BATTLE_UNIT_ATTACKER, iAttackerUnits);
				kRound.setNumAlive(BATTLE_UNIT_DEFENDER, iDefenderUnits);
				kRound.setNumKilled(BATTLE_UNIT_ATTACKER, iAttackerUnitsKilled);
				kRound.setNumKilled(BATTLE_UNIT_DEFENDER, iDefenderUnitsKilled);
				//

				kBattle.addBattleRound(kRound);

				iBattleRound++;
				// there may be some spillover kills if the round was forced...
				iDefenderUnitsKilled = iDefenderUnits - pDefenceUnit->getSubUnitsAlive(iDefenderDamage);
				iAttackerUnitsKilled = iAttackerUnits - pAttackUnit->getSubUnitsAlive(iAttackerDamage);
				// and if there are, we should increase the total number of rounds so that we can fit them in.
				if (iDefenderUnitsKilled > 0 || iAttackerUnitsKilled > 0)
					iTotalBattleRounds++;
			}
		} while (bNextRound);
	}
	FAssert(iAttackerDamage == kBattle.getDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_END));
	FAssert(iDefenderDamage == kBattle.getDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_END));

	FAssert(kBattle.getNumBattleRounds() >= 2);
	FAssert(verifyRoundsValid(kBattle));

	int extraTime = 0;

	// extra time for seige towers and surrendering leaders.
	if ((pAttackUnit->getLeaderUnitType() != NO_UNIT && pAttackUnit->isDead()) ||
		(pDefenceUnit->getLeaderUnitType() != NO_UNIT && pDefenceUnit->isDead()) ||
		pAttackUnit->showSeigeTower(pDefenceUnit)) {
		extraTime = BATTLE_TURNS_MELEE;
	}

	return BATTLE_TURNS_SETUP + BATTLE_TURNS_ENDING + kBattle.getNumMeleeRounds() * BATTLE_TURNS_MELEE + kBattle.getNumRangedRounds() * BATTLE_TURNS_MELEE + extraTime;
}

//------------------------------------------------------------------------------------------------
// FUNCTION:	CvBattleManager::computeDeadUnits
//! \brief		Computes the number of units dead, for either the ranged or melee portion of combat.
//! \param		kDefinition The battle definition.
//! \param		bRanged true if computing the number of units that die during the ranged portion of combat,
//!					false if computing the number of units that die during the melee portion of combat.
//! \param		iUnit The index of the unit to compute (BATTLE_UNIT_ATTACKER or BATTLE_UNIT_DEFENDER).
//! \retval		The number of units that should die for the given unit in the given portion of combat
//------------------------------------------------------------------------------------------------
int CvUnit::computeUnitsToDie(const CvBattleDefinition& kDefinition, bool bRanged, BattleUnitTypes iUnit) const {
	FAssertMsg(iUnit == BATTLE_UNIT_ATTACKER || iUnit == BATTLE_UNIT_DEFENDER, "Invalid unit index");

	BattleTimeTypes iBeginIndex = bRanged ? BATTLE_TIME_BEGIN : BATTLE_TIME_RANGED;
	BattleTimeTypes iEndIndex = bRanged ? BATTLE_TIME_RANGED : BATTLE_TIME_END;
	return kDefinition.getUnit(iUnit)->getSubUnitsAlive(kDefinition.getDamage(iUnit, iBeginIndex)) -
		kDefinition.getUnit(iUnit)->getSubUnitsAlive(kDefinition.getDamage(iUnit, iEndIndex));
}

//------------------------------------------------------------------------------------------------
// FUNCTION:    CvUnit::verifyRoundsValid
//! \brief      Verifies that all rounds in the battle plan are valid
//! \param      vctBattlePlan The battle plan
//! \retval     true if the battle plan (seems) valid, false otherwise
//------------------------------------------------------------------------------------------------
bool CvUnit::verifyRoundsValid(const CvBattleDefinition& battleDefinition) const {
	for (int i = 0; i < battleDefinition.getNumBattleRounds(); i++) {
		if (!battleDefinition.getBattleRound(i).isValid())
			return false;
	}
	return true;
}

//------------------------------------------------------------------------------------------------
// FUNCTION:    CvUnit::increaseBattleRounds
//! \brief      Increases the number of rounds in the battle.
//! \param      kBattleDefinition The definition of the battle
//------------------------------------------------------------------------------------------------
void CvUnit::increaseBattleRounds(CvBattleDefinition& kBattleDefinition) const {
	if (kBattleDefinition.getUnit(BATTLE_UNIT_ATTACKER)->isRanged() && kBattleDefinition.getUnit(BATTLE_UNIT_DEFENDER)->isRanged()) {
		kBattleDefinition.addNumRangedRounds(1);
	} else {
		kBattleDefinition.addNumMeleeRounds(1);
	}
}

//------------------------------------------------------------------------------------------------
// FUNCTION:    CvUnit::computeWaveSize
//! \brief      Computes the wave size for the round.
//! \param      bRangedRound true if the round is a ranged round
//! \param		iAttackerMax The maximum number of attackers that can participate in a wave (alive)
//! \param		iDefenderMax The maximum number of Defenders that can participate in a wave (alive)
//! \retval     The desired wave size for the given parameters
//------------------------------------------------------------------------------------------------
int CvUnit::computeWaveSize(bool bRangedRound, int iAttackerMax, int iDefenderMax) const {
	FAssertMsg(getCombatUnit() != NULL, "You must be fighting somebody!");
	int aiDesiredSize[BATTLE_UNIT_COUNT];
	if (bRangedRound) {
		aiDesiredSize[BATTLE_UNIT_ATTACKER] = getRangedWaveSize();
		aiDesiredSize[BATTLE_UNIT_DEFENDER] = getCombatUnit()->getRangedWaveSize();
	} else {
		aiDesiredSize[BATTLE_UNIT_ATTACKER] = getMeleeWaveSize();
		aiDesiredSize[BATTLE_UNIT_DEFENDER] = getCombatUnit()->getMeleeWaveSize();
	}

	aiDesiredSize[BATTLE_UNIT_DEFENDER] = aiDesiredSize[BATTLE_UNIT_DEFENDER] <= 0 ? iDefenderMax : aiDesiredSize[BATTLE_UNIT_DEFENDER];
	aiDesiredSize[BATTLE_UNIT_ATTACKER] = aiDesiredSize[BATTLE_UNIT_ATTACKER] <= 0 ? iDefenderMax : aiDesiredSize[BATTLE_UNIT_ATTACKER];
	return std::min(std::min(aiDesiredSize[BATTLE_UNIT_ATTACKER], iAttackerMax), std::min(aiDesiredSize[BATTLE_UNIT_DEFENDER],
		iDefenderMax));
}

bool CvUnit::isTargetOf(const CvUnit& attacker) const {
	CvUnitInfo& attackerInfo = attacker.getUnitInfo();
	CvUnitInfo& ourInfo = getUnitInfo();

	if (!plot()->isCity(true, getTeam())) {
		if (NO_UNITCLASS != getUnitClassType() && attackerInfo.getTargetUnitClass(getUnitClassType())) {
			return true;
		}

		if (NO_UNITCOMBAT != getUnitCombatType() && attackerInfo.getTargetUnitCombat(getUnitCombatType())) {
			return true;
		}
	}

	if (NO_UNITCLASS != attackerInfo.getUnitClassType() && ourInfo.getDefenderUnitClass(attackerInfo.getUnitClassType())) {
		return true;
	}

	if (NO_UNITCOMBAT != attackerInfo.getUnitCombatType() && ourInfo.getDefenderUnitCombat(attackerInfo.getUnitCombatType())) {
		return true;
	}

	return false;
}

bool CvUnit::isEnemy(TeamTypes eTeam, const CvPlot* pPlot) const {
	if (NULL == pPlot) {
		pPlot = plot();
	}

	return (atWar(GET_PLAYER(getCombatOwner(eTeam, pPlot)).getTeam(), eTeam));
}

bool CvUnit::isPotentialEnemy(TeamTypes eTeam, const CvPlot* pPlot) const {
	if (NULL == pPlot) {
		pPlot = plot();
	}

	return (::isPotentialEnemy(GET_PLAYER(getCombatOwner(eTeam, pPlot)).getTeam(), eTeam));
}

bool CvUnit::isSuicide() const {
	return (m_pUnitInfo->isSuicide() || getKamikazePercent() != 0);
}

int CvUnit::getDropRange() const {
	return (m_pUnitInfo->getDropRange());
}

void CvUnit::getDefenderCombatValues(CvUnit& kDefender, const CvPlot* pPlot, int iOurStrength, int iOurFirepower, int& iTheirOdds, int& iTheirStrength, int& iOurDamage, int& iTheirDamage, CombatDetails* pTheirDetails) const {
	iTheirStrength = kDefender.currCombatStr(pPlot, this, pTheirDetails);
	int iTheirFirepower = kDefender.currFirepower(pPlot, this);

	FAssert((iOurStrength + iTheirStrength) > 0);
	FAssert((iOurFirepower + iTheirFirepower) > 0);

	iTheirOdds = ((GC.getCOMBAT_DIE_SIDES() * iTheirStrength) / (iOurStrength + iTheirStrength));

	if (kDefender.isBarbarian()) {
		if (GET_PLAYER(getOwnerINLINE()).getWinsVsBarbs() < GC.getHandicapInfo(GET_PLAYER(getOwnerINLINE()).getHandicapType()).getFreeWinsVsBarbs()) {
			iTheirOdds = std::min((10 * GC.getCOMBAT_DIE_SIDES()) / 100, iTheirOdds);
		}
	}
	if (isBarbarian()) {
		if (GET_PLAYER(kDefender.getOwnerINLINE()).getWinsVsBarbs() < GC.getHandicapInfo(GET_PLAYER(kDefender.getOwnerINLINE()).getHandicapType()).getFreeWinsVsBarbs()) {
			iTheirOdds = std::max((90 * GC.getCOMBAT_DIE_SIDES()) / 100, iTheirOdds);
		}
	}

	int iStrengthFactor = ((iOurFirepower + iTheirFirepower + 1) / 2);

	iOurDamage = std::max(1, ((GC.getCOMBAT_DAMAGE() * (iTheirFirepower + iStrengthFactor)) / (iOurFirepower + iStrengthFactor)));
	iTheirDamage = std::max(1, ((GC.getCOMBAT_DAMAGE() * (iOurFirepower + iStrengthFactor)) / (iTheirFirepower + iStrengthFactor)));
}

int CvUnit::getTriggerValue(EventTriggerTypes eTrigger, const CvPlot* pPlot, bool bCheckPlot) const {
	CvEventTriggerInfo& kTrigger = GC.getEventTriggerInfo(eTrigger);
	if (kTrigger.getNumUnits() <= 0) {
		return MIN_INT;
	}

	if (isDead()) {
		return MIN_INT;
	}

	if (!CvString(kTrigger.getPythonCanDoUnit()).empty()) {
		long lResult;

		CyArgsList argsList;
		argsList.add(eTrigger);
		argsList.add(getOwnerINLINE());
		argsList.add(getID());

		gDLL->getPythonIFace()->callFunction(PYRandomEventModule, kTrigger.getPythonCanDoUnit(), argsList.makeFunctionArgs(), &lResult);

		if (0 == lResult) {
			return MIN_INT;
		}
	}

	if (kTrigger.getNumUnitsRequired() > 0) {
		bool bFoundValid = false;
		for (int i = 0; i < kTrigger.getNumUnitsRequired(); ++i) {
			if (getUnitClassType() == kTrigger.getUnitRequired(i)) {
				bFoundValid = true;
				break;
			}
		}

		if (!bFoundValid) {
			return MIN_INT;
		}
	}

	if (bCheckPlot) {
		if (kTrigger.isUnitsOnPlot()) {
			if (!plot()->canTrigger(eTrigger, getOwnerINLINE())) {
				return MIN_INT;
			}
		}
	}

	int iValue = 0;

	if (0 == getDamage() && kTrigger.getUnitDamagedWeight() > 0) {
		return MIN_INT;
	}

	iValue += getDamage() * kTrigger.getUnitDamagedWeight();

	iValue += getExperience() * kTrigger.getUnitExperienceWeight();

	if (NULL != pPlot) {
		iValue += plotDistance(getX_INLINE(), getY_INLINE(), pPlot->getX_INLINE(), pPlot->getY_INLINE()) * kTrigger.getUnitDistanceWeight();
	}

	return iValue;
}

bool CvUnit::canApplyEvent(EventTypes eEvent) const {
	const CvEventInfo& kEvent = GC.getEventInfo(eEvent);

	if (0 != kEvent.getUnitExperience()) {
		if (!canAcquirePromotionAny()) {
			return false;
		}
	}

	if (NO_PROMOTION != kEvent.getUnitPromotion()) {
		if (!canAcquirePromotion((PromotionTypes)kEvent.getUnitPromotion())) {
			return false;
		}
	}

	if (kEvent.getUnitImmobileTurns() > 0) {
		if (!canAttack()) {
			return false;
		}
	}

	return true;
}

void CvUnit::applyEvent(EventTypes eEvent) {
	if (!canApplyEvent(eEvent)) {
		return;
	}

	const CvEventInfo& kEvent = GC.getEventInfo(eEvent);

	if (0 != kEvent.getUnitExperience()) {
		setDamage(0);
		changeExperience(kEvent.getUnitExperience());
	}

	if (NO_PROMOTION != kEvent.getUnitPromotion()) {
		setHasPromotion((PromotionTypes)kEvent.getUnitPromotion(), true);
	}

	if (kEvent.getUnitImmobileTurns() > 0) {
		changeImmobileTimer(kEvent.getUnitImmobileTurns());
		CvWString szText = gDLL->getText("TXT_KEY_EVENT_UNIT_IMMOBILE", getNameKey(), kEvent.getUnitImmobileTurns());
		gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szText, "AS2D_UNITGIFTED", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_UNIT_TEXT"), getX_INLINE(), getY_INLINE(), true, true);
	}

	CvWString szNameKey(kEvent.getUnitNameKey());

	if (!szNameKey.empty()) {
		setName(gDLL->getText(kEvent.getUnitNameKey()));
	}

	if (kEvent.isDisbandUnit()) {
		kill(false);
	}
}

const CvArtInfoUnit* CvUnit::getArtInfo(int i, EraTypes eEra) const {
	return m_pCustomUnitMeshGroup ? m_pCustomUnitMeshGroup->getArtInfo(i, eEra, (UnitArtStyleTypes)GC.getCivilizationInfo(getCivilizationType()).getUnitArtStyleType()) : m_pUnitInfo->getArtInfo(i, eEra, (UnitArtStyleTypes)GC.getCivilizationInfo(getCivilizationType()).getUnitArtStyleType());
}

const TCHAR* CvUnit::getButton() const {
	const CvArtInfoUnit* pArtInfo = getArtInfo(0, GET_PLAYER(getOwnerINLINE()).getCurrentEra());

	if (NULL != pArtInfo) {
		return pArtInfo->getButton();
	}

	return m_pUnitInfo->getButton();
}

int CvUnit::getGroupSize() const {
	return m_pCustomUnitMeshGroup ? m_pCustomUnitMeshGroup->getGroupSize() : m_pUnitInfo->getGroupSize();
}

int CvUnit::getGroupDefinitions() const {
	return m_pCustomUnitMeshGroup ? m_pCustomUnitMeshGroup->getGroupDefinitions() : m_pUnitInfo->getGroupDefinitions();
}

int CvUnit::getUnitGroupRequired(int i) const {
	return m_pCustomUnitMeshGroup ? m_pCustomUnitMeshGroup->getUnitGroupRequired(i) : m_pUnitInfo->getUnitGroupRequired(i);
}

bool CvUnit::isRenderAlways() const {
	return m_pUnitInfo->isRenderAlways();
}

float CvUnit::getAnimationMaxSpeed() const {
	return m_pCustomUnitMeshGroup ? m_pCustomUnitMeshGroup->getUnitMaxSpeed() : m_pUnitInfo->getUnitMaxSpeed();
}

float CvUnit::getAnimationPadTime() const {
	return m_pCustomUnitMeshGroup ? m_pCustomUnitMeshGroup->getUnitPadTime() : m_pUnitInfo->getUnitPadTime();
}

const char* CvUnit::getFormationType() const {
	return m_pUnitInfo->getFormationType();
}

bool CvUnit::isMechUnit() const {
	return m_pUnitInfo->isMechUnit();
}

bool CvUnit::isRenderBelowWater() const {
	return m_pUnitInfo->isRenderBelowWater();
}

int CvUnit::getRenderPriority(UnitSubEntityTypes eUnitSubEntity, int iMeshGroupType, int UNIT_MAX_SUB_TYPES) const {
	if (eUnitSubEntity == UNIT_SUB_ENTITY_SIEGE_TOWER) {
		return (getOwner() * (GC.getNumUnitInfos() + 2) * UNIT_MAX_SUB_TYPES) + iMeshGroupType;
	} else {
		return (getOwner() * (GC.getNumUnitInfos() + 2) * UNIT_MAX_SUB_TYPES) + m_eUnitType * UNIT_MAX_SUB_TYPES + iMeshGroupType;
	}
}

bool CvUnit::isAlwaysHostile(const CvPlot* pPlot) const {
	if (!isAlwaysHostile()) {
		return false;
	}

	if (NULL != pPlot && pPlot->isCity(true, getTeam())) {
		return false;
	}

	return true;
}

bool CvUnit::verifyStackValid() {
	if (!alwaysInvisible()) {
		if (plot()->isVisibleEnemyUnit(this)) {
			return jumpToNearestValidPlot();
		}
	}

	return true;
}


// Private Functions...

//check if quick combat
bool CvUnit::isCombatVisible(const CvUnit* pDefender) const {
	bool bVisible = false;

	if (!m_pUnitInfo->isQuickCombat()) {
		if (NULL == pDefender || !pDefender->getUnitInfo().isQuickCombat()) {
			if (isHuman()) {
				if (!GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_QUICK_ATTACK)) {
					bVisible = true;
				}
			} else if (NULL != pDefender && pDefender->isHuman()) {
				if (!GET_PLAYER(pDefender->getOwnerINLINE()).isOption(PLAYEROPTION_QUICK_DEFENSE)) {
					bVisible = true;
				}
			}
		}
	}

	return bVisible;
}

// used by the executable for the red glow and plot indicators
bool CvUnit::shouldShowEnemyGlow(TeamTypes eForTeam) const {
	if (isDelayedDeath()) {
		return false;
	}

	if (getDomainType() == DOMAIN_AIR) {
		return false;
	}

	if (!canFight()) {
		return false;
	}

	CvPlot* pPlot = plot();
	if (pPlot == NULL) {
		return false;
	}

	TeamTypes ePlotTeam = pPlot->getTeam();
	if (ePlotTeam != eForTeam) {
		return false;
	}

	if (!isEnemy(ePlotTeam)) {
		return false;
	}

	return true;
}

bool CvUnit::shouldShowFoundBorders() const {
	return isFound();
}


void CvUnit::cheat(bool bCtrl, bool bAlt, bool bShift) {
	if (gDLL->getChtLvl() > 0) {
		if (bCtrl) {
			setPromotionReady(true);
		}
	}
}

float CvUnit::getHealthBarModifier() const {
	return (GC.getDefineFLOAT("HEALTH_BAR_WIDTH") / (GC.getGameINLINE().getBestLandUnitCombat() * 2));
}

void CvUnit::getLayerAnimationPaths(std::vector<AnimationPathTypes>& aAnimationPaths) const {
	for (int i = 0; i < GC.getNumPromotionInfos(); ++i) {
		PromotionTypes ePromotion = (PromotionTypes)i;
		if (isHasPromotion(ePromotion)) {
			AnimationPathTypes eAnimationPath = (AnimationPathTypes)GC.getPromotionInfo(ePromotion).getLayerAnimationPath();
			if (eAnimationPath != ANIMATIONPATH_NONE) {
				aAnimationPaths.push_back(eAnimationPath);
			}
		}
	}
}

int CvUnit::getSelectionSoundScript() const {
	int iScriptId = getArtInfo(0, GET_PLAYER(getOwnerINLINE()).getCurrentEra())->getSelectionSoundScriptId();
	if (iScriptId == -1) {
		iScriptId = GC.getCivilizationInfo(getCivilizationType()).getSelectionSoundScriptId();
	}
	return iScriptId;
}

// Original isBetterDefenderThan call (without the extra parameter) - now just a pass-through
bool CvUnit::isBetterDefenderThan(const CvUnit* pDefender, const CvUnit* pAttacker) const {
	return isBetterDefenderThan(pDefender, pAttacker, NULL);
}

// Modified version of best defender code (minus the initial boolean tests,
// which we still check in the original method)
bool CvUnit::LFBisBetterDefenderThan(const CvUnit* pDefender, const CvUnit* pAttacker, int* pBestDefenderRank) const {
	// We adjust ranking based on ratio of our adjusted strength compared to twice that of attacker
	// Effect is if we're over twice as strong as attacker, we increase our ranking
	// (more likely to be picked as defender) - otherwise, we reduce our ranking (less likely)

	// Get our adjusted rankings based on combat odds
	int iOurRanking = LFBgetDefenderRank(pAttacker);
	int iTheirRanking = -1;
	if (pBestDefenderRank)
		iTheirRanking = (*pBestDefenderRank);
	if (iTheirRanking == -1)
		iTheirRanking = pDefender->LFBgetDefenderRank(pAttacker);

	// In case of equal value, fall back on unit cycle order
	// (K-Mod. _reversed_ unit cycle order, so that inexperienced units defend first.)
	if (iOurRanking == iTheirRanking) {
		if (isBeforeUnitCycle(this, pDefender))
			iTheirRanking++;
		else
			iTheirRanking--;
	}

	// Retain the basic rank (before value adjustment) for the best defender
	if (pBestDefenderRank)
		if (iOurRanking > iTheirRanking)
			(*pBestDefenderRank) = iOurRanking;

	return (iOurRanking > iTheirRanking);
}

// Get the (adjusted) odds of attacker winning to use in deciding best attacker
int CvUnit::LFBgetAttackerRank(const CvUnit* pDefender, int& iUnadjustedRank) const {
	if (pDefender) {
		int iDefOdds = pDefender->LFBgetDefenderOdds(this);
		iUnadjustedRank = 1000 - iDefOdds;
		// If attacker has a chance to withdraw, factor that in as well
		if (withdrawalProbability() > 0)
			iUnadjustedRank += ((iDefOdds * withdrawalProbability()) / 100);
	} else {
		// No defender ... just use strength, but try to make it a number out of 1000
		iUnadjustedRank = currCombatStr(NULL, NULL) / 5;
	}

	return LFBgetValueAdjustedOdds(iUnadjustedRank, false);
}

// Get the (adjusted) odds of defender winning to use in deciding best defender
int CvUnit::LFBgetDefenderRank(const CvUnit* pAttacker) const {
	int iRank = LFBgetDefenderOdds(pAttacker);
	// Don't adjust odds for value if attacker is limited in their damage (i.e: no risk of death)
	if ((pAttacker != NULL) && (maxHitPoints() <= pAttacker->combatLimit()))
		iRank = LFBgetValueAdjustedOdds(iRank, true);

	return iRank;
}

// Get the unadjusted odds of defender winning (used for both best defender and best attacker)
int CvUnit::LFBgetDefenderOdds(const CvUnit* pAttacker) const {
	// Check if we have a valid attacker
	bool bUseAttacker = false;
	int iAttStrength = 0;
	if (pAttacker)
		iAttStrength = pAttacker->currCombatStr(NULL, NULL);
	if (iAttStrength > 0)
		bUseAttacker = true;

	int iDefense = 0;

	if (bUseAttacker && GC.getLFBUseCombatOdds()) {
		// We start with straight combat odds
		iDefense = LFBgetDefenderCombatOdds(pAttacker);
	} else {
		// Lacking a real opponent (or if combat odds turned off) fall back on just using strength
		iDefense = currCombatStr(plot(), pAttacker);
		if (bUseAttacker) {
			// Similiar to the standard method, except I reduced the affect (cut it in half) handle attacker
			// and defender together (instead of applying one on top of the other) and substract the
			// attacker first strikes (instead of adding attacker first strikes when defender is immune)
			int iFirstStrikes = 0;

			if (!pAttacker->immuneToFirstStrikes())
				iFirstStrikes += (firstStrikes() * 2) + chanceFirstStrikes();
			if (!immuneToFirstStrikes())
				iFirstStrikes -= ((pAttacker->firstStrikes() * 2) + pAttacker->chanceFirstStrikes());

			if (iFirstStrikes != 0) {
				// With COMBAT_DAMAGE=20, this makes each first strike worth 8% (and each chance worth 4%)
				iDefense *= ((iFirstStrikes * GC.getCOMBAT_DAMAGE() / 5) + 100);
				iDefense /= 100;
			}

			// Make it a number out of 1000, taking attacker into consideration
			iDefense = (iDefense * 1000) / (iDefense + iAttStrength);
		}
	}

	return iDefense;
}

// Take the unadjusted odds and adjust them based on unit value
int CvUnit::LFBgetValueAdjustedOdds(int iOdds, bool bDefender) const {
	// Adjust odds based on value
	int iValue = LFBgetRelativeValueRating();
	// K-Mod: if we are defending, then let those with defensive promotions fight!
	if (bDefender) {
		int iDef = LFGgetDefensiveValueAdjustment();
		// I'm a little bit concerned that if a unit gets a bunch of promotions with an xp discount,
		// that unit may end up being valued less than a completely inexperienced unit.
		// Thus the experienced unit may end up being sacrificed to protect the rookie.

		iValue -= iDef;
		iValue = std::max(0, iValue);
	}
	long iAdjustment = -250;
	if (GC.getLFBUseSlidingScale())
		iAdjustment = (iOdds - 990);
	// Value Adjustment = (odds-990)*(value*num/denom)^2
	long iValueAdj = (long)(iValue * GC.getLFBAdjustNumerator());
	iValueAdj *= iAdjustment;
	iValueAdj /= (long)GC.getLFBAdjustDenominator();
	int iRank = iOdds + iValueAdj + 10000;
	// Note that the +10000 is just to try keeping it > 0 - doesn't really matter, other than that -1
	// would be interpreted later as not computed yet, which would cause us to compute it again each time

	// K-Mod. If this unit is a transport, reduce the value based on the risk of losing the cargo.
	// (This replaces the adjustment from LFBgetDefenderOdds. For more info, see the comments in that function.)
	if (hasCargo()) {
		int iAssetValue = std::max(1, getUnitInfo().getAssetValue());
		int iCargoAssetValue = 0;
		std::vector<CvUnit*> aCargoUnits;
		getCargoUnits(aCargoUnits);
		for (uint i = 0; i < aCargoUnits.size(); ++i) {
			iCargoAssetValue += aCargoUnits[i]->getUnitInfo().getAssetValue();
		}
		iRank -= 2 * (1000 - iOdds) * iCargoAssetValue / std::max(1, iAssetValue + iCargoAssetValue);
	}

	return iRank;
}

// Method to evaluate the value of a unit relative to another
int CvUnit::LFBgetRelativeValueRating() const {
	int iValueRating = 0;

	// Check if led by a Great General
	if (GC.getLFBBasedOnGeneral() > 0)
		if (NO_UNIT != getLeaderUnitType())
			iValueRating += GC.getLFBBasedOnGeneral();

	// Assign experience value in tiers
	// (formula changed for K-Mod)
	if (GC.getLFBBasedOnExperience() > 0) {
		int iTier = getLevel();
		while (getExperience() >= iTier * iTier + 1) {
			iTier++;
		}
		iValueRating += iTier * GC.getLFBBasedOnExperience();
	}

	// Check if unit is limited in how many can exist
	if (GC.getLFBBasedOnLimited() > 0)
		if (isLimitedUnitClass(getUnitClassType()))
			iValueRating += GC.getLFBBasedOnLimited();

	// Check if unit has ability to heal
	if (GC.getLFBBasedOnHealer() > 0)
		if (getSameTileHeal() > 0)
			iValueRating += GC.getLFBBasedOnHealer();

	return iValueRating;
}

// K-Mod. unit value adjustment based on how many defensive promotions are active on this plot.
// (The purpose of this is to encourage experienced units to fight when their promotions are especially suited to the plot they are defending.)
int CvUnit::LFGgetDefensiveValueAdjustment() const {
	int iValue = 0;

	for (int iI = 0; iI < GC.getNumPromotionInfos(); ++iI) {
		if (!isHasPromotion((PromotionTypes)iI))
			continue;

		CvPromotionInfo& kPromotion = GC.getPromotionInfo((PromotionTypes)iI);
		bool bDefensive = false;

		// Cities and hills
		if ((kPromotion.getCityDefensePercent() > 0 && plot()->isCity()) ||
			(kPromotion.getHillsDefensePercent() > 0 && plot()->isHills())) {
			bDefensive = true;
		}
		// Features
		if (!bDefensive) {
			for (int iJ = 0; iJ < GC.getNumFeatureInfos(); ++iJ) {
				if (kPromotion.getFeatureDefensePercent(iJ) > 0 && plot()->getFeatureType() == iJ) {
					bDefensive = true;
					break;
				}
			}
		}
		// Terrain
		if (!bDefensive) {
			for (int iJ = 0; iJ < GC.getNumTerrainInfos(); ++iJ) {
				if (kPromotion.getTerrainDefensePercent(iJ) > 0 && plot()->getTerrainType() == iJ) {
					bDefensive = true;
					break;
				}
			}
		}

		if (bDefensive) {
			iValue += GC.getLFBDefensiveAdjustment();
		}
	}

	return iValue;
}

int CvUnit::LFBgetDefenderCombatOdds(const CvUnit* pAttacker) const {
	int iAttackerStrength = pAttacker->currCombatStr(NULL, NULL);
	int iAttackerFirepower = pAttacker->currFirepower(NULL, NULL);

	int iDefenderStrength = currCombatStr(plot(), pAttacker);
	int iDefenderFirepower = currFirepower(plot(), pAttacker);

	FAssert((iAttackerStrength + iDefenderStrength) > 0);
	FAssert((iAttackerFirepower + iDefenderFirepower) > 0);

	int iDefenderOdds = ((GC.getCOMBAT_DIE_SIDES() * iDefenderStrength) / (iAttackerStrength + iDefenderStrength));
	int iStrengthFactor = ((iAttackerFirepower + iDefenderFirepower + 1) / 2);

	// calculate damage done in one round
	//////

	int iDamageToAttacker = std::max(1, ((GC.getCOMBAT_DAMAGE() * (iDefenderFirepower + iStrengthFactor)) / (iAttackerFirepower + iStrengthFactor)));
	int iDamageToDefender = std::max(1, ((GC.getCOMBAT_DAMAGE() * (iAttackerFirepower + iStrengthFactor)) / (iDefenderFirepower + iStrengthFactor)));

	// calculate needed rounds.
	// Needed rounds = round_up(health/damage)
	//////

	int iDefenderHitLimit = maxHitPoints() - pAttacker->combatLimit();

	int iNeededRoundsAttacker = (std::max(0, currHitPoints() - iDefenderHitLimit) + iDamageToDefender - 1) / iDamageToDefender;
	int iNeededRoundsDefender = (pAttacker->currHitPoints() + iDamageToAttacker - 1) / iDamageToAttacker;

	// calculate possible first strikes distribution.
	// We can't use the getCombatFirstStrikes() function (only one result,
	// no distribution), so we need to mimic it.
	//////

	int iAttackerLowFS = (immuneToFirstStrikes()) ? 0 : pAttacker->firstStrikes();
	int iAttackerHighFS = (immuneToFirstStrikes()) ? 0 : (pAttacker->firstStrikes() + pAttacker->chanceFirstStrikes());

	int iDefenderLowFS = (pAttacker->immuneToFirstStrikes()) ? 0 : firstStrikes();
	int iDefenderHighFS = (pAttacker->immuneToFirstStrikes()) ? 0 : (firstStrikes() + chanceFirstStrikes());

	return LFBgetCombatOdds(iDefenderLowFS, iDefenderHighFS, iAttackerLowFS, iAttackerHighFS, iNeededRoundsDefender, iNeededRoundsAttacker, iDefenderOdds);
}

CvCity* CvUnit::getHomeCity() const {
	return getCity(m_homeCity);
}

void CvUnit::setHomeCity(const CvCity* pCity) {
	if (pCity != NULL) {
		m_homeCity = pCity->getIDInfo();
	} else {
		m_homeCity.reset();
	}
}

int CvUnit::getRange() const {
	int iRange = MAX_INT;
	const CvPlayer& kOwner = GET_PLAYER(getOwnerINLINE());
	switch (getRangeType()) {
	case UNITRANGE_IMMOBILE:
	case UNITRANGE_HOME:
	case UNITRANGE_TERRITORY:
		iRange = 0;
		break;
	case UNITRANGE_RANGE:
		// Get the base range of the unit scaled to the world size
		iRange = GC.getINITIAL_UNIT_RANGE() + kOwner.getExtraRange() + getExtraRange() + (GC.getMapINLINE().getWorldSize() * 2);
		// Apply any percentage modifiers
		iRange *= (100 + kOwner.getExtraRangeModifier() + getExtraRangeModifier());
		iRange /= 100;
		break;
	case UNITRANGE_UNLIMITED:
	default:
		break;
	}
	return iRange;
}

void CvUnit::setExtraRange(int iRange) {
	m_iExtraRange = iRange;
}

void CvUnit::changeExtraRange(int iChange) {
	if (iChange != 0) {
		m_iExtraRange += iChange;
		FAssert(getExtraRange() >= 0);
	}
}

int CvUnit::getExtraRange() const {
	return m_iExtraRange;
}

void CvUnit::setExtraRangeModifier(int iModifier) {
	m_iExtraRangeModifier = iModifier;
}

void CvUnit::changeExtraRangeModifier(int iChange) {
	if (iChange > 0) {
		m_iExtraRangeModifier += iChange;
	}
}

int CvUnit::getExtraRangeModifier() const {
	return m_iExtraRangeModifier;
}

void CvUnit::changeRangeUnboundCount(int iChange) {
	m_iRangeUnboundCount += iChange;
}

void CvUnit::changeTerritoryUnboundCount(int iChange) {
	m_iTerritoryUnboundCount += iChange;
}

UnitRangeTypes CvUnit::getRangeType() const {
	if (isImmobile())
		return UNITRANGE_IMMOBILE;

	if (!isEnabled())
		return UNITRANGE_HOME;

	UnitRangeTypes ePlayerUnitRangeType = GET_PLAYER(getOwnerINLINE()).getUnitRangeType(&(this->getUnitInfo()));
	switch (m_pUnitInfo->getRangeType()) {
	case UNITRANGE_IMMOBILE:
		return UNITRANGE_IMMOBILE;
		break;
	case UNITRANGE_HOME:
		return UNITRANGE_HOME;
		break;
	case UNITRANGE_TERRITORY:
		// If the unit is specifically NOT unbound or is neutral and the player is not unbound
		// If the unit is specifically ubound then we drop down to the next case
		if (m_iTerritoryUnboundCount == 0 && ePlayerUnitRangeType == UNITRANGE_TERRITORY) {
			return UNITRANGE_TERRITORY;
		}
		// No break here deliberately
	case UNITRANGE_RANGE:
		// If the unit is specifically NOT unbound or is neutral and the player is not unbound
		// If the unit is specifically ubound then we drop down to the next case
		if (m_iRangeUnboundCount == 0 && ePlayerUnitRangeType == UNITRANGE_RANGE) {
			return UNITRANGE_RANGE;
		}
		// No break here deliberately
	case UNITRANGE_UNLIMITED:
	default:
		return UNITRANGE_UNLIMITED;
		break;
	}
}


void CvUnit::setCivicEnabled(bool bEnable) {
	if (m_bCivicEnabled != bEnable) {
		m_bCivicEnabled = bEnable;
		CvWString	szBuffer = bEnable ? gDLL->getText("TXT_KEY_CIVIC_ENABLED_UNIT", getNameKey()) : gDLL->getText("TXT_KEY_CIVIC_DISABLED_UNIT", getNameKey());
		gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, NULL, MESSAGE_TYPE_MINOR_EVENT, getUnitInfo().getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_WHITE"), getX(), getY(), true, true);

	}
}

bool CvUnit::isCivicEnabled() const {
	return m_bCivicEnabled;
}

bool CvUnit::isEnabled() const {
	return isCivicEnabled() && isWorldViewEnabled();
}

void CvUnit::salvage(CvUnit* pDeadUnit) {
	if (pDeadUnit == NULL)
		return;

	CvPlayer& kOwner = GET_PLAYER(getOwnerINLINE());
	const CvPlayer& kDeadOwner = GET_PLAYER(pDeadUnit->getOwnerINLINE());
	const CvUnitInfo& kDeadUnit = GC.getUnitInfo(pDeadUnit->getUnitType());

	// Only get salvage from animals in the pre-settled stage
	if (kDeadUnit.isAnimal() && kOwner.isCivSettled())
		return;

	CvCity* pSalvageCity = getHomeCity() != NULL ? getHomeCity() : kOwner.findCity(getX_INLINE(), getY_INLINE());
	if (pSalvageCity != NULL) {
		for (YieldTypes eYield = (YieldTypes)0; eYield < NUM_YIELD_TYPES; eYield = (YieldTypes)(eYield + 1)) {
			if (kDeadUnit.getYieldFromKill(eYield) > 0 || kOwner.getBaseYieldFromUnit(eYield) > 0) {
				int iYield = std::max(kDeadUnit.getYieldFromKill(eYield), kOwner.getBaseYieldFromUnit(eYield));
				// We do some jiggery-pokery here so that pre-settled we get the rounded up value with the salvage modifier applied
				// This is because many of the animals give small results so the modifier would provide no benefit using standard integer division
				// once we are settled then go back to the usual interpretation
				iYield *= std::max(0, kOwner.getYieldFromUnitModifier(eYield) + getSalvageModifier() + 100);
				iYield = !kOwner.isCivSettled() ? iYield / 100 + (iYield % 100 != 0) : iYield / 100;

				switch (eYield) {
				case YIELD_FOOD:
					pSalvageCity->changeFood(iYield);
					break;

				case YIELD_PRODUCTION:
					pSalvageCity->changeProduction(iYield);
					break;
				default:
					break;
				}
				CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YIELD_FROM_UNIT", getNameKey(), iYield, GC.getYieldInfo(eYield).getChar(), pSalvageCity->getNameKey());
				gDLL->getInterfaceIFace()->addHumanMessage(pSalvageCity->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, ARTFILEMGR.getInterfaceArtInfo("WORLDBUILDER_CITY_EDIT")->getPath(), MESSAGE_TYPE_INFO, NULL, (ColorTypes)GC.getInfoTypeForString("COLOR_WHITE"), pDeadUnit->getX_INLINE(), pDeadUnit->getY_INLINE(), true, true);
			}
		}
	}

	std::vector<TechTypes> vAvailableTechs;
	std::vector<CommerceTypes> vValidCommerces;

	for (CommerceTypes eCommerce = (CommerceTypes)0; eCommerce < NUM_COMMERCE_TYPES; eCommerce = (CommerceTypes)(eCommerce + 1)) {
		int iJ;
		bool bValid = true;
		if (kDeadUnit.getCommerceFromKill(eCommerce) > 0 || kOwner.getBaseCommerceFromUnit(eCommerce)) {
			if (eCommerce == COMMERCE_RESEARCH) {
				// We can always gain research towards our current goal if we are not settled yet
				if (!kOwner.isCivSettled()) {
					TechTypes eCurrentResearch = GET_PLAYER(getOwnerINLINE()).getCurrentResearch();
					if (eCurrentResearch != NO_TECH) {
						vAvailableTechs.push_back(eCurrentResearch);
					}
				}

				for (iJ = 0; iJ < kDeadUnit.getNumPrereqAndTechs(); iJ++) {
					TechTypes eTempTech = (TechTypes)kDeadUnit.getPrereqAndTech(iJ);
					if (eTempTech != NO_TECH) {
						if (kOwner.canResearch(eTempTech, true) && GET_TEAM(kDeadOwner.getTeam()).isHasTech(eTempTech)) {
							vAvailableTechs.push_back(eTempTech);
						}
					}
				}

				for (PromotionTypes ePromotion = (PromotionTypes)0; ePromotion < GC.getNumPromotionInfos(); ePromotion = (PromotionTypes)(ePromotion + 1)) {
					if (pDeadUnit->isHasPromotion(ePromotion)) {
						TechTypes eTempTech = (TechTypes)GC.getPromotionInfo(ePromotion).getTechPrereq();
						if (eTempTech != NO_TECH) {
							if (kOwner.canResearch(eTempTech, true) && GET_TEAM(kDeadOwner.getTeam()).isHasTech(eTempTech)) {
								vAvailableTechs.push_back(eTempTech);
							}
						}
					}
				}

				bValid = (vAvailableTechs.size() > 0);
			}

			if (eCommerce == COMMERCE_ESPIONAGE && pDeadUnit->isBarbarian()) {
				bValid = false;
			}

			if (bValid) {
				vValidCommerces.push_back(eCommerce);
			}
		}
	}

	if (vValidCommerces.size() > 0) {
		int iRoll = (CommerceTypes)GC.getGameINLINE().getSorenRandNum(vValidCommerces.size(), "Pillage");

		CommerceTypes eCommerce = vValidCommerces[iRoll];

		int iCommerce = std::max(kDeadUnit.getCommerceFromKill(eCommerce), kOwner.getBaseCommerceFromUnit(eCommerce));
		// Same jiggery-pokery as the yield calculation above
		iCommerce *= std::max(0, kOwner.getCommerceFromUnitModifier(eCommerce) + getSalvageModifier() + 100);
		iCommerce = !kOwner.isCivSettled() ? iCommerce / 100 + (iCommerce % 100 != 0) : iCommerce / 100;

		int iCommerceVariable = (GC.getGameINLINE().getSorenRandNum(iCommerce, "Commerce Variable") * GC.getDefineINT("UNIT_SALVAGE_COMM_VARIABLE_PERCENT"));
		iCommerceVariable /= 100;

		int iTotalCommerce = iCommerce + iCommerceVariable;

		if (iTotalCommerce > 0) {
			CvWString szBuffer;
			switch (eCommerce) {
			case COMMERCE_GOLD:
				kOwner.changeGold(iTotalCommerce);
				szBuffer = gDLL->getText("TXT_KEY_MISC_GOLD_FROM_UNIT", iTotalCommerce, GC.getCommerceInfo(eCommerce).getChar(), kDeadOwner.getCivilizationAdjectiveKey(), pDeadUnit->getNameKey());
				gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_PILLAGE", MESSAGE_TYPE_INFO, m_pUnitInfo->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_WHITE"), getX_INLINE(), getY_INLINE());
				break;
			case COMMERCE_CULTURE:
				if (pSalvageCity != NULL) {
					pSalvageCity->changeCulture(getOwnerINLINE(), iTotalCommerce, true, true);
					szBuffer = gDLL->getText("TXT_KEY_MISC_CULTURE_FROM_UNIT", iTotalCommerce, GC.getCommerceInfo(eCommerce).getChar(), kDeadOwner.getCivilizationAdjectiveKey(), pDeadUnit->getNameKey(), pSalvageCity->getNameKey());
					gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_PILLAGE", MESSAGE_TYPE_INFO, m_pUnitInfo->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_WHITE"), getX_INLINE(), getY_INLINE());
				}
				break;
			case COMMERCE_RESEARCH:
				{
					TechTypes eRewardTech = vAvailableTechs[GC.getGameINLINE().getSorenRandNum(vAvailableTechs.size(), "Choose Tech")];
					if (eRewardTech != NO_TECH) {
						if (kOwner.getCurrentResearch() == eRewardTech) {
							iTotalCommerce *= GC.getDefineINT("UNIT_SALVAGE_CURRENT_TECH_PERCENT");
							iTotalCommerce /= 100;
						}

						GET_TEAM(getTeam()).changeResearchProgress(eRewardTech, iTotalCommerce, getOwnerINLINE());
						szBuffer = gDLL->getText("TXT_KEY_MISC_ACQUIRE_RESEARCH_FROM_UNIT", iTotalCommerce, GC.getCommerceInfo(eCommerce).getChar(), kDeadOwner.getCivilizationAdjectiveKey(), pDeadUnit->getNameKey(), GC.getTechInfo(eRewardTech).getTextKeyWide());
						gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_PILLAGE", MESSAGE_TYPE_INFO, m_pUnitInfo->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_WHITE"), getX_INLINE(), getY_INLINE());
					}
				}
				break;
			case COMMERCE_ESPIONAGE:
				if (!pDeadUnit->isBarbarian()) {
					GET_TEAM(getTeam()).changeEspionagePointsAgainstTeam(kDeadOwner.getTeam(), iTotalCommerce);
					szBuffer = gDLL->getText("TXT_KEY_MISC_GATHERED_INTELLIGENCE_FROM_UNIT", iTotalCommerce, GC.getCommerceInfo(eCommerce).getChar(), kDeadOwner.getCivilizationAdjectiveKey(), pDeadUnit->getNameKey(), kDeadOwner.getNameKey());
					gDLL->getInterfaceIFace()->addHumanMessage(getOwnerINLINE(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_PILLAGE", MESSAGE_TYPE_INFO, m_pUnitInfo->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_WHITE"), getX_INLINE(), getY_INLINE());
				}
				break;
			default:
				break;
			}
		}
	}
}

void CvUnit::setGroupPromotionChanged(bool bChanged) {
	m_bGroupPromotionChanged = bChanged;
}

bool CvUnit::isHasGroupPromotionChanged() const {
	return m_bGroupPromotionChanged;
}

bool CvUnit::isHasPromotionGroup(PromotionTypes eIndex) const {
	int iPromotionGroup = GC.getPromotionInfo(eIndex).getPromotionGroup();
	if (iPromotionGroup == 0) {
		return false;
	}

	for (PromotionTypes eLoopPromotion = (PromotionTypes)0; eLoopPromotion < GC.getNumPromotionInfos(); eLoopPromotion = (PromotionTypes)(eLoopPromotion + 1)) {
		if (isHasPromotion(eLoopPromotion) && (GC.getPromotionInfo(eLoopPromotion).getPromotionGroup() == iPromotionGroup)) {
			return true;
		}
	}
	return false;
}

void CvUnit::doFieldPromotions(CombatData* data, CvUnit* pDefender, CvPlot* pPlot) {
	// If neither unit can gain promotions then forget it
	if (getUnitCombatType() == NO_UNITCOMBAT && pDefender->getUnitCombatType() == NO_UNITCOMBAT)
		return;

	std::vector<PromotionTypes> aAttackerAvailablePromotions;
	std::vector<PromotionTypes> aDefenderAvailablePromotions;
	for (PromotionTypes ePromotion = (PromotionTypes)0; ePromotion < GC.getNumPromotionInfos(); ePromotion = (PromotionTypes)(ePromotion + 1)) {
		const CvPromotionInfo& kPromotion = GC.getPromotionInfo(ePromotion);
		/* Block These Promotions */
		if (kPromotion.getKamikazePercent() > 0)
			continue;

		if (kPromotion.getStateReligionPrereq() != NO_RELIGION && GET_PLAYER(getOwnerINLINE()).getStateReligion() != kPromotion.getStateReligionPrereq())
			continue;

		if (kPromotion.isLeader())
			continue;

		if (pDefender->isDead()) {
			if (!canAcquirePromotion(ePromotion)) //attacker can not acquire this promotion
				continue;

			//* attacker was crossing river
			if (kPromotion.isRiver() && data->bRiverAttack)	//this bonus is being applied to defender
			{
				aAttackerAvailablePromotions.push_back(ePromotion);
			}
			//* attack from water
			else if (kPromotion.isAmphib() && data->bAmphibAttack) {
				aAttackerAvailablePromotions.push_back(ePromotion);
			}
			//* attack terrain
			else if (kPromotion.getTerrainAttackPercent(pPlot->getTerrainType()) > 0) {
				aAttackerAvailablePromotions.push_back(ePromotion);
			}
			//* attack feature
			else if (pPlot->getFeatureType() != NO_FEATURE && kPromotion.getFeatureAttackPercent(pPlot->getFeatureType()) > 0) {
				aAttackerAvailablePromotions.push_back(ePromotion);
			}
			//* attack hills
			else if (kPromotion.getHillsAttackPercent() > 0 && pPlot->isHills()) {
				aAttackerAvailablePromotions.push_back(ePromotion);
			}
			//* attack city
			else if (kPromotion.getCityAttackPercent() > 0 && pPlot->isCity(true))	//count forts too
			{
				aAttackerAvailablePromotions.push_back(ePromotion);
			}
			//* first strikes/chances promotions
			else if ((kPromotion.getFirstStrikesChange() > 0 || kPromotion.getChanceFirstStrikesChange() > 0) && (firstStrikes() > 0 || chanceFirstStrikes() > 0)) {
				aAttackerAvailablePromotions.push_back(ePromotion);
			}
			//* unit combat mod
			else if (kPromotion.getUnitCombatModifierPercent(pDefender->getUnitCombatType()) > 0) {
				aAttackerAvailablePromotions.push_back(ePromotion);
			}
			//* combat strength promotions
			else if (kPromotion.getCombatPercent() > 0) {
				aAttackerAvailablePromotions.push_back(ePromotion);
			}
			//* domain mod
			else if (kPromotion.getDomainModifierPercent(pDefender->getDomainType())) {
				aAttackerAvailablePromotions.push_back(ePromotion);
			}
			//* blitz
			else if (kPromotion.isBlitz() && data->bAttackerUninjured) {
				aAttackerAvailablePromotions.push_back(ePromotion);
			}
			//* salvage
			else if (kPromotion.getSalvageModifier() > 0) {
				aAttackerAvailablePromotions.push_back(ePromotion);
			}
		}	//if defender is dead

		// Defender withdrawn
		else if (data->bDefenderWithdrawn) {
			//* defender withdrawn, give him withdrawal promo
			if (kPromotion.getWithdrawalChange() > 0 && pDefender->canAcquirePromotion(ePromotion)) {
				aDefenderAvailablePromotions.push_back(ePromotion);
			}
		}

		// Attacker withdrawn
		else if (data->bAttackerWithdrawn) {
			//* defender withdrawn, give him withdrawal promo
			if (kPromotion.getWithdrawalChange() > 0 && canAcquirePromotion(ePromotion)) {
				aAttackerAvailablePromotions.push_back(ePromotion);
			}
		} 
		
		// Attacker dead
		else {
			if (!pDefender->canAcquirePromotion(ePromotion))
				continue;

			//* defend terrain
			if (kPromotion.getTerrainDefensePercent(pPlot->getTerrainType()) > 0) {
				aDefenderAvailablePromotions.push_back(ePromotion);
			}
			//* defend feature
			else if (pPlot->getFeatureType() != NO_FEATURE && kPromotion.getFeatureDefensePercent(pPlot->getFeatureType()) > 0) {
				aDefenderAvailablePromotions.push_back(ePromotion);
			}
			//* defend hills
			else if (kPromotion.getHillsDefensePercent() > 0 && pPlot->isHills()) {
				aDefenderAvailablePromotions.push_back(ePromotion);
			}
			//* defend city
			else if (kPromotion.getCityDefensePercent() > 0 && pPlot->isCity(true))	//count forts too
			{
				aDefenderAvailablePromotions.push_back(ePromotion);
			}
			//* first strikes/chances promotions
			else if ((kPromotion.getFirstStrikesChange() > 0 || kPromotion.getChanceFirstStrikesChange() > 0) && (pDefender->firstStrikes() > 0 || pDefender->chanceFirstStrikes() > 0)) {
				aDefenderAvailablePromotions.push_back(ePromotion);
			}
			//* unit combat mod vs attacker unit type
			else if (kPromotion.getUnitCombatModifierPercent(getUnitCombatType()) > 0) {
				aDefenderAvailablePromotions.push_back(ePromotion);
			}
			//* combat strength promotions
			else if (kPromotion.getCombatPercent() > 0) {
				aDefenderAvailablePromotions.push_back(ePromotion);
			}
			//* domain mod
			else if (kPromotion.getDomainModifierPercent(getDomainType())) {
				aDefenderAvailablePromotions.push_back(ePromotion);
			}
			//* salvage
			else if (kPromotion.getSalvageModifier() > 0) {
				aDefenderAvailablePromotions.push_back(ePromotion);
			}
		}	//if attacker dead
	}	//end promotion types cycle

	//promote attacker:
	if (!isDead() && aAttackerAvailablePromotions.size() > 0) {
		int iHealthLostPerc = (getDamage() - data->iAttackerInitialDamage) * 100 / (maxHitPoints() - data->iAttackerInitialDamage);
		int iPromotionChance = (GC.getDefineINT("COMBAT_DIE_SIDES") - data->iWinningOdds) * (100 + iHealthLostPerc) / GC.getDefineINT("COMBAT_DIE_SIDES");

		if (GC.getGameINLINE().getSorenRandNum(GC.getDefineINT("COMBAT_DIE_SIDES"), "Occasional Promotion") < iPromotionChance) {
			//select random promotion from available
			PromotionTypes ePromotion = aAttackerAvailablePromotions[GC.getGameINLINE().getSorenRandNum(aAttackerAvailablePromotions.size(), "Select Promotion Type")];
			//promote
			setHasPromotion(ePromotion, true);
			//show message
			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_UNIT_PROMOTED_IN_BATTLE", getNameKey(), GC.getPromotionInfo(ePromotion).getText());
			gDLL->getInterfaceIFace()->addMessage(getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getPromotionInfo(ePromotion).getSound(), MESSAGE_TYPE_INFO, NULL, (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), this->plot()->getX_INLINE(), this->plot()->getY_INLINE());
		}
	}
	//promote defender:
	if (!pDefender->isDead() && aDefenderAvailablePromotions.size() > 0) {
		int iHealthLostPerc = (getDamage() - data->iDefenderInitialDamage) * 100 / (maxHitPoints() - data->iDefenderInitialDamage);
		int iPromotionChance = data->iWinningOdds * (100 + iHealthLostPerc) / GC.getDefineINT("COMBAT_DIE_SIDES");

		if (GC.getGameINLINE().getSorenRandNum(GC.getDefineINT("COMBAT_DIE_SIDES"), "Occasional Promotion") < iPromotionChance) {
			//select random promotion from available
			PromotionTypes ePromotion = aDefenderAvailablePromotions[GC.getGameINLINE().getSorenRandNum(aDefenderAvailablePromotions.size(), "Select Promotion Type")];
			//promote
			pDefender->setHasPromotion(ePromotion, true);
			//show message
			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_UNIT_PROMOTED_IN_BATTLE", pDefender->getNameKey(), GC.getPromotionInfo(ePromotion).getText());
			gDLL->getInterfaceIFace()->addMessage(pDefender->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getPromotionInfo(ePromotion).getSound(), MESSAGE_TYPE_INFO, NULL, (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pPlot->getX_INLINE(), pPlot->getY_INLINE());
		}
	}

}

int CvUnit::getCanMovePeaksCount() const {
	return m_iCanMovePeaksCount;
}

bool CvUnit::isCanMovePeaks() const {
	return (getCanMovePeaksCount() > 0);
}

void CvUnit::changeCanMovePeaksCount(int iChange) {
	m_iCanMovePeaksCount += iChange;
	FAssert(getCanMovePeaksCount() >= 0);
}

// We have been traded away to another player
void CvUnit::tradeUnit(PlayerTypes eReceivingPlayer) {
	if (eReceivingPlayer != NO_PLAYER) {
		CvCity* pBestCity = GET_PLAYER(eReceivingPlayer).getCapitalCity();

		if (getDomainType() == DOMAIN_SEA) {
			//Find the first coastal city, and put the ship there
			int iLoop;
			for (CvCity* pLoopCity = GET_PLAYER(eReceivingPlayer).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(eReceivingPlayer).nextCity(&iLoop)) {
				if (pLoopCity->isCoastal(GC.getMIN_WATER_SIZE_FOR_OCEAN())) {
					pBestCity = pLoopCity;
					break;
				}
			}
		}

		// If we are on a transport then remove ourselves
		setTransportUnit(NULL);

		CvUnit* pTradeUnit = GET_PLAYER(eReceivingPlayer).initUnit(getUnitType(), pBestCity->getX_INLINE(), pBestCity->getY_INLINE(), AI_getUnitAIType());

		pTradeUnit->convert(this);

		CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_TRADED_UNIT_TO_YOU", GET_PLAYER(getOwnerINLINE()).getNameKey(), pTradeUnit->getNameKey());
		gDLL->getInterfaceIFace()->addMessage(pTradeUnit->getOwnerINLINE(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_UNITGIFTED", MESSAGE_TYPE_INFO, pTradeUnit->getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_WHITE"), pTradeUnit->getX_INLINE(), pTradeUnit->getY_INLINE(), true, true);

	}
}

bool CvUnit::canTradeUnit(PlayerTypes eReceivingPlayer) {
	if (eReceivingPlayer == NO_PLAYER || eReceivingPlayer > MAX_PLAYERS) {
		return false;
	}

	if (getCargo() > 0) {
		return false;
	}

	bool bShip = false;
	bool bCoast = false;
	if (getDomainType() == DOMAIN_SEA) {
		bShip = true;
		int iLoop;
		for (CvCity* pLoopCity = GET_PLAYER(eReceivingPlayer).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(eReceivingPlayer).nextCity(&iLoop)) {
			if (pLoopCity->waterArea() != NULL && !pLoopCity->waterArea()->isLake()) {
				bCoast = true;
			}
		}
	}

	if (bShip && !bCoast) {
		return false;
	}

	return true;
}

bool CvUnit::canFortAttack() const {
	bool bFortAttack = false;
	for (UnitCombatTypes eUnitCombat = (UnitCombatTypes)0; eUnitCombat < GC.getNumUnitCombatInfos(); eUnitCombat = (UnitCombatTypes)(eUnitCombat + 1)) {
		if (GC.getUnitCombatInfo(eUnitCombat).isFortAttack() && isUnitCombatType(eUnitCombat)) {
			bFortAttack = true;
			break;
		}
	}
	return bFortAttack;
}

void CvUnit::checkWorldViewStatus() {
	bool bValid = true;
	const CvUnitInfo& kUnit = GC.getUnitInfo(m_eUnitType);
	int iNumWorldViewPrereqs = kUnit.getNumPrereqWorldViews();
	for (int iI = 0; iI < iNumWorldViewPrereqs && bValid; iI++) {
		if (kUnit.isPrereqWorldView(iI) && !GET_PLAYER(getOwnerINLINE()).isWorldViewActivated((WorldViewTypes)iI)) {
			bValid = false;
		}
	}
	m_bWorldViewEnabled = bValid;
}

bool CvUnit::isWorldViewEnabled() const {
	return m_bWorldViewEnabled;
}

int CvUnit::getSlaveCount(SpecialistTypes iIndex) const {
	return m_paiEnslavedCount[iIndex];
}

int CvUnit::getSlaveCountTotal() const {
	int iTotal = 0;
	for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
		iTotal += m_paiEnslavedCount[eSpecialist];
	}

	return iTotal;
}

void CvUnit::changeSlaveCount(SpecialistTypes iIndex, int iChange) {
	FAssertMsg(iIndex >= 0, "iIndex expected to be >= 0");
	FAssertMsg(iIndex < GC.getNumSpecialistInfos(), "iIndex expected to be < GC.getNumSpecialistInfos()");

	// Now do the extra slave processing
	int iNewValue = getSlaveCount(iIndex) + iChange;
	m_paiEnslavedCount[iIndex] = iNewValue;
	FAssert(getSlaveCount(iIndex) >= 0);
}

void CvUnit::setMaxSlaves(int iValue) {
	m_iMaxSlaves = iValue;
}

void CvUnit::changeMaxSlaves(int iChange) {
	m_iMaxSlaves += iChange;
}

int CvUnit::getMaxSlaves() const {
	return m_iMaxSlaves;
}

void CvUnit::changeSlaveControlCount(int iChange) {
	m_iSlaveControlCount += iChange;
}

int CvUnit::getSlaveControlCount() const {
	return std::max(m_iSlaveControlCount, getMaxSlaves() - getSlaveCountTotal());
}

bool CvUnit::canEnslave() const {
	return isEnabled() && (getMaxSlaves() > getSlaveCountTotal());
}

bool CvUnit::canSellSlave(const CvPlot* pPlot) const {
	if (getSlaveCountTotal() <= 0)
		return false;

	if (!isEnabled())
		return false;

	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL || pCity->getTeam() != getTeam() || !pCity->isSlaveMarket() || GET_PLAYER(getOwnerINLINE()).getGold() < pCity->getSlaveCost(getSlaveCountTotal()))
		return false;

	return true;
}

bool CvUnit::sellSlaves() {
	if (!canSellSlave(plot()))
		return false;

	CvPlayer& kPlayer = GET_PLAYER(getOwnerINLINE());

	// Lets see the colour of their money first!
	kPlayer.changeGold(0 - plot()->getPlotCity()->getSlaveCost(getSlaveCountTotal()));

	// Loop through all our slaves types
	for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
		int iNumSlaves = m_paiEnslavedCount[eSpecialist];
		for (int iY = 0; iY < iNumSlaves; iY++) {
			UnitTypes eSlave = getSlaveUnit();
			if (eSlave != NO_UNIT) {
				CvUnit* pSlave = kPlayer.initUnit(eSlave, getX(), getY());
				pSlave->setSlaveSpecialistType(eSpecialist);
				pSlave->finishMoves();
			}
		}
		m_paiEnslavedCount[eSpecialist] = 0;
	}

	finishMoves();

	return true;
}

bool CvUnit::enslaveUnit(CvUnit* pWinner, CvUnit* pLoser) {
	bool bEnslaved = false;

	CvUnit* pSlaver = getSlaver(pWinner);

	if (NULL != pSlaver) {
		int iSlaveSlotsLeft = pSlaver->getMaxSlaves() - pSlaver->getSlaveCountTotal();
		if (GET_PLAYER(pWinner->getOwnerINLINE()).isWorldViewActivated(WORLD_VIEW_SLAVERY) && iSlaveSlotsLeft > 0) {


			// If we have captured a slaver we also capture any slaves they have
			if (pLoser->getMaxSlaves() > 0) {
				// take any slaves they have
				for (SpecialistTypes eSpecialist = (SpecialistTypes)0; eSpecialist < GC.getNumSpecialistInfos(); eSpecialist = (SpecialistTypes)(eSpecialist + 1)) {
					int iNumSlaves = pLoser->m_paiEnslavedCount[eSpecialist];
					if (iNumSlaves > 0 && iSlaveSlotsLeft >= iNumSlaves) {
						// We can keep all the slaves of this type
						pSlaver->changeSlaveCount(eSpecialist, iNumSlaves);
						bEnslaved = true;

						iSlaveSlotsLeft -= iNumSlaves;
						if (iSlaveSlotsLeft == 0) {
							break;
						}
					} else if (iNumSlaves > 0 && iSlaveSlotsLeft > 0) {
						// We can only keep some of the slaves so fill up and exit
						pSlaver->changeSlaveCount(eSpecialist, iSlaveSlotsLeft);
						bEnslaved = true;
						continue;
					}
				}
			}

			// Now deal with the actual loser unit
			SpecialistTypes eLoserSpecialist = pLoser->getSlaveSpecialistType();
			if (eLoserSpecialist != NO_SPECIALIST) {
				pSlaver->changeSlaveCount(eLoserSpecialist, 1);
				bEnslaved = true;
			}
		}

		// If a unit is enslaved then have the loser remember this
		area()->resetSlaveMemoryPerPlayer(pLoser->getOwnerINLINE());
	}
	return bEnslaved;
}

CvUnit* CvUnit::getSlaver(CvUnit* pWinner) {
	FAssertMsg(pWinner != NULL, "Winner should not be NULL");

	CvUnit* pSlaver = NULL;

	CvPlot* pPlot = pWinner->plot();
	for (int i = 0; i < pPlot->getNumUnits(); i++) {
		CvUnit* pTargetUnit = pPlot->getUnitByIndex(i);
		if (NULL != pTargetUnit && pTargetUnit->getOwner() == pWinner->getOwner() && pTargetUnit->canEnslave()) {
			pSlaver = pTargetUnit;
			break;
		}
	}
	return pSlaver;
}

CvPlot* CvUnit::getBestSlaveMarket(bool bCurrentAreaOnly) {
	CvCity* pBestCity = NULL;
	int iBestValue = 0;

	TeamTypes eTeam = getTeam();
	int iArea = getArea();
	int iX = getX_INLINE(), iY = getY_INLINE();

	// check every player on our team's cities
	for (PlayerTypes eLoopPlayer = (PlayerTypes)0; eLoopPlayer < MAX_PLAYERS; eLoopPlayer = (PlayerTypes)(eLoopPlayer + 1)) {
		// is this player on our team?
		CvPlayerAI& kLoopPlayer = GET_PLAYER(eLoopPlayer);
		if (kLoopPlayer.isAlive() && kLoopPlayer.getTeam() == eTeam) {
			int iLoop;
			for (CvCity* pLoopCity = kLoopPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kLoopPlayer.nextCity(&iLoop)) {
				// can we sell slaves in this city 
				if (pLoopCity->isSlaveMarket()) {
					// if not same area, and only looking for current area then bail
					if (iArea != pLoopCity->getArea() && bCurrentAreaOnly)
						continue;

					// Start high so we can use division and still get a reasonable value comparison
					int iValue = 10000;

					// if we cannot path there, then give it a big negative
					int iTurns;
					if (!generatePath(pLoopCity->plot(), 0, true, &iTurns))
						iValue /= 5;

					// Further away is bad, using every 3 turns as a divisor
					iValue /= std::max(1, iTurns / 3);

					// Current area is a major benefit
					if (iArea == pLoopCity->getArea())
						iValue *= 6;

					// perefer our own cities
					if (eLoopPlayer == getOwnerINLINE())
						iValue *= 4;

					// If we need workers there
					if (pLoopCity->AI_getWorkersNeeded() > pLoopCity->AI_getWorkersHave())
						iValue *= 2;

					// Each 20 turns of production left in a city increments multiplier
					if (pLoopCity->canSettleSlave() && kLoopPlayer.isAnarchy())
						iValue *= std::min(1, pLoopCity->getProductionTurnsLeft() / 20);

					// If the city already has its max number of safe slaves
					if (pLoopCity->getSettledSlaveCountTotal() >= pLoopCity->getSlaveSafeLevel())
						iValue /= 25;

					if (iValue > iBestValue) {
						iBestValue = iValue;
						pBestCity = pLoopCity;
					}
				}
			}
		}
	}
	return pBestCity ? pBestCity->plot() : NULL;
}

UnitTypes CvUnit::getSlaveUnit() const {
	std::vector<UnitTypes> vSlaveUnits;
	for (UnitClassTypes eUnitClass = (UnitClassTypes)0; eUnitClass < GC.getNumUnitClassInfos(); eUnitClass = (UnitClassTypes)(eUnitClass + 1)) {
		UnitTypes eCivUnit = (UnitTypes)GC.getCivilizationInfo(this->getCivilizationType()).getCivilizationUnits(eUnitClass);
		if (eCivUnit != NULL && GC.getUnitInfo(eCivUnit).isSlave())
			vSlaveUnits.push_back(eCivUnit);
	}

	// Pick a slave type at random
	int iIndex = GC.getGameINLINE().getSorenRandNum(vSlaveUnits.size(), "Slave type");
	return vSlaveUnits.size() > 0 ? vSlaveUnits[iIndex] : NO_UNIT;
}

bool CvUnit::isSlave() const {
	return m_pUnitInfo->isSlave();
}

SpecialistTypes CvUnit::getSlaveSpecialistType() const {
	if (isMechUnit() || isAnimal()) {
		return NO_SPECIALIST;
	}

	return (SpecialistTypes)m_iSlaveSpecialistType;
}

void CvUnit::setSlaveSpecialistType(SpecialistTypes eSpecialistType) {
	m_iSlaveSpecialistType = eSpecialistType;
}


bool CvUnit::canWorkCity(const CvPlot* pPlot) const {
	if (!isSlave()) {
		return false;
	}

	if (!GET_PLAYER(getOwnerINLINE()).isWorldViewActivated(WORLD_VIEW_SLAVERY)) {
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL || pCity->getTeam() != getTeam() || !pCity->canSettleSlave()) {
		return false;
	}

	return true;
}

bool CvUnit::canShadow() const {
	if (!canFight())
		return false;

	if (!isEnabled())
		return false;

	// We need to cancel any existing shadow automation first
	if (getShadowUnit() != NULL)
		return false;

	return true;
}

bool CvUnit::canShadowAt(CvPlot* pShadowPlot, CvUnit* pShadowUnit) const {
	if (!canShadow())
		return false;

	if (pShadowPlot == NULL)
		return false;

	// If we don't have a unit yet iterate through the units on the plot to see if there is one we can shadow
	if (pShadowUnit == NULL) {
		CvUnit* pLoopShadow = NULL;
		CLLNode<IDInfo>* pUnitShadowNode = pShadowPlot->headUnitNode();
		while (pUnitShadowNode != NULL) {
			pLoopShadow = ::getUnit(pUnitShadowNode->m_data);
			pUnitShadowNode = pShadowPlot->nextUnitNode(pUnitShadowNode);
			if (canShadowAt(pShadowPlot, pLoopShadow)) {
				pShadowUnit = pLoopShadow;
				break;
			}
		}
	}

	if (pShadowUnit == NULL)
		return false;

	if (pShadowUnit->getTeam() != getTeam())
		return false;

	// Allow shadowing workers even if they are faster, the shadow will catch up, slightly risky, but up to the player
	if (pShadowUnit->baseMoves() > baseMoves() && GC.getUnitInfo(pShadowUnit->getUnitType()).getWorkRate() <= 0)
		return false;

	if (pShadowUnit == this)
		return false;

	int iPathTurns;
	if (!generatePath(pShadowPlot, 0, true, &iPathTurns))
		return false;

	return true;
}

CvUnit* CvUnit::getShadowUnit() const {
	return getUnit(m_shadowUnit);
}

void CvUnit::setShadowUnit(CvUnit* pUnit) {
	if (pUnit != NULL)
		m_shadowUnit = pUnit->getIDInfo();
	else
		m_shadowUnit.reset();
}

void CvUnit::clearShadowUnit() {
	m_shadowUnit.reset();
}

bool CvUnit::setShadowUnit(CvPlot* pPlot, int iFlags) {
	if (pPlot != NULL) {
		//Check for multiple valid units
		int iValidShadowUnits = 0;
		CLLNode<IDInfo>* pUnitShadowNode = pPlot->headUnitNode();
		while (pUnitShadowNode != NULL) {
			CvUnit* pLoopShadow = ::getUnit(pUnitShadowNode->m_data);
			pUnitShadowNode = pPlot->nextUnitNode(pUnitShadowNode);
			if (canShadowAt(pPlot, pLoopShadow)) {
				iValidShadowUnits++;
			}
		}
		//Strange Handling to ensure MP works
		if (iFlags == 0 && iValidShadowUnits > 1) {
			CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_SELECT_UNIT, getID(), pPlot->getX(), pPlot->getY());
			if (pInfo) {
				gDLL->getInterfaceIFace()->addPopup(pInfo, getOwnerINLINE(), true);
			}
		} else if (iValidShadowUnits > 0) {
			if (iValidShadowUnits == 1)
				setShadowUnit(pPlot->getCenterUnit());
			else
				setShadowUnit(GET_PLAYER(getOwnerINLINE()).getUnit(iFlags));

			return true;
		}
	}

	return false;
}

void CvUnit::awardSpyExperience(TeamTypes eTargetTeam, int iModifier) {
	int iDifficulty = (getSpyInterceptPercent(eTargetTeam, false) * (100 + iModifier)) / 100;
	if (iDifficulty < 1)
		changeExperience(1);
	else if (iDifficulty < 10)
		changeExperience(2);
	else if (iDifficulty < 25)
		changeExperience(3);
	else if (iDifficulty < 50)
		changeExperience(4);
	else if (iDifficulty < 75)
		changeExperience(5);
	else if (iDifficulty >= 75)
		changeExperience(6);

	testPromotionReady();
}

void CvUnit::setOriginalSpymaster(PlayerTypes ePlayer) {
	if (m_pSpy) m_pSpy->setOriginalSpymaster(ePlayer);
}

PlayerTypes CvUnit::getOriginalSpymaster() const {
	return m_pSpy ? m_pSpy->getOriginalSpymaster() : NO_PLAYER;
}

bool CvUnit::isDoubleAgent() const {
	PlayerTypes eOriginalSpymaster = getOriginalSpymaster();
	return eOriginalSpymaster != NO_PLAYER && eOriginalSpymaster != getOwnerINLINE() && GET_PLAYER(eOriginalSpymaster).isAlive();
}

bool CvUnit::isLoyal() const {
	return getLoyaltyCount() > 0;
}

void CvUnit::changeLoyaltyCount(int iChange) {
	m_iLoyaltyCount += iChange;
	FAssert(getLoyaltyCount() >= 0);
}

int CvUnit::getLoyaltyCount() const {
	return m_iLoyaltyCount;
}

// In anticpation of spies receiving modifiers to their evasion
int CvUnit::getSpyEvasionChance() const {
	return getSpyEvasionChanceExtra();
}

int CvUnit::getSpyEvasionChanceExtra() const {
	return m_pSpy ? m_pSpy->getEvasionChanceExtra() : 0;
}

void CvUnit::changeSpyEvasionChanceExtra(int iChange) {
	if (m_pSpy) m_pSpy->changeEvasionChanceExtra(iChange);
}

int CvUnit::getSpyPreparationModifier() const {
	return m_pSpy ? m_pSpy->getPreparationModifier() : 0;
}

void CvUnit::changeSpyPreparationModifier(int iChange) {
	if (m_pSpy) m_pSpy->changePreparationModifier(iChange);
}

int CvUnit::getSpyPoisonChangeExtra() const {
	return m_pSpy ? m_pSpy->getPoisonChangeExtra() : 0;
}

void CvUnit::changeSpyPoisonChangeExtra(int iChange) {
	if (m_pSpy) m_pSpy->changePoisonChangeExtra(iChange);
}

int CvUnit::getSpyDestroyImprovementChange() const {
	return m_pSpy ? m_pSpy->getDestroyImprovementChange() : 0;
}

void CvUnit::changeSpyDestroyImprovementChange(int iChange) {
	if (m_pSpy) m_pSpy->changeDestroyImprovementChange(iChange);
}

int CvUnit::getSpyRadiationCount() const {
	return m_pSpy ? m_pSpy->getRadiationCount() : 0;
}

bool CvUnit::isSpyRadiation() const {
	return (getSpyRadiationCount() > 0);
}

void CvUnit::changeSpyRadiationCount(int iChange) {
	if (m_pSpy) m_pSpy->changeRadiationCount(iChange);
}

int CvUnit::getSpyDiplomacyPenalty() const {
	return m_pSpy ? m_pSpy->getDiplomacyPenalty() : 0;
}

void CvUnit::changeSpyDiplomacyPenalty(int iChange) {
	if (m_pSpy) m_pSpy->changeDiplomacyPenalty(iChange);
}

int CvUnit::getSpyNukeCityChange() const {
	return m_pSpy ? m_pSpy->getNukeCityChange() : 0;
}

void CvUnit::changeSpyNukeCityChange(int iChange) {
	if (m_pSpy) m_pSpy->changeNukeCityChange(iChange);
}

bool CvUnit::spyNukeAffected(const CvPlot* pPlot, TeamTypes eTeam, int iRange) const {
	if (!(GET_TEAM(eTeam).isAlive())) {
		return false;
	}

	if (eTeam == getTeam()) {
		return false;
	}

	CvPlot* pLoopPlot;
	for (int iDX = -(iRange); iDX <= iRange; iDX++) {
		for (int iDY = -(iRange); iDY <= iRange; iDY++) {
			pLoopPlot = plotXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL) {
				if (pLoopPlot->getTeam() == eTeam) {
					return true;
				}

				if (pLoopPlot->plotCheck(PUF_isCombatTeam, eTeam, getTeam()) != NULL) {
					return true;
				}
			}
		}
	}

	return false;
}

bool CvUnit::spyNuke(int iX, int iY, bool bReveal) {
	CvPlot* pPlot = GC.getMapINLINE().plotINLINE(iX, iY);

	bool abTeamsAffected[MAX_TEAMS];
	for (TeamTypes eTeam = (TeamTypes)0; eTeam < MAX_TEAMS; eTeam = (TeamTypes)(eTeam + 1)) {
		abTeamsAffected[eTeam] = spyNukeAffected(pPlot, eTeam, 1);
	}

	if (bReveal) {
		for (TeamTypes eTeam = (TeamTypes)0; eTeam < MAX_TEAMS; eTeam = (TeamTypes)(eTeam + 1)) {
			if (abTeamsAffected[eTeam]) {
				if (!isEnemy(eTeam)) {
					if (!(eTeam == GET_TEAM(getTeam()).getID())) {
						GET_TEAM(getTeam()).declareWar(eTeam, false, WARPLAN_TOTAL);
					}
				}
				GET_TEAM(eTeam).changeWarWeariness(getTeam(), 100 * GC.getDefineINT("WW_HIT_BY_NUKE"));
				GET_TEAM(getTeam()).changeWarWeariness(eTeam, 100 * GC.getDefineINT("WW_ATTACKED_WITH_NUKE"));
				GET_TEAM(getTeam()).AI_changeWarSuccess(eTeam, GC.getDefineINT("WAR_SUCCESS_NUKE"));
			}
		}
	}

	if (bReveal) {
		for (TeamTypes eTeam = (TeamTypes)0; eTeam < MAX_TEAMS; eTeam = (TeamTypes)(eTeam + 1)) {
			if (GET_TEAM(eTeam).isAlive() && eTeam != getTeam() && abTeamsAffected[eTeam]) {
				for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
					CvPlayer& kPlayer = GET_PLAYER(ePlayer);
					if (kPlayer.isAlive() && kPlayer.getTeam() == eTeam) {
						kPlayer.AI_changeMemoryCount(getOwnerINLINE(), MEMORY_NUKED_US, 1);
					}
				}
			} else {
				for (TeamTypes eInnerTeam = (TeamTypes)0; eInnerTeam < MAX_TEAMS; eInnerTeam = (TeamTypes)(eInnerTeam + 1)) {
					if (GET_TEAM(eInnerTeam).isAlive() && abTeamsAffected[eInnerTeam] && GET_TEAM(eTeam).isHasMet(eInnerTeam) && GET_TEAM(eTeam).AI_getAttitude(eInnerTeam) >= ATTITUDE_CAUTIOUS) {
						for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
							CvPlayer& kPlayer = GET_PLAYER(ePlayer);
							if (kPlayer.isAlive() && kPlayer.getTeam() == eTeam) {
								kPlayer.AI_changeMemoryCount(getOwnerINLINE(), MEMORY_NUKED_FRIEND, 1);
							}
						}
						break;
					}
				}
			}
		}
	}

	for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
		if (GET_PLAYER(ePlayer).isAlive()) {
			CvWString szBuffer;
			if (bReveal)
				szBuffer = gDLL->getText("TXT_KEY_ESPIONAGE_NUKE_CAUGHT", GET_PLAYER(getOwnerINLINE()).getNameKey(), GET_PLAYER(pPlot->getOwnerINLINE()).getNameKey());
			else
				szBuffer = gDLL->getText("TXT_KEY_ESPIONAGE_NUKE_UNKNOWN", GET_PLAYER(pPlot->getOwnerINLINE()).getNameKey());

			gDLL->getInterfaceIFace()->addHumanMessage(ePlayer, true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_NUKE_EXPLODES", MESSAGE_TYPE_MAJOR_EVENT, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pPlot->getX_INLINE(), pPlot->getY_INLINE(), true, true);
		}
	}

	//This is just so the espionage mission makes the cool explosion effect.
	if (GC.getInfoTypeForString("EFFECT_ICBM_NUCLEAR_EXPLOSION") != -1) {
		gDLL->getEngineIFace()->TriggerEffect((EffectTypes)GC.getInfoTypeForString("EFFECT_ICBM_NUCLEAR_EXPLOSION"), pPlot->getPoint(), 0);
		gDLL->getInterfaceIFace()->playGeneralSound("AS2D_NUKE_EXPLODES", pPlot->getPoint());
	}
	pPlot->nukeExplosion(1, this);
	return true;
}

int CvUnit::getSpySwitchCivicChange() const {
	return m_pSpy ? m_pSpy->getSwitchCivicChange() : 0;
}

void CvUnit::changeSpySwitchCivicChange(int iChange) {
	if (m_pSpy) m_pSpy->changeSwitchCivicChange(iChange);
}

int CvUnit::getSpySwitchReligionChange() const {
	return m_pSpy ? m_pSpy->getSwitchReligionChange() : 0;
}

void CvUnit::changeSpySwitchReligionChange(int iChange) {
	if (m_pSpy) m_pSpy->changeSwitchReligionChange(iChange);
}

int CvUnit::getSpyDisablePowerChange() const {
	return m_pSpy ? m_pSpy->getDisablePowerChange() : 0;
}

void CvUnit::changeSpyDisablePowerChange(int iChange) {
	if (m_pSpy) m_pSpy->changeDisablePowerChange(iChange);
}

int CvUnit::getSpyEscapeChanceExtra() const {
	return m_pSpy ? m_pSpy->getEscapeChanceExtra() : 0;
}

void CvUnit::changeSpyEscapeChanceExtra(int iChange) {
	if (m_pSpy) m_pSpy->changeEscapeChanceExtra(iChange);
}

int CvUnit::getSpyEscapeChance() const {
	return getSpyEscapeChanceExtra();
}

int CvUnit::getSpyInterceptChanceExtra() const {
	return m_pSpy ? m_pSpy->getInterceptChanceExtra() : 0;
}

void CvUnit::changeSpyInterceptChanceExtra(int iChange) {
	if (m_pSpy) m_pSpy->changeInterceptChanceExtra(iChange);
}

int CvUnit::getSpyInterceptChance() const {
	return getSpyInterceptChanceExtra();
}

int CvUnit::getSpyUnhappyChange() const {
	return m_pSpy ? m_pSpy->getUnhappyChange() : 0;
}

void CvUnit::changeSpyUnhappyChange(int iChange) {
	if (m_pSpy) m_pSpy->changeUnhappyChange(iChange);
}

int CvUnit::getSpyRevoltChange() const {
	return m_pSpy ? m_pSpy->getRevoltChange() : 0;
}

void CvUnit::changeSpyRevoltChange(int iChange) {
	if (m_pSpy) m_pSpy->changeRevoltChange(iChange);
}

int CvUnit::getSpyWarWearinessChange() const {
	return m_pSpy ? m_pSpy->getWarWearinessChange() : 0;
}

void CvUnit::changeSpyWarWearinessChange(int iChange) {
	if (m_pSpy) m_pSpy->changeWarWearinessChange(iChange);
}

int CvUnit::getSpyReligionRemovalChange() const {
	return m_pSpy ? m_pSpy->getReligionRemovalChange() : 0;
}

void CvUnit::changeSpyReligionRemovalChange(int iChange) {
	if (m_pSpy) m_pSpy->changeReligionRemovalChange(iChange);
}

int CvUnit::getSpyCorporationRemovalChange() const {
	return m_pSpy ? m_pSpy->getCorporationRemovalChange() : 0;
}

void CvUnit::changeSpyCorporationRemovalChange(int iChange) {
	if (m_pSpy) m_pSpy->changeCorporationRemovalChange(iChange);
}

int CvUnit::getSpyCultureChange() const {
	return m_pSpy ? m_pSpy->getCultureChange() : 0;
}

void CvUnit::changeSpyCultureChange(int iChange) {
	if (m_pSpy) m_pSpy->changeCultureChange(iChange);
}

bool CvUnit::canAssassinate(const CvPlot* pPlot, SpecialistTypes eSpecialist, bool bTestVisible) const {

	if (!canEspionage(pPlot, bTestVisible))
		return false;

	CvCity* pCity = pPlot->getPlotCity();
	if (!pCity)
		return false;

	if (pCity->getFreeSpecialistCount(eSpecialist) <= 0)
		return false;

	if (eSpecialist != NO_SPECIALIST) {
		CvSpecialistInfo& kSpecialist = GC.getSpecialistInfo(eSpecialist);
		if (kSpecialist.isSlave() || kSpecialist.getGreatPeopleRateChange() > 0) {
			return false;
		}
	}

	return true;
}

int CvUnit::getSpyResearchSabotageChange() const {
	return  m_pSpy ? m_pSpy->getResearchSabotageChange() : 0;
}

void CvUnit::changeSpyResearchSabotageChange(int iChange) {
	if (m_pSpy) m_pSpy->changeResearchSabotageChange(iChange);
}

int CvUnit::getSpyDestroyProjectChange() const {
	return  m_pSpy ? m_pSpy->getDestroyProjectChange() : 0;
}

void CvUnit::changeSpyDestroyProjectChange(int iChange) {
	if (m_pSpy) m_pSpy->changeDestroyProjectChange(iChange);
}

int CvUnit::getSpyDestroyBuildingChange() const {
	return  m_pSpy ? m_pSpy->getDestroyBuildingChange() : 0;
}

void CvUnit::changeSpyDestroyBuildingChange(int iChange) {
	if (m_pSpy) m_pSpy->changeDestroyBuildingChange(iChange);
}

int CvUnit::getSpyDestroyProductionChange() const {
	return  m_pSpy ? m_pSpy->getDestroyProductionChange() : 0;
}

void CvUnit::changeSpyDestroyProductionChange(int iChange) {
	if (m_pSpy) m_pSpy->changeDestroyProductionChange(iChange);
}

int CvUnit::getSpyBuyTechChange() const {
	return  m_pSpy ? m_pSpy->getBuyTechChange() : 0;
}

void CvUnit::changeSpyBuyTechChange(int iChange) {
	if (m_pSpy) m_pSpy->changeBuyTechChange(iChange);
}

int CvUnit::getSpyStealTreasuryChange() const {
	return  m_pSpy ? m_pSpy->getStealTreasuryChange() : 0;
}

void CvUnit::changeSpyStealTreasuryChange(int iChange) {
	if (m_pSpy) m_pSpy->changeStealTreasuryChange(iChange);
}

CvCity* CvUnit::getClosestSafeCity() const {
	// Check for a city on the same landmass
	CvCity* pReturnCity = GC.getMapINLINE().findCity(getX_INLINE(), getY_INLINE(), NO_PLAYER, getTeam(), true, false);

	if (!pReturnCity) // Check for a coastal city on another landmass
		pReturnCity = GC.getMapINLINE().findCity(getX_INLINE(), getY_INLINE(), NO_PLAYER, getTeam(), false, true);

	if (!pReturnCity) // Check for any city on another landmass
		pReturnCity = GC.getMapINLINE().findCity(getX_INLINE(), getY_INLINE(), NO_PLAYER, getTeam(), false, false);

	if (!pReturnCity) // Otherwise use the capital - We should never get here as the capital should have been selected previously
		pReturnCity = GET_PLAYER(getOwnerINLINE()).getCapitalCity();

	return pReturnCity;
}

// unit influences combat area after victory 
// returns influence % in defended plot
float CvUnit::doVictoryInfluence(CvUnit* pLoserUnit, bool bAttacking, bool bWithdrawal) {

	if (isAnimal() || pLoserUnit->isAnimal())
		return 0.0f;

	if (isAlwaysHostile(plot()) || pLoserUnit->isAlwaysHostile(pLoserUnit->plot()))
		return 0.0f;

	if ((isBarbarian() || pLoserUnit->isBarbarian()) && !GC.getIDW_BARBARIAN_INFLUENCE_ENABLED())
		return 0.0f;

	if ((DOMAIN_SEA == getDomainType()) && !GC.getIDW_NAVAL_INFLUENCE_ENABLED())
		return 0.0f;

	CvPlot* pWinnerPlot = plot();
	CvPlot* pLoserPlot = pLoserUnit->plot();
	CvPlot* pDefenderPlot = bAttacking ? pLoserPlot : pWinnerPlot;
	int iWinnerCultureBefore = pDefenderPlot->getCulture(getOwnerINLINE()); //used later for influence %

	float fWinnerPlotMultiplier = GC.getIDW_WINNER_PLOT_MULTIPLIER(); // default 1.0 : same influence in WinnerPlot and LoserPlot
	float fLoserPlotMultiplier = GC.getIDW_LOSER_PLOT_MULTIPLIER(); // by default 1.0 : same influence in WinnerPlot and LoserPlot
	float bWithdrawalMultiplier = 0.5f;
	if (bWithdrawal) {
		fWinnerPlotMultiplier *= bWithdrawalMultiplier;
		fLoserPlotMultiplier *= bWithdrawalMultiplier;
	}

	if (pLoserPlot->isEnemyCity(*this)) // city combat 
	{
		if (pLoserPlot->getNumVisibleEnemyDefenders(this) > 1) {
			// if there are still some city defenders ->
			// we use same influence rules as for field combat
			influencePlots(pLoserPlot, pLoserUnit->getOwnerINLINE(), fLoserPlotMultiplier);
			influencePlots(pWinnerPlot, pLoserUnit->getOwnerINLINE(), fWinnerPlotMultiplier);
		} else // last defender is dead
		{
			// last city defender is dead -> influence is increased
			influencePlots(pLoserPlot, pLoserUnit->getOwnerINLINE(), fLoserPlotMultiplier * GC.getIDW_NO_CITY_DEFENDER_MULTIPLIER());
			influencePlots(pWinnerPlot, pLoserUnit->getOwnerINLINE(), fWinnerPlotMultiplier * GC.getIDW_NO_CITY_DEFENDER_MULTIPLIER());

			if (GC.getIDW_EMERGENCY_DRAFT_ENABLED()) {
				int iDefenderCulture = pLoserPlot->getCulture(pLoserUnit->getOwnerINLINE());
				int iAttackerCulture = pLoserPlot->getCulture(getOwnerINLINE());

				if (iDefenderCulture >= iAttackerCulture) {
					// if defender culture in city's central tile is still higher then atacker culture 
					// -> city is not captured yet but emergency militia is drafted
					pLoserPlot->getPlotCity()->emergencyConscript();

					// calculate city resistance % (to be displayed in game log)
					float fResistence = ((iDefenderCulture - iAttackerCulture) * 100.0f) / (2 * pDefenderPlot->countTotalCulture());

					CvWString szBuffer;
					szBuffer.Format(L"City militia has emerged! Resistance: %.1f%%", fResistence);
					gDLL->getInterfaceIFace()->addMessage(pLoserUnit->getOwnerINLINE(), false, GC.getDefineINT("EVENT_MESSAGE_TIME"), szBuffer, "AS2D_UNIT_BUILD_UNIT", MESSAGE_TYPE_INFO, GC.getUnitInfo(getUnitType()).getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pLoserPlot->getX_INLINE(), pLoserPlot->getY_INLINE(), true, true);
					gDLL->getInterfaceIFace()->addMessage(getOwnerINLINE(), true, GC.getDefineINT("EVENT_MESSAGE_TIME"), szBuffer, "AS2D_UNIT_BUILD_UNIT", MESSAGE_TYPE_INFO, GC.getUnitInfo(getUnitType()).getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pLoserPlot->getX_INLINE(), pLoserPlot->getY_INLINE());

				}
			}
		}
	} else // field combat
	{
		if (!pLoserUnit->canDefend()) {
			// no influence from worker capture
			return 0.0f;
		}

		if (pLoserPlot->getImprovementType() != NO_IMPROVEMENT
			&& GC.getImprovementInfo(pLoserPlot->getImprovementType()).getDefenseModifier() > 0
			&& pLoserPlot->getNumVisibleEnemyDefenders(this) > 1) {
			// fighting a defensive fortification
			influencePlots(pLoserPlot, pLoserUnit->getOwnerINLINE(), fLoserPlotMultiplier * GC.getIDW_FORT_CAPTURE_MULTIPLIER());
			influencePlots(pWinnerPlot, pLoserUnit->getOwnerINLINE(), fWinnerPlotMultiplier * GC.getIDW_FORT_CAPTURE_MULTIPLIER());

		} else {
			influencePlots(pLoserPlot, pLoserUnit->getOwnerINLINE(), fLoserPlotMultiplier);
			influencePlots(pWinnerPlot, pLoserUnit->getOwnerINLINE(), fWinnerPlotMultiplier);
		}
	}

	// calculate influence % in defended plot (to be displayed in game log)

	int iWinnerCultureAfter = pDefenderPlot->getCulture(getOwnerINLINE());
	int iTotalCulture = pDefenderPlot->countTotalCulture();
	float fInfluenceRatio = iTotalCulture <= 0 ? 0.0f : ((iWinnerCultureAfter - iWinnerCultureBefore) * 100.0f) / iTotalCulture;

	return fInfluenceRatio;
}

// unit influences given plot and surounding area i.e. transfers culture from target civ to unit's owner
void CvUnit::influencePlots(CvPlot* pCentralPlot, PlayerTypes eTargetPlayer, float fLocationMultiplier) {
	// calculate base multiplier used for all plots
	float fGameSpeedMultiplier = (float)GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getConstructPercent();
	fGameSpeedMultiplier /= 100;
	fGameSpeedMultiplier *= GC.getEraInfo(GC.getGameINLINE().getStartEra()).getConstructPercent();
	fGameSpeedMultiplier /= 100;
	fGameSpeedMultiplier = sqrt(fGameSpeedMultiplier);

	// each point of experience increases influence by 1%
	float fExperienceMultiplier = 1.0f + (getExperience() * GC.getIDW_EXPERIENCE_FACTOR());
	float fWarlordMultiplier = NO_UNIT == getLeaderUnitType() ? 1.0f : GC.getIDW_WARLORD_MULTIPLIER(); // default: +50%
	float fBaseMultiplier = GC.getIDW_BASE_COMBAT_INFLUENCE() * fGameSpeedMultiplier * fLocationMultiplier * fExperienceMultiplier * fWarlordMultiplier;
	if (fBaseMultiplier <= 0.0f)
		return;

	// get influence radius
	int iInfluenceRadius = GC.getIDW_INFLUENCE_RADIUS(); // like 2 square city workable radius
	if (iInfluenceRadius < 0)
		return;

	float fPlotDistanceFactor = GC.getIDW_PLOT_DISTANCE_FACTOR(); // default 0.2: influence decreases by 20% with plot distance
	for (int iDX = -iInfluenceRadius; iDX <= iInfluenceRadius; iDX++) {
		for (int iDY = -iInfluenceRadius; iDY <= iInfluenceRadius; iDY++) {
			int iDistance = plotDistance(0, 0, iDX, iDY);

			if (iDistance <= iInfluenceRadius) {
				CvPlot* pLoopPlot = plotXY(pCentralPlot->getX_INLINE(), pCentralPlot->getY_INLINE(), iDX, iDY);

				if (pLoopPlot != NULL) {
					// calculate distance multiplier for current plot
					float fDistanceMultiplier = 0.5f + 0.5f * fPlotDistanceFactor - fPlotDistanceFactor * iDistance;
					if (fDistanceMultiplier <= 0.0f)
						continue;
					int iTargetCulture = pLoopPlot->getCulture(eTargetPlayer);
					if (iTargetCulture <= 0)
						continue;
					int iCultureTransfer = int(fBaseMultiplier * fDistanceMultiplier * sqrt((float)iTargetCulture));
					if (iTargetCulture < iCultureTransfer) {
						// cannot transfer more culture than remaining target culture
						iCultureTransfer = iTargetCulture;
					}
					if (iCultureTransfer == 0 && iTargetCulture > 0) {
						// always at least 1 point of culture must be transfered
						// othervise we may have the problems with capturing of very low culture cities. 
						iCultureTransfer = 1;
					}

					if (iCultureTransfer > 0) {
						// target player's culture in plot is lowered
						pLoopPlot->changeCulture(eTargetPlayer, -iCultureTransfer, false);
						// unit owners's culture in plot is raised
						pLoopPlot->changeCulture(getOwnerINLINE(), iCultureTransfer, true);
					}
				}
			}
		}
	}
}

// unit influences current tile via pillaging 
// returns influence % in current plot
float CvUnit::doPillageInfluence() {
	return doInfluenceCulture(GC.getIDW_BASE_PILLAGE_INFLUENCE(), plot()->getOwner());
}

// unit influences current tile
// returns influence % in current plot
float CvUnit::doInfluenceCulture(float fInfluence, PlayerTypes eTargetPlayer) {
	if (isBarbarian() && !GC.getIDW_BARBARIAN_INFLUENCE_ENABLED())
		return 0.0f;

	if ((DOMAIN_SEA == getDomainType()) && !GC.getIDW_NAVAL_INFLUENCE_ENABLED())
		return 0.0f;

	CvPlot* pPlot = plot();
	if (pPlot == NULL) //should not happen
		return 0.0f;

	int iOurCultureBefore = pPlot->getCulture(getOwnerINLINE()); //used later for influence %

	float fGameSpeedMultiplier = (float)GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getConstructPercent();
	fGameSpeedMultiplier /= 100;
	fGameSpeedMultiplier *= GC.getEraInfo(GC.getGameINLINE().getStartEra()).getConstructPercent();
	fGameSpeedMultiplier /= 100;
	fGameSpeedMultiplier = sqrt(fGameSpeedMultiplier);

	int iTargetCulture = pPlot->getCulture(eTargetPlayer);
	if (iTargetCulture <= 0) {
		//should not happen
		return 0.0f;
	}
	int iCultureTransfer = int(fInfluence * fGameSpeedMultiplier * sqrt((float)iTargetCulture));
	if (iTargetCulture < iCultureTransfer) {
		// cannot transfer more culture than remaining target culture
		iCultureTransfer = iTargetCulture;
	}

	// target player's culture in plot is lowered
	pPlot->changeCulture(eTargetPlayer, -iCultureTransfer, false);
	// owners's culture in plot is raised
	pPlot->changeCulture(getOwnerINLINE(), iCultureTransfer, true);

	// calculate influence % in pillaged plot (to be displayed in game log)
	int iOurCultureAfter = pPlot->getCulture(getOwnerINLINE());
	float fInfluenceRatio = ((iOurCultureAfter - iOurCultureBefore) * 100.0f) / pPlot->countTotalCulture();

	return fInfluenceRatio;
}

void CvUnit::setDesiredDiscoveryTech(TechTypes eTech) {
	m_eDesiredDiscoveryTech = eTech;

	getGroup()->setActivityType(ACTIVITY_SLEEP);
}

TechTypes CvUnit::getDesiredDiscoveryTech() const {
	return m_eDesiredDiscoveryTech;
}

void CvUnit::waitForTech(int iFlag, int eTech) {
	if (eTech == NO_TECH) {
		CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_SELECT_DISCOVERY_TECH, getID(), 0, 0);
		if (pInfo) {
			gDLL->getInterfaceIFace()->addPopup(pInfo, getOwnerINLINE(), true);
		}
	} else {
		setDesiredDiscoveryTech((TechTypes)eTech);
	}
}

bool CvUnit::isAutoPromoting() const {
	return m_bAutoPromoting;
}

void CvUnit::setAutoPromoting(bool bNewValue) {
	m_bAutoPromoting = bNewValue;
	if (bNewValue) {
		//Force recalculation
		setPromotionReady(false);
		testPromotionReady();
	}
}

bool CvUnit::isAutoUpgrading() const {
	return m_bAutoUpgrading;
}

void CvUnit::setAutoUpgrading(bool bNewValue) {
	m_bAutoUpgrading = bNewValue;
}

int CvUnit::getWorkRateModifier() const {
	return m_iWorkRateModifier;
}

void CvUnit::changeWorkRateModifier(int iChange) {
	m_iWorkRateModifier += iChange;
}

bool CvUnit::isBuildLeaveFeature(BuildTypes eBuild, FeatureTypes eFeature) const {
	std::map<BuildTypes, std::map< FeatureTypes, int> >::const_iterator itBuild = m_mmBuildLeavesFeatures.find(eBuild);
	if (itBuild != m_mmBuildLeavesFeatures.end()) {
		std::map< FeatureTypes, int>::const_iterator itFeature = (itBuild->second).find(eFeature);
		if (itFeature != (itBuild->second).end()) {
			return (itFeature->second > 0);
		}
	}
	return false;
}

void CvUnit::changeBuildLeaveFeatureCount(BuildTypes eBuild, FeatureTypes eFeature, int iCount) {
	// Check if we have any existing values for builds
	std::map<BuildTypes, std::map< FeatureTypes, int> >::iterator itBuild = m_mmBuildLeavesFeatures.find(eBuild);
	if (itBuild == m_mmBuildLeavesFeatures.end()) {
		// We have no existing mapping for this build type so we have to set the count
		m_mmBuildLeavesFeatures[eBuild][eFeature] = iCount;
	} else {
		// We have some existing features for the build so check if we have the feature we are looking for
		std::map< FeatureTypes, int>::iterator itFeature = (itBuild->second).find(eFeature);
		if (itFeature == (itBuild->second).end()) {
			// We do not have this feature in the map so we can set the count
			m_mmBuildLeavesFeatures[eBuild][eFeature] = iCount;
		} else {
			// We have the feature so we need to modify the count
			m_mmBuildLeavesFeatures[eBuild][eFeature] = itFeature->second + iCount;
		}
	}
}

void CvUnit::setImmobile(bool bImmobile) {
	m_bImmobile = bImmobile;
}

bool CvUnit::isImmobile() const {
	return m_bImmobile || getImmobileTimer() > 0;
}

void CvUnit::becomeSlaver() {
	float fNewCombat = (float)(m_iBaseCombat * 90);
	fNewCombat /= 100;
	int iNewCombat = (int)floor(fNewCombat);

	setName(gDLL->getText("TXT_KEY_SLAVER_RENAME", getName().GetCString()));
	setBaseCombatStr(std::max(0, iNewCombat));
	setUnitCombatType((UnitCombatTypes)GC.getInfoTypeForString("UNITCOMBAT_SLAVER", true));
	setInvisibleType((InvisibleTypes)GC.getInfoTypeForString("INVISIBLE_SLAVER", true));
	AI_setUnitAIType(UNITAI_SLAVER);
	// Units can manage 1 slave for every 8 combat or part thereof
	float fSlaves = fNewCombat * 12.5f;
	fSlaves /= 100;
	setMaxSlaves((int)ceil(fSlaves));
	setFixedAI(true);
	setHiddenNationality(true);
	setAlwaysHostile(true);
	setSlaverGraphics();
	reloadEntity();
}

bool CvUnit::canBecomeSlaver() const {

	if (isSpecialistUnit())
		return false;

	if (!GET_PLAYER(getOwnerINLINE()).isWorldViewActivated(WORLD_VIEW_SLAVERY))
		return false;

	if (!canAttack())
		return false;

	if (m_iBaseCombat < 2)
		return false;

	if (isUnitCombatType((UnitCombatTypes)GC.getInfoTypeForString("UNITCOMBAT_SLAVER", true)))
		return false;

	return true;
}

void CvUnit::setFixedAI(bool bFixed) {
	m_bFixedAI = bFixed;
}

bool CvUnit::isFixedAI() const {
	return m_bFixedAI;
}

void CvUnit::setHiddenNationality(bool bHidden) {
	m_bHiddenNationality = bHidden;
}

bool CvUnit::isHiddenNationality() const {
	return m_bHiddenNationality;
}

void CvUnit::setAlwaysHostile(bool bHostile) {
	m_bAlwaysHostile = bHostile;
}

bool CvUnit::isAlwaysHostile() const {
	return m_bAlwaysHostile;
}

void CvUnit::setSlaverGraphics() {
	if (!GC.getGameINLINE().isSlaverUnitMeshGroupExists(getUnitType())) {
		GC.getGameINLINE().addSlaverUnitMeshGroup(getUnitType());
	}
	m_pCustomUnitMeshGroup = GC.getGameINLINE().getSlaverUnitMeshGroup(getUnitType());
}

bool CvUnit::isSlaver() const {
	return isUnitCombatType((UnitCombatTypes)GC.getInfoTypeForString("UNITCOMBAT_SLAVER", true));
}

int CvUnit::getMeleeWaveSize() const {
	return m_pCustomUnitMeshGroup ? m_pCustomUnitMeshGroup->getMeleeWaveSize() : m_pUnitInfo->getMeleeWaveSize();
}

int CvUnit::getRangedWaveSize() const {
	return m_pCustomUnitMeshGroup ? m_pCustomUnitMeshGroup->getRangedWaveSize() : m_pUnitInfo->getRangedWaveSize();
}

void CvUnit::changeSeeInvisibleCount(InvisibleTypes eInvisibleType, int iChange) {
	FAssertMsg(eInvisibleType >= 0, "iIndex expected to be >= 0");
	FAssertMsg(eInvisibleType < GC.getNumInvisibleInfos(), "eInvisibleType expected to be < GC.getNumInvisibleInfos()");

	m_paiSeeInvisibleCount[eInvisibleType] = m_paiSeeInvisibleCount[eInvisibleType] + iChange;
}

bool CvUnit::isSeeInvisible(InvisibleTypes eInvisibleType) const {
	return m_paiSeeInvisibleCount[eInvisibleType] > 0;
}

void CvUnit::setInvisibleType(InvisibleTypes eInvisible) {
	m_eInvisible = eInvisible;
}

void CvUnit::setWeaponType(WeaponTypes eWeapon) {
	m_eWeaponType = eWeapon;
	m_iWeaponStrength = m_eWeaponType != NO_WEAPON ? GC.getWeaponInfo(m_eWeaponType).getStrength() : 0;
}

int CvUnit::getWeaponStrength() const {
	return m_iWeaponStrength;
}

WeaponTypes CvUnit::getWeaponType() const {
	return m_eWeaponType;
}

void CvUnit::setAmmunitionType(WeaponTypes eAmmunition) {
	m_eAmmunitionType = eAmmunition;
	m_iAmmunitionStrength = m_eAmmunitionType != NO_WEAPON ? GC.getWeaponInfo(m_eAmmunitionType).getStrength() : 0;
}

int CvUnit::getAmmunitionStrength() const {
	return m_iAmmunitionStrength;
}

WeaponTypes CvUnit::getAmmunitionType() const {
	return m_eAmmunitionType;
}

void CvUnit::changeSalvageModifier(int iChange) {
	m_iSalvageModifier += iChange;
}

int CvUnit::getSalvageModifier() const {
	return m_iSalvageModifier;
}

// Description: Barbarian Leader's free unit support mission ...
bool CvUnit::canIncreaseBarbarianUnitSupport(const CvPlot* pPlot, bool bTestVisible) const {

	CvCity* pCity = pPlot->getPlotCity();
	if (!(m_pUnitInfo->isBarbarianLeader())) {
		return false;
	}

	if (pCity == NULL) {
		return false;
	}

	if (pCity->getOwnerINLINE() != getOwnerINLINE()) {
		return false;
	}

	return true;
}

bool CvUnit::increaseBarbarianUnitSupport(int iFreeUnitsSupport) {

	CvCity* pCity = plot()->getPlotCity();
	FAssertMsg(pCity != NULL, "City is not assigned a valid value");

	if (!canIncreaseBarbarianUnitSupport(plot())) {
		return false;
	}

	GET_PLAYER(getOwnerINLINE()).changeBaseFreeUnits(GC.getDefineINT("MISSION_REINFORCE_MILITARY_FREE_SUPPORT"));

	CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_FREE_UNIT_SUPPORT", getNameKey());
	gDLL->getInterfaceIFace()->addMessage(getOwnerINLINE(), false, GC.getDefineINT("EVENT_MESSAGE_TIME"), szBuffer, "AS2D_UNITGIFTED", MESSAGE_TYPE_INFO, GC.getUnitInfo(getUnitType()).getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pCity->getX_INLINE(), pCity->getY_INLINE());

	if (plot()->isActiveVisible(false)) {
		NotifyEntity(MISSION_FREE_UNIT_SUPPORT);
	}

	kill(true);

	return true;
}

// Description: Barbarian Leader's free unit support mission ...
bool CvUnit::canPlunderCity(const CvPlot* pPlot) const {

	CvCity* pCity = pPlot->getPlotCity();
	if (!m_pUnitInfo->isBarbarianLeader()) {
		return false;
	}

	if (pCity == NULL) {
		return false;
	}

	if (pCity->getOwnerINLINE() == getOwnerINLINE()) {
		return false;
	}

	return true;
}

bool CvUnit::plunderCity() {
	CvCity* pCity = plot()->getPlotCity();
	FAssertMsg(pCity != NULL, "City is not assigned a valid value");

	if (!canPlunderCity(plot())) {
		return false;
	}

	// First: Reduce city's population ...
	int killedPeople;
	if (pCity->getPopulation() < 2) {
		killedPeople = 0;
	} else if (pCity->getPopulation() < 5) {
		killedPeople = 1;
	} else {
		killedPeople = 2;
	}

	if (pCity->getPopulation() > killedPeople && killedPeople > 0) {
		pCity->changePopulation(-killedPeople);
	}

	// Second: Steal food and destroy production ...
	pCity->setFood(0);
	pCity->setProduction(0);

	CvPlayer& kOwner = GET_PLAYER(getOwnerINLINE());
	CvPlayer& kCityOwner = GET_PLAYER(pCity->getOwnerINLINE());
	// Third: Steal gold ...
	int iMaxGoldStolen = GC.getDefineINT("MAX_GOLD_STOLEN_BARBARIAN_LEADER");
	if (kCityOwner.getGold() > iMaxGoldStolen) {
		kOwner.changeGold(iMaxGoldStolen);
		kCityOwner.changeGold(-iMaxGoldStolen);
	} else {
		kOwner.changeGold(kCityOwner.getGold());
		kCityOwner.setGold(0);
	}

	// Fourth: Start city revolt ...
	pCity->changeCultureUpdateTimer(GC.getDefineINT("BASE_REVOLT_OCCUPATION_TURNS") + 1);
	pCity->changeOccupationTimer(GC.getDefineINT("BASE_REVOLT_OCCUPATION_TURNS") + 2);

	// Fifth: Reduce culture in city ...
	int iReducedCulture = (pCity->getCulture(pCity->getOwnerINLINE())) / 4;
	pCity->changeCulture(pCity->getOwnerINLINE(), -iReducedCulture, true, true);

	// Sixth: Capture citizens ...
	if (killedPeople > 0) {
		UnitTypes eGatherer = (UnitTypes)GC.getInfoTypeForString("UNIT_GATHERER");
		if (isHuman()) {
			if (kOwner.isCivSettled()) {
				kOwner.createNumUnits(getSlaveUnit(), killedPeople, kOwner.getCapitalCity()->getX(), kOwner.getCapitalCity()->getY());
			} else {
				kOwner.createNumUnits(eGatherer, killedPeople, kOwner.getCapitalCity()->getX(), kOwner.getCapitalCity()->getY());
			}
		} else {
			kOwner.createNumUnits(getSlaveUnit(), killedPeople, kOwner.getCapitalCity()->getX(), kOwner.getCapitalCity()->getY());
		}
	}

	CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_PLUNDER_CITY_01", getNameKey(), pCity->getNameKey());
	gDLL->getInterfaceIFace()->addMessage(getOwnerINLINE(), false, GC.getDefineINT("EVENT_MESSAGE_TIME"), szBuffer, "AS2D_PLUNDER_CITY", MESSAGE_TYPE_INFO, ARTFILEMGR.getInterfaceArtInfo("INTERFACE_PLUNDER_SHOW")->getPath(), (ColorTypes)GC.getInfoTypeForString("COLOR_GREEN"), pCity->getX(), pCity->getY(), true, true);

	for (PlayerTypes ePlayer = (PlayerTypes)0; ePlayer < MAX_CIV_PLAYERS; ePlayer = (PlayerTypes)(ePlayer + 1)) {
		const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
		if (kPlayer.isAlive()) {
			if (ePlayer != getOwnerINLINE() && ePlayer != pCity->getOwnerINLINE()) {
				if (GET_TEAM(kPlayer.getTeam()).isHasMet(pCity->getTeam())) {
					if (pCity->isRevealed(kPlayer.getTeam(), false)) {
						szBuffer = gDLL->getText("TXT_KEY_MISC_NEIGHBOR_CITY_PLUNDERED", kCityOwner.getCivilizationAdjective(), pCity->getNameKey());
						gDLL->getInterfaceIFace()->addMessage(ePlayer, false, GC.getDefineINT("EVENT_MESSAGE_TIME"), szBuffer, "AS2D_PLUNDER_CITY", MESSAGE_TYPE_INFO, ARTFILEMGR.getInterfaceArtInfo("INTERFACE_PLUNDER_SHOW")->getPath(), (ColorTypes)GC.getInfoTypeForString("COLOR_WHITE"), pCity->getX(), pCity->getY(), true, true);
					} else {
						szBuffer = gDLL->getText("TXT_KEY_MISC_NEIGHBOR_CITY_PLUNDERED", kCityOwner.getCivilizationAdjective(), pCity->getNameKey());
						gDLL->getInterfaceIFace()->addMessage(ePlayer, false, GC.getDefineINT("EVENT_MESSAGE_TIME"), szBuffer, "AS2D_PLUNDER_CITY", MESSAGE_TYPE_INFO, ARTFILEMGR.getInterfaceArtInfo("INTERFACE_PLUNDER_SHOW")->getPath(), (ColorTypes)GC.getInfoTypeForString("COLOR_WHITE"), NULL, NULL);
					}
				}
			}
		}
	}

	szBuffer = gDLL->getText("TXT_KEY_MISC_PLUNDER_CITY_PLUNDERED", pCity->getNameKey());
	gDLL->getInterfaceIFace()->addMessage(pCity->getOwnerINLINE(), false, GC.getDefineINT("EVENT_MESSAGE_TIME"), szBuffer, "AS2D_PLUNDER_CITY", MESSAGE_TYPE_INFO, ARTFILEMGR.getInterfaceArtInfo("INTERFACE_PLUNDER_SHOW")->getPath(), (ColorTypes)GC.getInfoTypeForString("COLOR_RED"), pCity->getX(), pCity->getY(), true, true);

	if (plot()->isActiveVisible(false)) {
		NotifyEntity(MISSION_PLUNDER_CITY);
	}

	kill(true);

	return true;
}

int CvUnit::becomeBarbarianCost() const {
	int iCost = GC.getDefineINT("BASE_BARBARIAN_CONVERTION_COST") * GC.getGameSpeedInfo(GC.getGameINLINE().getGameSpeedType()).getTrainPercent() / 100;

	iCost *= GET_PLAYER(getOwnerINLINE()).getBarbarianConvertionCostModifier() + 100;
	iCost /= 100;

	return iCost;
}

bool CvUnit::canBecomeBarbarian() const {

	if (isSpecialistUnit())
		return false;

	const CvPlayer& kOwner = GET_PLAYER(getOwnerINLINE());
	if (!kOwner.isCreateBarbarians())
		return false;

	if (kOwner.getGold() < becomeBarbarianCost())
		return false;

	return true;
}

void CvUnit::becomeBarbarian() {

	if (!canBecomeBarbarian())
		return;

	setName(gDLL->getText("TXT_KEY_BARBARIAN_RENAME", getName().GetCString()));
	setUnitCombatType((UnitCombatTypes)GC.getInfoTypeForString("UNITCOMBAT_BARBARIAN", true));
	AI_setUnitAIType(UNITAI_BARBARIAN);
	setFixedAI(true);
	setHiddenNationality(true);
	setAlwaysHostile(true);
	setBarbarianGraphics();
	reloadEntity();

	GET_PLAYER(getOwnerINLINE()).changeGold(-becomeBarbarianCost());
}

void CvUnit::setBarbarianGraphics() {
	if (!GC.getGameINLINE().isBarbarianUnitMeshGroupExists(getUnitType())) {
		GC.getGameINLINE().addBarbarianUnitMeshGroup(getUnitType());
	}
	m_pCustomUnitMeshGroup = GC.getGameINLINE().getBarbarianUnitMeshGroup(getUnitType());
}

bool CvUnit::isBarbarianConvert() const {
	return isUnitCombatType((UnitCombatTypes)GC.getInfoTypeForString("UNITCOMBAT_BARBARIAN", true));
}

bool CvUnit::isSpecialistUnit() const {
	return isSpy() || isSlaver() || isBarbarianConvert();
}

