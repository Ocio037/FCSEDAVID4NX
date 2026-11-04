#pragma once
#include<Types.h>
#include<Macros.h>
#include<MemTypes.h>
#include<FocaArchive.h>
#include<Foca3DGfx.h>
#include<DynamicArray.h>
#include<DynamicString.h>
namespace gx 
{ 
	class Display; 
	class CommandBuffer; 
	class Shader; 
}
namespace Game
{
	struct ModelLoadRequest
	{
		u32 Id = 0;
		String Name;
	};
	struct SceneModel
	{
		u32 Id = 0;
		Mem::UniquePtr<Foca3DGfx::FocaMdl> Model;
		Mem::UniquePtr<Foca3DGfx::FocaMdlInstance> Instance;
	};
	class GameScene final : NonCopy<>
	{
	public:
		GameScene();
		~GameScene();
		bool Init(const Foca3DGfx::FocaArchive& ModelArchive, const Foca3DGfx::FocaArchive& TextureArchive, const DynamicArray<ModelLoadRequest>& ModelRequests, gx::Display& Display, gx::Shader& Shader);
		void Update(f64 DeltaSeconds);
		void Draw(gx::CommandBuffer& CmdBuffer, bool TransparentPass = false);
		void CollectTransparentDraws(gx::CommandBuffer& CmdBuffer, const Foca3DGfx::TransparentDrawCollector& Collector);
		void Finalize();
		[[nodiscard]] const Foca3DGfx::FocaMdl* GetModel(u32 Id) const;
	private:
		DynamicArray<SceneModel> Models;
		Foca3DGfx::FocaMdlDrv MdlDrv;
	};
}
