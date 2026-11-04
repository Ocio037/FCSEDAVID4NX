#pragma once
#include"FCollision.h"
#include"FColTypes.h"
namespace FocaCol
{
	class ColOBB final : public CollisionBase
	{
	public:
		ColOBB();
		ColOBB(u32 Id, const glm::vec3& Center, const glm::vec3& Size, const glm::vec3& RotationDegrees, ColKind Kind = ColKind::Solid);
		ColKind GetKind() const;
		const glm::vec3& GetRotationDegrees() const;
		bool IsValid() const;
	private:
		ColKind Kind;
		glm::vec3 RotationDegrees;
	};
}