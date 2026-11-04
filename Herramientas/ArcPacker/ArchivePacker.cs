using System.Globalization;
using System.Text;
using System.Buffers.Binary;
namespace ArcPacker
{
    public class ArchiveEntry
    {
        public string FileName { get; set; } = string.Empty;
        public uint Id { get; set; }
        public uint[] Args { get; set; } = new uint[4];
        public uint Offset { get; set; }
        public uint CompressedSize { get; set; }
        public uint OriginalSize { get; set; }
        public byte[] CompressedData { get; set; } = Array.Empty<byte>();
        public ushort NameOffset { get; set; }
        public uint NameHash { get; set; }
        public bool IsCompressed { get; set; }
    }
    public static class ArchivePacker
    {
        private const ushort NeonVersion = 0x0104;
        private const int NeonHeaderSize = 16;
        private const int NeonEntrySize = 14;
        private static readonly byte[] NeonMagic = { (byte)'N', (byte)'E', (byte)'O', (byte)'N' };
        private static readonly byte[] ArcPackerMetadataMagic = { (byte)'F', (byte)'P', (byte)'K', (byte)'1' };
        private const uint ArcPackerMetadataVersion = 1;

        /// <summary>
        ///   Columna 1: Nombre de archivo (ej. focatest.bin)
        ///   Columna 2: ID (ej. 0)
        ///   Columnas 3-6: 4 argumentos adicionales u32 (ej. 0, 0, 0, 0)
        /// </summary>
        public static List<ArchiveEntry> ParseCsv(string csvPath)
        {
            if (!File.Exists(csvPath))
            {
                throw new FileNotFoundException($"No se encontro el archivo CSV: {csvPath}");
            }
            var entries = new List<ArchiveEntry>();
            string[] lines = ReadAllLinesShared(csvPath);
            for (int lineNum = 0; lineNum < lines.Length; lineNum++)
            {
                string line = lines[lineNum].Trim();
                if (string.IsNullOrWhiteSpace(line) || line.StartsWith("#") || line.StartsWith("//"))
                {
                    continue;
                }
                char separator = ',';
                if (line.Contains(';')) separator = ';';
                else if (line.Contains('\t')) separator = '\t';
                else if (line.Contains(',')) separator = ',';
                string[] parts = line.Split(separator);
                if (parts.Length < 2)
                {
                    string[] spaceParts = line.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
                    if (spaceParts.Length >= 2)
                    {
                        parts = spaceParts;
                    }
                    else
                    {
                        Console.WriteLine($"[AVISO] Linea {lineNum + 1} ignorada (formato invalido): '{line}'");
                        continue;
                    }
                }
                string fileName = parts[0].Trim().Trim('"', '\'');
                string idStr = parts[1].Trim().Trim('"', '\'');
                if (!uint.TryParse(idStr, out uint id))
                {
                    if (lineNum == 0)
                    {
                        continue;
                    }
                    else
                    {
                        throw new FormatException($"Error de formato en la linea {lineNum + 1} del CSV: '{line}'. El segundo argumento debe ser un ID numerico.");
                    }
                }
                uint[] extraArgs = new uint[4];
                for (int a = 0; a < 4; a++)
                {
                    int colIdx = 2 + a;
                    if (colIdx < parts.Length)
                    {
                        string argStr = parts[colIdx].Trim().Trim('"', '\'');
                        if (string.IsNullOrWhiteSpace(argStr))
                        {
                            extraArgs[a] = 0;
                        }
                        else if (uint.TryParse(argStr, NumberStyles.Integer, CultureInfo.InvariantCulture, out uint val))
                        {
                            extraArgs[a] = val;
                        }
                        else if (argStr.StartsWith("0x", StringComparison.OrdinalIgnoreCase) && uint.TryParse(argStr.Substring(2), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint hexVal))
                        {
                            extraArgs[a] = hexVal;
                        }
                        else
                        {
                            throw new FormatException($"Error en la linea {lineNum + 1} del CSV: '{argStr}' no es un numero u32 valido para el argumento adicional {a + 1}.");
                        }
                    }
                    else
                    {
                        extraArgs[a] = 0; 
                    }
                }
                entries.Add(new ArchiveEntry
                {
                    FileName = fileName,
                    Id = id,
                    Args = extraArgs
                });
            }
            if (entries.Count == 0)
            {
                throw new InvalidOperationException("El archivo CSV no contiene ninguna entrada valida para empaquetar.");
            }
            return entries;
        }
        public static void Pack(string csvPath, string baseDirectory, string outputDatPath)
        {
            outputDatPath = Path.ChangeExtension(outputDatPath, ".dat");
            if (!Directory.Exists(baseDirectory))
            {
                throw new DirectoryNotFoundException($"No se encontro la carpeta de archivos: {baseDirectory}");
            }
            List<ArchiveEntry> entries = ParseCsv(csvPath);
            if (entries.Count > ushort.MaxValue)
            {
                throw new InvalidOperationException("NEON permite como maximo 65535 entradas por archivo.");
            }
            for (int i = 0; i < entries.Count; i++)
            {
                ArchiveEntry entry = entries[i];
                if (entry.Id != i || entry.Args.Any(argument => argument != 0))
                {
                    Console.WriteLine($"[AVISO] NEON no serializa IDs ni argumentos CSV. '{entry.FileName}' quedara como ID {i} y argumentos 0.");
                }
            }
            Console.WriteLine($"Se encontraron {entries.Count} entradas en el CSV.");
            long totalOriginalBytes = 0;
            long totalCompressedBytes = 0;
            for (int i = 0; i < entries.Count; i++)
            {
                var entry = entries[i];
                string fullPath = Path.Combine(baseDirectory, entry.FileName);
                if (!File.Exists(fullPath))
                {
                    throw new FileNotFoundException($"El archivo a empaquetar no existe: '{fullPath}' (Entrada ID: {entry.Id})");
                }
                byte[] rawData = ReadAllBytesShared(fullPath);
                if (rawData.LongLength > uint.MaxValue)
                {
                    throw new InvalidOperationException($"El archivo '{entry.FileName}' supera el limite de 4 GiB.");
                }
                entry.OriginalSize = (uint)rawData.Length;
                totalOriginalBytes += rawData.Length;
                string argsStr = $"[{entry.Args[0]}, {entry.Args[1]}, {entry.Args[2]}, {entry.Args[3]}]";
                Console.Write($"[{i + 1}/{entries.Count}] Comprimiendo '{entry.FileName}' (ID: {entry.Id}, Args: {argsStr}, {entry.OriginalSize:N0} bytes)... ");
                byte[]? compressedData = null;
                try
                {
                    compressedData = Compression.Compress(rawData);
                    byte[] roundTrip = Compression.Decompress(compressedData);
                    entry.IsCompressed = compressedData.Length < rawData.Length && rawData.AsSpan().SequenceEqual(roundTrip);
                }
                catch (InvalidOperationException)
                {
                    entry.IsCompressed = false;
                }
                entry.CompressedData = entry.IsCompressed ? compressedData! : rawData;
                if (entry.CompressedData.LongLength > 0x7FFFFFFF)
                {
                    throw new InvalidOperationException($"El bloque '{entry.FileName}' supera el limite de tamano NEON.");
                }
                entry.CompressedSize = (uint)entry.CompressedData.Length;
                totalCompressedBytes += entry.CompressedSize;
                double ratio = rawData.Length > 0 ? (double)entry.CompressedSize / rawData.Length * 100.0 : 0.0;
                string compressionStatus = entry.IsCompressed ? $"{ratio:F1}%" : "sin compresion";
                Console.WriteLine($"OK -> {entry.CompressedSize:N0} bytes ({compressionStatus})");
            }
            byte[] metadata = BuildNeonMetadata(entries);
            ulong tableAndMetadataSize = (ulong)NeonHeaderSize + (ulong)entries.Count * NeonEntrySize + (ulong)metadata.Length;
            if (tableAndMetadataSize > uint.MaxValue)
            {
                throw new InvalidOperationException("Los metadatos del archivo NEON superan el limite de 4 GiB.");
            }
            uint metadataSize = (uint)tableAndMetadataSize;
            ulong currentOffset = metadataSize;
            foreach (ArchiveEntry entry in entries)
            {
                currentOffset = (currentOffset + 3) & ~3UL;
                if (currentOffset > 0x7FFFFFFF || currentOffset + entry.CompressedSize > 0x7FFFFFFF)
                {
                    throw new InvalidOperationException("El archivo NEON supera el limite de offsets de 2 GiB.");
                }
                entry.Offset = (uint)currentOffset;
                currentOffset += entry.CompressedSize;
            }
            string? outDir = Path.GetDirectoryName(outputDatPath);
            if (!string.IsNullOrEmpty(outDir) && !Directory.Exists(outDir))
            {
                Directory.CreateDirectory(outDir);
            }
            using (var fs = new FileStream(outputDatPath, FileMode.Create, FileAccess.Write, FileShare.None))
            using (var bw = new BinaryWriter(fs))
            {
                bw.Write(NeonMagic);
                bw.Write(NeonVersion);
                bw.Write((ushort)entries.Count);
                bw.Write(metadataSize);
                bw.Write((ushort)entries.Count);
                bw.Write((ushort)1);
                for (int i = 0; i < entries.Count; i++)
                {
                    var entry = entries[i];
                    bw.Write(entry.Offset | 0x80000000u);
                    bw.Write(entry.CompressedSize);
                    bw.Write(entry.NameHash);
                    bw.Write(entry.NameOffset);
                }
                bw.Write(metadata);
                for (int i = 0; i < entries.Count; i++)
                {
                    var entry = entries[i];
                    while (fs.Position < entry.Offset)
                    {
                        bw.Write((byte)0x00);
                    }
                    bw.Write(entry.CompressedData);
                }
            }

            FileInfo outInfo = new FileInfo(outputDatPath);
            Console.WriteLine();
            Console.WriteLine($"Correcto: NEON .dat, {entries.Count} entradas, {totalOriginalBytes:N0} -> {totalCompressedBytes:N0} bytes.");
        }
        private static byte[] BuildNeonMetadata(List<ArchiveEntry> entries)
        {
            using var nameStream = new MemoryStream();
            using (var nameWriter = new BinaryWriter(nameStream, Encoding.ASCII, leaveOpen: true))
            {
                foreach (ArchiveEntry entry in entries)
                {
                    if (entry.FileName.IndexOf('\0') >= 0 || entry.FileName.Any(character => character > 0x7F))
                    {
                        throw new InvalidOperationException($"El nombre '{entry.FileName}' no se puede representar como texto ASCII en NEON.");
                    }
                    if (nameStream.Position > ushort.MaxValue)
                    {
                        throw new InvalidOperationException("La tabla de nombres NEON supera los offsets de 16 bits.");
                    }
                    entry.NameOffset = (ushort)nameStream.Position;
                    byte[] nameBytes = Encoding.ASCII.GetBytes(entry.FileName);
                    nameWriter.Write(nameBytes);
                    nameWriter.Write((byte)0);
                    entry.NameHash = ComputeNameHash(nameBytes);
                }
            }
            if (nameStream.Length > ushort.MaxValue + 1L)
            {
                throw new InvalidOperationException("La tabla de nombres NEON supera los 64 KiB.");
            }
            using var metadataStream = new MemoryStream();
            using (var writer = new BinaryWriter(metadataStream, Encoding.ASCII, leaveOpen: true))
            {
                nameStream.Position = 0;
                nameStream.CopyTo(metadataStream);
                long dataStart = NeonHeaderSize + (long)entries.Count * NeonEntrySize;
                while (((dataStart + metadataStream.Position) & 3) != 0)
                {
                    writer.Write((byte)0);
                }
            }
            return metadataStream.ToArray();
        }
        private static uint ComputeNameHash(byte[] name)
        {
            uint hash = 0xFFFFFFFF;
            foreach (byte value in name)
            {
                hash ^= value;
                for (int bit = 0; bit < 8; bit++)
                {
                    hash = (hash >> 1) ^ ((hash & 1) != 0 ? 0xEDB88320u : 0u);
                }
            }
            return ~hash;
        }
        public static void Unpack(string datPath, string outputDirectory)
        {
            if (!File.Exists(datPath))
            {
                throw new FileNotFoundException($"No se encontro el archivo .dat: {datPath}");
            }
            using (var probe = new FileStream(datPath, FileMode.Open, FileAccess.Read, FileShare.Read))
            {
                byte[] magic = new byte[4];
                if (probe.Position == magic.Length && magic.AsSpan().SequenceEqual(NeonMagic))
                {
                    UnpackNeon(datPath, outputDirectory);
                    return;
                }
            }
            throw new InvalidDataException("Formato .dat desconocido; se esperaba NEON.");
        }
        private static void UnpackNeon(string datPath, string outputDirectory)
        {
            byte[] data = ReadAllBytesShared(datPath);
            if (data.Length < NeonHeaderSize)
            {
                throw new InvalidOperationException("El archivo NEON esta incompleto.");
            }
            ushort version = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(4, 2));
            ushort entryCount = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(6, 2));
            uint metadataSize = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(8, 4));
            long tableEndLong = NeonHeaderSize + (long)entryCount * NeonEntrySize;
            if (version != NeonVersion || tableEndLong > metadataSize || metadataSize > data.Length)
            {
                throw new InvalidOperationException("La cabecera o la tabla de entradas NEON no es valida.");
            }
            int tableEnd = (int)tableEndLong;
            int metadataEnd = (int)metadataSize;
            long extensionSize = 12L + (long)entryCount * 28;
            bool hasMetadata = false;
            int namesEnd = metadataEnd;
            if (extensionSize <= metadataEnd - tableEnd)
            {
                int extensionStart = metadataEnd - (int)extensionSize;
                if (data.AsSpan(extensionStart, 4).SequenceEqual(ArcPackerMetadataMagic) && BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(extensionStart + 4, 4)) == ArcPackerMetadataVersion && BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(extensionStart + 8, 4)) == entryCount)
                {
                    hasMetadata = true;
                    namesEnd = extensionStart;
                }
            }
            var entries = new List<ArchiveEntry>(entryCount);
            for (int i = 0; i < entryCount; i++)
            {
                int tablePosition = NeonHeaderSize + i * NeonEntrySize;
                uint rawOffset = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(tablePosition, 4));
                uint rawSize = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(tablePosition + 4, 4));
                ushort nameOffset = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(tablePosition + 12, 2));
                uint offset = rawOffset & 0x7FFFFFFF;
                uint storedSize = rawSize & 0x7FFFFFFF;
                long nameStartLong = (long)tableEnd + nameOffset;
                if (nameStartLong < tableEnd || nameStartLong >= namesEnd || (ulong)offset + storedSize > (ulong)data.Length)
                {
                    throw new InvalidOperationException($"La entrada NEON {i} contiene offsets fuera del archivo.");
                }
                int nameStart = (int)nameStartLong;
                int nameEnd = Array.IndexOf(data, (byte)0, nameStart, namesEnd - nameStart);
                if (nameEnd < 0)
                {
                    throw new InvalidOperationException($"El nombre de la entrada NEON {i} no termina en cero.");
                }
                var entry = new ArchiveEntry
                {
                    FileName = Encoding.ASCII.GetString(data, nameStart, nameEnd - nameStart),
                    Id = (uint)i,
                    Offset = offset,
                    CompressedSize = storedSize,
                    OriginalSize = storedSize,
                    CompressedData = new byte[checked((int)storedSize)]
                };
                Array.Copy(data, (int)offset, entry.CompressedData, 0, (int)storedSize);
                if (hasMetadata)
                {
                    int customPosition = metadataEnd - (int)extensionSize + 12 + i * 28;
                    entry.Id = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(customPosition, 4));
                    for (int argument = 0; argument < 4; argument++)
                    {
                        entry.Args[argument] = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(customPosition + 4 + argument * 4, 4));
                    }
                    entry.OriginalSize = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(customPosition + 20, 4));
                    uint compressionFlag = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(customPosition + 24, 4));
                    if (compressionFlag > 1)
                    {
                        throw new InvalidOperationException($"La entrada NEON con ID {entry.Id} tiene un indicador de compresion invalido.");
                    }
                    entry.IsCompressed = compressionFlag == 1;
                }
                else
                {
                    entry.IsCompressed = HasBlzPayload(entry.CompressedData);
                }
                entries.Add(entry);
            }
            if (!Directory.Exists(outputDirectory))
            {
                Directory.CreateDirectory(outputDirectory);
            }
            var usedNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (ArchiveEntry entry in entries)
            {
                byte[] unpacked = entry.IsCompressed ? Compression.Decompress(entry.CompressedData) : entry.CompressedData;
                if (hasMetadata && unpacked.LongLength != entry.OriginalSize)
                {
                    throw new InvalidOperationException($"Tamano incorrecto en la entrada NEON '{entry.FileName}'.");
                }
                string fileName = SanitizeOutputName(entry.FileName, $"entry_{entry.Id}.bin");
                string uniqueName = fileName;
                int suffix = 1;
                while (!usedNames.Add(uniqueName))
                {
                    uniqueName = $"{Path.GetFileNameWithoutExtension(fileName)}__{suffix++}{Path.GetExtension(fileName)}";
                }
                File.WriteAllBytes(Path.Combine(outputDirectory, uniqueName), unpacked);
            }
            Console.WriteLine($"Desempaquetado NEON completado ({entryCount} entradas).");
        }
        private static bool HasBlzPayload(byte[] data)
        {
            for (int appendedSize = 0; appendedSize < 0x20; appendedSize += 4)
            {
                int coreSize = data.Length - appendedSize;
                if (coreSize < 8)
                {
                    continue;
                }
                uint packed = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(coreSize - 8, 4));
                uint extraSize = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(coreSize - 4, 4));
                int headerSize = (int)(packed >> 24);
                uint compressedSize = packed & 0x00FFFFFF;
                if (headerSize < 8 || headerSize > 0x40 || compressedSize < headerSize || compressedSize > coreSize)
                {
                    continue;
                }
                bool validPadding = true;
                for (int i = coreSize - headerSize; i < coreSize - 8; i++)
                {
                    if (data[i] != 0xFF)
                    {
                        validPadding = false;
                        break;
                    }
                }
                if (validPadding && extraSize != 0)
                {
                    return true;
                }
            }
            return false;
        }
        private static string SanitizeOutputName(string name, string fallback)
        {
            string fileName = Path.GetFileName(name);
            var invalidCharacters = Path.GetInvalidFileNameChars();
            var characters = fileName.ToCharArray();
            for (int i = 0; i < characters.Length; i++)
            {
                if (invalidCharacters.Contains(characters[i]) || char.IsControl(characters[i]))
                {
                    characters[i] = '_';
                }
            }
            fileName = new string(characters).TrimEnd('.', ' ');
            return string.IsNullOrWhiteSpace(fileName) ? fallback : fileName;
        }
        private static string[] ReadAllLinesShared(string path)
        {
            using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            using var reader = new StreamReader(fs, Encoding.UTF8);
            var lines = new List<string>();
            string? line;
            while ((line = reader.ReadLine()) != null)
            {
                lines.Add(line);
            }
            return lines.ToArray();
        }
        private static byte[] ReadAllBytesShared(string path)
        {
            using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            using var ms = new MemoryStream();
            fs.CopyTo(ms);
            return ms.ToArray();
        }
    }
}