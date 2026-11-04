#pragma once
#include<MemBase.h>
#include<cstddef>
namespace Mem
{
	static constexpr std::size_t DefaultGlobalHeapSize = 24ull * 1024ull * 1024ull;
	[[nodiscard]] bool InitializeGlobalHeap(std::size_t ByteCount = DefaultGlobalHeapSize) noexcept;
	[[nodiscard]] MemBase* GetGlobalHeap() noexcept;
	[[nodiscard]] MemBase* GetCurrentHeap() noexcept;
	[[nodiscard]] MemBase* SetCurrentHeap(MemBase* Heap) noexcept;
	class CurrentHeapScope
	{
	public:
		explicit CurrentHeapScope(MemBase& Heap) noexcept;
		~CurrentHeapScope();
		CurrentHeapScope(const CurrentHeapScope&) = delete;
		CurrentHeapScope& operator=(const CurrentHeapScope&) = delete;
	private:
		MemBase* PreviousHeap;
	};
}