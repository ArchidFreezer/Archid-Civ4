#include "CvGameCoreDLL.h"
#include "CvSpy.h"


CvSpy::CvSpy()
{
	reset();
}


CvSpy::~CvSpy()
{
}

void CvSpy::reset()
{
	m_iCorporationRemovalChange = 0;
	m_iCultureChange = 0;
	m_iDestroyImprovementChange = 0;
	m_iDiplomacyPenalty = 0;
	m_iDisablePowerChange = 0;
	m_iEscapeChanceExtra = 0;
	m_iEvasionChanceExtra = 0;
	m_iInterceptChanceExtra = 0;
	m_iNukeCityChange = 0;
	m_iPoisonChangeExtra = 0;
	m_iPreparationModifier = 0;
	m_iRadiationCount = 0;
	m_iReligionRemovalChange = 0;
	m_iRevoltChange = 0;
	m_iSwitchCivicChange = 0;
	m_iSwitchReligionChange = 0;
	m_iUnhappyChange = 0;
	m_iWarWearinessChange = 0;
	m_iDestroyBuildingChange = 0;
	m_iDestroyProductionChange = 0;
	m_iDestroyProjectChange = 0;
	m_iResearchSabotageChange = 0;
	m_iBuyTechChange = 0;
	m_iStealTreasuryChange = 0;

	m_eOriginalSpymaster = NO_PLAYER;
}

void CvSpy::read(FDataStreamBase* pStream)
{
	reset();

	pStream->Read(&m_iCorporationRemovalChange);
	pStream->Read(&m_iCultureChange);
	pStream->Read(&m_iDestroyImprovementChange);
	pStream->Read(&m_iDiplomacyPenalty);
	pStream->Read(&m_iDisablePowerChange);
	pStream->Read(&m_iEscapeChanceExtra);
	pStream->Read(&m_iEvasionChanceExtra);
	pStream->Read(&m_iInterceptChanceExtra);
	pStream->Read(&m_iNukeCityChange);
	pStream->Read(&m_iPreparationModifier);
	pStream->Read(&m_iPoisonChangeExtra);
	pStream->Read(&m_iRadiationCount);
	pStream->Read(&m_iReligionRemovalChange);
	pStream->Read(&m_iRevoltChange);
	pStream->Read(&m_iSwitchCivicChange);
	pStream->Read(&m_iSwitchReligionChange);
	pStream->Read(&m_iUnhappyChange);
	pStream->Read(&m_iWarWearinessChange);
	pStream->Read(&m_iDestroyBuildingChange);
	pStream->Read(&m_iDestroyProductionChange);
	pStream->Read(&m_iDestroyProjectChange);
	pStream->Read(&m_iResearchSabotageChange);
	pStream->Read(&m_iBuyTechChange);
	pStream->Read(&m_iStealTreasuryChange);
	pStream->Read((int*)&m_eOriginalSpymaster);
}

void CvSpy::write(FDataStreamBase* pStream)
{
	pStream->Write(m_iCorporationRemovalChange);
	pStream->Write(m_iCultureChange);
	pStream->Write(m_iDestroyImprovementChange);
	pStream->Write(m_iDiplomacyPenalty);
	pStream->Write(m_iDisablePowerChange);
	pStream->Write(m_iEscapeChanceExtra);
	pStream->Write(m_iEvasionChanceExtra);
	pStream->Write(m_iInterceptChanceExtra);
	pStream->Write(m_iNukeCityChange);
	pStream->Write(m_iPoisonChangeExtra);
	pStream->Write(m_iPreparationModifier);
	pStream->Write(m_iRadiationCount);
	pStream->Write(m_iReligionRemovalChange);
	pStream->Write(m_iRevoltChange);
	pStream->Write(m_iSwitchCivicChange);
	pStream->Write(m_iSwitchReligionChange);
	pStream->Write(m_iUnhappyChange);
	pStream->Write(m_iWarWearinessChange);
	pStream->Write(m_iDestroyBuildingChange);
	pStream->Write(m_iDestroyProductionChange);
	pStream->Write(m_iDestroyProjectChange);
	pStream->Write(m_iResearchSabotageChange);
	pStream->Write(m_iBuyTechChange);
	pStream->Write(m_iStealTreasuryChange);
	pStream->Write(m_eOriginalSpymaster);
}

int CvSpy::getEvasionChance() const
{
	return getEvasionChanceExtra();
}

int CvSpy::getEvasionChanceExtra() const
{
	return m_iEvasionChanceExtra;
}

void CvSpy::changeEvasionChanceExtra(int iChange)
{
	m_iEvasionChanceExtra += iChange;
}

int CvSpy::getCorporationRemovalChange() const
{
	return m_iCorporationRemovalChange;
}

void CvSpy::changeCorporationRemovalChange(int iChange)
{
	m_iCorporationRemovalChange += iChange;
}

int CvSpy::getPreparationModifier() const
{
	return m_iPreparationModifier;
}

void CvSpy::changePreparationModifier(int iChange)
{
	m_iPreparationModifier += iChange;
}

int CvSpy::getCultureChange() const
{
	return m_iCultureChange;
}

void CvSpy::changeCultureChange(int iChange)
{
	m_iCultureChange+= iChange;
}

int CvSpy::getPoisonChangeExtra() const
{
	return m_iPoisonChangeExtra;
}

void CvSpy::changePoisonChangeExtra(int iChange)
{
	m_iPoisonChangeExtra += iChange;
}

int CvSpy::getDestroyImprovementChange() const
{
	return m_iDestroyImprovementChange;
}

void CvSpy::changeDestroyImprovementChange(int iChange)
{
	m_iDestroyImprovementChange += iChange;
}

int CvSpy::getRadiationCount() const
{
	return m_iRadiationCount;
}

void CvSpy::changeRadiationCount(int iChange)
{
	m_iRadiationCount += iChange;
}

int CvSpy::getDiplomacyPenalty() const
{
	return m_iDiplomacyPenalty;
}

void CvSpy::changeDiplomacyPenalty(int iChange)
{
	m_iDiplomacyPenalty += iChange;
}

int CvSpy::getNukeCityChange() const
{
	return m_iNukeCityChange;
}

void CvSpy::changeNukeCityChange(int iChange)
{
	m_iNukeCityChange += iChange;
}

int CvSpy::getSwitchCivicChange() const
{
	return m_iSwitchCivicChange;
}

void CvSpy::changeSwitchCivicChange(int iChange)
{
	m_iSwitchCivicChange += iChange;
}

int CvSpy::getSwitchReligionChange() const
{
	return m_iSwitchReligionChange;
}

void CvSpy::changeSwitchReligionChange(int iChange)
{
	m_iSwitchReligionChange += iChange;
}

int CvSpy::getDisablePowerChange() const
{
	return m_iDisablePowerChange;
}

void CvSpy::changeDisablePowerChange(int iChange)
{
	m_iDisablePowerChange += iChange;
}

int CvSpy::getEscapeChance() const
{
	return getEscapeChanceExtra();
}

int CvSpy::getEscapeChanceExtra() const
{
	return m_iEscapeChanceExtra;
}

void CvSpy::changeEscapeChanceExtra(int iChange)			
{
	m_iEscapeChanceExtra += iChange;
}

int CvSpy::getInterceptChance() const
{
	return getInterceptChanceExtra();
}

int CvSpy::getInterceptChanceExtra() const
{
	return m_iInterceptChanceExtra;
}

void CvSpy::changeInterceptChanceExtra(int iChange)			
{
	m_iInterceptChanceExtra += iChange;
}

int CvSpy::getUnhappyChange() const
{
	return m_iUnhappyChange;
}

void CvSpy::changeUnhappyChange(int iChange)			
{
	m_iUnhappyChange += iChange;
}

int CvSpy::getRevoltChange() const
{
	return m_iRevoltChange;
}

void CvSpy::changeRevoltChange(int iChange)			
{
	m_iRevoltChange += iChange;
}

int CvSpy::getWarWearinessChange() const
{
	return m_iWarWearinessChange;
}

void CvSpy::changeWarWearinessChange(int iChange)
{
	m_iWarWearinessChange += iChange;
}

int CvSpy::getReligionRemovalChange() const
{
	return m_iReligionRemovalChange;
}

void CvSpy::changeReligionRemovalChange(int iChange)
{
	m_iReligionRemovalChange += iChange;
}

PlayerTypes CvSpy::getOriginalSpymaster() const
{
	return m_eOriginalSpymaster;
}

void CvSpy::setOriginalSpymaster(PlayerTypes ePlayer)
{
	m_eOriginalSpymaster = ePlayer;
}

int CvSpy::getResearchSabotageChange() const {
	return m_iResearchSabotageChange;
}

void CvSpy::changeResearchSabotageChange(int iChange) {
	m_iResearchSabotageChange += iChange;
}

int CvSpy::getDestroyProjectChange() const {
	return m_iDestroyProjectChange;
}

void CvSpy::changeDestroyProjectChange(int iChange) {
	m_iDestroyProjectChange += iChange;
}

int CvSpy::getDestroyBuildingChange() const {
	return m_iDestroyBuildingChange;
}

void CvSpy::changeDestroyBuildingChange(int iChange) {
	m_iDestroyBuildingChange += iChange;
}

int CvSpy::getDestroyProductionChange() const {
	return m_iDestroyProductionChange;
}

void CvSpy::changeDestroyProductionChange(int iChange) {
	m_iDestroyProductionChange += iChange;
}

int CvSpy::getBuyTechChange() const {
	return m_iBuyTechChange;
}

void CvSpy::changeBuyTechChange(int iChange) {
	m_iBuyTechChange += iChange;
}

int CvSpy::getStealTreasuryChange() const {
	return m_iStealTreasuryChange;
}

void CvSpy::changeStealTreasuryChange(int iChange) {
	m_iStealTreasuryChange += iChange;
}
