using System;
using System.IO;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace Foca3DEditor
{
    public static class TexturePreviewHelper
    {
        public static BitmapSource? LoadTexture(string path)
        {
            if (!File.Exists(path)) return null;
            string ext = Path.GetExtension(path).ToLower();

            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")
            {
                var bmp = new BitmapImage();
                bmp.BeginInit();
                bmp.CacheOption = BitmapCacheOption.OnLoad;
                bmp.UriSource = new Uri(path);
                bmp.EndInit();
                return bmp;
            }

            try
            {
                using var fs = new FileStream(path, FileMode.Open, FileAccess.Read);
                using var br = new BinaryReader(fs);

                if (ext == ".tga") return LoadTga(br);
                if (ext == ".rgb") return LoadRgb(br);
                if (ext == ".ctx" || ext == ".ctxe") return LoadCtx(br);
                
                // For raw formats without header, assume square texture
                long size = fs.Length;
                if (size > 0)
                {
                    if (ext == ".a8" || ext == ".l8") return LoadRaw8(br, size, ext == ".a8", path);
                    if (ext == ".a4" || ext == ".l4") return LoadRaw4(br, size, ext == ".a4", path);
                    if (ext == ".la4") return LoadRawLA4(br, size, path);
                    if (ext == ".la8") return LoadRawLA8(br, size, path);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Debug - Error loading {ext}: {ex.Message}\n{ex.StackTrace}", "Debug Preview Error");
            }
            return null;
        }

        private static BitmapSource? LoadTga(BinaryReader br)
        {
            if (br.BaseStream.Length < 18) return null;

            byte idLength = br.ReadByte();
            byte colorMapType = br.ReadByte();
            byte imageType = br.ReadByte();
            br.BaseStream.Seek(5, SeekOrigin.Current);
            ushort xOrigin = br.ReadUInt16();
            ushort yOrigin = br.ReadUInt16();
            int width = br.ReadUInt16();
            int height = br.ReadUInt16();
            byte pixelDepth = br.ReadByte();
            byte imageDescriptor = br.ReadByte();

            br.BaseStream.Seek(idLength, SeekOrigin.Current);

            if (imageType != 2 && imageType != 10) return null;
            if (pixelDepth != 24 && pixelDepth != 32) return null;

            bool isRle = (imageType == 10);
            int bytesPerPixel = pixelDepth / 8;
            int stride = width * 4;
            byte[] pixels = new byte[width * height * 4];

            Action<int> writePixel = (idx) =>
            {
                byte b = br.ReadByte();
                byte g = br.ReadByte();
                byte r = br.ReadByte();
                if (bytesPerPixel == 4) br.ReadByte(); // Ignore TGA alpha to ensure visibility
                
                int outIdx = idx * 4;
                pixels[outIdx] = b;
                pixels[outIdx + 1] = g;
                pixels[outIdx + 2] = r;
                pixels[outIdx + 3] = 255; // Force opaque
            };

            if (!isRle)
            {
                for (int i = 0; i < width * height; i++) writePixel(i);
            }
            else
            {
                int currentPixel = 0;
                while (currentPixel < width * height)
                {
                    byte header = br.ReadByte();
                    int count = (header & 0x7F) + 1;
                    if ((header & 0x80) != 0)
                    {
                        byte b = br.ReadByte();
                        byte g = br.ReadByte();
                        byte r = br.ReadByte();
                        if (bytesPerPixel == 4) br.ReadByte();

                        for (int i = 0; i < count; i++)
                        {
                            int outIdx = currentPixel * 4;
                            pixels[outIdx] = b;
                            pixels[outIdx + 1] = g;
                            pixels[outIdx + 2] = r;
                            pixels[outIdx + 3] = 255;
                            currentPixel++;
                        }
                    }
                    else
                    {
                        for (int i = 0; i < count; i++) writePixel(currentPixel++);
                    }
                }
            }

            bool flipY = (imageDescriptor & 0x20) == 0;
            if (flipY)
            {
                byte[] flippedPixels = new byte[pixels.Length];
                for (int y = 0; y < height; y++)
                {
                    Array.Copy(pixels, (height - 1 - y) * stride, flippedPixels, y * stride, stride);
                }
                pixels = flippedPixels;
            }

            return BitmapSource.Create(width, height, 96, 96, PixelFormats.Bgra32, null, pixels, stride);
        }

        private static BitmapSource? LoadRgb(BinaryReader br)
        {
            if (br.BaseStream.Length < 16) return null;
            if (new string(br.ReadChars(4)) != "RGB ") return null;
            uint type = br.ReadUInt32();
            int width = (int)br.ReadUInt32();
            int height = (int)br.ReadUInt32();

            byte[] pixels = new byte[width * height * 4];
            for (int i = 0; i < width * height; i++)
            {
                if (br.BaseStream.Position >= br.BaseStream.Length) break;
                
                if (type == 565)
                {
                    if (br.BaseStream.Position >= br.BaseStream.Length - 1) break;
                    ushort val = br.ReadUInt16();
                    pixels[i * 4 + 0] = (byte)((val & 0x1F) << 3); // B
                    pixels[i * 4 + 1] = (byte)(((val >> 5) & 0x3F) << 2); // G
                    pixels[i * 4 + 2] = (byte)(((val >> 11) & 0x1F) << 3); // R
                    pixels[i * 4 + 3] = 255;
                }
                else if (type == 888)
                {
                    if (br.BaseStream.Position >= br.BaseStream.Length - 2) break;
                    pixels[i * 4 + 2] = br.ReadByte(); // R
                    pixels[i * 4 + 1] = br.ReadByte(); // G
                    pixels[i * 4 + 0] = br.ReadByte(); // B
                    pixels[i * 4 + 3] = 255;
                }
                else if (type == 8888)
                {
                    if (br.BaseStream.Position >= br.BaseStream.Length - 3) break;
                    pixels[i * 4 + 2] = br.ReadByte(); // R
                    pixels[i * 4 + 1] = br.ReadByte(); // G
                    pixels[i * 4 + 0] = br.ReadByte(); // B
                    pixels[i * 4 + 3] = br.ReadByte(); // A
                }
            }
            return BitmapSource.Create(width, height, 96, 96, PixelFormats.Bgra32, null, FlipY(pixels, width, height), width * 4);
        }

        private static BitmapSource? LoadCtx(BinaryReader br)
        {
            if (br.BaseStream.Length < 20) return null;
            if (new string(br.ReadChars(4)) != "CTX ") return null;
            br.ReadUInt32(); // EntryCount
            int width = (int)br.ReadUInt32();
            int height = (int)br.ReadUInt32();
            uint flags = br.ReadUInt32(); // 0 = BC1, 1 = BC2
            
            br.ReadUInt32(); // Offset
            br.ReadUInt32(); // Size

            byte[] pixels = new byte[width * height * 4];
            bool isBC2 = flags == 1;
            
            int blockCountX = (width + 3) / 4;
            int blockCountY = (height + 3) / 4;

            for (int by = 0; by < blockCountY; by++)
            {
                for (int bx = 0; bx < blockCountX; bx++)
                {
                    ulong alphaBlock = 0;
                    if (isBC2) alphaBlock = br.ReadUInt64();

                    ushort c0 = br.ReadUInt16();
                    ushort c1 = br.ReadUInt16();
                    uint indices = br.ReadUInt32();

                    // Decode RGB565 colors
                    int r0 = ((c0 >> 11) & 0x1F) * 255 / 31;
                    int g0 = ((c0 >> 5) & 0x3F) * 255 / 63;
                    int b0 = (c0 & 0x1F) * 255 / 31;

                    int r1 = ((c1 >> 11) & 0x1F) * 255 / 31;
                    int g1 = ((c1 >> 5) & 0x3F) * 255 / 63;
                    int b1 = (c1 & 0x1F) * 255 / 31;

                    for (int y = 0; y < 4; y++)
                    {
                        for (int x = 0; x < 4; x++)
                        {
                            int px = bx * 4 + x;
                            int py = by * 4 + y;
                            if (px >= width || py >= height) continue;

                            int bitOffset = (y * 4 + x) * 2;
                            int code = (int)((indices >> bitOffset) & 3);
                            
                            int r = 0, g = 0, b = 0, a = 255;
                            
                            if (code == 0) { r = r0; g = g0; b = b0; }
                            else if (code == 1) { r = r1; g = g1; b = b1; }
                            else if (code == 2)
                            {
                                if (c0 > c1 || isBC2) { r = (2 * r0 + r1) / 3; g = (2 * g0 + g1) / 3; b = (2 * b0 + b1) / 3; }
                                else { r = (r0 + r1) / 2; g = (g0 + g1) / 2; b = (b0 + b1) / 2; }
                            }
                            else if (code == 3)
                            {
                                if (c0 > c1 || isBC2) { r = (r0 + 2 * r1) / 3; g = (g0 + 2 * g1) / 3; b = (b0 + 2 * b1) / 3; }
                                else { r = 0; g = 0; b = 0; a = 0; } // Transparent black in BC1
                            }

                            if (isBC2)
                            {
                                int alphaShift = (y * 4 + x) * 4;
                                int alpha4 = (int)((alphaBlock >> alphaShift) & 0xF);
                                a = alpha4 * 255 / 15;
                            }

                            int outIdx = (py * width + px) * 4;
                            pixels[outIdx + 0] = (byte)b;
                            pixels[outIdx + 1] = (byte)g;
                            pixels[outIdx + 2] = (byte)r;
                            pixels[outIdx + 3] = (byte)a;
                        }
                    }
                }
            }
            
            return BitmapSource.Create(width, height, 96, 96, PixelFormats.Bgra32, null, FlipY(pixels, width, height), width * 4);
        }

        private static BitmapSource? LoadRaw8(BinaryReader br, long size, bool isAlpha, string path)
        {
            (int width, int height) = GetRawDimensions(path, size);
            if (width <= 0 || height <= 0) return null;

            byte[] pixels = new byte[width * height * 4];
            for (int i = 0; i < size; i++)
            {
                if (br.BaseStream.Position >= br.BaseStream.Length) break;
                if (i * 4 + 3 >= pixels.Length) break;

                byte val = br.ReadByte();
                pixels[i * 4 + 0] = isAlpha ? (byte)0 : val;
                pixels[i * 4 + 1] = isAlpha ? (byte)0 : val;
                pixels[i * 4 + 2] = isAlpha ? (byte)0 : val;
                pixels[i * 4 + 3] = isAlpha ? val : (byte)255;
            }
            return BitmapSource.Create(width, height, 96, 96, PixelFormats.Bgra32, null, FlipY(pixels, width, height), width * 4);
        }

        private static BitmapSource? LoadRaw4(BinaryReader br, long size, bool isAlpha, string path)
        {
            if (size > long.MaxValue / 2) return null;
            (int width, int height) = GetRawDimensions(path, size * 2);
            if (width <= 0 || height <= 0) return null;

            byte[] pixels = new byte[width * height * 4];
            for (int i = 0; i < size; i++)
            {
                if (br.BaseStream.Position >= br.BaseStream.Length) break;

                byte val = br.ReadByte();
                byte p1 = (byte)((val & 0x0F) * 17);
                byte p2 = (byte)(((val >> 4) & 0x0F) * 17);
                
                if ((i * 2) * 4 + 3 < pixels.Length)
                {
                    pixels[(i * 2) * 4 + 0] = isAlpha ? (byte)0 : p1;
                    pixels[(i * 2) * 4 + 1] = isAlpha ? (byte)0 : p1;
                    pixels[(i * 2) * 4 + 2] = isAlpha ? (byte)0 : p1;
                    pixels[(i * 2) * 4 + 3] = isAlpha ? p1 : (byte)255;
                }

                if ((i * 2 + 1) * 4 + 3 < pixels.Length)
                {
                    pixels[(i * 2 + 1) * 4 + 0] = isAlpha ? (byte)0 : p2;
                    pixels[(i * 2 + 1) * 4 + 1] = isAlpha ? (byte)0 : p2;
                    pixels[(i * 2 + 1) * 4 + 2] = isAlpha ? (byte)0 : p2;
                    pixels[(i * 2 + 1) * 4 + 3] = isAlpha ? p2 : (byte)255;
                }
            }
            return BitmapSource.Create(width, height, 96, 96, PixelFormats.Bgra32, null, FlipY(pixels, width, height), width * 4);
        }

        private static BitmapSource? LoadRawLA8(BinaryReader br, long size, string path)
        {
            if ((size & 1) != 0) return null;
            (int width, int height) = GetRawDimensions(path, size / 2);
            if (width <= 0 || height <= 0) return null;

            byte[] pixels = new byte[width * height * 4];
            for (int i = 0; i < width * height; i++)
            {
                if (br.BaseStream.Position >= br.BaseStream.Length - 1) break;
                if (i * 4 + 3 >= pixels.Length) break;

                byte l = br.ReadByte();
                byte a = br.ReadByte();
                pixels[i * 4 + 0] = l;
                pixels[i * 4 + 1] = l;
                pixels[i * 4 + 2] = l;
                pixels[i * 4 + 3] = a;
            }
            return BitmapSource.Create(width, height, 96, 96, PixelFormats.Bgra32, null, FlipY(pixels, width, height), width * 4);
        }

        private static BitmapSource? LoadRawLA4(BinaryReader br, long size, string path)
        {
            (int width, int height) = GetRawDimensions(path, size);
            if (width <= 0 || height <= 0) return null;

            byte[] pixels = new byte[width * height * 4];
            for (int i = 0; i < width * height; i++)
            {
                if (br.BaseStream.Position >= br.BaseStream.Length) break;
                if (i * 4 + 3 >= pixels.Length) break;

                byte val = br.ReadByte();
                byte l = (byte)((val & 0x0F) * 17);
                byte a = (byte)(((val >> 4) & 0x0F) * 17);
                pixels[i * 4 + 0] = l;
                pixels[i * 4 + 1] = l;
                pixels[i * 4 + 2] = l;
                pixels[i * 4 + 3] = a;
            }
            return BitmapSource.Create(width, height, 96, 96, PixelFormats.Bgra32, null, FlipY(pixels, width, height), width * 4);
        }

        private static (int Width, int Height) GetRawDimensions(string rawPath, long pixelCount)
        {
            if (pixelCount <= 0 || pixelCount > int.MaxValue / 4)
                return (0, 0);

            if (TryFindSourceTgaDimensions(rawPath, pixelCount, out int sourceWidth, out int sourceHeight))
                return (sourceWidth, sourceHeight);

            int width = GuessWidth(pixelCount);
            if (width <= 0 || pixelCount % width != 0)
                return (0, 0);
            return (width, (int)(pixelCount / width));
        }

        private static bool TryFindSourceTgaDimensions(string rawPath, long pixelCount, out int width, out int height)
        {
            width = 0;
            height = 0;
            string baseName = Path.GetFileNameWithoutExtension(rawPath);
            string? directoryPath = Path.GetDirectoryName(Path.GetFullPath(rawPath));
            if (string.IsNullOrEmpty(directoryPath)) return false;

            for (DirectoryInfo? directory = new DirectoryInfo(directoryPath); directory != null; directory = directory.Parent)
            {
                string candidate = Path.Combine(directory.FullName, baseName + ".tga");
                if (!File.Exists(candidate)) continue;

                try
                {
                    using var stream = new FileStream(candidate, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
                    using var reader = new BinaryReader(stream);
                    if (stream.Length < 18) continue;

                    stream.Position = 1;
                    byte colorMapType = reader.ReadByte();
                    byte imageType = reader.ReadByte();
                    stream.Position = 12;
                    int candidateWidth = reader.ReadUInt16();
                    int candidateHeight = reader.ReadUInt16();
                    byte pixelDepth = reader.ReadByte();

                    if (colorMapType == 0 && (imageType == 2 || imageType == 10) &&
                        (pixelDepth == 24 || pixelDepth == 32) && candidateWidth > 0 && candidateHeight > 0 &&
                        (long)candidateWidth * candidateHeight == pixelCount)
                    {
                        width = candidateWidth;
                        height = candidateHeight;
                        return true;
                    }
                }
                catch (IOException)
                {
                    // Si la TGA asociada no se puede leer, intentar en el siguiente directorio superior.
                }
                catch (UnauthorizedAccessException)
                {
                    // Si la TGA asociada no se puede leer, intentar en el siguiente directorio superior.
                }
            }

            return false;
        }

        private static int GuessWidth(long pixels)
        {
            if (pixels <= 0) return 0;
            
            int w = (int)Math.Sqrt(pixels);
            if (Math.Abs(w * w - pixels) <= 1) return w; // Square 1:1
            
            int h2 = (int)Math.Sqrt(pixels / 2);
            if (Math.Abs(h2 * (h2 * 2) - pixels) <= 1) return h2 * 2; // 2:1

            int w2 = (int)Math.Sqrt(pixels / 2);
            if (Math.Abs(w2 * (w2 * 2) - pixels) <= 1) return w2; // 1:2

            int h4 = (int)Math.Sqrt(pixels / 4);
            if (Math.Abs(h4 * (h4 * 4) - pixels) <= 1) return h4 * 4; // 4:1

            int w4 = (int)Math.Sqrt(pixels / 4);
            if (Math.Abs(w4 * (w4 * 4) - pixels) <= 1) return w4; // 1:4

            // Fallback: If it's a huge texture that failed aspect checks, just force the square root
            // otherwise a 1D image of > 16384 pixels will crash WPF hardware rendering.
            return w > 0 ? w : (int)pixels;
        }

        private static byte[] FlipY(byte[] pixels, int width, int height)
        {
            int stride = width * 4;
            byte[] flipped = new byte[pixels.Length];
            for (int y = 0; y < height; y++)
            {
                Array.Copy(pixels, y * stride, flipped, (height - 1 - y) * stride, stride);
            }
            return flipped;
        }
    }
}
