using System;
using System.Collections.Generic;
namespace ArcPacker
{
    public static class Compression
    {
        private const int MinMatch = 3;
        private const int MaxMatch = 18;
        private const int MinOffset = 3;
        private const int MaxOffset = 4098;
        public struct Token
        {
            public bool IsMatch;
            public byte Literal;
            public int Length;
            public int Offset;
        }
        public static byte[] Compress(byte[] uncompressed)
        {
            ArgumentNullException.ThrowIfNull(uncompressed);
            int uLen = uncompressed.Length;
            if (uLen == 0)
            {
                throw new InvalidOperationException("No se pueden comprimir datos vacios (0 bytes).");
            }
            byte[] revData = new byte[uLen];
            for (int i = 0; i < uLen; i++)
            {
                revData[i] = uncompressed[uLen - 1 - i];
            }
            List<Token> tokens = new();
            int j = 0;
            int[] head = new int[65536];
            Array.Fill(head, -1);
            int[] prev = new int[uLen];
            while (j < uLen)
            {
                int bestLength = 0;
                int bestOffset = 0;
                if (j >= MinOffset && j + MinMatch <= uLen)
                {
                    int maxLenPossible = Math.Min(MaxMatch, uLen - j);
                    int hash = ((revData[j] << 8) ^ (revData[j + 1] << 4) ^ revData[j + 2]) & 0xFFFF;
                    int candidate = head[hash];
                    int chainLimit = 64;
                    while (candidate != -1 && chainLimit-- > 0)
                    {
                        int offset = j - candidate;
                        if (offset > MaxOffset)
                        {
                            break;
                        }
                        if (offset >= MinOffset)
                        {
                            int matchLen = 0;
                            while (matchLen < maxLenPossible && revData[j + matchLen] == revData[candidate + matchLen])
                            {
                                matchLen++;
                            }
                            if (matchLen >= MinMatch && matchLen > bestLength)
                            {
                                bestLength = matchLen;
                                bestOffset = offset;
                                if (bestLength == MaxMatch)
                                {
                                    break;
                                }
                            }
                        }
                        candidate = prev[candidate];
                    }
                }
                if (bestLength >= MinMatch)
                {
                    tokens.Add(new Token
                    {
                        IsMatch = true,
                        Length = bestLength,
                        Offset = bestOffset
                    });
                    for (int k = 0; k < bestLength; k++)
                    {
                        int pos = j + k;
                        if (pos + 2 < uLen)
                        {
                            int h = ((revData[pos] << 8) ^ (revData[pos + 1] << 4) ^ revData[pos + 2]) & 0xFFFF;
                            prev[pos] = head[h];
                            head[h] = pos;
                        }
                    }
                    j += bestLength;
                }
                else
                {
                    tokens.Add(new Token
                    {
                        IsMatch = false,
                        Literal = revData[j]
                    });
                    if (j + 2 < uLen)
                    {
                        int h = ((revData[j] << 8) ^ (revData[j + 1] << 4) ^ revData[j + 2]) & 0xFFFF;
                        prev[j] = head[h];
                        head[h] = j;
                    }
                    j++;
                }
            }
            List<List<Token>> groups = new();
            for (int t = 0; t < tokens.Count; t += 8)
            {
                int count = Math.Min(8, tokens.Count - t);
                List<Token> group = new(count);
                for (int g = 0; g < count; g++)
                {
                    group.Add(tokens[t + g]);
                }
                groups.Add(group);
            }
            List<byte> lzBytes = new();
            for (int g = groups.Count - 1; g >= 0; g--)
            {
                List<Token> group = groups[g];
                byte flags = 0;
                List<byte> groupTokensData = new();
                for (int idx = 0; idx < group.Count; idx++)
                {
                    if (group[idx].IsMatch)
                    {
                        flags |= (byte)(1 << (7 - idx));
                    }
                }
                for (int idx = group.Count - 1; idx >= 0; idx--)
                {
                    Token token = group[idx];
                    if (token.IsMatch)
                    {
                        int lenParam = token.Length - MinMatch;
                        int offParam = token.Offset - MinOffset;
                        byte b = (byte)(offParam & 0xFF);
                        byte a = (byte)((lenParam << 4) | ((offParam >> 8) & 0x0F));
                        groupTokensData.Add(b);
                        groupTokensData.Add(a);
                    }
                    else
                    {
                        groupTokensData.Add(token.Literal);
                    }
                }
                lzBytes.AddRange(groupTokensData);
                lzBytes.Add(flags);
            }
            int lzLen = lzBytes.Count;
            int pad = (4 - (lzLen % 4)) % 4;
            for (int p = 0; p < pad; p++)
            {
                lzBytes.Add(0xFF);
            }
            int tailSize = 8 + pad;
            int compressedSize = lzBytes.Count + 8;
            int expansionSize = uLen - compressedSize;
            if (expansionSize < 0)
            {
                throw new InvalidOperationException($"El archivo no se pudo comprimir a un tamano menor al original. (Original: {uLen} bytes, Comprimido: {compressedSize} bytes)");
            }
            lzBytes.Add((byte)(compressedSize & 0xFF));
            lzBytes.Add((byte)((compressedSize >> 8) & 0xFF));
            lzBytes.Add((byte)((compressedSize >> 16) & 0xFF));
            lzBytes.Add((byte)tailSize);
            lzBytes.Add((byte)(expansionSize & 0xFF));
            lzBytes.Add((byte)((expansionSize >> 8) & 0xFF));
            lzBytes.Add((byte)((expansionSize >> 16) & 0xFF));
            lzBytes.Add(0x00);
            return lzBytes.ToArray();
        }
        public static byte[] Decompress(byte[] data)
        {
            if (data == null || data.Length < 8)
            {
                throw new ArgumentException("Datos insuficientes para el footer FEV/Reverse LZ77.", nameof(data));
            }
            int p = data.Length - 8;
            int compressedSize = data[p] | (data[p + 1] << 8) | (data[p + 2] << 16);
            int tailSize = data[p + 3];
            int expansionSize = data[p + 4] | (data[p + 5] << 8) | (data[p + 6] << 16);
            int fileSize = data.Length;
            int compressedStart = fileSize - compressedSize;
            int lzEnd = fileSize - tailSize;
            if (compressedSize < 8 || compressedStart < 0 || lzEnd < compressedStart || lzEnd > fileSize)
            {
                throw new InvalidOperationException("Footer Reverse LZ77 invalido.");
            }
            int totalUncompressed = compressedSize + expansionSize;
            byte[] compressed = new byte[compressedSize];
            Array.Copy(data, compressedStart, compressed, 0, compressedSize);
            byte[] output = new byte[totalUncompressed];
            int src = lzEnd - compressedStart;
            int dst = totalUncompressed;
            int start = 0;
            while (src > start)
            {
                src--;
                byte flags = compressed[src];
                for (int i = 0; i < 8; i++)
                {
                    if ((flags & 0x80) != 0)
                    {
                        if (src - 2 < start)
                        {
                            throw new InvalidOperationException("Match truncado en el flujo LZ.");
                        }
                        byte a = compressed[src - 1];
                        src -= 2;
                        byte b = compressed[src];
                        int length = (a >> 4) + MinMatch;
                        int offset = (((a & 0x0F) << 8) | b) + MinMatch;
                        int oldDst = dst;
                        int sourceBase = oldDst + offset;
                        for (int k = 0; k < length; k++)
                        {
                            int dstPos = oldDst - 1 - k;
                            int srcPos = sourceBase - 1 - k;
                            if (dstPos < start || srcPos < dstPos || srcPos >= totalUncompressed)
                            {
                                throw new InvalidOperationException($"Referencia LZ fuera de los limites del buffer: dstPos={dstPos}, start={start}, srcPos={srcPos}, total={totalUncompressed}, offset={offset}, length={length}, k={k}");
                            }
                            output[dstPos] = output[srcPos];
                        }
                        dst -= length;
                    }
                    else
                    {
                        if (src <= start) break;
                        src--;
                        dst--;
                        if (dst < start)
                        {
                            throw new InvalidOperationException("Literal provoco que el destino pasara del inicio del bloque.");
                        }
                        output[dst] = compressed[src];
                    }
                    flags = (byte)((flags << 1) & 0xFF);
                    if (src <= start) break;
                }
            }
            if (src != start || dst != start)
            {
                throw new InvalidOperationException($"La descompresion no llego limpiamente al inicio: src={src}, dst={dst}");
            }
            return output;
        }
    }
}