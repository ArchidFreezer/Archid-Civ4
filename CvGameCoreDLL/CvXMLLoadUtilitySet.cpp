//
// XML Set functions
//

#include "CvGameCoreDLL.h"
#include "CvDLLXMLIFaceBase.h"
#include "CvXMLLoadUtility.h"
#include "CvGlobals.h"
#include "CvArtFileMgr.h"
#include "CvGameTextMgr.h"
#include <algorithm>
#include "CvInfoWater.h"
#include "FProfiler.h"
#include "FVariableSystem.h"
#include "CvGameCoreUtils.h"

/// XML type preloading - start - Nightinggale

// the concept of preloading XML file types is as follows:
// As soon as possible, all XML files are read for their basic info.
// Just before reaching main menu, all files are read again, this time for their entire content.
//
// The result is that GC becomes aware of indexes for type strings and vector lengths right away.
// This mean functions like FindInInfoClass() and getNumUnitInfos() works much earlier in the startup process.
// Very importantly, they work for all types prior to actually reading any XML content or even CvPlayer constructor.
//
// The actual content is read just before reaching the main menu.
// This can't be done earlier due to other setup handled by the exe, but it is complete before reaching main menu.
//
// XML types are checked for name clashes. It will assert if a name is used more than once.
// Bugs caused by nameclashes are hard to debug and find the cause for. Example:
// If unit 17 clashes with a name giving the string ID 4, then it will overwrite unit 4.
// The bughunting will then focus on the missing unit 4, which is presumably bugfree.
//
// Effect on modules: unknown


bool bFirstRead = false;

// same as LoadGlobalClassInfo, except that it clears vectors without types
template <class T>
void CvXMLLoadUtility::PreLoadGlobalClassInfo(std::vector<T*>& aInfos, const char* szFileRoot, const char* szFileDirectory, const char* szXmlPath, CvCacheObject* (CvDLLUtilityIFaceBase::* pArgFunction) (const TCHAR*)) {
	if (!bFirstRead && aInfos.size() > 0 && aInfos[0]->getType() == NULL) {
		// Vector has content without types
		// with no types to find indexes for, all XML data will be added to the end
		// clear the vector to avoid duplicates (not to mention some with only basic info)
		aInfos.clear();
	}
	LoadGlobalClassInfo(aInfos, szFileRoot, szFileDirectory, szXmlPath, pArgFunction);
}

// progress display when reading actual data, not just the types
void CvXMLLoadUtility::PreUpdateProgressCB(const char* szMessage) {
	if (!bFirstRead) {
		UpdateProgressCB(szMessage);
	}
}

void CvXMLLoadUtility::readXMLfiles(bool bFirst) {
	bFirstRead = bFirst;

	if (bFirst) {
		UpdateProgressCB("Preload XML Types");
	}

	// LoadBasicInfos()
	PreLoadGlobalClassInfo(GC.getConceptInfo(), "CIV4BasicInfos", "BasicInfos", "Civ4BasicInfos/ConceptInfos/ConceptInfo");
	PreLoadGlobalClassInfo(GC.getNewConceptInfo(), "CIV4NewConceptInfos", "BasicInfos", "Civ4NewConceptInfos/NewConceptInfos/NewConceptInfo");
	PreLoadGlobalClassInfo(GC.getCityTabInfo(), "CIV4CityTabInfos", "BasicInfos", "Civ4CityTabInfos/CityTabInfos/CityTabInfo");
	PreLoadGlobalClassInfo(GC.getCalendarInfo(), "CIV4CalendarInfos", "BasicInfos", "Civ4CalendarInfos/CalendarInfos/CalendarInfo");
	PreLoadGlobalClassInfo(GC.getSeasonInfo(), "CIV4SeasonInfos", "BasicInfos", "Civ4SeasonInfos/SeasonInfos/SeasonInfo");
	PreLoadGlobalClassInfo(GC.getMonthInfo(), "CIV4MonthInfos", "BasicInfos", "Civ4MonthInfos/MonthInfos/MonthInfo");
	PreLoadGlobalClassInfo(GC.getDenialInfo(), "CIV4DenialInfos", "BasicInfos", "Civ4DenialInfos/DenialInfos/DenialInfo");
	PreLoadGlobalClassInfo(GC.getInvisibleInfo(), "CIV4InvisibleInfos", "BasicInfos", "Civ4InvisibleInfos/InvisibleInfos/InvisibleInfo");
	PreLoadGlobalClassInfo(GC.getUnitCombatInfo(), "CIV4UnitCombatInfos", "BasicInfos", "Civ4UnitCombatInfos/UnitCombatInfos/UnitCombatInfo");
	PreLoadGlobalClassInfo(GC.getDomainInfo(), "CIV4DomainInfos", "BasicInfos", "Civ4DomainInfos/DomainInfos/DomainInfo");
	PreLoadGlobalClassInfo(GC.getUnitAIInfo(), "CIV4UnitAIInfos", "BasicInfos", "Civ4UnitAIInfos/UnitAIInfos/UnitAIInfo");
	PreLoadGlobalClassInfo(GC.getAttitudeInfo(), "CIV4AttitudeInfos", "BasicInfos", "Civ4AttitudeInfos/AttitudeInfos/AttitudeInfo");
	PreLoadGlobalClassInfo(GC.getMemoryInfo(), "CIV4MemoryInfos", "BasicInfos", "Civ4MemoryInfos/MemoryInfos/MemoryInfo");

	// LoadPreMenuGlobals()
	PreLoadGlobalClassInfo(GC.getGameSpeedInfo(), "CIV4GameSpeedInfo", "GameInfo", "Civ4GameSpeedInfo/GameSpeedInfos/GameSpeedInfo");
	PreLoadGlobalClassInfo(GC.getTurnTimerInfo(), "CIV4TurnTimerInfo", "GameInfo", "Civ4TurnTimerInfo/TurnTimerInfos/TurnTimerInfo");
	PreLoadGlobalClassInfo(GC.getWorldInfo(), "CIV4WorldInfo", "GameInfo", "Civ4WorldInfo/WorldInfos/WorldInfo");
	PreLoadGlobalClassInfo(GC.getClimateInfo(), "CIV4ClimateInfo", "GameInfo", "Civ4ClimateInfo/ClimateInfos/ClimateInfo");
	PreLoadGlobalClassInfo(GC.getSeaLevelInfo(), "CIV4SeaLevelInfo", "GameInfo", "Civ4SeaLevelInfo/SeaLevelInfos/SeaLevelInfo");
	PreLoadGlobalClassInfo(GC.getAdvisorInfo(), "CIV4AdvisorInfos", "Interface", "Civ4AdvisorInfos/AdvisorInfos/AdvisorInfo");
	PreLoadGlobalClassInfo(GC.getTerrainInfo(), "CIV4TerrainInfos", "Terrain", "Civ4TerrainInfos/TerrainInfos/TerrainInfo");
	PreLoadGlobalClassInfo(GC.getEraInfo(), "CIV4EraInfos", "GameInfo", "Civ4EraInfos/EraInfos/EraInfo");
	PreLoadGlobalClassInfo(GC.getHurryInfo(), "CIV4HurryInfo", "GameInfo", "Civ4HurryInfo/HurryInfos/HurryInfo");
	PreLoadGlobalClassInfo(GC.getUnitClassInfo(), "CIV4UnitClassInfos", "Units", "Civ4UnitClassInfos/UnitClassInfos/UnitClassInfo");
	PreLoadGlobalClassInfo(GC.getSpecialistInfo(), "CIV4SpecialistInfos", "GameInfo", "Civ4SpecialistInfos/SpecialistInfos/SpecialistInfo");
	PreLoadGlobalClassInfo(GC.getVoteSourceInfo(), "CIV4VoteSourceInfos", "GameInfo", "Civ4VoteSourceInfos/VoteSourceInfos/VoteSourceInfo");
	PreLoadGlobalClassInfo(GC.getTechInfo(), "CIV4TechInfos", "Technologies", "Civ4TechInfos/TechInfos/TechInfo", &CvDLLUtilityIFaceBase::createTechInfoCacheObject);
	PreLoadGlobalClassInfo(GC.getFeatureInfo(), "Civ4FeatureInfos", "Terrain", "Civ4FeatureInfos/FeatureInfos/FeatureInfo");
	PreLoadGlobalClassInfo(GC.getReligionInfo(), "CIV4ReligionInfo", "GameInfo", "Civ4ReligionInfo/ReligionInfos/ReligionInfo");
	PreLoadGlobalClassInfo(GC.getAnimationCategoryInfo(), "CIV4AnimationInfos", "Units", "Civ4AnimationInfos/AnimationCategories/AnimationCategory");
	PreLoadGlobalClassInfo(GC.getAnimationPathInfo(), "CIV4AnimationPathInfos", "Units", "Civ4AnimationPathInfos/AnimationPaths/AnimationPath");
	PreLoadGlobalClassInfo(GC.getBonusClassInfo(), "CIV4BonusClassInfos", "Terrain", "Civ4BonusClassInfos/BonusClassInfos/BonusClassInfo");
	PreLoadGlobalClassInfo(GC.getBonusInfo(), "CIV4BonusInfos", "Terrain", "Civ4BonusInfos/BonusInfos/BonusInfo", &CvDLLUtilityIFaceBase::createBonusInfoCacheObject);
	PreLoadGlobalClassInfo(GC.getPromotionInfo(), "CIV4PromotionInfos", "Units", "Civ4PromotionInfos/PromotionInfos/PromotionInfo", &CvDLLUtilityIFaceBase::createPromotionInfoCacheObject);
	PreLoadGlobalClassInfo(GC.getTraitInfo(), "CIV4TraitInfos", "Civilizations", "Civ4TraitInfos/TraitInfos/TraitInfo");
	PreLoadGlobalClassInfo(GC.getGoodyInfo(), "CIV4GoodyInfo", "GameInfo", "Civ4GoodyInfo/GoodyInfos/GoodyInfo");
	PreLoadGlobalClassInfo(GC.getHandicapInfo(), "CIV4HandicapInfo", "GameInfo", "Civ4HandicapInfo/HandicapInfos/HandicapInfo", &CvDLLUtilityIFaceBase::createHandicapInfoCacheObject);
	PreLoadGlobalClassInfo(GC.getCursorInfo(), "CIV4CursorInfo", "GameInfo", "Civ4CursorInfo/CursorInfos/CursorInfo");
	PreLoadGlobalClassInfo(GC.getCivicOptionInfo(), "CIV4CivicOptionInfos", "GameInfo", "Civ4CivicOptionInfos/CivicOptionInfos/CivicOptionInfo");
	PreLoadGlobalClassInfo(GC.getUpkeepInfo(), "CIV4UpkeepInfo", "GameInfo", "Civ4UpkeepInfo/UpkeepInfos/UpkeepInfo");
	PreLoadGlobalClassInfo(GC.getSpecialBuildingInfo(), "CIV4SpecialBuildingInfos", "Buildings", "Civ4SpecialBuildingInfos/SpecialBuildingInfos/SpecialBuildingInfo");
	PreLoadGlobalClassInfo(GC.getCultureLevelInfo(), "CIV4CultureLevelInfo", "GameInfo", "Civ4CultureLevelInfo/CultureLevelInfos/CultureLevelInfo");
	PreLoadGlobalClassInfo(GC.getVictoryInfo(), "CIV4VictoryInfo", "GameInfo", "Civ4VictoryInfo/VictoryInfos/VictoryInfo");
	PreLoadGlobalClassInfo(GC.getCorporationInfo(), "CIV4CorporationInfo", "GameInfo", "Civ4CorporationInfo/CorporationInfos/CorporationInfo");
	PreLoadGlobalClassInfo(GC.getRouteInfo(), "Civ4RouteInfos", "Misc", "Civ4RouteInfos/RouteInfos/RouteInfo");
	PreLoadGlobalClassInfo(GC.getImprovementInfo(), "CIV4ImprovementInfos", "Terrain", "Civ4ImprovementInfos/ImprovementInfos/ImprovementInfo", &CvDLLUtilityIFaceBase::createImprovementInfoCacheObject);
	PreLoadGlobalClassInfo(GC.getBuildingClassInfo(), "CIV4BuildingClassInfos", "Buildings", "Civ4BuildingClassInfos/BuildingClassInfos/BuildingClassInfo");
	PreLoadGlobalClassInfo(GC.getBuildingInfo(), "CIV4BuildingInfos", "Buildings", "Civ4BuildingInfos/BuildingInfos/BuildingInfo", &CvDLLUtilityIFaceBase::createBuildingInfoCacheObject);
	PreLoadGlobalClassInfo(GC.getSpecialUnitInfo(), "CIV4SpecialUnitInfos", "Units", "Civ4SpecialUnitInfos/SpecialUnitInfos/SpecialUnitInfo");
	PreLoadGlobalClassInfo(GC.getProjectInfo(), "CIV4ProjectInfo", "GameInfo", "Civ4ProjectInfo/ProjectInfos/ProjectInfo");
	PreLoadGlobalClassInfo(GC.getCivicInfo(), "CIV4CivicInfos", "GameInfo", "Civ4CivicInfos/CivicInfos/CivicInfo", &CvDLLUtilityIFaceBase::createCivicInfoCacheObject);
	PreLoadGlobalClassInfo(GC.getLeaderHeadInfo(), "CIV4LeaderHeadInfos", "Civilizations", "Civ4LeaderHeadInfos/LeaderHeadInfos/LeaderHeadInfo", &CvDLLUtilityIFaceBase::createLeaderHeadInfoCacheObject);
	PreLoadGlobalClassInfo(GC.getColorInfo(), "CIV4ColorVals", "Interface", "Civ4ColorVals/ColorVals/ColorVal");
	PreLoadGlobalClassInfo(GC.getPlayerColorInfo(), "CIV4PlayerColorInfos", "Interface", "Civ4PlayerColorInfos/PlayerColorInfos/PlayerColorInfo");
	PreLoadGlobalClassInfo(GC.getEffectInfo(), "CIV4EffectInfos", "Misc", "Civ4EffectInfos/EffectInfos/EffectInfo");
	PreLoadGlobalClassInfo(GC.getEntityEventInfo(), "CIV4EntityEventInfos", "Units", "Civ4EntityEventInfos/EntityEventInfos/EntityEventInfo");
	PreLoadGlobalClassInfo(GC.getBuildInfo(), "CIV4BuildInfos", "Units", "Civ4BuildInfos/BuildInfos/BuildInfo");
	PreLoadGlobalClassInfo(GC.getUnitInfo(), "CIV4UnitInfos", "Units", "Civ4UnitInfos/UnitInfos/UnitInfo", &CvDLLUtilityIFaceBase::createUnitInfoCacheObject);
	PreLoadGlobalClassInfo(GC.getUnitArtStyleTypeInfo(), "CIV4UnitArtStyleTypeInfos", "Civilizations", "Civ4UnitArtStyleTypeInfos/UnitArtStyleTypeInfos/UnitArtStyleTypeInfo");
	PreLoadGlobalClassInfo(GC.getCivilizationInfo(), "CIV4CivilizationInfos", "Civilizations", "Civ4CivilizationInfos/CivilizationInfos/CivilizationInfo", &CvDLLUtilityIFaceBase::createCivilizationInfoCacheObject);
	PreLoadGlobalClassInfo(GC.getHints(), "CIV4Hints", "GameInfo", "Civ4Hints/HintInfos/HintInfo");
	PreLoadGlobalClassInfo(GC.getMainMenus(), "CIV4MainMenus", "Art", "Civ4MainMenus/MainMenus/MainMenu");
	PreLoadGlobalClassInfo(GC.getSlideShowInfo(), "CIV4SlideShowInfos", "Interface", "Civ4SlideShowInfos/SlideShowInfos/SlideShowInfo");
	PreLoadGlobalClassInfo(GC.getSlideShowRandomInfo(), "CIV4SlideShowRandomInfos", "Interface", "Civ4SlideShowRandomInfos/SlideShowRandomInfos/SlideShowRandomInfo");
	PreLoadGlobalClassInfo(GC.getWorldPickerInfo(), "CIV4WorldPickerInfos", "Interface", "Civ4WorldPickerInfos/WorldPickerInfos/WorldPickerInfo");
	PreLoadGlobalClassInfo(GC.getSpaceShipInfo(), "Civ4SpaceShipInfos", "Interface", "Civ4SpaceShipInfos/SpaceShipInfos/SpaceShipInfo");
	PreLoadGlobalClassInfo(GC.getYieldInfo(), "CIV4YieldInfos", "Terrain", "Civ4YieldInfos/YieldInfos/YieldInfo");
	PreLoadGlobalClassInfo(GC.getCommerceInfo(), "CIV4CommerceInfo", "GameInfo", "Civ4CommerceInfo/CommerceInfos/CommerceInfo");
	PreLoadGlobalClassInfo(GC.getGameOptionInfo(), "CIV4GameOptionInfos", "GameInfo", "Civ4GameOptionInfos/GameOptionInfos/GameOptionInfo");
	PreLoadGlobalClassInfo(GC.getMPOptionInfo(), "CIV4MPOptionInfos", "GameInfo", "Civ4MPOptionInfos/MPOptionInfos/MPOptionInfo");
	PreLoadGlobalClassInfo(GC.getForceControlInfo(), "CIV4ForceControlInfos", "GameInfo", "Civ4ForceControlInfos/ForceControlInfos/ForceControlInfo");

	// SetupGlobalLandscapeInfo()
	PreLoadGlobalClassInfo(GC.getLandscapeInfo(), "CIV4TerrainSettings", "Terrain", "Civ4TerrainSettings/LandscapeInfos/LandscapeInfo");

	// LoadPostMenuGlobals
	//throne room disabled
	PreUpdateProgressCB("Global Throne Room");

	PreLoadGlobalClassInfo(GC.getThroneRoomCamera(), "CIV4ThroneRoomCameraInfos", "Interface", "Civ4ThroneRoomCameraInfos/ThroneRoomCameraInfos/ThroneRoomCamera");
	PreLoadGlobalClassInfo(GC.getThroneRoomInfo(), "CIV4ThroneRoomInfos", "Interface", "Civ4ThroneRoomInfos/ThroneRoomInfos/ThroneRoomInfo");
	PreLoadGlobalClassInfo(GC.getThroneRoomStyleInfo(), "CIV4ThroneRoomStyleInfos", "Interface", "Civ4ThroneRoomStyleInfos/ThroneRoomStyleInfos/ThroneRoomStyleInfo");

	PreUpdateProgressCB("Global Events");

	PreLoadGlobalClassInfo(GC.getEventInfo(), "CIV4EventInfos", "Events", "Civ4EventInfos/EventInfos/EventInfo", &CvDLLUtilityIFaceBase::createEventInfoCacheObject);
	PreLoadGlobalClassInfo(GC.getEventTriggerInfo(), "CIV4EventTriggerInfos", "Events", "Civ4EventTriggerInfos/EventTriggerInfos/EventTriggerInfo", &CvDLLUtilityIFaceBase::createEventTriggerInfoCacheObject);

	PreUpdateProgressCB("Global Routes");

	PreLoadGlobalClassInfo(GC.getRouteModelInfo(), "Civ4RouteModelInfos", "Art", "Civ4RouteModelInfos/RouteModelInfos/RouteModelInfo");

	PreUpdateProgressCB("Global Rivers");

	PreLoadGlobalClassInfo(GC.getRiverInfo(), "CIV4RiverInfos", "Misc", "Civ4RiverInfos/RiverInfos/RiverInfo");
	PreLoadGlobalClassInfo(GC.getRiverModelInfo(), "CIV4RiverModelInfos", "Art", "Civ4RiverModelInfos/RiverModelInfos/RiverModelInfo");

	PreUpdateProgressCB("Global Other");

	PreLoadGlobalClassInfo(GC.getWaterPlaneInfo(), "CIV4WaterPlaneInfos", "Misc", "Civ4WaterPlaneInfos/WaterPlaneInfos/WaterPlaneInfo");
	PreLoadGlobalClassInfo(GC.getTerrainPlaneInfo(), "CIV4TerrainPlaneInfos", "Misc", "Civ4TerrainPlaneInfos/TerrainPlaneInfos/TerrainPlaneInfo");
	PreLoadGlobalClassInfo(GC.getCameraOverlayInfo(), "CIV4CameraOverlayInfos", "Misc", "Civ4CameraOverlayInfos/CameraOverlayInfos/CameraOverlayInfo");


	PreUpdateProgressCB("Global Process");

	PreLoadGlobalClassInfo(GC.getProcessInfo(), "CIV4ProcessInfo", "GameInfo", "Civ4ProcessInfo/ProcessInfos/ProcessInfo");

	PreUpdateProgressCB("Global Emphasize");

	PreLoadGlobalClassInfo(GC.getEmphasizeInfo(), "CIV4EmphasizeInfo", "GameInfo", "Civ4EmphasizeInfo/EmphasizeInfos/EmphasizeInfo");

	PreUpdateProgressCB("Global Other");

	PreLoadGlobalClassInfo(GC.getMissionInfo(), "CIV4MissionInfos", "Units", "Civ4MissionInfos/MissionInfos/MissionInfo");
	PreLoadGlobalClassInfo(GC.getControlInfo(), "CIV4ControlInfos", "Units", "Civ4ControlInfos/ControlInfos/ControlInfo");
	PreLoadGlobalClassInfo(GC.getCommandInfo(), "CIV4CommandInfos", "Units", "Civ4CommandInfos/CommandInfos/CommandInfo");
	PreLoadGlobalClassInfo(GC.getAutomateInfo(), "CIV4AutomateInfos", "Units", "Civ4AutomateInfos/AutomateInfos/AutomateInfo");

	PreUpdateProgressCB("Global Vote");

	PreLoadGlobalClassInfo(GC.getVoteInfo(), "CIV4VoteInfo", "GameInfo", "Civ4VoteInfo/VoteInfos/VoteInfo");

	PreUpdateProgressCB("Global Interface");

	PreLoadGlobalClassInfo(GC.getCameraInfo(), "CIV4CameraInfos", "Interface", "Civ4CameraInfos/CameraInfos/CameraInfo");
	PreLoadGlobalClassInfo(GC.getInterfaceModeInfo(), "CIV4InterfaceModeInfos", "Interface", "Civ4InterfaceModeInfos/InterfaceModeInfos/InterfaceModeInfo");

	// Load the formation info
	PreLoadGlobalClassInfo(GC.getUnitFormationInfo(), "CIV4FormationInfos", "Units", "UnitFormations/UnitFormation");

	// Load the attachable infos
	PreLoadGlobalClassInfo(GC.getAttachableInfo(), "CIV4AttachableInfos", "Misc", "Civ4AttachableInfos/AttachableInfos/AttachableInfo");

	if (bFirst) {
		// preloading the types will not require the special case code
		PreLoadGlobalClassInfo(GC.getDiplomacyInfo(), "CIV4DiplomacyInfos", "GameInfo", "Civ4DiplomacyInfos/DiplomacyInfos/DiplomacyInfo", &CvDLLUtilityIFaceBase::createDiplomacyInfoCacheObject);
	} else {
		// Special Case Diplomacy Info due to double vectored nature and appending of Responses
		LoadDiplomacyInfo(GC.getDiplomacyInfo(), "CIV4DiplomacyInfos", "GameInfo", "Civ4DiplomacyInfos/DiplomacyInfos/DiplomacyInfo", &CvDLLUtilityIFaceBase::createDiplomacyInfoCacheObject);
	}

	PreLoadGlobalClassInfo(GC.getQuestInfo(), "Civ4QuestInfos", "Misc", "Civ4QuestInfos/QuestInfo");
	PreLoadGlobalClassInfo(GC.getTutorialInfo(), "Civ4TutorialInfos", "Misc", "Civ4TutorialInfos/TutorialInfo");

	PreLoadGlobalClassInfo(GC.getEspionageMissionInfo(), "CIV4EspionageMissionInfo", "GameInfo", "Civ4EspionageMissionInfo/EspionageMissionInfos/EspionageMissionInfo");


	if (!bFirst) {
		SetGlobalActionInfo();
	}

	bFirstRead = false;
}

/// XML type preloading - end - Nightinggale

// Macro for Setting Global Art Defines
#define INIT_XML_GLOBAL_LOAD(xmlInfoPath, infoArray, numInfos)  SetGlobalClassInfo(infoArray, xmlInfoPath, numInfos);

bool CvXMLLoadUtility::ReadGlobalDefines(const TCHAR* szXMLFileName, CvCacheObject* cache) {
	bool bLoaded = false;	// used to make sure that the xml file was loaded correctly

	if (!gDLL->cacheRead(cache, szXMLFileName))			// src data file name
	{
		// load normally
		if (!CreateFXml()) {
			return false;
		}

		// load the new FXml variable with the szXMLFileName file
		bLoaded = LoadCivXml(m_pFXml, szXMLFileName);
		if (!bLoaded) {
			char	szMessage[1024];
			sprintf(szMessage, "LoadXML call failed for %s \n Current XML file is: %s", szXMLFileName, GC.getCurrentXMLFile().GetCString());
			gDLL->MessageBox(szMessage, "XML Load Error");
		}

		// if the load succeeded we will continue
		if (bLoaded) {
			// locate the first define tag in the xml
			if (gDLL->getXMLIFace()->LocateNode(m_pFXml, "Civ4Defines/Define")) {
				int i;	// loop counter
				// get the number of other Define tags in the xml file
				int iNumDefines = gDLL->getXMLIFace()->GetNumSiblings(m_pFXml);
				// add one to the total in order to include the current Define tag
				iNumDefines++;

				// loop through all the Define tags
				for (i = 0; i < iNumDefines; i++) {
					char szNodeType[256];	// holds the type of the current node
					char szName[256];

					// Skip any comments and stop at the next value we might want
					if (SkipToNextVal()) {
						// call the function that sets the FXml pointer to the first non-comment child of
						// the current tag and gets the value of that new node
						if (GetChildXmlVal(szName)) {
							// set the FXml pointer to the next sibling of the current tag``
							if (gDLL->getXMLIFace()->NextSibling(GetXML())) {
								// Skip any comments and stop at the next value we might want
								if (SkipToNextVal()) {
									// if we successfuly get the node type for the current tag
									if (gDLL->getXMLIFace()->GetLastLocatedNodeType(GetXML(), szNodeType)) {
										// if the node type of the current tag isn't null
										if (strcmp(szNodeType, "") != 0) {
											// if the node type of the current tag is a float then
											if (strcmp(szNodeType, "float") == 0) {
												// get the float value for the define
												float fVal;
												GetXmlVal(&fVal);
												GC.getDefinesVarSystem()->SetValue(szName, fVal);
											}
											// else if the node type of the current tag is an int then
											else if (strcmp(szNodeType, "int") == 0) {
												// get the int value for the define
												int iVal;
												GetXmlVal(&iVal);
												GC.getDefinesVarSystem()->SetValue(szName, iVal);
											}
											// else if the node type of the current tag is a boolean then
											else if (strcmp(szNodeType, "boolean") == 0) {
												// get the boolean value for the define
												bool bVal;
												GetXmlVal(&bVal);
												GC.getDefinesVarSystem()->SetValue(szName, bVal);
											}
											// otherwise we will assume it is a string/text value
											else {
												char szVal[256];
												// get the string/text value for the define
												GetXmlVal(szVal);
												GC.getDefinesVarSystem()->SetValue(szName, szVal);
											}
										}
										// otherwise we will default to getting the string/text value for the define
										else {
											char szVal[256];
											// get the string/text value for the define
											GetXmlVal(szVal);
											GC.getDefinesVarSystem()->SetValue(szName, szVal);
										}
									}
								}
							}

							// since we are looking at the children of a Define tag we will need to go up
							// one level so that we can go to the next Define tag.
							// Set the FXml pointer to the parent of the current tag
							gDLL->getXMLIFace()->SetToParent(GetXML());
						}
					}

					// now we set the FXml pointer to the sibling of the current tag, which should be the next
					// Define tag
					if (!gDLL->getXMLIFace()->NextSibling(m_pFXml)) {
						break;
					}
				}

				// write global defines info to cache
				bool bOk = gDLL->cacheWrite(cache);
				if (!bOk) {
					char	szMessage[1024];
					sprintf(szMessage, "Failed writing to global defines cache. \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
					gDLL->MessageBox(szMessage, "XML Caching Error");
				} else {
					logMsg("Wrote GlobalDefines to cache");
				}
			}
		}

		// delete the pointer to the FXml variable
		gDLL->getXMLIFace()->DestroyFXml(m_pFXml);
	} else {
		logMsg("Read GobalDefines from cache");
	}

	return true;
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetGlobalDefines()
//
//  PURPOSE :   Initialize the variables located in globaldefines.cpp/h with the values in
//				GlobalDefines.xml
//
//------------------------------------------------------------------------------------------------------
bool CvXMLLoadUtility::SetGlobalDefines() {
	UpdateProgressCB("GlobalDefines");

	/////////////////////////////////
	//
	// use disk cache if possible.
	// if no cache or cache is older than xml file, use xml file like normal, else read from cache
	//

	CvCacheObject* cache = gDLL->createGlobalDefinesCacheObject("GlobalDefines.dat");	// cache file name

	if (!ReadGlobalDefines("xml\\GlobalDefines.xml", cache)) {
		return false;
	}

	if (!ReadGlobalDefines("xml\\GlobalDefinesAlt.xml", cache)) {
		return false;
	}

	if (!ReadGlobalDefines("xml\\PythonCallbackDefines.xml", cache)) {
		return false;
	}

	if (gDLL->isModularXMLLoading()) {
		std::vector<CvString> aszFiles;
		gDLL->enumerateFiles(aszFiles, "modules\\*_GlobalDefines.xml");

		for (std::vector<CvString>::iterator it = aszFiles.begin(); it != aszFiles.end(); ++it) {
			if (!ReadGlobalDefines(*it, cache)) {
				return false;
			}
		}

		std::vector<CvString> aszModularFiles;
		gDLL->enumerateFiles(aszModularFiles, "modules\\*_PythonCallbackDefines.xml");

		for (std::vector<CvString>::iterator it = aszModularFiles.begin(); it != aszModularFiles.end(); ++it) {
			if (!ReadGlobalDefines(*it, cache)) {
				return false;
			}
		}
	}


	gDLL->destroyCache(cache);
	////////////////////////////////////////////////////////////////////////

	GC.cacheGlobals();

	return true;
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetPostGlobalsGlobalDefines()
//
//  PURPOSE :   This function assumes that the SetGlobalDefines function has already been called
//							it then loads the few global defines that needed to reference a global variable that
//							hadn't been loaded in prior to the SetGlobalDefines call
//
//------------------------------------------------------------------------------------------------------
bool CvXMLLoadUtility::SetPostGlobalsGlobalDefines() {
	const char* szVal = NULL;		// holds the string value from the define queue
	int idx;

	if (GC.getDefinesVarSystem()->GetSize() > 0) {
		SetGlobalDefine("LAND_TERRAIN", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("LAND_TERRAIN", idx);

		SetGlobalDefine("DEEP_WATER_TERRAIN", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("DEEP_WATER_TERRAIN", idx);

		SetGlobalDefine("SHALLOW_WATER_TERRAIN", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("SHALLOW_WATER_TERRAIN", idx);

		SetGlobalDefine("FROZEN_TERRAIN", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("FROZEN_TERRAIN", idx);

		SetGlobalDefine("COLD_TERRAIN", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("COLD_TERRAIN", idx);

		SetGlobalDefine("TEMPERATE_TERRAIN", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("TEMPERATE_TERRAIN", idx);

		SetGlobalDefine("DRY_TERRAIN", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("DRY_TERRAIN", idx);

		SetGlobalDefine("BARREN_TERRAIN", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("BARREN_TERRAIN", idx);

		SetGlobalDefine("COLD_FEATURE", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("COLD_FEATURE", idx);

		SetGlobalDefine("TEMPERATE_FEATURE", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("TEMPERATE_FEATURE", idx);

		SetGlobalDefine("WARM_FEATURE", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("WARM_FEATURE", idx);

		SetGlobalDefine("LAND_IMPROVEMENT", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("LAND_IMPROVEMENT", idx);

		SetGlobalDefine("WATER_IMPROVEMENT", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("WATER_IMPROVEMENT", idx);

		SetGlobalDefine("RUINS_IMPROVEMENT", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("RUINS_IMPROVEMENT", idx);

		SetGlobalDefine("NUKE_FEATURE", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("NUKE_FEATURE", idx);

		SetGlobalDefine("GLOBAL_WARMING_TERRAIN", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("GLOBAL_WARMING_TERRAIN", idx);

		SetGlobalDefine("CAPITAL_BUILDINGCLASS", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("CAPITAL_BUILDINGCLASS", idx);

		SetGlobalDefine("DEFAULT_SPECIALIST", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("DEFAULT_SPECIALIST", idx);

		SetGlobalDefine("INITIAL_CITY_ROUTE_TYPE", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("INITIAL_CITY_ROUTE_TYPE", idx);

		SetGlobalDefine("STANDARD_HANDICAP", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("STANDARD_HANDICAP", idx);

		SetGlobalDefine("STANDARD_HANDICAP_QUICK", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("STANDARD_HANDICAP_QUICK", idx);

		SetGlobalDefine("STANDARD_GAMESPEED", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("STANDARD_GAMESPEED", idx);

		SetGlobalDefine("STANDARD_TURNTIMER", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("STANDARD_TURNTIMER", idx);

		SetGlobalDefine("STANDARD_CLIMATE", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("STANDARD_CLIMATE", idx);

		SetGlobalDefine("STANDARD_SEALEVEL", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("STANDARD_SEALEVEL", idx);

		SetGlobalDefine("STANDARD_ERA", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("STANDARD_ERA", idx);

		SetGlobalDefine("STANDARD_CALENDAR", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("STANDARD_CALENDAR", idx);

		SetGlobalDefine("AI_HANDICAP", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("AI_HANDICAP", idx);

		SetGlobalDefine("BARBARIAN_HANDICAP", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("BARBARIAN_HANDICAP", idx);

		SetGlobalDefine("BARBARIAN_CIVILIZATION", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("BARBARIAN_CIVILIZATION", idx);

		SetGlobalDefine("BARBARIAN_LEADER", szVal);
		idx = FindInInfoClass(szVal);
		GC.getDefinesVarSystem()->SetValue("BARBARIAN_LEADER", idx);

		return true;
	}

	char	szMessage[1024];
	sprintf(szMessage, "Size of Global Defines is not greater than 0. \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
	gDLL->MessageBox(szMessage, "XML Load Error");

	return false;
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetGlobalTypes()
//
//  PURPOSE :   Initialize the variables located in globaltypes.cpp/h with the values in
//				GlobalTypes.xml
//
//------------------------------------------------------------------------------------------------------
bool CvXMLLoadUtility::SetGlobalTypes() {
	UpdateProgressCB("GlobalTypes");

	bool bLoaded = false;	// used to make sure that the xml file was loaded correctly
	if (!CreateFXml()) {
		return false;
	}

	// load the new FXml variable with the GlobalTypes.xml file
	bLoaded = LoadCivXml(m_pFXml, "xml/GlobalTypes.xml");
	if (!bLoaded) {
		char	szMessage[1024];
		sprintf(szMessage, "LoadXML call failed for GlobalTypes.xml. \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Load Error");
	}

	// if the load succeeded we will continue
	if (bLoaded) {
		SetGlobalStringArray(&GC.getAnimationOperatorTypes(), "Civ4Types/AnimationOperatorTypes/AnimationOperatorType", &GC.getNumAnimationOperatorTypes());
		int iEnumVal = NUM_FUNC_TYPES;
		SetGlobalStringArray(&GC.getFunctionTypes(), "Civ4Types/FunctionTypes/FunctionType", &iEnumVal, true);
		SetGlobalStringArray(&GC.getFlavorTypes(), "Civ4Types/FlavorTypes/FlavorType", &GC.getNumFlavorTypes());
		SetGlobalStringArray(&GC.getArtStyleTypes(), "Civ4Types/ArtStyleTypes/ArtStyleType", &GC.getNumArtStyleTypes());
		SetGlobalStringArray(&GC.getCitySizeTypes(), "Civ4Types/CitySizeTypes/CitySizeType", &GC.getNumCitySizeTypes());
		iEnumVal = NUM_CONTACT_TYPES;
		SetGlobalStringArray(&GC.getContactTypes(), "Civ4Types/ContactTypes/ContactType", &iEnumVal, true);
		iEnumVal = NUM_DIPLOMACYPOWER_TYPES;
		SetGlobalStringArray(&GC.getDiplomacyPowerTypes(), "Civ4Types/DiplomacyPowerTypes/DiplomacyPowerType", &iEnumVal, true);
		iEnumVal = NUM_AUTOMATE_TYPES;
		SetGlobalStringArray(&GC.getAutomateTypes(), "Civ4Types/AutomateTypes/AutomateType", &iEnumVal, true);
		iEnumVal = NUM_DIRECTION_TYPES;
		SetGlobalStringArray(&GC.getDirectionTypes(), "Civ4Types/DirectionTypes/DirectionType", &iEnumVal, true);
		SetGlobalStringArray(&GC.getFootstepAudioTypes(), "Civ4Types/FootstepAudioTypes/FootstepAudioType", &GC.getNumFootstepAudioTypes());

		gDLL->getXMLIFace()->SetToParent(m_pFXml);
		gDLL->getXMLIFace()->SetToParent(m_pFXml);
		SetListPairEnumString(&GC.getFootstepAudioTags(), "FootstepAudioTags", GC.getNumFootstepAudioTypes(), "");
	}

	// delete the pointer to the FXml variable
	DestroyFXml();

	return true;
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetDiplomacyCommentTypes()
//
//  PURPOSE :   Creates a full list of Diplomacy Comments
//
//
//------------------------------------------------------------------------------------------------------
void CvXMLLoadUtility::SetDiplomacyCommentTypes(CvString** ppszString, int* iNumVals) {
	FAssertMsg(false, "should never get here");
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetupGlobalLandscapeInfos()
//
//  PURPOSE :   Initialize the appropriate variables located in globals.cpp/h with the values in
//				Terrain\Civ4TerrainSettings.xml
//
//------------------------------------------------------------------------------------------------------
bool CvXMLLoadUtility::SetupGlobalLandscapeInfo() {
	// load order: 6
	// loads after main menu

	// Work now done by the XML preloading code

	return true;
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetGlobalArtDefines()
//
//  PURPOSE :   Initialize the appropriate variables located in globals.cpp/h with the values in
//				Civ4ArtDefines.xml
//
//------------------------------------------------------------------------------------------------------
bool CvXMLLoadUtility::SetGlobalArtDefines() {
	// load order: 3
	if (!CreateFXml()) {
		return false;
	}

	LoadGlobalClassInfo(ARTFILEMGR.getInterfaceArtInfo(), "CIV4ArtDefines_Interface", "Art", "Civ4ArtDefines/InterfaceArtInfos/InterfaceArtInfo", false);
	LoadGlobalClassInfo(ARTFILEMGR.getMovieArtInfo(), "CIV4ArtDefines_Movie", "Art", "Civ4ArtDefines/MovieArtInfos/MovieArtInfo", false);
	LoadGlobalClassInfo(ARTFILEMGR.getMiscArtInfo(), "CIV4ArtDefines_Misc", "Art", "Civ4ArtDefines/MiscArtInfos/MiscArtInfo", false);
	LoadGlobalClassInfo(ARTFILEMGR.getUnitArtInfo(), "CIV4ArtDefines_Unit", "Art", "Civ4ArtDefines/UnitArtInfos/UnitArtInfo", false);
	LoadGlobalClassInfo(ARTFILEMGR.getBuildingArtInfo(), "CIV4ArtDefines_Building", "Art", "Civ4ArtDefines/BuildingArtInfos/BuildingArtInfo", false);
	LoadGlobalClassInfo(ARTFILEMGR.getCivilizationArtInfo(), "CIV4ArtDefines_Civilization", "Art", "Civ4ArtDefines/CivilizationArtInfos/CivilizationArtInfo", false);
	LoadGlobalClassInfo(ARTFILEMGR.getLeaderheadArtInfo(), "CIV4ArtDefines_Leaderhead", "Art", "Civ4ArtDefines/LeaderheadArtInfos/LeaderheadArtInfo", false);
	LoadGlobalClassInfo(ARTFILEMGR.getBonusArtInfo(), "CIV4ArtDefines_Bonus", "Art", "Civ4ArtDefines/BonusArtInfos/BonusArtInfo", false);
	LoadGlobalClassInfo(ARTFILEMGR.getImprovementArtInfo(), "CIV4ArtDefines_Improvement", "Art", "Civ4ArtDefines/ImprovementArtInfos/ImprovementArtInfo", false);
	LoadGlobalClassInfo(ARTFILEMGR.getTerrainArtInfo(), "CIV4ArtDefines_Terrain", "Art", "Civ4ArtDefines/TerrainArtInfos/TerrainArtInfo", false);
	LoadGlobalClassInfo(ARTFILEMGR.getFeatureArtInfo(), "CIV4ArtDefines_Feature", "Art", "Civ4ArtDefines/FeatureArtInfos/FeatureArtInfo", false);

	DestroyFXml();

	return true;
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetGlobalText()
//
//  PURPOSE :   Handles all Global Text Infos
//
//------------------------------------------------------------------------------------------------------
bool CvXMLLoadUtility::LoadGlobalText() {
	CvCacheObject* cache = gDLL->createGlobalTextCacheObject("GlobalText.dat");	// cache file name
	if (!gDLL->cacheRead(cache)) {
		bool bLoaded = false;

		if (!CreateFXml()) {
			return false;
		}

		//
		// load all files in the xml text directory
		//
		std::vector<CvString> aszFiles;
		std::vector<CvString> aszModfiles;

		gDLL->enumerateFiles(aszFiles, "xml\\text\\*.xml");

		// K-Mod. Text files from mods may not have the same set of language as the base game.
		// When such a mismatch occurs, we cannot simply rely on "getCurrentLanguage()" to give us the correct text from the mod file. We should instead check the xml tags.
		// So, before we start loading text, we need to extract the current name of our language tag. This isn't as easy as I'd like. Here's what I'm going to do:
		// CIV4GameText_Misc1.xml contains the text for the language options dropdown menu in the settings screen; so I'm going to assume that particular text file is
		// well formed, and I'm going to use it to determine the current language name. (Note: I'd like to use the names from TXT_KEY_LANGUAGE_#, but that text isn't easy to access.)
		// label text for the currently selected language -- that should correspond to the xml label used for that language.
		std::string langauge_name;
		if (LoadCivXml(m_pFXml, "xml\\text\\CIV4GameText_Misc1.xml")) {
			bool bValid = true;
			bValid = bValid && gDLL->getXMLIFace()->LocateNode(m_pFXml, "Civ4GameText/TEXT");
			bValid = bValid && gDLL->getXMLIFace()->SetToChild(m_pFXml);

			if (bValid) {
				const int& iLanguage = GAMETEXT.getCurrentLanguage();
				const int iMax = GC.getDefineINT("MAX_NUM_LANGUAGES");
				for (int i = 0; i < iMax; i++) {
					SkipToNextVal();

					if (!gDLL->getXMLIFace()->NextSibling(m_pFXml))
						break;
					if (i == iLanguage) {
						char buffer[1024]; // no way to determine max tag name size. .. This is really bad; but what can I do about it?
						if (gDLL->getXMLIFace()->GetLastLocatedNodeTagName(m_pFXml, buffer)) {
							buffer[1023] = 0; // just in case the buffer isn't even terminated!
							langauge_name.assign(buffer);
						}
					}
				}
				// this is stupid...
				// the number of languages is a static private variable which can only be set by a non-static function.
				CvGameText dummy;
				dummy.setNumLanguages(i);
			}
		}

		// Remove duplicate files. (Both will be loaded from the mod folder anyway, so this will save us some time.)
		// However, we must not disturb the order of the list, because it is important that the modded files overrule the unmodded files.
		for (std::vector<CvString>::iterator it = aszFiles.begin(); it != aszFiles.end(); ++it) {
			std::vector<CvString>::iterator jt = it + 1;
			while (jt != aszFiles.end()) {
				if (it->CompareNoCase(*jt) == 0)
					jt = aszFiles.erase(jt);
				else
					++jt;
			}
		}

		if (gDLL->isModularXMLLoading()) {
			gDLL->enumerateFiles(aszModfiles, "modules\\*_CIV4GameText.xml");
			aszFiles.insert(aszFiles.end(), aszModfiles.begin(), aszModfiles.end());
		}

		for (std::vector<CvString>::iterator it = aszFiles.begin(); it != aszFiles.end(); ++it) {
			bLoaded = LoadCivXml(m_pFXml, *it); // Load the XML
			if (!bLoaded) {
				char	szMessage[1024];
				sprintf(szMessage, "LoadXML call failed for %s. \n Current XML file is: %s", (*it).c_str(), GC.getCurrentXMLFile().GetCString());
				gDLL->MessageBox(szMessage, "XML Load Error");
			}
			if (bLoaded) {
				// if the xml is successfully validated
				SetGameText("Civ4GameText", "Civ4GameText/TEXT", langauge_name);
			}
		}

		DestroyFXml();

		// write global text info to cache
		bool bOk = gDLL->cacheWrite(cache);
		if (!bLoaded) {
			char	szMessage[1024];
			sprintf(szMessage, "Failed writing to Global Text cache. \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
			gDLL->MessageBox(szMessage, "XML Caching Error");
		}
		if (bOk) {
			logMsg("Wrote GlobalText to cache");
		}
	}	// didn't read from cache
	else {
		logMsg("Read GlobalText from cache");
	}

	gDLL->destroyCache(cache);

	return true;
}

bool CvXMLLoadUtility::LoadBasicInfos() {
	// load order: 4

	// Work now done by the XML preloading code

	return true;
}

//
// Globals which must be loaded before the main menus.
// Don't put anything in here unless it has to be loaded before the main menus,
// instead try to load things in LoadPostMenuGlobals()
//
bool CvXMLLoadUtility::LoadPreMenuGlobals() {
	// load order: 5
	if (!CreateFXml()) {
		return false;
	}

	readXMLfiles(false);

	// Work now done by the XML preloading code

	// add types to global var system
	for (int i = 0; i < GC.getNumCursorInfos(); ++i) {
		int iVal;
		CvString szType = GC.getCursorInfo((CursorTypes)i).getType();
		if (GC.getDefinesVarSystem()->GetValue(szType, iVal)) {
			char szMessage[1024];
			sprintf(szMessage, "cursor type already set? \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
			gDLL->MessageBox(szMessage, "XML Error");
		}
		GC.getDefinesVarSystem()->SetValue(szType, i);
	}

	// Check Playables
	for (int i = 0; i < GC.getNumCivilizationInfos(); ++i) {
		// if the civilization is playable we will increment the playable var
		if (GC.getCivilizationInfo((CivilizationTypes)i).isPlayable()) {
			GC.getNumPlayableCivilizationInfos() += 1;
		}

		// if the civilization is playable by AI increments num playable
		if (GC.getCivilizationInfo((CivilizationTypes)i).isAIPlayable()) {
			GC.getNumAIPlayableCivilizationInfos() += 1;
		}
	}

	UpdateProgressCB("GlobalOther");

	DestroyFXml();

	return true;
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   LoadPostMenuGlobals()
//
//  PURPOSE :   loads global xml data which isn't needed for the main menus
//		this data is loaded as a secodn stage, when the game is launched
//
//------------------------------------------------------------------------------------------------------
bool CvXMLLoadUtility::LoadPostMenuGlobals() {
	// load order: 7
	// called after main menu

	// Work now done by the XML preloading code

	return true;
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetGlobalStringArray(TCHAR (**ppszString)[256], char* szTagName, int* iNumVals)
//
//  PURPOSE :   takes the szTagName parameter and if it finds it in the m_pFXml member variable
//				then it loads the ppszString parameter with the string values under it and the
//				iNumVals with the total number of tags with the szTagName in the xml file
//
//------------------------------------------------------------------------------------------------------
void CvXMLLoadUtility::SetGlobalStringArray(CvString** ppszString, char* szTagName, int* iNumVals, bool bUseEnum) {
	PROFILE_FUNC();
	logMsg("SetGlobalStringArray %s\n", szTagName);

	// null out the local string pointer so that it can be checked at the
	// end of the function in an FAssert
	CvString* pszString = NULL;	// hold the local pointer to the newly allocated string memory

	// if we locate the szTagName, the current node is set to the first instance of the tag name in the xml file
	if (gDLL->getXMLIFace()->LocateNode(m_pFXml, szTagName)) {
		if (!bUseEnum) {
			// get the total number of times this tag appears in the xml
			*iNumVals = gDLL->getXMLIFace()->NumOfElementsByTagName(m_pFXml, szTagName);
		}
		// initialize the memory based on the total number of tags in the xml and the 256 character length we selected
		*ppszString = new CvString[*iNumVals];
		// set the local pointer to the memory just allocated
		pszString = *ppszString;

		// loop through each of the tags
		for (int i = 0; i < *iNumVals; i++) {
			// get the string value at the current node
			GetXmlVal(pszString[i]);
			GC.setTypesEnum(pszString[i], i);

			// if can't set the current node to a sibling node we will break out of the for loop
			// otherwise we will keep looping
			if (!gDLL->getXMLIFace()->NextSibling(m_pFXml)) {
				break;
			}
		}
	}

	// if the local string pointer is null then we weren't able to find the szTagName in the xml
	// so we will FAssert to let whoever know it
	if (!pszString) {
		char	szMessage[1024];
		sprintf(szMessage, "Error locating tag node in SetGlobalStringArray function \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
}




//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetGlobalActionInfo(CvActionInfo** ppActionInfo, int* iNumVals)
//
//  PURPOSE :   Takes the szTagName parameter and if it exists in the xml it loads the ppActionInfo
//				with the value under it and sets the value of the iNumVals parameter to the total number
//				of occurances of the szTagName tag in the xml.
//
//------------------------------------------------------------------------------------------------------
void CvXMLLoadUtility::SetGlobalActionInfo() {
	PROFILE_FUNC();
	logMsg("SetGlobalActionInfo\n");

	if (!(NUM_INTERFACEMODE_TYPES > 0)) {
		char	szMessage[1024];
		sprintf(szMessage, "NUM_INTERFACE_TYPES is not greater than zero in CvXMLLoadUtility::SetGlobalActionInfo \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
	if (!(GC.getNumBuildInfos() > 0)) {
		char	szMessage[1024];
		sprintf(szMessage, "GC.getNumBuildInfos() is not greater than zero in CvXMLLoadUtility::SetGlobalActionInfo \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
	if (!(GC.getNumPromotionInfos() > 0)) {
		char	szMessage[1024];
		sprintf(szMessage, "GC.getNumPromotionInfos() is not greater than zero in CvXMLLoadUtility::SetGlobalActionInfo \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
	if (!(GC.getNumUnitClassInfos() > 0)) {
		char	szMessage[1024];
		sprintf(szMessage, "GC.getNumUnitClassInfos() is not greater than zero in CvXMLLoadUtility::SetGlobalActionInfo \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
	if (!(GC.getNumSpecialistInfos() > 0)) {
		char	szMessage[1024];
		sprintf(szMessage, "GC.getNumSpecialistInfos() is not greater than zero in CvXMLLoadUtility::SetGlobalActionInfo \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
	if (!(GC.getNumBuildingInfos() > 0)) {
		char	szMessage[1024];
		sprintf(szMessage, "GC.getNumBuildingInfos() is not greater than zero in CvXMLLoadUtility::SetGlobalActionInfo \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
	if (!(NUM_CONTROL_TYPES > 0)) {
		char	szMessage[1024];
		sprintf(szMessage, "NUM_CONTROL_TYPES is not greater than zero in CvXMLLoadUtility::SetGlobalActionInfo \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
	if (!(GC.getNumAutomateInfos() > 0)) {
		char	szMessage[1024];
		sprintf(szMessage, "GC.getNumAutomateInfos() is not greater than zero in CvXMLLoadUtility::SetGlobalActionInfo \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
	if (!(NUM_COMMAND_TYPES > 0)) {
		char	szMessage[1024];
		sprintf(szMessage, "NUM_COMMAND_TYPES is not greater than zero in CvXMLLoadUtility::SetGlobalActionInfo \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
	if (!(NUM_MISSION_TYPES > 0)) {
		char	szMessage[1024];
		sprintf(szMessage, "NUM_MISSION_TYPES is not greater than zero in CvXMLLoadUtility::SetGlobalActionInfo \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}

	int* piOrderedIndex = NULL;

	int iNumOrigVals = GC.getNumActionInfos();

	int iNumActionInfos = iNumOrigVals +
		NUM_INTERFACEMODE_TYPES +
		GC.getNumBuildInfos() +
		GC.getNumPromotionInfos() +
		GC.getNumReligionInfos() +
		GC.getNumCorporationInfos() +
		GC.getNumUnitInfos() +
		GC.getNumSpecialistInfos() +
		GC.getNumBuildingInfos() +
		NUM_CONTROL_TYPES +
		NUM_COMMAND_TYPES +
		GC.getNumAutomateInfos() +
		NUM_MISSION_TYPES;

	int* piIndexList = new int[iNumActionInfos];
	int* piPriorityList = new int[iNumActionInfos];
	int* piActionInfoTypeList = new int[iNumActionInfos];

	int iTotalActionInfoCount = 0;

	// loop through control info
	for (int i = 0; i < NUM_COMMAND_TYPES; i++) {
		piIndexList[iTotalActionInfoCount] = i;
		piPriorityList[iTotalActionInfoCount] = GC.getCommandInfo((CommandTypes)i).getOrderPriority();
		piActionInfoTypeList[iTotalActionInfoCount] = ACTIONSUBTYPE_COMMAND;
		iTotalActionInfoCount++;
	}

	for (int i = 0; i < NUM_INTERFACEMODE_TYPES; i++) {
		piIndexList[iTotalActionInfoCount] = i;
		piPriorityList[iTotalActionInfoCount] = GC.getInterfaceModeInfo((InterfaceModeTypes)i).getOrderPriority();
		piActionInfoTypeList[iTotalActionInfoCount] = ACTIONSUBTYPE_INTERFACEMODE;
		iTotalActionInfoCount++;
	}

	for (int i = 0; i < GC.getNumBuildInfos(); i++) {
		piIndexList[iTotalActionInfoCount] = i;
		piPriorityList[iTotalActionInfoCount] = GC.getBuildInfo((BuildTypes)i).getOrderPriority();
		piActionInfoTypeList[iTotalActionInfoCount] = ACTIONSUBTYPE_BUILD;
		iTotalActionInfoCount++;
	}

	for (int i = 0; i < GC.getNumPromotionInfos(); i++) {
		piIndexList[iTotalActionInfoCount] = i;
		piPriorityList[iTotalActionInfoCount] = GC.getPromotionInfo((PromotionTypes)i).getOrderPriority();
		piActionInfoTypeList[iTotalActionInfoCount] = ACTIONSUBTYPE_PROMOTION;
		iTotalActionInfoCount++;
	}

	for (int i = 0; i < GC.getNumUnitInfos(); i++) {
		piIndexList[iTotalActionInfoCount] = i;
		piPriorityList[iTotalActionInfoCount] = GC.getUnitInfo((UnitTypes)i).getOrderPriority();
		piActionInfoTypeList[iTotalActionInfoCount] = ACTIONSUBTYPE_UNIT;
		iTotalActionInfoCount++;
	}

	for (int i = 0; i < GC.getNumReligionInfos(); i++) {
		piIndexList[iTotalActionInfoCount] = i;
		piPriorityList[iTotalActionInfoCount] = GC.getReligionInfo((ReligionTypes)i).getOrderPriority();
		piActionInfoTypeList[iTotalActionInfoCount] = ACTIONSUBTYPE_RELIGION;
		iTotalActionInfoCount++;
	}

	for (int i = 0; i < GC.getNumCorporationInfos(); i++) {
		piIndexList[iTotalActionInfoCount] = i;
		piPriorityList[iTotalActionInfoCount] = GC.getCorporationInfo((CorporationTypes)i).getOrderPriority();
		piActionInfoTypeList[iTotalActionInfoCount] = ACTIONSUBTYPE_CORPORATION;
		iTotalActionInfoCount++;
	}

	for (int i = 0; i < GC.getNumSpecialistInfos(); i++) {
		piIndexList[iTotalActionInfoCount] = i;
		piPriorityList[iTotalActionInfoCount] = GC.getSpecialistInfo((SpecialistTypes)i).getOrderPriority();
		piActionInfoTypeList[iTotalActionInfoCount] = ACTIONSUBTYPE_SPECIALIST;
		iTotalActionInfoCount++;
	}

	for (int i = 0; i < GC.getNumBuildingInfos(); i++) {
		piIndexList[iTotalActionInfoCount] = i;
		piPriorityList[iTotalActionInfoCount] = GC.getBuildingInfo((BuildingTypes)i).getOrderPriority();
		piActionInfoTypeList[iTotalActionInfoCount] = ACTIONSUBTYPE_BUILDING;
		iTotalActionInfoCount++;
	}

	for (int i = 0; i < NUM_CONTROL_TYPES; i++) {
		piIndexList[iTotalActionInfoCount] = i;
		piPriorityList[iTotalActionInfoCount] = GC.getControlInfo((ControlTypes)i).getOrderPriority();
		piActionInfoTypeList[iTotalActionInfoCount] = ACTIONSUBTYPE_CONTROL;
		iTotalActionInfoCount++;
	}

	for (int i = 0; i < GC.getNumAutomateInfos(); i++) {
		piIndexList[iTotalActionInfoCount] = i;
		piPriorityList[iTotalActionInfoCount] = GC.getAutomateInfo(i).getOrderPriority();
		piActionInfoTypeList[iTotalActionInfoCount] = ACTIONSUBTYPE_AUTOMATE;
		iTotalActionInfoCount++;
	}

	for (int i = 0; i < NUM_MISSION_TYPES; i++) {
		piIndexList[iTotalActionInfoCount] = i;
		piPriorityList[iTotalActionInfoCount] = GC.getMissionInfo((MissionTypes)i).getOrderPriority();
		piActionInfoTypeList[iTotalActionInfoCount] = ACTIONSUBTYPE_MISSION;
		iTotalActionInfoCount++;
	}

	SAFE_DELETE_ARRAY(piOrderedIndex);
	piOrderedIndex = new int[iNumActionInfos];

	orderHotkeyInfo(&piOrderedIndex, piPriorityList, iNumActionInfos);
	for (int i = 0; i < iNumActionInfos; i++) {
		CvActionInfo* pActionInfo = new CvActionInfo;
		pActionInfo->setOriginalIndex(piIndexList[piOrderedIndex[i]]);
		pActionInfo->setSubType((ActionSubTypes)piActionInfoTypeList[piOrderedIndex[i]]);
		if ((ActionSubTypes)piActionInfoTypeList[piOrderedIndex[i]] == ACTIONSUBTYPE_COMMAND) {
			GC.getCommandInfo((CommandTypes)piIndexList[piOrderedIndex[i]]).setActionInfoIndex(i);
		} else if ((ActionSubTypes)piActionInfoTypeList[piOrderedIndex[i]] == ACTIONSUBTYPE_INTERFACEMODE) {
			GC.getInterfaceModeInfo((InterfaceModeTypes)piIndexList[piOrderedIndex[i]]).setActionInfoIndex(i);
		} else if ((ActionSubTypes)piActionInfoTypeList[piOrderedIndex[i]] == ACTIONSUBTYPE_BUILD) {
			GC.getBuildInfo((BuildTypes)piIndexList[piOrderedIndex[i]]).setMissionType(FindInInfoClass("MISSION_BUILD"));
			GC.getBuildInfo((BuildTypes)piIndexList[piOrderedIndex[i]]).setActionInfoIndex(i);
		} else if ((ActionSubTypes)piActionInfoTypeList[piOrderedIndex[i]] == ACTIONSUBTYPE_PROMOTION) {
			GC.getPromotionInfo((PromotionTypes)piIndexList[piOrderedIndex[i]]).setCommandType(FindInInfoClass("COMMAND_PROMOTION"));
			GC.getPromotionInfo((PromotionTypes)piIndexList[piOrderedIndex[i]]).setActionInfoIndex(i);
			GC.getPromotionInfo((PromotionTypes)piIndexList[piOrderedIndex[i]]).setHotKeyDescription(GC.getPromotionInfo((PromotionTypes)piIndexList[piOrderedIndex[i]]).getTextKeyWide(), GC.getCommandInfo((CommandTypes)(GC.getPromotionInfo((PromotionTypes)piIndexList[piOrderedIndex[i]]).getCommandType())).getTextKeyWide(), CreateHotKeyFromDescription(GC.getPromotionInfo((PromotionTypes)piIndexList[piOrderedIndex[i]]).getHotKey(), GC.getPromotionInfo((PromotionTypes)piIndexList[piOrderedIndex[i]]).isShiftDown(), GC.getPromotionInfo((PromotionTypes)piIndexList[piOrderedIndex[i]]).isAltDown(), GC.getPromotionInfo((PromotionTypes)piIndexList[piOrderedIndex[i]]).isCtrlDown()));
		} else if ((ActionSubTypes)piActionInfoTypeList[piOrderedIndex[i]] == ACTIONSUBTYPE_UNIT) {
			GC.getUnitInfo((UnitTypes)piIndexList[piOrderedIndex[i]]).setCommandType(FindInInfoClass("COMMAND_UPGRADE"));
			GC.getUnitInfo((UnitTypes)piIndexList[piOrderedIndex[i]]).setActionInfoIndex(i);
			GC.getUnitInfo((UnitTypes)piIndexList[piOrderedIndex[i]]).setHotKeyDescription(GC.getUnitInfo((UnitTypes)piIndexList[piOrderedIndex[i]]).getTextKeyWide(), GC.getCommandInfo((CommandTypes)(GC.getUnitInfo((UnitTypes)piIndexList[piOrderedIndex[i]]).getCommandType())).getTextKeyWide(), CreateHotKeyFromDescription(GC.getUnitInfo((UnitTypes)piIndexList[piOrderedIndex[i]]).getHotKey(), GC.getUnitInfo((UnitTypes)piIndexList[piOrderedIndex[i]]).isShiftDown(), GC.getUnitInfo((UnitTypes)piIndexList[piOrderedIndex[i]]).isAltDown(), GC.getUnitInfo((UnitTypes)piIndexList[piOrderedIndex[i]]).isCtrlDown()));
		} else if ((ActionSubTypes)piActionInfoTypeList[piOrderedIndex[i]] == ACTIONSUBTYPE_RELIGION) {
			GC.getReligionInfo((ReligionTypes)piIndexList[piOrderedIndex[i]]).setMissionType(FindInInfoClass("MISSION_SPREAD"));
			GC.getReligionInfo((ReligionTypes)piIndexList[piOrderedIndex[i]]).setActionInfoIndex(i);
			GC.getReligionInfo((ReligionTypes)piIndexList[piOrderedIndex[i]]).setHotKeyDescription(GC.getReligionInfo((ReligionTypes)piIndexList[piOrderedIndex[i]]).getTextKeyWide(), GC.getMissionInfo((MissionTypes)(GC.getReligionInfo((ReligionTypes)piIndexList[piOrderedIndex[i]]).getMissionType())).getTextKeyWide(), CreateHotKeyFromDescription(GC.getReligionInfo((ReligionTypes)piIndexList[piOrderedIndex[i]]).getHotKey(), GC.getReligionInfo((ReligionTypes)piIndexList[piOrderedIndex[i]]).isShiftDown(), GC.getReligionInfo((ReligionTypes)piIndexList[piOrderedIndex[i]]).isAltDown(), GC.getReligionInfo((ReligionTypes)piIndexList[piOrderedIndex[i]]).isCtrlDown()));
		} else if ((ActionSubTypes)piActionInfoTypeList[piOrderedIndex[i]] == ACTIONSUBTYPE_CORPORATION) {
			GC.getCorporationInfo((CorporationTypes)piIndexList[piOrderedIndex[i]]).setMissionType(FindInInfoClass("MISSION_SPREAD_CORPORATION"));
			GC.getCorporationInfo((CorporationTypes)piIndexList[piOrderedIndex[i]]).setActionInfoIndex(i);
			GC.getCorporationInfo((CorporationTypes)piIndexList[piOrderedIndex[i]]).setHotKeyDescription(GC.getCorporationInfo((CorporationTypes)piIndexList[piOrderedIndex[i]]).getTextKeyWide(), GC.getMissionInfo((MissionTypes)(GC.getCorporationInfo((CorporationTypes)piIndexList[piOrderedIndex[i]]).getMissionType())).getTextKeyWide(), CreateHotKeyFromDescription(GC.getCorporationInfo((CorporationTypes)piIndexList[piOrderedIndex[i]]).getHotKey(), GC.getCorporationInfo((CorporationTypes)piIndexList[piOrderedIndex[i]]).isShiftDown(), GC.getCorporationInfo((CorporationTypes)piIndexList[piOrderedIndex[i]]).isAltDown(), GC.getCorporationInfo((CorporationTypes)piIndexList[piOrderedIndex[i]]).isCtrlDown()));
		} else if ((ActionSubTypes)piActionInfoTypeList[piOrderedIndex[i]] == ACTIONSUBTYPE_SPECIALIST) {
			GC.getSpecialistInfo((SpecialistTypes)piIndexList[piOrderedIndex[i]]).setMissionType(FindInInfoClass("MISSION_JOIN"));
			GC.getSpecialistInfo((SpecialistTypes)piIndexList[piOrderedIndex[i]]).setActionInfoIndex(i);
			GC.getSpecialistInfo((SpecialistTypes)piIndexList[piOrderedIndex[i]]).setHotKeyDescription(GC.getSpecialistInfo((SpecialistTypes)piIndexList[piOrderedIndex[i]]).getTextKeyWide(), GC.getMissionInfo((MissionTypes)(GC.getSpecialistInfo((SpecialistTypes)piIndexList[piOrderedIndex[i]]).getMissionType())).getTextKeyWide(), CreateHotKeyFromDescription(GC.getSpecialistInfo((SpecialistTypes)piIndexList[piOrderedIndex[i]]).getHotKey(), GC.getSpecialistInfo((SpecialistTypes)piIndexList[piOrderedIndex[i]]).isShiftDown(), GC.getSpecialistInfo((SpecialistTypes)piIndexList[piOrderedIndex[i]]).isAltDown(), GC.getSpecialistInfo((SpecialistTypes)piIndexList[piOrderedIndex[i]]).isCtrlDown()));
		} else if ((ActionSubTypes)piActionInfoTypeList[piOrderedIndex[i]] == ACTIONSUBTYPE_BUILDING) {
			GC.getBuildingInfo((BuildingTypes)piIndexList[piOrderedIndex[i]]).setMissionType(FindInInfoClass("MISSION_CONSTRUCT"));
			GC.getBuildingInfo((BuildingTypes)piIndexList[piOrderedIndex[i]]).setActionInfoIndex(i);
			GC.getBuildingInfo((BuildingTypes)piIndexList[piOrderedIndex[i]]).setHotKeyDescription(GC.getBuildingInfo((BuildingTypes)piIndexList[piOrderedIndex[i]]).getTextKeyWide(), GC.getMissionInfo((MissionTypes)(GC.getBuildingInfo((BuildingTypes)piIndexList[piOrderedIndex[i]]).getMissionType())).getTextKeyWide(), CreateHotKeyFromDescription(GC.getBuildingInfo((BuildingTypes)piIndexList[piOrderedIndex[i]]).getHotKey(), GC.getBuildingInfo((BuildingTypes)piIndexList[piOrderedIndex[i]]).isShiftDown(), GC.getBuildingInfo((BuildingTypes)piIndexList[piOrderedIndex[i]]).isAltDown(), GC.getBuildingInfo((BuildingTypes)piIndexList[piOrderedIndex[i]]).isCtrlDown()));
		} else if ((ActionSubTypes)piActionInfoTypeList[piOrderedIndex[i]] == ACTIONSUBTYPE_CONTROL) {
			GC.getControlInfo((ControlTypes)piIndexList[piOrderedIndex[i]]).setActionInfoIndex(i);
		} else if ((ActionSubTypes)piActionInfoTypeList[piOrderedIndex[i]] == ACTIONSUBTYPE_AUTOMATE) {
			GC.getAutomateInfo(piIndexList[piOrderedIndex[i]]).setActionInfoIndex(i);
		} else if ((ActionSubTypes)piActionInfoTypeList[piOrderedIndex[i]] == ACTIONSUBTYPE_MISSION) {
			GC.getMissionInfo((MissionTypes)piIndexList[piOrderedIndex[i]]).setActionInfoIndex(i + iNumOrigVals);
		}

		GC.getActionInfo().push_back(pActionInfo);
	}

	SAFE_DELETE_ARRAY(piOrderedIndex);
	SAFE_DELETE_ARRAY(piIndexList);
	SAFE_DELETE_ARRAY(piPriorityList);
	SAFE_DELETE_ARRAY(piActionInfoTypeList);
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetGlobalAnimationPathInfo(CvAnimationPathInfo** ppAnimationPathInfo, char* szTagName, int* iNumVals)
//
//  PURPOSE :   Takes the szTagName parameter and if it exists in the xml it loads the ppAnimationPathInfo
//				with the value under it and sets the value of the iNumVals parameter to the total number
//				of occurances of the szTagName tag in the xml.
//
//------------------------------------------------------------------------------------------------------
void CvXMLLoadUtility::SetGlobalAnimationPathInfo(CvAnimationPathInfo** ppAnimationPathInfo, char* szTagName, int* iNumVals) {
	PROFILE_FUNC();
	logMsg("SetGlobalAnimationPathInfo %s\n", szTagName);

	CvAnimationPathInfo* pAnimPathInfo = NULL;	// local pointer to the domain info memory

	if (gDLL->getXMLIFace()->LocateNode(m_pFXml, szTagName)) {
		// get the number of times the szTagName tag appears in the xml file
		*iNumVals = gDLL->getXMLIFace()->NumOfElementsByTagName(m_pFXml, szTagName);

		// allocate memory for the domain info based on the number above
		*ppAnimationPathInfo = new CvAnimationPathInfo[*iNumVals];
		pAnimPathInfo = *ppAnimationPathInfo;

		gDLL->getXMLIFace()->SetToParent(m_pFXml);
		gDLL->getXMLIFace()->SetToChild(m_pFXml);
		gDLL->getXMLIFace()->SetToChild(m_pFXml);


		// Loop through each tag.
		for (int i = 0; i < *iNumVals; i++) {
			SkipToNextVal();	// skip to the next non-comment node

			if (!pAnimPathInfo[i].read(this))
				break;
			GC.setInfoTypeFromString(pAnimPathInfo[i].getType(), i);	// add type to global info type hash map
			if (!gDLL->getXMLIFace()->NextSibling(m_pFXml)) {
				break;
			}
		}
	}

	// if we didn't find the tag name in the xml then we never set the local pointer to the
	// newly allocated memory and there for we will FAssert to let people know this most
	// interesting fact
	if (!pAnimPathInfo) {
		char	szMessage[1024];
		sprintf(szMessage, "Error finding tag node in SetGlobalAnimationPathInfo function \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetGlobalUnitScales(float* pfLargeScale, float* pfSmallScale, char* szTagName)
//
//  PURPOSE :   Takes the szTagName parameter and if it exists in the xml it loads the ppPromotionInfo
//				with the value under it and sets the value of the iNumVals parameter to the total number
//				of occurances of the szTagName tag in the xml.
//
//------------------------------------------------------------------------------------------------------
void CvXMLLoadUtility::SetGlobalUnitScales(float* pfLargeScale, float* pfSmallScale, char* szTagName) {
	PROFILE_FUNC();
	logMsg("SetGlobalUnitScales %s\n", szTagName);
	// if we successfully locate the szTagName node
	if (gDLL->getXMLIFace()->LocateNode(m_pFXml, szTagName)) {
		// call the function that sets the FXml pointer to the first non-comment child of
		// the current tag and gets the value of that new node
		if (GetChildXmlVal(pfLargeScale)) {
			// set the current xml node to it's next sibling and then
			// get the sibling's TCHAR value
			GetNextXmlVal(pfSmallScale);

			// set the current xml node to it's parent node
			gDLL->getXMLIFace()->SetToParent(m_pFXml);
		}
	} else {
		// if we didn't find the tag name in the xml then we never set the local pointer to the
		// newly allocated memory and there for we will FAssert to let people know this most
		// interesting fact
		char	szMessage[1024];
		sprintf(szMessage, "Error finding tag node in SetGlobalUnitScales function \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetGameText()
//
//  PURPOSE :   Reads game text info from XML and adds it to the translation manager
//
//------------------------------------------------------------------------------------------------------
void CvXMLLoadUtility::SetGameText(const char* szTextGroup, const char* szTagName, const std::string& language_name) {
	PROFILE_FUNC();
	logMsg("SetGameText %s\n", szTagName);

	if (gDLL->getXMLIFace()->LocateNode(m_pFXml, szTextGroup)) // Get the Text Group 1st
	{
		int iNumVals = gDLL->getXMLIFace()->GetNumChildren(m_pFXml);	// Get the number of Children that the Text Group has
		gDLL->getXMLIFace()->LocateNode(m_pFXml, szTagName); // Now switch to the TEXT Tag
		gDLL->getXMLIFace()->SetToParent(m_pFXml);
		gDLL->getXMLIFace()->SetToChild(m_pFXml);

		// loop through each tag
		for (int i = 0; i < iNumVals; i++) {
			CvGameText textInfo;
			//textInfo.read(this);
			textInfo.read(this, language_name); // K-Mod

			gDLL->addText(textInfo.getType() /*id*/, textInfo.getText(), textInfo.getGender(), textInfo.getPlural());
			if (!gDLL->getXMLIFace()->NextSibling(m_pFXml) && i != iNumVals - 1) {
				char	szMessage[1024];
				sprintf(szMessage, "failed to find sibling \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
				gDLL->MessageBox(szMessage, "XML Error");
				break;
			}
		}
	}
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetGlobalClassInfo - This is a template function that is USED FOR ALMOST ALL INFO CLASSES.
//		Each info class should have a read(CvXMLLoadUtility*) function that is responsible for initializing
//		the class from xml data.
//
//  PURPOSE :   takes the szTagName parameter and loads the ppszString with the text values
//				under the tags.  This will be the hints displayed during game initialization and load
//
//------------------------------------------------------------------------------------------------------
template <class T>
void CvXMLLoadUtility::SetGlobalClassInfo(std::vector<T*>& aInfos, const char* szTagName) {
	char szLog[256];
	sprintf(szLog, "SetGlobalClassInfo (%s)", szTagName);
	PROFILE(szLog);
	logMsg(szLog);

	// if we successfully locate the tag name in the xml file
	if (gDLL->getXMLIFace()->LocateNode(m_pFXml, szTagName)) {
		// loop through each tag
		do {
			//SkipToNextVal();	// skip to the next non-comment node

			T* pClassInfo = new T;

			FAssert(NULL != pClassInfo);
			if (NULL == pClassInfo) {
				break;
			}

			bool bSuccess = false;
			if (bFirstRead) {
				bSuccess = pClassInfo->CvInfoBase::read(this);
			} else {
				bSuccess = pClassInfo->read(this);
			}

			FAssert(bSuccess);
			if (!bSuccess) {
				delete pClassInfo;
				break;
			}

			int iIndex = -1;
			if (NULL != pClassInfo->getType()) {
				iIndex = GC.getInfoTypeForString(pClassInfo->getType(), true);
			}

			if (-1 == iIndex) {
				aInfos.push_back(pClassInfo);
				if (NULL != pClassInfo->getType()) {
					GC.setInfoTypeFromString(pClassInfo->getType(), (int)aInfos.size() - 1);	// add type to global info type hash map
				}
			} else {
				FAssertMsg(!bFirstRead, CvString::format("%s is used multiple times", pClassInfo->getType()).c_str());
				SAFE_DELETE(aInfos[iIndex]);
				aInfos[iIndex] = pClassInfo;
			}

			//} while (gDLL->getXMLIFace()->NextSibling(m_pFXml));
		} while (gDLL->getXMLIFace()->NextSibling(m_pFXml) && SkipToNextVal()); // K-Mod
	}
}

void CvXMLLoadUtility::SetDiplomacyInfo(std::vector<CvDiplomacyInfo*>& DiploInfos, const char* szTagName) {
	char szLog[256];
	sprintf(szLog, "SetDiplomacyInfo (%s)", szTagName);
	PROFILE(szLog);
	logMsg(szLog);

	// if we successfully locate the tag name in the xml file
	if (gDLL->getXMLIFace()->LocateNode(m_pFXml, szTagName)) {
		// loop through each tag
		do {
			//SkipToNextVal();	// skip to the next non-comment node

			CvString szType;
			GetChildXmlValByName(szType, "Type");
			int iIndex = GC.getInfoTypeForString(szType, true);

			if (-1 == iIndex) {
				CvDiplomacyInfo* pClassInfo = new CvDiplomacyInfo;

				if (NULL == pClassInfo) {
					FAssert(false);
					break;
				}

				pClassInfo->read(this);
				if (NULL != pClassInfo->getType()) {
					GC.setInfoTypeFromString(pClassInfo->getType(), (int)DiploInfos.size());	// add type to global info type hash map
				}
				DiploInfos.push_back(pClassInfo);
			} else {
				DiploInfos[iIndex]->read(this);
			}

			//} while (gDLL->getXMLIFace()->NextSibling(m_pFXml));
		} while (gDLL->getXMLIFace()->NextSibling(m_pFXml) && SkipToNextVal()); // K-Mod
	}
}

template <class T>
void CvXMLLoadUtility::LoadGlobalClassInfo(std::vector<T*>& aInfos, const char* szFileRoot, const char* szFileDirectory, const char* szXmlPath, CvCacheObject* (CvDLLUtilityIFaceBase::* pArgFunction) (const TCHAR*)) {
	bool bLoaded = false;
	bool bWriteCache = true;
	CvCacheObject* pCache = NULL;
	GC.addToInfosVectors(&aInfos);

	if (NULL != pArgFunction) {
		pCache = (gDLL->*pArgFunction)(CvString::format("%s.dat", szFileRoot));	// cache file name

		if (gDLL->cacheRead(pCache, CvString::format("xml\\\\%s\\\\%s.xml", szFileDirectory, szFileRoot))) {
			logMsg("Read %s from cache", szFileDirectory);
			bLoaded = true;
			bWriteCache = false;
		}
	}

	if (!bLoaded) {
		bLoaded = LoadCivXml(m_pFXml, CvString::format("xml\\%s/%s.xml", szFileDirectory, szFileRoot));

		if (!bLoaded) {
			char szMessage[1024];
			sprintf(szMessage, "LoadXML call failed for %s.", CvString::format("%s/%s.xml", szFileDirectory, szFileRoot).GetCString());
			gDLL->MessageBox(szMessage, "XML Load Error");
		} else {
			SetGlobalClassInfo(aInfos, szXmlPath);

			if (gDLL->isModularXMLLoading()) {
				std::vector<CvString> aszFiles;
				gDLL->enumerateFiles(aszFiles, CvString::format("modules\\*_%s.xml", szFileRoot));  // search for the modular files

				for (std::vector<CvString>::iterator it = aszFiles.begin(); it != aszFiles.end(); ++it) {
					bLoaded = LoadCivXml(m_pFXml, *it);

					if (!bLoaded) {
						char szMessage[1024];
						sprintf(szMessage, "LoadXML call failed for %s.", (*it).GetCString());
						gDLL->MessageBox(szMessage, "XML Load Error");
					} else {
						SetGlobalClassInfo(aInfos, szXmlPath);
					}
				}
			}

			if (NULL != pArgFunction && bWriteCache) {
				// write info to cache
				bool bOk = gDLL->cacheWrite(pCache);
				if (!bOk) {
					char szMessage[1024];
					sprintf(szMessage, "Failed writing to %s cache. \n Current XML file is: %s", szFileDirectory, GC.getCurrentXMLFile().GetCString());
					gDLL->MessageBox(szMessage, "XML Caching Error");
				}
				if (bOk) {
					logMsg("Wrote %s to cache", szFileDirectory);
				}
			}
		}
	}

	if (NULL != pArgFunction) {
		gDLL->destroyCache(pCache);
	}
}


void CvXMLLoadUtility::LoadDiplomacyInfo(std::vector<CvDiplomacyInfo*>& DiploInfos, const char* szFileRoot, const char* szFileDirectory, const char* szXmlPath, CvCacheObject* (CvDLLUtilityIFaceBase::* pArgFunction) (const TCHAR*)) {
	bool bLoaded = false;
	bool bWriteCache = true;
	CvCacheObject* pCache = NULL;

	if (NULL != pArgFunction) {
		pCache = (gDLL->*pArgFunction)(CvString::format("%s.dat", szFileRoot));	// cache file name

		if (gDLL->cacheRead(pCache, CvString::format("xml\\\\%s\\\\%s.xml", szFileDirectory, szFileRoot))) {
			logMsg("Read %s from cache", szFileDirectory);
			bLoaded = true;
			bWriteCache = false;
		}
	}

	if (!bLoaded) {
		bLoaded = LoadCivXml(m_pFXml, CvString::format("xml\\%s/%s.xml", szFileDirectory, szFileRoot));

		if (!bLoaded) {
			char szMessage[1024];
			sprintf(szMessage, "LoadXML call failed for %s.", CvString::format("%s/%s.xml", szFileDirectory, szFileRoot).GetCString());
			gDLL->MessageBox(szMessage, "XML Load Error");
		} else {
			SetDiplomacyInfo(DiploInfos, szXmlPath);

			if (gDLL->isModularXMLLoading()) {
				std::vector<CvString> aszFiles;
				gDLL->enumerateFiles(aszFiles, CvString::format("modules\\*_%s.xml", szFileRoot));  // search for the modular files

				for (std::vector<CvString>::iterator it = aszFiles.begin(); it != aszFiles.end(); ++it) {
					bLoaded = LoadCivXml(m_pFXml, *it);

					if (!bLoaded) {
						char szMessage[1024];
						sprintf(szMessage, "LoadXML call failed for %s.", (*it).GetCString());
						gDLL->MessageBox(szMessage, "XML Load Error");
					} else {
						SetDiplomacyInfo(DiploInfos, szXmlPath);
					}
				}
			}

			if (NULL != pArgFunction && bWriteCache) {
				// write info to cache
				bool bOk = gDLL->cacheWrite(pCache);
				if (!bOk) {
					char szMessage[1024];
					sprintf(szMessage, "Failed writing to %s cache. \n Current XML file is: %s", szFileDirectory, GC.getCurrentXMLFile().GetCString());
					gDLL->MessageBox(szMessage, "XML Caching Error");
				}
				if (bOk) {
					logMsg("Wrote %s to cache", szFileDirectory);
				}
			}
		}
	}

	if (NULL != pArgFunction) {
		gDLL->destroyCache(pCache);
	}
}

//
// helper sort predicate
//

struct OrderIndex { int m_iPriority; int m_iIndex; };
bool sortHotkeyPriority(const OrderIndex orderIndex1, const OrderIndex orderIndex2) {
	return (orderIndex1.m_iPriority > orderIndex2.m_iPriority);
}

void CvXMLLoadUtility::orderHotkeyInfo(int** ppiSortedIndex, int* pHotkeyIndex, int iLength) {
	std::vector<OrderIndex> viOrderPriority;

	viOrderPriority.resize(iLength);
	int* piSortedIndex = *ppiSortedIndex;

	// set up vector
	for (int iI = 0; iI < iLength; iI++) {
		viOrderPriority[iI].m_iPriority = pHotkeyIndex[iI];
		viOrderPriority[iI].m_iIndex = iI;
	}

	// sort the array
	std::sort(viOrderPriority.begin(), viOrderPriority.end(), sortHotkeyPriority);

	// insert new order into the array to return
	for (iI = 0; iI < iLength; iI++) {
		piSortedIndex[iI] = viOrderPriority[iI].m_iIndex;
	}
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetFeatureStruct(int** ppiFeatureTech, int** ppiFeatureTime, int** ppiFeatureProduction, bool** ppbFeatureRemove)
//
//  PURPOSE :   allocate and set the feature struct variables for the CvBuildInfo class
//
//------------------------------------------------------------------------------------------------------
void CvXMLLoadUtility::SetFeatureStruct(int** ppiFeatureTech, int** ppiFeatureTime, int** ppiFeatureProduction, bool** ppbFeatureRemove) {
	if (GC.getNumFeatureInfos() < 1) {
		char	szMessage[1024];
		sprintf(szMessage, "no feature infos set yet! \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
	InitList(ppiFeatureTech, GC.getNumFeatureInfos(), -1);
	InitList(ppiFeatureTime, GC.getNumFeatureInfos());
	InitList(ppiFeatureProduction, GC.getNumFeatureInfos());
	InitList(ppbFeatureRemove, GC.getNumFeatureInfos());

	int* paiFeatureTech = *ppiFeatureTech;
	int* paiFeatureTime = *ppiFeatureTime;
	int* paiFeatureProduction = *ppiFeatureProduction;
	bool* pabFeatureRemove = *ppbFeatureRemove;

	if (gDLL->getXMLIFace()->SetToChildByTagName(m_pFXml, "FeatureStructs")) {
		int iNumSibs = gDLL->getXMLIFace()->GetNumChildren(m_pFXml);

		if (0 < iNumSibs) {
			if (gDLL->getXMLIFace()->SetToChildByTagName(m_pFXml, "FeatureStruct")) {
				if (!(iNumSibs <= GC.getNumFeatureInfos())) {
					char	szMessage[1024];
					sprintf(szMessage, "iNumSibs is greater than GC.getNumFeatureInfos in SetFeatureStruct function \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
					gDLL->MessageBox(szMessage, "XML Error");
				}

				TCHAR szTextVal[256];	// temporarily hold the text value of the current xml node
				for (int i = 0; i < iNumSibs; i++) {
					GetChildXmlValByName(szTextVal, "FeatureType");
					int iFeatureIndex = FindInInfoClass(szTextVal);
					if (!(iFeatureIndex != -1)) {
						char	szMessage[1024];
						sprintf(szMessage, "iFeatureIndex is -1 inside SetFeatureStruct function \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
						gDLL->MessageBox(szMessage, "XML Error");
					}
					GetChildXmlValByName(szTextVal, "PrereqTech");
					paiFeatureTech[iFeatureIndex] = FindInInfoClass(szTextVal);
					GetChildXmlValByName(&paiFeatureTime[iFeatureIndex], "iTime");
					GetChildXmlValByName(&paiFeatureProduction[iFeatureIndex], "iProduction");
					GetChildXmlValByName(&pabFeatureRemove[iFeatureIndex], "bRemove");

					if (!gDLL->getXMLIFace()->NextSibling(m_pFXml)) {
						break;
					}
				}

				gDLL->getXMLIFace()->SetToParent(m_pFXml);
			}
		}

		gDLL->getXMLIFace()->SetToParent(m_pFXml);
	}
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetImprovementBonuses(CvImprovementBonusInfo** ppImprovementBonus)
//
//  PURPOSE :   Allocate memory for the improvement bonus pointer and fill it based on the
//				values in the xml.
//
//------------------------------------------------------------------------------------------------------
void CvXMLLoadUtility::SetImprovementBonuses(CvImprovementBonusInfo** ppImprovementBonus) {
	// Skip any comments and stop at the next value we might want
	if (SkipToNextVal()) {
		// initialize the boolean list to the correct size and all the booleans to false
		InitImprovementBonusList(ppImprovementBonus, GC.getNumBonusInfos());
		// set the local pointer to the memory we just allocated
		CvImprovementBonusInfo* paImprovementBonus = *ppImprovementBonus;

		// get the total number of children the current xml node has
		int iNumSibs = gDLL->getXMLIFace()->GetNumChildren(m_pFXml);
		// if we can set the current xml node to the child of the one it is at now
		if (gDLL->getXMLIFace()->SetToChild(m_pFXml)) {
			if (!(iNumSibs <= GC.getNumBonusInfos())) {
				char	szMessage[1024];
				sprintf(szMessage, "For loop iterator is greater than array size \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
				gDLL->MessageBox(szMessage, "XML Error");
			}
			// loop through all the siblings
			for (int i = 0; i < iNumSibs; i++) {
				// skip to the next non-comment node
				if (SkipToNextVal()) {
					// call the function that sets the FXml pointer to the first non-comment child of
					// the current tag and gets the value of that new node
					TCHAR szNodeVal[256];
					if (GetChildXmlVal(szNodeVal)) {
						int iBonusIndex;	// index of the match in the bonus types list
						// call the find in list function to return either -1 if no value is found
						// or the index in the list the match is found at
						iBonusIndex = FindInInfoClass(szNodeVal);
						// if we found a match we will get the next sibling's boolean value at that match's index
						if (iBonusIndex >= 0) {
							GetNextXmlVal(&paImprovementBonus[iBonusIndex].m_bBonusMakesValid);
							GetNextXmlVal(&paImprovementBonus[iBonusIndex].m_bBonusTrade);
							GetNextXmlVal(&paImprovementBonus[iBonusIndex].m_iDiscoverRand);
							gDLL->getXMLIFace()->SetToParent(m_pFXml);

							SAFE_DELETE_ARRAY(paImprovementBonus[iBonusIndex].m_piYieldChange);	// free memory - MT, since we are about to reallocate
							SetList(&paImprovementBonus[iBonusIndex].m_piYieldChange, "YieldChanges", NUM_YIELD_TYPES);
						} else {
							gDLL->getXMLIFace()->SetToParent(m_pFXml);
						}

						// set the current xml node to it's parent node
					}

					// if we cannot set the current xml node to it's next sibling then we will break out of the for loop
					// otherwise we will continue looping
					if (!gDLL->getXMLIFace()->NextSibling(m_pFXml)) {
						break;
					}
				}
			}
			// set the current xml node to it's parent node
			gDLL->getXMLIFace()->SetToParent(m_pFXml);
		}
	}
}

void CvXMLLoadUtility::SetListInfo(int** ppList, const TCHAR* szRootTagName, int iListLength) {
	if (iListLength <= 0) {
		char	szMessage[1024];
		sprintf(szMessage, "Allocating zero or less memory in CvXMLLoadUtility::SetListInfo \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
	InitList(ppList, iListLength, -1);
	if (gDLL->getXMLIFace()->SetToChildByTagName(m_pFXml, szRootTagName)) {
		if (SkipToNextVal()) {
			int iNumSibs = gDLL->getXMLIFace()->GetNumChildren(m_pFXml);
			int* pList = *ppList;
			if (0 < iNumSibs) {
				if (iNumSibs > iListLength) {
					char	szMessage[1024];
					sprintf(szMessage, "There are more siblings than memory allocated for them in CvXMLLoadUtility::SetListInfo \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
					gDLL->MessageBox(szMessage, "XML Error");
				}
				TCHAR szTextVal[256];
				if (SkipToNextVal() && GetChildXmlVal(szTextVal)) // K-Mod. (without this, a comment in the xml could break this)
				{
					for (int i = 0; i < iNumSibs; ++i) {
						pList[i] = GC.getInfoTypeForString(szTextVal);
						if (!GetNextXmlVal(szTextVal)) {
							break;
						}
					}
					gDLL->getXMLIFace()->SetToParent(m_pFXml);
				}
			}
		}

		gDLL->getXMLIFace()->SetToParent(m_pFXml);
	}
}

void CvXMLLoadUtility::SetListPairInfos(int** ppList, const TCHAR* szRootTagName, int iInfoBaseLength) {
	if (!(0 < iInfoBaseLength)) {
		char	szMessage[1024];
		sprintf(szMessage, "Allocating zero or less memory in CvXMLLoadUtility::SetVariableListTagPair \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
	InitList(ppList, iInfoBaseLength, -1);
	if (gDLL->getXMLIFace()->SetToChildByTagName(m_pFXml, szRootTagName)) {
		if (SkipToNextVal()) {
			int iNumSibs = gDLL->getXMLIFace()->GetNumChildren(m_pFXml);
			int* pList = *ppList;
			if (0 < iNumSibs) {
				if (!(iNumSibs <= iInfoBaseLength)) {
					char	szMessage[1024];
					sprintf(szMessage, "There are more siblings than memory allocated for them in CvXMLLoadUtility::SetVariableListTagPair \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
					gDLL->MessageBox(szMessage, "XML Error");
				}
				if (gDLL->getXMLIFace()->SetToChild(m_pFXml)) {
					TCHAR szTextVal[256];
					for (int i = 0; i < iNumSibs; i++) {
						if (SkipToNextVal() && GetChildXmlVal(szTextVal)) // K-Mod. (without this, a comment in the xml could break this)
						{
							int iIndexVal = FindInInfoClass(szTextVal);
							if (iIndexVal != -1) {
								if (GetNextXmlVal(szTextVal)) {
									int iIndexVal2 = FindInInfoClass(szTextVal);
									if (iIndexVal2 != -1) {
										pList[iIndexVal] = iIndexVal2;
									}
								}
							}
							gDLL->getXMLIFace()->SetToParent(m_pFXml);
						}

						if (!gDLL->getXMLIFace()->NextSibling(m_pFXml)) {
							break;
						}
					}

					gDLL->getXMLIFace()->SetToParent(m_pFXml);
				}
			}
		}

		gDLL->getXMLIFace()->SetToParent(m_pFXml);
	}
}

//------------------------------------------------------------------------------------------------------
//
//	FUNCTION:	SetListPairEnumString(CvString **ppszList, const TCHAR* szRootTagName, CvString* m_paszTagList, int iTagListLength, CvString szDefaultListVal = "")
//
//  PURPOSE :   allocate and initialize a list from a tag pair in the xml
//
//------------------------------------------------------------------------------------------------------
void CvXMLLoadUtility::SetListPairEnumString(CvString** ppszList, const TCHAR* szRootTagName, int iTagListLength, CvString szDefaultListVal) {
	if (!(0 < iTagListLength)) {
		char	szMessage[1024];
		sprintf(szMessage, "Allocating zero or less memory in CvXMLLoadUtility::SetVariableListTagPair \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
		gDLL->MessageBox(szMessage, "XML Error");
	}
	InitStringList(ppszList, iTagListLength, szDefaultListVal);
	if (gDLL->getXMLIFace()->SetToChildByTagName(m_pFXml, szRootTagName)) {
		if (SkipToNextVal()) {
			int iNumSibs = gDLL->getXMLIFace()->GetNumChildren(m_pFXml);
			CvString* pszList = *ppszList;
			if (0 < iNumSibs) {
				if (!(iNumSibs <= iTagListLength)) {
					char	szMessage[1024];
					sprintf(szMessage, "There are more siblings than memory allocated for them in CvXMLLoadUtility::SetVariableListTagPair \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
					gDLL->MessageBox(szMessage, "XML Error");
				}
				if (gDLL->getXMLIFace()->SetToChild(m_pFXml)) {
					TCHAR szTextVal[256];
					for (int i = 0; i < iNumSibs; i++) {
						//if (GetChildXmlVal(szTextVal))
						if (SkipToNextVal() && GetChildXmlVal(szTextVal)) // K-Mod. (without this, a comment in the xml could break this)
						{
							int iIndexVal = GC.getTypesEnum(szTextVal);
							if (iIndexVal != -1) {
								GetNextXmlVal(pszList[iIndexVal]);
							}

							gDLL->getXMLIFace()->SetToParent(m_pFXml);
						}

						if (!gDLL->getXMLIFace()->NextSibling(m_pFXml)) {
							break;
						}
					}

					gDLL->getXMLIFace()->SetToParent(m_pFXml);
				}
			}
		}

		gDLL->getXMLIFace()->SetToParent(m_pFXml);
	}
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetVariableListTagPairForAudioScripts(int **ppiList, const TCHAR* szRootTagName, int iInfoBaseLength, int iDefaultListVal)
//
//  PURPOSE :   allocate and initialize a list from a tag pair in the xml for audio scripts
//
//------------------------------------------------------------------------------------------------------
void CvXMLLoadUtility::SetListPairInfoForAudioScripts(int** ppiList, const TCHAR* szRootTagName, int iInfoBaseLength, int iDefaultListVal) {
	if (gDLL->getXMLIFace()->SetToChildByTagName(m_pFXml, szRootTagName)) {
		if (SkipToNextVal()) {
			int iNumSibs = gDLL->getXMLIFace()->GetNumChildren(m_pFXml);
			if (!(0 < iInfoBaseLength)) {
				char	szMessage[1024];
				sprintf(szMessage, "Allocating zero or less memory in CvXMLLoadUtility::SetVariableListTagPair \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
				gDLL->MessageBox(szMessage, "XML Error");
			}
			InitList(ppiList, iInfoBaseLength, iDefaultListVal);
			int* piList = *ppiList;
			if (0 < iNumSibs) {
				if (!(iNumSibs <= iInfoBaseLength)) {
					char	szMessage[1024];
					sprintf(szMessage, "There are more siblings than memory allocated for them in CvXMLLoadUtility::SetVariableListTagPair \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
					gDLL->MessageBox(szMessage, "XML Error");
				}
				if (gDLL->getXMLIFace()->SetToChild(m_pFXml)) {
					TCHAR szTextVal[256];
					for (int i = 0; i < iNumSibs; i++) {
						if (SkipToNextVal() && GetChildXmlVal(szTextVal)) // K-Mod. (without this, a comment in the xml could break this)
						{
							int iIndexVal = FindInInfoClass(szTextVal);
							if (iIndexVal != -1) {
								CvString szTemp;
								GetNextXmlVal(szTemp);
								if (szTemp.GetLength() > 0)
									piList[iIndexVal] = gDLL->getAudioTagIndex(szTemp);
								else
									piList[iIndexVal] = -1;
							}

							gDLL->getXMLIFace()->SetToParent(m_pFXml);
						}

						if (!gDLL->getXMLIFace()->NextSibling(m_pFXml)) {
							break;
						}
					}

					gDLL->getXMLIFace()->SetToParent(m_pFXml);
				}
			}
		}

		gDLL->getXMLIFace()->SetToParent(m_pFXml);
	}
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   SetVariableListTagPair(int **ppiList, const TCHAR* szRootTagName, CvString* m_paszTagList, int iTagListLength, int iDefaultListVal)
//
//  PURPOSE :   allocate and initialize a list from a tag pair in the xml for audio scripts
//
//------------------------------------------------------------------------------------------------------
void CvXMLLoadUtility::SetListPairEnumForAudioScripts(int** ppiList, const TCHAR* szRootTagName, int iTagListLength, int iDefaultListVal) {
	if (gDLL->getXMLIFace()->SetToChildByTagName(m_pFXml, szRootTagName)) {
		if (SkipToNextVal()) {
			int iNumSibs = gDLL->getXMLIFace()->GetNumChildren(m_pFXml);
			if (!(0 < iTagListLength)) {
				char	szMessage[1024];
				sprintf(szMessage, "Allocating zero or less memory in CvXMLLoadUtility::SetVariableListTagPairForAudio \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
				gDLL->MessageBox(szMessage, "XML Error");
			}
			InitList(ppiList, iTagListLength, iDefaultListVal);
			int* piList = *ppiList;
			if (0 < iNumSibs) {
				if (!(iNumSibs <= iTagListLength)) {
					char	szMessage[1024];
					sprintf(szMessage, "There are more siblings than memory allocated for them in CvXMLLoadUtility::SetVariableListTagPairForAudio \n Current XML file is: %s", GC.getCurrentXMLFile().GetCString());
					gDLL->MessageBox(szMessage, "XML Error");
				}
				if (gDLL->getXMLIFace()->SetToChild(m_pFXml)) {
					TCHAR szTextVal[256];
					for (int i = 0; i < iNumSibs; i++) {
						if (SkipToNextVal() && GetChildXmlVal(szTextVal)) // K-Mod. (without this, a comment in the xml could break this)
						{
							int iIndexVal = GC.getTypesEnum(szTextVal);
							if (iIndexVal != -1) {
								CvString szTemp;
								GetNextXmlVal(szTemp);
								if (szTemp.GetLength() > 0)
									piList[iIndexVal] = gDLL->getAudioTagIndex(szTemp);
								else
									piList[iIndexVal] = -1;
							}

							gDLL->getXMLIFace()->SetToParent(m_pFXml);
						}

						if (!gDLL->getXMLIFace()->NextSibling(m_pFXml)) {
							break;
						}
					}

					gDLL->getXMLIFace()->SetToParent(m_pFXml);
				}
			}
		}

		gDLL->getXMLIFace()->SetToParent(m_pFXml);
	}
}

DllExport bool CvXMLLoadUtility::LoadPlayerOptions() {
	// load order: 1
	if (!CreateFXml())
		return false;

	readXMLfiles(true);

	LoadGlobalClassInfo(GC.getPlayerOptionInfo(), "CIV4PlayerOptionInfos", "GameInfo", "Civ4PlayerOptionInfos/PlayerOptionInfos/PlayerOptionInfo");
	FAssert(GC.getNumPlayerOptionInfos() == NUM_PLAYEROPTION_TYPES);

	DestroyFXml();
	return true;
}

DllExport bool CvXMLLoadUtility::LoadGraphicOptions() {
	// load order: 2
	if (!CreateFXml())
		return false;

	LoadGlobalClassInfo(GC.getGraphicOptionInfo(), "CIV4GraphicOptionInfos", "GameInfo", "Civ4GraphicOptionInfos/GraphicOptionInfos/GraphicOptionInfo");
	FAssert(GC.getNumGraphicOptions() == NUM_GRAPHICOPTION_TYPES);

	DestroyFXml();
	return true;
}
