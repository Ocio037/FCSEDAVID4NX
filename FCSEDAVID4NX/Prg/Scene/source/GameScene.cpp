#include"GameScene.h"
#include<algorithm>
#include<cctype>
namespace
{
	String NormalizeModelName(String Name)
	{
		const std::size_t separator = Name.find_last_of("/\\");
		if (separator != String::npos)
		{
			Name.erase(0, separator + 1);
		}
		for (char& character : Name)
		{
			character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
		}
		constexpr char Extension[] = ".f3d";
		constexpr std::size_t ExtensionLength = sizeof(Extension) - 1;
		if (Name.size() >= ExtensionLength && Name.compare(Name.size() - ExtensionLength, ExtensionLength, Extension) == 0)
		{
			Name.resize(Name.size() - ExtensionLength);
		}
		return Name;
	}
}
Game::GameScene::GameScene()
{

}
Game::GameScene::~GameScene()
{
	Finalize();
}
bool Game::GameScene::Init(const Foca3DGfx::FocaArchive& ModelArchive, const Foca3DGfx::FocaArchive& TextureArchive, const DynamicArray<ModelLoadRequest>& ModelRequests, gx::Display& Display, gx::Shader& Shader)
{
	Finalize();
	MdlDrv.Initialize(&Display);
	MdlDrv.SetShader(&Shader);
	const auto& archiveModels = ModelArchive.GetModels();
	for (const ModelLoadRequest& request : ModelRequests)
	{
		if (request.Name.empty() || GetModel(request.Id) != nullptr)
		{
			return false;
		}
		const String normalizedRequest = NormalizeModelName(request.Name);
		const auto archiveModel = std::find_if(archiveModels.begin(), archiveModels.end(), [&normalizedRequest](const Foca3DGfx::detail::FocaModel& model)
		{
			return NormalizeModelName(model.FileName) == normalizedRequest;
		});
		if (archiveModel == archiveModels.end() || !archiveModel->IncludedData || archiveModel->Data.empty())
		{
			return false;
		}
		auto mdl = Mem::MakeUnique<Foca3DGfx::FocaMdl>();
		if (!mdl || !mdl->LoadFromArchiveModel(*archiveModel))
		{
			return false;
		}
		auto instance = Mem::MakeUnique<Foca3DGfx::FocaMdlInstance>();
		if (!instance || !MdlDrv.BuildBuffers(*instance, *mdl, TextureArchive))
		{
			return false;
		}
		Models.push_back(SceneModel{ request.Id, std::move(mdl), std::move(instance) });
	}
	return true;
}
void Game::GameScene::Update(f64 DeltaSeconds)
{
	for (SceneModel& model : Models)
	{
		if (model.Instance)
		{
			MdlDrv.Update(*model.Instance, DeltaSeconds);
		}
	}
}
void Game::GameScene::Draw(gx::CommandBuffer& CmdBuffer, bool TransparentPass)
{
	for (const auto& model : Models)
	{
		if (model.Instance)
		{
			MdlDrv.Draw(CmdBuffer, *model.Instance, TransparentPass);
		}
	}
}
void Game::GameScene::CollectTransparentDraws(gx::CommandBuffer& CmdBuffer, const Foca3DGfx::TransparentDrawCollector& Collector)
{
	for (const auto& model : Models)
	{
		if (model.Instance)
		{
			MdlDrv.CollectTransparentDraws(CmdBuffer, *model.Instance, Collector);
		}
	}
}
void Game::GameScene::Finalize()
{
	Models.clear();
	MdlDrv.Finalize();
}
const Foca3DGfx::FocaMdl* Game::GameScene::GetModel(u32 Id) const
{
	const auto model = std::find_if(Models.begin(), Models.end(), [Id](const SceneModel& entry)
	{
		return entry.Id == Id;
	});
	return model == Models.end() ? nullptr : model->Model.get();
}
