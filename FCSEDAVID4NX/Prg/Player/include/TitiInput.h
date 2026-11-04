#pragma once
#include<glm/glm.hpp>
namespace Player
{
	struct TitiInputState
	{
		glm::vec3 Movement{ 0.0f };
		bool JumpDown = false;
	};
	class TitiInput final
	{
	public:
		[[nodiscard]] static TitiInputState ReadKeyboard();
	};
}