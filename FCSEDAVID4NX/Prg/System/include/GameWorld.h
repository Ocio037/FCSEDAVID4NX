#pragma once
#include<Types.h>
#include<Macros.h>
#include<MemTypes.h>
#include<glm/glm.hpp>
#include<DynamicString.h>
#ifdef _DEBUG
#include<FocaUtils/FocaCon.h>
#endif
namespace GameUtil
{
	class FileLoader;
}
namespace gx
{
	class Display;
	class CommandBuffer;
	class Shader;
}
namespace Player
{
	class Titi;
}
namespace Game
{
	class MapSystem;
	class GameWorld final : NonCopy<>
	{
	public:
		GameWorld();
		~GameWorld();
		bool Init(GameUtil::FileLoader& Resources, gx::Display& Display, gx::Shader& Shader, const String& MapPath, const String& MapInfoPath, String* Error = nullptr , FSys::Utils::FocaCon* ScriptConsole = nullptr);
		void Update(f64 DeltaSeconds);
		void Draw(gx::CommandBuffer& CmdBuffer, gx::Shader& Shader);
		void Finalize();
		void ToggleCollisionVisibility();
		void RequestMapLoad(u32 MapId);
		[[nodiscard]] glm::vec3 GetPlayerPosition() const;
	private:
		bool PlayerEnabled;
		Mem::UniquePtr<MapSystem> Map;
		Mem::UniquePtr<Player::Titi> PlayerActor;
		bool ShowCollisionVolumes;
	};
}