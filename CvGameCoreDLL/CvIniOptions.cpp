/**********************************************************************

File:		CvIniOptions.cpp
Author:		Based on work by EmperorFool
Created:	2009-01-21

		Copyright (c) 2009 The BUG Mod. All rights reserved.

**********************************************************************/

// This file has been edited for K-Fallout

#include "CvGameCoreDLL.h"
#include "CyArgsList.h"
#include "CvDLLPythonIFaceBase.h"
#include "FVariableSystem.h"

#include "CvIniOptions.h"
#include "CvGlobals.h"

bool getOptionBOOL(const char* option, bool bDefault)
{
	if (GC.hasIniOptBool(option))
		return GC.getIniOptBool(option);

	CyArgsList argsList;
	long lResult = 0;

	argsList.add(option);
	argsList.add(bDefault);

	gDLL->getPythonIFace()->callFunction(PYIniModule, "getOptionBOOL", argsList.makeFunctionArgs(), &lResult);

	GC.setIniOpt(option, lResult != 0);

	return lResult != 0;
}

int getOptionINT(const char* option, int iDefault)
{
	if (GC.hasIniOptInt(option))
		return GC.getIniOptInt(option);

	CyArgsList argsList;
	long lResult = 0;

	argsList.add(option);
	argsList.add(iDefault);

	gDLL->getPythonIFace()->callFunction(PYIniModule, "getOptionINT", argsList.makeFunctionArgs(), &lResult);

	GC.setIniOpt(option, lResult);

	return lResult;
}

CvString getOptionSTRING(const char* option, CvString szDefault)
{
	if (GC.hasIniOptString(option))
		return GC.getIniOptString(option);

	CyArgsList argsList;
	CvString szResult;

	argsList.add(option);
	argsList.add(szDefault);

	gDLL->getPythonIFace()->callFunction(PYIniModule, "getOptionSTRING", argsList.makeFunctionArgs(), &szResult);

	GC.setIniOpt(option, szResult);

	return CvString(szResult);
}

double getOptionDOUBLE(const char* option, float fDefault)
{
	if (GC.hasIniOptDouble(option))
		return GC.getIniOptDouble(option);

	CyArgsList argsList;
	CvString szResult;

	argsList.add(option);
	argsList.add(fDefault);

	gDLL->getPythonIFace()->callFunction(PYIniModule, "getOptionSTRING", argsList.makeFunctionArgs(), &szResult);

	GC.setIniOpt(option, std::atof(szResult.c_str()));

	return std::atof(szResult.c_str());
}


