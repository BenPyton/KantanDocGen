#include "DocFiles/DocFile.h"

const FString& FDocFile::GetDocTitle() const
{
	auto Parent = ParentFile.Pin();
	if (Parent.IsValid())
		return Parent->GetDocTitle();
	return FString();
}
