#include"GameWorld.h"
#include"GameMap.h"
#include"titi.h"
#include<FileLoader.h>
#include<Display.h>
#include<CommandBuffer.h>
#include<Shader.h>
#include<DynamicArray.h>
#include<algorithm>
Game::GameWorld::GameWorld() : PlayerEnabled(true), ShowCollisionVolumes(false)
{

}
Game::GameWorld::~GameWorld()
{
	Finalize();
}
bool Game::GameWorld::Init(GameUtil::FileLoader& Resources, gx::Display& Display, gx::Shader& Shader, const String& MapPath, const String& MapInfoPath, String* Error, FSys::Utils::FocaCon* ScriptConsole)
{
	Finalize();
	PlayerEnabled = true;
	PlayerActor = Mem::MakeUnique<::Player::Titi>();
	if (!PlayerActor || !PlayerActor->Init(Resources, Display, Shader))
	{
		if (Error) *Error = "No se pudo cargar el modelo o la textura del jugador (IDs 2 y 3 en data.dat).";
		Finalize();
		return false;
	}
	Map = Mem::MakeUnique<MapSystem>();
	if (!Map || !Map->Load(MapPath, MapInfoPath, Resources, Display, Shader, Error, ScriptConsole, 0, &PlayerEnabled))
	{
		Finalize();
		return false;
	}
	return true;
}
void Game::GameWorld::Update(f64 DeltaSeconds)
{
	if (Map)
	{
		Map->Update(DeltaSeconds);
	}
	if (PlayerActor && PlayerEnabled)
	{
		PlayerActor->Update(DeltaSeconds, Map.get());
	}
}
void Game::GameWorld::Draw(gx::CommandBuffer& CmdBuffer, gx::Shader& Shader)
{
	if (Map)
	{
		Map->Draw(CmdBuffer, false);
	}
	if (PlayerActor && PlayerEnabled)
	{
		PlayerActor->Draw(CmdBuffer, false);
	}
	DynamicArray<Foca3DGfx::TransparentDraw> transparentDraws;
	const Foca3DGfx::TransparentDrawCollector collectDraw(&transparentDraws, [](void* Context, const Foca3DGfx::TransparentDraw& Draw)
	{
		static_cast<DynamicArray<Foca3DGfx::TransparentDraw>*>(Context)->push_back(Draw);
	});
	if (Map)
	{
		Map->CollectTransparentDraws(CmdBuffer, collectDraw);
	}
	if (PlayerActor && PlayerEnabled)
	{
		PlayerActor->CollectTransparentDraws(CmdBuffer, collectDraw);
	}
	std::stable_sort(transparentDraws.begin(), transparentDraws.end(), [](const Foca3DGfx::TransparentDraw& left, const Foca3DGfx::TransparentDraw& right)
	{
		return left.Depth > right.Depth;
	});
	for (const Foca3DGfx::TransparentDraw& draw : transparentDraws)
	{
		draw.Execute(CmdBuffer);
	}
	if (ShowCollisionVolumes && Map)
	{
		Map->DrawCollisions(CmdBuffer, Shader);
	}
}
void Game::GameWorld::Finalize()
{
	PlayerActor.reset();
	Map.reset();
	ShowCollisionVolumes = false;
	PlayerEnabled = true;
}
void Game::GameWorld::ToggleCollisionVisibility()
{
	ShowCollisionVolumes = !ShowCollisionVolumes;
}
void Game::GameWorld::RequestMapLoad(u32 MapId)
{
	if (Map)
	{
		Map->RequestMapLoad(MapId);
	}
}
glm::vec3 Game::GameWorld::GetPlayerPosition() const
{
	return PlayerActor ? PlayerActor->GetPosition() : glm::vec3(0.0f);
}