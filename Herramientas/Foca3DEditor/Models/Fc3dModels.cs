using System;
using System.Collections.Generic;
using System.IO;
using System.Numerics;
using System.Text;

namespace Foca3DEditor.Models
{
    public class Fc3dMaterial
    {
        public string Name { get; set; } = string.Empty;
        public float[] Color { get; set; } = new float[4];
        public float Metallic { get; set; }
        public float Roughness { get; set; }
        public List<Fc3dMappingNode> MappingNodes { get; set; } = new();
    }

    public class Fc3dMappingNode
    {
        public string Name { get; set; } = string.Empty;
        public string VectorType { get; set; } = string.Empty;
        public Vector3 Location { get; set; }
        public Vector3 Rotation { get; set; }
        public Vector3 Scale { get; set; }
    }

    public class Fc3dMesh
    {
        public string Name { get; set; } = string.Empty;
        public int MaterialIndex { get; set; }
        public int ArmatureIndex { get; set; }
        public uint VertexCount { get; set; }
        public uint IndexCount { get; set; }
        public List<Fc3dVertex> Vertices { get; set; } = new();
        public List<Fc3dTriangle> Triangles { get; set; } = new();
    }

    public class Fc3dVertex
    {
        public Vector3 Position { get; set; }
        public Vector3 Normal { get; set; }
        public Vector4 Color { get; set; } = Vector4.One;
        public List<Vector2> UvLayers { get; set; } = new();
        public List<Fc3dWeight> Weights { get; set; } = new();
    }

    public class Fc3dWeight
    {
        public ushort BoneIndex { get; set; }
        public float Weight { get; set; }
    }

    public class Fc3dTriangle
    {
        public uint V0 { get; set; }
        public uint V1 { get; set; }
        public uint V2 { get; set; }
        public uint MaterialIndex { get; set; }
    }

    public class Fc3dArmature
    {
        public string Name { get; set; } = string.Empty;
        public List<Fc3dBone> Bones { get; set; } = new();
    }

    public class Fc3dBone
    {
        public string Name { get; set; } = string.Empty;
        public int ParentIndex { get; set; }
        public Matrix4x4 Matrix { get; set; }
    }

    public class Fc3dClip
    {
        public string Name { get; set; } = string.Empty;
        public byte Code { get; set; }
        public string Owner { get; set; } = string.Empty;
        public string Action { get; set; } = string.Empty;
        public string Slot { get; set; } = string.Empty;
        public int FirstFrame { get; set; }
        public int LastFrame { get; set; }
        public uint SampleFrames { get; set; }
        
        public List<Fc3dSkeletalFrame> SkeletalFrames { get; set; } = new();
        public List<string> MappingNodes { get; set; } = new();
        public List<Fc3dTransformFrame> TransformFrames { get; set; } = new();
    }

    public class Fc3dSkeletalFrame
    {
        public int FrameNumber { get; set; }
        public Matrix4x4 WorldMatrix { get; set; }
        public List<Matrix4x4> BoneMatrices { get; set; } = new();
    }

    public class Fc3dTransformFrame
    {
        public int FrameNumber { get; set; }
        public List<Fc3dNodeTransform> NodeTransforms { get; set; } = new();
    }

    public class Fc3dNodeTransform
    {
        public Vector3 Location { get; set; }
        public Vector3 Rotation { get; set; }
        public Vector3 Scale { get; set; }
    }
    public class TextureEntry
    {
        public string Name { get; set; } = string.Empty;
        public string Format { get; set; } = string.Empty;
        public string SourcePath { get; set; } = string.Empty;
        public uint Width { get; set; }
        public uint Height { get; set; }
        public bool IncludeData { get; set; } = true;
    }
}
