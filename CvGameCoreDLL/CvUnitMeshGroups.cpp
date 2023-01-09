#include "CvGameCoreDLL.h"
#include "CvUnitMeshGroups.h"

CvUnitMeshGroups::CvUnitMeshGroups()
{
	reset();
}

CvUnitMeshGroups::~CvUnitMeshGroups()
{
}

void CvUnitMeshGroups::reset()
{
	m_eBaseType = NO_UNIT;
	m_iGroupSize = 0;
	m_iMeleeWaveSize = 0;
	m_iRangedWaveSize = 0;
	m_fUnitMaxSpeed = 0.0f;
	m_fUnitPadTime = 0.0f;

	m_viAddedGroups.clear();
	m_viUnitGroupRequired.clear();
	m_vszEarlyArtDefineTags.clear();
	m_vszMiddleArtDefineTags.clear();
	m_vszLateArtDefineTags.clear();
}

void CvUnitMeshGroups::init(UnitTypes eUnitType)
{
	if (eUnitType <= NO_UNIT)
		return;

	reset();

	const CvUnitInfo& kUnit = GC.getUnitInfo(eUnitType);
	m_eBaseType = eUnitType;
	m_iGroupSize = kUnit.getGroupSize();
	m_iMeleeWaveSize = kUnit.getMeleeWaveSize();
	m_iRangedWaveSize = kUnit.getRangedWaveSize();
	m_fUnitMaxSpeed = kUnit.getUnitMaxSpeed();
	m_fUnitPadTime = kUnit.getUnitPadTime();

	int iNumGroups = kUnit.getGroupDefinitions();
	for (int i = 0; i < iNumGroups; i++)
	{
		m_viUnitGroupRequired.push_back(kUnit.getUnitGroupRequired(i));
		m_vszEarlyArtDefineTags.push_back(kUnit.getEarlyArtDefineTag(i, NO_UNIT_ARTSTYLE));
		m_vszMiddleArtDefineTags.push_back(kUnit.getMiddleArtDefineTag(i, NO_UNIT_ARTSTYLE));
		m_vszLateArtDefineTags.push_back(kUnit.getLateArtDefineTag(i, NO_UNIT_ARTSTYLE));
	}
}

void CvUnitMeshGroups::addGroup(int iIndex, int iNumRequired, const TCHAR* szEarlyArtDefineTag, const TCHAR* szMiddleArtDefineTag, const TCHAR* szLateArtDefineTag)
{
	m_iGroupSize++;
	m_iMeleeWaveSize += m_iMeleeWaveSize > 0 ? 1 : 0;
	m_iRangedWaveSize += m_iRangedWaveSize > 0 ? 1 : 0;
	m_viAddedGroups.push_back(iIndex);
	std::vector<int>::iterator it = m_viUnitGroupRequired.begin();
	m_viUnitGroupRequired.insert(it + iIndex, iNumRequired);
	std::vector<CvString>::iterator sit = m_vszEarlyArtDefineTags.begin();
	m_vszEarlyArtDefineTags.insert(sit + iIndex, szEarlyArtDefineTag);
	sit = m_vszMiddleArtDefineTags.begin();
	m_vszMiddleArtDefineTags.insert(sit + iIndex, szMiddleArtDefineTag);
	sit = m_vszLateArtDefineTags.begin();
	m_vszLateArtDefineTags.insert(sit + iIndex, szLateArtDefineTag);
}

int CvUnitMeshGroups::getGroupSize() const
{
	return m_iGroupSize;
}

int CvUnitMeshGroups::getMeleeWaveSize() const
{
	return m_iMeleeWaveSize;
}

int CvUnitMeshGroups::getRangedWaveSize() const
{
	return m_iRangedWaveSize;
}

float CvUnitMeshGroups::getUnitMaxSpeed() const
{
	return m_fUnitMaxSpeed;
}

float CvUnitMeshGroups::getUnitPadTime() const
{
	return m_fUnitPadTime;
}

int CvUnitMeshGroups::getGroupDefinitions() const
{
	return (int)m_viUnitGroupRequired.size();
}

int CvUnitMeshGroups::getUnitGroupRequired(int i) const
{
	return m_viUnitGroupRequired[i];
}

int CvUnitMeshGroups::getBaseIndex(int i) const
{
	// We need to find out how many rows have been added before this index and subtract that number
	int iBaseIndex = i;
	for (std::vector<int>::const_iterator it = m_viAddedGroups.begin(); it != m_viAddedGroups.end(); ++it)
	{
		if ((*it) < i)
			iBaseIndex--;
	}
	return iBaseIndex;
}

bool CvUnitMeshGroups::isAddedGroup(int i) const
{
	return std::find(m_viAddedGroups.begin(), m_viAddedGroups.end(), i) != m_viAddedGroups.end();
}

const TCHAR* CvUnitMeshGroups::getEarlyArtDefineTag(int i, UnitArtStyleTypes eStyle) const
{
	if (NO_UNIT_ARTSTYLE != eStyle && NO_UNIT != m_eBaseType && !isAddedGroup(i))
	{
		const TCHAR* pcTag = GC.getUnitArtStyleTypeInfo(eStyle).getEarlyArtDefineTag(getBaseIndex(i), m_eBaseType);
		if (NULL != pcTag)
		{
			return pcTag;
		}
	}

	return ((int)m_vszEarlyArtDefineTags.size() >= i) ? m_vszEarlyArtDefineTags[i] : NULL;
}

const TCHAR* CvUnitMeshGroups::getLateArtDefineTag(int i, UnitArtStyleTypes eStyle) const
{
	if (NO_UNIT_ARTSTYLE != eStyle && NO_UNIT != m_eBaseType && !isAddedGroup(i))
	{
		const TCHAR* pcTag = GC.getUnitArtStyleTypeInfo(eStyle).getLateArtDefineTag(getBaseIndex(i), m_eBaseType);
		if (NULL != pcTag)
		{
			return pcTag;
		}
	}

	return ((int)m_vszLateArtDefineTags.size() >= i) ? m_vszLateArtDefineTags[i] : NULL;
}

const TCHAR* CvUnitMeshGroups::getMiddleArtDefineTag(int i, UnitArtStyleTypes eStyle) const
{
	if (NO_UNIT_ARTSTYLE != eStyle && NO_UNIT != m_eBaseType && !isAddedGroup(i))
	{
		const TCHAR* pcTag = GC.getUnitArtStyleTypeInfo(eStyle).getMiddleArtDefineTag(getBaseIndex(i), m_eBaseType);
		if (NULL != pcTag)
		{
			return pcTag;
		}
	}

	return ((int)m_vszMiddleArtDefineTags.size() >= i) ? m_vszMiddleArtDefineTags[i] : NULL;
}

const CvArtInfoUnit* CvUnitMeshGroups::getArtInfo(int i, EraTypes eEra, UnitArtStyleTypes eStyle) const
{
	if ((eEra > GC.getNumEraInfos() / 2) && !CvString(getLateArtDefineTag(i, eStyle)).empty())
	{
		return ARTFILEMGR.getUnitArtInfo(getLateArtDefineTag(i, eStyle));
	}
	else if ((eEra > GC.getNumEraInfos() / 4) && !CvString(getMiddleArtDefineTag(i, eStyle)).empty())
	{
		return ARTFILEMGR.getUnitArtInfo(getMiddleArtDefineTag(i, eStyle));
	}
	else
	{
		return ARTFILEMGR.getUnitArtInfo(getEarlyArtDefineTag(i, eStyle));
	}
}

void CvUnitMeshGroups::read(FDataStreamBase* stream)
{
	stream->Read(&m_iGroupSize);
	stream->Read(&m_iMeleeWaveSize);
	stream->Read(&m_iRangedWaveSize);
	stream->Read(&m_fUnitMaxSpeed);
	stream->Read(&m_fUnitPadTime);
	stream->Read((int*)&m_eBaseType);

	int iSize;
	int iElement;
	stream->Read(&iSize);
	m_viAddedGroups.clear();
  for (int i = 0; i < iSize; ++i)
  {
    stream->Read(&iElement);
		m_viAddedGroups.push_back(iElement);
	}

	stream->Read(&iSize);
	m_viUnitGroupRequired.clear();
  for (int i = 0; i < iSize; ++i)
  {
    stream->Read(&iElement);
		m_viUnitGroupRequired.push_back(iElement);
	}

	CvString szString;
	m_vszEarlyArtDefineTags.clear();
  for (int i = 0; i < iSize; ++i)
  {
    stream->ReadString(szString);
		m_vszEarlyArtDefineTags.push_back(szString);
	}

	m_vszMiddleArtDefineTags.clear();
  for (int i = 0; i < iSize; ++i)
  {
    stream->ReadString(szString);
		m_vszMiddleArtDefineTags.push_back(szString);
  }

	m_vszLateArtDefineTags.clear();
  for (int i = 0; i < iSize; ++i)
  {
    stream->ReadString(szString);
		m_vszLateArtDefineTags.push_back(szString);
  }
}

void CvUnitMeshGroups::write(FDataStreamBase* stream)
{
	stream->Write(m_iGroupSize);
	stream->Write(m_iMeleeWaveSize);
	stream->Write(m_iRangedWaveSize);
	stream->Write(m_fUnitMaxSpeed);
	stream->Write(m_fUnitPadTime);
	stream->Write(m_eBaseType);

	stream->Write(m_viAddedGroups.size());
	for (std::vector<int>::iterator it = m_viAddedGroups.begin(); it != m_viAddedGroups.end(); ++it)
	{
		stream->Write(*it);
	}

	stream->Write(m_viUnitGroupRequired.size());
	for (std::vector<int>::iterator it = m_viUnitGroupRequired.begin(); it != m_viUnitGroupRequired.end(); ++it)
	{
		stream->Write(*it);
	}
	for (std::vector<CvString>::iterator it = m_vszEarlyArtDefineTags.begin(); it != m_vszEarlyArtDefineTags.end(); ++it)
	{
		stream->WriteString(*it);
	}
	for (std::vector<CvString>::iterator it = m_vszMiddleArtDefineTags.begin(); it != m_vszMiddleArtDefineTags.end(); ++it)
	{
		stream->WriteString(*it);
	}
	for (std::vector<CvString>::iterator it = m_vszLateArtDefineTags.begin(); it != m_vszLateArtDefineTags.end(); ++it)
	{
		stream->WriteString(*it);
	}
	
}
