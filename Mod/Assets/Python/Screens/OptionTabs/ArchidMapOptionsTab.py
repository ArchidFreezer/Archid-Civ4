## ArchidMapOptionsTab
##
## Tab for the Archid Map Options.
##

import BugOptionsTab

class ArchidMapOptionsTab(BugOptionsTab.BugOptionsTab):
	"Archid Map Options Screen Tab"
	
	def __init__(self, screen):
		BugOptionsTab.BugOptionsTab.__init__(self, "Map", "Map")

	def create(self, screen):
		tab = self.createTab(screen)
		panel = self.createMainPanel(screen)
		column = self.addOneColumnLayout(screen, panel)
		
		self.createCityBarPanel(screen, column)
		screen.attachHSeparator(column, column + "Sep1")
		self.createCityTileStatusPanel(screen, column)
		
	def createCityBarPanel(self, screen, panel):
		self.addLabel(screen, panel, "CityBar", "CityBar:")
		self.addCheckbox(screen, panel, "CityBar__StarvationTurns")
		
	def createCityTileStatusPanel(self, screen, panel):
		left, center, right = self.addThreeColumnLayout(screen, panel, "CityPlotsEnabled", True)
		self.addLabel(screen, left, "CityPlots", "City Tiles:")
		self.addCheckbox(screen, center, "CityBar__CityControlledPlots")
		self.addCheckbox(screen, right, "CityBar__CityPlotStatus")
		
		working = self.addOneColumnLayout(screen, panel)
		self.addLabel(screen, working, "WorkingPlots", "Working:")
		wone, wtwo, wthree, wfour = self.addMultiColumnLayout(screen, working, 4, "CityPlotsOptionsWorking")
		self.addCheckbox(screen, wone, "CityBar__WorkingImprovedPlot", True)
		wip1, wip2 = self.addTwoColumnLayout(screen, wone)
		self.addColorDropdown(screen, wip1, wip2, "CityBar__WorkingImprovedPlotColour", True, "LAYOUT_RIGHT")
		self.addFloatDropdown(screen, wip1, wip2, "CityBar__WorkingImprovedPlotAlpha", True, "LAYOUT_RIGHT")
		self.addCheckbox(screen, wtwo, "CityBar__WorkingImprovablePlot", True)
		wdp1, wdp2 = self.addTwoColumnLayout(screen, wtwo)
		self.addColorDropdown(screen, wdp1, wdp2, "CityBar__WorkingImprovablePlotColour", True, "LAYOUT_RIGHT")
		self.addFloatDropdown(screen, wdp1, wdp2, "CityBar__WorkingImprovablePlotAlpha", True, "LAYOUT_RIGHT")
		self.addCheckbox(screen, wthree, "CityBar__WorkingImprovableBonusPlot", True)
		wbp1, wbp2 = self.addTwoColumnLayout(screen, wthree)
		self.addColorDropdown(screen, wbp1, wbp2, "CityBar__WorkingImprovableBonusPlotColour", True, "LAYOUT_RIGHT")
		self.addFloatDropdown(screen, wbp1, wbp2, "CityBar__WorkingImprovableBonusPlotAlpha", True, "LAYOUT_RIGHT")
		self.addCheckbox(screen, wfour, "CityBar__WorkingUnimprovablePlot", True)
		wup1, wup2 = self.addTwoColumnLayout(screen, wfour)
		self.addColorDropdown(screen, wup1, wup2, "CityBar__WorkingUnimprovablePlotColour", True, "LAYOUT_RIGHT")
		self.addFloatDropdown(screen, wup1, wup2, "CityBar__WorkingUnimprovablePlotAlpha", True, "LAYOUT_RIGHT")

		notworking = self.addOneColumnLayout(screen, panel)
		self.addLabel(screen, notworking, "NotWorkingPlots", "Not Working:")
		nwone, nwtwo, nwthree, nwfour = self.addMultiColumnLayout(screen, notworking, 4, "CityPlotsOptionsNotWorking")
		self.addCheckbox(screen, nwone, "CityBar__NotWorkingImprovedPlot", True)
		nwip1, nwip2 = self.addTwoColumnLayout(screen, nwone)
		self.addColorDropdown(screen, nwip1, nwip2, "CityBar__NotWorkingImprovedPlotColour", True, "LAYOUT_RIGHT")
		self.addFloatDropdown(screen, nwip1, nwip2, "CityBar__NotWorkingImprovedPlotAlpha", True, "LAYOUT_RIGHT")
		self.addCheckbox(screen, nwtwo, "CityBar__NotWorkingImprovablePlot", True)
		nwdp1, nwdp2 = self.addTwoColumnLayout(screen, nwtwo)
		self.addColorDropdown(screen, nwdp1, nwdp2, "CityBar__NotWorkingImprovablePlotColour", True, "LAYOUT_RIGHT")
		self.addFloatDropdown(screen, nwdp1, nwdp2, "CityBar__NotWorkingImprovablePlotAlpha", True, "LAYOUT_RIGHT")
		self.addCheckbox(screen, nwthree, "CityBar__NotWorkingImprovableBonusPlot", True)
		nwbp1, nwbp2 = self.addTwoColumnLayout(screen, nwthree)
		self.addColorDropdown(screen, nwbp1, nwbp2, "CityBar__NotWorkingImprovableBonusPlotColour", True, "LAYOUT_RIGHT")
		self.addFloatDropdown(screen, nwbp1, nwbp2, "CityBar__NotWorkingImprovableBonusPlotAlpha", True, "LAYOUT_RIGHT")
		self.addCheckbox(screen, nwfour, "CityBar__NotWorkingUnimprovablePlot", True)
		nwup1, nwup2 = self.addTwoColumnLayout(screen, nwfour)
		self.addColorDropdown(screen, nwup1, nwup2, "CityBar__NotWorkingUnimprovablePlotColour", True, "LAYOUT_RIGHT")
		self.addFloatDropdown(screen, nwup1, nwup2, "CityBar__NotWorkingUnimprovablePlotAlpha", True, "LAYOUT_RIGHT")
		