#pragma once
#include<Types.h>
#include<DynamicArray.h>
#include<DynamicString.h>
namespace ohana
{
	struct FestStartEventEntry
	{
		String Value;
		u32 Int1 = 0;
		u32 Int2 = 0;
	};
	struct FestFile
	{
		DynamicArray<FestStartEventEntry> StartEvent;
		DynamicArray<u8> Bytecode;
	};
	[[nodiscard]] bool parse_fest(const void* data, uptr size, FestFile& fest);
}
