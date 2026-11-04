using System.Collections.Generic;
using System.IO;
using System.Text;

namespace FocaCollisionEditor
{
    public sealed class FcolFileData
    {
        public string Model { get; set; } = string.Empty;
        public List<CollisionVolume> Collisions { get; set; } = new();
    }

    public static class FcolFile
    {
        private const uint Version = 3;
        private const uint IdVersion = 2;
        private const uint LegacyVersion = 1;
        private const uint MaxCollisionCount = 1_000_000;
        private const uint MaxStringLength = 1_048_576;
        private static readonly byte[] Magic = { (byte)'F', (byte)'C', (byte)'O', (byte)'L' };

        public static FcolFileData Read(string path)
        {
            using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
            using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);
            if (stream.Length < 12 || (stream.Length & 3) != 0)
            {
                throw new InvalidDataException("El archivo .fcol no tiene un tamaño alineado a 4 bytes o está incompleto.");
            }
            for (int i = 0; i < Magic.Length; ++i)
            {
                if (reader.ReadByte() != Magic[i])
                {
                    throw new InvalidDataException("El archivo no tiene la firma FCOL.");
                }
            }

            uint version = reader.ReadUInt32();
            if (version != Version && version != IdVersion && version != LegacyVersion)
            {
                throw new InvalidDataException($"La versión .fcol {version} no es compatible.");
            }
            uint collisionCount = reader.ReadUInt32();
            if (collisionCount > MaxCollisionCount)
            {
                throw new InvalidDataException("El archivo .fcol declara demasiadas colisiones.");
            }

            var data = new FcolFileData();
            if (version == LegacyVersion || version == IdVersion)
            {
                data.Model = ReadAlignedString(reader);
            }
            var usedIds = new HashSet<uint>();
            for (uint i = 0; i < collisionCount; ++i)
            {
                uint id = version == LegacyVersion ? i : reader.ReadUInt32();
                if (!usedIds.Add(id))
                {
                    throw new InvalidDataException($"El archivo .fcol contiene el ID repetido {id}.");
                }
                var collision = new CollisionVolume { Id = id };
                if (version == LegacyVersion)
                {
                    _ = ReadAlignedString(reader);
                }
                uint shape = reader.ReadUInt32();
                uint kind = reader.ReadUInt32();
                if (shape > (uint)CollisionShape.OBB || kind > (uint)CollisionKind.Trigger)
                {
                    throw new InvalidDataException("El archivo .fcol contiene un tipo de colisión desconocido.");
                }
                collision.Shape = (CollisionShape)shape;
                collision.Kind = (CollisionKind)kind;
                collision.CenterX = reader.ReadSingle();
                collision.CenterY = reader.ReadSingle();
                collision.CenterZ = reader.ReadSingle();
                collision.SizeX = reader.ReadSingle();
                collision.SizeY = reader.ReadSingle();
                collision.SizeZ = reader.ReadSingle();
                collision.RotationX = reader.ReadSingle();
                collision.RotationY = reader.ReadSingle();
                collision.RotationZ = reader.ReadSingle();
                if (!IsValid(collision))
                {
                    throw new InvalidDataException($"La colisión con ID {collision.Id} contiene valores no válidos.");
                }
                data.Collisions.Add(collision);
            }
            if (stream.Position != stream.Length)
            {
                throw new InvalidDataException("El archivo .fcol contiene datos sobrantes o un conteo incorrecto.");
            }
            return data;
        }

        public static void Write(string path, IReadOnlyList<CollisionVolume> collisions)
        {
            if (collisions.Count > MaxCollisionCount)
            {
                throw new InvalidDataException("Hay demasiadas colisiones para exportar.");
            }
            foreach (var collision in collisions)
            {
                if (!IsValid(collision))
                {
                    throw new InvalidDataException($"La colisión con ID {collision.Id} contiene valores no válidos.");
                }
            }
            var usedIds = new HashSet<uint>();
            foreach (var collision in collisions)
            {
                if (!usedIds.Add(collision.Id))
                {
                    throw new InvalidDataException($"El ID {collision.Id} está asignado a más de una colisión.");
                }
            }
            using var stream = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None);
            using var writer = new BinaryWriter(stream, Encoding.UTF8, leaveOpen: true);
            writer.Write(Magic);
            writer.Write(Version);
            writer.Write((uint)collisions.Count);
            foreach (var collision in collisions)
            {
                writer.Write(collision.Id);
                writer.Write((uint)collision.Shape);
                writer.Write((uint)collision.Kind);
                writer.Write(collision.CenterX);
                writer.Write(collision.CenterY);
                writer.Write(collision.CenterZ);
                writer.Write(collision.SizeX);
                writer.Write(collision.SizeY);
                writer.Write(collision.SizeZ);
                writer.Write(collision.RotationX);
                writer.Write(collision.RotationY);
                writer.Write(collision.RotationZ);
            }
        }

        private static string ReadAlignedString(BinaryReader reader)
        {
            uint byteCount = reader.ReadUInt32();
            if (byteCount > MaxStringLength || byteCount > reader.BaseStream.Length - reader.BaseStream.Position)
            {
                throw new InvalidDataException("Una cadena del archivo .fcol tiene una longitud no válida.");
            }
            byte[] bytes = reader.ReadBytes((int)byteCount);
            if (bytes.Length != byteCount)
            {
                throw new EndOfStreamException("El archivo .fcol terminó dentro de una cadena.");
            }
            uint padding = (4 - (byteCount & 3)) & 3;
            for (uint i = 0; i < padding; ++i)
            {
                if (reader.ReadByte() != 0)
                {
                    throw new InvalidDataException("El relleno de una cadena .fcol debe ser cero.");
                }
            }
            return new UTF8Encoding(false, true).GetString(bytes);
        }

        private static void WriteAlignedString(BinaryWriter writer, string? value)
        {
            byte[] bytes = Encoding.UTF8.GetBytes(value ?? string.Empty);
            if (bytes.Length > MaxStringLength)
            {
                throw new InvalidDataException("Una cadena supera el tamaño permitido para .fcol.");
            }
            writer.Write((uint)bytes.Length);
            writer.Write(bytes);
            int padding = (4 - (bytes.Length & 3)) & 3;
            for (int i = 0; i < padding; ++i)
            {
                writer.Write((byte)0);
            }
        }

        private static bool IsValid(CollisionVolume collision)
        {
            return collision != null &&
                (uint)collision.Shape <= (uint)CollisionShape.OBB &&
                (uint)collision.Kind <= (uint)CollisionKind.Trigger &&
                float.IsFinite(collision.CenterX) && float.IsFinite(collision.CenterY) && float.IsFinite(collision.CenterZ) &&
                float.IsFinite(collision.SizeX) && float.IsFinite(collision.SizeY) && float.IsFinite(collision.SizeZ) &&
                collision.SizeX > 0.0f && collision.SizeY > 0.0f && collision.SizeZ > 0.0f &&
                float.IsFinite(collision.RotationX) && float.IsFinite(collision.RotationY) && float.IsFinite(collision.RotationZ);
        }
    }
}
