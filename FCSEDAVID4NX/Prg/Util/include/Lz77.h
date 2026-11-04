#pragma once
#include<Types.h>
#include<DynamicArray.h>
namespace GameUtil
{
	bool ReverseLz77Decompress(const void* Data, u64 size, DynamicArray<u8>& Out);
}
