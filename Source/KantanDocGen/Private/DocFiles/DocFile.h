// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2024 Benoit Pelletier. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DocGenHelper.h"
#include "Templates/SharedPointer.h"
#include "OutputFormats/DocGenOutputFormatFactoryBase.h"

class DocTreeNode;

class FDocFile
{
public:
	FDocFile(TWeakPtr<FDocFile> Parent = nullptr)
		: ParentFile(Parent)
	{}
	virtual ~FDocFile() = default;

	virtual bool SaveFile(FString const& OutDir, const TArray<UDocGenOutputFormatFactoryBase*>& OutputFormats) const = 0;
	virtual const FString& GetDocTitle() const;

	virtual TSharedPtr<DocTreeNode> GetDocTree(UObject* Instance) const = 0;

	TSharedPtr<FDocFile> GetParentFile() const { return ParentFile.Pin(); }

	virtual bool GenerateTypeMembers(UObject* Instance) = 0;

private:
	TWeakPtr<FDocFile> ParentFile {nullptr};
};

class FRootDocFile : public FDocFile
{
public:
	using FDocFile::FDocFile;
	//FRootDocFile() : FDocFile(nullptr) {}

	static UClass* GetInstanceType() { return nullptr; }

	TSharedPtr<DocTreeNode> CreateDocTree(FString const& InDocTitle)
	{
		TSharedPtr<DocTreeNode> NewDocTree = MakeShared<DocTreeNode>();
		DocTitle = InDocTitle;
		if (InitDocTree(NewDocTree, DocTitle))
			DocTree = NewDocTree;

		return NewDocTree;
	}

	virtual bool InitDocTree(TSharedPtr<DocTreeNode> DocTree, FString const& DocTitle) const = 0;
	virtual const FString& GetDocTitle() const override { return DocTitle; }
	virtual TSharedPtr<DocTreeNode> GetDocTree(UObject* Instance) const override { return DocTree; }
	virtual bool GenerateTypeMembers(UObject* Instance) override final { return true; }
	
	virtual bool SaveFile(FString const& OutDir, const TArray<UDocGenOutputFormatFactoryBase*>& OutputFormats) const
	{
		FDocGenHelper::SerializeDocToFile(DocTree, OutDir, GetFileName(), OutputFormats);
		return true;
	}

protected:
	virtual FString GetFileName() const = 0;

private:
	TSharedPtr<DocTreeNode> DocTree;
	FString DocTitle;
};

template<class T UE_REQUIRES(TIsDerivedFrom<T, UObject>::IsDerived)>
class FMultiDocFile : public FDocFile
{
public:
	using FDocFile::FDocFile;

	static UClass* GetInstanceType() { return T::StaticClass(); }

	TSharedPtr<DocTreeNode> CreateDocTree(T* Type)
	{
		TSharedPtr<DocTreeNode> NewDocTree = MakeShared<DocTreeNode>();
		InitDocTree(NewDocTree, Type);
		return NewDocTree;
	}

	virtual bool GenerateTypeMembers(UObject* Instance) override final
	{
		T* CastedInstance = Cast<T>(Instance);
		return GenerateTypeMembers(CastedInstance);
	}

	virtual bool InitDocTree(TSharedPtr<DocTreeNode> DocTree, T* Type) const = 0;
	virtual bool UpdateParentDoc(TSharedPtr<DocTreeNode> DocTree, T* Type) const = 0;
	virtual bool GenerateTypeMembers(T* Instance) = 0;

	// If this DocFile is a child of another MultiDocFile, you should provide the parent instance of this instance.
	// For example, you should return a UClass for a UK2Node instance.
	// If the parent DocFile is a RootDocFile, no need to override this function, this is irrelevant.
	virtual UObject* GetParentInstance() const { return nullptr; }

	virtual bool SaveFile(FString const& OutDir, const TArray<UDocGenOutputFormatFactoryBase*>& OutputFormats) const
	{
		FDocGenHelper::SerializeDocMap(DocTreeMap, OutDir / SubDirName(), OutputFormats);
		return true;
	}

	virtual TSharedPtr<DocTreeNode> GetDocTree(UObject* Instance) const override
	{
		T* CastedInstance = Cast<T>(Instance);
		return DocTreeMap[CastedInstance];
	}

	TSharedPtr<DocTreeNode> GetDocTree(T* Instance, bool bCreate = false)
	{
		TSharedPtr<DocTreeNode>* FoundDocTree = DocTreeMap.Find(Instance);
		if (FoundDocTree)
			return *FoundDocTree;

		if (!bCreate)
			return nullptr;

		TSharedPtr<DocTreeNode> NewDocTree = CreateDocTree(Instance);
		AddDocTree(Instance, NewDocTree);
		return NewDocTree;
	}

	void Clear()
	{
		DocTreeMap.Empty();
	}

protected:
	virtual FString SubDirName() const = 0;

	void AddDocTree(T* Instance, TSharedPtr<DocTreeNode> DocTree)
	{
		DocTreeMap.Add(Instance, DocTree);

		TSharedPtr<FDocFile> Parent = GetParentFile();
		if (!Parent.IsValid())
			return;

		UObject* ParentInstance = GetParentInstance();
		TSharedPtr<DocTreeNode> ParentDoc = Parent->GetDocTree(ParentInstance);
		if (!ParentDoc.IsValid())
			return;

		UpdateParentDoc(ParentDoc, Instance);
	}

private:
	TMap<TWeakObjectPtr<T>, TSharedPtr<DocTreeNode>> DocTreeMap;
};
