// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#include "ContentPathEnumerator.h"
#include "KantanDocGenLog.h"
#if UE_VERSION_OLDER_THAN(5, 3, 0)
	#include "AssetRegistryModule.h"
	#include "ARFilter.h"
#else
	#include "AssetRegistry/AssetRegistryModule.h"
	#include "AssetRegistry/ARFilter.h"
#endif
#include "Engine/Blueprint.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "Animation/AnimBlueprint.h"


#if UE_VERSION_OLDER_THAN(5, 3, 0)
#define GET_ASSET_PATH(ASSET) ASSET.ObjectPath.ToString()
#define GET_CLASS_PATH(TYPE) TYPE::StaticClass()->GetFName()
#define AddClass(TYPE) ClassNames.Add(GET_CLASS_PATH(TYPE))
#define ExcludeClass(TYPE) RecursiveClassesExclusionSet.Add(GET_CLASS_PATH(TYPE))
#else
#define GET_ASSET_PATH(ASSET) ASSET.GetSoftObjectPath().ToString()
#define GET_CLASS_PATH(TYPE) TYPE::StaticClass()->GetClassPathName()
#define AddClass(TYPE) ClassPaths.Add(GET_CLASS_PATH(TYPE))
#define ExcludeClass(TYPE) RecursiveClassPathsExclusionSet.Add(GET_CLASS_PATH(TYPE))
#endif

FContentPathEnumerator::FContentPathEnumerator(
	FName const& InPath
)
{
	CurIndex = 0;

	Prepass(InPath);
}

void FContentPathEnumerator::Prepass(FName const& Path)
{
	auto& AssetRegistryModule = FModuleManager::GetModuleChecked< FAssetRegistryModule >("AssetRegistry");
	auto& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.bRecursiveClasses = true;
	Filter.AddClass(UBlueprint);
	Filter.AddClass(UUserDefinedEnum);
	Filter.AddClass(UUserDefinedStruct);

	// @TODO: Not sure about this, but for some reason was generating docs for 'AnimInstance' itself.
	Filter.ExcludeClass(UAnimBlueprint);

	AssetRegistry.GetAssetsByPath(Path, AssetList, true);
	AssetRegistry.RunAssetsThroughFilter(AssetList, Filter);
}

UObject* FContentPathEnumerator::GetNext()
{
	UObject* Result = nullptr;

	while(CurIndex < AssetList.Num())
	{
		auto const& AssetData = AssetList[CurIndex];
		++CurIndex;
		UE_LOG(LogKantanDocGen, Log, TEXT("[CONTENT] Enumerating object at '%s'"), *AssetData.GetSoftObjectPath().ToString());

		if (nullptr != (Result = Cast<UBlueprint>(AssetData.GetAsset())));
		else if (nullptr != (Result = Cast<UUserDefinedStruct>(AssetData.GetAsset())));
		else if (nullptr != (Result = Cast<UUserDefinedEnum>(AssetData.GetAsset())));
		else Result = nullptr;

		if(Result)
		{
			UE_LOG(LogKantanDocGen, Log, TEXT("[CONTENT] Enumerating object '%s' at '%s'"), *Result->GetName(), *GET_ASSET_PATH(AssetData));
			break;
		}
	}
	
	return Result;
}

float FContentPathEnumerator::EstimateProgress() const
{
	return (float)CurIndex / (AssetList.Num() - 1);
}

int32 FContentPathEnumerator::EstimatedSize() const
{
	return AssetList.Num();
}

