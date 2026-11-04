#include "LocalFc.h"
#include <nvtt/nvtt.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cstring>
#include <stdexcept>
#include <queue>
#include <algorithm>
#pragma comment(lib, "nvtt30205.lib")
struct ImageData 
{
    uint32_t width;
    uint32_t height;
    std::vector<uint8_t> pixels; // RGBA
};
ImageData LoadTga(const std::string& path) 
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open TGA file");
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) 
    {
        throw std::runtime_error("Cannot read TGA file");
    }
    if (size < 18) throw std::runtime_error("TGA too small");
    uint8_t idLength = buffer[0];
    uint8_t colorMapType = buffer[1];
    uint8_t imageType = buffer[2];
    uint16_t width = buffer[12] | (buffer[13] << 8);
    uint16_t height = buffer[14] | (buffer[15] << 8);
    uint8_t bitsPerPixel = buffer[16];
    uint8_t descriptor = buffer[17];
    if (colorMapType != 0) throw std::runtime_error("Color map not supported");
    if (imageType != 2 && imageType != 10) throw std::runtime_error("Only uncompressed or RLE truecolor supported");
    if (width == 0 || height == 0) throw std::runtime_error("Invalid dimensions");
    if (bitsPerPixel != 24 && bitsPerPixel != 32) throw std::runtime_error("Only 24 or 32 bpp supported");
    int bytesPerPixel = bitsPerPixel / 8;
    int dataOffset = 18 + idLength;
    std::vector<uint8_t> img_data(width * height * bytesPerPixel);
    if (imageType == 2) 
    {
        if (dataOffset + img_data.size() > buffer.size()) throw std::runtime_error("TGA truncated");
        std::memcpy(img_data.data(), &buffer[dataOffset], img_data.size());
    } 
    else 
    {
        int input = dataOffset;
        int output = 0;
        int pixelCount = width * height;
        int writtenPixels = 0;
        while (writtenPixels < pixelCount) 
        {
            if (input >= (int)buffer.size()) throw std::runtime_error("TGA RLE truncated");
            uint8_t packet = buffer[input++];
            int count = (packet & 0x7F) + 1;
            bool rle = (packet & 0x80) != 0;
            if (writtenPixels + count > pixelCount) throw std::runtime_error("TGA RLE overflow");
            if (rle) 
            {
                if (input + bytesPerPixel > (int)buffer.size()) throw std::runtime_error("TGA RLE pixel truncated");
                for (int i = 0; i < count; i++) 
                {
                    std::memcpy(&img_data[output], &buffer[input], bytesPerPixel);
                    output += bytesPerPixel;
                    writtenPixels++;
                }
                input += bytesPerPixel;
            } 
            else
            {
                int byteCount = count * bytesPerPixel;
                if (input + byteCount > (int)buffer.size()) throw std::runtime_error("TGA RLE raw truncated");
                std::memcpy(&img_data[output], &buffer[input], byteCount);
                input += byteCount;
                output += byteCount;
                writtenPixels += count;
            }
        }
    }
    bool topDown = (descriptor & 0x20) != 0;
    std::vector<uint8_t> rgba(width * height * 4);
    for (int y = 0; y < height; y++) 
    {
        int srcY = topDown ? (height - 1 - y) : y;
        for (int x = 0; x < width; x++) 
        {
            int srcIdx = (srcY * width + x) * bytesPerPixel;
            int dstIdx = (y * width + x) * 4;
            rgba[dstIdx + 0] = img_data[srcIdx + 2];
            rgba[dstIdx + 1] = img_data[srcIdx + 1];
            rgba[dstIdx + 2] = img_data[srcIdx + 0];
            rgba[dstIdx + 3] = (bytesPerPixel == 4) ? img_data[srcIdx + 3] : 255;
        }
    }
    return { width, height, rgba };
}
uint8_t Luminance(uint8_t r, uint8_t g, uint8_t b) 
{
    return (uint8_t)(((r * 299) + (g * 587) + (b * 114) + 500) / 1000);
}
uint8_t To4(uint8_t v) 
{ 
    return (uint8_t)((v * 15 + 127) / 255);
}
std::vector<uint8_t> ConvertA4(const std::vector<uint8_t>& rgba) 
{
    std::vector<uint8_t> out((rgba.size() / 4 + 1) / 2, 0);
    for (size_t i = 0; i < rgba.size() / 4; i++) 
    {
        uint8_t a = To4(rgba[i * 4 + 3]);
        if ((i & 1) == 0) out[i / 2] |= a; else out[i / 2] |= (a << 4);
    }
    return out;
}
std::vector<uint8_t> ConvertA8(const std::vector<uint8_t>& rgba) 
{
    std::vector<uint8_t> out(rgba.size() / 4);
    for (size_t i = 0; i < out.size(); i++) out[i] = rgba[i * 4 + 3];
    return out;
}
std::vector<uint8_t> ConvertL4(const std::vector<uint8_t>& rgba) 
{
    std::vector<uint8_t> out((rgba.size() / 4 + 1) / 2, 0);
    for (size_t i = 0; i < rgba.size() / 4; i++) 
    {
        float alpha = rgba[i * 4 + 3] / 255.0f;
        uint8_t r = (uint8_t)(rgba[i * 4] * alpha);
        uint8_t g = (uint8_t)(rgba[i * 4 + 1] * alpha);
        uint8_t b = (uint8_t)(rgba[i * 4 + 2] * alpha);
        uint8_t l = To4(Luminance(r, g, b));
        if ((i & 1) == 0) out[i / 2] |= l; else out[i / 2] |= (l << 4);
    }
    return out;
}
std::vector<uint8_t> ConvertL8(const std::vector<uint8_t>& rgba) 
{
    std::vector<uint8_t> out(rgba.size() / 4);
    for (size_t i = 0; i < out.size(); i++) 
    {
        float alpha = rgba[i * 4 + 3] / 255.0f;
        uint8_t r = (uint8_t)(rgba[i * 4] * alpha);
        uint8_t g = (uint8_t)(rgba[i * 4 + 1] * alpha);
        uint8_t b = (uint8_t)(rgba[i * 4 + 2] * alpha);
        out[i] = Luminance(r, g, b);
    }
    return out;
}
std::vector<uint8_t> ConvertLA4(const std::vector<uint8_t>& rgba) 
{
    std::vector<uint8_t> out(rgba.size() / 4);
    for (size_t i = 0; i < out.size(); i++) 
    {
        uint8_t l = To4(Luminance(rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2]));
        uint8_t a = To4(rgba[i * 4 + 3]);
        out[i] = l | (a << 4);
    }
    return out;
}
std::vector<uint8_t> ConvertLA8(const std::vector<uint8_t>& rgba) 
{
    std::vector<uint8_t> out(rgba.size() / 2);
    for (size_t i = 0; i < rgba.size() / 4; i++) 
    {
        out[i * 2 + 0] = Luminance(rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2]);
        out[i * 2 + 1] = rgba[i * 4 + 3];
    }
    return out;
}
std::vector<uint8_t> ConvertR5G6B5(const std::vector<uint8_t>& rgba) 
{
    std::vector<uint8_t> out(rgba.size() / 2);
    for (size_t i = 0; i < rgba.size() / 4; i++) 
    {
        uint16_t r = (rgba[i * 4 + 0] >> 3) & 0x1F;
        uint16_t g = (rgba[i * 4 + 1] >> 2) & 0x3F;
        uint16_t b = (rgba[i * 4 + 2] >> 3) & 0x1F;
        uint16_t rgb565 = (r << 11) | (g << 5) | b;
        out[i * 2 + 0] = rgb565 & 0xFF;
        out[i * 2 + 1] = (rgb565 >> 8) & 0xFF;
    }
    return out;
}
std::vector<uint8_t> ConvertR8G8B8(const std::vector<uint8_t>& rgba) 
{
    std::vector<uint8_t> out((rgba.size() / 4) * 3);
    for (size_t i = 0; i < rgba.size() / 4; i++) 
    {
        out[i * 3 + 0] = rgba[i * 4 + 0];
        out[i * 3 + 1] = rgba[i * 4 + 1];
        out[i * 3 + 2] = rgba[i * 4 + 2];
    }
    return out;
}
std::vector<uint8_t> ConvertR8G8B8A8(const std::vector<uint8_t>& rgba) 
{
    return rgba;
}
bool DetectAlpha(const std::vector<uint8_t>& rgba) 
{
    for (size_t i = 3; i < rgba.size(); i += 4) 
    {
        if (rgba[i] != 255) return true;
    }
    return false;
}
std::vector<uint8_t> PrepareRgbaForCompression(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height, bool keepAlpha) 
{
    std::vector<uint8_t> cleaned = rgba;
    if (!keepAlpha) 
    {
        for (size_t i = 3; i < cleaned.size(); i += 4) cleaned[i] = 255;
        return cleaned;
    }
    uint32_t pixelCount = width * height;
    std::vector<int32_t> source(pixelCount, -1);
    std::queue<uint32_t> pending;
    for (uint32_t y = 0; y < height; ++y) 
    {
        for (uint32_t x = 0; x < width; ++x) 
        {
            uint32_t pixelIndex = y * width + x;
            if (cleaned[pixelIndex * 4 + 3] != 0) 
            {
                source[pixelIndex] = static_cast<int32_t>(pixelIndex);
                pending.push(pixelIndex);
            }
        }
    }
    if (pending.empty()) return cleaned;
    int NeighborX[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    int NeighborY[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    while (!pending.empty()) 
    {
        uint32_t current = pending.front();
        pending.pop();
        uint32_t cx = current % width;
        uint32_t cy = current / width;
        int32_t currentSource = source[current];
        for (int n = 0; n < 8; ++n) 
        {
            int nx = static_cast<int>(cx) + NeighborX[n];
            int ny = static_cast<int>(cy) + NeighborY[n];
            if (nx < 0 || nx >= static_cast<int>(width) || ny < 0 || ny >= static_cast<int>(height)) continue;
            uint32_t neighbor = static_cast<uint32_t>(ny) * width + static_cast<uint32_t>(nx);
            if (source[neighbor] != -1) continue;
            source[neighbor] = currentSource;
            pending.push(neighbor);
        }
    }
    for (uint32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) 
    {
        uint32_t byteIndex = pixelIndex * 4;
        if (cleaned[byteIndex + 3] != 0) continue;
        int32_t sourceIndex = source[pixelIndex];
        if (sourceIndex < 0) continue;
        uint32_t sourceByteIndex = static_cast<uint32_t>(sourceIndex) * 4;
        cleaned[byteIndex + 0] = cleaned[sourceByteIndex + 0];
        cleaned[byteIndex + 1] = cleaned[sourceByteIndex + 1];
        cleaned[byteIndex + 2] = cleaned[sourceByteIndex + 2];
    }
    return cleaned;
}
struct MyOutputHandler : public nvtt::OutputHandler 
{
    std::vector<unsigned char> buffer;
    virtual void beginImage(int size, int width, int height, int depth, int face, int miplevel) override 
    {
        buffer.reserve(buffer.size() + std::max(size, 0));
    }
    virtual bool writeData(const void* data, int size) override 
    {
        if (data && size > 0) 
        {
            const unsigned char* bytes = static_cast<const unsigned char*>(data);
            buffer.insert(buffer.end(), bytes, bytes + size);
        }
        return true;
    }
    virtual void endImage() override {}
};
std::vector<uint8_t> CreateBC(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height, bool keepAlpha) 
{
    nvtt::Context context;
    context.enableCudaAcceleration(false);
    std::vector<uint8_t> preparedData = PrepareRgbaForCompression(rgba, width, height, keepAlpha);
    std::vector<uint8_t> bgraData = preparedData;
    for (size_t i = 0; i < bgraData.size(); i += 4) 
    {
        std::swap(bgraData[i], bgraData[i + 2]);
    }
    nvtt::Surface surface;
    if (!surface.setImage(nvtt::InputFormat::InputFormat_BGRA_8UB, width, height, 1, bgraData.data())) 
    {
        return {};
    }
    nvtt::CompressionOptions comp;
    comp.setQuality(nvtt::Quality_Highest);
    comp.setFormat(keepAlpha ? nvtt::Format_BC2 : nvtt::Format_BC1);
    MyOutputHandler handler;
    nvtt::OutputOptions outOpt;
    outOpt.setOutputHandler(&handler);
    outOpt.setOutputHeader(false);
    outOpt.setContainer(nvtt::Container::Container_DDS);
    if (!context.compress(surface, 1, 1, comp, outOpt)) return {};
    return std::move(handler.buffer);
}
#pragma pack(push, 1)
struct TextureEntry 
{
    uint32_t Offset;
    uint32_t Size;
};
struct FatHeader 
{
    uint8_t Firma[4];
    uint32_t EntryCount;
    uint32_t Width;
    uint32_t Height;
    uint32_t Flags;
};
struct RgbHeader 
{
    char Magic[4];
    uint32_t Type;
    uint32_t Width;
    uint32_t Height;
};
#pragma pack(pop)
int main(int argc, char* argv[]) 
{
    if (argc < 3) 
    {
        std::cerr << "Uso: " << argv[0] << " <formato> <input.tga>\n";
        return -1;
    }
    std::string format = argv[1];
    std::string inputPath = argv[2];
    try 
    {
        ImageData img = LoadTga(inputPath);
        std::vector<uint8_t> payload;
        std::string ext = "";
        bool isRGB = false;
        RgbHeader rgbHeader{};
        bool isCTX = false;
        std::vector<uint8_t> ctxHeaderData;
        if (format == "-ctx" || format == "-ctxa4") 
        {
            ext = ".ctx";
            bool forceAlpha = (format == "-ctxa4");
            bool hasAlpha = forceAlpha || DetectAlpha(img.pixels);
            payload = CreateBC(img.pixels, img.width, img.height, hasAlpha);
            if (payload.empty()) throw std::runtime_error("Error creating BC compression.");
            FatHeader fh{};
            std::memcpy(fh.Firma, "CTX ", 4);
            fh.EntryCount = 1;
            fh.Width = img.width;
            fh.Height = img.height;
            fh.Flags = hasAlpha ? 1 : 0;
            TextureEntry te{};
            te.Offset = sizeof(FatHeader) + sizeof(TextureEntry);
            te.Size = static_cast<uint32_t>(payload.size());
            ctxHeaderData.resize(sizeof(FatHeader) + sizeof(TextureEntry));
            std::memcpy(ctxHeaderData.data(), &fh, sizeof(FatHeader));
            std::memcpy(ctxHeaderData.data() + sizeof(FatHeader), &te, sizeof(TextureEntry));
            isCTX = true;
        }
        else if (format == "-a4") { ext = ".a4"; payload = ConvertA4(img.pixels); }
        else if (format == "-a8") { ext = ".a8"; payload = ConvertA8(img.pixels); }
        else if (format == "-l4") { ext = ".l4"; payload = ConvertL4(img.pixels); }
        else if (format == "-l8") { ext = ".l8"; payload = ConvertL8(img.pixels); }
        else if (format == "-la4") { ext = ".la4"; payload = ConvertLA4(img.pixels); }
        else if (format == "-la8") { ext = ".la8"; payload = ConvertLA8(img.pixels); }
        else if (format == "-r5g6b5") { ext = ".rgb"; payload = ConvertR5G6B5(img.pixels); isRGB = true; rgbHeader.Type = 565; }
        else if (format == "-r8g8b8") { ext = ".rgb"; payload = ConvertR8G8B8(img.pixels); isRGB = true; rgbHeader.Type = 888; }
        else if (format == "-r8g8b8a8") { ext = ".rgb"; payload = ConvertR8G8B8A8(img.pixels); isRGB = true; rgbHeader.Type = 8888; }
        else 
        {
            std::cerr << "Formato no soportado: " << format << "\n";
            return -1;
        }
        if (isRGB) 
        {
            std::memcpy(rgbHeader.Magic, "RGB ", 4);
            rgbHeader.Width = img.width;
            rgbHeader.Height = img.height;
        }
        std::string outPath = inputPath;
        size_t dotPos = outPath.find_last_of(".");
        if (dotPos != std::string::npos) 
        {
            outPath = outPath.substr(0, dotPos) + ext;
        } 
        else 
        {
            outPath += ext;
        }
        std::ofstream out(outPath, std::ios::binary);
        if (!out) 
        {
            std::cerr << "No se pudo crear archivo de salida: " << outPath << "\n";
            return -1;
        }
        if (isCTX) 
        {
            out.write(reinterpret_cast<const char*>(ctxHeaderData.data()), ctxHeaderData.size());
        } 
        else if (isRGB) 
        {
            out.write(reinterpret_cast<const char*>(&rgbHeader), sizeof(RgbHeader));
        }
        out.write(reinterpret_cast<const char*>(payload.data()), payload.size());
        out.close();
        std::cout << "TextureConverter (Uadeo): " << inputPath << " -> " << outPath << "\n";
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "ERROR: " << e.what() << "\n";
        return -1;
    }
    return 0;
}