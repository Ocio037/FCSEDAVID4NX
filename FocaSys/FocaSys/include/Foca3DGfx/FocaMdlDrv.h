#pragma once
#include<Types.h>
#include<Macros.h>
#include<CommandBuffer.h>
#include<Shader.h>
#include<Display.h>
#include<Buffer.h>
#include<Texture.h>
#include"FocaMdl.h"
#include<DynamicArray.h>
#include<DynamicDeque.h>
#include<glm/vec3.hpp>
#include<string_view>
namespace Foca3DGfx
{
    class FocaArchive;
    struct MeshBatch
    {
        gx::Buffer IndexBuf;
        gx::Texture Texture;
        u32 IndexCount = 0;
        bool HasIndices = false;
        bool Transparent = false;
        f32 SortCenter[3]{};
    };
    struct MeshBuffers
    {
        gx::Buffer VertexBuf;
        DynamicArray<detail::FocaVertex> VertexData;
        u32 VertexCount = 0;
        DynamicDeque<MeshBatch> Batches;
    };
    struct TransparentDraw
    {
        f32 Depth = 0.0f;
        gx::Shader* Shader = nullptr;
        gx::Buffer* VertexBuffer = nullptr;
        gx::Buffer* IndexBuffer = nullptr;
        gx::Texture* Texture = nullptr;
        u32 VertexCount = 0;
        u32 IndexCount = 0;
        bool HasIndices = false;
        void Execute(gx::CommandBuffer& CmdBuffer) const;
    };
    class TransparentDrawCollector final
    {
    public:
        using SubmitFunction = void(*)(void*, const TransparentDraw&);
        TransparentDrawCollector() = default;
        TransparentDrawCollector(void* Context, SubmitFunction Submit) : Context(Context), Submit(Submit) {}
        explicit operator bool() const { return Submit != nullptr; }
        void Add(const TransparentDraw& Draw) const
        {
            if (Submit != nullptr)
            {
                Submit(Context, Draw);
            }
        }
    private:
        void* Context = nullptr;
        SubmitFunction Submit = nullptr;
    };
    // Runtime state for one rendered copy of immutable FocaMdl data.
    // The referenced FocaMdl and gx::Display must outlive this instance.
    class FocaMdlInstance final : NonCopy<>
    {
    public:
        FocaMdlInstance() = default;
        ~FocaMdlInstance();
        void Finalize();
        [[nodiscard]] bool IsValid() const { return ModelData != nullptr; }
        [[nodiscard]] const FocaMdl* GetModelData() const { return ModelData; }
    private:
        friend class FocaMdlDrv;
        const FocaMdl* ModelData = nullptr;
        DynamicDeque<MeshBuffers> Meshes;
        glm::vec3 Offset{ 0.0f };
        glm::vec3 Scale{ 1.0f };
        const detail::FocaUvAnimation* UvAnimation = nullptr;
        f64 UvAnimationTime = 0.0;
        bool UvAnimationLoop = true;
    };
    class FocaMdlDrv final : NonCopy<>
    {
    public:
        FocaMdlDrv();
        explicit FocaMdlDrv(gx::Display* Display);
        ~FocaMdlDrv();
        void Initialize(gx::Display* Display);
        void Finalize();
        void SetShader(gx::Shader* Shader);
        gx::Shader* GetShader() const;
        bool BuildBuffers(FocaMdlInstance& Instance, const FocaMdl& Model);
        bool BuildBuffers(FocaMdlInstance& Instance, const FocaMdl& Model, const FocaArchive& Archive);
        bool BuildBuffers(FocaMdlInstance& Instance, const FocaMdl& Model, const detail::FocaTexture& Texture);
		bool UpdateUVs(FocaMdlInstance& Instance, u32 MeshIndex, const DynamicArray<f32>& UVs);
		bool PlayUvAnimation(FocaMdlInstance& Instance, std::string_view AnimationName, bool Loop = true);
		void Update(FocaMdlInstance& Instance, f64 DeltaSeconds);
		bool UpdateUvAnimation(FocaMdlInstance& Instance, const detail::FocaUvAnimation& Animation, f64 TimeSeconds, bool Loop = true);
		bool SetModelOffset(FocaMdlInstance& Instance, f32 X, f32 Y, f32 Z);
		bool SetModelScale(FocaMdlInstance& Instance, f32 X, f32 Y, f32 Z);
		void Draw(gx::CommandBuffer& CmdBuffer, FocaMdlInstance& Instance, bool TransparentPass = false);
		void DrawMesh(gx::CommandBuffer& CmdBuffer, FocaMdlInstance& Instance, u32 MeshIndex, bool TransparentPass = false);
		void CollectTransparentDraws(gx::CommandBuffer& CmdBuffer, FocaMdlInstance& Instance, const TransparentDrawCollector& Collector);
    private:
        gx::Display* Display = nullptr;
        gx::Shader* Shader = nullptr;
        gx::Texture DefaultTexture;  
        bool BuildBuffersInternal(FocaMdlInstance& Instance, const FocaMdl& Model, const FocaArchive* Archive, const detail::FocaTexture* DirectTexture);
    };
}
