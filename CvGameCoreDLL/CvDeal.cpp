// CvDeal.cpp

#include "CvGameCoreDLL.h"
#include "CvGlobals.h"
#include "CvGameAI.h"
#include "CvTeamAI.h"
#include "CvPlayerAI.h"
#include "CvMap.h"
#include "CvPlot.h"
#include "CvGameCoreUtils.h"
#include "CvGameTextMgr.h"
#include "CvDLLInterfaceIFaceBase.h"
#include "CvEventReporter.h"

#include "BetterBTSAI.h"

// Public Functions...

CvDeal::CvDeal() {
	reset();
}


CvDeal::~CvDeal() {
	uninit();
}


void CvDeal::init(int iID, PlayerTypes eFirstPlayer, PlayerTypes eSecondPlayer) {
	//--------------------------------
	// Init saved data
	reset(iID, eFirstPlayer, eSecondPlayer);

	//--------------------------------
	// Init non-saved data

	//--------------------------------
	// Init other game data
	setInitialGameTurn(GC.getGameINLINE().getGameTurn());
}


void CvDeal::uninit() {
	m_firstTrades.clear();
	m_secondTrades.clear();
}


// FUNCTION: reset()
// Initializes data members that are serialized.
void CvDeal::reset(int iID, PlayerTypes eFirstPlayer, PlayerTypes eSecondPlayer) {
	//--------------------------------
	// Uninit class
	uninit();

	m_iID = iID;
	m_iInitialGameTurn = 0;

	m_eFirstPlayer = eFirstPlayer;
	m_eSecondPlayer = eSecondPlayer;
}


void CvDeal::kill(bool bKillTeam) {
	if ((getLengthFirstTrades() > 0) || (getLengthSecondTrades() > 0)) {
		CvWString szString;
		CvWStringBuffer szDealString;
		CvWString szCancelString = gDLL->getText("TXT_KEY_POPUP_DEAL_CANCEL");

		if (GET_TEAM(GET_PLAYER(getFirstPlayer()).getTeam()).isHasMet(GET_PLAYER(getSecondPlayer()).getTeam())) {
			szDealString.clear();
			GAMETEXT.getDealString(szDealString, *this, getFirstPlayer());
			szString.Format(L"%s: %s", szCancelString.GetCString(), szDealString.getCString());
			gDLL->getInterfaceIFace()->addHumanMessage((PlayerTypes)getFirstPlayer(), true, GC.getEVENT_MESSAGE_TIME(), szString, "AS2D_DEAL_CANCELLED");
		}

		if (GET_TEAM(GET_PLAYER(getSecondPlayer()).getTeam()).isHasMet(GET_PLAYER(getFirstPlayer()).getTeam())) {
			szDealString.clear();
			GAMETEXT.getDealString(szDealString, *this, getSecondPlayer());
			szString.Format(L"%s: %s", szCancelString.GetCString(), szDealString.getCString());
			gDLL->getInterfaceIFace()->addHumanMessage((PlayerTypes)getSecondPlayer(), true, GC.getEVENT_MESSAGE_TIME(), szString, "AS2D_DEAL_CANCELLED");
		}
	}

	CLLNode<TradeData>* pNode;

	for (pNode = headFirstTradesNode(); (pNode != NULL); pNode = nextFirstTradesNode(pNode)) {
		endTrade(pNode->m_data, getFirstPlayer(), getSecondPlayer(), bKillTeam);
	}

	for (pNode = headSecondTradesNode(); (pNode != NULL); pNode = nextSecondTradesNode(pNode)) {
		endTrade(pNode->m_data, getSecondPlayer(), getFirstPlayer(), bKillTeam);
	}

	GC.getGameINLINE().deleteDeal(getID());
}


void CvDeal::addTrades(CLinkList<TradeData>* pFirstList, CLinkList<TradeData>* pSecondList, bool bCheckAllowed) {
	if (isVassalTrade(pFirstList) && isVassalTrade(pSecondList)) {
		return;
	}

	if (pFirstList != NULL) {
		for (CLLNode<TradeData>* pNode = pFirstList->head(); pNode; pNode = pFirstList->next(pNode)) {
			if (bCheckAllowed) {
				if (!(GET_PLAYER(getFirstPlayer()).canTradeItem(getSecondPlayer(), pNode->m_data))) {
					return;
				}
			}
		}
	}

	if (pSecondList != NULL) {
		for (CLLNode<TradeData>* pNode = pSecondList->head(); pNode; pNode = pSecondList->next(pNode)) {
			if (bCheckAllowed && !(GET_PLAYER(getSecondPlayer()).canTradeItem(getFirstPlayer(), pNode->m_data))) {
				return;
			}
		}
	}

	TeamTypes eFirstTeam = GET_PLAYER(getFirstPlayer()).getTeam();
	TeamTypes eSecondTeam = GET_PLAYER(getSecondPlayer()).getTeam();
	bool bBumpUnits = false; // K-Mod

	if (atWar(eFirstTeam, eSecondTeam)) {
		// free vassals of capitulating team before peace is signed
		if (isVassalTrade(pSecondList)) {
			for (int iI = 0; iI < MAX_TEAMS; iI++) {
				TeamTypes eLoopTeam = (TeamTypes)iI;
				CvTeam& kLoopTeam = GET_TEAM(eLoopTeam);
				if ((eLoopTeam != eFirstTeam) && (eLoopTeam != eSecondTeam)) {
					if (kLoopTeam.isAlive() && kLoopTeam.isVassal(eSecondTeam)) {
						GET_TEAM(eSecondTeam).freeVassal(eLoopTeam);
						int iSecondSuccess = GET_TEAM(eFirstTeam).AI_getWarSuccess(eSecondTeam) + GC.getWAR_SUCCESS_CITY_CAPTURING() * GET_TEAM(eSecondTeam).getNumCities();
						GET_TEAM(eFirstTeam).AI_setWarSuccess(eLoopTeam, std::max(iSecondSuccess, GET_TEAM(eFirstTeam).AI_getWarSuccess(eLoopTeam)));
					}
				}
			}
		} else if (isVassalTrade(pFirstList)) // K-Mod added 'else'
		{
			for (int iI = 0; iI < MAX_TEAMS; iI++) {
				TeamTypes eLoopTeam = (TeamTypes)iI;
				CvTeam& kLoopTeam = GET_TEAM(eLoopTeam);
				if ((eLoopTeam != eFirstTeam) && (eLoopTeam != eSecondTeam)) {
					if (kLoopTeam.isAlive() && kLoopTeam.isVassal(eFirstTeam)) {
						GET_TEAM(eFirstTeam).freeVassal(eLoopTeam);
						int iFirstSuccess = GET_TEAM(eSecondTeam).AI_getWarSuccess(eFirstTeam) + GC.getWAR_SUCCESS_CITY_CAPTURING() * GET_TEAM(eFirstTeam).getNumCities();
						GET_TEAM(eSecondTeam).AI_setWarSuccess(eLoopTeam, std::max(iFirstSuccess, GET_TEAM(eSecondTeam).AI_getWarSuccess(eLoopTeam)));
					}
				}
			}
		}

		// K-Mod. Bump units only after all trades are completed, because some deals (such as city gifts) may affect which units get bumped.
		// (originally, units were bumped automatically while executing the peace deal trades)
		// Note: the original code didn't bump units for vassal trades. This is can erroneously allow the vassal's units to stay in the master's land.
		GET_TEAM(eFirstTeam).makePeace(eSecondTeam, false);
		bBumpUnits = true;
	} else {
		if (!isPeaceDealBetweenOthers(pFirstList, pSecondList)) {
			if ((pSecondList != NULL) && (pSecondList->getLength() > 0)) {
				int iValue = GET_PLAYER(getFirstPlayer()).AI_dealVal(getSecondPlayer(), pSecondList, true);

				if ((pFirstList != NULL) && (pFirstList->getLength() > 0)) {
					GET_PLAYER(getFirstPlayer()).AI_changePeacetimeTradeValue(getSecondPlayer(), iValue);
				} else {
					GET_PLAYER(getFirstPlayer()).AI_changePeacetimeGrantValue(getSecondPlayer(), iValue);
				}
			}
			if ((pFirstList != NULL) && (pFirstList->getLength() > 0)) {
				int iValue = GET_PLAYER(getSecondPlayer()).AI_dealVal(getFirstPlayer(), pFirstList, true);

				if ((pSecondList != NULL) && (pSecondList->getLength() > 0)) {
					GET_PLAYER(getSecondPlayer()).AI_changePeacetimeTradeValue(getFirstPlayer(), iValue);
				} else {
					GET_PLAYER(getSecondPlayer()).AI_changePeacetimeGrantValue(getFirstPlayer(), iValue);
				}
			}
			GET_PLAYER(getFirstPlayer()).AI_updateAttitudeCache(getSecondPlayer());
			GET_PLAYER(getSecondPlayer()).AI_updateAttitudeCache(getFirstPlayer());
		}
	}

	bool bAlliance = false;

	if (pFirstList != NULL) {
		// K-Mod. Vassal deals need to be implemented last, so that master/vassal power is set correctly.
		for (CLLNode<TradeData>* pNode = pFirstList->head(); pNode; pNode = pFirstList->next(pNode)) {
			if (isVassal(pNode->m_data.m_eItemType)) {
				pFirstList->moveToEnd(pNode);
				break;
			}
		}
		// K-Mod end
		for (CLLNode<TradeData>* pNode = pFirstList->head(); pNode; pNode = pFirstList->next(pNode)) {
			bool bSave = startTrade(pNode->m_data, getFirstPlayer(), getSecondPlayer());
			bBumpUnits = bBumpUnits || pNode->m_data.m_eItemType == TRADE_PEACE; // K-Mod

			if (bSave)
				insertAtEndFirstTrades(pNode->m_data);
			if (pNode->m_data.m_eItemType == TRADE_PERMANENT_ALLIANCE)
				bAlliance = true;
		}
	}

	if (pSecondList != NULL) {
		// K-Mod. Vassal deals need to be implemented last, so that master/vassal power is set correctly.
		for (CLLNode<TradeData>* pNode = pFirstList->head(); pNode; pNode = pFirstList->next(pNode)) {
			if (isVassal(pNode->m_data.m_eItemType)) {
				pFirstList->moveToEnd(pNode);
				break;
			}
		}
		// K-Mod end
		for (CLLNode<TradeData>* pNode = pSecondList->head(); pNode; pNode = pSecondList->next(pNode)) {
			bool bSave = startTrade(pNode->m_data, getSecondPlayer(), getFirstPlayer());
			bBumpUnits = bBumpUnits || pNode->m_data.m_eItemType == TRADE_PEACE; // K-Mod

			if (bSave)
				insertAtEndSecondTrades(pNode->m_data);

			if (pNode->m_data.m_eItemType == TRADE_PERMANENT_ALLIANCE)
				bAlliance = true;
		}
	}

	if (bAlliance) {
		if (eFirstTeam < eSecondTeam) {
			GET_TEAM(eFirstTeam).addTeam(eSecondTeam);
		} else if (eSecondTeam < eFirstTeam) {
			GET_TEAM(eSecondTeam).addTeam(eFirstTeam);
		}
	}

	if (bBumpUnits) {
		GC.getMapINLINE().verifyUnitValidPlot();
	}
}


void CvDeal::doTurn() {
	if (!isPeaceDeal()) {
		if (getLengthSecondTrades() > 0) {
			int iValue = (GET_PLAYER(getFirstPlayer()).AI_dealVal(getSecondPlayer(), getSecondTrades()) / GC.getDefineINT("PEACE_TREATY_LENGTH"));

			if (getLengthFirstTrades() > 0) {
				GET_PLAYER(getFirstPlayer()).AI_changePeacetimeTradeValue(getSecondPlayer(), iValue);
			} else {
				GET_PLAYER(getFirstPlayer()).AI_changePeacetimeGrantValue(getSecondPlayer(), iValue);
			}
		}

		if (getLengthFirstTrades() > 0) {
			int iValue = (GET_PLAYER(getSecondPlayer()).AI_dealVal(getFirstPlayer(), getFirstTrades()) / GC.getDefineINT("PEACE_TREATY_LENGTH"));

			if (getLengthSecondTrades() > 0) {
				GET_PLAYER(getSecondPlayer()).AI_changePeacetimeTradeValue(getFirstPlayer(), iValue);
			} else {
				GET_PLAYER(getSecondPlayer()).AI_changePeacetimeGrantValue(getFirstPlayer(), iValue);
			}
		}
		// K-Mod note: for balance reasons this function should probably be called at the boundary of some particular player's turn,
		// rather than at the turn boundary of the game itself. -- Unfortunately, the game currently doesn't work like this.
		// Also, note that we do not update attitudes of particular players here, but instead update all of them at the game turn boundary.
	}
}


// XXX probably should have some sort of message for the user or something...
void CvDeal::verify() {
	bool bCancelDeal = false;

	const CvPlayer& kFirstPlayer = GET_PLAYER(getFirstPlayer());
	const CvPlayer& kSecondPlayer = GET_PLAYER(getSecondPlayer());

	for (CLLNode<TradeData>* pNode = headFirstTradesNode(); (pNode != NULL); pNode = nextFirstTradesNode(pNode)) {
		if (pNode->m_data.m_eItemType == TRADE_RESOURCES) {
			// XXX embargoes?
			if ((kFirstPlayer.getNumTradeableBonuses((BonusTypes)(pNode->m_data.m_iData)) < 0) ||
				!(kFirstPlayer.canTradeNetworkWith(getSecondPlayer())) ||
				GET_TEAM(kFirstPlayer.getTeam()).isBonusObsolete((BonusTypes)pNode->m_data.m_iData) ||
				GET_TEAM(kSecondPlayer.getTeam()).isBonusObsolete((BonusTypes)pNode->m_data.m_iData)) {
				bCancelDeal = true;
			}
		}
	}

	for (CLLNode<TradeData>* pNode = headSecondTradesNode(); (pNode != NULL); pNode = nextSecondTradesNode(pNode)) {
		if (pNode->m_data.m_eItemType == TRADE_RESOURCES) {
			// XXX embargoes?
			if ((GET_PLAYER(getSecondPlayer()).getNumTradeableBonuses((BonusTypes)(pNode->m_data.m_iData)) < 0) ||
				!(GET_PLAYER(getSecondPlayer()).canTradeNetworkWith(getFirstPlayer())) ||
				GET_TEAM(kFirstPlayer.getTeam()).isBonusObsolete((BonusTypes)pNode->m_data.m_iData) ||
				GET_TEAM(kSecondPlayer.getTeam()).isBonusObsolete((BonusTypes)pNode->m_data.m_iData)) {
				bCancelDeal = true;
			}
		}
	}

	if (isCancelable(NO_PLAYER)) {
		if (isPeaceDeal()) {
			bCancelDeal = true;
		}
	}

	if (bCancelDeal) {
		kill();
	}
}


bool CvDeal::isPeaceDeal() const {
	for (CLLNode<TradeData>* pNode = headFirstTradesNode(); (pNode != NULL); pNode = nextFirstTradesNode(pNode)) {
		if (pNode->m_data.m_eItemType == getPeaceItem()) {
			return true;
		}
	}

	for (CLLNode<TradeData>* pNode = headSecondTradesNode(); (pNode != NULL); pNode = nextSecondTradesNode(pNode)) {
		if (pNode->m_data.m_eItemType == getPeaceItem()) {
			return true;
		}
	}

	return false;
}

bool CvDeal::isVassalDeal() const {
	return (isVassalTrade(&m_firstTrades) || isVassalTrade(&m_secondTrades));
}

bool CvDeal::isVassalTrade(const CLinkList<TradeData>* pList) {
	if (pList) {
		for (CLLNode<TradeData>* pNode = pList->head(); pNode != NULL; pNode = pList->next(pNode)) {
			if (isVassal(pNode->m_data.m_eItemType)) {
				return true;
			}
		}
	}

	return false;
}


bool CvDeal::isUncancelableVassalDeal(PlayerTypes eByPlayer, CvWString* pszReason) const {
	for (CLLNode<TradeData>* pNode = headFirstTradesNode(); (pNode != NULL); pNode = nextFirstTradesNode(pNode)) {
		if (isVassal(pNode->m_data.m_eItemType)) {
			if (eByPlayer == getSecondPlayer()) {
				if (pszReason) {
					*pszReason += gDLL->getText("TXT_KEY_MISC_DEAL_NO_CANCEL_EVER");
				}

				return true;
			}
		}

		if (pNode->m_data.m_eItemType == TRADE_SURRENDER) {
			CvTeam& kVassal = GET_TEAM(GET_PLAYER(getFirstPlayer()).getTeam());
			TeamTypes eMaster = GET_PLAYER(getSecondPlayer()).getTeam();

			if (!kVassal.canVassalRevolt(eMaster)) {
				if (pszReason) {
					CvWStringBuffer szBuffer;
					GAMETEXT.setVassalRevoltHelp(szBuffer, eMaster, GET_PLAYER(getFirstPlayer()).getTeam());
					*pszReason = szBuffer.getCString();
				}

				return true;
			}
		}
	}

	for (CLLNode<TradeData>* pNode = headSecondTradesNode(); (pNode != NULL); pNode = nextSecondTradesNode(pNode)) {
		if (isVassal(pNode->m_data.m_eItemType)) {
			if (eByPlayer == getFirstPlayer()) {
				if (pszReason) {
					*pszReason += gDLL->getText("TXT_KEY_MISC_DEAL_NO_CANCEL_EVER");
				}

				return true;
			}
		}

		if (pNode->m_data.m_eItemType == TRADE_SURRENDER) {
			CvTeam& kVassal = GET_TEAM(GET_PLAYER(getSecondPlayer()).getTeam());
			TeamTypes eMaster = GET_PLAYER(getFirstPlayer()).getTeam();

			if (!kVassal.canVassalRevolt(eMaster)) {
				if (pszReason) {
					if (pszReason) {
						CvWStringBuffer szBuffer;
						GAMETEXT.setVassalRevoltHelp(szBuffer, eMaster, GET_PLAYER(getFirstPlayer()).getTeam());
						*pszReason = szBuffer.getCString();
					}
				}

				return true;
			}
		}
	}

	return false;
}

bool CvDeal::isVassalTributeDeal(const CLinkList<TradeData>* pList) {
	for (CLLNode<TradeData>* pNode = pList->head(); pNode != NULL; pNode = pList->next(pNode)) {
		if (pNode->m_data.m_eItemType != TRADE_RESOURCES) {
			return false;
		}
	}

	return true;
}

bool CvDeal::isPeaceDealBetweenOthers(CLinkList<TradeData>* pFirstList, CLinkList<TradeData>* pSecondList) const {
	if (pFirstList != NULL) {
		for (CLLNode<TradeData>* pNode = pFirstList->head(); pNode; pNode = pFirstList->next(pNode)) {
			if (pNode->m_data.m_eItemType == TRADE_PEACE) {
				return true;
			}
		}
	}

	if (pSecondList != NULL) {
		for (CLLNode<TradeData>* pNode = pSecondList->head(); pNode; pNode = pSecondList->next(pNode)) {
			if (pNode->m_data.m_eItemType == TRADE_PEACE) {
				return true;
			}
		}
	}

	return false;
}


int CvDeal::getID() const {
	return m_iID;
}


void CvDeal::setID(int iID) {
	m_iID = iID;
}


int CvDeal::getInitialGameTurn() const {
	return m_iInitialGameTurn;
}


void CvDeal::setInitialGameTurn(int iNewValue) {
	m_iInitialGameTurn = iNewValue;
}


PlayerTypes CvDeal::getFirstPlayer() const {
	return m_eFirstPlayer;
}


PlayerTypes CvDeal::getSecondPlayer() const {
	return m_eSecondPlayer;
}


void CvDeal::clearFirstTrades() {
	m_firstTrades.clear();
}


void CvDeal::insertAtEndFirstTrades(TradeData trade) {
	m_firstTrades.insertAtEnd(trade);
}


CLLNode<TradeData>* CvDeal::nextFirstTradesNode(CLLNode<TradeData>* pNode) const {
	return m_firstTrades.next(pNode);
}


int CvDeal::getLengthFirstTrades() const {
	return m_firstTrades.getLength();
}


CLLNode<TradeData>* CvDeal::headFirstTradesNode() const {
	return m_firstTrades.head();
}


const CLinkList<TradeData>* CvDeal::getFirstTrades() const {
	return &(m_firstTrades);
}


void CvDeal::clearSecondTrades() {
	m_secondTrades.clear();
}


void CvDeal::insertAtEndSecondTrades(TradeData trade) {
	m_secondTrades.insertAtEnd(trade);
}


CLLNode<TradeData>* CvDeal::nextSecondTradesNode(CLLNode<TradeData>* pNode) const {
	return m_secondTrades.next(pNode);
}


int CvDeal::getLengthSecondTrades() const {
	return m_secondTrades.getLength();
}


CLLNode<TradeData>* CvDeal::headSecondTradesNode() const {
	return m_secondTrades.head();
}


const CLinkList<TradeData>* CvDeal::getSecondTrades() const {
	return &(m_secondTrades);
}


void CvDeal::write(FDataStreamBase* pStream) {
	uint uiFlag = 0;
	pStream->Write(uiFlag);		// flag for expansion

	pStream->Write(m_iID);
	pStream->Write(m_iInitialGameTurn);

	pStream->Write(m_eFirstPlayer);
	pStream->Write(m_eSecondPlayer);

	m_firstTrades.Write(pStream);
	m_secondTrades.Write(pStream);
}

void CvDeal::read(FDataStreamBase* pStream) {
	uint uiFlag = 0;
	pStream->Read(&uiFlag);	// flags for expansion

	pStream->Read(&m_iID);
	pStream->Read(&m_iInitialGameTurn);

	pStream->Read((int*)&m_eFirstPlayer);
	pStream->Read((int*)&m_eSecondPlayer);

	m_firstTrades.Read(pStream);
	m_secondTrades.Read(pStream);
}

// Protected Functions...

// Returns true if the trade should be saved...
bool CvDeal::startTrade(TradeData trade, PlayerTypes eFromPlayer, PlayerTypes eToPlayer) {

	CvPlayerAI& kFromPlayer = GET_PLAYER(eFromPlayer);
	CvPlayerAI& kToPlayer = GET_PLAYER(eToPlayer);
	TeamTypes eToTeam = kToPlayer.getTeam();

	bool bSave = false;
	switch (trade.m_eItemType) {
	case TRADE_TECHNOLOGIES:
		{
			// K-Mod only adjust tech_from_any memory if this is a tech from a recent era
			// and the team receiving the tech isn't already more than 2/3 of the way through.
			// (This is to prevent the AI from being crippled by human players selling them lots of tech scraps.)
			// Note: the current game era is the average of all the player eras, rounded down. (It no longer includes barbs.)
			bool bSignificantTech =
				GC.getTechInfo((TechTypes)trade.m_iData).getEra() >= GC.getGame().getCurrentEra() - 1 &&
				GET_TEAM(eToTeam).getResearchLeft((TechTypes)trade.m_iData) > GET_TEAM(eToTeam).getResearchCost((TechTypes)trade.m_iData) / 3;
			GET_TEAM(eToTeam).setHasTech(((TechTypes)trade.m_iData), true, eToPlayer, true, true);
			GET_TEAM(eToTeam).setNoTradeTech(((TechTypes)trade.m_iData), true);

			if (gTeamLogLevel >= 2) {
				logBBAI("    Player %d (%S) trades tech %S to player %d (%S)", eFromPlayer, kFromPlayer.getCivilizationDescription(0), GC.getTechInfo((TechTypes)trade.m_iData).getDescription(), eToPlayer, kToPlayer.getCivilizationDescription(0));
			}

			for (PlayerTypes eLoopPlayer = (PlayerTypes)0; eLoopPlayer < MAX_PLAYERS; eLoopPlayer = (PlayerTypes)(eLoopPlayer + 1)) // Include barbarians
			{
				CvPlayer& kLoopPLayer = GET_PLAYER(eLoopPlayer);
				if (kLoopPLayer.isAlive()) {
					if (kLoopPLayer.getTeam() == eToTeam) {
						kLoopPLayer.AI_changeMemoryCount(eFromPlayer, MEMORY_TRADED_TECH_TO_US, 1);
					} else if (bSignificantTech) // K-Mod
					{
						if (GET_TEAM(kLoopPLayer.getTeam()).isHasMet(eToTeam)) {
							kLoopPLayer.AI_changeMemoryCount(eToPlayer, MEMORY_RECEIVED_TECH_FROM_ANY, 1);
						}
					}
				}
			}
			break;
		}

	case TRADE_RESOURCES:
		kFromPlayer.changeBonusExport(((BonusTypes)trade.m_iData), 1);
		kToPlayer.changeBonusImport(((BonusTypes)trade.m_iData), 1);
		if (gTeamLogLevel >= 2) {
			logBBAI("    Player %d (%S) trades bonus type %S due to TRADE_RESOURCES with %d (%S)", eFromPlayer, kFromPlayer.getCivilizationDescription(0), GC.getBonusInfo((BonusTypes)trade.m_iData).getDescription(), eToPlayer, kToPlayer.getCivilizationDescription(0));
		}
		bSave = true;
		break;

	case TRADE_WORKER:
	case TRADE_MILITARY_UNIT:
		{
			CvUnit* pUnit = GET_PLAYER(eFromPlayer).getUnit(trade.m_iData);
			if (pUnit != NULL) {
				pUnit->tradeUnit(eToPlayer);
			}
		}
		break;

	case TRADE_CITIES:
		{
			CvCity* pCity = kFromPlayer.getCity(trade.m_iData);
			if (pCity != NULL) {
				if (gTeamLogLevel >= 2) {
					logBBAI("    Player %d (%S) gives a city due to TRADE_CITIES with %d (%S)", eFromPlayer, kFromPlayer.getCivilizationDescription(0), eToPlayer, kToPlayer.getCivilizationDescription(0));
				}
				pCity->doTask(TASK_GIFT, eToPlayer);
			}
		}
		break;

	case TRADE_GOLD:
		kFromPlayer.changeGold(-(trade.m_iData));
		kToPlayer.changeGold(trade.m_iData);
		kFromPlayer.AI_changeGoldTradedTo(eToPlayer, trade.m_iData);

		if (gTeamLogLevel >= 2) {
			logBBAI("    Player %d (%S) trades gold %d due to TRADE_GOLD with player %d (%S)", eFromPlayer, kFromPlayer.getCivilizationDescription(0), trade.m_iData, eToPlayer, kToPlayer.getCivilizationDescription(0));
		}

		// Python Event
		CvEventReporter::getInstance().playerGoldTrade(eFromPlayer, eToPlayer, trade.m_iData);

		break;

	case TRADE_GOLD_PER_TURN:
		kFromPlayer.changeGoldPerTurnByPlayer(eToPlayer, -(trade.m_iData));
		kToPlayer.changeGoldPerTurnByPlayer(eFromPlayer, trade.m_iData);

		if (gTeamLogLevel >= 2) {
			logBBAI("    Player %d (%S) trades gold per turn %d due to TRADE_GOLD_PER_TURN with player %d (%S)", eFromPlayer, kFromPlayer.getCivilizationDescription(0), trade.m_iData, eToPlayer, kToPlayer.getCivilizationDescription(0));
		}

		bSave = true;
		break;

	case TRADE_MAPS:
		for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++) {
			CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

			if (pLoopPlot->isRevealed(kFromPlayer.getTeam(), false)) {
				pLoopPlot->setRevealed(eToTeam, true, false, kFromPlayer.getTeam(), false);
			}
		}

		for (PlayerTypes eLoopPlayer = (PlayerTypes)0; eLoopPlayer < MAX_PLAYERS; eLoopPlayer = (PlayerTypes)(eLoopPlayer + 1)) {
			CvPlayer& kLoopPLayer = GET_PLAYER(eLoopPlayer);
			if (kLoopPLayer.isAlive()) {
				if (kLoopPLayer.getTeam() == eToTeam) {
					kLoopPLayer.updatePlotGroups();
				}
			}
		}

		if (gTeamLogLevel >= 2) {
			logBBAI("    Player %d (%S) trades maps due to TRADE_MAPS with player %d (%S)", eFromPlayer, kFromPlayer.getCivilizationDescription(0), eToPlayer, kToPlayer.getCivilizationDescription(0));
		}
		break;

	case TRADE_SURRENDER:
	case TRADE_VASSAL:
		if (trade.m_iData == 0) {
			startTeamTrade(trade.m_eItemType, kFromPlayer.getTeam(), eToTeam, false);
			GET_TEAM(kFromPlayer.getTeam()).setVassal(eToTeam, true, TRADE_SURRENDER == trade.m_eItemType);
			if (gTeamLogLevel >= 2) {
				if (TRADE_SURRENDER == trade.m_eItemType) {
					logBBAI("    Player %d (%S) trades themselves as vassal due to TRADE_SURRENDER with player %d (%S)", eFromPlayer, kFromPlayer.getCivilizationDescription(0), eToPlayer, kToPlayer.getCivilizationDescription(0));
				} else {
					logBBAI("    Player %d (%S) trades themselves as vassal due to TRADE_VASSAL with player %d (%S)", eFromPlayer, kFromPlayer.getCivilizationDescription(0), eToPlayer, kToPlayer.getCivilizationDescription(0));
				}
			}
		} else {
			bSave = true;
		}


		break;

	case TRADE_PEACE:
		if (gTeamLogLevel >= 2) {
			logBBAI("    Team %d (%S) makes peace with team %d due to TRADE_PEACE with %d (%S)", kFromPlayer.getTeam(), kFromPlayer.getCivilizationDescription(0), trade.m_iData, eToPlayer, kToPlayer.getCivilizationDescription(0));
		}
		GET_TEAM(kFromPlayer.getTeam()).makePeace((TeamTypes)trade.m_iData, false); // K-Mod. (units will be bumped after the rest of the trade deals are completed.)
		GET_TEAM(kFromPlayer.getTeam()).signPeaceTreaty((TeamTypes)trade.m_iData); // K-Mod. Use a standard peace treaty rather than a simple cease-fire.
		// K-Mod todo: this team should offer something fair to the peace-team if this teams endWarVal is higher.
		break;

	case TRADE_WAR:
		if (gTeamLogLevel >= 2) {
			logBBAI("    Team %d (%S) declares war on team %d due to TRADE_WAR with %d (%S)", kFromPlayer.getTeam(), kFromPlayer.getCivilizationDescription(0), trade.m_iData, eToPlayer, kToPlayer.getCivilizationDescription(0));
		}
		GET_TEAM(kFromPlayer.getTeam()).declareWar(((TeamTypes)trade.m_iData), true, NO_WARPLAN);

		for (PlayerTypes eLoopPlayer = (PlayerTypes)0; eLoopPlayer < MAX_PLAYERS; eLoopPlayer = (PlayerTypes)(eLoopPlayer + 1)) {
			CvPlayer& kLoopPLayer = GET_PLAYER(eLoopPlayer);
			if (kLoopPLayer.isAlive()) {
				if (kLoopPLayer.getTeam() == ((TeamTypes)trade.m_iData)) {
					kLoopPLayer.AI_changeMemoryCount(eToPlayer, MEMORY_HIRED_WAR_ALLY, 1);
				}
			}
		}
		break;

	case TRADE_EMBARGO:
		kFromPlayer.stopTradingWithTeam((TeamTypes)trade.m_iData);

		for (PlayerTypes eLoopPlayer = (PlayerTypes)0; eLoopPlayer < MAX_PLAYERS; eLoopPlayer = (PlayerTypes)(eLoopPlayer + 1)) {
			CvPlayer& kLoopPLayer = GET_PLAYER(eLoopPlayer);
			if (kLoopPLayer.isAlive()) {
				if (kLoopPLayer.getTeam() == ((TeamTypes)trade.m_iData)) {
					kLoopPLayer.AI_changeMemoryCount(eToPlayer, MEMORY_HIRED_TRADE_EMBARGO, 1);
				}
			}
		}
		if (gTeamLogLevel >= 2) {
			logBBAI("    Player %d (%S) signs embargo against team %d due to TRADE_EMBARGO with player %d (%S)", eFromPlayer, kFromPlayer.getCivilizationDescription(0), (TeamTypes)trade.m_iData, eToPlayer, kToPlayer.getCivilizationDescription(0));
		}
		break;

	case TRADE_CIVIC:
		{
			CivicTypes* paeNewCivics = new CivicTypes[GC.getNumCivicOptionInfos()];

			for (CivicOptionTypes eCivicOption = (CivicOptionTypes)0; eCivicOption < GC.getNumCivicOptionInfos(); eCivicOption = (CivicOptionTypes)(eCivicOption + 1)) {
				paeNewCivics[eCivicOption] = kFromPlayer.getCivics(eCivicOption);
			}

			paeNewCivics[GC.getCivicInfo((CivicTypes)trade.m_iData).getCivicOptionType()] = ((CivicTypes)trade.m_iData);

			kFromPlayer.revolution(paeNewCivics, true);

			if (kFromPlayer.AI_getCivicTimer() < GC.getDefineINT("PEACE_TREATY_LENGTH")) {
				kFromPlayer.AI_setCivicTimer(GC.getDefineINT("PEACE_TREATY_LENGTH"));
			}
			if (gTeamLogLevel >= 2) {
				logBBAI("    Player %d (%S) switched civics due to TRADE_CIVICS with player %d (%S)", eFromPlayer, kFromPlayer.getCivilizationDescription(0), eToPlayer, kToPlayer.getCivilizationDescription(0));
			}

			SAFE_DELETE_ARRAY(paeNewCivics);
		}
		break;

	case TRADE_RELIGION:
		kFromPlayer.convert((ReligionTypes)trade.m_iData);

		if (kFromPlayer.AI_getReligionTimer() < GC.getDefineINT("PEACE_TREATY_LENGTH")) {
			kFromPlayer.AI_setReligionTimer(GC.getDefineINT("PEACE_TREATY_LENGTH"));
		}
		if (gTeamLogLevel >= 2) {
			logBBAI("    Player %d (%S) switched religions due to TRADE_RELIGION with player %d (%S)", eFromPlayer, kFromPlayer.getCivilizationDescription(0), eToPlayer, kToPlayer.getCivilizationDescription(0));
		}
		break;

	case TRADE_EMBASSY:
		if (trade.m_iData == 0) {
			startTeamTrade(TRADE_EMBASSY, kFromPlayer.getTeam(), eToTeam, true);
			GET_TEAM(kFromPlayer.getTeam()).setHasEmbassy(eToTeam, true);
		} else {
			bSave = true;
		}
		break;

	case TRADE_OPEN_BORDERS:
		if (trade.m_iData == 0) {
			startTeamTrade(TRADE_OPEN_BORDERS, kFromPlayer.getTeam(), eToTeam, true);
			GET_TEAM(kFromPlayer.getTeam()).setOpenBorders(eToTeam, true);
			endTeamTrade(TRADE_LIMITED_BORDERS, kFromPlayer.getTeam(), eToTeam);
			if (gTeamLogLevel >= 2) {
				logBBAI("    Player %d (%S_1) signs open borders due to TRADE_OPEN_BORDERS with player %d (%S_2)", eFromPlayer, kFromPlayer.getCivilizationDescription(0), eToPlayer, kToPlayer.getCivilizationDescription(0));
			}
		} else {
			bSave = true;
		}
		break;

	case TRADE_LIMITED_BORDERS:
		if (trade.m_iData == 0) {
			startTeamTrade(TRADE_LIMITED_BORDERS, kFromPlayer.getTeam(), eToTeam, true);
			GET_TEAM(kFromPlayer.getTeam()).setLimitedBorders(eToTeam, true);
		} else {
			bSave = true;
		}
		break;

	case TRADE_FREE_TRADE_ZONE:
		if (trade.m_iData == 0) {
			startTeamTrade(TRADE_FREE_TRADE_ZONE, GET_PLAYER(eFromPlayer).getTeam(), GET_PLAYER(eToPlayer).getTeam(), true);
			GET_TEAM(GET_PLAYER(eFromPlayer).getTeam()).setFreeTradeAgreement(eToTeam, true);
		} else {
			bSave = true;
		}
		break;

	case TRADE_DEFENSIVE_PACT:
		if (trade.m_iData == 0) {
			startTeamTrade(TRADE_DEFENSIVE_PACT, kFromPlayer.getTeam(), eToTeam, true);
			GET_TEAM(kFromPlayer.getTeam()).setDefensivePact(eToTeam, true);
			if (gTeamLogLevel >= 2) {
				logBBAI("    Player %d (%S) signs defensive pact due to TRADE_DEFENSIVE_PACT with player %d (%S)", eFromPlayer, kFromPlayer.getCivilizationDescription(0), eToPlayer, kToPlayer.getCivilizationDescription(0));
			}
		} else {
			bSave = true;
		}
		break;

	case TRADE_PERMANENT_ALLIANCE:
		break;

	case TRADE_PEACE_TREATY:
		GET_TEAM(kFromPlayer.getTeam()).setForcePeace(eToTeam, true);
		if (gTeamLogLevel >= 2) {
			logBBAI("    Player %d (%S) signs peace treaty due to TRADE_PEACE_TREATY with player %d (%S)", eFromPlayer, kFromPlayer.getCivilizationDescription(0), eToPlayer, kToPlayer.getCivilizationDescription(0));
		}
		bSave = true;
		break;

	case TRADE_NON_AGGRESSION:
		if (trade.m_iData == 0) {
			startTeamTrade(TRADE_NON_AGGRESSION, kFromPlayer.getTeam(), kToPlayer.getTeam(), true);
			GET_TEAM(kFromPlayer.getTeam()).setHasNonAggression(eToTeam, true);
		} else {
			bSave = true;
		}
		break;

	default:
		FAssert(false);
		break;
	}

	return bSave;
}


void CvDeal::endTrade(TradeData trade, PlayerTypes eFromPlayer, PlayerTypes eToPlayer, bool bTeam) {
	CvPlayerAI& kFromPlayer = GET_PLAYER(eFromPlayer);
	CvPlayerAI& kToPlayer = GET_PLAYER(eToPlayer);
	TeamTypes eFromTeam = (TeamTypes)kFromPlayer.getTeam();
	TeamTypes eToTeam = (TeamTypes)kToPlayer.getTeam();
	CvTeam& kFromTeam = GET_TEAM(eFromTeam);

	switch (trade.m_eItemType) {
	case TRADE_TECHNOLOGIES:
		FAssert(false);
		break;

	case TRADE_RESOURCES:
		kToPlayer.changeBonusImport((BonusTypes)trade.m_iData, -1);
		kFromPlayer.changeBonusExport((BonusTypes)trade.m_iData, -1);
		break;

	case TRADE_CITIES:
	case TRADE_GOLD:
	case TRADE_WORKER:
	case TRADE_MILITARY_UNIT:
		FAssert(false);
		break;

	case TRADE_GOLD_PER_TURN:
		kFromPlayer.changeGoldPerTurnByPlayer(eToPlayer, trade.m_iData);
		kToPlayer.changeGoldPerTurnByPlayer(eFromPlayer, -(trade.m_iData));
		break;

	case TRADE_MAPS:
	case TRADE_PEACE:
	case TRADE_WAR:
	case TRADE_EMBARGO:
	case TRADE_CIVIC:
	case TRADE_RELIGION:
		FAssert(false);
		break;

	case TRADE_VASSAL:
		kFromTeam.setVassal(eToTeam, false, false);
		if (bTeam) {
			endTeamTrade(TRADE_VASSAL, eFromTeam, eToTeam);
		}
		break;

	case TRADE_SURRENDER:
		kFromTeam.setVassal(eToTeam, false, true);
		if (bTeam) {
			endTeamTrade(TRADE_SURRENDER, eFromTeam, eToTeam);
		}
		break;

	case TRADE_EMBASSY:
		kFromTeam.setHasEmbassy(eToTeam, false);
		if (bTeam) {
			endTeamTrade(TRADE_EMBASSY, eFromTeam, eToTeam);
			endTeamTrade(TRADE_OPEN_BORDERS, eFromTeam, eToTeam);
			endTeamTrade(TRADE_DEFENSIVE_PACT, eFromTeam, eToTeam);
			endTeamTrade(TRADE_PERMANENT_ALLIANCE, eFromTeam, eToTeam);
			endTeamTrade(TRADE_FREE_TRADE_ZONE, eFromTeam, eToTeam);
			endTeamTrade(TRADE_NON_AGGRESSION, eFromTeam, eToTeam);
		}

		for (PlayerTypes eOuterPlayer = (PlayerTypes)0; eOuterPlayer < MAX_PLAYERS; eOuterPlayer = (PlayerTypes)(eOuterPlayer + 1)) {
			CvPlayer& kOuterPlayer = GET_PLAYER(eOuterPlayer);
			if (kOuterPlayer.isAlive()) {
				if (kOuterPlayer.getTeam() == eFromTeam) {
					for (PlayerTypes eInnerPlayer = (PlayerTypes)0; eInnerPlayer < MAX_PLAYERS; eInnerPlayer = (PlayerTypes)(eInnerPlayer + 1)) {
						const CvPlayer& kInnerPlayer = GET_PLAYER(eInnerPlayer);
						if (kInnerPlayer.isAlive()) {
							if (kInnerPlayer.getTeam() == eToTeam) {
								kOuterPlayer.AI_changeMemoryCount(eInnerPlayer, MEMORY_RECALLED_AMBASSADOR, 1);
							}
						}
					}
				}
			}
		}
		break;

	case TRADE_OPEN_BORDERS:
		kFromTeam.setOpenBorders((eToTeam), false);
		if (bTeam) {
			endTeamTrade(TRADE_OPEN_BORDERS, eFromTeam, eToTeam);
		}

		for (PlayerTypes eOuterPlayer = (PlayerTypes)0; eOuterPlayer < MAX_PLAYERS; eOuterPlayer = (PlayerTypes)(eOuterPlayer + 1)) {
			CvPlayer& kOuterPlayer = GET_PLAYER(eOuterPlayer);
			if (kOuterPlayer.isAlive()) {
				if (kOuterPlayer.getTeam() == eToTeam) {
					for (PlayerTypes eInnerPlayer = (PlayerTypes)0; eInnerPlayer < MAX_PLAYERS; eInnerPlayer = (PlayerTypes)(eInnerPlayer + 1)) {
						const CvPlayer& kInnerPlayer = GET_PLAYER(eInnerPlayer);
						if (kInnerPlayer.isAlive()) {
							if (kInnerPlayer.getTeam() == eFromTeam) {
								kOuterPlayer.AI_changeMemoryCount(eInnerPlayer, MEMORY_CANCELLED_OPEN_BORDERS, 1);
							}
						}
					}
				}
			}
		}
		break;

	case TRADE_LIMITED_BORDERS:
		GET_TEAM(GET_PLAYER(eFromPlayer).getTeam()).setLimitedBorders(((TeamTypes)(GET_PLAYER(eToPlayer).getTeam())), false);
		if (bTeam) {
			endTeamTrade(TRADE_LIMITED_BORDERS, GET_PLAYER(eFromPlayer).getTeam(), GET_PLAYER(eToPlayer).getTeam());
		}

		for (PlayerTypes eOuterPlayer = (PlayerTypes)0; eOuterPlayer < MAX_PLAYERS; eOuterPlayer = (PlayerTypes)(eOuterPlayer + 1)) {
			CvPlayer& kOuterPlayer = GET_PLAYER(eOuterPlayer);
			if (kOuterPlayer.isAlive()) {
				if (kOuterPlayer.getTeam() == eFromTeam) {
					for (PlayerTypes eInnerPlayer = (PlayerTypes)0; eInnerPlayer < MAX_PLAYERS; eInnerPlayer = (PlayerTypes)(eInnerPlayer + 1)) {
						const CvPlayer& kInnerPlayer = GET_PLAYER(eInnerPlayer);
						if (kInnerPlayer.isAlive()) {
							if (kInnerPlayer.getTeam() == eToTeam) {
								kOuterPlayer.AI_changeMemoryCount(eInnerPlayer, MEMORY_CANCELLED_OPEN_BORDERS, 1);
							}
						}
					}
				}
			}
		}
		break;

	case TRADE_FREE_TRADE_ZONE:
		GET_TEAM(kFromPlayer.getTeam()).setFreeTradeAgreement((TeamTypes)kToPlayer.getTeam(), false);
		if (bTeam) {
			endTeamTrade(TRADE_FREE_TRADE_ZONE, kFromPlayer.getTeam(), kToPlayer.getTeam());
		}

		for (PlayerTypes eOuterPlayer = (PlayerTypes)0; eOuterPlayer < MAX_PLAYERS; eOuterPlayer = (PlayerTypes)(eOuterPlayer + 1)) {
			CvPlayer& kOuterPlayer = GET_PLAYER(eOuterPlayer);
			if (kOuterPlayer.isAlive()) {
				if (kOuterPlayer.getTeam() == kFromPlayer.getTeam()) {
					for (PlayerTypes eInnerPlayer = (PlayerTypes)0; eInnerPlayer < MAX_PLAYERS; eInnerPlayer = (PlayerTypes)(eInnerPlayer + 1)) {
						const CvPlayer& kInnerPlayer = GET_PLAYER(eInnerPlayer);
						if (kInnerPlayer.isAlive()) {
							if (kInnerPlayer.getTeam() == kToPlayer.getTeam()) {
								kOuterPlayer.AI_changeMemoryCount(eInnerPlayer, MEMORY_CANCELLED_FREE_TRADE_AGREEMENT, 1);
							}
						}
					}
				}
			}
		}
		break;

	case TRADE_DEFENSIVE_PACT:
		kFromTeam.setDefensivePact((eToTeam), false);
		if (bTeam) {
			endTeamTrade(TRADE_DEFENSIVE_PACT, eFromTeam, eToTeam);
		}
		break;

	case TRADE_PERMANENT_ALLIANCE:
		FAssert(false);
		break;

	case TRADE_PEACE_TREATY:
		kFromTeam.setForcePeace((eToTeam), false);
		break;

	case TRADE_NON_AGGRESSION:
		kFromTeam.setHasNonAggression(eToTeam, false);
		if (bTeam) {
			endTeamTrade(TRADE_NON_AGGRESSION, eFromTeam, eToTeam);
		}

		for (PlayerTypes eOuterPlayer = (PlayerTypes)0; eOuterPlayer < MAX_PLAYERS; eOuterPlayer = (PlayerTypes)(eOuterPlayer + 1)) {
			CvPlayer& kOuterPlayer = GET_PLAYER(eOuterPlayer);
			if (kOuterPlayer.isAlive()) {
				if (kOuterPlayer.getTeam() == eFromTeam) {
					for (PlayerTypes eInnerPlayer = (PlayerTypes)0; eInnerPlayer < MAX_PLAYERS; eInnerPlayer = (PlayerTypes)(eInnerPlayer + 1)) {
						const CvPlayer& kInnerPlayer = GET_PLAYER(eInnerPlayer);
						if (kInnerPlayer.isAlive()) {
							if (kInnerPlayer.getTeam() == eToTeam) {
								kOuterPlayer.AI_changeMemoryCount(eInnerPlayer, MEMORY_CANCELLED_NON_AGGRESSION, 1);
							}
						}
					}
				}
			}
		}
		break;

	default:
		FAssert(false);
		break;
	}
}

void CvDeal::startTeamTrade(TradeableItems eItem, TeamTypes eFromTeam, TeamTypes eToTeam, bool bDual) {
	for (int iI = 0; iI < MAX_PLAYERS; iI++) {
		CvPlayer& kLoopFromPlayer = GET_PLAYER((PlayerTypes)iI);
		if (kLoopFromPlayer.isAlive()) {
			if (kLoopFromPlayer.getTeam() == eFromTeam) {
				for (int iJ = 0; iJ < MAX_PLAYERS; iJ++) {
					CvPlayer& kLoopToPlayer = GET_PLAYER((PlayerTypes)iJ);
					if (kLoopToPlayer.isAlive()) {
						if (kLoopToPlayer.getTeam() == eToTeam) {
							TradeData item;
							setTradeItem(&item, eItem, 1);
							CLinkList<TradeData> ourList;
							ourList.insertAtEnd(item);
							CLinkList<TradeData> theirList;
							if (bDual) {
								theirList.insertAtEnd(item);
							}
							GC.getGame().implementDeal((PlayerTypes)iI, (PlayerTypes)iJ, &ourList, &theirList);
						}
					}
				}
			}
		}
	}
}

void CvDeal::endTeamTrade(TradeableItems eItem, TeamTypes eFromTeam, TeamTypes eToTeam) {
	int iLoop;
	for (CvDeal* pLoopDeal = GC.getGameINLINE().firstDeal(&iLoop); pLoopDeal != NULL; pLoopDeal = GC.getGameINLINE().nextDeal(&iLoop)) {
		if (pLoopDeal != this) {
			bool bValid = true;

			if (GET_PLAYER(pLoopDeal->getFirstPlayer()).getTeam() == eFromTeam && GET_PLAYER(pLoopDeal->getSecondPlayer()).getTeam() == eToTeam) {
				if (pLoopDeal->getFirstTrades()) {
					for (CLLNode<TradeData>* pNode = pLoopDeal->getFirstTrades()->head(); pNode; pNode = pLoopDeal->getFirstTrades()->next(pNode)) {
						if (pNode->m_data.m_eItemType == eItem) {
							bValid = false;
						}
					}
				}
			}

			if (bValid && GET_PLAYER(pLoopDeal->getFirstPlayer()).getTeam() == eToTeam && GET_PLAYER(pLoopDeal->getSecondPlayer()).getTeam() == eFromTeam) {
				if (pLoopDeal->getSecondTrades() != NULL) {
					for (CLLNode<TradeData>* pNode = pLoopDeal->getSecondTrades()->head(); pNode; pNode = pLoopDeal->getSecondTrades()->next(pNode)) {
						if (pNode->m_data.m_eItemType == eItem) {
							bValid = false;
						}
					}
				}
			}

			if (!bValid) {
				pLoopDeal->kill(false);
			}
		}
	}
}

bool CvDeal::isCancelable(PlayerTypes eByPlayer, CvWString* pszReason) {
	if (isUncancelableVassalDeal(eByPlayer, pszReason)) {
		return false;
	}

	int iTurns = turnsToCancel(eByPlayer);
	if (pszReason) {
		if (iTurns > 0) {
			*pszReason = gDLL->getText("TXT_KEY_MISC_DEAL_NO_CANCEL", iTurns);
		}
	}

	return (iTurns <= 0);
}

int CvDeal::turnsToCancel(PlayerTypes eByPlayer) {
	return (getInitialGameTurn() + GC.getDefineINT("PEACE_TREATY_LENGTH") - GC.getGameINLINE().getGameTurn());
}

// static
bool CvDeal::isAnnual(TradeableItems eItem) {
	switch (eItem) {
	case TRADE_RESOURCES:
	case TRADE_GOLD_PER_TURN:
	case TRADE_VASSAL:
	case TRADE_SURRENDER:
	case TRADE_OPEN_BORDERS:
	case TRADE_DEFENSIVE_PACT:
	case TRADE_PERMANENT_ALLIANCE:
	case TRADE_EMBASSY:
	case TRADE_LIMITED_BORDERS:
	case TRADE_FREE_TRADE_ZONE:
	case TRADE_NON_AGGRESSION:
		return true;
		break;
	}

	return false;
}

// static
bool CvDeal::isDual(TradeableItems eItem, bool bExcludePeace) {
	switch (eItem) {
	case TRADE_OPEN_BORDERS:
	case TRADE_DEFENSIVE_PACT:
	case TRADE_PERMANENT_ALLIANCE:
	case TRADE_EMBASSY:
	case TRADE_LIMITED_BORDERS:
	case TRADE_FREE_TRADE_ZONE:
	case TRADE_NON_AGGRESSION:
		return true;
	case TRADE_PEACE_TREATY:
		return (!bExcludePeace);
	}

	return false;
}

// static
bool CvDeal::hasData(TradeableItems eItem) {
	return (eItem != TRADE_MAPS &&
		eItem != TRADE_VASSAL &&
		eItem != TRADE_SURRENDER &&
		eItem != TRADE_OPEN_BORDERS &&
		eItem != TRADE_DEFENSIVE_PACT &&
		eItem != TRADE_PERMANENT_ALLIANCE &&
		eItem != TRADE_EMBASSY &&
		eItem != TRADE_NON_AGGRESSION &&
		eItem != TRADE_PEACE_TREATY);
}

bool CvDeal::isGold(TradeableItems eItem) {
	return (eItem == getGoldItem() || eItem == getGoldPerTurnItem());
}

bool CvDeal::isVassal(TradeableItems eItem) {
	return (eItem == TRADE_VASSAL || eItem == TRADE_SURRENDER);
}

bool CvDeal::isEndWar(TradeableItems eItem) {
	if (eItem == getPeaceItem()) {
		return true;
	}

	if (isVassal(eItem)) {
		return true;
	}

	return false;
}

TradeableItems CvDeal::getPeaceItem() {
	return TRADE_PEACE_TREATY;
}

TradeableItems CvDeal::getGoldItem() {
	return TRADE_GOLD;
}

TradeableItems CvDeal::getGoldPerTurnItem() {
	return TRADE_GOLD_PER_TURN;
}

bool CvDeal::isEmbassy() {
	CLLNode<TradeData>* pNode;

	for (pNode = headFirstTradesNode(); (pNode != NULL); pNode = nextFirstTradesNode(pNode)) {
		if (pNode->m_data.m_eItemType == TRADE_EMBASSY) {
			return true;
		}
	}

	for (pNode = headSecondTradesNode(); (pNode != NULL); pNode = nextSecondTradesNode(pNode)) {
		if (pNode->m_data.m_eItemType == TRADE_EMBASSY) {
			return true;
		}
	}

	return false;
}
