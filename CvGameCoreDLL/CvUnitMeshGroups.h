#pragma once

#include "CvInfos.h"
#include "CvGlobals.h"

#ifndef CIV4_UNITMESHGROUPS_H
#define CIV4_UNITMESHGROUPS_H

class CvUnitMeshGroups
{
public:
	CvUnitMeshGroups(void);
	~CvUnitMeshGroups(void);

	void reset();

	void init(UnitTypes eUnitType);
	void addGroup(int iIndex, int iNumRequired, const TCHAR* szEarlyArtDefineTag, const TCHAR* szMiddleArtDefineTag, const TCHAR* szLateArtDefineTag);

	bool isAddedGroup(int i) const;
	int getBaseIndex(int i) const;

	int getGroupSize() const;									// Exposed to Python - the initial number of individuals in the unit group
	int getMeleeWaveSize() const;							// Exposed to Python
	int getRangedWaveSize() const;						// Exposed to Python

	float getUnitMaxSpeed() const;					// Exposed to Python
	float getUnitPadTime() const;					// Exposed to Python

	UnitTypes getBaseType() const;

	int getGroupDefinitions() const;					// Exposed to Python - the number of UnitMeshGroups for this unit
	int getUnitGroupRequired(int i) const;

	const TCHAR* getEarlyArtDefineTag(int i, UnitArtStyleTypes eStyle) const;				// Exposed to Python
	const TCHAR* getLateArtDefineTag(int i, UnitArtStyleTypes eStyle) const;				// Exposed to Python
	const TCHAR* getMiddleArtDefineTag(int i, UnitArtStyleTypes eStyle) const;				// Exposed to Python

	const CvArtInfoUnit* getArtInfo(int i, EraTypes eEra, UnitArtStyleTypes eStyle) const;

	void read(FDataStreamBase* stream);
	void write(FDataStreamBase* stream);

protected:
	int m_iGroupSize;
	int m_iMeleeWaveSize;
	int m_iRangedWaveSize;
	
	float m_fUnitMaxSpeed;
	float m_fUnitPadTime;

	UnitTypes m_eBaseType;

	std::vector<int> m_viAddedGroups;
	std::vector<int> m_viUnitGroupRequired;

	std::vector<CvString> m_vszEarlyArtDefineTags;
	std::vector<CvString> m_vszLateArtDefineTags;
	std::vector<CvString> m_vszMiddleArtDefineTags;

};

#endif