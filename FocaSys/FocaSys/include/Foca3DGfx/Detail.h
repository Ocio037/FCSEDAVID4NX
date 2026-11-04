#pragma once
#include<Types.h>
#include<DynamicArray.h>
#include<DynamicString.h>
namespace Foca3DGfx
{
    namespace detail
    {
        struct FocaMaterial
        {
            String Name;
            String AssignedTexture;
        };
        struct FocaMesh
        {
            String Name;
            u32 VertexCount;
            u32 IndexCount;
            u32 MaterialIndex;
        };
        struct FocaModel
        {
            String FileName;
            bool IncludedData;
            DynamicArray<FocaMaterial> Materials;
            DynamicArray<FocaMesh> Meshes;
            DynamicArray<u8> Data;
        };
        struct FocaTexture
        {
            String Name;
            String Format;
            u32 Width = 0;
            u32 Height = 0;
            bool IncludedData;
            DynamicArray<u8> Data;
        };
        struct FocaVertex
        {
            f32 Position[3]{};
            f32 Normal[3]{};
            f32 Color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            f32 UV[2]{};
        };
        struct FocaTriangle
        {
            u32 V0;
            u32 V1;
            u32 V2;
            u32 MaterialIndex;
        };
        struct FocaMesh3D
        {
            String Name;
            s32 ArmatureIndex = -1;
            u32 MaterialIndex = 0;
            u32 VertexCount = 0;
            u32 IndexCount = 0;
            DynamicArray<FocaVertex> Vertices;
            DynamicArray<FocaTriangle> Triangles;
            DynamicArray<s32> MaterialSlots;
        };
        struct FocaMaterial3D
        {
            String Name;
            f32 Color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            f32 Metallic = 0.0f;
            f32 Roughness = 0.5f;
            String AssignedTexture;
        };
        struct FocaUvMappingTransform
        {
            f32 Location[3]{};
            f32 Rotation[3]{};
            f32 Scale[3] = { 1.0f, 1.0f, 1.0f };
            u8 InterpolationModes[9] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        };
        struct FocaUvAnimationSample
        {
            s32 Frame = 0;
            DynamicArray<FocaUvMappingTransform> MappingTransforms;
        };
        struct FocaUvAnimation
        {
            String Name;
            String ActionName;
            DynamicArray<String> MappingNodes;
            DynamicArray<FocaUvAnimationSample> Samples;
        };
    }
}
