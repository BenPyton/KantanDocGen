// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2024 Benoit Pelletier. All Rights Reserved.

#include "EnumDocFile.h"
#include "DocTreeNode.h"

bool FEnumDocFile::InitDocTree(TSharedPtr<DocTreeNode> DocTree, UEnum* Enum) const
{
	DocTree->AppendChildWithValueEscaped(TEXT("doctype"), TEXT("enum"));
	DocTree->AppendChildWithValueEscaped(TEXT("docs_name"), GetDocTitle());
	DocTree->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Enum));
	DocTree->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Enum));
	DocTree->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Enum));
	DocTree->AppendChildWithValueEscaped(TEXT("sourcepath"), FDocGenHelper::GetSourcePath(Enum));
	DocTree->AppendChildWithValueEscaped(TEXT("blueprint_type"), FDocGenHelper::GetBoolString(FDocGenHelper::IsBlueprintType(Enum)));
	return true;
}

bool FEnumDocFile::UpdateParentDoc(TSharedPtr<DocTreeNode> ParentDocTree, UEnum* Enum) const
{
	auto DocTreeEnumsElement = FDocGenHelper::GetChildNode(ParentDocTree, TEXT("enums"), /*bCreate = */true);
	auto DocTreeEnum = DocTreeEnumsElement->AppendChild("enum");
	DocTreeEnum->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Enum));
	DocTreeEnum->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Enum));
	DocTreeEnum->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Enum, /*bShortDescription =*/true));
	DocTreeEnum->AppendChildWithValueEscaped(TEXT("type"), FDocGenHelper::GetObjectNativeness(Enum));
	DocTreeEnum->AppendChildWithValueEscaped(TEXT("blueprint_type"), FDocGenHelper::GetBoolString(FDocGenHelper::IsBlueprintType(Enum)));
	return true;
}

bool FEnumDocFile::GenerateTypeMembers(UEnum* EnumInstance)
{
	if ((EnumInstance != nullptr) && EnumInstance->HasAnyFlags(RF_NeedLoad))
	{
		EnumInstance->GetLinker()->Preload(EnumInstance);
	}
	EnumInstance->ConditionalPostLoad();

	auto EnumDocTree = CreateDocTree(EnumInstance);

	const bool bHasComment = FDocGenHelper::GenerateDoxygenNode(EnumInstance, EnumDocTree);
	if (bHasComment == false)
	{
		FDocGenHelper::PrintWarning(FString::Printf(TEXT("No description for UEnum: %s"), *EnumInstance->GetName()));
	}

	bool bShouldBeDocumented = FDocGenHelper::IsBlueprintType(EnumInstance);

	auto ValueList = FDocGenHelper::GetChildNode(EnumDocTree, TEXT("values"), /*bCreate = */true);
	for (int32 EnumIndex = 0; EnumIndex < EnumInstance->NumEnums() - 1; ++EnumIndex)
	{
		bool const bShouldBeHidden = EnumInstance->HasMetaData(TEXT("Hidden"), EnumIndex) ||
			EnumInstance->HasMetaData(TEXT("Spacer"), EnumIndex);
		if (bShouldBeHidden)
			continue;

		auto Value = ValueList->AppendChild("value");
		Value->AppendChildWithValueEscaped("name", EnumInstance->GetNameStringByIndex(EnumIndex));
		Value->AppendChildWithValueEscaped("displayname",
			EnumInstance->GetDisplayNameTextByIndex(EnumIndex).ToString());
		Value->AppendChildWithValueEscaped("description",
			EnumInstance->GetToolTipTextByIndex(EnumIndex).ToString());
	}

	if (bShouldBeDocumented)
	{
		AddDocTree(EnumInstance, EnumDocTree);
	}
	return true;
}
