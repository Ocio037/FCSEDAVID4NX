#pragma once
#include<Types.h>
#include<DynamicString.h>
#include<DynamicArray.h>
namespace ohana
{
	namespace util 
	{
		bool ReadU32(const u8* Data, uptr Size, uptr& Offset, u32& Value);
		bool ReadBytecodeStringPool(const void* Data, uptr Size, DynamicArray<String>& Strings);
	}
}
