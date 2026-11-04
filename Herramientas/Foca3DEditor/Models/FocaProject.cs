using System.Collections.Generic;

namespace Foca3DEditor.Models
{
    public class FocaProject
    {
        public List<string> Models { get; set; } = new();
        public List<TextureEntry> Textures { get; set; } = new();
        public Dictionary<string, string> MaterialAssignments { get; set; } = new();
        public Dictionary<string, bool> ModelIncludeData { get; set; } = new();
    }
}
