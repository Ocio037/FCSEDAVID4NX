#pragma once
#include<Types.h>
#include<Macros.h>
#include<MemTypes.h>
#include<Foca3DGfx.h>
#include<glm/glm.hpp>
#include<TitiController.h>
namespace GameUtil
{
	class FileLoader;
}
namespace gx
{
	class Display;
	class Shader;
	class CommandBuffer;
}
namespace Game
{
	class MapSystem;
}
namespace Player
{
	class Titi final : NonCopy<>
	{
	public:
		Titi();
		~Titi();
		bool Init(GameUtil::FileLoader& Resources, gx::Display& Display, gx::Shader& Shader);
		void Update(f64 DeltaSeconds, const Game::MapSystem* Map = nullptr);
		void Draw(gx::CommandBuffer& CmdBuffer, bool TransparentPass = false);
		void CollectTransparentDraws(gx::CommandBuffer& CmdBuffer, const Foca3DGfx::TransparentDrawCollector& Collector);
		const glm::vec3& GetPosition() const;
		void Finalize();
	private:
		Mem::UniquePtr<Foca3DGfx::FocaMdl> Model;
		Foca3DGfx::FocaMdlInstance ModelInstance;
		Foca3DGfx::FocaMdlDrv ModelDriver;
		TitiController Controller;
	};
}
