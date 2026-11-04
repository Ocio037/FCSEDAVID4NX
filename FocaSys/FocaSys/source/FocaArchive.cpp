#include"FocaArchive.h"
#include<cstring>
Foca3DGfx::FocaArchive::FocaArchive() : Version(0)
{

}
Foca3DGfx::FocaArchive::~FocaArchive()
{

}
Foca3DGfx::detail::FocaModel Foca3DGfx::FocaArchive::GetModel(std::string_view Name) const
{
    for (const auto& mdl : Models)
    {
        if (mdl.FileName == Name)
        {
            return mdl;
        }
    }
    return detail::FocaModel{};
}
Foca3DGfx::detail::FocaTexture Foca3DGfx::FocaArchive::GetTexture(std::string_view Name) const
{
    for (const auto& tex : Textures)
    {
        if (tex.Name == Name) 
        {
            return tex;
        }
    }
    return detail::FocaTexture{};
}
u16 Foca3DGfx::FocaArchive::GetVersion() const
{
    return Version;
}
const DynamicArray<Foca3DGfx::detail::FocaModel>& Foca3DGfx::FocaArchive::GetModels() const
{
    return Models;
}
const DynamicArray<Foca3DGfx::detail::FocaTexture>& Foca3DGfx::FocaArchive::GetTextures() const
{
    return Textures;
}
bool Foca3DGfx::FocaArchive::LoadFromMemory(const void* Data, u64 size)
{
    if (!Data || size < 6) return false;
    const u8* ptr = static_cast<const u8*>(Data);
    const u8* end = ptr + size;
    auto readBytes = [&](void* dst, size_t len) -> bool {
        if (ptr + len > end) return false;
        std::memcpy(dst, ptr, len);
        ptr += len;
        return true;
    };
    auto readString = [&](String& outStr) -> bool {
        u32 len = 0;
        if (!readBytes(&len, sizeof(len))) return false;
        if (ptr + len > end) return false;
        outStr.assign(reinterpret_cast<const char*>(ptr), len);
        ptr += len;
        return true;
    };
    char magic[4];
    if (!readBytes(magic, 4)) return false;
    if (std::strncmp(magic, "FOCA", 4) != 0) return false;
    if (!readBytes(&Version, sizeof(Version))) return false;
    Models.clear();
    Textures.clear();
    u32 numModels = 0;
    if (!readBytes(&numModels, sizeof(numModels))) return false;
    for (u32 i = 0; i < numModels; i++)
    {
        detail::FocaModel model;
        if (!readString(model.FileName)) return false;
        u8 includeData = 0;
        if (!readBytes(&includeData, sizeof(includeData))) return false;
        model.IncludedData = (includeData != 0);
        if (model.IncludedData)
        {
            u32 numMaterials = 0;
            if (!readBytes(&numMaterials, sizeof(numMaterials))) return false;
            for (u32 m = 0; m < numMaterials; m++)
            {
                detail::FocaMaterial mat;
                if (!readString(mat.Name)) return false;
                if (!readString(mat.AssignedTexture)) return false;
                model.Materials.push_back(mat);
            }

            u32 numMeshes = 0;
            if (!readBytes(&numMeshes, sizeof(numMeshes))) return false;
            for (u32 m = 0; m < numMeshes; m++)
            {
                detail::FocaMesh mesh;
                if (!readString(mesh.Name)) return false;
                if (!readBytes(&mesh.VertexCount, sizeof(mesh.VertexCount))) return false;
                if (!readBytes(&mesh.IndexCount, sizeof(mesh.IndexCount))) return false;
                if (!readBytes(&mesh.MaterialIndex, sizeof(mesh.MaterialIndex))) return false;
                model.Meshes.push_back(mesh);
            }

            u32 modelDataLen = 0;
            if (!readBytes(&modelDataLen, sizeof(modelDataLen))) return false;
            if (modelDataLen > 0)
            {
                model.Data.resize(modelDataLen);
                if (!readBytes(model.Data.data(), modelDataLen)) return false;
            }
        }
        Models.push_back(model);
    }
    u32 numTextures = 0;
    if (!readBytes(&numTextures, sizeof(numTextures))) return false;
    for (u32 i = 0; i < numTextures; i++)
    {
        detail::FocaTexture tex;
        if (!readString(tex.Name)) return false;
        if (!readString(tex.Format)) return false;
        if (Version >= 3)
        {
            if (!readBytes(&tex.Width, sizeof(tex.Width))) return false;
            if (!readBytes(&tex.Height, sizeof(tex.Height))) return false;
        }
        u8 includeData = 0;
        if (!readBytes(&includeData, sizeof(includeData))) return false;
        tex.IncludedData = (includeData != 0);
        if (tex.IncludedData)
        {
            u32 dataLen = 0;
            if (!readBytes(&dataLen, sizeof(dataLen))) return false;

            if (dataLen > 0)
            {
                tex.Data.resize(dataLen);
                if (!readBytes(tex.Data.data(), dataLen)) return false;
            }
        }
        Textures.push_back(tex);
    }
    return true;
}
