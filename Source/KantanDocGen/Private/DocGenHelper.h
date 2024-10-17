// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2024 Benoit Pelletier. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define ENABLE_TEAMCITY_LOGS 1

struct FDocGenHelper
{
private:
	static void TrimTarget(FString& Str);
	static FString GetRawDisplayName(const FString& Name);
	static FString GetObjectRawDisplayName(const UObject* Obj);
	static bool GenerateDoxygenNodeFromComment(const FString& Comment, TSharedPtr<DocTreeNode> ParentNode);
	static bool ShouldDocumentPin(const UEdGraphPin* Pin);
	static bool ExtractPinInformation(const UEdGraphPin* Pin, FString& OutName, FString& OutType, FString& OutDescription);
	static bool GetBoolMetadata(const UField* Field, const FName& MetadataName);

public:
	static void PrintWarning(const FString& Msg);
	static const FString& GetBoolString(bool Value);
	static FString GetNodeShortTitle(const UEdGraphNode* Node);
	static FString GetNodeFullTitle(const UEdGraphNode* Node);
	static FString GetNodeDescription(const UEdGraphNode* Node);
	static FString GetTypeSignature(const FProperty* Property);
	static FString GetEventSignature(const FProperty* Property);

	// UField are UClass, UStruct and UEnum (is there a way to merge with FField?)
	static FString GetDisplayName(const UField* Field);

	// FField are FProperty (is there a way to merge with UField?)
	static FString GetDisplayName(const FField* Field);

	// Forward GetDisplayName if called on a TWeakObjectPtr<>
	template<typename T>
	static FString GetDisplayName(const TWeakObjectPtr<T>& Ptr)
	{
		return GetDisplayName(Ptr.Get());
	}

	static FString GetDescription(const UField* Field, bool bShortDescription = false);
	static FString GetDescription(const FField* Field, bool bShortDescription = false);
	static FString GetSourcePath(const UField* Field);
	static FString GetTypeHierarchy(const UStruct* Struct);
	static FString GetClassGroup(const UClass* Class);
	static FString GetCategory(const UEdGraphNode* Node);
	static FString GetCategory(const FField* Field);
	static FString GetObjectNativeness(const UObject* Object);
	static bool IsBlueprintable(const UField* Field);
	static bool IsBlueprintType(const UField* Field);
	static FString GetPropertyBlueprintAccess(const FProperty* Property, bool& bOutRead, bool& bOutWrite);
	static FString GetPropertyEditorAccess(const FProperty* Property, bool& bOutEditable, bool& bOutTemplate, bool& bOutInstance);

	static FString GetNodeImgName(const UEdGraphNode* Node);
	static FString GetNodeDirectory(const UEdGraphNode* Node);

	// Get a child node of a DocTreeNode, creating it if necessary if bCreate is true.
	static TSharedPtr<DocTreeNode> GetChildNode(TSharedPtr<DocTreeNode> Parent, const FString& ChildName, bool bCreate = false);

	// UField are UClass, UStruct and UEnum (is there a way to merge with FField?)
	static bool GenerateDoxygenNode(const UField* Field, TSharedPtr<DocTreeNode> ParentNode);

	// FField are FProperty (is there a way to merge with UField?)
	static bool GenerateDoxygenNode(const FField* Field, TSharedPtr<DocTreeNode> ParentNode);
	static bool GenerateFieldsNode(const UStruct* Struct, TSharedPtr<DocTreeNode> ParentNode);
	static bool GenerateEventsNode(const UStruct* Struct, TSharedPtr<DocTreeNode> ParentNode);
	static bool GenerateParamNode(const UEdGraphPin* Pin, TSharedPtr<DocTreeNode> ParentNode);
	static bool GenerateInheritanceNode(const FField* Field, const UField* Parent, TSharedPtr<DocTreeNode> ParentNode);

	// DocId for any UObject derived class.
	static FString GetDocId(const UObject* Object);

	// Overload of the GetDocId for UEdGraphNode
	static FString GetDocId(const UEdGraphNode* Node);

	// Forward GetDocId if called on a TWeakObjectPtr<>
	template<typename T>
	static FString GetDocId(const TWeakObjectPtr<T>& Ptr)
	{
		return GetDocId(Ptr.Get());
	}

	static bool SerializeDocToFile(TSharedPtr<DocTreeNode> Doc, const FString& OutputDirectory, const FString& FileName, const TArray<UDocGenOutputFormatFactoryBase*>& OutputFormats);

	// Return true if the directoy has been created.
	static bool CreateImgDir(const FString& ParentDirectory);

	template<class T>
	static void SerializeDocMap(TMap<T, TSharedPtr<DocTreeNode>> Map, const FString& OutputDirectory, const TArray<UDocGenOutputFormatFactoryBase*>& OutputFormats)
	{
		for (const auto& Entry : Map)
		{
			auto DocId = GetDocId(Entry.Key);
			const auto DocPath = OutputDirectory / DocId;
			FDocGenHelper::SerializeDocToFile(Entry.Value, DocPath, DocId, OutputFormats);
		}
	}
};
