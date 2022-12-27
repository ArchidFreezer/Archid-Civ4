#pragma once

#ifndef INI_OPTIONS_H
#define INI_OPTIONS_H

// name of the Python module where all the functions that the DLL calls must live
// MUST BE A BUILT-IN MODULE IN THE ENTRYPOINTS FOLDER
// currently CvAppInterface
#define PYIniModule				PYCivModule

bool getOptionBOOL(const char* option, bool bDefault = true);
int getOptionINT(const char* id, int iDefault = 0);
double getOptionDOUBLE(const char* id, float fDefault = 0.0);
CvString getOptionSTRING(const char* id, CvString szDefault = NULL);

#endif