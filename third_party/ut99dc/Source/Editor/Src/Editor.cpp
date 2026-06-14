/*=============================================================================
	Editor.cpp: Unreal editor package.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

// Global variables.
EDITOR_API FGlobalTopicTable GTopics;

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) EDITOR_API FName EDITOR_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "EditorClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// Package implementation.
IMPLEMENT_PACKAGE(Editor);

/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/
