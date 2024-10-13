// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2024 Benoit Pelletier. All Rights Reserved.

#pragma once

#include "DocFile.h"

class FIndexDocFile : public FRootDocFile
{
public:
	using FRootDocFile::FRootDocFile;

	virtual bool InitDocTree(TSharedPtr<DocTreeNode> DocTree, FString const& DocTitle) const override;
	virtual FString GetFileName() const override;
};
