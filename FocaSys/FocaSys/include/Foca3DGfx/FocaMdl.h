#pragma once
#include<Types.h>
#include<Macros.h>
#include<DynamicArray.h>
#include<DynamicString.h>
#include<string_view>
#include"Detail.h"
namespace Foca3DGfx
{
    class FocaMdl final : NonCopy<>
    {
    public:
        FocaMdl();
        ~FocaMdl();
        bool LoadFromMemory(const void* Data, u64 size);
        bool LoadFromArchiveModel(const detail::FocaModel& Model);
        u16 GetVersion() const;
        f32 GetFps() const;
        const DynamicArray<detail::FocaMesh3D>& GetMeshes() const;
        const DynamicArray<detail::FocaMaterial3D>& GetMaterials() const;
        const DynamicArray<detail::FocaUvAnimation>& GetUvAnimations() const;
        const detail::FocaUvAnimation* FindFirstAnimatedUvAnimation() const;
        bool EvaluateUvAnimation(const detail::FocaUvAnimation& Animation, f64 TimeSeconds, DynamicArray<DynamicArray<f32>>& MeshUVs, bool Loop = true) const;
        const detail::FocaMesh3D* FindMesh(std::string_view Name) const;
        const detail::FocaMaterial3D* FindMaterial(std::string_view Name) const;
        u32 GetTotalVertexCount() const;
        u32 GetTotalIndexCount() const;
    private:
        u16 Version;
        f32 Fps;
        DynamicArray<detail::FocaMesh3D> Meshes;
        DynamicArray<detail::FocaMaterial3D> Materials;
        DynamicArray<detail::FocaUvAnimation> UvAnimations;
    };
}