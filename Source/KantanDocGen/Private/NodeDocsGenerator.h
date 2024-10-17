// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class UClass;
class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UK2Node;
class UBlueprintNodeSpawner;
class FXmlFile;

class FNodeDocsGenerator
{
public:
	FNodeDocsGenerator(const TArray<class UDocGenOutputFormatFactoryBase*>& OutputFormats);
	~FNodeDocsGenerator();

public:
	struct FNodeProcessingState
	{
		TSharedPtr<class DocTreeNode> ClassDocTree;
		FString ClassDocsPath;
		FString RelImageBasePath;
		FString ImageFilename;

		FNodeProcessingState():
			ClassDocTree()
			, ClassDocsPath()
			, RelImageBasePath()
			, ImageFilename()
		{}
	};

public:
	/** Callable only from game thread */
	bool GT_Init(FString const& InDocsTitle, FString const& InOutputDir, UClass* BlueprintContextClass = AActor::StaticClass());
	UK2Node* GT_InitializeForSpawner(UBlueprintNodeSpawner* Spawner, UObject* SourceObject, FNodeProcessingState& OutState);
	bool GT_Finalize(FString OutputPath);
	/**/

	/** Callable from background thread */
	bool GenerateNodeImage(UEdGraphNode* Node, FNodeProcessingState& State);
	bool GenerateNodeDocTree(UK2Node* Node, FNodeProcessingState& State);
	bool GenerateVariableDocTree(UK2Node_Variable* Node, FNodeProcessingState& State);
	bool GenerateTypeMembers(UObject* Type);
	/**/

protected:
	void CleanUp();
	bool SaveVariableDocFile(FString const& OutDir);

	// @TODO: Move it in a FDocFile for K2Node class?
	bool UpdateClassDocWithNode(TSharedPtr<DocTreeNode> DocTree, UEdGraphNode* Node);
	bool UpdateClassDocWithVariable(TSharedPtr<DocTreeNode> DocTree, UK2Node_Variable* Node);

	static void AdjustNodeForSnapshot(UEdGraphNode* Node);
	static UClass* MapToAssociatedClass(UK2Node* NodeInst, UObject* Source);
	static bool IsSpawnerDocumentable(UBlueprintNodeSpawner* Spawner, bool bIsBlueprint);
	static bool ShouldNodeGenerateImage(const UEdGraphNode* Node);

	template <typename T UE_REQUIRES(TIsDerivedFrom<T, FDocFile>::IsDerived)>
	TSharedPtr<T> CreateDocFile(TWeakPtr<FDocFile> Parent = nullptr)
	{
		TSharedPtr<T> DocFile = MakeShared<T>(Parent);
		UClass* InstanceType = T::GetInstanceType();
		DocFiles.Add(InstanceType, DocFile);
		return DocFile;
	}

	template <typename T UE_REQUIRES(TIsDerivedFrom<T, FDocFile>::IsDerived)>
	TSharedPtr<T> GetDocFile() const
	{
		UClass* InstanceType = T::GetInstanceType();
		return StaticCastSharedPtr<T>(DocFiles[InstanceType]);
	}

	TSharedPtr<DocTreeNode> GetVariableDocTree(const FString& VariableId, bool& bFound, bool bCreate = false);

protected:
	TWeakObjectPtr< UBlueprint > DummyBP;
	TWeakObjectPtr< UEdGraph > Graph;
	TSharedPtr< class SGraphPanel > GraphPanel;

	FString DocsTitle;
	TSharedPtr<DocTreeNode> IndexTree;
	// @TODO: use an FDocFile instead, but find a way to retrieve class id for saving files
	TMap<FString, TSharedPtr<DocTreeNode>> VariableDocTreeMap;
	TArray<UDocGenOutputFormatFactoryBase*> OutputFormats;
	FString OutputDir;
	bool SaveAllFormats(FString const& OutDir, TSharedPtr<DocTreeNode> Document){ return false; };

private:
	TMap<UClass*, TSharedPtr<FDocFile>> DocFiles;

public:
	//
	double GenerateNodeImageTime = 0.0;
	double GenerateNodeDocsTime = 0.0;
	//
};


