#pragma once
#include<Types.h>
#include<Macros.h>
#include<Foca3DGfx.h>
#include<glm/glm.hpp>
#include<TitiInput.h>
namespace Game
{
	class MapSystem;
}
namespace Player
{
	class TitiController final : NonCopy<>
	{
	public:
		TitiController();
		~TitiController();
		void Initialize(const glm::vec3& ModelHalfSize);
		void Update(f64 DeltaSeconds, const TitiInputState& Input, const Game::MapSystem* Map, Foca3DGfx::FocaMdlInstance& ModelInstance, Foca3DGfx::FocaMdlDrv& ModelDriver);
		void Finalize();
		[[nodiscard]] const glm::vec3& GetPosition() const;
	private:
		glm::vec3 Position;
		glm::vec3 Velocity;
		glm::vec3 CollisionHalfSize;
		f32 FacingScale;
		bool Grounded;
		bool JumpWasDown;
		bool Running;
	};
}
