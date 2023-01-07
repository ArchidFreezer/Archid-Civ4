#pragma once

#ifndef CV_SPYINFO_H
#define CV_SPYINFO_H

class CvSpy
{
public:
	CvSpy();
	~CvSpy();

	void reset();
	void read(FDataStreamBase* pStream);
	void write(FDataStreamBase* pStream);

	int getEvasionChance() const;
	int getEvasionChanceExtra() const;
	int getPreparationModifier() const;
	int getDestroyImprovementChange() const;
	int getPoisonChangeExtra() const;
	int getRadiationCount() const;
	int getDiplomacyPenalty() const;
	int getDisablePowerChange() const;
	int getNukeCityChange() const;
	int getSwitchCivicChange() const;
	int getSwitchReligionChange() const;
	int getEscapeChance() const;
	int getEscapeChanceExtra() const;
	int getInterceptChance() const;
	int getInterceptChanceExtra() const;
	int getCorporationRemovalChange() const;
	int getCultureChange() const;
	int getReligionRemovalChange() const;
	int getRevoltChange() const;
	int getUnhappyChange() const;
	int getWarWearinessChange() const;
	int getDestroyBuildingChange() const;
	int getDestroyProductionChange() const;
	int getDestroyProjectChange() const;
	int getResearchSabotageChange() const;

	void changeEvasionChanceExtra(int iChange);
	void changePreparationModifier(int iChange);
	void changeDestroyImprovementChange(int iChange);
	void changePoisonChangeExtra(int iChange);
	void changeRadiationCount(int iChange);
	void changeDiplomacyPenalty(int iChange);
	void changeDisablePowerChange(int iChange);
	void changeNukeCityChange(int iChange);
	void changeSwitchCivicChange(int iChange);
	void changeSwitchReligionChange(int iChange);
	void changeEscapeChanceExtra(int iChange);
	void changeInterceptChanceExtra(int iChange);
	void changeCorporationRemovalChange(int iChange);
	void changeCultureChange(int iChange);
	void changeReligionRemovalChange(int iChange);
	void changeRevoltChange(int iChange);
	void changeUnhappyChange(int iChange);
	void changeWarWearinessChange(int iChange);
	void setOriginalSpymaster(PlayerTypes ePlayer);
	void changeDestroyBuildingChange(int iChange);
	void changeDestroyProductionChange(int iChange);
	void changeDestroyProjectChange(int iChange);
	void changeResearchSabotageChange(int iChange);

	PlayerTypes getOriginalSpymaster() const;

protected:
	int m_iEvasionChanceExtra;
	int m_iPreparationModifier;
	int m_iPoisonChangeExtra;
	int m_iDestroyImprovementChange;
	int m_iRadiationCount;
	int m_iDiplomacyPenalty;
	int m_iNukeCityChange;
	int m_iSwitchCivicChange;
	int m_iSwitchReligionChange;
	int m_iDisablePowerChange;
	int m_iEscapeChanceExtra;
	int m_iInterceptChanceExtra;
	int m_iUnhappyChange;
	int m_iRevoltChange;
	int m_iWarWearinessChange;
	int m_iReligionRemovalChange;
	int m_iCorporationRemovalChange;
	int m_iCultureChange;
	int m_iDestroyBuildingChange;
	int m_iDestroyProductionChange;
	int m_iDestroyProjectChange;
	int m_iResearchSabotageChange;

	PlayerTypes m_eOriginalSpymaster;
};

#endif