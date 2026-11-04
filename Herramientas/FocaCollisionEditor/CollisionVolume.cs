namespace FocaCollisionEditor
{
    public enum CollisionShape
    {
        AABB,
        OBB
    }

    public enum CollisionKind
    {
        Solid,
        Trigger
    }

    public sealed class CollisionVolume
    {
        public uint Id { get; set; }
        public CollisionShape Shape { get; set; } = CollisionShape.AABB;
        public CollisionKind Kind { get; set; } = CollisionKind.Solid;
        public float CenterX { get; set; }
        public float CenterY { get; set; }
        public float CenterZ { get; set; }
        public float SizeX { get; set; } = 1.0f;
        public float SizeY { get; set; } = 1.0f;
        public float SizeZ { get; set; } = 1.0f;
        public float RotationX { get; set; }
        public float RotationY { get; set; }
        public float RotationZ { get; set; }

        public System.Numerics.Vector3 Center => new(CenterX, CenterY, CenterZ);

        public System.Numerics.Vector3 Size => new(SizeX, SizeY, SizeZ);

        public override string ToString()
        {
            string kind = Kind == CollisionKind.Trigger ? "Trigger" : "Sólida";
            return $"ID {Id}  [{Shape} · {kind}]";
        }
    }

}
