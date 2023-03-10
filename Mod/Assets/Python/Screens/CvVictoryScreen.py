from CvPythonExtensions import *
import CvUtil
import ScreenInput
import time
import BugCore
import BugOptions
import ArchidUtils
gc = CyGlobalContext()
ArtFileMgr = CyArtFileMgr()

VICTORY_CONDITION_SCREEN = 0
GAME_SETTINGS_SCREEN = 1
UN_RESOLUTION_SCREEN = 2
UN_MEMBERS_SCREEN = 3

class CvVictoryScreen:
	def __init__(self, screenId):
		self.screenId = screenId
		self.DEBUG_DROPDOWN_ID =  "VictoryScreenDropdownWidget"
		self.WIDGET_ID = "VictoryScreenWidget"
		self.VC_TAB_ID = "VictoryTabWidget"
		self.SETTINGS_TAB_ID = "SettingsTabWidget"
		self.UN_MEMBERS_TAB_ID = "MembersTabWidget"
		self.SPACESHIP_SCREEN_BUTTON = 1234
		
		self.Z_BACKGROUND = -6.1
		self.Z_CONTROLS = self.Z_BACKGROUND - 0.2

		self.Y_TITLE = 12
		
		self.X_AREA = 10
		self.Y_AREA = 60

		self.X_LINK = 100
		self.MARGIN = 20
								
		self.nWidgetCount = 0
		self.iActivePlayer = -1
		self.bVoteTab = False

		self.iScreen = VICTORY_CONDITION_SCREEN
						
	def getScreen(self):
		return CyGInterfaceScreen("VictoryScreen", self.screenId)
										
	def interfaceScreen(self):
		screen = self.getScreen()
		if screen.isActive():
			return
		self.W_AREA = screen.getXResolution() - 12
		self.H_AREA = (screen.getYResolution() - 120)/24 * 24 + 2
		screen.setRenderInterfaceOnly(True);
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)

		self.iActivePlayer = CyGame().getActivePlayer()
		if self.iScreen == -1:
			self.iScreen = VICTORY_CONDITION_SCREEN
## Unique Background ##
		screen.addDDSGFC("ScreenBackground", ArchidUtils.getBackground(), 0, 0, screen.getXResolution(), screen.getYResolution(), WidgetTypes.WIDGET_GENERAL, -1, -1 )
## Unique Background ##
		screen.addPanel( "TechTopPanel", u"", u"", True, False, 0, 0, screen.getXResolution(), 55, PanelStyles.PANEL_STYLE_TOPBAR )
		screen.addPanel( "TechBottomPanel", u"", u"", True, False, 0, screen.getYResolution() - 55, screen.getXResolution(), 55, PanelStyles.PANEL_STYLE_BOTTOMBAR )
		screen.showWindowBackground( False )
		screen.setText("VictoryScreenExit", "Background", "<font=4>" + CyTranslator().getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper() + "</font>", CvUtil.FONT_RIGHT_JUSTIFY, screen.getXResolution() - 30, screen.getYResolution() - 42, self.Z_CONTROLS, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1 )
		screen.setLabel("VictoryScreenHeader", "Background", u"<font=4b>" + CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_TITLE", ()).upper() + u"</font>", CvUtil.FONT_CENTER_JUSTIFY, screen.getXResolution()/2, self.Y_TITLE, self.Z_CONTROLS, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1)
		
		if self.iScreen == VICTORY_CONDITION_SCREEN:
			self.showVictoryConditionScreen()
		elif self.iScreen == GAME_SETTINGS_SCREEN:
			self.showGameSettingsScreen()
		elif self.iScreen == UN_RESOLUTION_SCREEN:
			self.showVotingScreen()
		elif self.iScreen == UN_MEMBERS_SCREEN:
			self.showMembersScreen()

	def drawTabs(self):
	
		screen = self.getScreen()

		xLink = self.X_LINK
		if (self.iScreen != VICTORY_CONDITION_SCREEN):
			screen.setText(self.VC_TAB_ID, "", u"<font=4>" + CyTranslator().getText("TXT_KEY_MAIN_MENU_VICTORIES", ()).upper() + "</font>", CvUtil.FONT_CENTER_JUSTIFY, xLink, screen.getYResolution() - 42, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1)
		else:
			screen.setText(self.VC_TAB_ID, "", u"<font=4>" + CyTranslator().getColorText("TXT_KEY_MAIN_MENU_VICTORIES", (), gc.getInfoTypeForString("COLOR_YELLOW")).upper() + "</font>", CvUtil.FONT_CENTER_JUSTIFY, xLink, screen.getYResolution() - 42, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1)
		xLink += screen.getXResolution()/4
			
		if (self.iScreen != GAME_SETTINGS_SCREEN):
			screen.setText(self.SETTINGS_TAB_ID, "", u"<font=4>" + CyTranslator().getText("TXT_KEY_MAIN_MENU_SETTINGS", ()).upper() + "</font>", CvUtil.FONT_CENTER_JUSTIFY, xLink, screen.getYResolution() - 42, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1)
		else:
			screen.setText(self.SETTINGS_TAB_ID, "", u"<font=4>" + CyTranslator().getColorText("TXT_KEY_MAIN_MENU_SETTINGS", (), gc.getInfoTypeForString("COLOR_YELLOW")).upper() + "</font>", CvUtil.FONT_CENTER_JUSTIFY, xLink, screen.getYResolution() - 42, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1)
		xLink += screen.getXResolution()/4
			
		if self.bVoteTab:			
			if (self.iScreen != UN_RESOLUTION_SCREEN):
				screen.setText("VotingTabWidget", "", u"<font=4>" + CyTranslator().getText("TXT_KEY_VOTING_TITLE", ()).upper() + "</font>", CvUtil.FONT_CENTER_JUSTIFY, xLink, screen.getYResolution() - 42, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1)
			else:
				screen.setText("VotingTabWidget", "", u"<font=4>" + CyTranslator().getColorText("TXT_KEY_VOTING_TITLE", (), gc.getInfoTypeForString("COLOR_YELLOW")).upper() + "</font>", CvUtil.FONT_CENTER_JUSTIFY, xLink, screen.getYResolution() - 42, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1)
			xLink += screen.getXResolution()/4

			if (self.iScreen != UN_MEMBERS_SCREEN):
				screen.setText(self.UN_MEMBERS_TAB_ID, "", u"<font=4>" + CyTranslator().getText("TXT_KEY_MEMBERS_TITLE", ()).upper() + "</font>", CvUtil.FONT_CENTER_JUSTIFY, xLink, screen.getYResolution() - 42, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1)
			else:
				screen.setText(self.UN_MEMBERS_TAB_ID, "", u"<font=4>" + CyTranslator().getColorText("TXT_KEY_MEMBERS_TITLE", (), gc.getInfoTypeForString("COLOR_YELLOW")).upper() + "</font>", CvUtil.FONT_CENTER_JUSTIFY, xLink, screen.getYResolution() - 42, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1)
			xLink += screen.getXResolution()/4

	def showVotingScreen(self):
		self.deleteAllWidgets()
		activePlayer = gc.getPlayer(self.iActivePlayer)
		iActiveTeam = activePlayer.getTeam()
		
		aiVoteBuilding = []
		for i in xrange(gc.getNumBuildingInfos()):
			for j in range(gc.getNumVoteSourceInfos()):
				if (gc.getBuildingInfo(i).getVoteSourceType() == j):								
					iUNTeam = -1
					bUnknown = true
					for iLoopTeam in xrange(gc.getMAX_CIV_TEAMS()):
						if gc.getTeam(iLoopTeam).isAlive() and not gc.getTeam(iLoopTeam).isMinorCiv():
							if gc.getTeam(iLoopTeam).getBuildingClassCount(gc.getBuildingInfo(i).getBuildingClassType()) > 0:
								iUNTeam = iLoopTeam
								if (iLoopTeam == iActiveTeam or gc.getGame().isDebugMode() or gc.getTeam(activePlayer.getTeam()).isHasMet(iLoopTeam)):
									bUnknown = false		
								break
									
					aiVoteBuilding.append((i, iUNTeam, bUnknown))
				
		if (len(aiVoteBuilding) == 0):
			return

		screen = self.getScreen()
## Unique Background ##
		PanelStyle = PanelStyles.PANEL_STYLE_BLUE50
		if CyUserProfile().getGraphicOption(gc.getInfoTypeForString("GRAPHICOPTION_TRANSPARENT_PANEL")):
			PanelStyle = PanelStyles.PANEL_STYLE_IN
		screen.addPanel(self.getNextWidgetName(), "", "", False, False, self.X_AREA-10, self.Y_AREA-15, self.W_AREA+20, self.H_AREA+30, PanelStyle)
## Unique Background ##
		szTable = self.getNextWidgetName()
		screen.addTableControlGFC(szTable, 2, self.X_AREA, self.Y_AREA, self.W_AREA, self.H_AREA, False, False, 24, 24, TableStyles.TABLE_STYLE_STANDARD)
		screen.enableSelect(szTable, False)		
		screen.setTableColumnHeader(szTable, 0, "", screen.getXResolution() * 3/4)
		screen.setTableColumnHeader(szTable, 1, "", screen.getXResolution()/4)

		for i in xrange(len(aiVoteBuilding)):
			(iVoteBuilding, iUNTeam, bUnknown) = aiVoteBuilding[i]
			iRow = screen.appendTableRow(szTable)
			screen.setTableText(szTable, 0, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_ELECTION", (gc.getBuildingInfo(iVoteBuilding).getDescription(), )), gc.getBuildingInfo(iVoteBuilding).getButton(), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
			if iUNTeam > -1:
				if bUnknown:
					szName = CyTranslator().getText("TXT_KEY_TOPCIVS_UNKNOWN", ())
				else:
					szName = gc.getTeam(iUNTeam).getName()
					iBestLeader = gc.getTeam(iUNTeam).getLeaderID()
					sBestButton = gc.getLeaderHeadInfo(gc.getPlayer(iBestLeader).getLeaderType()).getButton()
				screen.setTableText(szTable, 1, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_BUILT", (szName, )), sBestButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
			else:
				screen.setTableText(szTable, 1, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_NOT_BUILT", ()), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)

			for i in xrange(gc.getNumVoteSourceInfos()):
				if i == gc.getBuildingInfo(iVoteBuilding).getVoteSourceType():
					if CyGame().canHaveSecretaryGeneral(i) and CyGame().getSecretaryGeneral(i) > -1:
						iRow = screen.appendTableRow(szTable)
						screen.setTableText(szTable, 0, iRow, gc.getVoteSourceInfo(i).getSecretaryGeneralText(), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						pSecretaryGeneralTeam = gc.getTeam(gc.getGame().getSecretaryGeneral(i))
						iBestLeader = pSecretaryGeneralTeam.getLeaderID()
						sBestButton = gc.getLeaderHeadInfo(gc.getPlayer(iBestLeader).getLeaderType()).getButton()
						screen.setTableText(szTable, 1, iRow, pSecretaryGeneralTeam.getName(), sBestButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
	
					for iLoop in xrange(gc.getNumVoteInfos()):
						if CyGame().countPossibleVote(iLoop, i) > 0:		
							info = gc.getVoteInfo(iLoop)
							if info.isVoteSourceType(gc.getBuildingInfo(iVoteBuilding).getVoteSourceType()):
								if CyGame().isChooseElection(iLoop):			
									iRow = screen.appendTableRow(szTable)
									screen.setTableText(szTable, 0, iRow, info.getDescription(), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
									if gc.getGame().isVotePassed(iLoop):
										screen.setTableText(szTable, 1, iRow, CyTranslator().getText("TXT_KEY_POPUP_PASSED", ()), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
									else:
										screen.setTableText(szTable, 1, iRow, CyTranslator().getText("TXT_KEY_POPUP_ELECTION_OPTION", (u"", gc.getGame().getVoteRequired(iLoop, i), gc.getGame().countPossibleVote(iLoop, i))), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
			if i <= len(aiVoteBuilding) - 1:
				screen.appendTableRow(szTable)				
		self.drawTabs()

	def findVoteSourceBuilding(self, iVoteSource):
		for iBuilding in xrange(gc.getNumBuildingInfos()):
			BuildingInfo = gc.getBuildingInfo(iBuilding)
			if BuildingInfo.getVoteSourceType() == iVoteSource:
				return iBuilding
		return -1

	def showMembersScreen(self):
	
		self.deleteAllWidgets()
		activePlayer = gc.getPlayer(self.iActivePlayer)
		iActiveTeam = activePlayer.getTeam()
		screen = self.getScreen()

## Unique Background ##
		PanelStyle = PanelStyles.PANEL_STYLE_BLUE50
		if CyUserProfile().getGraphicOption(gc.getInfoTypeForString("GRAPHICOPTION_TRANSPARENT_PANEL")):
			PanelStyle = PanelStyles.PANEL_STYLE_IN
		screen.addPanel(self.getNextWidgetName(), "", "", False, False, self.X_AREA-10, self.Y_AREA-15, self.W_AREA+20, self.H_AREA+30, PanelStyle)
## Unique Background ##
		szTable = self.getNextWidgetName()
		screen.addTableControlGFC(szTable, 2, self.X_AREA, self.Y_AREA, self.W_AREA, self.H_AREA, False, False, 24,24, TableStyles.TABLE_STYLE_STANDARD)
		screen.enableSelect(szTable, False)		
		screen.setTableColumnHeader(szTable, 0, "", screen.getXResolution() * 3/4)
		screen.setTableColumnHeader(szTable, 1, "", screen.getXResolution()/4)

		for i in xrange(gc.getNumVoteSourceInfos()):
			if CyGame().isDiploVote(i):
				kVoteSource = gc.getVoteSourceInfo(i)
				iRow = screen.appendTableRow(szTable)
				iVoteBuilding = self.findVoteSourceBuilding(i)
				sVoteButton = ""
				if iVoteBuilding > -1:
					sVoteButton = gc.getBuildingInfo(iVoteBuilding).getButton()
				screen.setTableText(szTable, 0, iRow, u"<font=4b>" + kVoteSource.getDescription().upper() + u"</font>", sVoteButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
				if CyGame().getVoteSourceReligion(i) > -1:
					screen.setTableText(szTable, 1, iRow, gc.getReligionInfo(CyGame().getVoteSourceReligion(i)).getDescription(), gc.getReligionInfo(CyGame().getVoteSourceReligion(i)).getButton(), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					
				iSecretaryGeneralVote = -1
				if CyGame().canHaveSecretaryGeneral(i) and CyGame().getSecretaryGeneral(i) > -1:		
					for j in xrange(gc.getNumVoteInfos()):
						if gc.getVoteInfo(j).isVoteSourceType(i):
							if gc.getVoteInfo(j).isSecretaryGeneral():
								iSecretaryGeneralVote = j
								break
								
				for j in xrange(gc.getMAX_CIV_PLAYERS()):
					if gc.getPlayer(j).isAlive() and gc.getTeam(iActiveTeam).isHasMet(gc.getPlayer(j).getTeam()):
						szPlayerText = gc.getPlayer(j).getName()
						sBestButton = gc.getLeaderHeadInfo(gc.getPlayer(j).getLeaderType()).getButton()
						if iSecretaryGeneralVote > -1:
							szPlayerText += CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_PLAYER_VOTES", (gc.getPlayer(j).getVotes(iSecretaryGeneralVote, i), )) 
						if (CyGame().canHaveSecretaryGeneral(i) and gc.getGame().getSecretaryGeneral(i) == gc.getPlayer(j).getTeam()):
							iRow = screen.appendTableRow(szTable)
							screen.setTableText(szTable, 0, iRow, szPlayerText, sBestButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
							screen.setTableText(szTable, 1, iRow, gc.getVoteSourceInfo(i).getSecretaryGeneralText(), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						elif (gc.getPlayer(j).isFullMember(i)):
							iRow = screen.appendTableRow(szTable)
							screen.setTableText(szTable, 0, iRow, szPlayerText, sBestButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
							screen.setTableText(szTable, 1, iRow, CyTranslator().getText("TXT_KEY_VOTESOURCE_FULL_MEMBER", ()), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						elif (gc.getPlayer(j).isVotingMember(i)):
							iRow = screen.appendTableRow(szTable)
							screen.setTableText(szTable, 0, iRow, szPlayerText, sBestButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
							screen.setTableText(szTable, 1, iRow, CyTranslator().getText("TXT_KEY_VOTESOURCE_VOTING_MEMBER", ()), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
				iRow = screen.appendTableRow(szTable)					
		self.drawTabs()


	def showGameSettingsScreen(self):
	
		self.deleteAllWidgets()	
		screen = self.getScreen()
		self.SETTINGS_PANEL_X1 = 30
		self.SETTINGS_PANEL_WIDTH = (screen.getXResolution() - 20 - self.SETTINGS_PANEL_X1 * 2)/3
		self.SETTINGS_PANEL_X2 = self.SETTINGS_PANEL_X1 + self.SETTINGS_PANEL_WIDTH + 10
		self.SETTINGS_PANEL_X3 = self.SETTINGS_PANEL_X2 + self.SETTINGS_PANEL_WIDTH + 10
		self.SETTINGS_PANEL_Y = 150
		self.SETTINGS_PANEL_HEIGHT = screen.getYResolution() - self.SETTINGS_PANEL_Y - 110		
		activePlayer = gc.getPlayer(self.iActivePlayer)		

		szSettingsPanel = self.getNextWidgetName()
## Unique Background ##
		PanelStyle = PanelStyles.PANEL_STYLE_MAIN
		if CyUserProfile().getGraphicOption(gc.getInfoTypeForString("GRAPHICOPTION_TRANSPARENT_PANEL")):
			PanelStyle = PanelStyles.PANEL_STYLE_IN
		screen.addPanel(szSettingsPanel, CyTranslator().getText("TXT_KEY_MAIN_MENU_SETTINGS", ()).upper(), "", True, True, self.SETTINGS_PANEL_X1, self.SETTINGS_PANEL_Y - 10, self.SETTINGS_PANEL_WIDTH, self.SETTINGS_PANEL_HEIGHT, PanelStyle)
## Unique Background ##
		szSettingsTable = self.getNextWidgetName()
		screen.addListBoxGFC(szSettingsTable, "", self.SETTINGS_PANEL_X1 + self.MARGIN, self.SETTINGS_PANEL_Y + self.MARGIN, self.SETTINGS_PANEL_WIDTH - 2*self.MARGIN, self.SETTINGS_PANEL_HEIGHT - 2*self.MARGIN, TableStyles.TABLE_STYLE_EMPTY)
		screen.enableSelect(szSettingsTable, False)
		
		screen.appendListBoxStringNoUpdate(szSettingsTable, CyTranslator().getText("TXT_KEY_LEADER_CIV_DESCRIPTION", (activePlayer.getNameKey(), activePlayer.getCivilizationShortDescriptionKey())), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )
		screen.appendListBoxStringNoUpdate(szSettingsTable, u"     (" + CyGameTextMgr().parseLeaderTraits(activePlayer.getLeaderType(), activePlayer.getCivilizationType(), True, False) + ")", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )
		screen.appendListBoxStringNoUpdate(szSettingsTable, " ", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )
		screen.appendListBoxStringNoUpdate(szSettingsTable, CyTranslator().getText("TXT_KEY_SETTINGS_DIFFICULTY", (gc.getHandicapInfo(activePlayer.getHandicapType()).getTextKey(), )), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )
		screen.appendListBoxStringNoUpdate(szSettingsTable, " ", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )
		screen.appendListBoxStringNoUpdate(szSettingsTable, gc.getMap().getMapScriptName(), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )
		screen.appendListBoxStringNoUpdate(szSettingsTable, CyTranslator().getText("TXT_KEY_SETTINGS_MAP_SIZE", (gc.getWorldInfo(gc.getMap().getWorldSize()).getTextKey(), )), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )
		screen.appendListBoxStringNoUpdate(szSettingsTable, CyTranslator().getText("TXT_KEY_SETTINGS_CLIMATE", (gc.getClimateInfo(gc.getMap().getClimate()).getTextKey(), )), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )
		screen.appendListBoxStringNoUpdate(szSettingsTable, CyTranslator().getText("TXT_KEY_SETTINGS_SEA_LEVEL", (gc.getSeaLevelInfo(gc.getMap().getSeaLevel()).getTextKey(), )), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )
		screen.appendListBoxStringNoUpdate(szSettingsTable, " ", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )
		screen.appendListBoxStringNoUpdate(szSettingsTable, CyTranslator().getText("TXT_KEY_SETTINGS_STARTING_ERA", (gc.getEraInfo(gc.getGame().getStartEra()).getTextKey(), )), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )
		screen.appendListBoxStringNoUpdate(szSettingsTable, CyTranslator().getText("TXT_KEY_SETTINGS_GAME_SPEED", (gc.getGameSpeedInfo(gc.getGame().getGameSpeedType()).getTextKey(), )), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )
		
		screen.updateListBox(szSettingsTable)
		
		szOptionsPanel = self.getNextWidgetName()
# Unique Background ##
		screen.addPanel(szOptionsPanel, CyTranslator().getText("TXT_KEY_MAIN_MENU_CUSTOM_SETUP_OPTIONS", ()).upper(), "", True, True, self.SETTINGS_PANEL_X2, self.SETTINGS_PANEL_Y - 10, self.SETTINGS_PANEL_WIDTH, self.SETTINGS_PANEL_HEIGHT, PanelStyle)
## Unique Background ##
		szOptionsTable = self.getNextWidgetName()
		screen.addListBoxGFC(szOptionsTable, "", self.SETTINGS_PANEL_X2 + self.MARGIN, self.SETTINGS_PANEL_Y + self.MARGIN, self.SETTINGS_PANEL_WIDTH - 2*self.MARGIN, self.SETTINGS_PANEL_HEIGHT - 2*self.MARGIN, TableStyles.TABLE_STYLE_EMPTY)
		screen.enableSelect(szOptionsTable, False)

		for i in xrange(GameOptionTypes.NUM_GAMEOPTION_TYPES):
			if gc.getGame().isOption(i):
				screen.appendListBoxStringNoUpdate(szOptionsTable, gc.getGameOptionInfo(i).getDescription(), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)		

		if (gc.getGame().isOption(GameOptionTypes.GAMEOPTION_ADVANCED_START)):
			szNumPoints = u"%s %d" % (CyTranslator().getText("TXT_KEY_ADVANCED_START_POINTS", ()), gc.getGame().getNumAdvancedStartPoints())
			screen.appendListBoxStringNoUpdate(szOptionsTable, szNumPoints, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)		

		if (gc.getGame().isGameMultiPlayer()):
			for i in range(gc.getNumMPOptionInfos()):
				if (gc.getGame().isMPOption(i)):
					screen.appendListBoxStringNoUpdate(szOptionsTable, gc.getMPOptionInfo(i).getDescription(), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
		
			if (gc.getGame().getMaxTurns() > 0):
				szMaxTurns = u"%s %d" % (CyTranslator().getText("TXT_KEY_TURN_LIMIT_TAG", ()), gc.getGame().getMaxTurns())
				screen.appendListBoxStringNoUpdate(szOptionsTable, szMaxTurns, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)		
				
			if (gc.getGame().getMaxCityElimination() > 0):
				szMaxCityElimination = u"%s %d" % (CyTranslator().getText("TXT_KEY_CITY_ELIM_TAG", ()), gc.getGame().getMaxCityElimination())
				screen.appendListBoxStringNoUpdate(szOptionsTable, szMaxCityElimination, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)		

		if (gc.getGame().hasSkippedSaveChecksum()):
			screen.appendListBoxStringNoUpdate(szOptionsTable, "Skipped Checksum", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)		
			
		screen.updateListBox(szOptionsTable)

		szCivsPanel = self.getNextWidgetName()
## Unique Background ##
		screen.addPanel(szCivsPanel, CyTranslator().getText("TXT_KEY_RIVALS_MET", ()).upper(), "", True, True, self.SETTINGS_PANEL_X3, self.SETTINGS_PANEL_Y - 10, self.SETTINGS_PANEL_WIDTH, self.SETTINGS_PANEL_HEIGHT, PanelStyle)
## Unique Background ##
		szCivsTable = self.getNextWidgetName()
		screen.addListBoxGFC(szCivsTable, "", self.SETTINGS_PANEL_X3 + self.MARGIN, self.SETTINGS_PANEL_Y + self.MARGIN, self.SETTINGS_PANEL_WIDTH - 2*self.MARGIN, self.SETTINGS_PANEL_HEIGHT - 2*self.MARGIN, TableStyles.TABLE_STYLE_EMPTY)
		screen.enableSelect(szCivsTable, False)

		for iLoopPlayer in xrange(gc.getMAX_CIV_PLAYERS()):
			player = gc.getPlayer(iLoopPlayer)
			if (player.isEverAlive() and iLoopPlayer != self.iActivePlayer and (gc.getTeam(player.getTeam()).isHasMet(activePlayer.getTeam()) or gc.getGame().isDebugMode()) and not player.isBarbarian() and not player.isMinorCiv()):
				screen.appendListBoxStringNoUpdate(szCivsTable, CyTranslator().getText("TXT_KEY_LEADER_CIV_DESCRIPTION", (player.getNameKey(), player.getCivilizationShortDescriptionKey())), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )
				screen.appendListBoxStringNoUpdate(szCivsTable, u"     (" + CyGameTextMgr().parseLeaderTraits(player.getLeaderType(), player.getCivilizationType(), True, False) + ")", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )
				screen.appendListBoxStringNoUpdate(szCivsTable, " ", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )

		screen.updateListBox(szCivsTable)

		self.drawTabs()
		

	def showVictoryConditionScreen(self):
					
		activePlayer = gc.getPlayer(self.iActivePlayer)
		iActiveTeam = activePlayer.getTeam()
		pTeam = gc.getTeam(iActiveTeam)
		
		# Conquest
		nRivals = -1 
		iBestPopTeam = -1
		bestPop = 0
		iBestScoreTeam = -1
		bestScore = 0
		iBestLandTeam = -1
		bestLand = 0
		iBestCultureTeam = -1
		bestCulture = 0
		for iLoopTeam in xrange(gc.getMAX_CIV_TEAMS()):
			if (gc.getTeam(iLoopTeam).isAlive() and not gc.getTeam(iLoopTeam).isMinorCiv()):
				if not gc.getTeam(iLoopTeam).isVassal(iActiveTeam):
					nRivals += 1

				if iLoopTeam != iActiveTeam and pTeam.isHasMet(iLoopTeam) or CyGame().isDebugMode():
					teamPop = gc.getTeam(iLoopTeam).getTotalPopulation()
					if teamPop > bestPop:
						bestPop = teamPop
						iBestPopTeam = iLoopTeam

					teamScore = CyGame().getTeamScore(iLoopTeam)
					if teamScore > bestScore:
						bestScore = teamScore
						iBestScoreTeam = iLoopTeam

					teamLand = gc.getTeam(iLoopTeam).getTotalLand()
					if teamLand > bestLand:
						bestLand = teamLand
						iBestLandTeam = iLoopTeam

					teamCulture = gc.getTeam(iLoopTeam).countTotalCulture()
					if (teamCulture > bestCulture):
						bestCulture = teamCulture
						iBestCultureTeam = iLoopTeam
		# Population
		totalPop = CyGame().getTotalPopulation()
		ourPop = pTeam.getTotalPopulation()
		popPercent = 0.0
		if (totalPop > 0):
			popPercent = (ourPop * 100.0) / totalPop

		# Score
		ourScore = CyGame().getTeamScore(iActiveTeam)

		# Land Area
		totalLand = CyMap().getLandPlots()
		ourLand = pTeam.getTotalLand()
		landPercent = 0.0
		if (totalLand > 0):
			landPercent = (ourLand * 100.0) / totalLand

		# Religion
		iOurReligion = -1
		ourReligionPercent = 0
		for iLoopReligion in xrange(gc.getNumReligionInfos()):
			if (pTeam.hasHolyCity(iLoopReligion)):
				religionPercent = CyGame().calculateReligionPercent(iLoopReligion)
				if (religionPercent > ourReligionPercent):
					ourReligionPercent = religionPercent
					iOurReligion = iLoopReligion

		iBestReligion = -1
		bestReligionPercent = 0
		for iLoopReligion in xrange(gc.getNumReligionInfos()):
			if (iLoopReligion != iOurReligion):
				religionPercent = CyGame().calculateReligionPercent(iLoopReligion)
				if (religionPercent > bestReligionPercent):
					bestReligionPercent = religionPercent
					iBestReligion = iLoopReligion

		# Total Culture
		ourCulture = pTeam.countTotalCulture()

		# Vote
		aiVoteBuilding = []
		for i in xrange(gc.getNumBuildingInfos()):
			for j in xrange(gc.getNumVoteSourceInfos()):
				if (gc.getBuildingInfo(i).getVoteSourceType() == j):
					iUNTeam = -1
					bUnknown = true 
					for iLoopTeam in xrange(gc.getMAX_CIV_TEAMS()):
						if (gc.getTeam(iLoopTeam).isAlive() and not gc.getTeam(iLoopTeam).isMinorCiv()):
							if (gc.getTeam(iLoopTeam).getBuildingClassCount(gc.getBuildingInfo(i).getBuildingClassType()) > 0):
								iUNTeam = iLoopTeam
								if (iLoopTeam == iActiveTeam or gc.getGame().isDebugMode() or pTeam.isHasMet(iLoopTeam)):
									bUnknown = false		
								break

					aiVoteBuilding.append((i, iUNTeam, bUnknown))

		self.bVoteTab = (len(aiVoteBuilding) > 0)
		
		self.deleteAllWidgets()	
		screen = self.getScreen()
														
		# Start filling in the table below
## Unique Background ##
		PanelStyle = PanelStyles.PANEL_STYLE_BLUE50
		if CyUserProfile().getGraphicOption(gc.getInfoTypeForString("GRAPHICOPTION_TRANSPARENT_PANEL")):
			PanelStyle = PanelStyles.PANEL_STYLE_IN
		screen.addPanel(self.getNextWidgetName(), "", "", False, False, self.X_AREA-10, self.Y_AREA-15, self.W_AREA+20, self.H_AREA+30, PanelStyle)
## Unique Background ##
		szTable = self.getNextWidgetName()
		screen.addTableControlGFC(szTable, 6, self.X_AREA, self.Y_AREA, self.W_AREA, self.H_AREA, False, False, 24,24, TableStyles.TABLE_STYLE_STANDARD)
		screen.setTableColumnHeader(szTable, 0, "", screen.getXResolution()/3)
		screen.setTableColumnHeader(szTable, 1, "", screen.getXResolution()/9)
		screen.setTableColumnHeader(szTable, 2, "", screen.getXResolution()/6)
		screen.setTableColumnHeader(szTable, 3, "", screen.getXResolution()/9)
		screen.setTableColumnHeader(szTable, 4, "", screen.getXResolution()/6)
		screen.setTableColumnHeader(szTable, 5, "", screen.getXResolution()/9)
		screen.appendTableRow(szTable)
## Ultrapack ##	
		iLeaderType = gc.getPlayer(self.iActivePlayer).getLeaderType()
		sLeaderButton = gc.getLeaderHeadInfo(iLeaderType).getButton()
		for iLoopVC in xrange(gc.getNumVictoryInfos()):
			victory = gc.getVictoryInfo(iLoopVC)
			if CyGame().isVictoryValid(iLoopVC):
				
				iNumRows = screen.getTableNumRows(szTable)
				szVictoryType = u"<font=4b>" + victory.getDescription().upper() + u"</font>"
				if (victory.isEndScore() and (gc.getGame().getMaxTurns() > gc.getGame().getElapsedGameTurns())):
					szVictoryType += "    (" + CyTranslator().getText("TXT_KEY_MISC_TURNS_LEFT", (gc.getGame().getMaxTurns() - gc.getGame().getElapsedGameTurns(), )) + ")"

				iVictoryTitleRow = iNumRows - 1
				screen.setTableText(szTable, 0, iVictoryTitleRow, szVictoryType, "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
				bSpaceshipFound = False
					
				bEntriesFound = False
				
				if (victory.isTargetScore() and CyGame().getTargetScore() != 0):
										
					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_TARGET_SCORE", (gc.getGame().getTargetScore(), )), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					screen.setTableText(szTable, 2, iRow, pTeam.getName() + ":", sLeaderButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					screen.setTableText(szTable, 3, iRow, (u"%d" % ourScore), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					
					if (iBestScoreTeam != -1):
						iBestLeader = gc.getTeam(iBestScoreTeam).getLeaderID()
						sBestButton = gc.getLeaderHeadInfo(gc.getPlayer(iBestLeader).getLeaderType()).getButton()
						screen.setTableText(szTable, 4, iRow, gc.getTeam(iBestScoreTeam).getName() + ":", sBestButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 5, iRow, (u"%d" % bestScore), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						
					bEntriesFound = True
				
				if (victory.isEndScore()):

					szText1 = CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_HIGHEST_SCORE", (CyGameTextMgr().getTimeStr(gc.getGame().getStartTurn() + gc.getGame().getMaxTurns(), false), ))

					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, szText1, "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					screen.setTableText(szTable, 2, iRow, pTeam.getName() + ":", sLeaderButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					screen.setTableText(szTable, 3, iRow, (u"%d" % ourScore), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					
					if (iBestScoreTeam != -1):
						iBestLeader = gc.getTeam(iBestScoreTeam).getLeaderID()
						sBestButton = gc.getLeaderHeadInfo(gc.getPlayer(iBestLeader).getLeaderType()).getButton()
						screen.setTableText(szTable, 4, iRow, gc.getTeam(iBestScoreTeam).getName() + ":", sBestButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 5, iRow, (u"%d" % bestScore), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						
					bEntriesFound = True
					
				if (victory.isConquest()):
					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_ELIMINATE_ALL", ()), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					screen.setTableText(szTable, 2, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_RIVALS_LEFT", ()), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					screen.setTableText(szTable, 3, iRow, unicode(nRivals), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					bEntriesFound = True
					
				
				if (CyGame().getAdjustedPopulationPercent(iLoopVC) > 0):			
					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_PERCENT_POP", (gc.getGame().getAdjustedPopulationPercent(iLoopVC), )), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					screen.setTableText(szTable, 2, iRow, pTeam.getName() + ":", sLeaderButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					screen.setTableText(szTable, 3, iRow, (u"%.2f%%" % popPercent), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					if (iBestPopTeam != -1):
						iBestLeader = gc.getTeam(iBestPopTeam).getLeaderID()
						sBestButton = gc.getLeaderHeadInfo(gc.getPlayer(iBestLeader).getLeaderType()).getButton()
						screen.setTableText(szTable, 4, iRow, gc.getTeam(iBestPopTeam).getName() + ":", sBestButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 5, iRow, (u"%.2f%%" % (bestPop * 100 / totalPop)), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					bEntriesFound = True


				if (CyGame().getAdjustedLandPercent(iLoopVC) > 0):
					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_PERCENT_LAND", (gc.getGame().getAdjustedLandPercent(iLoopVC), )), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					screen.setTableText(szTable, 2, iRow, pTeam.getName() + ":", sLeaderButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					screen.setTableText(szTable, 3, iRow, (u"%.2f%%" % landPercent), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					if (iBestLandTeam != -1):
						iBestLeader = gc.getTeam(iBestLandTeam).getLeaderID()
						sBestButton = gc.getLeaderHeadInfo(gc.getPlayer(iBestLeader).getLeaderType()).getButton()
						screen.setTableText(szTable, 4, iRow, gc.getTeam(iBestLandTeam).getName() + ":", sBestButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 5, iRow, (u"%.2f%%" % (bestLand * 100 / totalLand)), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					bEntriesFound = True

				if (victory.getReligionPercent() > 0):			
					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_PERCENT_RELIGION", (victory.getReligionPercent(), )), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					if (iOurReligion != -1):
						screen.setTableText(szTable, 2, iRow, gc.getReligionInfo(iOurReligion).getDescription() + ":", gc.getReligionInfo(iOurReligion).getButton(), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 3, iRow, (u"%d%%" % ourReligionPercent), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					else:
						screen.setTableText(szTable, 2, iRow, pTeam.getName() + ":", sLeaderButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 3, iRow, u"No Holy City", "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					if (iBestReligion != -1):
						screen.setTableText(szTable, 4, iRow, gc.getReligionInfo(iBestReligion).getDescription() + ":", gc.getReligionInfo(iBestReligion).getButton(), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 5, iRow, (u"%d%%" % religionPercent), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					bEntriesFound = True
				
				if (victory.getTotalCultureRatio() > 0):			
					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_PERCENT_CULTURE", (int((100.0 * bestCulture) / victory.getTotalCultureRatio()), )), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					screen.setTableText(szTable, 2, iRow, pTeam.getName() + ":", sLeaderButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					screen.setTableText(szTable, 3, iRow, u"<font=2>%d%c</font>" %(ourCulture, gc.getCommerceInfo(CommerceTypes.COMMERCE_CULTURE).getChar()), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					if (iBestLandTeam != -1):
						iBestLeader = gc.getTeam(iBestLandTeam).getLeaderID()
						sBestButton = gc.getLeaderHeadInfo(gc.getPlayer(iBestLeader).getLeaderType()).getButton()
						screen.setTableText(szTable, 4, iRow, gc.getTeam(iBestCultureTeam).getName() + ":", sBestButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 5, iRow, u"<font=2>%d%c</font>" %(bestCulture, gc.getCommerceInfo(CommerceTypes.COMMERCE_CULTURE).getChar()), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
					bEntriesFound = True

				iBestBuildingTeam = -1
				bestBuilding = 0
				for iLoopTeam in xrange(gc.getMAX_CIV_TEAMS()):
					if (gc.getTeam(iLoopTeam).isAlive() and not gc.getTeam(iLoopTeam).isMinorCiv()):
						if (iLoopTeam != iActiveTeam and (pTeam.isHasMet(iLoopTeam) or CyGame().isDebugMode())):
							teamBuilding = 0
							for i in xrange(gc.getNumBuildingClassInfos()):
								iThreshold = gc.getBuildingClassInfo(i).getVictoryThreshold(iLoopVC)
								if iThreshold > 0:					
									teamBuilding += min(iThreshold, gc.getTeam(iLoopTeam).getBuildingClassCount(i))
							if (teamBuilding > bestBuilding):
								bestBuilding = teamBuilding
								iBestBuildingTeam = iLoopTeam	
											
				for i in xrange(gc.getNumBuildingClassInfos()):
					if (gc.getBuildingClassInfo(i).getVictoryThreshold(iLoopVC) > 0):
						iRow = screen.appendTableRow(szTable)
						szNumber = unicode(gc.getBuildingClassInfo(i).getVictoryThreshold(iLoopVC))
						screen.setTableText(szTable, 0, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_BUILDING", (szNumber, gc.getBuildingClassInfo(i).getTextKey())), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 2, iRow, pTeam.getName() + ":", sLeaderButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 3, iRow, pTeam.getBuildingClassCount(i), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						if (iBestBuildingTeam != -1):
							iBestLeader = gc.getTeam(iBestBuildingTeam).getLeaderID()
							sBestButton = gc.getLeaderHeadInfo(gc.getPlayer(iBestLeader).getLeaderType()).getButton()
							screen.setTableText(szTable, 4, iRow, gc.getTeam(iBestBuildingTeam).getName() + ":", sBestButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
							screen.setTableText(szTable, 5, iRow, gc.getTeam(iBestBuildingTeam).getBuildingClassCount(i), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						bEntriesFound = True
						
				iBestProjectTeam = -1
				bestProject = 0
				for iLoopTeam in xrange(gc.getMAX_CIV_TEAMS()):
					if (gc.getTeam(iLoopTeam).isAlive() and not gc.getTeam(iLoopTeam).isMinorCiv()):
						if (iLoopTeam != iActiveTeam and (pTeam.isHasMet(iLoopTeam) or CyGame().isDebugMode())):
							teamProject = 0
							for i in xrange(gc.getNumProjectInfos()):
								iThreshold = gc.getProjectInfo(i).getVictoryThreshold(iLoopVC)
								if iThreshold > 0:					
									teamProject += min(iThreshold, gc.getTeam(iLoopTeam).getProjectCount(i))
							if (teamProject > bestProject):
								bestProject = teamProject
								iBestProjectTeam = iLoopTeam					
					
				for i in xrange(gc.getNumProjectInfos()):
					if (gc.getProjectInfo(i).getVictoryThreshold(iLoopVC) > 0):
						iRow = screen.appendTableRow(szTable)
						if (gc.getProjectInfo(i).getVictoryMinThreshold(iLoopVC) == gc.getProjectInfo(i).getVictoryThreshold(iLoopVC)):
							szNumber = unicode(gc.getProjectInfo(i).getVictoryThreshold(iLoopVC))
						else:
							szNumber = unicode(gc.getProjectInfo(i).getVictoryMinThreshold(iLoopVC)) + u"-" + unicode(gc.getProjectInfo(i).getVictoryThreshold(iLoopVC))
						screen.setTableText(szTable, 0, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_BUILDING", (szNumber, gc.getProjectInfo(i).getTextKey())), gc.getProjectInfo(i).getButton(), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 2, iRow, pTeam.getName() + ":", sLeaderButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 3, iRow, str(pTeam.getProjectCount(i)), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						
						if (gc.getProjectInfo(i).isSpaceship()):
							bSpaceshipFound = True
						
						if (iBestProjectTeam != -1):
							iBestLeader = gc.getTeam(iBestProjectTeam).getLeaderID()
							sBestButton = gc.getLeaderHeadInfo(gc.getPlayer(iBestLeader).getLeaderType()).getButton()
							screen.setTableText(szTable, 4, iRow, gc.getTeam(iBestProjectTeam).getName() + ":", sBestButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
							screen.setTableText(szTable, 5, iRow, unicode(gc.getTeam(iBestProjectTeam).getProjectCount(i)), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						bEntriesFound = True
						
				#add spaceship button
				if (bSpaceshipFound):
					screen.setButtonGFC("SpaceShipButton" + str(iLoopVC), CyTranslator().getText("TXT_KEY_GLOBELAYER_STRATEGY_VIEW", ()), "", 0, 0, 15, 10, WidgetTypes.WIDGET_GENERAL, self.SPACESHIP_SCREEN_BUTTON, -1, ButtonStyles.BUTTON_STYLE_STANDARD )
					screen.attachControlToTableCell("SpaceShipButton" + str(iLoopVC), szTable, iVictoryTitleRow, 1)
					
					victoryDelay = gc.getTeam(iActiveTeam).getVictoryCountdown(iLoopVC)
					if((victoryDelay > 0) and (gc.getGame().getGameState() != GameStateTypes.GAMESTATE_EXTENDED)):
						victoryDate = CyGameTextMgr().getTimeStr(gc.getGame().getGameTurn() + victoryDelay, false)
						screen.setTableText(szTable, 2, iVictoryTitleRow, CyTranslator().getText("TXT_KEY_SPACE_SHIP_SCREEN_ARRIVAL", ()) + ":", "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 3, iVictoryTitleRow, victoryDate, "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 4, iVictoryTitleRow, CyTranslator().getText("TXT_KEY_REPLAY_SCREEN_TURNS", ()) + ":", "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						screen.setTableText(szTable, 5, iVictoryTitleRow, str(victoryDelay), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						
				if (victory.isDiploVote()):
					for (iVoteBuilding, iUNTeam, bUnknown) in aiVoteBuilding:
						iRow = screen.appendTableRow(szTable)
						screen.setTableText(szTable, 0, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_ELECTION", (gc.getBuildingInfo(iVoteBuilding).getDescription(), )), gc.getBuildingInfo(iVoteBuilding).getButton(), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						if (iUNTeam != -1):
							if bUnknown:
								szName = CyTranslator().getText("TXT_KEY_TOPCIVS_UNKNOWN", ())
								sBestButton = ""
							else:
								szName = gc.getTeam(iUNTeam).getName()
								iBestLeader = gc.getTeam(iUNTeam).getLeaderID()
								sBestButton = gc.getLeaderHeadInfo(gc.getPlayer(iBestLeader).getLeaderType()).getButton()
							
							screen.setTableText(szTable, 2, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_BUILT", (szName, )), sBestButton, WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						else:
							screen.setTableText(szTable, 2, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_NOT_BUILT", ()), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						bEntriesFound = True
					
				if (victory.getCityCulture() != CultureLevelTypes.NO_CULTURELEVEL and victory.getNumCultureCities() > 0):
					ourBestCities = self.getListCultureCities(iActiveTeam)[0:victory.getNumCultureCities()]
					
					iBestCultureTeam = -1
					bestCityCulture = 0
					maxCityCulture = CyGame().getCultureThreshold(victory.getCityCulture())
					for iLoopTeam in xrange(gc.getMAX_CIV_TEAMS()):
						pLoopTeam = gc.getTeam(iLoopTeam)
						if pLoopTeam.isAlive() and not pLoopTeam.isMinorCiv():
							if (iLoopTeam != iActiveTeam and pTeam.isHasMet(iLoopTeam)) or CyGame().isDebugMode():
								theirBestCities = self.getListCultureCities(iLoopTeam)[0:victory.getNumCultureCities()]
								
								iTotalCulture = 0
								for loopCity in theirBestCities:
									iTotalCulture += min(loopCity[0], maxCityCulture)
								
								if (iTotalCulture >= bestCityCulture):
									bestCityCulture = iTotalCulture
									iBestCultureTeam = iLoopTeam

					if (iBestCultureTeam > -1):
						theirBestCities = self.getListCultureCities(iBestCultureTeam)[0:(victory.getNumCultureCities())]
					else:
						theirBestCities = []
					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, CyTranslator().getText("TXT_KEY_VICTORY_SCREEN_CITY_CULTURE", (victory.getNumCultureCities(), gc.getCultureLevelInfo(victory.getCityCulture()).getTextKey())), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)

					for i in xrange(victory.getNumCultureCities()):
						if (len(ourBestCities) > i):
							screen.setTableText(szTable, 2, iRow, ourBestCities[i][1].getName() + ":", gc.getCivilizationInfo(ourBestCities[i][1].getCivilizationType()).getButton(), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
							screen.setTableText(szTable, 3, iRow, u"<font=2>%d%c</font>" %(ourBestCities[i][0], gc.getCommerceInfo(CommerceTypes.COMMERCE_CULTURE).getChar()), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						if (len(theirBestCities) > i):
							screen.setTableText(szTable, 4, iRow, theirBestCities[i][1].getName() + ":", gc.getCivilizationInfo(theirBestCities[i][1].getCivilizationType()).getButton(), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
							screen.setTableText(szTable, 5, iRow, u"<font=2>%d%c</font>" %(theirBestCities[i][0], gc.getCommerceInfo(CommerceTypes.COMMERCE_CULTURE).getChar()), "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY)
						if (i < victory.getNumCultureCities()-1):
							iRow = screen.appendTableRow(szTable)
					bEntriesFound = True
					
				if (bEntriesFound) and iLoopVC != gc.getNumVictoryInfos() -1:
					screen.appendTableRow(szTable)
					screen.appendTableRow(szTable)
## Ultrapack ##

		# civ picker dropdown
		if (CyGame().isDebugMode()):
			self.szDropdownName = self.DEBUG_DROPDOWN_ID
			screen.addDropDownBoxGFC(self.szDropdownName, 22, 12, 300, WidgetTypes.WIDGET_GENERAL, -1, -1, FontTypes.GAME_FONT)
			for j in range(gc.getMAX_PLAYERS()):
				if (gc.getPlayer(j).isAlive()):
					screen.addPullDownString(self.szDropdownName, gc.getPlayer(j).getName(), j, j, False )
		
		self.drawTabs()
## Ultrapack ##
	def getListCultureCities(self, iTeam):
		lCities = []
		for iPlayerX in xrange(gc.getMAX_PLAYERS()):
			pPlayerX = gc.getPlayer(iPlayerX)
			if pPlayerX.isAlive() and pPlayerX.getTeam() == iTeam:
				(loopCity, iter) = pPlayerX.firstCity(False)
				while(loopCity):
					lCities.append([loopCity.getCulture(iPlayerX), loopCity])
					(loopCity, iter) = pPlayerX.nextCity(iter, False)
		lCities.sort()
		lCities.reverse()
		return lCities
## Ultrapack ##				

	def getNextWidgetName(self):
		szName = self.WIDGET_ID + str(self.nWidgetCount)
		self.nWidgetCount += 1
		return szName
	
	def deleteAllWidgets(self):
		screen = self.getScreen()
		i = self.nWidgetCount - 1
		while (i >= 0):
			self.nWidgetCount = i
			screen.deleteWidget(self.getNextWidgetName())
			i -= 1
		self.nWidgetCount = 0		

	def handleInput (self, inputClass):
		if (inputClass.getNotifyCode() == NotifyCode.NOTIFY_LISTBOX_ITEM_SELECTED):
			if (inputClass.getFunctionName() == self.DEBUG_DROPDOWN_ID):
				szName = self.DEBUG_DROPDOWN_ID
				iIndex = self.getScreen().getSelectedPullDownID(szName)
				self.iActivePlayer = self.getScreen().getPullDownData(szName, iIndex)
				self.iScreen = VICTORY_CONDITION_SCREEN
				self.showVictoryConditionScreen()				
		elif (inputClass.getNotifyCode() == NotifyCode.NOTIFY_CLICKED):
			if (inputClass.getFunctionName() == self.VC_TAB_ID):
				self.iScreen = VICTORY_CONDITION_SCREEN
				self.showVictoryConditionScreen()				
			elif (inputClass.getFunctionName() == self.SETTINGS_TAB_ID):
				self.iScreen = GAME_SETTINGS_SCREEN
				self.showGameSettingsScreen()
			elif (inputClass.getFunctionName() == "VotingTabWidget"):
				self.iScreen = UN_RESOLUTION_SCREEN
				self.showVotingScreen()
			elif (inputClass.getFunctionName() == self.UN_MEMBERS_TAB_ID):
				self.iScreen = UN_MEMBERS_SCREEN
				self.showMembersScreen()
			elif (inputClass.getData1() == self.SPACESHIP_SCREEN_BUTTON):
				#close screen
				screen = self.getScreen()
				screen.setDying(True)
				CyInterface().clearSelectedCities()
				
				#popup spaceship screen
				popupInfo = CyPopupInfo()
				popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON_SCREEN)
				popupInfo.setData1(-1)
				popupInfo.setText(u"showSpaceShip")
				popupInfo.addPopup(self.iActivePlayer)

	def update(self, fDelta):
		return