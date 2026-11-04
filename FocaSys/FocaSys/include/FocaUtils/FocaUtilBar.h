#pragma once
#ifndef FOCA_UTILS_DEBUG_PROCESS_BARS_H_
#define FOCA_UTILS_DEBUG_PROCESS_BARS_H_
#if defined(_DEBUG)
#include<Buffer.h>
#include<CommandBuffer.h>
#include<Display.h>
#include<Shader.h>
#include<Texture.h>
#include<Macros.h>
#include<Types.h>
#include<MemTypes.h>
#include<algorithm>
#include<array>
#include<cstddef>
#include<limits>
#include<Timer.h>
#include<glm/mat4x4.hpp>
#define UserBarCount 8
#define MaxIntervalMarkers 64
#define MaxIntervalMarkers 64
#define MaxRectangles 176
#define MaxVertices MaxRectangles * 6
#define FrameDuration 16666.0f
#define MaxMeterScale 10.0f
namespace FSys
{
	namespace Utils
	{
		struct Color
		{
			u8 R = 255;
			u8 G = 129;
			u8 B = 30;
			u8 A = 255;
		};
		struct CTime
		{
			u64 StartTime = 0;
			u32 Cost = 0;
			u32 PeakCost = 0;
			u32 PeakCounter = 0;
			Color BarColor{};
			bool Running = false;
			void Start(u8 red, u8 green, u8 blue)
			{
				BarColor = { red, green, blue };
				StartTime = Timer::getTick();
				Running = true;
			}
			void End()
			{
				if (!Running)
				{
					return;
				}
				const u64 endTime = Timer::getTick();
				const u64 elapsedTicks = endTime >= StartTime ? endTime - StartTime : 0;
				const u64 frequency = Timer::getFrequency();
				const u64 elapsedMicroseconds = frequency > 0 ? (elapsedTicks / frequency) * 1000000ULL + ((elapsedTicks % frequency) * 1000000ULL) / frequency : 0ULL;
				Cost = static_cast<u32>((std::clamp<u64>(elapsedMicroseconds, 1ULL, (std::numeric_limits<u32>::max)())));
				Running = false;
			}
			void UpdatePeak()
			{
				if (++PeakCounter >= 16 || Cost > PeakCost)
				{
					PeakCost = Cost;
					PeakCounter = 0;
				}
			}
			void UpdateUserPeak()
			{
				if (++PeakCounter >= 16 || Cost > PeakCost)
				{
					PeakCost = Cost;
					PeakCounter = 0;
				}
			}
			void Clear()
			{
				StartTime = 0;
				Cost = 0;
				PeakCost = 0;
				PeakCounter = 0;
				Running = false;
			}
		};
		struct DebugVertex
		{
			f32 Position[3]{};
			f32 Normal[3]{};
			f32 Color[4]{};
			f32 UV[2]{};
		};
		static_assert(sizeof(DebugVertex) == 48);
		class FocaUtilBar final : private NonCopy<>
		{
		public:
			FocaUtilBar();
			~FocaUtilBar();
			static FocaUtilBar* Create(gx::Display& display, gx::Shader& dedicatedDebugShader);
			static void Destroy();
			static FocaUtilBar* GetManager();
			bool Initialize(gx::Display& display, gx::Shader& dedicatedDebugShader);
			void Finalize();
			void Clear();
			void SetVisible(bool visible);
			void SetPosition(s32 x, s32 y);
			void SetWidth(s32 width);
			void SetBarWidth(s32 width);
			void SetBarHeight(s32 height);
			void SetCostFrame(u32 frameCount);
			[[nodiscard]] u32 GetGpCost() const;
			[[nodiscard]] u32 GetCpuCost() const;
			[[nodiscard]] u32 GetIdleCost() const;
			[[nodiscard]] u32 GetGpWaitCost() const;
			[[nodiscard]] u32 GetWholeLoopCost() const;
			[[nodiscard]] u32 GetUserCost(s32 index) const;
			void IdleStart();
			void IdleEnd();
			void GpStart();
			void GpEnd();
			void CpuStart();
			void CpuEnd();
			void GpWaitStart();
			void GpWaitEnd();
			void WholeLoopStart();
			void WholeLoopEnd();
			[[nodiscard]] s32 GetUnuseUserBar() const;
			s32 UserStart(u8 red, u8 green, u8 blue);
			void UserStart(s32 index, u8 red, u8 green, u8 blue);
			void UserEnd(s32 index);
			void Draw(gx::CommandBuffer& commandBuffer);
			void DrawProcessBar(gx::CommandBuffer& commandBuffer);
		void DrawDebugRectangle(gx::CommandBuffer& commandBuffer, f32 x, f32 y, f32 width, f32 height,
			const Color& color, f32 viewportWidth, f32 viewportHeight);
		private:
			static f32 TimeToPixels(u32 microseconds, f32 intervalWidth);
			static std::size_t GetIntervalCount(f32 width, f32 intervalWidth, f32 scaleX);
			static void AdjustMeterLength(u32 cost, f32& scale, u32& counter, f32 width, f32 scaleX);
			void AppendColorRectangle(f32 x, f32 y, f32 width, f32 height, const Color& color, f32 screenWidth, f32 screenHeight, std::size_t& vertexIndex);
			void AppendFrame(f32 x, f32 y, f32 width, f32 height, f32 thicknessX, f32 thicknessY, const Color& color, f32 screenWidth, f32 screenHeight, std::size_t& vertexIndex);
			void AppendTimeBar(f32 x, f32 y, f32 height, u32 microseconds, f32 intervalWidth, const Color& color, f32 screenWidth, f32 screenHeight, std::size_t& vertexIndex);
			void AppendRectangle(f32 x0, f32 y0, f32 x1, f32 y1, f32 red, f32 green, f32 blue, f32 alpha, f32 screenWidth, f32 screenHeight, std::size_t& vertexIndex);
			void AppendVertex(f32 x, f32 y, f32 red, f32 green, f32 blue, f32 alpha, std::size_t index);
			gx::Display* Display = nullptr;
			gx::Shader* DebugShader = nullptr;
			gx::Buffer VertexBuffer;
			gx::Buffer DebugRectangleBuffer;
			gx::Texture WhiteTexture;
			std::array<DebugVertex, MaxVertices> Vertices{};
			CTime Idle;
			CTime Gp;
			CTime Cpu;
			CTime GpWait;
			CTime WholeLoop;
			std::array<CTime, UserBarCount> Users{};
			u32 ActiveUsers = 0;
			u32 CostFrame = 0;
			f32 FrameScale = 8.0f;
			f32 UserScale = 10.0f;
			u32 FrameScaleCounter = 0;
			u32 UserScaleCounter = 0;
			s32 PositionX = -1;
			s32 PositionY = -1;
			s32 Width = 0;
			s32 BarWidthOverride = 0;
			bool Visible = true;
		inline static Mem::UniquePtr<FocaUtilBar> Manager;
		};
	}
}
#endif // defined(_DEBUG)
#endif // !FOCA_UTILS_DEBUG_PROCESS_BARS_H_
