// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2024 Benoit Pelletier. All Rights Reserved.

#include "DocGenHelper.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Message.h"
#include "DocTreeNode.h"
#include "OutputFormats/DocGenOutputFormatFactoryBase.h"
#include "DoxygenParserHelpers.h"
#include "KantanDocGenLog.h"
#include "K2Node_Variable.h"

void FDocGenHelper::TrimTarget(FString& Str)
{
	auto TargetIdx = Str.Find(TEXT("Target is "), ESearchCase::CaseSensitive);
	if (TargetIdx != INDEX_NONE)
	{
		Str = Str.Left(TargetIdx).TrimEnd();
	}
}

FString FDocGenHelper::GetRawDisplayName(const FString& Name)
{
	return FName::NameToDisplayString(Name, false);
}

FString FDocGenHelper::GetObjectRawDisplayName(const UObject* Obj)
{
	if (!Obj)
		return TEXT("NULL");

	if (auto FuncNode = Cast<UK2Node_CallFunction>(Obj))
	{
		auto Func = FuncNode->GetTargetFunction();
		if (Func)
		{
			return GetRawDisplayName(Func->GetName());
		}
	}

	return GetRawDisplayName(Obj->GetName());
}

bool FDocGenHelper::GenerateDoxygenNodeFromComment(const FString& Comment, TSharedPtr<DocTreeNode> ParentNode)
{
	check(ParentNode.IsValid());
	auto DoxygenTags = Detail::ParseDoxygenTagsForString(Comment);
	if (DoxygenTags.Num())
	{
		auto DoxygenElement = ParentNode->AppendChild("doxygen");
		for (auto CurrentTag : DoxygenTags)
		{
			for (auto CurrentValue : CurrentTag.Value)
			{
				DoxygenElement->AppendChildWithValueEscaped(CurrentTag.Key, CurrentValue);
			}
		}
	}
	return Comment.Len() > 0;
}

bool FDocGenHelper::ShouldDocumentPin(const UEdGraphPin* Pin)
{
	return !Pin->bHidden;
}

// For K2 pins only!
bool FDocGenHelper::ExtractPinInformation(const UEdGraphPin* Pin, FString& OutName, FString& OutType, FString& OutDescription)
{
	FString Tooltip;
	Pin->GetOwningNode()->GetPinHoverText(*Pin, Tooltip);

	if (!Tooltip.IsEmpty())
	{
		// @NOTE: This is based on the formatting in UEdGraphSchema_K2::ConstructBasicPinTooltip.
		// If that is changed, this will fail!

		auto TooltipPtr = *Tooltip;

		// Parse name line
		FParse::Line(&TooltipPtr, OutName);
		// Parse type line
		FParse::Line(&TooltipPtr, OutType);

		// Currently there is an empty line here, but FParse::Line seems to gobble up empty lines as part of the
		// previous call. Anyway, attempting here to deal with this generically in case that weird behaviour changes.
		while (*TooltipPtr == TEXT('\n'))
		{
			FString Buf;
			FParse::Line(&TooltipPtr, Buf);
		}

		// What remains is the description
		OutDescription = TooltipPtr;
	}

	// @NOTE: Currently overwriting the name and type as suspect this is more robust to future engine changes.

	OutName = Pin->GetDisplayName().ToString();
	if (OutName.IsEmpty() && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		OutName = Pin->Direction == EEdGraphPinDirection::EGPD_Input ? TEXT("In") : TEXT("Out");
	}

	OutType = UEdGraphSchema_K2::TypeToText(Pin->PinType).ToString();

	return true;
}

bool FDocGenHelper::GetBoolMetadata(const UField* Field, const FName& MetadataName)
{
	if (const UStruct* Struct = Cast<UStruct>(Field))
	{
		return Struct->GetBoolMetaDataHierarchical(MetadataName);
	}

	return Field->GetBoolMetaData(MetadataName);
}

void FDocGenHelper::PrintWarning(const FString& Msg)
{
	UE_LOG(LogKantanDocGen, Warning, TEXT("%s"), *Msg);
#if ENABLE_TEAMCITY_LOGS
	FString LogStr = FString::Printf(TEXT("##teamcity[message status='WARNING' text='%s']\n"), *Msg);
	FPlatformMisc::LocalPrint(*LogStr);
#endif
}

const FString& FDocGenHelper::GetBoolString(bool Value)
{
	static const FString True = TEXT("true");
	static const FString False = TEXT("false");
	return (Value) ? True : False;
}

FString FDocGenHelper::GetNodeShortTitle(const UEdGraphNode* Node)
{
	return Node->GetNodeTitle(ENodeTitleType::ListView).ToString().TrimEnd();
}

FString FDocGenHelper::GetNodeFullTitle(const UEdGraphNode* Node)
{
	FString NodeFullTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	TrimTarget(NodeFullTitle);
	return NodeFullTitle;
}

FString FDocGenHelper::GetNodeDescription(const UEdGraphNode* Node)
{
	FString NodeDesc = Node->GetTooltipText().ToString();
	TrimTarget(NodeDesc);

	const FString DefaultDescription = GetObjectRawDisplayName(Node);
	if (NodeDesc == DefaultDescription)
		return FString();

	return NodeDesc;
}

FString FDocGenHelper::GetTypeSignature(const FProperty* Property)
{
	check(Property);
	FString ExtendedParameters;
	FString ParamConst = (Property->PropertyFlags & CPF_ConstParm) ? TEXT("const ") : TEXT("");
	FString ParamRef = (Property->PropertyFlags & CPF_ReferenceParm) ? TEXT("&") : TEXT("");
	FString ParamType = Property->GetCPPType(&ExtendedParameters);
	return ParamConst + ParamType + ExtendedParameters + ParamRef;
}

FString FDocGenHelper::GetEventSignature(const FProperty* Property)
{
	check(Property);
	auto Delegate = CastField<FMulticastDelegateProperty>(Property);
	check(Delegate);
	// @TODO: Find a way to get the signature of the delegate
	return FString();
}

// UField are UClass, UStruct and UEnum (is there a way to merge with FField?)
FString FDocGenHelper::GetDisplayName(const UField* Field)
{
	return Field ? Field->GetDisplayNameText().ToString() : FString("None");
}

// FField are FProperty (is there a way to merge with UField?)
FString FDocGenHelper::GetDisplayName(const FField* Field)
{
	return Field ? Field->GetDisplayNameText().ToString() : FString("None");
}

FString FDocGenHelper::GetDescription(const UField* Field, bool bShortDescription)
{
	check(Field);
	if (const UClass* Class = Cast<UClass>(Field))
	{
		if (!bShortDescription && Class->HasAllClassFlags(CLASS_Interface))
			return "*UInterface cannot be documented*";
	}

	FString Description = Field->GetToolTipText(bShortDescription).ToString();
	if (Description == GetObjectRawDisplayName(Field))
		return FString();

	return Description;
}

FString FDocGenHelper::GetDescription(const FField* Field, bool bShortDescription)
{
	check(Field);
	FString Description = Field->GetToolTipText(bShortDescription).ToString();
	if (Description == GetRawDisplayName(Field->GetName()))
		return FString();

	return Description;
}

FString FDocGenHelper::GetSourcePath(const UField* Field)
{
	check(Field);
	static const FName MD_RelativePath("ModuleRelativePath");
	FString Path = Field->GetMetaData(MD_RelativePath);
	if (Path.IsEmpty())
	{
		Path = Field->GetPathName();
		Path.RemoveFromEnd("." + Field->GetName());
	}
	return Path;
}

FString FDocGenHelper::GetTypeHierarchy(const UStruct* Struct)
{
	check(Struct);
	const UStruct* Parent = Struct->GetSuperStruct();
	FString OutputStr = FDocGenHelper::GetDisplayName(Struct);
	while (nullptr != Parent)
	{
		// TODO: Create a node list of classes, with a possible ClassDocId if they belong to the doc too.
		// So we could create links in the generated doc to the related parent class pages.
		OutputStr = FString::Printf(TEXT("%s > %s"), *FDocGenHelper::GetDisplayName(Parent), *OutputStr);
		Parent = Parent->GetSuperStruct();
	}
	return OutputStr;
}

FString FDocGenHelper::GetClassGroup(const UClass* Class)
{
	const bool bIsNative = Class->HasAnyClassFlags(CLASS_Native);
	if (bIsNative)
	{
		TArray<FString> Groups;
		Class->GetClassGroupNames(Groups);
		if (Groups.Num() > 0)
			return Groups[0];
	}
	else
	{
		UObject* DO = Class->GetDefaultObject();
		UBlueprint* BP = Cast<UBlueprint>(DO);
		if (nullptr != BP)
		{
			if (!BP->BlueprintCategory.IsEmpty())
				return BP->BlueprintCategory;
			else if (!BP->BlueprintNamespace.IsEmpty())
				return BP->BlueprintNamespace;
		}
	}
	return FString();
}

FString FDocGenHelper::GetCategory(const UEdGraphNode* Node)
{
	if (auto K2Node = Cast<UK2Node>(Node))
	{
		return K2Node->GetMenuCategory().ToString();
	}

	return FString();
}

FString FDocGenHelper::GetCategory(const FField* Field)
{
	return Field->GetMetaDataText(TEXT("Category")).ToString();
}

FString FDocGenHelper::GetObjectNativeness(const UObject* Object)
{
	check(Object);
	return Object->IsNative() ? TEXT("C++") : TEXT("Blueprint");
}

bool FDocGenHelper::IsBlueprintable(const UField* Field)
{
	return GetBoolMetadata(Field, FBlueprintMetadata::MD_IsBlueprintBase);
}

bool FDocGenHelper::IsBlueprintType(const UField* Field)
{
	if (const UStruct* Struct = Cast<UStruct>(Field))
	{
		// Struct is a blueprint type only if "BlueprintType" keyword is present in the hierarchy AND either:
		// - there is no NotBlueprintType
		// - BlueprintType struct is child of NotBlueprintType struct
		const UStruct* FirstBlueprintTypeStruct = Struct->HasMetaDataHierarchical(FBlueprintMetadata::MD_AllowableBlueprintVariableType);
		const UStruct* FirstNotBlueprintTypeStruct = Struct->HasMetaDataHierarchical(FBlueprintMetadata::MD_NotAllowableBlueprintVariableType);
		return (FirstBlueprintTypeStruct != nullptr)
			&& (FirstNotBlueprintTypeStruct == nullptr || FirstBlueprintTypeStruct->IsChildOf(FirstNotBlueprintTypeStruct));
	}
	
	return GetBoolMetadata(Field, FBlueprintMetadata::MD_AllowableBlueprintVariableType);
}

FString FDocGenHelper::GetPropertyBlueprintAccess(const FProperty* Property, bool& bOutRead, bool& bOutWrite)
{
	bOutRead = Property->HasAllPropertyFlags(EPropertyFlags::CPF_BlueprintVisible);
	bOutWrite = bOutRead && !Property->HasAllPropertyFlags(EPropertyFlags::CPF_BlueprintReadOnly);

	if (bOutRead)
	{
		return (bOutWrite) ? TEXT("Read/Write") : TEXT("Read Only");
	}
	return FString();
}

FString FDocGenHelper::GetPropertyEditorAccess(const FProperty* Property, bool& bOutEditable, bool& bOutTemplate, bool& bOutInstance)
{
	bOutEditable = Property->HasAllPropertyFlags(EPropertyFlags::CPF_Edit) && !Property->HasAllPropertyFlags(EPropertyFlags::CPF_EditConst);
	if (bOutEditable)
	{
		bOutTemplate = !Property->HasAllPropertyFlags(EPropertyFlags::CPF_DisableEditOnTemplate);
		bOutInstance = !Property->HasAllPropertyFlags(EPropertyFlags::CPF_DisableEditOnInstance);

		if (bOutTemplate || bOutInstance)
		{
			FString EditorAccess = TEXT("Anywhere");
			if (!bOutTemplate)
				EditorAccess = TEXT("Instance Only");
			if (!bOutInstance)
				EditorAccess = TEXT("Defaults Only");
			return EditorAccess;
		}
	}
	return FString();
}

// @TODO: Move it in a specialized class of FDocFile
FString FDocGenHelper::GetNodeImgName(const UEdGraphNode* Node)
{
	const FString DocId = GetDocId(Node);

	if (const auto* VarNode = Cast<UK2Node_Variable>(Node))
	{
		const FString NodeName = VarNode->GetClass()->GetName();
		if (NodeName.EndsWith(TEXT("Get")))
		{
			return DocId + TEXT("_get");
		}
		else if (NodeName.EndsWith(TEXT("Set")))
		{
			return DocId + TEXT("_set");
		}
	}

	return DocId;
}

// @TODO: Move it in a specialized class of FDocFile
FString FDocGenHelper::GetNodeDirectory(const UEdGraphNode* Node)
{
	if (Node->IsA<UK2Node_Variable>())
	{
		return TEXT("Variables");
	}

	return TEXT("Nodes");
}

// Get a child node of a DocTreeNode, creating it if necessary if bCreate is true.
TSharedPtr<DocTreeNode> FDocGenHelper::GetChildNode(TSharedPtr<DocTreeNode> Parent, const FString& ChildName, bool bCreate)
{
	check(Parent.IsValid());
	TSharedPtr<DocTreeNode> ChildNode = Parent->FindChildByName(ChildName);
	if (!ChildNode.IsValid() && bCreate)
	{
		ChildNode = Parent->AppendChild(ChildName);
	}
	return ChildNode;
}

// UField are UClass, UStruct and UEnum (is there a way to merge with FField?)
bool FDocGenHelper::GenerateDoxygenNode(const UField* Field, TSharedPtr<DocTreeNode> ParentNode)
{
	check(Field);
	const FString& Comment = Field->GetMetaData(TEXT("Comment"));
	return GenerateDoxygenNodeFromComment(Comment, ParentNode);
}

// FField are FProperty (is there a way to merge with UField?)
bool FDocGenHelper::GenerateDoxygenNode(const FField* Field, TSharedPtr<DocTreeNode> ParentNode)
{
	check(Field);
	const FString& Comment = Field->GetMetaData(TEXT("Comment"));
	return GenerateDoxygenNodeFromComment(Comment, ParentNode);
}

bool FDocGenHelper::GenerateFieldsNode(const UStruct* Struct, TSharedPtr<DocTreeNode> ParentNode)
{
	check(Struct && ParentNode.IsValid());
	bool bHasProperties = false;
	for (TFieldIterator<FProperty> PropertyIterator(Struct); PropertyIterator; ++PropertyIterator)
	{
		bool bBlueprintRead, bBlueprintWrite;
		FString BlueprintAccess = GetPropertyBlueprintAccess(*PropertyIterator, bBlueprintRead, bBlueprintWrite);

		bool bEditInEditor, bEditInTemplate, bEditInInstance;
		FString EditorAccess = GetPropertyEditorAccess(*PropertyIterator, bEditInEditor, bEditInTemplate, bEditInInstance);

		const bool bDeprecated = PropertyIterator->HasAnyPropertyFlags(CPF_Deprecated);
		if (!(bBlueprintRead || bBlueprintWrite || bEditInEditor || bDeprecated))
			continue;

		auto MemberList = FDocGenHelper::GetChildNode(ParentNode, TEXT("fields"), /*bCreate = */true);
		auto Member = MemberList->AppendChild(TEXT("field"));
		Member->AppendChildWithValueEscaped("name", PropertyIterator->GetNameCPP());
		Member->AppendChildWithValueEscaped("display_name", FDocGenHelper::GetDisplayName(*PropertyIterator));
		Member->AppendChildWithValueEscaped("type", FDocGenHelper::GetTypeSignature(*PropertyIterator));
		Member->AppendChildWithValueEscaped("category", FDocGenHelper::GetCategory(*PropertyIterator));

		const bool bInherited = FDocGenHelper::GenerateInheritanceNode(*PropertyIterator, Struct, Member);
		bHasProperties |= !bInherited;

		UE_LOG(LogKantanDocGen, Display, TEXT("[%s] Member found: %s (inherited: %s)")
			, *Struct->GetName()
			, *PropertyIterator->GetNameCPP()
			, *FDocGenHelper::GetBoolString(bInherited)
		);

		if (!BlueprintAccess.IsEmpty())
		{
			Member->AppendChildWithValueEscaped("blueprint_access", BlueprintAccess);
		}

		if (!EditorAccess.IsEmpty())
		{
			Member->AppendChildWithValueEscaped("editor_access", EditorAccess);
		}

		if (bDeprecated)
		{
			FText DetailedMessage =
				FText::FromString(PropertyIterator->GetMetaData(FBlueprintMetadata::MD_DeprecationMessage));
			Member->AppendChildWithValueEscaped("deprecated", DetailedMessage.ToString());
		}

		Member->AppendChildWithValueEscaped("description", FDocGenHelper::GetDescription(*PropertyIterator));

		// Generate the detailed doxygen tags from the comment.
		const bool bHasComment = GenerateDoxygenNode(*PropertyIterator, Member);

		// Avoid any property that is part of the superclass and then "redefined" in this Class
		if (bInherited == false && bHasComment == false)
		{
			const bool IsPublic = PropertyIterator->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPublic);
			const FString Context = Cast<UClass>(Struct) ? TEXT("UClass-MemberTag") : TEXT("UScriptStruct-property");

			PrintWarning(FString::Printf(
				TEXT("No description for %s (IsPublic %i): %s::%s")
				, *Context
				, IsPublic
				, *Struct->GetName()
				, *PropertyIterator->GetNameCPP()
			));
		}
	}
	return bHasProperties;
}

bool FDocGenHelper::GenerateEventsNode(const UStruct* Struct, TSharedPtr<DocTreeNode> ParentNode)
{
	check(Struct && ParentNode.IsValid());
	bool bHasEvent = false;
	for (TFieldIterator<FProperty> PropertyIterator(Struct); PropertyIterator; ++PropertyIterator)
	{
		const bool bBlueprintAssignable = PropertyIterator->HasAllPropertyFlags(EPropertyFlags::CPF_BlueprintAssignable);
		const bool bDeprecated = PropertyIterator->HasAnyPropertyFlags(CPF_Deprecated);

		if (!(bBlueprintAssignable))
			continue;

		auto EventList = FDocGenHelper::GetChildNode(ParentNode, TEXT("events"), /*bCreate = */true);
		auto Event = EventList->AppendChild(TEXT("event"));
		Event->AppendChildWithValueEscaped("name", PropertyIterator->GetNameCPP());
		Event->AppendChildWithValueEscaped("display_name", FDocGenHelper::GetDisplayName(*PropertyIterator));
		Event->AppendChildWithValueEscaped("signature", FDocGenHelper::GetEventSignature(*PropertyIterator));
		Event->AppendChildWithValueEscaped("category", FDocGenHelper::GetCategory(*PropertyIterator));

		const bool bInherited = FDocGenHelper::GenerateInheritanceNode(*PropertyIterator, Struct, Event);
		bHasEvent |= !bInherited;
		UE_LOG(LogKantanDocGen, Display, TEXT("[%s] Event found: %s (inherited: %s)")
			, *Struct->GetName()
			, *PropertyIterator->GetNameCPP()
			, *FDocGenHelper::GetBoolString(bInherited)
		);

		if (bDeprecated)
		{
			FText DetailedMessage =
				FText::FromString(PropertyIterator->GetMetaData(FBlueprintMetadata::MD_DeprecationMessage));
			Event->AppendChildWithValueEscaped("deprecated", DetailedMessage.ToString());
		}

		Event->AppendChildWithValueEscaped("description", FDocGenHelper::GetDescription(*PropertyIterator));

		// Generate the detailed doxygen tags from the comment.
		const bool bHasComment = GenerateDoxygenNode(*PropertyIterator, Event);

		// Avoid any property that is part of the superclass and then "redefined" in this Class
		if (bInherited == false && bHasComment == false)
		{
			const bool IsPublic = PropertyIterator->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPublic);
			const FString Context = Cast<UClass>(Struct) ? TEXT("UClass-MemberTag") : TEXT("UScriptStruct-property");

			PrintWarning(FString::Printf(
				TEXT("No description for %s (IsPublic %i): %s::%s")
				, *Context
				, IsPublic
				, *Struct->GetName()
				, *PropertyIterator->GetNameCPP()
			));
		}
	}
	return bHasEvent;
}

bool FDocGenHelper::GenerateParamNode(const UEdGraphPin* Pin, TSharedPtr<DocTreeNode> ParentNode)
{
	if (!ShouldDocumentPin(Pin))
		return false;

	auto Input = ParentNode->AppendChild(TEXT("param"));

	FString PinName, PinType, PinDesc;
	ExtractPinInformation(Pin, PinName, PinType, PinDesc);

	Input->AppendChildWithValueEscaped(TEXT("name"), PinName);
	Input->AppendChildWithValueEscaped(TEXT("type"), PinType);
	Input->AppendChildWithValueEscaped(TEXT("description"), PinDesc);
	return true;
}


bool FDocGenHelper::GenerateInheritanceNode(const FField* Field, const UField* Parent, TSharedPtr<DocTreeNode> ParentNode)
{
	check(Field != nullptr);
	UField* Owner = Field->Owner.Get<UField>();
	const bool bInherited = Owner != Parent;
	if (bInherited)
	{
		auto InheritanceNode = ParentNode->AppendChild(TEXT("inheritedFrom"));
		InheritanceNode->AppendChildWithValueEscaped("id", FDocGenHelper::GetDocId(Owner));
		InheritanceNode->AppendChildWithValueEscaped("display_name", FDocGenHelper::GetDisplayName(Owner));
	}
	return bInherited;
}

// DocId for any UObject derived class.
FString FDocGenHelper::GetDocId(const UObject* Object)
{
	return Object->GetName();
}

// Overload of the GetDocId for UEdGraphNode
FString FDocGenHelper::GetDocId(const UEdGraphNode* Node)
{
	// @TODO: Not sure this is right thing to use
	return Node->GetDocumentationExcerptName();
}

bool FDocGenHelper::SerializeDocToFile(TSharedPtr<DocTreeNode> Doc, const FString& OutputDirectory, const FString& FileName, const TArray<UDocGenOutputFormatFactoryBase*>& OutputFormats)
{
	bool bSuccess = true;
	for (const auto& FactoryObject : OutputFormats)
	{
		auto Serializer = FactoryObject->CreateSerializer();
		Doc->SerializeWith(Serializer);
		bSuccess &= Serializer->SaveToFile(OutputDirectory, FileName);
	}
	return bSuccess;
}

// Return true if the directoy has been created.
bool FDocGenHelper::CreateImgDir(const FString& ParentDirectory)
{
	// @TODO: Should create the img folder only if there is node images
	auto ImagePath = ParentDirectory / "img";
	if (!IFileManager::Get().DirectoryExists(*ImagePath))
	{
		IFileManager::Get().MakeDirectory(*ImagePath, true);
		return true;
	}
	return false;
}
