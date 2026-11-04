#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#define NOMINMAX
#include"GameMap.h"
#include"GameScene.h"
#include<FestFile.h>
#include<FileLoader.h>
#include<Foca3DGfx/FocaMdl.h>
#include<algorithm>
#include<array>
#include<charconv>
#include<cctype>
#include<cstddef>
#include<limits>
#include<string_view>
#include<type_traits>
#include<utility>
#include<glm/glm.hpp>
#include<glm/gtx/euler_angles.hpp>
namespace
{
	struct CollisionDebugVertex
	{
		f32 Position[3]{};
		f32 Normal[3]{};
		f32 Color[4]{};
		f32 UV[2]{};
	};
	static_assert(sizeof(CollisionDebugVertex) == 48);
	String NormalizeResourceName(String Name, const char* Extension)
	{
		const std::size_t Separator = Name.find_last_of("/\\");
		if (Separator != String::npos)
		{
			Name.erase(0, Separator + 1);
		}
		for (char& Character : Name)
		{
			Character = static_cast<char>(std::tolower(static_cast<unsigned char>(Character)));
		}
		const std::size_t ExtensionLength = std::char_traits<char>::length(Extension);
		if (Name.size() >= ExtensionLength && Name.compare(Name.size() - ExtensionLength, ExtensionLength, Extension) == 0)
		{
			Name.resize(Name.size() - ExtensionLength);
		}
		return Name;
	}
	glm::vec3 GetModelCenter(const Foca3DGfx::FocaMdl* Model)
	{
		if (Model == nullptr)
		{
			return glm::vec3(0.0f);
		}
		glm::vec3 minimum(std::numeric_limits<f32>::max());
		glm::vec3 maximum(std::numeric_limits<f32>::lowest());
		bool hasPositions = false;
		for (const auto& mesh : Model->GetMeshes())
		{
			for (const auto& vertex : mesh.Vertices)
			{
				const glm::vec3 position(vertex.Position[0], vertex.Position[1], vertex.Position[2]);
				minimum = glm::min(minimum, position);
				maximum = glm::max(maximum, position);
				hasPositions = true;
			}
		}
		return hasPositions ? (minimum + maximum) * 0.5f : glm::vec3(0.0f);
	}
	glm::vec3 ToRenderPosition(const glm::vec3& Position, const glm::vec3& ModelCenter)
	{
		return glm::vec3(Position.y - ModelCenter.y, Position.z - ModelCenter.z, 0.5f - (Position.x - ModelCenter.x));
	}
	void AppendCollisionBox(DynamicArray<CollisionDebugVertex>& Vertices, const glm::vec3& Center, const glm::vec3& Size, const glm::vec3& RotationDegrees, const glm::vec3& ModelCenter)
	{
		const glm::mat4 rotation = glm::yawPitchRoll(glm::radians(RotationDegrees.y), glm::radians(RotationDegrees.x), glm::radians(RotationDegrees.z));
		const glm::mat3 rotation3 = glm::mat3(rotation);
		const glm::vec3 halfSize = Size * 0.5f;
		std::array<glm::vec3, 8> corners{};
		for (u32 index = 0; index < static_cast<u32>(corners.size()); ++index)
		{
			const glm::vec3 local((index & 1) == 0 ? -halfSize.x : halfSize.x, (index & 2) == 0 ? -halfSize.y : halfSize.y, (index & 4) == 0 ? -halfSize.z : halfSize.z);
			corners[index] = ToRenderPosition(Center + rotation3 * local, ModelCenter);
		}
		static constexpr u32 triangles[] =
		{
			0, 2, 1, 1, 2, 3,
			4, 5, 6, 5, 7, 6,
			0, 1, 4, 1, 5, 4,
			2, 6, 3, 3, 6, 7,
			0, 4, 2, 2, 4, 6,
			1, 3, 5, 3, 7, 5
		};
		for (u32 index : triangles)
		{
			CollisionDebugVertex vertex{};
			vertex.Position[0] = corners[index].x;
			vertex.Position[1] = corners[index].y;
			vertex.Position[2] = corners[index].z;
			vertex.Color[0] = 0.0f;
			vertex.Color[1] = 1.0f;
			vertex.Color[2] = 0.0f;
			vertex.Color[3] = 0.35f;
			Vertices.push_back(vertex);
		}
	}
}
Game::MapSystem::MapSystem() : Resources(nullptr),
	CollisionDebugVertexCount(0),
	GraphicsDisplay(nullptr),
	GraphicsShader(nullptr),
	RequestedMapId(0),
	HasRequestedMap(false),
	ScriptConsole(nullptr),
	PlayerEnabled(nullptr)
{

}
Game::MapSystem::~MapSystem()
{
	Finalize();
}
bool Game::MapSystem::Load(const String& DatPath, const String& MapInfoPath, GameUtil::FileLoader& ResourceLoader, gx::Display& Display, gx::Shader& Shader, String* Error, FSys::Utils::FocaCon* ScriptConsoleIn, u32 MapId, bool* PlayerEnabledIn)
{
	Finalize();
	if (PlayerEnabledIn)
	{
		PlayerEnabled = PlayerEnabledIn;
	}
	if (PlayerEnabled)
	{
		*PlayerEnabled = true;
	}
	LoadedDatPath = DatPath;
	LoadedMapInfoPath = MapInfoPath;
	GraphicsDisplay = &Display;
	GraphicsShader = &Shader;
	ScriptConsole = ScriptConsoleIn;
	if (Error)
	{
		Error->clear();
	}
	auto fail = [this, Error](const char* Message)
	{
		if (Error)
		{
			*Error = Message;
		}
		Finalize();
		return false;
	};
	Resources = &ResourceLoader;
	String resourceError;
	if (!Resources->RequestMountArchive("map", DatPath, &resourceError))
	{
		return fail(resourceError.c_str());
	}
	if (!Resources->RequestMountArchive("mapinfo", MapInfoPath, &resourceError))
	{
		return fail(resourceError.c_str());
	}
	const GameUtil::ArcFile* mapArchive = Resources->ReceiveArchive("map", &resourceError);
	if (mapArchive == nullptr)
	{
		return fail(resourceError.c_str());
	}
	const GameUtil::ArcFile* mapInfoArchive = Resources->ReceiveArchive("mapinfo", &resourceError);
	if (mapInfoArchive == nullptr)
	{
		return fail(resourceError.c_str());
	}
	const GameUtil::ArcEntry* modelEntry = Resources->FindResourceById("map", MapId);
	if (modelEntry == nullptr)
	{
		return fail("map.dat no contiene el mapa solicitado.");
	}
	const GameUtil::ArcEntry* mapInfoEntry = Resources->FindResourceById("mapinfo", MapId);
	if (mapInfoEntry == nullptr)
	{
		return fail("mapinfo.dat no contiene informacion para el ID del mapa.");
	}
	char mapInfoId[16]{};
	const auto idConversion = std::to_chars(mapInfoId, mapInfoId + sizeof(mapInfoId), MapId);
	if (idConversion.ec != std::errc{})
	{
		return fail("No se pudo convertir el ID del mapa.");
	}
	MapInfoArchiveName = "mapinfo/";
	MapInfoArchiveName.append(mapInfoId, static_cast<std::size_t>(idConversion.ptr - mapInfoId));
	if (Resources->CreateAndAddArchive(MapInfoArchiveName, mapInfoEntry->DecompressedData.data(),
		mapInfoEntry->DecompressedData.size(), &resourceError) == nullptr)
	{
		return fail(resourceError.c_str());
	}
	const GameUtil::ArcEntry* startEventEntry = Resources->FindResourceById(MapInfoArchiveName, 0);
	if (startEventEntry == nullptr)
	{
		return fail("El archivo de informacion del mapa no contiene el script FEST inicial con ID 0.");
	}
	ohana::FestFile InitialEvent;
	if (!ohana::parse_fest(startEventEntry->DecompressedData.data(), startEventEntry->DecompressedData.size(), InitialEvent))
	{
		return fail("El FEST del evento inicial no es valido o su version no coincide con el motor.");
	}
	DynamicArray<ModelLoadRequest> modelRequests;
	DynamicArray<CollisionResource> collisionArchives;
	u32 initialTrackId = 0;
	bool hasInitialTrackEntry = false;
	for (const ohana::FestStartEventEntry& request : InitialEvent.StartEvent)
	{
		if (request.Int1 == 0)
		{
			if (hasInitialTrackEntry)
			{
				return fail("El evento inicial solo puede seleccionar una pista de arranque.");
			}
			hasInitialTrackEntry = true;
			initialTrackId = request.Int2;
			continue;
		}
		if (request.Value.empty())
		{
			return fail("El evento inicial contiene un nombre de recurso vacio.");
		}
		if (request.Int1 == 1)
		{
			if (std::any_of(modelRequests.begin(), modelRequests.end(), [&request](const ModelLoadRequest& model)
				{ return model.Id == request.Int2; }))
			{
				return fail("El evento inicial asigna el mismo ID a mas de un modelo.");
			}
			modelRequests.push_back(ModelLoadRequest{ request.Int2, request.Value });
		}
		else if (request.Int1 == 2)
		{
			if (std::any_of(collisionArchives.begin(), collisionArchives.end(), [&request](const CollisionResource& collision) { return collision.Id == request.Int2; }))
			{
				return fail("El evento inicial asigna el mismo ID a mas de un archivo de colisiones.");
			}
			const GameUtil::ArcEntry* collisionEntry = Resources->FindResourceByName(MapInfoArchiveName, request.Value, ".fcol");
			if (collisionEntry == nullptr)
			{
				return fail("El evento inicial solicita un archivo .fcol que no esta en el paquete del mapa.");
			}
			CollisionResource collision;
			collision.Id = request.Int2;
			collision.Name = request.Value;
			if (!collision.Archive.LoadFromMemory(collisionEntry->DecompressedData.data(), collisionEntry->DecompressedData.size()))
			{
				return fail("No se pudo leer un archivo .fcol solicitado por el evento inicial.");
			}
			collisionArchives.push_back(std::move(collision));
		}
		else
		{
			return fail("El evento inicial contiene una operacion desconocida; usa 0 para pista, 1 para modelo o 2 para colisiones.");
		}
	}
	if (!Archive.LoadFromMemory(modelEntry->DecompressedData.data(), modelEntry->DecompressedData.size()))
	{
		return fail("El recurso del mapa no es un archivo FOCA valido.");
	}
	const Foca3DGfx::FocaArchive* textureSource = &Archive;
	if (modelEntry->Arg0 == 1)
	{
		const GameUtil::ArcEntry* textureEntry = Resources->FindResourceById("map", 100);
		if (textureEntry == nullptr || !TextureArchive.LoadFromMemory(textureEntry->DecompressedData.data(), textureEntry->DecompressedData.size()) || TextureArchive.GetTextures().empty())
		{
			return fail("El mapa requiere el paquete de texturas ID 100, pero no se pudo cargar.");
		}
		textureSource = &TextureArchive;
	}
	else
	{
		for (const GameUtil::ArcEntry& entry : mapArchive->Entries)
		{
			if (&entry == modelEntry)
			{
				continue;
			}
			if (TextureArchive.LoadFromMemory(entry.DecompressedData.data(), entry.DecompressedData.size()) && !TextureArchive.GetTextures().empty())
			{
				textureSource = &TextureArchive;
				break;
			}
		}
	}
	Scene = Mem::MakeUnique<GameScene>();
	if (!Scene->Init(Archive, *textureSource, modelRequests, Display, Shader))
	{
		return fail("No se pudieron cargar los modelos indicados por el evento inicial. Revisa sus nombres y texturas.");
	}
	CollisionArchives = std::move(collisionArchives);
	if (!BuildCollisionDebugGeometry(modelRequests, Display, Shader))
	{
		return fail("No se pudo preparar la visualizacion de las colisiones.");
	}
	if (!Script.Initialize(InitialEvent.Bytecode, *this, Error, ScriptConsole, initialTrackId, PlayerEnabled))
	{
		Finalize();
		return false;
	}
	return true;
}
void Game::MapSystem::RequestMapLoad(u32 MapId)
{
	RequestedMapId = MapId;
	HasRequestedMap = true;
}
bool Game::MapSystem::SetPlayerEnabled(bool Enabled)
{
	if (PlayerEnabled == nullptr)
	{
		return false;
	}
	*PlayerEnabled = Enabled;
	return true;
}
bool Game::MapSystem::BuildCollisionDebugGeometry(const DynamicArray<ModelLoadRequest>& ModelRequests, gx::Display& Display, gx::Shader& Shader)
{
	CollisionDebugBuffer.Finalize();
	CollisionDebugTexture.Finalize();
	CollisionDebugVertexCount = 0;
	DynamicArray<CollisionDebugVertex> vertices;
	for (CollisionResource& resource : CollisionArchives)
	{
		const String collisionName = NormalizeResourceName(resource.Name, ".fcol");
		const auto modelRequest = std::find_if(ModelRequests.begin(), ModelRequests.end(), [&collisionName](const ModelLoadRequest& request)
		{
			return NormalizeResourceName(request.Name, ".f3d") == collisionName;
		});
		const Foca3DGfx::FocaMdl* model = modelRequest != ModelRequests.end() && Scene ? Scene->GetModel(modelRequest->Id) : nullptr;
		const glm::vec3 modelCenter = GetModelCenter(model);
		resource.ModelCenter = modelCenter;
		for (const FocaCol::ColArc::Collision& collision : resource.Archive.GetCollisions())
		{
			std::visit([&vertices, &modelCenter](const auto& box)
			{
				using BoxType = std::decay_t<decltype(box)>;
				if constexpr (std::is_same_v<BoxType, FocaCol::ColOBB>)
				{
					AppendCollisionBox(vertices, box.GetCenter(), box.GetSize(), box.GetRotationDegrees(), modelCenter);
				}
				else
				{
					AppendCollisionBox(vertices, box.GetCenter(), box.GetSize(), glm::vec3(0.0f), modelCenter);
				}
			}, collision);
		}
	}
	if (vertices.empty())
	{
		return true;
	}
	if (vertices.size() > std::numeric_limits<u32>::max())
	{
		return false;
	}
	static constexpr u8 whitePixel[] = { 255, 255, 255, 255 };
	if (!CollisionDebugTexture.Initialize(Display, Shader, 1, 1, whitePixel))
	{
		return false;
	}
	CollisionDebugBuffer.Initialize(Display);
	if (!CollisionDebugBuffer.CreateVertexBuffer(vertices.data(), static_cast<u64>(vertices.size()) * sizeof(CollisionDebugVertex)))
	{
		CollisionDebugBuffer.Finalize();
		CollisionDebugTexture.Finalize();
		return false;
	}
	CollisionDebugVertexCount = static_cast<u32>(vertices.size());
	return true;
}
void Game::MapSystem::Update(f64 DeltaSeconds)
{
	if (Scene)
	{
		Scene->Update(DeltaSeconds);
	}
	Script.Update();
	if (HasRequestedMap)
	{
		const u32 RequestedId = RequestedMapId;
		HasRequestedMap = false;
		GameUtil::FileLoader* const ResourceLoader = Resources;
		gx::Display* const Display = GraphicsDisplay;
		gx::Shader* const Shader = GraphicsShader;
		const String DatPath = LoadedDatPath;
		const String MapInfoPath = LoadedMapInfoPath;
		FSys::Utils::FocaCon* const Console = ScriptConsole;
		String LoadError;
		if (ResourceLoader == nullptr || Display == nullptr || Shader == nullptr || !Load(DatPath, MapInfoPath, *ResourceLoader, *Display, *Shader, &LoadError, Console, RequestedId, PlayerEnabled))
		{
			if (Console)
			{
				MapScript::ShowLoadError(Console, std::string_view(LoadError.data(), LoadError.size()));
			}
		}
	}
}
void Game::MapSystem::Draw(gx::CommandBuffer& CmdBuffer, bool TransparentPass)
{
	if (Scene)
	{
		Scene->Draw(CmdBuffer, TransparentPass);
	}
}
void Game::MapSystem::CollectTransparentDraws(gx::CommandBuffer& CmdBuffer, const Foca3DGfx::TransparentDrawCollector& Collector)
{
	if (Scene)
	{
		Scene->CollectTransparentDraws(CmdBuffer, Collector);
	}
}
void Game::MapSystem::DrawCollisions(gx::CommandBuffer& CmdBuffer, gx::Shader& Shader)
{
	if (CollisionDebugVertexCount == 0 || !CollisionDebugBuffer.IsValid() || !CollisionDebugTexture.IsValid())
	{
		return;
	}
	CmdBuffer.SetShader(&Shader);
	CmdBuffer.SetTransparencyPass(true);
	CmdBuffer.BindTexture(&CollisionDebugTexture);
	CmdBuffer.BindVertexBuffer(&CollisionDebugBuffer);
	CmdBuffer.Draw(CollisionDebugVertexCount, 1, 0, 0);
}
void Game::MapSystem::Finalize()
{
	Script.Finalize();
	ScriptConsole = nullptr;
	CollisionDebugBuffer.Finalize();
	CollisionDebugTexture.Finalize();
	CollisionDebugVertexCount = 0;
	if (Scene)
	{
		Scene.reset();
	}
	CollisionArchives.clear();
	if (Resources)
	{
		if (!MapInfoArchiveName.empty())
		{
			Resources->RemoveFile(MapInfoArchiveName);
			MapInfoArchiveName.clear();
		}
		Resources->RemoveFile("mapinfo");
		Resources->RemoveFile("map");
		Resources = nullptr;
	}
	GraphicsDisplay = nullptr;
	GraphicsShader = nullptr;
	LoadedDatPath.clear();
	LoadedMapInfoPath.clear();
	HasRequestedMap = false;
}
Game::GameScene* Game::MapSystem::GetScene() const
{
	return Scene.get();
}
const Foca3DGfx::FocaMdl* Game::MapSystem::GetModel(u32 Id) const
{
	return Scene ? Scene->GetModel(Id) : nullptr;
}
const FocaCol::ColArc* Game::MapSystem::GetCollisionArchive(u32 Id) const
{
	const auto collision = std::find_if(CollisionArchives.begin(), CollisionArchives.end(), [Id](const CollisionResource& entry)
	{
		return entry.Id == Id;
	});
	return collision == CollisionArchives.end() ? nullptr : &collision->Archive;
}
const DynamicArray<Game::CollisionResource>& Game::MapSystem::GetCollisionArchives() const
{
	return CollisionArchives;
}