// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2024 Benoit Pelletier. All Rights Reserved.

#include "ClassDocFile.h"
#include "DocTreeNode.h"

namespace
{
	bool IsHidden(UClass* Class)
	{
		check(Class);
		return Class->HasAnyClassFlags(EClassFlags::CLASS_Hidden);
	}
}

bool FClassDocFile::InitDocTree(TSharedPtr<DocTreeNode> DocTree, UClass* Class) const
{
	DocTree->AppendChildWithValueEscaped(TEXT("doctype"), TEXT("class"));
	DocTree->AppendChildWithValueEscaped(TEXT("docs_name"), GetDocTitle());
	DocTree->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Class));
	DocTree->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Class));
	DocTree->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Class));
	DocTree->AppendChildWithValueEscaped(TEXT("sourcepath"), FDocGenHelper::GetSourcePath(Class));
	DocTree->AppendChildWithValueEscaped(TEXT("classTree"), FDocGenHelper::GetTypeHierarchy(Class));
	DocTree->AppendChildWithValueEscaped(TEXT("blueprint_type"), FDocGenHelper::GetBoolString(FDocGenHelper::IsBlueprintType(Class)));
	DocTree->AppendChildWithValueEscaped(TEXT("blueprintable"), FDocGenHelper::GetBoolString(FDocGenHelper::IsBlueprintable(Class)));
	DocTree->AppendChildWithValueEscaped(TEXT("abstract"), FDocGenHelper::GetBoolString(Class->GetBoolMetaData(TEXT("Abstract"))));

	if (!Class->Interfaces.IsEmpty())
	{
		auto DocTreeInterfaceList = DocTree->AppendChild("interfaces");
		for (const auto& Interface : Class->Interfaces)
		{
			auto DocTreeInterface = DocTreeInterfaceList->AppendChild("interface");
			DocTreeInterface->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Interface.Class));
			DocTreeInterface->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Interface.Class));
			DocTreeInterface->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Interface.Class, /*bShortDescription =*/true));
			DocTreeInterface->AppendChildWithValueEscaped(TEXT("native"), FDocGenHelper::GetBoolString(Interface.PointerOffset != 0));
		}
	}

	return true;
}

bool FClassDocFile::UpdateParentDoc(TSharedPtr<DocTreeNode> ParentDocTree, UClass* Class) const
{
	auto DocTreeClassesElement = FDocGenHelper::GetChildNode(ParentDocTree, TEXT("classes"), /*bCreate = */true);
	auto DocTreeClass = DocTreeClassesElement->AppendChild("class");
	DocTreeClass->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Class));
	DocTreeClass->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Class));
	DocTreeClass->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Class, /*bShortDescription =*/true));
	DocTreeClass->AppendChildWithValueEscaped(TEXT("type"), FDocGenHelper::GetObjectNativeness(Class));
	DocTreeClass->AppendChildWithValueEscaped(TEXT("group"), FDocGenHelper::GetClassGroup(Class));
	DocTreeClass->AppendChildWithValueEscaped(TEXT("blueprint_type"), FDocGenHelper::GetBoolString(FDocGenHelper::IsBlueprintType(Class)));
	DocTreeClass->AppendChildWithValueEscaped(TEXT("blueprintable"), FDocGenHelper::GetBoolString(FDocGenHelper::IsBlueprintable(Class)));
	DocTreeClass->AppendChildWithValueEscaped(TEXT("abstract"), FDocGenHelper::GetBoolString(Class->GetBoolMetaData(TEXT("Abstract"))));
	return true;
}

bool FClassDocFile::GenerateTypeMembers(UClass* ClassInstance)
{
	TSharedPtr<DocTreeNode> ClassDocTree = GetDocTree(ClassInstance);
	bool bIsCreated = false;
	if (!ClassDocTree.IsValid())
	{
		ClassDocTree = CreateDocTree(ClassInstance);
		bIsCreated = true;
	}

	const bool bIsBlueprintable = FDocGenHelper::IsBlueprintable(ClassInstance);
	const bool bIsBlueprintType = FDocGenHelper::IsBlueprintType(ClassInstance);
	const bool bIsHidden = ::IsHidden(ClassInstance);
	bool bClassShouldBeDocumented = bIsBlueprintable || bIsBlueprintType;
	bClassShouldBeDocumented |= FDocGenHelper::GenerateFieldsNode(ClassInstance, ClassDocTree);
	bClassShouldBeDocumented |= FDocGenHelper::GenerateEventsNode(ClassInstance, ClassDocTree);
	bClassShouldBeDocumented &= !bIsHidden;

	const bool bHasComment = FDocGenHelper::GenerateDoxygenNode(ClassInstance, ClassDocTree);

	// Only insert this into the map of classdocs if:
	// - it wasnt already in there,
	// - we actually need it to be included
	if (bIsCreated && bClassShouldBeDocumented)
	{
		AddDocTree(ClassInstance, ClassDocTree);
	}

	if (bHasComment == false)
	{
		FDocGenHelper::PrintWarning(FString::Printf(TEXT("No description for UClass: %s"), *ClassInstance->GetName()));
	}

	return true;
}
