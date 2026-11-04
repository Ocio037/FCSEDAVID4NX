#pragma once
#include"FColTypes.h"
#include<glm/glm.hpp>
namespace FocaCol
{
	class CollisionBase
	{
	public:
		~CollisionBase();
		u32 GetId() const;
		const glm::vec3& GetCenter() const;
		const glm::vec3& GetSize() const;
	protected:
		CollisionBase();
		CollisionBase(u32 Id, const glm::vec3& Center, const glm::vec3& Size);
		u32 Id;
		glm::vec3 Center;
		glm::vec3 Size;
	};
}