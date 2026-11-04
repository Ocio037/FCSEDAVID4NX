#include<FestFile.h>
#include<cstring>
#include<utility>
#include"FestUtil.h"
bool ohana::parse_fest(const void* Data, uptr Size, ohana::FestFile& Fest)
{
	Fest.StartEvent.clear();
	Fest.Bytecode.clear();
	constexpr uptr HeaderSize = 16;
	constexpr u32 FestVersion = 2;
	const auto* Bytes = static_cast<const u8*>(Data);
	if (Bytes == nullptr || Size < HeaderSize || std::memcmp(Bytes, "FEST", 4) != 0)
	{
		return false;
	}
	uptr Offset = 4;
	u32 Version = 0;
	u32 StartEventSize = 0;
	u32 BytecodeSize = 0;
	if (!util::ReadU32(Bytes, Size, Offset, Version) || !util::ReadU32(Bytes, Size, Offset, StartEventSize) || !util::ReadU32(Bytes, Size, Offset, BytecodeSize) || Version != FestVersion || StartEventSize < sizeof(u32) ||static_cast<u64>(HeaderSize) + StartEventSize + BytecodeSize != Size)
	{
		return false;
	}
	const uptr StartEventOffset = HeaderSize;
	const uptr BytecodeOffset = StartEventOffset + StartEventSize;
	DynamicArray<String> Strings;
	if (!util::ReadBytecodeStringPool(Bytes + BytecodeOffset, BytecodeSize, Strings))
	{
		return false;
	}
	uptr RowOffset = StartEventOffset;
	u32 RowCount = 0;
	if (!util::ReadU32(Bytes, BytecodeOffset, RowOffset, RowCount) || static_cast<u64>(sizeof(u32)) + static_cast<u64>(RowCount) * 3 * sizeof(u32) != StartEventSize)
	{
		return false;
	}
	Fest.StartEvent.reserve(RowCount);
	for (u32 Index = 0; Index < RowCount; ++Index)
	{
		u32 StringIndex = 0;
		ohana::FestStartEventEntry Entry;
		if (!util::ReadU32(Bytes, BytecodeOffset, RowOffset, StringIndex) || !util::ReadU32(Bytes, BytecodeOffset, RowOffset, Entry.Int1) || !util::ReadU32(Bytes, BytecodeOffset, RowOffset, Entry.Int2) || StringIndex >= Strings.size())
		{
			Fest.StartEvent.clear();
			return false;
		}
		Entry.Value = Strings[StringIndex];
		Fest.StartEvent.push_back(std::move(Entry));
	}
	if (RowOffset != BytecodeOffset)
	{
		Fest.StartEvent.clear();
		return false;
	}
	Fest.Bytecode.reserve(BytecodeSize);
	for (u32 Index = 0; Index < BytecodeSize; ++Index)
	{
		Fest.Bytecode.push_back(Bytes[BytecodeOffset + Index]);
	}
	return true;
}