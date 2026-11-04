#include"GameCamera.h"
#include<Shader.h>
#include<algorithm>
#include<cmath>
#include<glm/ext/matrix_clip_space.hpp>
#include<glm/gtc/matrix_transform.hpp>
namespace
{
	constexpr f32 ViewTransitionDuration = 0.55f;
	constexpr f32 PerspectiveCameraDistance = 3.0f;
}
Game::GameCamera::GameCamera() :
	RotationPitch(0.0f),
	RotationYaw(0.0f),
	TargetRotationPitch(0.0f),
	TargetRotationYaw(0.0f),
	ViewTransition(0.0f),
	CameraDistance(1.0f),
	PendingZoom(0.0f),
	PerspectiveMode(false)
{

}
Game::GameCamera::~GameCamera()
{

}
void Game::GameCamera::Toggle3DView()
{
	PerspectiveMode = !PerspectiveMode;
	TargetRotationYaw = RotationYaw;
	TargetRotationPitch = RotationPitch;
}
void Game::GameCamera::AddZoom(f32 WheelSteps)
{
	PendingZoom += WheelSteps;
}
void Game::GameCamera::Update(f64 DeltaSeconds, f32 YawDirection, f32 PitchDirection)
{
	const f32 Delta = static_cast<f32>(DeltaSeconds);
	if (PerspectiveMode)
	{
		const f32 RotationSpeed = 1.5f * Delta;
		TargetRotationYaw += YawDirection * RotationSpeed;
		TargetRotationPitch += PitchDirection * RotationSpeed;
		TargetRotationPitch = std::clamp(TargetRotationPitch, -1.2f, 1.2f);
	}
	const f32 TransitionStep = Delta / ViewTransitionDuration;
	if (PerspectiveMode)
	{
		ViewTransition = std::min(1.0f, ViewTransition + TransitionStep);
	}
	else
	{
		ViewTransition = std::max(0.0f, ViewTransition - TransitionStep);
	}
	const f32 RotationFollow = std::min(1.0f, Delta * 8.0f);
	RotationPitch += (TargetRotationPitch - RotationPitch) * RotationFollow;
	RotationYaw += (TargetRotationYaw - RotationYaw) * RotationFollow;
	if (PendingZoom != 0.0f)
	{
		CameraDistance *= std::pow(1.1f, -PendingZoom);
		CameraDistance = std::clamp(CameraDistance, 1.0f, 100.0f);
		PendingZoom = 0.0f;
	}
}
void Game::GameCamera::Apply(gx::Shader& Shader, const glm::vec3& Target, u32 ViewportWidth, u32 ViewportHeight) const
{
	if (ViewportWidth == 0 || ViewportHeight == 0)
	{
		return;
	}
	const f32 ViewBlend = ViewTransition * ViewTransition * (3.0f - 2.0f * ViewTransition);
	glm::mat4 Mtx(1.0f);
	Mtx = glm::translate(Mtx, glm::vec3(0.0f, 0.0f, PerspectiveCameraDistance * CameraDistance));
	Mtx = glm::rotate(Mtx, RotationYaw, glm::vec3(0.0f, 1.0f, 0.0f));
	Mtx = glm::rotate(Mtx, RotationPitch, glm::vec3(1.0f, 0.0f, 0.0f));
	const f32 CameraZoom = 1.0f / CameraDistance;
	Mtx = glm::scale(Mtx, glm::vec3(CameraZoom));
	Mtx = glm::translate(Mtx, -Target);
	const f32 AspectCorrection = static_cast<f32>(ViewportHeight) / static_cast<f32>(ViewportWidth);
	const f32 OrthographicDepthScale = 1.0f / (PerspectiveCameraDistance * CameraDistance + 100.0f);
	const glm::mat4 OrthographicProjection = glm::scale(glm::mat4(1.0f), glm::vec3(AspectCorrection, -1.0f, OrthographicDepthScale));
	const f32 FieldOfView = glm::mix(70.0f, 37.0f, ViewBlend);
	glm::mat4 PerspectiveProjection = glm::perspectiveLH_ZO(glm::radians(FieldOfView), static_cast<f32>(ViewportWidth) / static_cast<f32>(ViewportHeight), 0.1f, 5000.0f);
	PerspectiveProjection = glm::scale(PerspectiveProjection, glm::vec3(1.0f, -1.0f, 1.0f));
	const glm::mat4 Projection = OrthographicProjection + (PerspectiveProjection - OrthographicProjection) * ViewBlend;
	const glm::mat4 World(1.0f);
	Shader.SetMatrices(Mtx, Projection, World);
}
bool Game::GameCamera::Is3DView() const
{
	return false;
}