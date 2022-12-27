from CvPythonExtensions import *
import CvScreensInterface
import BugOptions

gc = CyGlobalContext()
g_options = BugOptions.getOptions()

def getOptionsScreen():
	return CvScreensInterface.archidOptionsScreen

def parseControlName(name):
	"Parses the name of a control into the section and option values"
	delimPos = name.index('_')
	result = []
	result.append(name[:delimPos])
	result.append(name[delimPos+1:])
	return result

def handleResetButtonInput(argsList):
	"Resets these options"
	g_options.read()
	getOptionsScreen().refreshScreen()
	
	return 1
	
def handleErrorExitButtonInput (argsList):
	"Exits the screen"
	getOptionsScreen().close(False)
	return 1

def handleExitButtonInput (argsList):
	"Exits the screen"
	getOptionsScreen().close()
	return 1

def handleArchidCheckboxClicked(argsList):
	value, name = argsList
	getOptionsScreen().setOptionValue(name, value)
	return 1

def handleArchidTextEditChange(argsList):
	value, name = argsList
	getOptionsScreen().setOptionValue(name, value)
	return 1

def handleArchidDropdownChange(argsList):
	iIndex, name = argsList
	getOptionsScreen().setOptionIndex(name, iIndex)
	return 1

def handleArchidIntDropdownChange(argsList):
	iIndex, name = argsList
	getOptionsScreen().setOptionIndex(name, iIndex)
	return 1

def handleArchidFloatDropdownChange(argsList):
	iIndex, name = argsList
	getOptionsScreen().setOptionIndex(name, iIndex)
	return 1

def handleArchidColorDropdownChange(argsList):
	iIndex, name = argsList
	getOptionsScreen().setOptionIndex(name, iIndex)
	return 1

def handleArchidSliderChanged(argsList):
	value, name = argsList
	getOptionsScreen().setOptionValue(name, value)
	return 1

def handleLanguagesDropdownBoxInput(argsList):
	value, name = argsList
	CvEventInterface.getEventManager().fireEvent("LanguageChanged", value)
	return 1

def handleResolutionDropdownInput(argsList):
	value, name = argsList
	CvEventInterface.getEventManager().fireEvent("ResolutionChanged", value)
	return 1
