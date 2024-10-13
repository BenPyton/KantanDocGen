// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2024 Benoit Pelletier. All Rights Reserved.

#pragma once

#include "DocFile.h"
#include "UObject/Class.h"

class FStructDocFile : public FMultiDocFile<UScriptStruct>
{
public:
	using FMultiDocFile::FMultiDocFile;

	virtual bool InitDocTree(TSharedPtr<DocTreeNode> DocTree, UScriptStruct* Struct) const override;
	virtual bool UpdateParentDoc(TSharedPtr<DocTreeNode> ParentDocTree, UScriptStruct* Struct) const override;
	virtual bool GenerateTypeMembers(UScriptStruct* Struct) override;

protected:
	virtual FString SubDirName() const { return TEXT("Structs"); }
};
