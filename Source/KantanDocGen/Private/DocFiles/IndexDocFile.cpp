// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2024 Benoit Pelletier. All Rights Reserved.

#include "IndexDocFile.h"
#include "DocTreeNode.h"

bool FIndexDocFile::InitDocTree(TSharedPtr<DocTreeNode> InDocTree, FString const& InDocTitle) const
{
	InDocTree->AppendChildWithValueEscaped(TEXT("doctype"), TEXT("index"));
	InDocTree->AppendChildWithValueEscaped(TEXT("display_name"), InDocTitle);
	return true;
}

FString FIndexDocFile::GetFileName() const
{
	return TEXT("index");
}
