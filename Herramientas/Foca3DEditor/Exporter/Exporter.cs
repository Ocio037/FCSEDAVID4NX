using System;
using System.IO;
using System.Text;
using Foca3DEditor.Models;

namespace Foca3DEditor.Exporter
{
    internal class Exporter
    {
        public void Export(string path, FocaProject project, System.Collections.Generic.List<Fc3dFile> loadedModels)
        {
            using (var fs = new FileStream(path, FileMode.Create, FileAccess.Write))
            using (var bw = new BinaryWriter(fs, Encoding.UTF8))
            {
                // Magic FOCA
                bw.Write(Encoding.ASCII.GetBytes("FOCA"));
                bw.Write((ushort)3); // version 3: texture dimensions for raw formats

                // Write Models
                bw.Write((uint)project.Models.Count);
                for (int i = 0; i < project.Models.Count; i++)
                {
                    string modelPath = project.Models[i];
                    string modelFileName = Path.GetFileName(modelPath);
                    WriteString(bw, modelFileName);
                    
                    var fc3dFile = i < loadedModels.Count ? loadedModels[i] : null;
                    if (fc3dFile == null && File.Exists(modelPath))
                    {
                        try { fc3dFile = Fc3dFile.Read(modelPath); } catch { }
                    }

                    bool includeData = !project.ModelIncludeData.ContainsKey(modelFileName) || project.ModelIncludeData[modelFileName];
                    bw.Write(includeData);
                    
                    if (includeData)
                    {
                        // Pack Materials & Assignments
                        if (fc3dFile != null)
                        {
                            bw.Write((uint)fc3dFile.Materials.Count);
                            foreach (var mat in fc3dFile.Materials)
                            {
                                WriteString(bw, mat.Name);
                                string key = $"{modelFileName}_{mat.Name}";
                                string assignedTexture = project.MaterialAssignments.ContainsKey(key) ? project.MaterialAssignments[key] : "";
                                WriteString(bw, assignedTexture);
                            }
                            
                            // Pack Meshes
                            bw.Write((uint)fc3dFile.Meshes.Count);
                            foreach (var mesh in fc3dFile.Meshes)
                            {
                                WriteString(bw, mesh.Name);
                                bw.Write((uint)mesh.VertexCount);
                                bw.Write((uint)mesh.IndexCount);
                                bw.Write((uint)mesh.MaterialIndex);
                            }
                        }
                        else
                        {
                            bw.Write((uint)0); // 0 materials
                            bw.Write((uint)0); // 0 meshes
                        }

                        // Pack Model Binary Data (.f3d)
                        if (File.Exists(modelPath))
                        {
                            byte[] modelBytes = File.ReadAllBytes(modelPath);
                            bw.Write((uint)modelBytes.Length);
                            bw.Write(modelBytes);
                        }
                        else
                        {
                            bw.Write((uint)0);
                        }
                    }
                }

                // Write Textures
                bw.Write((uint)project.Textures.Count);
                foreach (var tex in project.Textures)
                {
                    string texPath = tex.SourcePath;
                    if (!File.Exists(texPath))
                    {
                        string? dir = Path.GetDirectoryName(path);
                        if (dir != null)
                        {
                            string candidate = Path.Combine(dir, tex.Name);
                            if (File.Exists(candidate)) texPath = candidate;
                        }
                    }

                    uint textureWidth = tex.Width;
                    uint textureHeight = tex.Height;
                    if ((textureWidth == 0 || textureHeight == 0) && File.Exists(texPath))
                    {
                        var preview = Foca3DEditor.TexturePreviewHelper.LoadTexture(texPath);
                        if (preview != null)
                        {
                            textureWidth = (uint)preview.PixelWidth;
                            textureHeight = (uint)preview.PixelHeight;
                        }
                    }

                    WriteString(bw, tex.Name);
                    WriteString(bw, tex.Format);
                    bw.Write(textureWidth);
                    bw.Write(textureHeight);
                    bw.Write(tex.IncludeData);

                    if (tex.IncludeData)
                    {
                        if (File.Exists(texPath))
                        {
                            byte[] data = File.ReadAllBytes(texPath);
                            bw.Write((uint)data.Length);
                            bw.Write(data);
                        }
                        else
                        {
                            bw.Write((uint)0);
                        }
                    }
                }
            }
        }

        private void WriteString(BinaryWriter bw, string str)
        {
            byte[] bytes = Encoding.UTF8.GetBytes(str);
            bw.Write((uint)bytes.Length);
            bw.Write(bytes);
        }
    }
}
