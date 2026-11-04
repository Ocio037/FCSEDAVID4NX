#include<Font.h>
#include<algorithm>
#include<cstdarg>
#include<cwchar>
#include<cstring>
#include<glm/gtc/matrix_transform.hpp>
#include<limits>
namespace
{
	constexpr char16 FirstChar = 0x0020;
	constexpr char16 LastChar = 0x007E;
	constexpr u32 MaxTextCount = 64;
	constexpr u32 MaxTextChars = 4096;
	constexpr u32 VerticesPerGlyph = 6;
	constexpr u32 MaxVertices = MaxTextChars * VerticesPerGlyph;
}
Font::font::font()
{
	InitializeDefaultCharset();
}
Font::font::font(gx::Display& display, gx::CommandBuffer& commandBuffer) : font()
{
	Create(display, commandBuffer);
}
Font::font::~font()
{
	Destroy();
}
void Font::font::Create(gx::Display& display, gx::CommandBuffer& commandBuffer)
{
	Textura.Finalize();
	VertexBuffer.Finalize();
	Shader.Finalize();
	Display = &display;
	CmdBuffer = &commandBuffer;
	Shader.Initialize(Display);
	VertexBuffer.Initialize(display);
	Vertices.assign(MaxVertices, VertexData{});
	VertexBuffer.CreateVertexBuffer(Vertices.data(), static_cast<u64>(Vertices.size()) * sizeof(VertexData));
	currentVertexCount = 0;
	textCount = 0;
	ShaderLoaded = false;
	AtlasLoaded = false;
	RebuildAtlasTexture();
}
void Font::font::Destroy()
{
	if (CmdBuffer)
	{
		CmdBuffer->SetShader(nullptr);
	}
	Textura.Finalize();
	VertexBuffer.Finalize();
	Shader.Finalize();
	Vertices.clear();
	AtlasLayersLoaded.clear();
	Display = nullptr;
	CmdBuffer = nullptr;
	Info = {};
	GridCols = 0;
	GridRows = 0;
	currentVertexCount = 0;
	textCount = 0;
	TextColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	HasAtlasInfo = false;
	ShaderLoaded = false;
	AtlasLoaded = false;
	Transparent = true;
}
void Font::font::Reset()
{
	currentVertexCount = 0;
	textCount = 0;
}
void Font::font::Update()
{
	Reset();
}
void Font::font::SetAtlasInfo(AtlasInfo info)
{
	Info = info;
	GridCols = info.GridCols;
	GridRows = info.GridRows;
	HasAtlasInfo = info.Width != 0 && info.Height != 0 && GridCols != 0 && GridRows != 0;
	RebuildAtlasTexture();
}
void Font::font::LoadCharsetMap(const char16* buffer, uptr numChars)
{
	if (!buffer || numChars == 0 || numChars > static_cast<uptr>((std::numeric_limits<s32>::max)()))
	{
		CharsetBase = FirstChar;
		CharsetLUT.clear();
		CharsetCount = 0;
		RebuildAtlasTexture();
		return;
	}
	char16 minChar = buffer[0];
	char16 maxChar = buffer[0];
	for (uptr i = 1; i < numChars; ++i)
	{
		minChar = (std::min)(minChar, buffer[i]);
		maxChar = (std::max)(maxChar, buffer[i]);
	}
	CharsetBase = minChar;
	CharsetLUT.assign(static_cast<uptr>(maxChar - minChar + 1), -1);
	for (uptr i = 0; i < numChars; ++i)
	{
		CharsetLUT[static_cast<uptr>(buffer[i] - minChar)] = static_cast<s32>(i);
	}
	CharsetCount = static_cast<u32>(numChars);
	RebuildAtlasTexture();
}
void Font::font::LoadShader(const char* vertexShader, u32 vertexShaderSize, const char* fragmentShader,
	u32 fragmentShaderSize, bool transparent)
{
	Textura.Finalize();
	AtlasLayersLoaded.clear();
	AtlasLoaded = false;
	ShaderLoaded = false;
	Shader.Finalize();
	if (!Display || !vertexShader || !fragmentShader)
	{
		return;
	}
	Shader.Initialize(Display);
	if (!Shader.Load(vertexShader, vertexShaderSize, fragmentShader, fragmentShaderSize, gx::ShaderVertexLayout::PositionUvLayer))
	{
		Shader.Finalize();
		return;
	}
	Transparent = transparent;
	ShaderLoaded = true;
	RebuildAtlasTexture();
}
void Font::font::LoadAtlas(const void* imageData, uptr size, u32 layer)
{
	if (!Textura.IsValid() || !imageData || layer >= AtlasLayersLoaded.size())
	{
		return;
	}
	if (Textura.UploadLayer(imageData, static_cast<u64>(size), layer))
	{
		AtlasLayersLoaded[layer] = true;
		AtlasLoaded = false;
	}
}
void Font::font::FinAtlasLoad()
{
	AtlasLoaded = Textura.IsValid() && !AtlasLayersLoaded.empty() && std::all_of(AtlasLayersLoaded.begin(), AtlasLayersLoaded.end(), [](bool loaded) { return loaded; });
}
void Font::font::SetTextColor(f32 r, f32 g, f32 b, f32 a)
{
	TextColor = glm::vec4(r, g, b, a);
}
void Font::font::DrawTextU(const char16* wstr, f32 x, f32 y, f32 charW, f32 charH)
{
	if (!IsValid() || !wstr || GridCols == 0 || GridRows == 0 || textCount >= MaxTextCount || currentVertexCount >= MaxVertices)
	{
		return;
	}
	const u32 availableVertices = MaxVertices - currentVertexCount;
	DynamicArray<VertexData> drawVertices;
	const size_t stringLength = std::char_traits<char16_t>::length(wstr);
	const size_t reserveCount = (std::min)(static_cast<size_t>(availableVertices), stringLength * VerticesPerGlyph);
	drawVertices.reserve(reserveCount);
	const u32 charsPerLayer = GridCols * GridRows;
	while (*wstr)
	{
		if (drawVertices.size() + VerticesPerGlyph > availableVertices)
		{
			break;
		}
		const s32 mapIndex = static_cast<s32>(*wstr) - static_cast<s32>(CharsetBase);
		if (mapIndex >= 0 && static_cast<uptr>(mapIndex) < CharsetLUT.size())
		{
			const s32 atlasIndex = CharsetLUT[static_cast<uptr>(mapIndex)];
			if (atlasIndex >= 0)
			{
				const u32 layer = static_cast<u32>(atlasIndex) / charsPerLayer;
				const u32 indexInLayer = static_cast<u32>(atlasIndex) % charsPerLayer;
				const u32 col = indexInLayer % GridCols;
				const u32 row = indexInLayer / GridCols;
				const f32 u0 = static_cast<f32>(col) / static_cast<f32>(GridCols);
				const f32 v0 = 1.0f - static_cast<f32>(row + 1) / static_cast<f32>(GridRows);
				const f32 u1 = static_cast<f32>(col + 1) / static_cast<f32>(GridCols);
				const f32 v1 = 1.0f - static_cast<f32>(row) / static_cast<f32>(GridRows);
				const f32 layerZ = static_cast<f32>(layer);
				const f32 x0 = x;
				const f32 y0 = y;
				const f32 x1 = x + charW;
				const f32 y1 = y + charH;
				drawVertices.push_back({ {x0, y0, 0.0f}, {u0, v1, layerZ} });
				drawVertices.push_back({ {x1, y0, 0.0f}, {u1, v1, layerZ} });
				drawVertices.push_back({ {x0, y1, 0.0f}, {u0, v0, layerZ} });
				drawVertices.push_back({ {x0, y1, 0.0f}, {u0, v0, layerZ} });
				drawVertices.push_back({ {x1, y0, 0.0f}, {u1, v1, layerZ} });
				drawVertices.push_back({ {x1, y1, 0.0f}, {u1, v0, layerZ} });
				x += charW;
			}
		}
		++wstr;
	}
	if (drawVertices.empty())
	{
		return;
	}
	const gx::NativeFrameContext frameContext = CmdBuffer->GetNativeFrameContext();
	const gx::detail::DisplayResolution displayResolution = Display->GetResolution();
	const u32 viewportWidth = frameContext.SceneExtent.width != 0 ? frameContext.SceneExtent.width : displayResolution.Width;
	const u32 viewportHeight = frameContext.SceneExtent.height != 0 ? frameContext.SceneExtent.height : displayResolution.Height;
	if (viewportWidth == 0 || viewportHeight == 0)
	{
		return;
	}
	const u32 firstVertex = currentVertexCount;
	const u64 byteOffset = static_cast<u64>(firstVertex) * sizeof(VertexData);
	const u64 byteSize = static_cast<u64>(drawVertices.size()) * sizeof(VertexData);
	if (!VertexBuffer.UpdateVertexBuffer(drawVertices.data(), byteSize, byteOffset))
	{
		return;
	}
	std::memcpy(Vertices.data() + firstVertex, drawVertices.data(), static_cast<size_t>(byteSize));
	const glm::mat4 identity(1.0f);
	const glm::mat4 projection = glm::orthoLH_ZO(0.0f, static_cast<f32>(viewportWidth), 0.0f, static_cast<f32>(viewportHeight), 0.0f, 1.0f);
	Shader.SetMatrices(identity, projection, identity);
	CmdBuffer->SetTransparencyPass(Transparent);
	CmdBuffer->SetOverlayPass(Transparent);
	CmdBuffer->SetShader(&Shader);
	CmdBuffer->PushFragmentConstants(&TextColor, sizeof(TextColor));
	CmdBuffer->BindTexture(&Textura);
	CmdBuffer->BindVertexBuffer(&VertexBuffer);
	CmdBuffer->Draw(static_cast<u32>(drawVertices.size()), 1, firstVertex, 0);
	CmdBuffer->SetOverlayPass(false);
	CmdBuffer->SetTransparencyPass(false);
	currentVertexCount += static_cast<u32>(drawVertices.size());
	++textCount;
}
void Font::font::Draw(const char16* wstr, f32 x, f32 y, f32 charW, f32 charH)
{
	DrawTextU(wstr, x, y, charW, charH);
}
void Font::font::Draw(f32 x, f32 y, f32 charW, f32 charH, const char16* format, ...)
{
	if (!format)
	{
		return;
	}
	DynamicString<wchar_t> wideFormat;
	for (const char16* code = format; *code; ++code)
	{
		wideFormat.push_back(static_cast<wchar_t>(*code));
	}
	va_list args;
	va_start(args, format);
	DynamicArray<wchar_t> formatted(256);
	bool success = false;
	while (formatted.size() <= 65536)
	{
		va_list copy;
		va_copy(copy, args);
		const int written = std::vswprintf(formatted.data(), formatted.size(), wideFormat.c_str(), copy);
		va_end(copy);
		if (written >= 0 && static_cast<size_t>(written) < formatted.size())
		{
			formatted.resize(static_cast<size_t>(written));
			success = true;
			break;
		}
		formatted.resize(formatted.size() * 2);
	}
	va_end(args);
	if (!success)
	{
		return;
	}
	DynamicString<char16> text;
	text.reserve(formatted.size());
	for (wchar_t code : formatted)
	{
		text.push_back(static_cast<char16>(code));
	}
	DrawTextU(text.c_str(), x, y, charW, charH);
}
void Font::font::RectDrawer()
{

}
void Font::font::SetColor(f32 r, f32 g, f32 b, f32 a)
{
	SetTextColor(r, g, b, a);
}
bool Font::font::IsValid() const
{
	return Display && CmdBuffer && ShaderLoaded && AtlasLoaded && Textura.IsValid() && VertexBuffer.IsValid();
}
void Font::font::InitializeDefaultCharset()
{
	CharsetBase = FirstChar;
	CharsetLUT.assign(LastChar - FirstChar + 1, -1);
	u32 index = 0;
	for (char16 code = FirstChar; code <= LastChar; ++code)
	{
		CharsetLUT[code - FirstChar] = static_cast<s32>(index++);
	}
	CharsetCount = index;
}
void Font::font::RebuildAtlasTexture()
{
	Textura.Finalize();
	AtlasLayersLoaded.clear();
	AtlasLoaded = false;
	if (!Display || !ShaderLoaded || !HasAtlasInfo)
	{
		return;
	}
	const u32 layers = GetAtlasLayerCount();
	if (layers == 0 || !Textura.InitializeArray(*Display, Shader, Info.Width, Info.Height, layers,
		gx::TextureFilter::Nearest, Info.Format))
	{
		return;
	}
	AtlasLayersLoaded.assign(layers, false);
}
u32 Font::font::GetAtlasLayerCount() const
{
	if (GridCols == 0 || GridRows == 0)
	{
		return 0;
	}
	const u64 charsPerLayer = static_cast<u64>(GridCols) * GridRows;
	const u64 layers = (static_cast<u64>(CharsetCount) + charsPerLayer - 1) / charsPerLayer;
	return layers == 0 ? 1u : static_cast<u32>(layers);
}