#include "FestUtil.h"
bool ohana::util::ReadU32(const u8* Data, uptr Size, uptr& Offset, u32& Value)
{
	if (Offset > Size || Size - Offset < sizeof(u32))
	{
		return false;
	}
	Value = static_cast<u32>(Data[Offset]) | (static_cast<u32>(Data[Offset + 1]) << 8) | (static_cast<u32>(Data[Offset + 2]) << 16) | (static_cast<u32>(Data[Offset + 3]) << 24);
	Offset += sizeof(u32);
	return true;
}
bool ohana::util::ReadBytecodeStringPool(const void* Data, uptr Size, DynamicArray<String>& Strings)
{
	Strings.clear();
	constexpr uptr HeaderSize = 1 + 4 * sizeof(u32);
	constexpr u8 BytecodeVersion = 2;
	const auto* Bytes = static_cast<const u8*>(Data);
	if (Bytes == nullptr || Size < HeaderSize || Bytes[0] != BytecodeVersion)
	{
		return false;
	}
	uptr HeaderOffset = 1;
	u32 FunctionCount = 0;
	u32 InstructionCount = 0;
	u32 StringCount = 0;
	u32 StringPoolSize = 0;
	if (!ReadU32(Bytes, Size, HeaderOffset, FunctionCount) || !ReadU32(Bytes, Size, HeaderOffset, InstructionCount) || !ReadU32(Bytes, Size, HeaderOffset, StringCount) || !ReadU32(Bytes, Size, HeaderOffset, StringPoolSize))
	{
		return false;
	}
	(void)FunctionCount;
	(void)InstructionCount;
	if (StringPoolSize > Size - HeaderSize || StringCount > StringPoolSize / sizeof(u32))
	{
		return false;
	}
	uptr Offset = Size - StringPoolSize;
	Strings.reserve(StringCount);
	for (u32 Index = 0; Index < StringCount; ++Index)
	{
		u32 StringSize = 0;
		if (!ReadU32(Bytes, Size, Offset, StringSize) || Offset > Size || StringSize > Size - Offset)
		{
			Strings.clear();
			return false;
		}
		Strings.emplace_back(reinterpret_cast<const char*>(Bytes + Offset), StringSize);
		Offset += StringSize;
	}
	if (Offset != Size)
	{
		Strings.clear();
		return false;
	}
	return true;
}