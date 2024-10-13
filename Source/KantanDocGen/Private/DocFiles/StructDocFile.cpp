// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2024 Benoit Pelletier. All Rights Reserved.

#include "StructDocFile.h"
#include "DocTreeNode.h"

bool FStructDocFile::InitDocTree(TSharedPtr<DocTreeNode> DocTree, UScriptStruct* Struct) const
{
	DocTree->AppendChildWithValueEscaped(TEXT("doctype"), TEXT("struct"));
	DocTree->AppendChildWithValueEscaped(TEXT("docs_name"), GetDocTitle());
	DocTree->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Struct));
	DocTree->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Struct));
	DocTree->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Struct));
	DocTree->AppendChildWithValueEscaped(TEXT("sourcepath"), FDocGenHelper::GetSourcePath(Struct));
	DocTree->AppendChildWithValueEscaped(TEXT("classTree"), FDocGenHelper::GetTypeHierarchy(Struct));
	DocTree->AppendChildWithValueEscaped(TEXT("blueprint_type"), FDocGenHelper::GetBoolString(FDocGenHelper::IsBlueprintType(Struct)));
	return true;
}

bool FStructDocFile::UpdateParentDoc(TSharedPtr<DocTreeNode> ParentDocTree, UScriptStruct* Struct) const
{
	auto DocTreeStructsElement = FDocGenHelper::GetChildNode(ParentDocTree, TEXT("structs"), /*bCreate = */true);
	auto DocTreeStruct = DocTreeStructsElement->AppendChild("struct");
	DocTreeStruct->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Struct));
	DocTreeStruct->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Struct));
	DocTreeStruct->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Struct, /*bShortDescription =*/true));
	DocTreeStruct->AppendChildWithValueEscaped(TEXT("type"), FDocGenHelper::GetObjectNativeness(Struct));
	DocTreeStruct->AppendChildWithValueEscaped(TEXT("blueprint_type"), FDocGenHelper::GetBoolString(FDocGenHelper::IsBlueprintType(Struct)));
	return true;
}

bool FStructDocFile::GenerateTypeMembers(UScriptStruct* Struct)
{
	if (!Struct->HasAnyFlags(EObjectFlags::RF_ArchetypeObject | EObjectFlags::RF_ClassDefaultObject))
	{
		auto StructDocTree = CreateDocTree(Struct);
		const bool bHasComment = FDocGenHelper::GenerateDoxygenNode(Struct, StructDocTree);
		if (bHasComment == false)
		{
			FDocGenHelper::PrintWarning(FString::Printf(TEXT("Warning in UScriptStruct: %s"), *Struct->GetName()));
		}

		const bool bIsBlueprintable = (nullptr != Struct->HasMetaDataHierarchical(FBlueprintMetadata::MD_IsBlueprintBase));
		const bool bIsBlueprintType = (nullptr != Struct->HasMetaDataHierarchical(FBlueprintMetadata::MD_AllowableBlueprintVariableType));
		bool bShouldBeDocumented = bIsBlueprintable || bIsBlueprintType;
		bShouldBeDocumented |= FDocGenHelper::GenerateFieldsNode(Struct, StructDocTree);

		if (bShouldBeDocumented)
		{
			AddDocTree(Struct, StructDocTree);
		}
	}
	return true;
}
