// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#include "NodeDocsGenerator.h"

#include "AnimGraphNode_Base.h"
#include "Async/Async.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintBoundNodeSpawner.h"
#include "BlueprintComponentNodeSpawner.h"
#include "BlueprintEventNodeSpawner.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintNodeSpawner.h"
#include "DocTreeNode.h"
#include "DoxygenParserHelpers.h"
#include "EdGraphSchema_K2.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HighResScreenshot.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Message.h"
#include "KantanDocGenLog.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/EngineVersionComparison.h"
#include "NodeFactory.h"
#include "OutputFormats/DocGenOutputFormatFactoryBase.h"
#include "Runtime/ImageWriteQueue/Public/ImageWriteTask.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"
#include "Slate/WidgetRenderer.h"
#include "Stats/StatsMisc.h"
#include "TextureResource.h"
#include "ThreadingHelpers.h"

#define ENABLE_TEAMCITY_LOGS 1

struct FDocGenHelper
{
private:
	static void TrimTarget(FString& Str)
	{
		auto TargetIdx = Str.Find(TEXT("Target is "), ESearchCase::CaseSensitive);
		if (TargetIdx != INDEX_NONE)
		{
			Str = Str.Left(TargetIdx).TrimEnd();
		}
	}

	static FString GetRawDisplayName(const FString& Name)
	{
		return FName::NameToDisplayString(Name, false);
	}

	static FString GetObjectRawDisplayName(const UObject* Obj)
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

	static bool GenerateDoxygenNodeFromComment(const FString& Comment, TSharedPtr<DocTreeNode> ParentNode)
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

	static bool ShouldDocumentPin(const UEdGraphPin* Pin)
	{
		return !Pin->bHidden;
	}

	// For K2 pins only!
	static bool ExtractPinInformation(const UEdGraphPin* Pin, FString& OutName, FString& OutType, FString& OutDescription)
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

public:

	static void PrintWarning(const FString& Msg)
	{
		UE_LOG(LogKantanDocGen, Warning, TEXT("%s"), *Msg);
#if ENABLE_TEAMCITY_LOGS
		FString LogStr = FString::Printf(TEXT("##teamcity[message status='WARNING' text='%s']\n"), *Msg);
		FPlatformMisc::LocalPrint(*LogStr);
#endif
	}

	static const FString& GetBoolString(bool Value)
	{
		static const FString True = TEXT("true");
		static const FString False = TEXT("false");
		return (Value) ? True : False;
	}

	static FString GetNodeShortTitle(const UEdGraphNode* Node)
	{
		return Node->GetNodeTitle(ENodeTitleType::ListView).ToString().TrimEnd();
	}

	static FString GetNodeFullTitle(const UEdGraphNode* Node)
	{
		FString NodeFullTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		TrimTarget(NodeFullTitle);
		return NodeFullTitle;
	}

	static FString GetNodeDescription(const UEdGraphNode* Node)
	{
		FString NodeDesc = Node->GetTooltipText().ToString();
		TrimTarget(NodeDesc);

		const FString DefaultDescription = GetObjectRawDisplayName(Node);
		if (NodeDesc == DefaultDescription)
			return FString();

		return NodeDesc;
	}

	static FString GetTypeSignature(const FProperty* Property)
	{
		check(Property);
		FString ExtendedParameters;
		FString ParamConst = (Property->PropertyFlags & CPF_ConstParm) ? TEXT("const ") : TEXT("");
		FString ParamRef = (Property->PropertyFlags & CPF_ReferenceParm) ? TEXT("&") : TEXT("");
		FString ParamType = Property->GetCPPType(&ExtendedParameters);
		return ParamConst + ParamType + ExtendedParameters + ParamRef;
	}

	// UField are UClass, UStruct and UEnum (is there a way to merge with FField?)
	static FString GetDisplayName(const UField* Field)
	{
		return Field ? Field->GetDisplayNameText().ToString() : FString("None");
	}

	// FField are FProperty (is there a way to merge with UField?)
	static FString GetDisplayName(const FField* Field)
	{
		return Field ? Field->GetDisplayNameText().ToString() : FString("None");
	}

	// Forward GetDisplayName if called on a TWeakObjectPtr<>
	template<typename T>
	static FString GetDisplayName(const TWeakObjectPtr<T>& Ptr)
	{
		return GetDisplayName(Ptr.Get());
	}

	static FString GetDescription(const UField* Field)
	{
		check(Field);
		if (const UClass* Class = Cast<UClass>(Field))
		{
			if (Class->HasAllClassFlags(CLASS_Interface))
				return "*UInterface cannot be documented*";
		}

		FString Description = Field->GetToolTipText().ToString();
		if (Description == GetObjectRawDisplayName(Field))
			return FString();

		return Description;
	}

	static FString GetDescription(const FField* Field)
	{
		check(Field);
		FString Description = Field->GetToolTipText().ToString();
		if (Description == GetRawDisplayName(Field->GetName()))
			return FString();

		return Description;
	}

	static FString GetSourcePath(const UField* Field)
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

	static FString GetTypeHierarchy(const UStruct* Struct)
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

	static FString GetClassGroup(const UClass* Class)
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

	static FString GetObjectNativeness(const UObject* Object)
	{
		check(Object);
		return Object->IsNative() ? TEXT("C++") : TEXT("Blueprint");
	}

	// Get a child node of a DocTreeNode, creating it if necessary if bCreate is true.
	static TSharedPtr<DocTreeNode> GetChildNode(TSharedPtr<DocTreeNode> Parent, const FString& ChildName, bool bCreate = false)
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
	static bool GenerateDoxygenNode(const UField* Field, TSharedPtr<DocTreeNode> ParentNode)
	{
		check(Field);
		const FString& Comment = Field->GetMetaData(TEXT("Comment"));
		return GenerateDoxygenNodeFromComment(Comment, ParentNode);
	}

	// FField are FProperty (is there a way to merge with UField?)
	static bool GenerateDoxygenNode(const FField* Field, TSharedPtr<DocTreeNode> ParentNode)
	{
		check(Field);
		const FString& Comment = Field->GetMetaData(TEXT("Comment"));
		return GenerateDoxygenNodeFromComment(Comment, ParentNode);
	}

	static bool GenerateFieldsNode(const UStruct* Struct, TSharedPtr<DocTreeNode> ParentNode)
	{
		check(Struct && ParentNode.IsValid());
		bool bHasProperties = false;
		for (TFieldIterator<FProperty> PropertyIterator(Struct); PropertyIterator; ++PropertyIterator)
		{
			if (!(PropertyIterator->PropertyFlags & (CPF_BlueprintVisible | CPF_Edit)
					|| (PropertyIterator->HasAnyPropertyFlags(CPF_Deprecated))))
				continue;

			bHasProperties = true;

			UStruct* Parent = Struct->GetSuperStruct();
			const bool bInherited = Parent && PropertyIterator->IsInContainer(Parent);
			UE_LOG(LogKantanDocGen, Display, TEXT("member for %s found : %s (inherited: %d [parent: %s])"), *Struct->GetName(), *PropertyIterator->GetNameCPP(), bInherited, *GetNameSafe(Parent));

			auto MemberList = FDocGenHelper::GetChildNode(ParentNode, TEXT("fields"), /*bCreate = */true);
			auto Member = MemberList->AppendChild(TEXT("field"));
			Member->AppendChildWithValueEscaped("name", PropertyIterator->GetNameCPP());
			Member->AppendChildWithValueEscaped("display_name", FDocGenHelper::GetDisplayName(*PropertyIterator));
			Member->AppendChildWithValueEscaped("type", FDocGenHelper::GetTypeSignature(*PropertyIterator));
			Member->AppendChildWithValueEscaped("inherited", FDocGenHelper::GetBoolString(bInherited));

			if (PropertyIterator->HasAnyPropertyFlags(CPF_Deprecated))
			{
				FText DetailedMessage =
					FText::FromString(PropertyIterator->GetMetaData(FBlueprintMetadata::MD_DeprecationMessage));
				Member->AppendChildWithValueEscaped("deprecated", DetailedMessage.ToString());
			}

			FString PropDesc = PropertyIterator->GetToolTipText().ToString();
			if (PropDesc != GetRawDisplayName(PropertyIterator->GetName()))
				Member->AppendChildWithValueEscaped("description", PropDesc);

			// Generate the detailed doxygen tags from the comment.
			const bool bHasComment = GenerateDoxygenNode(*PropertyIterator, Member);

			// Avoid any property that is part of the superclass and then "redefined" in this Class
			if (bInherited == false && bHasComment == false)
			{
				const bool IsPublic = PropertyIterator->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPublic);
				const FString Context = Cast<UClass>(Struct) ? TEXT("UClass-MemberTag") : TEXT("UScriptStruct-property");

				PrintWarning(FString::Printf(
					TEXT("No doc for %s (IsPublic %i): %s::%s")
					, *Context
					, IsPublic
					, *Struct->GetName()
					, *PropertyIterator->GetNameCPP()
				));
			}
		}
		return bHasProperties;
	}

	static bool GenerateParamNode(const UEdGraphPin* Pin, TSharedPtr<DocTreeNode> ParentNode)
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

	// DocId for any UObject derived class.
	static FString GetDocId(const UObject* Object)
	{
		return Object->GetName();
	}

	// Overload of the GetDocId for UEdGraphNode
	static FString GetDocId(const UEdGraphNode* Node)
	{
		// @TODO: Not sure this is right thing to use
		return Node->GetDocumentationExcerptName();
	}

	// Forward GetDocId if called on a TWeakObjectPtr<>
	template<typename T>
	static FString GetDocId(const TWeakObjectPtr<T>& Ptr)
	{
		return GetDocId(Ptr.Get());
	}

	static bool SerializeDocToFile(TSharedPtr<DocTreeNode> Doc, const FString& OutputDirectory, const FString& FileName, const TArray<UDocGenOutputFormatFactoryBase*>& OutputFormats)
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
	static bool CreateImgDir(const FString& ParentDirectory)
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

	template<class T>
	static void SerializeDocMap(TMap<T, TSharedPtr<DocTreeNode>> Map, const FString& OutputDirectory, const TArray<UDocGenOutputFormatFactoryBase*>& OutputFormats)
	{
		for (const auto& Entry : Map)
		{
			FString DocId = GetDocId(Entry.Key);
			FString DocDisplayName = GetDisplayName(Entry.Key).Replace(TEXT(" "), TEXT("-"));
			const auto DocPath = OutputDirectory / DocId;
			FDocGenHelper::CreateImgDir(DocPath);
			FDocGenHelper::SerializeDocToFile(Entry.Value, DocPath, DocDisplayName, OutputFormats);
		}
	}
};

FNodeDocsGenerator::~FNodeDocsGenerator()
{
	CleanUp();
}

bool FNodeDocsGenerator::GT_Init(FString const& InDocsTitle, FString const& InOutputDir, UClass* BlueprintContextClass)
{
	DummyBP = CastChecked<UBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		BlueprintContextClass, ::GetTransientPackage(), NAME_None, EBlueprintType::BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None));
	if (!DummyBP.IsValid())
	{
		return false;
	}

	Graph = FBlueprintEditorUtils::CreateNewGraph(DummyBP.Get(), TEXT("TempoGraph"), UEdGraph::StaticClass(),
												  UEdGraphSchema_K2::StaticClass());

	DummyBP->AddToRoot();
	Graph->AddToRoot();

	GraphPanel = SNew(SGraphPanel).GraphObj(Graph.Get());
	// We want full detail for rendering, passing a super-high zoom value will guarantee the highest LOD.
	GraphPanel->RestoreViewSettings(FVector2D(0, 0), 10.0f);

	DocsTitle = InDocsTitle;

	IndexTree = InitIndexDocTree(DocsTitle);

	ClassDocTreeMap.Empty();
	OutputDir = InOutputDir;

	return true;
}

UK2Node* FNodeDocsGenerator::GT_InitializeForSpawner(UBlueprintNodeSpawner* Spawner, UObject* SourceObject,
													 FNodeProcessingState& OutState)
{
	if (!IsSpawnerDocumentable(Spawner, SourceObject->IsA<UBlueprint>()))
	{
		UE_LOG(
			LogKantanDocGen,
			Warning, TEXT("Not documentable %s with node class %s. SourceObject: %s"),
			*Spawner->GetClass()->GetName(),
			Spawner->NodeClass ? *Spawner->NodeClass->GetName() : TEXT("None"),
			*SourceObject->GetName()
		);
		return nullptr;
	}

	// Spawn an instance into the graph
	auto NodeInst = Spawner->Invoke(Graph.Get(), IBlueprintNodeBinder::FBindingSet {}, FVector2D(0, 0));

	// Currently Blueprint nodes only
	const auto K2NodeInst = Cast< UK2Node >(NodeInst);

	if (K2NodeInst == nullptr)
	{
		UE_LOG(LogKantanDocGen, Warning, TEXT("Failed to create node from spawner of class %s with node class %s."),
			   *Spawner->GetClass()->GetName(), Spawner->NodeClass ? *Spawner->NodeClass->GetName() : TEXT("None"));
		return nullptr;
	}

	const auto AssociatedClass = MapToAssociatedClass(K2NodeInst, SourceObject);
	
	// TODO: Should UserDefinedStruct and UserDefinedEnum pass here?
	if (!AssociatedClass) 
		return nullptr;

	// Create the class doc tree if necessary.
	GetClassDocTree(AssociatedClass, /*bCreate = */true);

	OutState = FNodeProcessingState();
	OutState.ClassDocsPath = OutputDir / FDocGenHelper::GetDocId(AssociatedClass);
	OutState.ClassDocTree = ClassDocTreeMap.FindChecked(AssociatedClass);

	return K2NodeInst;
}

bool FNodeDocsGenerator::GT_Finalize(FString OutputPath)
{
	if (!SaveClassDocFile(OutputPath))
	{
		return false;
	}
	if (!SaveEnumDocFile(OutputPath))
	{
		return false;
	}
	if (!SaveStructDocFile(OutputPath))
	{
		return false;
	}
	if (!SaveIndexFile(OutputPath))
	{
		return false;
	}

	return true;
}

void FNodeDocsGenerator::CleanUp()
{
	if (GraphPanel.IsValid())
	{
		GraphPanel.Reset();
	}

	if (DummyBP.IsValid())
	{
		DummyBP->RemoveFromRoot();
		DummyBP.Reset();
	}
	if (Graph.IsValid())
	{
		Graph->RemoveFromRoot();
		Graph.Reset();
	}
}

bool FNodeDocsGenerator::GenerateNodeImage(UEdGraphNode* Node, FNodeProcessingState& State)
{
	SCOPE_SECONDS_COUNTER(GenerateNodeImageTime);

	const FVector2D DrawSize(1024.0f, 1024.0f);

	bool bSuccess = false;

	AdjustNodeForSnapshot(Node);

	FString NodeName = FDocGenHelper::GetDocId(Node);

	FIntRect Rect;

	TUniquePtr<TImagePixelData<FColor>> PixelData;

	auto RenderNodeResult = Async(EAsyncExecution::TaskGraphMainThread, [this, Node, DrawSize, &Rect, &PixelData] {
		auto NodeWidget = FNodeFactory::CreateNodeWidget(Node);
		NodeWidget->SetOwner(GraphPanel.ToSharedRef());

		const bool bUseGammaCorrection = false;
		FWidgetRenderer Renderer(false);
		Renderer.SetIsPrepassNeeded(true);
		auto RenderTarget = Renderer.DrawWidget(NodeWidget.ToSharedRef(), DrawSize);
		auto Desired = NodeWidget->GetDesiredSize();
#if UE_VERSION_NEWER_THAN(5, 0, 0)
		FlushRenderingCommands();
#else 
		FlushRenderingCommands(true);
#endif
		FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
		Rect = FIntRect(0, 0, (int32) Desired.X, (int32) Desired.Y);
		FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
		ReadPixelFlags.SetLinearToGamma(true); // @TODO: is this gamma correction, or something else?

		PixelData = MakeUnique<TImagePixelData<FColor>>(FIntPoint((int32) Desired.X, (int32) Desired.Y));
		PixelData->Pixels.SetNumUninitialized((int32) Desired.X * (int32) Desired.Y);

		if (RTResource->ReadPixelsPtr(PixelData->Pixels.GetData(), ReadPixelFlags, Rect) == false)
		{
			UE_LOG(LogKantanDocGen, Warning, TEXT("Failed to read pixels for node image."));
			return false;
		}
		BeginReleaseResource(RTResource);
		return true;
	});

	if (!RenderNodeResult.Get())
	{
		return false;
	}

	State.RelImageBasePath = TEXT("../img");
	FString ImageBasePath = State.ClassDocsPath / TEXT("img"); // State.RelImageBasePath;
	if (!IFileManager::Get().DirectoryExists(*ImageBasePath))
	{
		IFileManager::Get().MakeDirectory(*ImageBasePath, true);
	}
	FString ImgFilename = FString::Printf(TEXT("nd_img_%s.png"), *NodeName);
	FString ScreenshotSaveName = ImageBasePath / ImgFilename;

	TUniquePtr<FImageWriteTask> ImageTask = MakeUnique<FImageWriteTask>();
	ImageTask->PixelData = MoveTemp(PixelData);
	ImageTask->Filename = ScreenshotSaveName;
	ImageTask->Format = EImageFormat::PNG;
	ImageTask->CompressionQuality = (int32) EImageCompressionQuality::Default;
	ImageTask->bOverwriteFile = true;
	ImageTask->PixelPreProcessors.Add([](FImagePixelData* PixelData) {
		check(PixelData->GetType() == EImagePixelType::Color);

		TImagePixelData<FColor>* ColorData = static_cast<TImagePixelData<FColor>*>(PixelData);
		for (FColor& Pixel : static_cast<TImagePixelData<FColor>*>(PixelData)->Pixels)
		{
			if (Pixel.A >= 90)
			{
				Pixel.A = 255;
			}
		}
	});

	if (ImageTask->RunTask())
	{
		// Success!
		bSuccess = true;
		State.ImageFilename = ImgFilename;
	}
	else
	{
		UE_LOG(LogKantanDocGen, Warning, TEXT("Failed to save screenshot image for node: %s"), *NodeName);
	}

	return bSuccess;
}

TSharedPtr<DocTreeNode> FNodeDocsGenerator::InitIndexDocTree(FString const& IndexTitle)
{
	TSharedPtr<DocTreeNode> IndexDocTree = MakeShared<DocTreeNode>();
	IndexDocTree->AppendChildWithValueEscaped(TEXT("doctype"), TEXT("index"));
	IndexDocTree->AppendChildWithValueEscaped(TEXT("display_name"), IndexTitle);
	return IndexDocTree;
}

TSharedPtr<DocTreeNode> FNodeDocsGenerator::GetClassDocTree(UClass* Class, bool bCreate /* = false*/)
{
	TSharedPtr<DocTreeNode>* FoundClassDocTree = ClassDocTreeMap.Find(Class);
	if (FoundClassDocTree)
		return *FoundClassDocTree;

	if (!bCreate)
		return nullptr;

	TSharedPtr<DocTreeNode> NewClassDocTree = InitClassDocTree(Class);
	ClassDocTreeMap.Add(Class, NewClassDocTree);
	UpdateIndexDocWithClass(IndexTree, Class);
	return NewClassDocTree;
}

TSharedPtr<DocTreeNode> FNodeDocsGenerator::InitClassDocTree(UClass* Class)
{
	TSharedPtr<DocTreeNode> ClassDoc = MakeShared<DocTreeNode>();
	ClassDoc->AppendChildWithValueEscaped(TEXT("doctype"), TEXT("class"));
	ClassDoc->AppendChildWithValueEscaped(TEXT("docs_name"), DocsTitle);
	ClassDoc->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Class));
	ClassDoc->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Class));
	ClassDoc->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Class));
	ClassDoc->AppendChildWithValueEscaped(TEXT("sourcepath"), FDocGenHelper::GetSourcePath(Class));
	ClassDoc->AppendChildWithValueEscaped(TEXT("classTree"), FDocGenHelper::GetTypeHierarchy(Class));
	return ClassDoc;
}

TSharedPtr<DocTreeNode> FNodeDocsGenerator::InitStructDocTree(UScriptStruct* Struct)
{
	TSharedPtr<DocTreeNode> StructDoc = MakeShared<DocTreeNode>();
	StructDoc->AppendChildWithValueEscaped(TEXT("doctype"), TEXT("struct"));
	StructDoc->AppendChildWithValueEscaped(TEXT("docs_name"), DocsTitle);
	StructDoc->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Struct));
	StructDoc->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Struct));
	StructDoc->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Struct));
	StructDoc->AppendChildWithValueEscaped(TEXT("sourcepath"), FDocGenHelper::GetSourcePath(Struct));
	StructDoc->AppendChildWithValueEscaped(TEXT("classTree"), FDocGenHelper::GetTypeHierarchy(Struct));
	return StructDoc;
}

TSharedPtr<DocTreeNode> FNodeDocsGenerator::InitEnumDocTree(UEnum* Enum)
{
	TSharedPtr<DocTreeNode> EnumDoc = MakeShared<DocTreeNode>();
	EnumDoc->AppendChildWithValueEscaped(TEXT("doctype"), TEXT("enum"));
	EnumDoc->AppendChildWithValueEscaped(TEXT("docs_name"), DocsTitle);
	EnumDoc->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Enum));
	EnumDoc->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Enum));
	EnumDoc->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Enum));
	EnumDoc->AppendChildWithValueEscaped(TEXT("sourcepath"), FDocGenHelper::GetSourcePath(Enum));
	return EnumDoc;
}

bool FNodeDocsGenerator::UpdateIndexDocWithClass(TSharedPtr<DocTreeNode> DocTree, UClass* Class)
{
	auto DocTreeClassesElement = FDocGenHelper::GetChildNode(DocTree, TEXT("classes"), /*bCreate = */true);
	auto DocTreeClass = DocTreeClassesElement->AppendChild("class");
	DocTreeClass->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Class));
	DocTreeClass->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Class));
	DocTreeClass->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Class));
	DocTreeClass->AppendChildWithValueEscaped(TEXT("group"), FDocGenHelper::GetClassGroup(Class));
	DocTreeClass->AppendChildWithValueEscaped(TEXT("type"), FDocGenHelper::GetObjectNativeness(Class));
	return true;
}

bool FNodeDocsGenerator::UpdateIndexDocWithStruct(TSharedPtr<DocTreeNode> DocTree, UStruct* Struct)
{
	auto DocTreeStructsElement = FDocGenHelper::GetChildNode(DocTree, TEXT("structs"), /*bCreate = */true);
	auto DocTreeStruct = DocTreeStructsElement->AppendChild("struct");
	DocTreeStruct->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Struct));
	DocTreeStruct->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Struct));
	DocTreeStruct->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Struct));
	DocTreeStruct->AppendChildWithValueEscaped(TEXT("type"), FDocGenHelper::GetObjectNativeness(Struct));
	return true;
}

bool FNodeDocsGenerator::UpdateIndexDocWithEnum(TSharedPtr<DocTreeNode> DocTree, UEnum* Enum)
{
	auto DocTreeEnumsElement = FDocGenHelper::GetChildNode(DocTree, TEXT("enums"), /*bCreate = */true);
	auto DocTreeEnum = DocTreeEnumsElement->AppendChild("enum");
	DocTreeEnum->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Enum));
	DocTreeEnum->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Enum));
	DocTreeEnum->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Enum));
	DocTreeEnum->AppendChildWithValueEscaped(TEXT("type"), FDocGenHelper::GetObjectNativeness(Enum));
	return true;
}

bool FNodeDocsGenerator::UpdateClassDocWithNode(TSharedPtr<DocTreeNode> DocTree, UEdGraphNode* Node)
{
	auto DocTreeNodesElement = FDocGenHelper::GetChildNode(DocTree, TEXT("nodes"), /*bCreate = */true);
	auto DocTreeNode = DocTreeNodesElement->AppendChild("node");
	DocTreeNode->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Node));
	DocTreeNode->AppendChildWithValueEscaped(TEXT("shorttitle"), FDocGenHelper::GetNodeShortTitle(Node));
	DocTreeNode->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetNodeDescription(Node));
	DocTreeNode->AppendChildWithValueEscaped(TEXT("type"), FDocGenHelper::GetObjectNativeness(Node));
	return true;
}

bool FNodeDocsGenerator::GenerateNodeDocTree(UK2Node* Node, FNodeProcessingState& State)
{
	//if (auto EventNode = Cast<UK2Node_Event>(Node))
	//{
	//	return true; // Skip events
	//}
	SCOPE_SECONDS_COUNTER(GenerateNodeDocsTime);

	auto NodeDocsPath = State.ClassDocsPath / TEXT("nodes");

	TSharedPtr<DocTreeNode> NodeDocFile = MakeShared<DocTreeNode>();
	NodeDocFile->AppendChildWithValueEscaped(TEXT("doctype"), TEXT("node"));
	NodeDocFile->AppendChildWithValueEscaped("docs_name", DocsTitle);
	NodeDocFile->AppendChildWithValueEscaped("class_id", State.ClassDocTree->FindChildByName("id")->GetValue());
	NodeDocFile->AppendChildWithValueEscaped("class_name", State.ClassDocTree->FindChildByName("display_name")->GetValue());
	FString NodeShortTitle = FDocGenHelper::GetNodeShortTitle(Node);
	NodeDocFile->AppendChildWithValueEscaped("shorttitle", NodeShortTitle);
	FString NodeFullTitle = FDocGenHelper::GetNodeFullTitle(Node);
	NodeDocFile->AppendChildWithValueEscaped("fulltitle", NodeFullTitle);
	NodeDocFile->AppendChildWithValueEscaped("description", FDocGenHelper::GetNodeDescription(Node));
	NodeDocFile->AppendChildWithValueEscaped("imgpath", State.RelImageBasePath / State.ImageFilename);
	NodeDocFile->AppendChildWithValueEscaped("category", Node->GetMenuCategory().ToString());

	if (auto FuncNode = Cast<UK2Node_CallFunction>(Node))
	{
		auto Func = FuncNode->GetTargetFunction();
		if (Func)
		{
			NodeDocFile->AppendChildWithValueEscaped("funcname", Func->GetAuthoredName());
			NodeDocFile->AppendChildWithValueEscaped("rawcomment", Func->GetMetaData(TEXT("Comment")));
			NodeDocFile->AppendChildWithValue("static", FDocGenHelper::GetBoolString(Func->HasAnyFunctionFlags(FUNC_Static)));
			NodeDocFile->AppendChildWithValue("autocast", FDocGenHelper::GetBoolString(Func->HasMetaData(TEXT("BlueprintAutocast"))));
			TArray<FStringFormatArg> Args;

			if (FProperty* RetProp = Func->GetReturnProperty())
			{
				Args.Add({FDocGenHelper::GetTypeSignature(RetProp)});
			}
			else
			{
				Args.Add({"void"});
			}
			Args.Add({Func->GetAuthoredName()});
			FString FuncParams;
			for (TFieldIterator<FProperty> PropertyIterator(Func); PropertyIterator; ++PropertyIterator)
			{
				UE_LOG(LogKantanDocGen, Display, TEXT("Found property %s in %s"), *PropertyIterator->GetName(), *Func->GetAuthoredName());

				if (!(PropertyIterator->PropertyFlags & CPF_Parm))
					continue;

				// Skip the return type as we handled it earlier
				if (PropertyIterator->HasAllPropertyFlags(CPF_ReturnParm))
				{
					continue;
				}

				FString ParamString = FDocGenHelper::GetTypeSignature(*PropertyIterator) + TEXT(" ") + PropertyIterator->GetAuthoredName();
				if (FuncParams.Len() != 0)
				{
					FuncParams.Append(", ");
				}
				FuncParams.Append(ParamString);
			}
			Args.Add({FuncParams});
			Args.Add({Func->HasAnyFunctionFlags(FUNC_Const) ? " const" : ""});
			NodeDocFile->AppendChildWithValueEscaped("rawsignature", FString::Format(TEXT("{0} {1}({2}){3}"), Args));

			FDocGenHelper::GenerateDoxygenNode(Func, NodeDocFile);
		}
		else
		{
			UE_LOG(LogKantanDocGen, Warning, TEXT("[KantanDocGen] Failed to get target function for node %s "),
				   *NodeFullTitle);
		}
	}
	else
	{
		UE_LOG(LogKantanDocGen, Warning, TEXT("[KantanDocGen] Cannot get type for node %s "), *NodeFullTitle);
	}

	for (auto Pin : Node->Pins)
	{
		TSharedPtr<DocTreeNode> ParamsNode = nullptr;
		if (Pin->Direction == EEdGraphPinDirection::EGPD_Input)
		{
			ParamsNode = FDocGenHelper::GetChildNode(NodeDocFile, TEXT("inputs"), /*bCreate = */true);
		}
		else if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			ParamsNode = FDocGenHelper::GetChildNode(NodeDocFile, TEXT("outputs"), /*bCreate = */true);
		}

		if (ParamsNode.IsValid())
		{
			FDocGenHelper::GenerateParamNode(Pin, ParamsNode);
		}
	}

	FDocGenHelper::SerializeDocToFile(NodeDocFile, NodeDocsPath, NodeShortTitle.Replace(TEXT(" "), TEXT("-")), OutputFormats);

	if (!UpdateClassDocWithNode(State.ClassDocTree, Node))
	{
		return false;
	}

	return true;
}

bool FNodeDocsGenerator::GenerateTypeMembers(UObject* Type)
{
	if (Type)
	{
		UE_LOG(LogKantanDocGen, Display, TEXT("generating type members for : %s"), *Type->GetName());
		if (Type->GetClass() == UClass::StaticClass())
		{
			UClass* ClassInstance = Cast<UClass>(Type);
			TSharedPtr<DocTreeNode> ClassDocTree = GetClassDocTree(ClassInstance);
			bool bIsCreated = false;
			if (!ClassDocTree.IsValid())
			{
				ClassDocTree = InitClassDocTree(ClassInstance);
				bIsCreated = true;
			}
			const bool bIsBlueprintable = (nullptr != ClassInstance->HasMetaDataHierarchical(FBlueprintMetadata::MD_IsBlueprintBase));
			const bool bIsBlueprintType = (nullptr != ClassInstance->HasMetaDataHierarchical(FBlueprintMetadata::MD_AllowableBlueprintVariableType));
			bool bClassShouldBeDocumented = bIsBlueprintable || bIsBlueprintType;
			bClassShouldBeDocumented |= FDocGenHelper::GenerateFieldsNode(ClassInstance, ClassDocTree);

			const bool bHasComment = FDocGenHelper::GenerateDoxygenNode(ClassInstance, ClassDocTree);

			// Only insert this into the map of classdocs if:
			// - it wasnt already in there,
			// - we actually need it to be included
			if (bIsCreated && bClassShouldBeDocumented)
			{
				ClassDocTreeMap.Add(ClassInstance, ClassDocTree);
				UpdateIndexDocWithClass(IndexTree, ClassInstance);
			}

			if (bHasComment == false)
			{
				FDocGenHelper::PrintWarning(FString::Printf(TEXT("No doc for UClass: %s"), *Type->GetName()));
			}
		}
		else if (Type->GetClass() == UScriptStruct::StaticClass())
		{
			UScriptStruct* Struct = Cast<UScriptStruct>(Type);
			if (!Struct->HasAnyFlags(EObjectFlags::RF_ArchetypeObject | EObjectFlags::RF_ClassDefaultObject))
			{
				auto StructDocTree = InitStructDocTree(Struct);
				const bool bHasComment = FDocGenHelper::GenerateDoxygenNode(Struct, StructDocTree);
				if (bHasComment == false)
				{
					FDocGenHelper::PrintWarning(FString::Printf(TEXT("Warning in UScriptStruct: %s"), *Type->GetName()));
				}

				const bool bIsBlueprintable = (nullptr != Struct->HasMetaDataHierarchical(FBlueprintMetadata::MD_IsBlueprintBase));
				const bool bIsBlueprintType = (nullptr != Struct->HasMetaDataHierarchical(FBlueprintMetadata::MD_AllowableBlueprintVariableType));
				bool bShouldBeDocumented = bIsBlueprintable || bIsBlueprintType;
				bShouldBeDocumented |= FDocGenHelper::GenerateFieldsNode(Struct, StructDocTree);

				if (bShouldBeDocumented)
				{
					StructDocTreeMap.Add(Struct, StructDocTree);
					UpdateIndexDocWithStruct(IndexTree, Struct);
				}
			}
		}
		else if (Type->GetClass() == UEnum::StaticClass())
		{
			UEnum* EnumInstance = Cast<UEnum>(Type);
			if ((EnumInstance != nullptr) && EnumInstance->HasAnyFlags(RF_NeedLoad))
			{
				EnumInstance->GetLinker()->Preload(EnumInstance);
			}
			EnumInstance->ConditionalPostLoad();

			auto EnumDocTree = InitEnumDocTree(EnumInstance);

			const bool bHasComment = FDocGenHelper::GenerateDoxygenNode(EnumInstance, EnumDocTree);
			if (bHasComment == false)
			{
				FDocGenHelper::PrintWarning(FString::Printf(TEXT("Warning in UEnum: %s"), *Type->GetName()));
			}

			bool bShouldBeDocumented = EnumInstance->HasMetaData(*FBlueprintMetadata::MD_AllowableBlueprintVariableType.ToString());

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
				UpdateIndexDocWithEnum(IndexTree, EnumInstance);
				EnumDocTreeMap.Add(EnumInstance, EnumDocTree);
			}
		}
	}

	return true;
}

bool FNodeDocsGenerator::SaveIndexFile(FString const& OutDir)
{
	FDocGenHelper::SerializeDocToFile(IndexTree, OutDir, TEXT("index"), OutputFormats);
	return true;
}

bool FNodeDocsGenerator::SaveClassDocFile(FString const& OutDir)
{
	FDocGenHelper::SerializeDocMap(ClassDocTreeMap, OutDir, OutputFormats);
	return true;
}

bool FNodeDocsGenerator::SaveEnumDocFile(FString const& OutDir)
{
	FDocGenHelper::SerializeDocMap(EnumDocTreeMap, OutDir, OutputFormats);
	return true;
}

bool FNodeDocsGenerator::SaveStructDocFile(FString const& OutDir)
{
	FDocGenHelper::SerializeDocMap(StructDocTreeMap, OutDir, OutputFormats);
	return true;
}

void FNodeDocsGenerator::AdjustNodeForSnapshot(UEdGraphNode* Node)
{
	// Hide default value box containing 'self' for Target pin
	if (auto K2_Schema = Cast<UEdGraphSchema_K2>(Node->GetSchema()))
	{
		if (auto TargetPin = Node->FindPin(K2_Schema->PN_Self))
		{
			TargetPin->bDefaultValueIsIgnored = true;
		}
	}
}

#include "BlueprintDelegateNodeSpawner.h"
#include "BlueprintVariableNodeSpawner.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "Engine/UserDefinedStruct.h"

/*
This takes a graph node object and attempts to map it to the class which the node conceptually belong to.
If there is no special mapping for the node, the function determines the class from the source object.
*/
UClass* FNodeDocsGenerator::MapToAssociatedClass(UK2Node* NodeInst, UObject* Source)
{
	// For nodes derived from UK2Node_CallFunction, associate with the class owning the called function.
	if (auto FuncNode = Cast<UK2Node_CallFunction>(NodeInst))
	{
		auto Func = FuncNode->GetTargetFunction();
		if (Func)
		{
			return Func->GetOwnerClass();
		}
	}

	// Default fallback
	if (auto SourceClass = Cast<UClass>(Source))
	{
		return SourceClass;
	}
	else if (auto SourceBP = Cast<UBlueprint>(Source))
	{
		return SourceBP->GeneratedClass;
	}
	else
	{
		return nullptr;
	}
}

bool FNodeDocsGenerator::IsSpawnerDocumentable(UBlueprintNodeSpawner* Spawner, bool bIsBlueprint)
{
	// Spawners of or deriving from the following classes will be excluded
	static const TSubclassOf<UBlueprintNodeSpawner> ExcludedSpawnerClasses[] = {
		UBlueprintVariableNodeSpawner::StaticClass(),
		UBlueprintDelegateNodeSpawner::StaticClass(),
		UBlueprintBoundNodeSpawner::StaticClass(),
		UBlueprintComponentNodeSpawner::StaticClass(),
	};

	// Spawners of or deriving from the following classes will be excluded in a blueprint context
	static const TSubclassOf<UBlueprintNodeSpawner> BlueprintOnlyExcludedSpawnerClasses[] = {
		UBlueprintEventNodeSpawner::StaticClass(),
	};

	// Spawners for nodes of these types (or their subclasses) will be excluded
	static const TSubclassOf<UK2Node> ExcludedNodeClasses[] = {
		UK2Node_DynamicCast::StaticClass(),
		UK2Node_Message::StaticClass(),
		UAnimGraphNode_Base::StaticClass(),
	};

	// Function spawners for functions with any of the following metadata tags will also be excluded
	static const FName ExcludedFunctionMeta[] = {TEXT("BlueprintAutocast")};

	static const uint32 PermittedAccessSpecifiers = (FUNC_Public | FUNC_Protected);

	for (auto ExclSpawnerClass : ExcludedSpawnerClasses)
	{
		if (Spawner->IsA(ExclSpawnerClass))
		{
			return false;
		}
	}

	if (bIsBlueprint)
	{
		for (auto ExclSpawnerClass : BlueprintOnlyExcludedSpawnerClasses)
		{
			if (Spawner->IsA(ExclSpawnerClass))
			{
				return false;
			}
		}
	}

	for (auto ExclNodeClass : ExcludedNodeClasses)
	{
		if (Spawner->NodeClass->IsChildOf(ExclNodeClass))
		{
			return false;
		}
	}

	if (auto FuncSpawner = Cast<UBlueprintFunctionNodeSpawner>(Spawner))
	{
		auto Func = FuncSpawner->GetFunction();

		// @NOTE: We exclude based on access level, but only if this is not a spawner for a blueprint event
		// (custom events do not have any access specifiers)
		if ((Func->FunctionFlags & FUNC_BlueprintEvent) == 0 && (Func->FunctionFlags & PermittedAccessSpecifiers) == 0)
		{
			return false;
		}

		for (auto const& Meta : ExcludedFunctionMeta)
		{
			if (Func->HasMetaData(Meta))
			{
				return false;
			}
		}
	}

	return true;
}
