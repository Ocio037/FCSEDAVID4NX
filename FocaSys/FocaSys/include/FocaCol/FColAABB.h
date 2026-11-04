#pragma once
#include"FCollision.h"
#include"FColTypes.h"
namespace FocaCol
{
	class ColAABB final : public CollisionBase
	{
	public:
		ColAABB();
		ColAABB(u32 Id, const glm::vec3& Center, const glm::vec3& Size, ColKind Kind = ColKind::Solid);
		ColKind GetKind() const;
		glm::vec3 GetMinimum() const;
		glm::vec3 GetMaximum() const;
		bool IsValid() const;
	private:
		ColKind Kind;
	};
}
