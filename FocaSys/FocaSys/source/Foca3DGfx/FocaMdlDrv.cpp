#define NOMINMAX
#include"Foca3DGfx/FocaMdlDrv.h"
#include"F3DUtils.h"
#include"FocaArchive.h"
#include<algorithm>
#include<cctype>
#include<cmath>
#include<cstdint>
#include<limits>
#include<DynamicArray.h>
#include<DynamicMap.h>
#include<DynamicString.h>
#include<MemAlcExp.h>
#include<glm/vec4.hpp>
using Foca3DGfx::detail::CalculateSortCenter;
using Foca3DGfx::detail::DecodeBcColor;
using Foca3DGfx::detail::GuessRawDimensions;
using Foca3DGfx::detail::HasTransparentVertex;
using Foca3DGfx::detail::Lowercase;
using Foca3DGfx::detail::NormalizeTextureFormat;
using Foca3DGfx::detail::ReadU16;
using Foca3DGfx::detail::ReadU32;
namespace
{
	struct DecodedTexture
	{
		u32 Width = 0;
		u32 Height = 0;
		Mem::MemAlcExp PixelHeap;
		u8* Pixels = nullptr;
		std::size_t PixelByteCount = 0;
		~DecodedTexture()
		{
			if (Pixels != nullptr)
			{
				(void)PixelHeap.Free(Pixels);
			}
		}
		[[nodiscard]] bool ResizePixels(std::size_t ByteCount, u8 Value = 0)
		{
			constexpr std::size_t HeapSlack = 4096;
			if (Pixels != nullptr || ByteCount == 0 || ByteCount > (std::numeric_limits<std::size_t>::max)() - HeapSlack)
			{
				return false;
			}
			if (!PixelHeap.IsInitialized() && !PixelHeap.InitializeOwned(ByteCount + HeapSlack))
			{
				return false;
			}
			Pixels = static_cast<u8*>(PixelHeap.Allocate(ByteCount, alignof(u8)));
			if (Pixels == nullptr)
			{
				return false;
			}
			std::fill_n(Pixels, ByteCount, Value);
			PixelByteCount = ByteCount;
			return true;
		}
	};		
	void StorePixel(DecodedTexture& image, u32 x, u32 y, u8 r, u8 g, u8 b, u8 a)
	{
		const size_t offset = (static_cast<size_t>(y) * image.Width + x) * 4;
		image.Pixels[offset + 0] = r;
		image.Pixels[offset + 1] = g;
		image.Pixels[offset + 2] = b;
		image.Pixels[offset + 3] = a;
	}
	bool HasTransparentPixels(const DecodedTexture& image)
	{
		for (size_t i = 3; i < image.PixelByteCount; i += 4)
		{
			if (image.Pixels[i] < 255)
			{
				return true;
			}
		}
		return false;
	}			
	bool DecodeCtx(const DynamicArray<u8>& data, DecodedTexture& image)
	{
		if (data.size() < 28 || data[0] != 'C' || data[1] != 'T' || data[2] != 'X' || data[3] != ' ')
		{
			return false;
		}
		image.Width = ReadU32(data, 8);
		image.Height = ReadU32(data, 12);
		const u32 flags = ReadU32(data, 16);
		const u32 dataOffset = ReadU32(data, 20);
		const u32 dataSize = ReadU32(data, 24);
		const bool bc2 = flags == 1;
		const size_t blockBytes = bc2 ? 16 : 8;
		const u64 pixelCount = static_cast<u64>(image.Width) * image.Height;
		const size_t blocksX = (static_cast<size_t>(image.Width) + 3) / 4;
		const size_t blocksY = (static_cast<size_t>(image.Height) + 3) / 4;
		if (image.Width == 0 || image.Height == 0 || pixelCount > SIZE_MAX / 4 || blocksY > SIZE_MAX / blocksX || blocksX * blocksY > SIZE_MAX / blockBytes)
		{
			return false;
		}
		const size_t requiredBytes = blocksX * blocksY * blockBytes;
		if (dataOffset > data.size() || dataSize > data.size() - dataOffset || dataSize < requiredBytes || !image.ResizePixels(static_cast<size_t>(pixelCount) * 4, 255))
		{
			return false;
		}
		size_t blockOffset = dataOffset;
		for (size_t by = 0; by < blocksY; ++by)
		{
			for (size_t bx = 0; bx < blocksX; ++bx)
			{
				const u64 alphaBits = bc2 ? (static_cast<u64>(ReadU32(data, blockOffset)) | (static_cast<u64>(ReadU32(data, blockOffset + 4)) << 32)) : 0;
				const size_t colorOffset = blockOffset + (bc2 ? 8 : 0);
				const u16 color0 = ReadU16(data, colorOffset);
				const u16 color1 = ReadU16(data, colorOffset + 2);
				const u32 indices = ReadU32(data, colorOffset + 4);
				u8 colors[4][4]{};
				DecodeBcColor(color0, colors[0][0], colors[0][1], colors[0][2]);
				DecodeBcColor(color1, colors[1][0], colors[1][1], colors[1][2]);
				colors[0][3] = colors[1][3] = 255;
				if (color0 > color1 || bc2)
				{
					for (u32 c = 0; c < 3; ++c)
					{
						colors[2][c] = static_cast<u8>((2 * colors[0][c] + colors[1][c]) / 3);
						colors[3][c] = static_cast<u8>((colors[0][c] + 2 * colors[1][c]) / 3);
					}
					colors[2][3] = colors[3][3] = 255;
				}
				else
				{
					for (u32 c = 0; c < 3; ++c)
					{
						colors[2][c] = static_cast<u8>((colors[0][c] + colors[1][c]) / 2);
					}
					colors[2][3] = 255;
					colors[3][0] = colors[3][1] = colors[3][2] = colors[3][3] = 0;
				}
				for (u32 py = 0; py < 4; ++py)
				{
					for (u32 px = 0; px < 4; ++px)
					{
						const u32 x = static_cast<u32>(bx * 4 + px);
						const u32 y = static_cast<u32>(by * 4 + py);
						if (x >= image.Width || y >= image.Height)
						{
							continue;
						}
						const u32 pixelIndex = py * 4 + px;
						const u32 colorIndex = (indices >> (pixelIndex * 2)) & 3;
						u8 alpha = colors[colorIndex][3];
						if (bc2)
						{
							alpha = static_cast<u8>(((alphaBits >> (pixelIndex * 4)) & 0xf) * 17);
						}
						StorePixel(image, x, y, colors[colorIndex][0], colors[colorIndex][1], colors[colorIndex][2], alpha);
					}
				}
				blockOffset += blockBytes;
			}
		}
		return true;
	}
	bool DecodeRgb(const DynamicArray<u8>& data, DecodedTexture& image)
	{
		if (data.size() < 16 || data[0] != 'R' || data[1] != 'G' || data[2] != 'B' || data[3] != ' ')
		{
			return false;
		}
		const u32 type = ReadU32(data, 4);
		image.Width = ReadU32(data, 8);
		image.Height = ReadU32(data, 12);
		const u32 bytesPerPixel = type == 565 ? 2 : type == 888 ? 3 : type == 8888 ? 4 : 0;
		const u64 pixelCount = static_cast<u64>(image.Width) * image.Height;
		if (bytesPerPixel == 0 || image.Width == 0 || image.Height == 0 || pixelCount > SIZE_MAX / 4 || pixelCount * bytesPerPixel > data.size() - 16)
		{
			return false;
		}
		if (!image.ResizePixels(static_cast<size_t>(pixelCount) * 4))
		{
			return false;
		}
		size_t sourceOffset = 16;
		for (u32 y = 0; y < image.Height; ++y)
		{
			for (u32 x = 0; x < image.Width; ++x)
			{
				u8 r = 0, g = 0, b = 0, a = 255;
				if (type == 565)
				{
					const u16 value = ReadU16(data, sourceOffset);
					sourceOffset += 2;
					r = static_cast<u8>(((value >> 11) & 0x1f) * 255 / 31);
					g = static_cast<u8>(((value >> 5) & 0x3f) * 255 / 63);
					b = static_cast<u8>((value & 0x1f) * 255 / 31);
				}
				else
				{
					r = data[sourceOffset++];
					g = data[sourceOffset++];
					b = data[sourceOffset++];
					if (type == 8888)
					{
						a = data[sourceOffset++];
					}
				}
				StorePixel(image, x, y, r, g, b, a);
			}
		}
		return true;
	}
	bool DecodeRaw(const DynamicArray<u8>& data, const String& format, u32 width, u32 height, DecodedTexture& image)
	{
		if ((format == "a4" || format == "l4") && data.size() > SIZE_MAX / 2)
		{
			return false;
		}
		const size_t pixelCount = (format == "a4" || format == "l4") ? data.size() * 2 : (format == "la8") ? data.size() / 2 : (format == "la4" || format == "a8" || format == "l8") ? data.size() : 0;
		if (pixelCount == 0 || pixelCount > SIZE_MAX / 4)
		{
			return false;
		}
		if (width != 0 || height != 0)
		{
			if (width == 0 || height == 0 || static_cast<u64>(width) * height != pixelCount)
			{
				return false;
			}
			image.Width = width;
			image.Height = height;
		}
		else if (!GuessRawDimensions(pixelCount, image.Width, image.Height))
		{
			return false;
		}
		if (!image.ResizePixels(pixelCount * 4))
		{
			return false;
		}
		for (size_t i = 0; i < pixelCount; ++i)
		{
			u8 luminance = 255;
			u8 alpha = 255;
			if (format == "a8")
			{
				alpha = data[i];
			}
			else if (format == "l8")
			{
				luminance = data[i];
			}
			else if (format == "a4" || format == "l4")
			{
				const u8 packed = data[i / 2];
				const u8 nibble = static_cast<u8>((i & 1) == 0 ? packed & 0xf : packed >> 4);
				if (format == "a4")
				{
					alpha = static_cast<u8>(nibble * 17);
				}
				else
				{
					luminance = static_cast<u8>(nibble * 17);
				}
			}
			else if (format == "la4")
			{
				luminance = static_cast<u8>((data[i] & 0xf) * 17);
				alpha = static_cast<u8>((data[i] >> 4) * 17);
			}
			else if (format == "la8")
			{
				luminance = data[i * 2];
				alpha = data[i * 2 + 1];
			}
			else
			{
				return false;
			}
			StorePixel(image, static_cast<u32>(i % image.Width), static_cast<u32>(i / image.Width), luminance, luminance, luminance, alpha);
		}
		return true;
	}
	bool DecodeTga(const DynamicArray<u8>& data, DecodedTexture& image)
	{
		if (data.size() < 18 || data[1] != 0 || (data[2] != 2 && data[2] != 10))
		{
			return false;
		}
		image.Width = ReadU16(data, 12);
		image.Height = ReadU16(data, 14);
		const u32 bpp = data[16];
		const u32 bytesPerPixel = bpp / 8;
		if (image.Width == 0 || image.Height == 0 || (bpp != 24 && bpp != 32) || static_cast<u64>(image.Width) * image.Height > SIZE_MAX / 4)
		{
			return false;
		}
		if (!image.ResizePixels(static_cast<size_t>(image.Width) * image.Height * 4, 0))
		{
			return false;
		}
		const size_t pixelCount = static_cast<size_t>(image.Width) * image.Height;
		size_t input = 18 + data[0];
		size_t outputPixel = 0;
		const bool topOrigin = (data[17] & 0x20) != 0;
		auto emitPixel = [&](const u8* pixel)
		{
			const u32 fileY = static_cast<u32>(outputPixel / image.Width);
			const u32 x = static_cast<u32>(outputPixel % image.Width);
			const u32 y = topOrigin ? image.Height - 1 - fileY : fileY;
			StorePixel(image, x, y, pixel[2], pixel[1], pixel[0], bytesPerPixel == 4 ? pixel[3] : 255);
			++outputPixel;
		};
		while (outputPixel < pixelCount)
		{
			if (data[2] == 2)
			{
				if (input + bytesPerPixel > data.size())
				{
					return false;
				}
				emitPixel(data.data() + input);
				input += bytesPerPixel;
				continue;
			}
			if (input >= data.size())
			{
				return false;
			}
			const u8 packet = data[input++];
			const size_t count = (packet & 0x7f) + 1;
			if (outputPixel + count > pixelCount)
			{
				return false;
			}
			if (packet & 0x80)
			{
				if (input + bytesPerPixel > data.size()) return false;
				const u8* pixel = data.data() + input;
				input += bytesPerPixel;
				for (size_t i = 0; i < count; ++i)
				{
					emitPixel(pixel);
				}
			}
			else
			{
				if (input + count * bytesPerPixel > data.size())
				{
					return false;
				}
				for (size_t i = 0; i < count; ++i)
				{
					emitPixel(data.data() + input);
					input += bytesPerPixel;
				}
			}
		}
		return true;
	}
	bool DecodeBmp(const DynamicArray<u8>& data, DecodedTexture& image)
	{
		if (data.size() < 54 || data[0] != 'B' || data[1] != 'M')
		{
			return false;
		}
		const u32 pixelOffset = ReadU32(data, 10);
		const u32 dibSize = ReadU32(data, 14);
		if (dibSize < 40 || data.size() < 14 + dibSize)
		{
			return false;
		}
		const s32 signedWidth = static_cast<s32>(ReadU32(data, 18));
		const s32 signedHeight = static_cast<s32>(ReadU32(data, 22));
		const u16 planes = ReadU16(data, 26);
		const u16 bpp = ReadU16(data, 28);
		const u32 compression = ReadU32(data, 30);
		if (signedWidth <= 0 || signedHeight == 0 || planes != 1 || (bpp != 24 && bpp != 32) || compression != 0)
		{
			return false;
		}
		image.Width = static_cast<u32>(signedWidth);
		image.Height = static_cast<u32>(signedHeight < 0 ? -static_cast<s64>(signedHeight) : signedHeight);
		if (static_cast<u64>(image.Width) * image.Height > SIZE_MAX / 4)
		{
			return false;
		}
		const size_t bytesPerPixel = bpp / 8;
		const size_t rowStride = ((static_cast<size_t>(image.Width) * bpp + 31) / 32) * 4;
		if (pixelOffset > data.size() || rowStride * image.Height > data.size() - pixelOffset)
		{
			return false;
		}
		if (!image.ResizePixels(static_cast<size_t>(image.Width) * image.Height * 4))
		{
			return false;
		}
		const bool topDown = signedHeight < 0;
		for (u32 fileY = 0; fileY < image.Height; ++fileY)
		{
			const u32 y = topDown ? image.Height - 1 - fileY : fileY;
			for (u32 x = 0; x < image.Width; ++x)
			{
				const size_t offset = pixelOffset + static_cast<size_t>(fileY) * rowStride + x * bytesPerPixel;
				StorePixel(image, x, y, data[offset + 2], data[offset + 1], data[offset], bytesPerPixel == 4 ? data[offset + 3] : 255);
			}
		}
		return true;
	}
	bool DecodeTexture(const Foca3DGfx::detail::FocaTexture& texture, DecodedTexture& decoded)
	{
		const String format = NormalizeTextureFormat(texture.Format, texture.Name);
		if (format == "ctx" || format == "ctxe") return DecodeCtx(texture.Data, decoded);
		if (format == "rgb") return DecodeRgb(texture.Data, decoded);
		if (format == "tga") return DecodeTga(texture.Data, decoded);
		if (format == "bmp") return DecodeBmp(texture.Data, decoded);
		return DecodeRaw(texture.Data, format, texture.Width, texture.Height, decoded);
	}
}
Foca3DGfx::FocaMdlInstance::~FocaMdlInstance()
{
	Finalize();
}
void Foca3DGfx::FocaMdlInstance::Finalize()
{
	Meshes.clear();
	ModelData = nullptr;
	Offset = glm::vec3(0.0f);
	Scale = glm::vec3(1.0f);
	UvAnimation = nullptr;
	UvAnimationTime = 0.0;
	UvAnimationLoop = true;
}
Foca3DGfx::FocaMdlDrv::FocaMdlDrv()
{

}
Foca3DGfx::FocaMdlDrv::FocaMdlDrv(gx::Display* Display)
{
	Initialize(Display);
}
Foca3DGfx::FocaMdlDrv::~FocaMdlDrv()
{
	Finalize();
}
void Foca3DGfx::FocaMdlDrv::Initialize(gx::Display* Display)
{
	this->Display = Display;
}
void Foca3DGfx::FocaMdlDrv::Finalize()
{
	DefaultTexture.Finalize();
	Display = nullptr;
	Shader = nullptr;
}
void Foca3DGfx::FocaMdlDrv::SetShader(gx::Shader* Shader)
{
	this->Shader = Shader;
}
gx::Shader* Foca3DGfx::FocaMdlDrv::GetShader() const
{
	return Shader;
}
bool Foca3DGfx::FocaMdlDrv::BuildBuffers(FocaMdlInstance& Instance, const FocaMdl& Model)
{
	return BuildBuffersInternal(Instance, Model, nullptr, nullptr);
}
bool Foca3DGfx::FocaMdlDrv::BuildBuffers(FocaMdlInstance& Instance, const FocaMdl& Model, const FocaArchive& Archive)
{
	return BuildBuffersInternal(Instance, Model, &Archive, nullptr);
}
bool Foca3DGfx::FocaMdlDrv::BuildBuffers(FocaMdlInstance& Instance, const FocaMdl& Model, const detail::FocaTexture& Texture)
{
	return BuildBuffersInternal(Instance, Model, nullptr, &Texture);
}
bool Foca3DGfx::FocaMdlDrv::UpdateUVs(FocaMdlInstance& Instance, u32 MeshIndex, const DynamicArray<f32>& UVs)
{
	if (Instance.ModelData == nullptr || MeshIndex >= Instance.Meshes.size())
	{
		return false;
	}
	auto& gpuMesh = Instance.Meshes[MeshIndex];
	if (UVs.size() != gpuMesh.VertexData.size() * 2)
	{
		return false;
	}
	for (size_t i = 0; i < gpuMesh.VertexData.size(); ++i)
	{
		gpuMesh.VertexData[i].UV[0] = UVs[i * 2];
		gpuMesh.VertexData[i].UV[1] = UVs[i * 2 + 1];
	}
	return gpuMesh.VertexBuf.UpdateVertexBuffer(gpuMesh.VertexData.data(), gpuMesh.VertexData.size() * sizeof(detail::FocaVertex));
}
bool Foca3DGfx::FocaMdlDrv::PlayUvAnimation(FocaMdlInstance& Instance, std::string_view AnimationName, bool Loop)
{
	if (Instance.ModelData == nullptr || AnimationName.empty())
	{
		return false;
	}
	const auto& animations = Instance.ModelData->GetUvAnimations();
	const auto animationIt = std::find_if(animations.begin(), animations.end(), [&](const detail::FocaUvAnimation& animation) {
		return (animation.Name == AnimationName || animation.ActionName == AnimationName) && !animation.Samples.empty() && !animation.MappingNodes.empty();
	});
	if (animationIt == animations.end() || !UpdateUvAnimation(Instance, *animationIt, 0.0, Loop))
	{
		return false;
	}
	Instance.UvAnimation = &*animationIt;
	Instance.UvAnimationTime = 0.0;
	Instance.UvAnimationLoop = Loop;
	return true;
}
void Foca3DGfx::FocaMdlDrv::Update(FocaMdlInstance& Instance, f64 DeltaSeconds)
{
	if (!std::isfinite(DeltaSeconds) || Instance.ModelData == nullptr || Instance.UvAnimation == nullptr || Instance.UvAnimation->Samples.size() < 2)
	{
		return;
	}
	const f64 deltaSeconds = std::max(DeltaSeconds, 0.0);
	f64 nextTime = Instance.UvAnimationTime + deltaSeconds;
	if (!Instance.UvAnimationLoop)
	{
		const f64 fps = std::max(static_cast<f64>(Instance.ModelData->GetFps()), 1.0);
		const f64 endTime = static_cast<f64>(Instance.UvAnimation->Samples.size() - 1) / fps;
		nextTime = std::min(nextTime, endTime);
		if (nextTime <= Instance.UvAnimationTime)
		{
			return;
		}
	}
	if (UpdateUvAnimation(Instance, *Instance.UvAnimation, nextTime, Instance.UvAnimationLoop))
	{
		Instance.UvAnimationTime = nextTime;
	}
}
bool Foca3DGfx::FocaMdlDrv::UpdateUvAnimation(FocaMdlInstance& Instance, const detail::FocaUvAnimation& Animation, f64 TimeSeconds, bool Loop)
{
	if (Instance.ModelData == nullptr)
	{
		return false;
	}
	DynamicArray<DynamicArray<f32>> meshUVs;
	if (!Instance.ModelData->EvaluateUvAnimation(Animation, TimeSeconds, meshUVs, Loop))
	{
		return false;
	}
	for (u32 meshIndex = 0; meshIndex < static_cast<u32>(meshUVs.size()); ++meshIndex)
	{
		if (!meshUVs[meshIndex].empty() && !UpdateUVs(Instance, meshIndex, meshUVs[meshIndex]))
		{
			return false;
		}
	}
	return true;
}
bool Foca3DGfx::FocaMdlDrv::SetModelOffset(FocaMdlInstance& Instance, f32 X, f32 Y, f32 Z)
{
	if (Instance.ModelData == nullptr)
	{
		return false;
	}
	const f32 delta[3] =
	{
		X - Instance.Offset[0],
		Y - Instance.Offset[1],
		Z - Instance.Offset[2]
	};
	if (delta[0] == 0.0f && delta[1] == 0.0f && delta[2] == 0.0f)
	{
		return true;
	}
	bool updated = true;
	for (auto& mesh : Instance.Meshes)
	{
		for (auto& vertex : mesh.VertexData)
		{
			for (u32 axis = 0; axis < 3; ++axis)
			{
				vertex.Position[axis] += delta[axis];
			}
		}
		if (!mesh.VertexData.empty() && !mesh.VertexBuf.UpdateVertexBuffer(mesh.VertexData.data(), mesh.VertexData.size() * sizeof(detail::FocaVertex)))
		{
			updated = false;
		}
		for (auto& batch : mesh.Batches)
		{
			for (u32 axis = 0; axis < 3; ++axis)
			{
				batch.SortCenter[axis] += delta[axis];
			}
		}
	}
	Instance.Offset[0] = X;
	Instance.Offset[1] = Y;
	Instance.Offset[2] = Z;
	return updated;
}
bool Foca3DGfx::FocaMdlDrv::SetModelScale(FocaMdlInstance& Instance, f32 X, f32 Y, f32 Z)
{
	if (Instance.ModelData == nullptr)
	{
		return false;
	}
	const f32 scale[3] = { X, Y, Z };
	f32 ratio[3]{};
	for (u32 axis = 0; axis < 3; ++axis)
	{
		if (!std::isfinite(scale[axis]) || std::abs(scale[axis]) < 1.0e-6f)
		{
			return false;
		}
		ratio[axis] = scale[axis] / Instance.Scale[axis];
	}
	if (ratio[0] == 1.0f && ratio[1] == 1.0f && ratio[2] == 1.0f)
	{
		return true;
	}
	bool updated = true;
	for (auto& mesh : Instance.Meshes)
	{
		for (auto& vertex : mesh.VertexData)
		{
			for (u32 axis = 0; axis < 3; ++axis)
			{
				vertex.Position[axis] = Instance.Offset[axis] + (vertex.Position[axis] - Instance.Offset[axis]) * ratio[axis];
				vertex.Normal[axis] /= ratio[axis];
			}
			const f32 normalLength = std::sqrt(vertex.Normal[0] * vertex.Normal[0] + vertex.Normal[1] * vertex.Normal[1] + vertex.Normal[2] * vertex.Normal[2]);
			if (normalLength > 1.0e-6f)
			{
				for (f32& component : vertex.Normal)
				{
					component /= normalLength;
				}
			}
		}
		if (!mesh.VertexData.empty() && !mesh.VertexBuf.UpdateVertexBuffer(mesh.VertexData.data(), mesh.VertexData.size() * sizeof(detail::FocaVertex)))
		{
			updated = false;
		}
		for (auto& batch : mesh.Batches)
		{
			for (u32 axis = 0; axis < 3; ++axis)
			{
				batch.SortCenter[axis] = Instance.Offset[axis] + (batch.SortCenter[axis] - Instance.Offset[axis]) * ratio[axis];
			}
		}
	}
	for (u32 axis = 0; axis < 3; ++axis)
	{
		Instance.Scale[axis] = scale[axis];
	}
	return updated;
}
bool Foca3DGfx::FocaMdlDrv::BuildBuffersInternal(FocaMdlInstance& Instance, const FocaMdl& Model, const FocaArchive* Archive, const detail::FocaTexture* DirectTexture)
{
	Instance.Finalize();
	if (!Display || !Shader)
	{
		return false;
	}
	static constexpr u8 whitePixel[4] = { 255, 255, 255, 255 };
	if (!DefaultTexture.IsValid() && !DefaultTexture.Initialize(*Display, *Shader, 1, 1, whitePixel))
	{
		return false;
	}
	const auto& meshes = Model.GetMeshes();
	const auto& materials = Model.GetMaterials();
	static const DynamicArray<detail::FocaTexture> noTextures;
	const auto& archiveTextures = Archive ? Archive->GetTextures() : noTextures;
	f32 minX = std::numeric_limits<f32>::max();
	f32 maxX = std::numeric_limits<f32>::lowest();
	f32 minY = std::numeric_limits<f32>::max();
	f32 maxY = std::numeric_limits<f32>::lowest();
	f32 minZ = std::numeric_limits<f32>::max();
	f32 maxZ = std::numeric_limits<f32>::lowest();
	bool hasPositions = false;
	for (const auto& mesh : meshes)
	{
		for (const auto& vertex : mesh.Vertices)
		{
			minX = std::min(minX, vertex.Position[0]);
			maxX = std::max(maxX, vertex.Position[0]);
			minY = std::min(minY, vertex.Position[1]);
			maxY = std::max(maxY, vertex.Position[1]);
			minZ = std::min(minZ, vertex.Position[2]);
			maxZ = std::max(maxZ, vertex.Position[2]);
			hasPositions = true;
		}
	}
	const f32 centerY = hasPositions ? (minY + maxY) * 0.5f : 0.0f;
	const f32 centerZ = hasPositions ? (minZ + maxZ) * 0.5f : 0.0f;
	const f32 centerX = hasPositions ? (minX + maxX) * 0.5f : 0.0f;
	Instance.ModelData = &Model;
	for (const auto& mesh : meshes)
	{
		Instance.Meshes.emplace_back();
		auto& gpuMesh = Instance.Meshes.back();
		gpuMesh.VertexCount = static_cast<u32>(mesh.Vertices.size());
		gpuMesh.VertexBuf.Initialize(*Display);
		if (!mesh.Vertices.empty())
		{
			gpuMesh.VertexData = mesh.Vertices;
			for (auto& vertex : gpuMesh.VertexData)
			{
				const f32 x = vertex.Position[0];
				const f32 y = vertex.Position[1];
				const f32 z = vertex.Position[2];
				const f32 nx = vertex.Normal[0];
				const f32 ny = vertex.Normal[1];
				const f32 nz = vertex.Normal[2];
				vertex.Position[0] = y - centerY;
				vertex.Position[1] = z - centerZ;
				vertex.Position[2] = 0.5f - (x - centerX);
				vertex.Normal[0] = ny;
				vertex.Normal[1] = nz;
				vertex.Normal[2] = nx;
			}
			const u64 vertexBytes = gpuMesh.VertexData.size() * sizeof(detail::FocaVertex);
			if (!gpuMesh.VertexBuf.CreateVertexBuffer(gpuMesh.VertexData.data(), vertexBytes))
			{
				Instance.Finalize();
				return false;
			}
		}
		DynamicMap<u32, DynamicArray<u32>> indicesByMaterial;
		if (mesh.Triangles.empty())
		{
			indicesByMaterial[mesh.MaterialIndex];
		}
		else
		{
			for (const auto& triangle : mesh.Triangles)
			{
				u32 materialIndex = mesh.MaterialIndex;
				if (triangle.MaterialIndex < mesh.MaterialSlots.size())
				{
					const s32 slotMaterial = mesh.MaterialSlots[triangle.MaterialIndex];
					materialIndex = slotMaterial >= 0 ? static_cast<u32>(slotMaterial) : UINT32_MAX;
				}
				auto& indices = indicesByMaterial[materialIndex];
				indices.push_back(triangle.V0);
				indices.push_back(triangle.V1);
				indices.push_back(triangle.V2);
			}
		}
		for (const auto& [materialIndex, indices] : indicesByMaterial)
		{
			gpuMesh.Batches.emplace_back();
			auto& batch = gpuMesh.Batches.back();
			batch.Transparent = HasTransparentVertex(gpuMesh.VertexData, indices);
			CalculateSortCenter(gpuMesh.VertexData, indices, batch.SortCenter);
			if (!indices.empty())
			{
				batch.IndexBuf.Initialize(*Display);
				if (!batch.IndexBuf.CreateIndexBuffer(indices.data(), static_cast<u32>(indices.size())))
				{
					Instance.Finalize();
					return false;
				}
				batch.IndexCount = static_cast<u32>(indices.size());
				batch.HasIndices = true;
			}
			DecodedTexture decoded;
			if (DirectTexture)
			{
				if (!DirectTexture->IncludedData || DirectTexture->Data.empty() || !DecodeTexture(*DirectTexture, decoded) || !batch.Texture.Initialize(*Display, *Shader, decoded.Width, decoded.Height, decoded.Pixels))
				{
					Instance.Finalize();
					return false;
				}
				batch.Transparent = batch.Transparent || HasTransparentPixels(decoded);
			}
			else if (Archive && materialIndex < materials.size() && !materials[materialIndex].AssignedTexture.empty())
			{
				const String& assignedName = materials[materialIndex].AssignedTexture;
				const auto textureIt = std::find_if(archiveTextures.begin(), archiveTextures.end(), [&](const detail::FocaTexture& texture) {
					return Lowercase(texture.Name) == Lowercase(assignedName);
				});
				if (textureIt != archiveTextures.end() && textureIt->IncludedData && !textureIt->Data.empty() && DecodeTexture(*textureIt, decoded) && batch.Texture.Initialize(*Display, *Shader, decoded.Width, decoded.Height, decoded.Pixels))
				{
					batch.Transparent = batch.Transparent || HasTransparentPixels(decoded);
				}
			}
		}
	}
	if (const detail::FocaUvAnimation* animation = Model.FindFirstAnimatedUvAnimation(); animation && UpdateUvAnimation(Instance, *animation, 0.0, true))
	{
		Instance.UvAnimation = animation;
		Instance.UvAnimationTime = 0.0;
	}
	return true;
}
void Foca3DGfx::FocaMdlDrv::Draw(gx::CommandBuffer& CmdBuffer, FocaMdlInstance& Instance, bool TransparentPass)
{
	if (Instance.ModelData == nullptr)
	{
		return;
	}
	CmdBuffer.SetTransparencyPass(TransparentPass);
	if (Shader)
	{
		CmdBuffer.SetShader(Shader);
	}
	for (u32 i = 0; i < static_cast<u32>(Instance.ModelData->GetMeshes().size()); ++i)
	{
		DrawMesh(CmdBuffer, Instance, i, TransparentPass);
	}
}
void Foca3DGfx::FocaMdlDrv::DrawMesh(gx::CommandBuffer& CmdBuffer, FocaMdlInstance& Instance, u32 MeshIndex, bool TransparentPass)
{
	if (Instance.ModelData == nullptr || MeshIndex >= Instance.ModelData->GetMeshes().size() || MeshIndex >= Instance.Meshes.size())
	{
		return;
	}
	auto& gpuMesh = Instance.Meshes[MeshIndex];
	if (!gpuMesh.VertexBuf.IsValid())
	{
		return;
	}
	CmdBuffer.BindVertexBuffer(&gpuMesh.VertexBuf);
	for (auto& batch : gpuMesh.Batches)
	{
		if (batch.Transparent != TransparentPass)
		{
			continue;
		}
		CmdBuffer.BindTexture(batch.Texture.IsValid() ? &batch.Texture : &DefaultTexture);
		if (batch.HasIndices && batch.IndexCount > 0)
		{
			CmdBuffer.BindIndexBuffer(&batch.IndexBuf);
			CmdBuffer.DrawIndexed(batch.IndexCount, 1, 0, 0, 0);
		}
		else if (gpuMesh.VertexCount > 0)
		{
			CmdBuffer.Draw(gpuMesh.VertexCount, 1, 0, 0);
		}
	}
}
void Foca3DGfx::FocaMdlDrv::CollectTransparentDraws(gx::CommandBuffer& CmdBuffer, FocaMdlInstance& Instance, const TransparentDrawCollector& Collector)
{
	(void)CmdBuffer;
	if (!Instance.ModelData || !Collector)
	{
		return;
	}
	for (u32 meshIndex = 0; meshIndex < static_cast<u32>(Instance.ModelData->GetMeshes().size()); ++meshIndex)
	{
		if (meshIndex >= Instance.Meshes.size())
		{
			break;
		}
		auto& gpuMesh = Instance.Meshes[meshIndex];
		if (!gpuMesh.VertexBuf.IsValid())
		{
			continue;
		}
		for (auto& batch : gpuMesh.Batches)
		{
			if (!batch.Transparent)
			{
				continue;
			}
			glm::vec4 center(batch.SortCenter[0], batch.SortCenter[1], batch.SortCenter[2], 1.0f);
			if (Shader)
			{
				center = Shader->GetMtx() * center;
			}
			TransparentDraw Draw;
			Draw.Depth = center.z;
			Draw.Shader = Shader;
			Draw.VertexBuffer = &gpuMesh.VertexBuf;
			Draw.IndexBuffer = &batch.IndexBuf;
			Draw.Texture = batch.Texture.IsValid() ? &batch.Texture : &DefaultTexture;
			Draw.VertexCount = gpuMesh.VertexCount;
			Draw.IndexCount = batch.IndexCount;
			Draw.HasIndices = batch.HasIndices;
			Collector.Add(Draw);
		}
	}
}
void Foca3DGfx::TransparentDraw::Execute(gx::CommandBuffer& CmdBuffer) const
{
	CmdBuffer.SetTransparencyPass(true);
	if (Shader != nullptr)
	{
		CmdBuffer.SetShader(Shader);
	}
	CmdBuffer.BindVertexBuffer(VertexBuffer);
	CmdBuffer.BindTexture(Texture);
	if (HasIndices && IndexCount > 0)
	{
		CmdBuffer.BindIndexBuffer(IndexBuffer);
		CmdBuffer.DrawIndexed(IndexCount, 1, 0, 0, 0);
	}
	else if (VertexCount > 0)
	{
		CmdBuffer.Draw(VertexCount, 1, 0, 0);
	}
}
