#define NOMINMAX
#include"TitiController.h"
#include"GameMap.h"
#include<algorithm>
#include<cmath>
#include<DynamicArray.h>
#include<limits>
#include<type_traits>
#include<variant>
#include<glm/gtc/matrix_transform.hpp>

namespace
{
	static constexpr f32 Gravity = 9.8f;
	static constexpr f32 MovementSpeed = 3.0f;
	static constexpr f32 JumpSpeed = 7.0f;
	static constexpr u32 CollisionSolveIterations = 4;
	struct PhysicsBox
	{
		glm::vec3 Center{ 0.0f };
		glm::vec3 HalfSize{ 0.0f };
		glm::vec3 Axes[3]
		{
			glm::vec3(1.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f),
			glm::vec3(0.0f, 0.0f, 1.0f)
		};
	};
	glm::vec3 ToRenderPosition(const glm::vec3& Position, const glm::vec3& ModelCenter)
	{
		return glm::vec3(Position.y - ModelCenter.y, Position.z - ModelCenter.z, 0.5f - (Position.x - ModelCenter.x));
	}
	glm::vec3 ToRenderDirection(const glm::vec3& Direction)
	{
		return glm::vec3(Direction.y, Direction.z, -Direction.x);
	}
	template<typename BoxType>
	void AppendPhysicsBox(const BoxType& Collision, const glm::vec3& ModelCenter, DynamicArray<PhysicsBox>& Boxes)
	{
		if (Collision.GetKind() != FocaCol::ColKind::Solid)
		{
			return;
		}
		glm::vec3 rotationDegrees(0.0f);
		if constexpr (std::is_same_v<BoxType, FocaCol::ColOBB>)
		{
			rotationDegrees = Collision.GetRotationDegrees();
		}
		glm::mat4 rotation(1.0f);
		rotation = glm::rotate(rotation, glm::radians(rotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
		rotation = glm::rotate(rotation, glm::radians(rotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
		rotation = glm::rotate(rotation, glm::radians(rotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
		const glm::mat3 rotation3(rotation);
		PhysicsBox box;
		box.Center = ToRenderPosition(Collision.GetCenter(), ModelCenter);
		box.HalfSize = Collision.GetSize() * 0.5f;
		for (u32 axis = 0; axis < 3; ++axis)
		{
			box.Axes[axis] = glm::normalize(ToRenderDirection(rotation3[axis]));
		}
		Boxes.push_back(box);
	}
	DynamicArray<PhysicsBox> GetSolidCollisionBoxes(const Game::MapSystem* Map)
	{
		DynamicArray<PhysicsBox> boxes;
		if (Map == nullptr)
		{
			return boxes;
		}
		for (const Game::CollisionResource& resource : Map->GetCollisionArchives())
		{
			for (const FocaCol::ColArc::Collision& collision : resource.Archive.GetCollisions())
			{
				std::visit([&](const auto& box)
				{
					AppendPhysicsBox(box, resource.ModelCenter, boxes);
				}, collision);
			}
		}
		return boxes;
	}
	bool ResolvePlayerAgainstBox(glm::vec3& PlayerCenter, const glm::vec3& PlayerHalfSize,
		glm::vec3& Velocity, const PhysicsBox& Box, bool& Grounded)
	{
		static const glm::vec3 worldAxes[3] =
		{
			glm::vec3(1.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f),
			glm::vec3(0.0f, 0.0f, 1.0f)
		};
		const glm::vec3 delta = PlayerCenter - Box.Center;
		f32 minimumPenetration = std::numeric_limits<f32>::max();
		glm::vec3 collisionNormal(0.0f);
		auto testAxis = [&](const glm::vec3& candidateAxis)
		{
			const f32 axisLengthSquared = glm::dot(candidateAxis, candidateAxis);
			if (axisLengthSquared < 1.0e-8f)
			{
				return true;
			}
			const glm::vec3 axis = candidateAxis / std::sqrt(axisLengthSquared);
			const f32 playerRadius = PlayerHalfSize.x * std::abs(axis.x) + PlayerHalfSize.y * std::abs(axis.y) + PlayerHalfSize.z * std::abs(axis.z);
			f32 boxRadius = 0.0f;
			for (u32 index = 0; index < 3; ++index)
			{
				boxRadius += Box.HalfSize[index] * std::abs(glm::dot(axis, Box.Axes[index]));
			}
			const f32 signedDistance = glm::dot(delta, axis);
			const f32 penetration = playerRadius + boxRadius - std::abs(signedDistance);
			if (penetration <= 0.0f)
			{
				return false;
			}
			if (penetration < minimumPenetration)
			{
				minimumPenetration = penetration;
				collisionNormal = signedDistance >= 0.0f ? axis : -axis;
			}
			return true;
		};
		for (const glm::vec3& axis : worldAxes)
		{
			if (!testAxis(axis)) return false;
		}
		for (const glm::vec3& axis : Box.Axes)
		{
			if (!testAxis(axis)) return false;
		}
		for (const glm::vec3& worldAxis : worldAxes)
		{
			for (const glm::vec3& boxAxis : Box.Axes)
			{
				if (!testAxis(glm::cross(worldAxis, boxAxis))) return false;
			}
		}
		if (minimumPenetration == std::numeric_limits<f32>::max())
		{
			return false;
		}
		PlayerCenter += collisionNormal * (minimumPenetration + 1.0e-4f);
		const f32 velocityIntoSurface = glm::dot(Velocity, collisionNormal);
		if (velocityIntoSurface < 0.0f)
		{
			Velocity -= collisionNormal * velocityIntoSurface;
		}
		if (collisionNormal.y > 0.55f && Velocity.y <= 0.0f)
		{
			Grounded = true;
		}
		return true;
	}
}
Player::TitiController::TitiController() : 
	Position({ 0.0f, -1.48f, -2.5f }),
	Velocity({ 0.0f }),
	CollisionHalfSize({ 0.25f }),
	FacingScale(1.0f),
	Grounded(false),
	JumpWasDown(false),
	Running(false)
{

}
Player::TitiController::~TitiController()
{

}
void Player::TitiController::Initialize(const glm::vec3& ModelHalfSize)
{
	Position = glm::vec3(0.0f, -1.48f, -2.5f);
	Velocity = glm::vec3(0.0f);
	CollisionHalfSize = glm::vec3(ModelHalfSize.y, ModelHalfSize.z, ModelHalfSize.x);
	FacingScale = 1.0f;
	Grounded = false;
	JumpWasDown = false;
	Running = false;
}
void Player::TitiController::Update(f64 DeltaSeconds, const TitiInputState& Input, const Game::MapSystem* Map, Foca3DGfx::FocaMdlInstance& ModelInstance, Foca3DGfx::FocaMdlDrv& ModelDriver)
{
	const f32 deltaSeconds = std::clamp(static_cast<f32>(DeltaSeconds), 0.0f, 0.05f);
	glm::vec3 movement = Input.Movement;
	const bool jumpDown = Input.JumpDown;
	const bool movingHorizontally = movement.x != 0.0f;
	if (movement.x < 0.0f)
	{
		FacingScale = -1.0f;
	}
	else if (movement.x > 0.0f)
	{
		FacingScale = 1.0f;
	}
	if (movingHorizontally != Running && ModelDriver.PlayUvAnimation(ModelInstance,
		movingHorizontally ? "02_CORRIDA" : "01_IDLE", true))
	{
		Running = movingHorizontally;
	}
	if (jumpDown && !JumpWasDown && Grounded)
	{
		Velocity.y = JumpSpeed;
		Grounded = false;
	}
	JumpWasDown = jumpDown;
	const f32 movementLength = glm::length(movement);
	if (movementLength > 1.0f)
	{
		movement /= movementLength;
	}
	Velocity.x = movement.x * MovementSpeed;
	Velocity.z = movement.z * MovementSpeed;
	Velocity.y -= Gravity * deltaSeconds;
	Position += Velocity * deltaSeconds;
	const DynamicArray<PhysicsBox> collisionBoxes = GetSolidCollisionBoxes(Map);
	Grounded = false;
	for (u32 iteration = 0; iteration < CollisionSolveIterations; ++iteration)
	{
		bool resolvedAny = false;
		for (const PhysicsBox& box : collisionBoxes)
		{
			resolvedAny = ResolvePlayerAgainstBox(Position, CollisionHalfSize, Velocity, box, Grounded) || resolvedAny;
		}
		if (!resolvedAny)
		{
			break;
		}
	}
	ModelDriver.SetModelOffset(ModelInstance, Position.x, Position.y, Position.z - 0.5f);
	ModelDriver.SetModelScale(ModelInstance, FacingScale, 1.0f, 1.0f);
}
void Player::TitiController::Finalize()
{
	Position = glm::vec3(0.0f, -1.48f, -2.5f);
	Velocity = glm::vec3(0.0f);
	CollisionHalfSize = glm::vec3(0.25f);
	FacingScale = 1.0f;
	Grounded = false;
	JumpWasDown = false;
	Running = false;
}
const glm::vec3& Player::TitiController::GetPosition() const
{
	return Position;
}
