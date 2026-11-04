#include"Foca3DGfx/FocaMdl.h"
#include"F3DUtils.h"
#include<glm/gtc/matrix_transform.hpp>
Foca3DGfx::FocaMdl::FocaMdl() : 
    Version(0),
    Fps(30.0f)
{

}
Foca3DGfx::FocaMdl::~FocaMdl()
{

}
bool Foca3DGfx::FocaMdl::LoadFromMemory(const void* Data, u64 size)
{
    if (!Data || size < 24)
    {
        return false;
    }
    const u8* ptr = static_cast<const u8*>(Data);
    const u8* end = ptr + size;
    auto readBytes = [&](void* dst, size_t len) -> bool
    {
        if (ptr + len > end)
        {
                return false;
        }
        std::memcpy(dst, ptr, len);
        ptr += len;
        return true;
    };
    auto skipBytes = [&](u64 len) -> bool
        {
            if (len > static_cast<u64>(end - ptr)) return false;
            ptr += static_cast<size_t>(len);
            return true;
        };
    auto readString = [&](String& outStr) -> bool
        {
            u32 len = 0;
            if (!readBytes(&len, sizeof(len))) return false;
            if (ptr + len > end) return false;
            outStr.assign(reinterpret_cast<const char*>(ptr), len);
            ptr += len;
            return true;
        };
    char magic[4];
    if (!readBytes(magic, 4))
    {
        return false;
    }
    if (std::strncmp(magic, "FC3D", 4) != 0)
    {
        return false;
    }
    u16 ver = 0;
    u16 reserved = 0;
    if (!readBytes(&ver, sizeof(ver)))
    {
        return false;
    }
    if (!readBytes(&reserved, sizeof(reserved)))
    {
        return false;
    }
    Version = ver;
    u32 numMeshes = 0;
    u32 numMaterials = 0;
    u32 numArmatures = 0;
    u32 numClips = 0;
    f32 fps = 30.0f;
    if (!readBytes(&numMeshes, sizeof(numMeshes))) return false;
    if (!readBytes(&numMaterials, sizeof(numMaterials))) return false;
    if (!readBytes(&numArmatures, sizeof(numArmatures))) return false;
    if (!readBytes(&numClips, sizeof(numClips))) return false;
    if (!readBytes(&fps, sizeof(fps))) return false;
    Fps = fps;
    Materials.clear();
    Meshes.clear();
    UvAnimations.clear();
    for (u32 i = 0; i < numMaterials; ++i)
    {
        detail::FocaMaterial3D mat;
        if (!readString(mat.Name)) return false;
        if (!readBytes(mat.Color, sizeof(mat.Color))) return false;
        if (!readBytes(&mat.Metallic, sizeof(mat.Metallic))) return false;
        if (!readBytes(&mat.Roughness, sizeof(mat.Roughness))) return false;
        u32 mappingNodesCount = 0;
        if (!readBytes(&mappingNodesCount, sizeof(mappingNodesCount))) return false;
        for (u32 j = 0; j < mappingNodesCount; ++j)
        {
            String nodeName;
            String vectorType;
            f32 transform[9];
            if (!readString(nodeName)) return false;
            if (!readString(vectorType)) return false;
            if (!readBytes(transform, sizeof(transform))) return false;
        }
        Materials.push_back(mat);
    }
    for (u32 i = 0; i < numMeshes; ++i)
    {
        detail::FocaMesh3D mesh;
        if (!readString(mesh.Name)) return false;
        s32 armIdx = -1;
        u32 materialSlotsCount = 0;
        u32 vertexCount = 0;
        u32 trianglesCount = 0;
        u32 uvLayersCount = 0;
        if (!readBytes(&armIdx, sizeof(armIdx))) return false;
        if (!readBytes(&materialSlotsCount, sizeof(materialSlotsCount))) return false;
        if (!readBytes(&vertexCount, sizeof(vertexCount))) return false;
        if (!readBytes(&trianglesCount, sizeof(trianglesCount))) return false;
        if (!readBytes(&uvLayersCount, sizeof(uvLayersCount))) return false;
        mesh.ArmatureIndex = armIdx;
        mesh.VertexCount = vertexCount;
        mesh.IndexCount = trianglesCount * 3;
        mesh.MaterialSlots.reserve(materialSlotsCount);
        for (u32 m = 0; m < materialSlotsCount; ++m)
        {
            s32 matIdx = -1;
            if (!readBytes(&matIdx, sizeof(matIdx))) return false;
            mesh.MaterialSlots.push_back(matIdx);
            if (m == 0 && matIdx >= 0)
            {
                mesh.MaterialIndex = static_cast<u32>(matIdx);
            }
        }
        for (u32 u = 0; u < uvLayersCount; ++u)
        {
            String uvName;
            if (!readString(uvName)) return false;
        }
        f32 worldMatrix[16];
        if (!readBytes(worldMatrix, sizeof(worldMatrix))) return false;
        mesh.Vertices.reserve(vertexCount);
        for (u32 v = 0; v < vertexCount; ++v)
        {
            detail::FocaVertex vert;
            if (!readBytes(vert.Position, sizeof(vert.Position))) return false;
            if (!readBytes(vert.Normal, sizeof(vert.Normal))) return false;
            if (Version >= 3)
            {
                if (!readBytes(vert.Color, sizeof(vert.Color)))
                {
                    return false;
                }
            }
            else
            {
                vert.Color[0] = 1.0f;
                vert.Color[1] = 1.0f;
                vert.Color[2] = 1.0f;
                vert.Color[3] = 1.0f;
            }
            for (u32 u = 0; u < uvLayersCount; ++u)
            {
                f32 uv[2];
                if (!readBytes(uv, sizeof(uv))) return false;
                if (u == 0)
                {
                    vert.UV[0] = uv[0];
                    vert.UV[1] = uv[1];
                }
            }
            u16 weightCount = 0;
            if (!readBytes(&weightCount, sizeof(weightCount))) return false;
            for (u16 w = 0; w < weightCount; ++w)
            {
                u16 boneIdx = 0;
                f32 weight = 0.0f;
                if (!readBytes(&boneIdx, sizeof(boneIdx))) return false;
                if (!readBytes(&weight, sizeof(weight))) return false;
            }
            mesh.Vertices.push_back(vert);
        }
        mesh.Triangles.reserve(trianglesCount);
        for (u32 t = 0; t < trianglesCount; ++t)
        {
            detail::FocaTriangle tri;
            if (!readBytes(&tri.V0, sizeof(tri.V0))) return false;
            if (!readBytes(&tri.V1, sizeof(tri.V1))) return false;
            if (!readBytes(&tri.V2, sizeof(tri.V2))) return false;
            if (!readBytes(&tri.MaterialIndex, sizeof(tri.MaterialIndex))) return false;
            mesh.Triangles.push_back(tri);
        }
        Meshes.push_back(std::move(mesh));
    }
    for (u32 i = 0; i < numArmatures; ++i)
    {
        String armatureName;
        u32 boneCount = 0;
        if (!readString(armatureName) || !readBytes(&boneCount, sizeof(boneCount))) return false;
        for (u32 bone = 0; bone < boneCount; ++bone)
        {
            String boneName;
            s32 parentIndex = -1;
            f32 matrix[16];
            if (!readString(boneName) || !readBytes(&parentIndex, sizeof(parentIndex)) || !readBytes(matrix, sizeof(matrix)))
            {
                return false;
            }
        }
    }
    for (u32 i = 0; i < numClips; ++i)
    {
        String clipName;
        String ownerName;
        String actionName;
        String slotName;
        u8 clipType = 0;
        s32 startFrame = 0;
        s32 endFrame = 0;
        u32 sampleCount = 0;
        if (!readString(clipName) || !readBytes(&clipType, sizeof(clipType)) || !readString(ownerName) || !readString(actionName) || !readString(slotName) || !readBytes(&startFrame, sizeof(startFrame)) || !readBytes(&endFrame, sizeof(endFrame)) || !readBytes(&sampleCount, sizeof(sampleCount)))
        {
            return false;
        }
        if (clipType == 1)
        {
            u32 boneCount = 0;
            if (!readBytes(&boneCount, sizeof(boneCount))) return false;
            const u64 bytesPerFrame = sizeof(s32) + (static_cast<u64>(boneCount) + 1) * 16 * sizeof(f32);
            if (sampleCount != 0 && bytesPerFrame > UINT64_MAX / sampleCount) return false;
            if (!skipBytes(bytesPerFrame * sampleCount)) return false;
            continue;
        }
        if (clipType != 2)
        {
            return false;
        }
        detail::FocaUvAnimation animation;
        animation.Name = std::move(clipName);
        animation.ActionName = std::move(actionName);
        u32 mappingNodeCount = 0;
        if (!readBytes(&mappingNodeCount, sizeof(mappingNodeCount))) return false;
        const u64 bytesPerMapping = 9 * sizeof(f32) + (Version >= 4 ? 9 * sizeof(u8) : 0);
        const u64 bytesPerSample = sizeof(s32) + static_cast<u64>(mappingNodeCount) * bytesPerMapping;
        if (sampleCount != 0 && bytesPerSample > UINT64_MAX / sampleCount) return false;
        if (bytesPerSample * sampleCount > static_cast<u64>(end - ptr)) return false;
        animation.MappingNodes.reserve(mappingNodeCount);
        for (u32 node = 0; node < mappingNodeCount; ++node)
        {
            String nodeName;
            if (!readString(nodeName)) return false;
            animation.MappingNodes.push_back(std::move(nodeName));
        }
        animation.Samples.reserve(sampleCount);
        for (u32 sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
        {
            detail::FocaUvAnimationSample sample;
            if (!readBytes(&sample.Frame, sizeof(sample.Frame))) return false;
            sample.MappingTransforms.resize(mappingNodeCount);
            for (auto& transform : sample.MappingTransforms)
            {
                if (!readBytes(transform.Location, sizeof(transform.Location)) || !readBytes(transform.Rotation, sizeof(transform.Rotation)) || !readBytes(transform.Scale, sizeof(transform.Scale)))
                {
                    return false;
                }
                if (Version >= 4 && !readBytes(transform.InterpolationModes, sizeof(transform.InterpolationModes)))
                {
                    return false;
                }
            }
            animation.Samples.push_back(std::move(sample));
        }
        UvAnimations.push_back(std::move(animation));
    }
    return true;
}
bool Foca3DGfx::FocaMdl::LoadFromArchiveModel(const detail::FocaModel& Model)
{
    if (!Model.Data.empty())
    {
        if (!LoadFromMemory(Model.Data.data(), Model.Data.size()))
        {
            return false;
        }
        for (const auto& matAssignment : Model.Materials)
        {
            for (auto& mat : Materials)
            {
                if (mat.Name == matAssignment.Name)
                {
                    mat.AssignedTexture = matAssignment.AssignedTexture;
                }
            }
        }
        return true;
    }
    Meshes.clear();
    Materials.clear();
    Version = 3;
    for (const auto& mat : Model.Materials)
    {
        detail::FocaMaterial3D m;
        m.Name = mat.Name;
        m.AssignedTexture = mat.AssignedTexture;
        Materials.push_back(m);
    }
    for (const auto& mesh : Model.Meshes)
    {
        detail::FocaMesh3D m;
        m.Name = mesh.Name;
        m.VertexCount = mesh.VertexCount;
        m.IndexCount = mesh.IndexCount;
        m.MaterialIndex = mesh.MaterialIndex;
        Meshes.push_back(m);
    }
    return true;
}
u16 Foca3DGfx::FocaMdl::GetVersion() const
{
    return Version;
}
f32 Foca3DGfx::FocaMdl::GetFps() const
{
    return Fps;
}
const DynamicArray<Foca3DGfx::detail::FocaMesh3D>& Foca3DGfx::FocaMdl::GetMeshes() const
{
    return Meshes;
}
const DynamicArray<Foca3DGfx::detail::FocaMaterial3D>& Foca3DGfx::FocaMdl::GetMaterials() const
{
    return Materials;
}
const DynamicArray<Foca3DGfx::detail::FocaUvAnimation>& Foca3DGfx::FocaMdl::GetUvAnimations() const
{
    return UvAnimations;
}
const Foca3DGfx::detail::FocaUvAnimation* Foca3DGfx::FocaMdl::FindFirstAnimatedUvAnimation() const
{
	for (const auto& animation : UvAnimations)
	{
		if (animation.Samples.size() > 1 && !animation.MappingNodes.empty())
		{
			return &animation;
		}
	}
	return nullptr;
}
bool Foca3DGfx::FocaMdl::EvaluateUvAnimation(const detail::FocaUvAnimation& Animation, f64 TimeSeconds, DynamicArray<DynamicArray<f32>>& MeshUVs, bool Loop) const
{
	const auto& samples = Animation.Samples;
	if (samples.empty() || Animation.MappingNodes.empty() || !std::isfinite(TimeSeconds))
	{
		return false;
	}
	for (const auto& sample : samples)
	{
		if (sample.MappingTransforms.size() != Animation.MappingNodes.size())
		{
			return false;
		}
	}
	const f64 fps = std::max(static_cast<f64>(Fps), 1.0);
	const f64 framePosition = std::max(TimeSeconds, 0.0) * fps;
	if (!std::isfinite(framePosition))
	{
		return false;
	}
	const f64 phase = Loop ? std::fmod(framePosition, static_cast<f64>(samples.size())) : std::min(framePosition, static_cast<f64>(samples.size() - 1));
	const size_t sampleIndex0 = static_cast<size_t>(phase);
	const size_t sampleIndex1 = Loop ? (sampleIndex0 + 1) % samples.size() : std::min(sampleIndex0 + 1, samples.size() - 1);
	const f32 blend = static_cast<f32>(phase - static_cast<f64>(sampleIndex0));
	DynamicArray<detail::FocaUvMappingTransform> transforms = samples[sampleIndex0].MappingTransforms;
	for (size_t i = 0; i < transforms.size(); ++i)
	{
		for (u32 component = 0; component < 9; ++component)
		{
			const u8 mode = samples[sampleIndex0].MappingTransforms[i].InterpolationModes[component];
			detail::SetUvComponent(transforms[i], component, detail::InterpolateUvComponent(samples, i, component, sampleIndex0, sampleIndex1, blend, mode));
		}
	}
	DynamicArray<glm::mat4> mappingMatrices;
	mappingMatrices.reserve(transforms.size());
	for (const auto& transform : transforms)
	{
		const glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(transform.Scale[0], transform.Scale[1], transform.Scale[2]));
		const glm::mat4 rotateX = glm::rotate(glm::mat4(1.0f), transform.Rotation[0], glm::vec3(1.0f, 0.0f, 0.0f));
		const glm::mat4 rotateY = glm::rotate(glm::mat4(1.0f), transform.Rotation[1], glm::vec3(0.0f, 1.0f, 0.0f));
		const glm::mat4 rotateZ = glm::rotate(glm::mat4(1.0f), transform.Rotation[2], glm::vec3(0.0f, 0.0f, 1.0f));
		const glm::mat4 translate = glm::translate(glm::mat4(1.0f), glm::vec3(transform.Location[0], transform.Location[1], transform.Location[2]));
		mappingMatrices.push_back(translate * rotateZ * rotateY * rotateX * scale);
	}
	MeshUVs.clear();
	MeshUVs.resize(Meshes.size());
	for (size_t meshIndex = 0; meshIndex < Meshes.size(); ++meshIndex)
	{
		const auto& mesh = Meshes[meshIndex];
		auto& animatedUVs = MeshUVs[meshIndex];
		animatedUVs.reserve(mesh.Vertices.size() * 2);
		for (const auto& vertex : mesh.Vertices)
		{
			glm::vec4 uv(vertex.UV[0], vertex.UV[1], 0.0f, 1.0f);
			for (const auto& mappingMatrix : mappingMatrices)
			{
				uv = mappingMatrix * uv;
			}
			animatedUVs.push_back(uv.x);
			animatedUVs.push_back(uv.y);
		}
	}
	return true;
}
const Foca3DGfx::detail::FocaMesh3D* Foca3DGfx::FocaMdl::FindMesh(std::string_view Name) const
{
    for (const auto& mesh : Meshes)
    {
        if (mesh.Name == Name)
        {
            return &mesh;
        }
    }
    return nullptr;
}
const Foca3DGfx::detail::FocaMaterial3D* Foca3DGfx::FocaMdl::FindMaterial(std::string_view Name) const
{
    for (const auto& mat : Materials)
    {
        if (mat.Name == Name) 
        {
            return &mat;
        }
    }
    return nullptr;
}
u32 Foca3DGfx::FocaMdl::GetTotalVertexCount() const
{
    u32 total = 0;
    for (const auto& mesh : Meshes)
    {
        total += mesh.VertexCount;
    }
    return total;
}
u32 Foca3DGfx::FocaMdl::GetTotalIndexCount() const
{
    u32 total = 0;
    for (const auto& mesh : Meshes)
    {
        total += mesh.IndexCount;
    }
    return total;
}