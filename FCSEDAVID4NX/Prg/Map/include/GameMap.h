#pragma once
#include<Types.h>
#include<Macros.h>
#include<MemTypes.h>
#include<Buffer.h>
#include<Texture.h>
#include<FocaArchive.h>
#include<Foca3DGfx/FocaMdlDrv.h>
#include<FocaCol/FColArc.h>
#include<glm/glm.hpp>
#include<DynamicArray.h>
#include<DynamicString.h>
#include<MapScript.h>
#ifdef _DEBUG
#include<FocaUtils/FocaCon.h>
#endif
namespace gx 
{ 
	class Display;
	class CommandBuffer; 
	class Shader; 
}
namespace GameUtil
{
	class FileLoader;
}
namespace Foca3DGfx
{
	class FocaMdl;
}
namespace Game
{
	class GameScene;
	struct ModelLoadRequest;
	struct CollisionResource
	{
		u32 Id = 0;
		String Name;
		glm::vec3 ModelCenter{ 0.0f };
		FocaCol::ColArc Archive;
	};
	class MapSystem final : NonCopy<>
	{
	public:
		MapSystem();
		~MapSystem();
		bool Load(const String& DatPath, const String& MapInfoPath, GameUtil::FileLoader& ResourceLoader, gx::Display& Display, gx::Shader& Shader, String* Error = nullptr, FSys::Utils::FocaCon* ScriptConsole = nullptr, u32 MapId = 0, bool* PlayerEnabled = nullptr);
		void Update(f64 DeltaSeconds);
		void Draw(gx::CommandBuffer& CmdBuffer, bool TransparentPass = false);
		void CollectTransparentDraws(gx::CommandBuffer& CmdBuffer, const Foca3DGfx::TransparentDrawCollector& Collector);
		void DrawCollisions(gx::CommandBuffer& CmdBuffer, gx::Shader& Shader);
		void Finalize();
		void RequestMapLoad(u32 MapId);
		[[nodiscard]] bool SetPlayerEnabled(bool Enabled);
		[[nodiscard]] GameScene* GetScene() const;
		[[nodiscard]] const Foca3DGfx::FocaMdl* GetModel(u32 Id) const;
		[[nodiscard]] const FocaCol::ColArc* GetCollisionArchive(u32 Id) const;
		[[nodiscard]] const DynamicArray<CollisionResource>& GetCollisionArchives() const;
	private:
		bool BuildCollisionDebugGeometry(const DynamicArray<ModelLoadRequest>& ModelRequests, gx::Display& Display, gx::Shader& Shader);
		GameUtil::FileLoader* Resources;
		gx::Display* GraphicsDisplay;
		gx::Shader* GraphicsShader;
		String LoadedDatPath;
		String LoadedMapInfoPath;
		String MapInfoArchiveName;
		Foca3DGfx::FocaArchive Archive;
		Foca3DGfx::FocaArchive TextureArchive;
		MapScript Script;
		DynamicArray<CollisionResource> CollisionArchives;
		Mem::UniquePtr<GameScene> Scene;
		gx::Buffer CollisionDebugBuffer;
		gx::Texture CollisionDebugTexture;
		u32 CollisionDebugVertexCount;
		u32 RequestedMapId;
		bool HasRequestedMap;
		FSys::Utils::FocaCon* ScriptConsole;
		bool* PlayerEnabled;
	};
}
