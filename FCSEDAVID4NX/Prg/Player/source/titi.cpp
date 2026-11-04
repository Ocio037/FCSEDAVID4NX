#define NOMINMAX
#include"titi.h"
#include<FileLoader.h>
#include<Display.h>
#include<Shader.h>
#include<CommandBuffer.h>
#include<algorithm>
#include<limits>
#include<glm/glm.hpp>
namespace
{
	static constexpr u32 TitiModelAssetId = 2;
	static constexpr u32 TitiTextureAssetId = 3;
}
Player::Titi::Titi() 
{

}
Player::Titi::~Titi()
{
	Finalize();
}
bool Player::Titi::Init(GameUtil::FileLoader& Resources, gx::Display& Display, gx::Shader& Shader)
{
	Finalize();
	const GameUtil::ArcEntry* modelEntry = Resources.FindResourceById(GameUtil::DataArchiveName, TitiModelAssetId);
	const GameUtil::ArcEntry* textureEntry = Resources.FindResourceById(GameUtil::DataArchiveName, TitiTextureAssetId);
	if (modelEntry == nullptr || textureEntry == nullptr || modelEntry->DecompressedData.empty() || textureEntry->DecompressedData.empty())
	{
		return false;
	}
	Model = Mem::MakeUnique<Foca3DGfx::FocaMdl>();
	if (!Model || !Model->LoadFromMemory(modelEntry->DecompressedData.data(), modelEntry->DecompressedData.size()) || Model->GetMeshes().empty())
	{
		Finalize();
		return false;
	}
	glm::vec3 minimum(std::numeric_limits<f32>::max());
	glm::vec3 maximum(std::numeric_limits<f32>::lowest());
	bool hasVertices = false;
	for (const auto& mesh : Model->GetMeshes())
	{
		for (const auto& vertex : mesh.Vertices)
		{
			const glm::vec3 position(vertex.Position[0], vertex.Position[1], vertex.Position[2]);
			minimum = glm::min(minimum, position);
			maximum = glm::max(maximum, position);
			hasVertices = true;
		}
	}
	if (!hasVertices)
	{
		Finalize();
		return false;
	}
	Foca3DGfx::detail::FocaTexture texture{};
	texture.Name = "titimdl.ctx";
	texture.Format = "ctx";
	texture.IncludedData = true;
	texture.Data = textureEntry->DecompressedData;
	ModelDriver.Initialize(&Display);
	ModelDriver.SetShader(&Shader);
	if (!ModelDriver.BuildBuffers(ModelInstance, *Model, texture))
	{
		Finalize();
		return false;
	}
	Controller.Initialize((maximum - minimum) * 0.5f);
	if (!ModelDriver.PlayUvAnimation(ModelInstance, "01_IDLE", true))
	{
		Finalize();
		return false;
	}
	return true;
}
void Player::Titi::Update(f64 DeltaSeconds, const Game::MapSystem* Map)
{
	if (!Model)
	{
		return;
	}
	const f32 deltaSeconds = std::clamp(static_cast<f32>(DeltaSeconds), 0.0f, 0.05f);
	Controller.Update(deltaSeconds, TitiInput::ReadKeyboard(), Map, ModelInstance, ModelDriver);
	ModelDriver.Update(ModelInstance, deltaSeconds);
}
void Player::Titi::Draw(gx::CommandBuffer& CmdBuffer, bool TransparentPass)
{
	if (Model)
	{
		ModelDriver.Draw(CmdBuffer, ModelInstance, TransparentPass);
	}
}
void Player::Titi::CollectTransparentDraws(gx::CommandBuffer& CmdBuffer, const Foca3DGfx::TransparentDrawCollector& Collector)
{
	if (Model)
	{
		ModelDriver.CollectTransparentDraws(CmdBuffer, ModelInstance, Collector);
	}
}
const glm::vec3& Player::Titi::GetPosition() const
{
	return Controller.GetPosition();
}
void Player::Titi::Finalize()
{
	ModelInstance.Finalize();
	ModelDriver.Finalize();
	Model.reset();
	Controller.Finalize();
}
