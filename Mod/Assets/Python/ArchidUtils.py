## PlayerUtil
##
## Many of these functions come from the BUG mod Python files such as PlayerUtil etc. These however have multiple
## imports meaning that I would need to carry over many more files than I needed so I have copied the functions
## that I use to reduce the number of files in this mod.

from CvPythonExtensions import *

gc = CyGlobalContext()

## Players - Getting IDs and Cy objects 
def getActivePlayerID():
	"""Returns the Player ID for the active player."""
	return gc.getGame().getActivePlayer()

## Players - Iteration
def players(alive=None, human=None, barbarian=None, minor=None, active=None):
	"""
	Creates an iterator for all valid CyPlayers that were ever alive.
	
	Pass in True or False for alive to limit to alive or dead players, respectively.
	Pass in True or False for human to limit to human or AI players, respectively.
	Pass in True or False for barbarian to limit to/from barbarian players, respectively.
	Pass in True or False for minor to limit to/from minor players, respectively.
	Pass in True or False for active to limit to/from players that can ever be active, respectively.
	
	for player in PlayerUtil.players():
		...
	"""
	for ePlayer in range(gc.getMAX_PLAYERS()):
		player = gc.getPlayer(ePlayer)
		if not player.isNone() and player.isEverAlive():
			if matchPlayerOrTeam(player, alive, human, barbarian, minor, active):
				yield player
