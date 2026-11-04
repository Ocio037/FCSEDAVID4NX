#pragma once
#ifndef GX_DISPLAY_H_
#define GX_DISPLAY_H_
#include<Macros.h>
#include<Types.h>
#include<MemTypes.h>
#include<Detail/Displayinfo.h>
namespace gx {
	namespace internal {
		struct Device;
	}
	class CommandBuffer;
	struct DisplayData;
	class Display final : private NonCopy<>
	{
	public:
		Display();
		explicit Display(const detail::DisplayInfo& Info);
		~Display();
		void Initialize(const detail::DisplayInfo& Info);
		void Finalize();
		[[nodiscard]] const internal::Device* GetDeviceInfo() const;
		[[nodiscard]] detail::DisplayResolution GetResolution() const;
	private:
		Mem::UniquePtr<DisplayData>Data;
	};
}
#endif // !GX_DISPLAY_H_
