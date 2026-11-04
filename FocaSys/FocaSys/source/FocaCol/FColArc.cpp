#include "FocaCol/FColArc.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <DynamicArray.h>
#include <DynamicString.h>
#include <DynamicUnorderedSet.h>
#include <fstream>
#include <limits>
#include <utility>
namespace
{
	constexpr u32 CurrentVersion = 3;
	constexpr u32 IdVersion = 2;
	constexpr u32 LegacyVersion = 1;
	constexpr u32 MaxCollisionCount = 1'000'000;
	constexpr u32 MaxStringLength = 1'048'576;
	class FcolReader final
	{
	public:
		FcolReader(const void* Data, u64 Size) : Data(static_cast<const u8*>(Data)), Size(Size)
		{

		}
		bool ReadBytes(void* Destination, size_t ByteCount)
		{
			if (ByteCount > Size - Offset)
			{
				return false;
			}
			std::memcpy(Destination, Data + Offset, ByteCount);
			Offset += ByteCount;
			return true;
		}
		bool ReadU32(u32& Value)
		{
			u8 Bytes[4]{};
			if (!ReadBytes(Bytes, sizeof(Bytes)))
			{
				return false;
			}
			Value = static_cast<u32>(Bytes[0]) | (static_cast<u32>(Bytes[1]) << 8) | (static_cast<u32>(Bytes[2]) << 16) | (static_cast<u32>(Bytes[3]) << 24);
			return true;
		}
		bool ReadFloat(f32& Value)
		{
			u32 Bits = 0;
			if (!ReadU32(Bits))
			{
				return false;
			}
			Value = std::bit_cast<f32>(Bits);
			return true;
		}
		bool SkipAlignedString()
		{
			u32 ByteCount = 0;
			if (!ReadU32(ByteCount) || ByteCount > MaxStringLength || ByteCount > Size - Offset)
			{
				return false;
			}
			Offset += ByteCount;
			const u32 Padding = (4 - (ByteCount & 3)) & 3;
			for (u32 i = 0; i < Padding; ++i)
			{
				u8 Value = 0;
				if (!ReadBytes(&Value, sizeof(Value)) || Value != 0)
				{
					return false;
				}
			}
			return true;
		}
		u64 GetOffset() const { return Offset; }
		u64 GetSize() const { return Size; }
	private:
		const u8* Data = nullptr;
		u64 Size = 0;
		u64 Offset = 0;
	};
}
bool FocaCol::ColArc::Add(const ColAABB& Collision)
{
	if (!Collision.IsValid() || ContainsId(Collision.GetId()))
	{
		return false;
	}
	Collisions.emplace_back(Collision);
	return true;
}
bool FocaCol::ColArc::Add(const ColOBB& Collision)
{
	if (!Collision.IsValid() || ContainsId(Collision.GetId()))
	{
		return false;
	}
	Collisions.emplace_back(Collision);
	return true;
}
bool FocaCol::ColArc::AddAABB(u32 Id, const glm::vec3& Center, const glm::vec3& Size, ColKind Kind)
{
	return Add(ColAABB(Id, Center, Size, Kind));
}
bool FocaCol::ColArc::AddOBB(u32 Id, const glm::vec3& Center, const glm::vec3& Size, const glm::vec3& RotationDegrees, ColKind Kind)
{
	return Add(ColOBB(Id, Center, Size, RotationDegrees, Kind));
}
void FocaCol::ColArc::Clear()
{
	Collisions.clear();
}
bool FocaCol::ColArc::LoadFromFile(std::string_view Path)
{
	String FilePath(Path.begin(), Path.end());
	std::ifstream File(FilePath.c_str(), std::ios::binary | std::ios::ate);
	if (!File)
	{
		return false;
	}
	const std::streamoff FileSizeSigned = static_cast<std::streamoff>(File.tellg());
	if (FileSizeSigned < 0)
	{
		return false;
	}
	const auto FileSize = static_cast<u64>(FileSizeSigned);
	if (FileSize > static_cast<u64>((std::numeric_limits<std::streamsize>::max)()) || FileSize > static_cast<u64>((std::numeric_limits<size_t>::max)()))
	{
		return false;
	}
	DynamicArray<u8> Data(static_cast<size_t>(FileSize));
	File.seekg(0, std::ios::beg);
	if (!Data.empty() && !File.read(reinterpret_cast<char*>(Data.data()), static_cast<std::streamsize>(Data.size())))
	{
		return false;
	}
	return LoadFromMemory(Data.data(), FileSize);
}
bool FocaCol::ColArc::LoadFromMemory(const void* Data, u64 Size)
{
	if (Data == nullptr || Size < 12 || (Size & 3) != 0)
	{
		return false;
	}
	FcolReader Reader(Data, Size);
	char Magic[4]{};
	u32 Version = 0;
	u32 CollisionCount = 0;
	if (!Reader.ReadBytes(Magic, sizeof(Magic)) || std::memcmp(Magic, "FCOL", sizeof(Magic)) != 0 || !Reader.ReadU32(Version) || (Version != CurrentVersion && Version != IdVersion && Version != LegacyVersion) || !Reader.ReadU32(CollisionCount) || CollisionCount > MaxCollisionCount)
	{
		return false;
	}
	if (Version == IdVersion || Version == LegacyVersion)
	{
		if (!Reader.SkipAlignedString())
		{
			return false;
		}
	}
	DynamicArray<Collision> LoadedCollisions;
	LoadedCollisions.reserve(CollisionCount);
	DynamicUnorderedSet<u32> UsedIds;
	UsedIds.reserve(CollisionCount);
	for (u32 i = 0; i < CollisionCount; ++i)
	{
		u32 Id = i;
		if (Version != LegacyVersion && !Reader.ReadU32(Id))
		{
			return false;
		}
		if (!UsedIds.emplace(Id).second)
		{
			return false;
		}
		if (Version == LegacyVersion && !Reader.SkipAlignedString())
		{
			return false;
		}
		u32 Shape = 0;
		u32 Kind = 0;
		f32 CenterX = 0.0f;
		f32 CenterY = 0.0f;
		f32 CenterZ = 0.0f;
		f32 SizeX = 0.0f;
		f32 SizeY = 0.0f;
		f32 SizeZ = 0.0f;
		f32 RotationX = 0.0f;
		f32 RotationY = 0.0f;
		f32 RotationZ = 0.0f;
		if (!Reader.ReadU32(Shape) || !Reader.ReadU32(Kind) || Shape > 1 || Kind > static_cast<u32>(ColKind::Trigger) ||
			!Reader.ReadFloat(CenterX) || !Reader.ReadFloat(CenterY) || !Reader.ReadFloat(CenterZ) ||
			!Reader.ReadFloat(SizeX) || !Reader.ReadFloat(SizeY) || !Reader.ReadFloat(SizeZ) ||
			!Reader.ReadFloat(RotationX) || !Reader.ReadFloat(RotationY) || !Reader.ReadFloat(RotationZ) ||
			!std::isfinite(CenterX) || !std::isfinite(CenterY) || !std::isfinite(CenterZ) ||
			!std::isfinite(SizeX) || !std::isfinite(SizeY) || !std::isfinite(SizeZ) ||
			!std::isfinite(RotationX) || !std::isfinite(RotationY) || !std::isfinite(RotationZ))
		{
			return false;
		}
		const glm::vec3 Center(CenterX, CenterY, CenterZ);
		const glm::vec3 BoxSize(SizeX, SizeY, SizeZ);
		const auto CollisionKind = static_cast<ColKind>(Kind);
		if (Shape == 0)
		{
			ColAABB Box(Id, Center, BoxSize, CollisionKind);
			if (!Box.IsValid())
			{
				return false;
			}
			LoadedCollisions.emplace_back(Box);
		}
		else
		{
			ColOBB Box(Id, Center, BoxSize, glm::vec3(RotationX, RotationY, RotationZ), CollisionKind);
			if (!Box.IsValid())
			{
				return false;
			}
			LoadedCollisions.emplace_back(Box);
		}
	}
	if (Reader.GetOffset() != Reader.GetSize())
	{
		return false;
	}
	Collisions = std::move(LoadedCollisions);
	return true;
}
const FocaCol::ColAABB* FocaCol::ColArc::GetAABB(u32 Id) const
{
	for (const Collision& Entry : Collisions)
	{
		if (const auto* Box = std::get_if<ColAABB>(&Entry); Box != nullptr && Box->GetId() == Id)
		{
			return Box;
		}
	}
	return nullptr;
}
const FocaCol::ColOBB* FocaCol::ColArc::GetOBB(u32 Id) const
{
	for (const Collision& Entry : Collisions)
	{
		if (const auto* Box = std::get_if<ColOBB>(&Entry); Box != nullptr && Box->GetId() == Id)
		{
			return Box;
		}
	}
	return nullptr;
}
bool FocaCol::ColArc::ContainsId(u32 Id) const
{
	return std::any_of(Collisions.begin(), Collisions.end(), [Id](const Collision& Entry)
	{
		return std::visit([Id](const auto& Box) { return Box.GetId() == Id; }, Entry);
	});
}