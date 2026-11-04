#pragma once
#include<Types.h>
#include<DynamicArray.h>
#include<DynamicString.h>
namespace GameUtil
{
	struct ArcEntry
	{
		String FileName;
		u32 Id = 0;
		u32 Offset = 0;
		u32 CompressedSize = 0;
		u32 OriginalSize = 0;
		u32 Arg0 = 0;
		u32 Arg1 = 0;
		u32 Arg2 = 0;
		u32 Arg3 = 0;
		DynamicArray<u8> DecompressedData;
	};
	struct ArcFile
	{
		u32 FileCount = 0;
		DynamicArray<ArcEntry> Entries;
	};
	bool ArcLoad(const void* Data, u64 size, ArcFile& Out);
}
