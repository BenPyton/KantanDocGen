// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#include "KantanDocGenCommands.h"


#define LOCTEXT_NAMESPACE "KantanDocGen"


void FKantanDocGenCommands::RegisterCommands()
{
	UI_COMMAND(ShowDocGenUI, "Generate Documentation...", "Opens a dialog to generate documentation for Blueprint and Blueprint-exposed C++ classes.", EUserInterfaceActionType::Button, FInputChord());
	NameToCommandMap.Add(TEXT("ShowDocGenUI"), ShowDocGenUI);
}


#undef LOCTEXT_NAMESPACE


