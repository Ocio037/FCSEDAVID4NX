using System;
using System.Collections.Generic;
using System.IO;
using System.Numerics;
using System.Text;

namespace Foca3DEditor.Models
{
    public class Fc3dFile
    {
        public uint Version { get; set; }
        public float Fps { get; set; }
        public List<Fc3dMesh> Meshes { get; set; } = new();
        public List<Fc3dMaterial> Materials { get; set; } = new();
        public List<Fc3dArmature> Armatures { get; set; } = new();
        public List<Fc3dClip> Clips { get; set; } = new();

        public static Fc3dFile Read(string path)
        {
            using var fs = new FileStream(path, FileMode.Open, FileAccess.Read);
            using var br = new BinaryReader(fs);

            byte[] magic = br.ReadBytes(4);
            if (Encoding.ASCII.GetString(magic) != "FC3D")
                throw new Exception("No es un archivo FC3D");

            uint version = br.ReadUInt16();
            br.ReadUInt16(); // reserved
            uint numMeshes = br.ReadUInt32();
            uint numMaterials = br.ReadUInt32();
            uint numArmatures = br.ReadUInt32();
            uint numClips = br.ReadUInt32();
            float fps = br.ReadSingle();

            var file = new Fc3dFile { Version = version, Fps = fps };

            for (uint i = 0; i < numMaterials; i++)
            {
                var mat = new Fc3dMaterial();
                mat.Name = ReadString(br);
                
                mat.Color[0] = br.ReadSingle();
                mat.Color[1] = br.ReadSingle();
                mat.Color[2] = br.ReadSingle();
                mat.Color[3] = br.ReadSingle();
                mat.Metallic = br.ReadSingle();
                mat.Roughness = br.ReadSingle();

                uint mappingNodesCount = br.ReadUInt32();
                for (uint j = 0; j < mappingNodesCount; j++)
                {
                    mat.MappingNodes.Add(new Fc3dMappingNode
                    {
                        Name = ReadString(br),
                        VectorType = ReadString(br),
                        Location = new Vector3(br.ReadSingle(), br.ReadSingle(), br.ReadSingle()),
                        Rotation = new Vector3(br.ReadSingle(), br.ReadSingle(), br.ReadSingle()),
                        Scale = new Vector3(br.ReadSingle(), br.ReadSingle(), br.ReadSingle())
                    });
                }
                file.Materials.Add(mat);
            }

            for (uint i = 0; i < numMeshes; i++)
            {
                var mesh = new Fc3dMesh();
                mesh.Name = ReadString(br);
                mesh.ArmatureIndex = br.ReadInt32();
                
                uint materialSlotsCount = br.ReadUInt32();
                mesh.VertexCount = br.ReadUInt32();
                uint trianglesCount = br.ReadUInt32();
                uint uvLayersCount = br.ReadUInt32();

                mesh.IndexCount = trianglesCount * 3;

                // Read material slots (even if we just use the first or none for simple rendering)
                for (uint m = 0; m < materialSlotsCount; m++)
                {
                    int matIdx = br.ReadInt32();
                    if (m == 0) mesh.MaterialIndex = matIdx; // Just use first material index for fallback rendering
                }

                // Read UV layer names
                for (uint u = 0; u < uvLayersCount; u++)
                {
                    ReadString(br); // We can ignore names for now
                }

                // Read world matrix
                Matrix4x4 worldMatrix = ReadMatrix(br);

                for (uint v = 0; v < mesh.VertexCount; v++)
                {
                    var vert = new Fc3dVertex();
                    vert.Position = new Vector3(br.ReadSingle(), br.ReadSingle(), br.ReadSingle());
                    vert.Normal = new Vector3(br.ReadSingle(), br.ReadSingle(), br.ReadSingle());
                    
                    if (version >= 3)
                    {
                        vert.Color = new Vector4(br.ReadSingle(), br.ReadSingle(), br.ReadSingle(), br.ReadSingle());
                    }
                    else
                    {
                        vert.Color = Vector4.One;
                    }

                    for (uint u = 0; u < uvLayersCount; u++)
                    {
                        vert.UvLayers.Add(new Vector2(br.ReadSingle(), br.ReadSingle()));
                    }

                    ushort weightCount = br.ReadUInt16();
                    for (ushort w = 0; w < weightCount; w++)
                    {
                        vert.Weights.Add(new Fc3dWeight
                        {
                            BoneIndex = br.ReadUInt16(),
                            Weight = br.ReadSingle()
                        });
                    }
                    mesh.Vertices.Add(vert);
                }

                for (uint t = 0; t < trianglesCount; t++)
                {
                    var triangle = new Fc3dTriangle
                    {
                        V0 = br.ReadUInt32(),
                        V1 = br.ReadUInt32(),
                        V2 = br.ReadUInt32(),
                        MaterialIndex = br.ReadUInt32()
                    };
                    mesh.Triangles.Add(triangle);
                }
                file.Meshes.Add(mesh);
            }

            for (uint i = 0; i < numArmatures; i++)
            {
                var arm = new Fc3dArmature();
                arm.Name = ReadString(br);
                uint boneCount = br.ReadUInt32();
                for (uint b = 0; b < boneCount; b++)
                {
                    arm.Bones.Add(new Fc3dBone
                    {
                        Name = ReadString(br),
                        ParentIndex = br.ReadInt32(),
                        Matrix = ReadMatrix(br)
                    });
                }
                file.Armatures.Add(arm);
            }

            for (uint i = 0; i < numClips; i++)
            {
                var clip = new Fc3dClip();
                clip.Name = ReadString(br);
                clip.Code = br.ReadByte();
                clip.Owner = ReadString(br);
                clip.Action = ReadString(br);
                clip.Slot = ReadString(br);
                clip.FirstFrame = br.ReadInt32();
                clip.LastFrame = br.ReadInt32();
                clip.SampleFrames = br.ReadUInt32();

                if (clip.Code == 0) // SKELETAL
                {
                    uint numBones = br.ReadUInt32();
                    for (uint f = 0; f < clip.SampleFrames; f++)
                    {
                        var frame = new Fc3dSkeletalFrame();
                        frame.FrameNumber = br.ReadInt32();
                        frame.WorldMatrix = ReadMatrix(br);
                        for (uint b = 0; b < numBones; b++)
                        {
                            frame.BoneMatrices.Add(ReadMatrix(br));
                        }
                        clip.SkeletalFrames.Add(frame);
                    }
                }
                else
                {
                    uint numNodes = br.ReadUInt32();
                    for (uint n = 0; n < numNodes; n++)
                    {
                        clip.MappingNodes.Add(ReadString(br));
                    }
                    for (uint f = 0; f < clip.SampleFrames; f++)
                    {
                        var frame = new Fc3dTransformFrame();
                        frame.FrameNumber = br.ReadInt32();
                        for (uint n = 0; n < numNodes; n++)
                        {
                            frame.NodeTransforms.Add(new Fc3dNodeTransform
                            {
                                Location = new Vector3(br.ReadSingle(), br.ReadSingle(), br.ReadSingle()),
                                Rotation = new Vector3(br.ReadSingle(), br.ReadSingle(), br.ReadSingle()),
                                Scale = new Vector3(br.ReadSingle(), br.ReadSingle(), br.ReadSingle())
                            });
                        }
                        clip.TransformFrames.Add(frame);
                    }
                }
                file.Clips.Add(clip);
            }

            return file;
        }

        private static string ReadString(BinaryReader br)
        {
            uint len = br.ReadUInt32();
            byte[] bytes = br.ReadBytes((int)len);
            return Encoding.UTF8.GetString(bytes);
        }

        private static Matrix4x4 ReadMatrix(BinaryReader br)
        {
            return new Matrix4x4(
                br.ReadSingle(), br.ReadSingle(), br.ReadSingle(), br.ReadSingle(),
                br.ReadSingle(), br.ReadSingle(), br.ReadSingle(), br.ReadSingle(),
                br.ReadSingle(), br.ReadSingle(), br.ReadSingle(), br.ReadSingle(),
                br.ReadSingle(), br.ReadSingle(), br.ReadSingle(), br.ReadSingle()
            );
        }
    }
}
