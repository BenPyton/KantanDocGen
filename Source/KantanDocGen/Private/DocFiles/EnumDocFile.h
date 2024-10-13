// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2024 Benoit Pelletier. All Rights Reserved.

#pragma once

#include "DocFile.h"
#include "UObject/Class.h"

class FEnumDocFile : public FMultiDocFile<UEnum>
{
public:
	using FMultiDocFile::FMultiDocFile;

	virtual bool InitDocTree(TSharedPtr<DocTreeNode> DocTree, UEnum* Enum) const override;
	virtual bool UpdateParentDoc(TSharedPtr<DocTreeNode> ParentDocTree, UEnum* Enum) const override;
	virtual bool GenerateTypeMembers(UEnum* Enum) override;

protected:
	virtual FString SubDirName() const { return TEXT("Enums"); }
};
