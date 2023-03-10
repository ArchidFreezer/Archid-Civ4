## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

# For Input see CvOptionsScreenCallbackInterface in Python\EntryPoints\

from CvPythonExtensions import *
import CvUtil
import BugOptions
import BugUtil
import ArchidErrorOptionsTab
import ArchidACOOptionsTab
import ArchidCityDisplayOptionsTab
import ArchidGeneralOptionsTab
import ArchidMapOptionsTab
import ArchidPlatyUIOptionsTab

# globals
gc = CyGlobalContext()
localText = CyTranslator()

class ArchidOptionsScreen:
	"Archid Options Screen"

	def __init__(self):
		self.iScreenHeight = 50
		self.reopen = False
		self.options = BugOptions.getOptions()
		self.callbackIFace = "ArchidOptionsScreenCallbackInterface"
		self.tabs = []

	def getTabControl(self):
		return self.pTabControl

	def addTab(self, tab):
		tab.setOptions(self.options)
		self.tabs.append(tab)

#########################################################################################
################################## SCREEN CONSTRUCTION ##################################
#########################################################################################


	def refreshScreen(self):
		self.reopen = True
		self.close(False)

	def interfaceScreen (self):
		"Initial creation of the screen"
		del self.tabs[:]

		title = BugUtil.getPlainText("TXT_KEY_ARCHID_OPT_TITLE", "Archid Mod Options")
		self.pTabControl = CyGTabCtrl(title, False, False)
		self.pTabControl.setModal(1)
		self.pTabControl.setSize(950, 715)
		self.pTabControl.setControlsExpanding(False)
		self.pTabControl.setColumnLength(self.iScreenHeight)

		if self.options.isLoaded():
			self.addTab(ArchidGeneralOptionsTab.ArchidGeneralOptionsTab(self))
			self.addTab(ArchidCityDisplayOptionsTab.ArchidCityDisplayOptionsTab(self))
			self.addTab(ArchidMapOptionsTab.ArchidMapOptionsTab(self))
			self.addTab(ArchidACOOptionsTab.ArchidACOOptionsTab(self))
			self.addTab(ArchidPlatyUIOptionsTab.ArchidPlatyUIOptionsTab(self))
		else:
			self.addTab(ArchidErrorOptionsTab.ArchidErrorOptionsTab(self))

		self.createTabs()

	def createTabs(self):
		for i, tab in enumerate(self.tabs):
			if not self.reopen or i % 2:
				tab.create(self.pTabControl)

	def clearAllTranslations(self):
		"Clear the translations of all tabs in response to the user choosing a language"
		for tab in self.tabs:
			tab.clearTranslation()

	def close(self, write=True):
		# TODO: check for error
		if (write):
			self.options.write()
		self.pTabControl.destroy()
		self.pTabControl = None
		if self.reopen:
			self.reopen = False
			self.interfaceScreen()

	def setOptionValue(self, name, value):
		option = self.options.getOption(name)
		if (option is not None):
			option.setValue(value)

	def setOptionIndex(self, name, index):
		option = self.options.getOption(name)
		if (option is not None):
			option.setIndex(index)

