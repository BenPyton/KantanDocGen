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
#include "BlueprintVariableNodeSpawner.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "DocGenHelper.h"
#include "DocFiles/ClassDocFile.h"
#include "DocFiles/StructDocFile.h"
#include "DocFiles/EnumDocFile.h"
#include "DocFiles/IndexDocFile.h"

FNodeDocsGenerator::FNodeDocsGenerator(const TArray<class UDocGenOutputFormatFactoryBase*>& OutputFormats)
	: OutputFormats(OutputFormats)
{
	auto IndexDoc = CreateDocFile<FIndexDocFile>();
	CreateDocFile<FClassDocFile>(IndexDoc);
	CreateDocFile<FStructDocFile>(IndexDoc);
	CreateDocFile<FEnumDocFile>(IndexDoc);
}

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

	//IndexTree = IndexDoc.CreateDocTree(DocsTitle);
	GetDocFile<FIndexDocFile>()->CreateDocTree(DocsTitle);

	//ClassDoc.Clear();
	GetDocFile<FClassDocFile>()->Clear();
	OutputDir = InOutputDir;

	return true;
}

UK2Node* FNodeDocsGenerator::GT_InitializeForSpawner(UBlueprintNodeSpawner* Spawner, UObject* SourceObject,
													 FNodeProcessingState& OutState)
{
	if (!IsSpawnerDocumentable(Spawner, SourceObject->IsA<UBlueprint>()))
	{
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
	if (AssociatedClass == nullptr)
	{
		UE_LOG(LogKantanDocGen, Error, TEXT("Can't found associated class for node %s."), *K2NodeInst->GetName());
		return nullptr;
	}

	// Create the class doc tree if necessary.
	//TSharedPtr<DocTreeNode> ClassDocTree = ClassDoc.GetDocTree(AssociatedClass, /*bCreate = */true);
	TSharedPtr<DocTreeNode> ClassDocTree = GetDocFile<FClassDocFile>()->GetDocTree(AssociatedClass, /*bCreate = */true);

	const FString ClassID = FDocGenHelper::GetDocId(AssociatedClass);

	OutState = FNodeProcessingState();
	OutState.ClassDocsPath = OutputDir / TEXT("Classes") / ClassID;
	OutState.ClassDocTree = ClassDocTree;

	return K2NodeInst;
}

bool FNodeDocsGenerator::GT_Finalize(FString OutputPath)
{
	for (const auto& DocFile : DocFiles)
	{
		if (!DocFile.Value.IsValid() || !DocFile.Value->SaveFile(OutputPath, OutputFormats))
		{
			return false;
		}
	}

	if (!SaveVariableDocFile(OutputPath))
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

	if (!ShouldNodeGenerateImage(Node))
	{
		return true;
	}

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

	State.RelImageBasePath = TEXT("./img");

	FString NodeName = FDocGenHelper::GetDocId(Node);
	FString ImageBasePath = State.ClassDocsPath / FDocGenHelper::GetNodeDirectory(Node) / NodeName / TEXT("img");
	//FDocGenHelper::CreateImgDir(ImageBasePath);

	FString ImgFilename = FString::Printf(TEXT("nd_img_%s.png"), *FDocGenHelper::GetNodeImgName(Node));
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

// @see SaveVariableDocFile for the reason why we don't use FDocFile here.
TSharedPtr<DocTreeNode> FNodeDocsGenerator::GetVariableDocTree(const FString& VariableId, bool& bFound, bool bCreate /* = false*/)
{
	TSharedPtr<DocTreeNode>* FoundDocTree = VariableDocTreeMap.Find(VariableId);
	bFound = (nullptr != FoundDocTree);
	if (FoundDocTree)
		return *FoundDocTree;

	if (!bCreate)
		return nullptr;

	TSharedPtr<DocTreeNode> NewDocTree = MakeShared<DocTreeNode>();
	VariableDocTreeMap.Add(VariableId, NewDocTree);
	return NewDocTree;
}

bool FNodeDocsGenerator::UpdateClassDocWithNode(TSharedPtr<DocTreeNode> DocTree, UEdGraphNode* Node)
{
	auto DocTreeNodesElement = FDocGenHelper::GetChildNode(DocTree, TEXT("nodes"), /*bCreate = */true);
	auto DocTreeNode = DocTreeNodesElement->AppendChild("node");
	DocTreeNode->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Node));
	DocTreeNode->AppendChildWithValueEscaped(TEXT("shorttitle"), FDocGenHelper::GetNodeShortTitle(Node));
	DocTreeNode->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetNodeDescription(Node));
	DocTreeNode->AppendChildWithValueEscaped(TEXT("type"), FDocGenHelper::GetObjectNativeness(Node));
	DocTreeNode->AppendChildWithValueEscaped(TEXT("category"), FDocGenHelper::GetCategory(Node));
	return true;
}

// @see SaveVariableDocFile for the reason why we don't use FDocFile here.
bool FNodeDocsGenerator::UpdateClassDocWithVariable(TSharedPtr<DocTreeNode> DocTree, UK2Node_Variable* Node)
{
	FProperty* Property = Node->GetPropertyForVariable();
	auto DocTreeNodesElement = FDocGenHelper::GetChildNode(DocTree, TEXT("variables"), /*bCreate = */true);
	auto DocTreeNode = DocTreeNodesElement->AppendChild("variable");
	DocTreeNode->AppendChildWithValueEscaped(TEXT("id"), FDocGenHelper::GetDocId(Node));
	DocTreeNode->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Property));
	DocTreeNode->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Property));
	DocTreeNode->AppendChildWithValueEscaped(TEXT("type"), FDocGenHelper::GetObjectNativeness(Node));
	DocTreeNode->AppendChildWithValueEscaped(TEXT("category"), FDocGenHelper::GetCategory(Property));
	DocTreeNode->AppendChildWithValueEscaped(TEXT("variable_type"), FDocGenHelper::GetTypeSignature(Property));
	return true;
}

bool FNodeDocsGenerator::GenerateNodeDocTree(UK2Node* Node, FNodeProcessingState& State)
{
	if (auto EventNode = Cast<UK2Node_Event>(Node))
	{
		return true; // Skip events
	}
	SCOPE_SECONDS_COUNTER(GenerateNodeDocsTime);

	TSharedPtr<DocTreeNode> NodeDocFile = MakeShared<DocTreeNode>();
	NodeDocFile->AppendChildWithValueEscaped(TEXT("doctype"), TEXT("node"));
	NodeDocFile->AppendChildWithValueEscaped("docs_name", DocsTitle);
	NodeDocFile->AppendChildWithValueEscaped("class_id", State.ClassDocTree->FindChildByName("id")->GetValue());
	NodeDocFile->AppendChildWithValueEscaped("class_name", State.ClassDocTree->FindChildByName("display_name")->GetValue());
	NodeDocFile->AppendChildWithValueEscaped("shorttitle", FDocGenHelper::GetNodeShortTitle(Node));
	FString NodeFullTitle = FDocGenHelper::GetNodeFullTitle(Node);
	NodeDocFile->AppendChildWithValueEscaped("fulltitle", NodeFullTitle);
	NodeDocFile->AppendChildWithValueEscaped("description", FDocGenHelper::GetNodeDescription(Node));
	NodeDocFile->AppendChildWithValueEscaped("imgpath", State.RelImageBasePath / State.ImageFilename);
	NodeDocFile->AppendChildWithValueEscaped("category", FDocGenHelper::GetCategory(Node));

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

	const FString NodeDocID = FDocGenHelper::GetDocId(Node);
	const FString NodeDocsPath = State.ClassDocsPath / TEXT("Nodes") / NodeDocID;
	FDocGenHelper::SerializeDocToFile(NodeDocFile, NodeDocsPath, NodeDocID, OutputFormats);

	if (!UpdateClassDocWithNode(State.ClassDocTree, Node))
	{
		return false;
	}

	return true;
}

bool FNodeDocsGenerator::GenerateVariableDocTree(UK2Node_Variable* Node, FNodeProcessingState& State)
{
	SCOPE_SECONDS_COUNTER(GenerateNodeDocsTime);

	FString VariableId = FDocGenHelper::GetDocId(Node);
	FString ClassId = State.ClassDocTree->FindChildByName("id")->GetValue();

	bool VariableExists = false;
	TSharedPtr<DocTreeNode> VarDocFile = GetVariableDocTree(ClassId / VariableId, VariableExists, /*bCreate = */true);
	// @TODO: Use FDocFile to align with the class/struct/enum/index files (@see SaveVariableDocFile function)
	//TSharedPtr<DocTreeNode> VarDocTree = GetDocFile<FVariableDocFile>()->GetDocTree(Node, /*bCreate = */true);

	if (!State.ImageFilename.IsEmpty())
	{
		if (Node->IsA<UK2Node_VariableGet>())
			VarDocFile->AppendChildWithValueEscaped("imgpath_get", State.RelImageBasePath / State.ImageFilename);
		else if (Node->IsA<UK2Node_VariableSet>())
			VarDocFile->AppendChildWithValueEscaped("imgpath_set", State.RelImageBasePath / State.ImageFilename);
	}

	// @TODO: Once the FDocFile is used, the init below should be done during the GetVariableDocTree above when creating the file.
	// So this condition should be removed too.
	if (VariableExists)
	{
		//UE_LOG(LogKantanDocGen, Error, TEXT("Already existing variable %s, node: %s"), *(ClassId / VariableId), *NodeName);
		return true;
	}

	//UE_LOG(LogKantanDocGen, Error, TEXT("New variable %s, node: %s"), *(ClassId / VariableId), *NodeName);
	// @TODO: Once the FDocFile is used, this should be moved in InitDocTree function.
	FProperty* Property = Node->GetPropertyForVariable();
	VarDocFile->AppendChildWithValueEscaped(TEXT("doctype"), TEXT("variable"));
	VarDocFile->AppendChildWithValueEscaped(TEXT("docs_name"), DocsTitle);
	VarDocFile->AppendChildWithValueEscaped(TEXT("class_id"), State.ClassDocTree->FindChildByName("id")->GetValue());
	VarDocFile->AppendChildWithValueEscaped(TEXT("class_name"), State.ClassDocTree->FindChildByName("display_name")->GetValue());
	VarDocFile->AppendChildWithValueEscaped(TEXT("id"), Property->GetAuthoredName());
	VarDocFile->AppendChildWithValueEscaped(TEXT("display_name"), FDocGenHelper::GetDisplayName(Property));
	VarDocFile->AppendChildWithValueEscaped(TEXT("description"), FDocGenHelper::GetDescription(Property));
	VarDocFile->AppendChildWithValueEscaped(TEXT("category"), FDocGenHelper::GetCategory(Property));

	UEdGraphPin* ValuePin = Node->GetValuePin();
	VarDocFile->AppendChildWithValueEscaped(TEXT("variable_type"), UEdGraphSchema_K2::TypeToText(ValuePin->PinType).ToString());
	
	bool bBlueprintRead, bBlueprintWrite;
	FString BlueprintAccess = FDocGenHelper::GetPropertyBlueprintAccess(Property, bBlueprintRead, bBlueprintWrite);
	if (!BlueprintAccess.IsEmpty())
		VarDocFile->AppendChildWithValueEscaped(TEXT("blueprint_access"), BlueprintAccess);

	bool bEditable, bEditableInTemplate, bEditableInInstance;
	FString EditorAccess = FDocGenHelper::GetPropertyEditorAccess(Property, bEditable, bEditableInTemplate, bEditableInInstance);
	if (!EditorAccess.IsEmpty())
		VarDocFile->AppendChildWithValueEscaped(TEXT("editor_access"), EditorAccess);

	if (!UpdateClassDocWithVariable(State.ClassDocTree, Node))
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
		for (const auto& DocFile : DocFiles)
		{
			if (Type->GetClass() != DocFile.Key)
				continue;

			DocFile.Value->GenerateTypeMembers(Type);
			break;
		}
	}

	return true;
}

bool FNodeDocsGenerator::SaveVariableDocFile(FString const& OutDir)
{
	// Don't use SerializeDocMap because it's a different process
	for (const auto& Pair : VariableDocTreeMap)
	{
		const auto Variable = Pair.Value;
		check(Variable.IsValid());

		// The key of the map is a string of the format "ClassId/VariableId"
		// I've made that to be able to save it in the proper class directory.
		// But we could maybe retrieve it from the DocTree node "class_id"?
		// This will maybe allow us to use the UK2Node_Variable instances as key of the map,
		// and thus creating a child of FDocFile to align with the class/struct/enum/index.
		int SplitIndex = 0;
		FString ClassId, VariableId;
		const bool bSplitted = Pair.Key.Split("/", &ClassId, &VariableId);
		check(bSplitted);
		const auto DocPath = OutDir / "Classes" / ClassId / TEXT("Variables") / VariableId;

		FDocGenHelper::SerializeDocToFile(Variable, DocPath, VariableId, OutputFormats);
	}
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
		// UBlueprintVariableNodeSpawner::StaticClass(),
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

bool FNodeDocsGenerator::ShouldNodeGenerateImage(const UEdGraphNode* Node)
{
	if (const auto* VarNode = Cast<UK2Node_Variable>(Node))
	{
		FProperty* Property = VarNode->GetPropertyForVariable();
		const bool bReadOnly = Property->HasAllPropertyFlags(EPropertyFlags::CPF_BlueprintReadOnly);
		const bool bIsSetter = Node->IsA<UK2Node_VariableSet>();
		return !(bReadOnly && bIsSetter);
	}
	return true;
}
