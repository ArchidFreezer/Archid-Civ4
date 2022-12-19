# Civ4-Archid

## Setup
The easy way to develop and play this mod is to have the git clone in a separate location from your Civ IV mod files and create a link between them using the following syntax:
`mklink /J <Path to Civ4 mod folder>\Archid <path to mod folder in git clone>`
e.g.
`mklink /J "%UserProfile%\Documents\My Games\Beyond the Sword\Mods\Archid" "E:\Projects\Civ4\github\Civ4-Archid.git\mod"`

## Shortcut Keys
There are several additional screens available with the mod that are accessed via shortcut keys rather than UI buttons:

### Platy UI
- [Ctrl-F9] - Art Definition Checker
- [Ctrl-10] - Mirror Image
- [Ctrl-F11] - List of Info IDs from XML files
- [Ctrl-F12] - Shows time details for game speeds 

### Testing
- [Ctrl-Shift-L] - Open popup to switch which player the human controls 
- [Ctrl-Shift-P] - Open popup to change the civ and leader type for any player in the game 
- [Ctrl-Shift-M] - Turn control of your civ over to the AI for the rest of the round. 
- [Ctrl-Shift-X] - Open a dialog for user to select number of turns for AI to play their civ 

## Core Mods included
There are many mods components that I have drawn either code or inspiration from, but the main components that underpin this are:
- K-Mod by Karadoc [https://github.com/karadoc/Civ4-K-Mod]
- Platy Ultrapack by platyping [https://forums.civfanatics.com/threads/platys-peculiar-pleasant-posh-python.497337/#post-12491187]
- BUG [https://forums.civfanatics.com/forums/civ4-bts-unaltered-gameplay-bug-bat-and-bull.268/]

## Issue Management
Issue management is  primarily tracked in a personal JIRA repository that is not available publically. Issues that are raised in GitHub will be reviewed and added to JIRA if appropriate and the GitHub issue will be updated accordingly.

For my reference the JIRA repository is located here: [https://archidfreezer.atlassian.net/jira/software/c/projects/AC4/issues]

## Commit Message Format
Commit messages should be in the format as described here [https://gist.github.com/tonibardina/9290fbc7d605b4f86919426e614fe692] 