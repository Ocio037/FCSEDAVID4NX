#pragma once
#include "FColAABB.h"
#include "FColOBB.h"
#include <DynamicArray.h>
#include <cstddef>
#include <string_view>
#include <variant>
namespace FocaCol
{
	class ColArc final
	{
	public:
		using Collision = std::variant<ColAABB, ColOBB>;
		ColArc() = default;
		bool Add(const ColAABB& Collision);
		bool Add(const ColOBB& Collision);
		bool AddAABB(u32 Id, const glm::vec3& Center, const glm::vec3& Size, ColKind Kind = ColKind::Solid);
		bool AddOBB(u32 Id, const glm::vec3& Center, const glm::vec3& Size, const glm::vec3& RotationDegrees, ColKind Kind = ColKind::Solid);
		void Clear();
		bool LoadFromMemory(const void* Data, u64 Size);
		bool LoadFromFile(std::string_view Path);
		const DynamicArray<Collision>& GetCollisions() const { return Collisions; }
		std::size_t GetCollisionCount() const { return Collisions.size(); }
		const ColAABB* GetAABB(u32 Id) const;
		const ColOBB* GetOBB(u32 Id) const;
	private:
		bool ContainsId(u32 Id) const;
		DynamicArray<Collision> Collisions;
	};
}