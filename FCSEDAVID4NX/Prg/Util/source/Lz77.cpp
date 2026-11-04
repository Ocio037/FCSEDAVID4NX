#include"Lz77.h"
#include<cstring>
static constexpr s32 MinMatch = 3;
bool GameUtil::ReverseLz77Decompress(const void* Data, u64 size, DynamicArray<u8>& Out)
{
	if (Data == nullptr || size < 8)
	{
		return false;
	}
	const u8* data = static_cast<const u8*>(Data);
	u64 p = size - 8;
	s32 compressedSize = data[p] | (data[p + 1] << 8) | (data[p + 2] << 16);
	s32 tailSize = data[p + 3];
	s32 expansionSize = data[p + 4] | (data[p + 5] << 8) | (data[p + 6] << 16);
	s32 fileSize = static_cast<s32>(size);
	s32 compressedStart = fileSize - compressedSize;
	s32 lzEnd = fileSize - tailSize;
	if (compressedStart < 0 || lzEnd < 0 || compressedStart > fileSize)
	{
		return false;
	}
	s32 totalUncompressed = compressedSize + expansionSize;
	if (totalUncompressed <= 0)
	{
		return false;
	}
	DynamicArray<u8> compressed(compressedSize);
	std::memcpy(compressed.data(), data + compressedStart, compressedSize);
	DynamicArray<u8> output(totalUncompressed, 0);
	s32 src = lzEnd - compressedStart;
	s32 dst = totalUncompressed;
	s32 start = 0;
	while (src > start)
	{
		src--;
		u8 flags = compressed[src];
		for (s32 i = 0; i < 8; i++)
		{
			if ((flags & 0x80) != 0)
			{
				if (src - 2 < start)
				{
					return false;
				}
				u8 a = compressed[src - 1];
				src -= 2;
				u8 b = compressed[src];
				s32 length = (a >> 4) + MinMatch;
				s32 offset = (((a & 0x0F) << 8) | b) + MinMatch;
				s32 oldDst = dst;
				s32 sourceBase = oldDst + offset;
				for (s32 k = 0; k < length; k++)
				{
					s32 dstPos = oldDst - 1 - k;
					s32 srcPos = sourceBase - 1 - k;
					if (dstPos < start || srcPos < dstPos || srcPos >= totalUncompressed)
					{
						return false;
					}
					output[dstPos] = output[srcPos];
				}
				dst -= length;
			}
			else
			{
				if (src <= start) break;
				src--;
				dst--;
				if (dst < start)
				{
					return false;
				}
				output[dst] = compressed[src];
			}
			flags = static_cast<u8>((flags << 1) & 0xFF);
			if (src <= start) break;
		}
	}
	if (src != start || dst != start)
	{
		return false;
	}
	Out = std::move(output);
	return true;
}