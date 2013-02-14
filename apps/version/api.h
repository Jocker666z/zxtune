/*
Abstract:
  Version functions interface

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#pragma once
#ifndef VERSION_API_H_DEFINED
#define VERSION_API_H_DEFINED

//common includes
#include <types.h>

String GetProgramTitle();
String GetProgramVersion();
String GetBuildDate();
String GetBuildPlatform();
String GetBuildArchitecture();
String GetBuildArchitectureVersion();
String GetProgramVersionString();

#endif //VERSION_API_H_DEFINED
