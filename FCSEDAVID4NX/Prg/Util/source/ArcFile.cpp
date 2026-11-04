#include "ArcFile.h"
#include "Lz77.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace
{
	constexpr u32 NeonHeaderSize = 16;
	constexpr u32 NeonEntrySize = 14;
	constexpr u32 ArcPackerMetadataVersion = 1;

	u16 ReadU16(const u8* Data)
	{
		return static_cast<u16>(Data[0]) | (static_cast<u16>(Data[1]) << 8);
	}

	u32 ReadU32(const u8* Data)
	{
		return static_cast<u32>(Data[0]) |
			(static_cast<u32>(Data[1]) << 8) |
			(static_cast<u32>(Data[2]) << 16) |
			(static_cast<u32>(Data[3]) << 24);
	}

	bool HasReverseLzPayload(const u8* Data, u64 Size)
	{
		for (u64 appendedSize = 0; appendedSize < 0x20; appendedSize += 4)
		{
			if (appendedSize > Size)
			{
				continue;
			}
			const u64 coreSize = Size - appendedSize;
			if (coreSize < 8)
			{
				continue;
			}

			const u32 packed = ReadU32(Data + coreSize - 8);
			const u32 extraSize = ReadU32(Data + coreSize - 4);
			const u32 headerSize = packed >> 24;
			const u32 compressedSize = packed & 0x00FFFFFF;
			if (headerSize < 8 || headerSize > 0x40 || compressedSize < headerSize || compressedSize > coreSize)
			{
				continue;
			}

			bool validPadding = true;
			for (u64 index = coreSize - headerSize; index < coreSize - 8; ++index)
			{
				if (Data[index] != 0xFF)
				{
					validPadding = false;
					break;
				}
			}
			if (validPadding && extraSize != 0)
			{
				return true;
			}
		}
		return false;
	}

	bool IsNeonArchive(const u8* Data, u64 Size)
	{
		if (Data == nullptr || Size < NeonHeaderSize || std::memcmp(Data, "NEON", 4) != 0 || ReadU16(Data + 4) != 0x0104)
		{
			return false;
		}

		const u16 entryCount = ReadU16(Data + 6);
		const u32 metadataSize = ReadU32(Data + 8);
		const u64 tableEnd = static_cast<u64>(NeonHeaderSize) + static_cast<u64>(entryCount) * NeonEntrySize;
		return tableEnd <= metadataSize && metadataSize <= Size;
	}

	bool LoadNeon(const u8* Data, u64 Size, GameUtil::ArcFile& Out)
	{
		if (Data == nullptr || Size < NeonHeaderSize || std::memcmp(Data, "NEON", 4) != 0)
		{
			return false;
		}

		const u16 version = ReadU16(Data + 4);
		const u16 entryCount = ReadU16(Data + 6);
		const u32 metadataSize = ReadU32(Data + 8);
		const u64 tableEnd = static_cast<u64>(NeonHeaderSize) + static_cast<u64>(entryCount) * NeonEntrySize;
		if (version != 0x0104 || tableEnd > metadataSize || metadataSize > Size)
		{
			return false;
		}

		const u64 extensionSize = 12ull + static_cast<u64>(entryCount) * 28ull;
		u64 extensionStart = metadataSize;
		bool hasArcPackerMetadata = false;
		if (extensionSize <= static_cast<u64>(metadataSize) - tableEnd)
		{
			extensionStart = static_cast<u64>(metadataSize) - extensionSize;
			const u8* extension = Data + extensionStart;
			if (std::memcmp(extension, "FPK1", 4) == 0 && ReadU32(extension + 4) == ArcPackerMetadataVersion && ReadU32(extension + 8) == entryCount)
			{
				hasArcPackerMetadata = true;
			}
		}
		if (!hasArcPackerMetadata)
		{
			extensionStart = metadataSize;
		}

		DynamicArray<GameUtil::ArcEntry> entries;
		entries.resize(entryCount);
		for (u32 i = 0; i < entryCount; ++i)
		{
			const u8* tableEntry = Data + NeonHeaderSize + static_cast<u64>(i) * NeonEntrySize;
			const u32 rawOffset = ReadU32(tableEntry);
			const u32 rawSize = ReadU32(tableEntry + 4);
			const u16 nameOffset = ReadU16(tableEntry + 12);
			GameUtil::ArcEntry& entry = entries[i];
			entry.Offset = rawOffset & 0x7FFFFFFF;
			entry.CompressedSize = rawSize & 0x7FFFFFFF;
			if (entry.Offset < metadataSize || static_cast<u64>(entry.Offset) + entry.CompressedSize > Size)
			{
				return false;
			}

			const u64 namePosition = tableEnd + nameOffset;
			if (namePosition < tableEnd || namePosition >= extensionStart)
			{
				return false;
			}
			const u8* name = Data + namePosition;
			const u64 nameLimit = extensionStart - namePosition;
			const void* terminator = std::memchr(name, 0, static_cast<size_t>(nameLimit));
			if (terminator == nullptr)
			{
				return false;
			}
			const auto* nameEnd = static_cast<const u8*>(terminator);
			entry.FileName.assign(reinterpret_cast<const char*>(name), static_cast<size_t>(nameEnd - name));

			bool isCompressed = false;
			if (hasArcPackerMetadata)
			{
				const u8* customEntry = Data + extensionStart + 12 + static_cast<u64>(i) * 28;
				entry.Id = ReadU32(customEntry);
				entry.Arg0 = ReadU32(customEntry + 4);
				entry.Arg1 = ReadU32(customEntry + 8);
				entry.Arg2 = ReadU32(customEntry + 12);
				entry.Arg3 = ReadU32(customEntry + 16);
				entry.OriginalSize = ReadU32(customEntry + 20);
				const u32 compressionFlag = ReadU32(customEntry + 24);
				if (compressionFlag > 1)
				{
					return false;
				}
				isCompressed = compressionFlag != 0;
			}
			else
			{
				entry.Id = i;
				entry.Arg0 = 0;
				entry.Arg1 = 0;
				entry.Arg2 = 0;
				entry.Arg3 = 0;
			}

			const u8* payload = Data + entry.Offset;
			if (hasArcPackerMetadata)
			{
				if (isCompressed)
				{
					if (!GameUtil::ReverseLz77Decompress(payload, entry.CompressedSize, entry.DecompressedData) || entry.DecompressedData.size() != entry.OriginalSize)
					{
						return false;
					}
				}
				else
				{
					if (entry.CompressedSize != entry.OriginalSize)
					{
						return false;
					}
					entry.DecompressedData.assign(payload, payload + entry.CompressedSize);
				}
			}
			else
			{
				if (IsNeonArchive(payload, entry.CompressedSize))
				{
					entry.DecompressedData.assign(payload, payload + entry.CompressedSize);
				}
				else if (HasReverseLzPayload(payload, entry.CompressedSize))
				{
					if (!GameUtil::ReverseLz77Decompress(payload, entry.CompressedSize, entry.DecompressedData))
					{
						return false;
					}
				}
				else
				{
					entry.DecompressedData.assign(payload, payload + entry.CompressedSize);
				}
				entry.OriginalSize = static_cast<u32>(entry.DecompressedData.size());
			}
		}

		Out.FileCount = entryCount;
		Out.Entries = std::move(entries);
		return true;
	}
}

bool GameUtil::ArcLoad(const void* Data, u64 size, ArcFile& Out)
{
	if (Data == nullptr || size < 4)
	{
		return false;
	}
	const u8* data = static_cast<const u8*>(Data);
	if (size >= 4 && std::memcmp(data, "NEON", 4) == 0)
	{
		return LoadNeon(data, size, Out);
	}

	return false;
}
