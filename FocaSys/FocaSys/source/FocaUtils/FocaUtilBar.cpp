#include<FocaUtils/FocaUtilBar.h>
FSys::Utils::FocaUtilBar::FocaUtilBar()
{

}
FSys::Utils::FocaUtilBar::~FocaUtilBar()
{
	Finalize();
}
FSys::Utils::FocaUtilBar* FSys::Utils::FocaUtilBar::Create(gx::Display& display, gx::Shader& dedicatedDebugShader)
{
	if (!Manager)
	{
		Manager = Mem::MakeUnique<FocaUtilBar>();
	}
	if (!Manager || !Manager->Initialize(display, dedicatedDebugShader))
	{
		Destroy();
		return nullptr;
	}
	return Manager.get();
}
void FSys::Utils::FocaUtilBar::Destroy()
{
	Manager.reset();
}
FSys::Utils::FocaUtilBar* FSys::Utils::FocaUtilBar::GetManager()
{
	return Manager.get();
}
bool FSys::Utils::FocaUtilBar::Initialize(gx::Display& display, gx::Shader& dedicatedDebugShader)
{
	Finalize();
	Display = &display;
	DebugShader = &dedicatedDebugShader;
	VertexBuffer.Initialize(display);
	DebugRectangleBuffer.Initialize(display);
	constexpr std::array<u8, 4> whitePixel = { 255, 255, 255, 255 };
	if (!WhiteTexture.Initialize(display, dedicatedDebugShader, 1, 1, whitePixel.data()))
	{
		Finalize();
		return false;
	}
	if (!VertexBuffer.CreateVertexBuffer(Vertices.data(), sizeof(Vertices)))
	{
		Finalize();
		return false;
	}
	if (!DebugRectangleBuffer.CreateVertexBuffer(Vertices.data(), sizeof(DebugVertex) * 6))
	{
		Finalize();
		return false;
	}
	return true;
}
void FSys::Utils::FocaUtilBar::Finalize()
{
	WhiteTexture.Finalize();
	DebugRectangleBuffer.Finalize();
	VertexBuffer.Finalize();
	Display = nullptr;
	DebugShader = nullptr;
	Clear();
}
void FSys::Utils::FocaUtilBar::Clear()
{
	Idle.Clear();
	Gp.Clear();
	Cpu.Clear();
	GpWait.Clear();
	WholeLoop.Clear();
	for (CTime& user : Users)
	{
		user.Clear();
	}
	ActiveUsers = 0;
	CostFrame = 0;
	FrameScale = 8.0f;
	UserScale = 10.0f;
	FrameScaleCounter = 0;
	UserScaleCounter = 0;
}
void FSys::Utils::FocaUtilBar::SetVisible(bool visible)
{
	Visible = visible;
}
void FSys::Utils::FocaUtilBar::SetPosition(s32 x, s32 y)
{
	PositionX = x;
	PositionY = y;
}
void FSys::Utils::FocaUtilBar::SetWidth(s32 width)
{
	Width = width;
}
void FSys::Utils::FocaUtilBar::SetBarWidth(s32 width)
{
	BarWidthOverride = (std::max)(width, 1);
}
void FSys::Utils::FocaUtilBar::SetBarHeight(s32 height)
{
	SetBarWidth(height);
}
void FSys::Utils::FocaUtilBar::SetCostFrame(u32 frameCount)
{
	CostFrame = frameCount;
}
u32 FSys::Utils::FocaUtilBar::GetGpCost() const
{
	return Gp.Cost;
}
u32 FSys::Utils::FocaUtilBar::GetCpuCost() const
{
	return Cpu.Cost;
}
u32 FSys::Utils::FocaUtilBar::GetIdleCost() const
{
	return Idle.Cost;
}
u32 FSys::Utils::FocaUtilBar::GetGpWaitCost() const
{
	return GpWait.Cost;
}
u32 FSys::Utils::FocaUtilBar::GetWholeLoopCost() const
{
	return WholeLoop.Cost;
}
u32 FSys::Utils::FocaUtilBar::GetUserCost(s32 index) const
{
	return index >= 0 && index < static_cast<s32>(UserBarCount) ? Users[index].Cost : 0;
}
void FSys::Utils::FocaUtilBar::IdleStart()
{
	Idle.Start(255, 129, 30);
}
void FSys::Utils::FocaUtilBar::IdleEnd()
{
	Idle.End();
}
void FSys::Utils::FocaUtilBar::GpStart()
{
	Gp.Start(255, 129, 30);
}
void FSys::Utils::FocaUtilBar::GpEnd()
{
	Gp.End();
}
void FSys::Utils::FocaUtilBar::CpuStart()
{
	Cpu.Start(255, 129, 30);
}
void FSys::Utils::FocaUtilBar::CpuEnd()
{
	Cpu.End();
}
void FSys::Utils::FocaUtilBar::GpWaitStart()
{
	GpWait.Start(255, 129, 30);
}
void FSys::Utils::FocaUtilBar::GpWaitEnd()
{
	GpWait.End();
}
void FSys::Utils::FocaUtilBar::WholeLoopStart()
{
	WholeLoop.Start(255, 129, 30);
}
void FSys::Utils::FocaUtilBar::WholeLoopEnd()
{
	WholeLoop.End();
}
s32 FSys::Utils::FocaUtilBar::GetUnuseUserBar() const
{
	for (s32 index = 0; index < static_cast<s32>(UserBarCount); ++index)
	{
		if ((ActiveUsers & (1u << index)) == 0)
		{
			return index;
		}
	}
	return -1;
}
s32 FSys::Utils::FocaUtilBar::UserStart(u8 red, u8 green, u8 blue)
{
	const s32 index = GetUnuseUserBar();
	if (index >= 0)
	{
		UserStart(index, red, green, blue);
	}
	return index;
}
void FSys::Utils::FocaUtilBar::UserStart(s32 index, u8 red, u8 green, u8 blue)
{
	if (index < 0 || index >= static_cast<s32>(UserBarCount))
	{
		return;
	}
	ActiveUsers |= 1u << index;
	Users[index].Start(red, green, blue);
}
void FSys::Utils::FocaUtilBar::UserEnd(s32 index)
{
	if (index >= 0 && index < static_cast<s32>(UserBarCount))
	{
		Users[index].End();
	}
}
void FSys::Utils::FocaUtilBar::Draw(gx::CommandBuffer& commandBuffer)
{
	DrawProcessBar(commandBuffer);
}
void FSys::Utils::FocaUtilBar::DrawProcessBar(gx::CommandBuffer& commandBuffer)
{
	if (!Visible || !Display || !DebugShader || !WhiteTexture.IsValid())
	{
		return;
	}
	const gx::detail::DisplayResolution resolution = Display->GetResolution();
	if (resolution.Width == 0 || resolution.Height == 0)
	{
		return;
	}
	Idle.UpdatePeak();
	Gp.UpdatePeak();
	Cpu.UpdatePeak();
	u32 maxUserPeak = 0;
	for (std::size_t i = 0; i < UserBarCount; ++i)
	{
		Users[i].UpdateUserPeak();
		maxUserPeak = (std::max)(maxUserPeak, Users[i].PeakCost);
	}
	const f32 screenWidth = static_cast<f32>(resolution.Width);
	const f32 screenHeight = static_cast<f32>(resolution.Height);
	const bool highResolution = screenHeight > 400.0f;
	const f32 scaleX = screenWidth / 640.0f;
	const f32 scaleY = screenHeight / (highResolution ? 480.0f : 240.0f);
	const f32 defaultBarWidth = highResolution ? 2.0f : 1.0f;
	const f32 barUnit = static_cast<f32>(BarWidthOverride > 0 ? BarWidthOverride : static_cast<s32>(defaultBarWidth)) * scaleY;
	const f32 barWidth = static_cast<f32>(Width > 0 ? Width : 562) * scaleX;
	const f32 barX = PositionX >= 0 ? static_cast<f32>(PositionX) * scaleX : static_cast<f32>((static_cast<s32>(screenWidth) - static_cast<s32>(barWidth)) / 2);
	const f32 barY = PositionY >= 0 ? static_cast<f32>(PositionY) * scaleY : screenHeight - (highResolution ? 40.0f : 20.0f) * scaleY;
	const f32 panelHeight = barUnit * 8.0f;
	const f32 rowHeight = barUnit * 2.0f;
	const f32 userY = PositionY >= 0 ? barY - barUnit * 15.0f : screenHeight - (highResolution ? 70.0f : 35.0f) * scaleY;
	AdjustMeterLength(WholeLoop.Cost, FrameScale, FrameScaleCounter, barWidth, scaleX);
	const f32 intervalWidth = static_cast<f32>(static_cast<u32>(FrameScale * 20.0f)) * scaleX;
	const std::size_t intervalCount = GetIntervalCount(barWidth, intervalWidth, scaleX);
	const f32 borderWidthX = (std::max)(scaleX, barUnit * 0.5f);
	const f32 borderWidthY = (std::max)(scaleY, barUnit * 0.5f);
	const f32 lineHeight = scaleY;
	const u32 gpuTime = Gp.Cost > GpWait.Cost ? Gp.Cost - GpWait.Cost : 0;
	const u32 totalTime = gpuTime > Cpu.Cost ? gpuTime - Cpu.Cost : 0;
	const f32 gpuWidth = TimeToPixels(gpuTime, intervalWidth);
	const f32 cpuStart = barX + scaleX + TimeToPixels(totalTime, intervalWidth);
	const f32 idleStart = barX + scaleX + TimeToPixels(Gp.Cost, intervalWidth);
	std::size_t vertexIndex = 0;
	const Color background{ 0, 0, 50, 200 };
	const Color frame{ 50, 50, 150, 255 };
	AppendColorRectangle(barX, barY, barWidth, panelHeight, background, screenWidth, screenHeight, vertexIndex);
	AppendFrame(barX, barY, barWidth, panelHeight, borderWidthX, borderWidthY, frame, screenWidth, screenHeight, vertexIndex);
	const bool targetOverrun = CostFrame > intervalCount;
	const f32 targetWidth = targetOverrun ? barWidth : static_cast<f32>(static_cast<u64>(CostFrame) * static_cast<u32>(intervalWidth)) + 2.0f * scaleX;
	const Color targetColor = targetOverrun ? Color{ 250, 0, 0, 200 } : Color{ 0, 250, 250, 200 };
	AppendColorRectangle(barX, barY + panelHeight + lineHeight, targetWidth, lineHeight, targetColor, screenWidth, screenHeight, vertexIndex);
	const f32 wholeLoopWidth = TimeToPixels(WholeLoop.Cost, intervalWidth);
	const Color wholeLoopColor = wholeLoopWidth > barWidth ? Color{ 255, 100, 0, 255 } : Color{ 50, 255, 0, 255 };
	AppendColorRectangle(barX, barY, (std::min)(wholeLoopWidth, barWidth), lineHeight, wholeLoopColor, screenWidth, screenHeight, vertexIndex);
	const f32 gpuY = barY + barUnit;
	AppendTimeBar(barX + scaleX, gpuY, rowHeight, gpuTime, intervalWidth, Color{ 80, 255, 80, 255 }, screenWidth, screenHeight, vertexIndex);
	const f32 waitX = barX + scaleX + gpuWidth;
	AppendColorRectangle(waitX, gpuY, TimeToPixels(GpWait.Cost, intervalWidth), rowHeight, Color{ 0, 255, 0, 255 }, screenWidth, screenHeight, vertexIndex);
	const f32 cpuWidth = Cpu.Cost > 0 ? (std::max)(scaleX, TimeToPixels(Cpu.Cost, intervalWidth)) : 0.0f;
	AppendColorRectangle(cpuStart, gpuY + rowHeight, cpuWidth, rowHeight, Color{ 255, 80, 80, 255 }, screenWidth, screenHeight, vertexIndex);
	AppendTimeBar(idleStart, gpuY + rowHeight * 2.0f, rowHeight, Idle.Cost, intervalWidth, Color{ 180, 180, 160, 255 }, screenWidth, screenHeight, vertexIndex);
	const Color minorTick{ 100, 100, 255, 255 };
	const Color majorTick{ 180, 255, 255, 255 };
	const f32 tickWidth = 2.0f * scaleX;
	for (std::size_t i = 1; i < intervalCount && i <= MaxIntervalMarkers; ++i)
	{
		const f32 tickX = barX + static_cast<f32>(i) * intervalWidth + scaleX;
		AppendColorRectangle(tickX, gpuY, tickWidth, panelHeight - barUnit * 2.0f, (i % 5) != 0 ? minorTick : majorTick, screenWidth, screenHeight, vertexIndex);
	}
	if (maxUserPeak > 0)
	{
		for (CTime& user : Users)
		{
			user.UpdateUserPeak();
		}
		AdjustMeterLength(maxUserPeak, UserScale, UserScaleCounter, barWidth, scaleX);
		const f32 userIntervalWidth = static_cast<f32>(static_cast<u32>(UserScale * 20.0f)) * scaleX;
		const std::size_t userIntervalCount = GetIntervalCount(barWidth, userIntervalWidth, scaleX);
		const f32 userHeight = barUnit * 10.0f;
		AppendColorRectangle(barX, userY, barWidth, userHeight, background, screenWidth, screenHeight, vertexIndex);
		AppendFrame(barX, userY, barWidth, userHeight, borderWidthX, borderWidthY, frame, screenWidth, screenHeight, vertexIndex);
		for (std::size_t i = 0; i < UserBarCount; ++i)
		{
			CTime& user = Users[i];
			if (user.Cost > 0 || user.PeakCost > 0)
			{
				const f32 y = userY + barUnit + static_cast<f32>(i) * barUnit;
				AppendTimeBar(barX + scaleX, y, barUnit, user.Cost, userIntervalWidth, user.BarColor, screenWidth, screenHeight, vertexIndex);
				const f32 peakWidth = TimeToPixels(user.PeakCost, userIntervalWidth);
				const f32 markerWidth = 3.0f * scaleX;
				const f32 markerDrawWidth = (std::min)(peakWidth, markerWidth);
				const f32 markerX = peakWidth < markerWidth ? barX : barX + peakWidth - markerWidth;
				AppendColorRectangle(markerX, y, markerDrawWidth, barUnit, Color{ 255, 200, 50, 255 }, screenWidth, screenHeight, vertexIndex);
			}
			user.Cost = 0;
		}
		for (std::size_t i = 1; i < userIntervalCount && i <= MaxIntervalMarkers; ++i)
		{
			const f32 tickX = barX + static_cast<f32>(i) * userIntervalWidth + scaleX;
			AppendColorRectangle(tickX, userY + barUnit, tickWidth, userHeight - barUnit * 2.0f, (i % 5) != 0 ? minorTick : majorTick, screenWidth, screenHeight, vertexIndex);
		}
	}
	else
	{
		for (CTime& user : Users)
		{
			user.Cost = 0;
		}
	}
	ActiveUsers = 0;

	if (!VertexBuffer.UpdateVertexBuffer(Vertices.data(), sizeof(Vertices)))
	{
		return;
	}
	const glm::mat4 identity(1.0f);
	DebugShader->SetMatrices(identity, identity, identity);
	commandBuffer.SetOverlayPass(true);
	commandBuffer.SetShader(DebugShader);
	commandBuffer.BindVertexBuffer(&VertexBuffer);
	commandBuffer.BindTexture(&WhiteTexture);
	commandBuffer.Draw(static_cast<u32>(vertexIndex), 1, 0, 0);
	commandBuffer.SetOverlayPass(false);
}
void FSys::Utils::FocaUtilBar::DrawDebugRectangle(gx::CommandBuffer& commandBuffer, f32 x, f32 y, f32 width, f32 height,
	const Color& color, f32 viewportWidth, f32 viewportHeight)
{
	if (!Display || !DebugShader || !WhiteTexture.IsValid() || viewportWidth <= 0.0f || viewportHeight <= 0.0f)
	{
		return;
	}
	std::size_t vertexIndex = 0;
	AppendColorRectangle(x, y, width, height, color, viewportWidth, viewportHeight, vertexIndex);
	if (vertexIndex == 0 || !DebugRectangleBuffer.UpdateVertexBuffer(Vertices.data(), static_cast<u64>(vertexIndex) * sizeof(DebugVertex)))
	{
		return;
	}
	const glm::mat4 identity(1.0f);
	DebugShader->SetMatrices(identity, identity, identity);
	commandBuffer.SetOverlayPass(true);
	commandBuffer.SetShader(DebugShader);
	commandBuffer.BindVertexBuffer(&DebugRectangleBuffer);
	commandBuffer.BindTexture(&WhiteTexture);
	commandBuffer.Draw(static_cast<u32>(vertexIndex), 1, 0, 0);
	commandBuffer.SetOverlayPass(false);
}
f32 FSys::Utils::FocaUtilBar::TimeToPixels(u32 microseconds, f32 intervalWidth)
{
	const u64 pixels = static_cast<u64>(microseconds) * static_cast<u32>(intervalWidth) / static_cast<u64>(FrameDuration);
	return static_cast<f32>(pixels);
}
std::size_t FSys::Utils::FocaUtilBar::GetIntervalCount(f32 width, f32 intervalWidth, f32 scaleX)
{
	if (intervalWidth <= 0.0f)
	{
		return 1;
	}
	const f32 count = (width - 4.0f * scaleX + intervalWidth) / intervalWidth;
	return static_cast<std::size_t>((std::clamp)(count, 1.0f, static_cast<f32>(MaxIntervalMarkers + 1)));
}
void FSys::Utils::FocaUtilBar::AdjustMeterLength(u32 cost, f32& scale, u32& counter, f32 width, f32 scaleX)
{
	bool reduced = false;
	while (scale > 1.0f && static_cast<f32>(cost) * scale * 20.0f * scaleX / FrameDuration > width - 30.0f * scaleX)
	{
		scale = (std::max)(1.0f, scale - 0.1f);
		reduced = true;
	}
	if (scale >= MaxMeterScale)
	{
		counter = 0;
	}
	if (scale > MaxMeterScale - 0.2f)
	{
		scale = MaxMeterScale;
	}
	if (!reduced && scale < MaxMeterScale)
	{
		++counter;
		if (counter >= 30 && static_cast<f32>(cost) * scale * 20.0f * scaleX / FrameDuration < width - 60.0f * scaleX)
		{
			scale = (std::min)(MaxMeterScale, scale + 0.2f);
		}
	}
}
void FSys::Utils::FocaUtilBar::AppendColorRectangle(f32 x, f32 y, f32 width, f32 height, const Color& color, f32 screenWidth, f32 screenHeight, std::size_t& vertexIndex)
{
	if (width <= 0.0f || height <= 0.0f || vertexIndex + 6 > Vertices.size())
	{
		return;
	}
	AppendRectangle(x, y, x + width, y + height, static_cast<f32>(color.R) / 255.0f, static_cast<f32>(color.G) / 255.0f, static_cast<f32>(color.B) / 255.0f, static_cast<f32>(color.A) / 255.0f, screenWidth, screenHeight, vertexIndex);
}
void FSys::Utils::FocaUtilBar::AppendFrame(f32 x, f32 y, f32 width, f32 height, f32 thicknessX, f32 thicknessY, const Color& color, f32 screenWidth, f32 screenHeight, std::size_t& vertexIndex)
{
	const f32 horizontalThickness = (std::min)(thicknessY, height * 0.5f);
	const f32 verticalThickness = (std::min)(thicknessX, width * 0.5f);
	AppendColorRectangle(x, y, width, horizontalThickness, color, screenWidth, screenHeight, vertexIndex);
	AppendColorRectangle(x, y + height - horizontalThickness, width, horizontalThickness, color, screenWidth, screenHeight, vertexIndex);
	AppendColorRectangle(x, y + horizontalThickness, verticalThickness, height - horizontalThickness * 2.0f, color, screenWidth, screenHeight, vertexIndex);
	AppendColorRectangle(x + width - verticalThickness, y + horizontalThickness, verticalThickness, height - horizontalThickness * 2.0f, color, screenWidth, screenHeight, vertexIndex);
}
void FSys::Utils::FocaUtilBar::AppendTimeBar(f32 x, f32 y, f32 height, u32 microseconds, f32 intervalWidth, const Color& color, f32 screenWidth, f32 screenHeight, std::size_t& vertexIndex)
{
	AppendColorRectangle(x, y, TimeToPixels(microseconds, intervalWidth), height, color, screenWidth, screenHeight, vertexIndex);
}
void FSys::Utils::FocaUtilBar::AppendRectangle(f32 x0, f32 y0, f32 x1, f32 y1, f32 red, f32 green, f32 blue, f32 alpha, f32 screenWidth, f32 screenHeight, std::size_t& vertexIndex)
{
	const f32 left = x0 * 2.0f / screenWidth - 1.0f;
	const f32 top = y0 * 2.0f / screenHeight - 1.0f;
	const f32 right = x1 * 2.0f / screenWidth - 1.0f;
	const f32 bottom = y1 * 2.0f / screenHeight - 1.0f;
	AppendVertex(left, top, red, green, blue, alpha, vertexIndex++);
	AppendVertex(right, top, red, green, blue, alpha, vertexIndex++);
	AppendVertex(right, bottom, red, green, blue, alpha, vertexIndex++);
	AppendVertex(left, top, red, green, blue, alpha, vertexIndex++);
	AppendVertex(right, bottom, red, green, blue, alpha, vertexIndex++);
	AppendVertex(left, bottom, red, green, blue, alpha, vertexIndex++);
}
void FSys::Utils::FocaUtilBar::AppendVertex(f32 x, f32 y, f32 red, f32 green, f32 blue, f32 alpha, std::size_t index)
{
	DebugVertex& vertex = Vertices[index];
	vertex.Position[0] = x;
	vertex.Position[1] = y;
	vertex.Position[2] = 0.0f;
	vertex.Color[0] = red;
	vertex.Color[1] = green;
	vertex.Color[2] = blue;
	vertex.Color[3] = alpha;
	vertex.UV[0] = 0.5f;
	vertex.UV[1] = 0.5f;
}
