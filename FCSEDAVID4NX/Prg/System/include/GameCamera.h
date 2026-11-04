#pragma once
#include<Types.h>
#include<Macros.h>
#include<glm/glm.hpp>
namespace gx
{
	class Shader;
}
namespace Game
{
	class GameCamera final : NonCopy<>
	{
	public:
		GameCamera();
		~GameCamera();
		void Toggle3DView();
		void AddZoom(f32 WheelSteps);
		void Update(f64 DeltaSeconds, f32 YawDirection = 0.0f, f32 PitchDirection = 0.0f);
		void Apply(gx::Shader& Shader, const glm::vec3& Target, u32 ViewportWidth, u32 ViewportHeight) const;
		[[nodiscard]] bool Is3DView() const;
	private:
		f32 RotationPitch;
		f32 RotationYaw;
		f32 TargetRotationPitch;
		f32 TargetRotationYaw;
		f32 ViewTransition;
		f32 CameraDistance;
		f32 PendingZoom;
		bool PerspectiveMode;
	};
}