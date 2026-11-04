#pragma once
#include <MemBase.h>
#include <MemTypes.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
namespace Mem
{
	class MemAlcLinear;
	struct State
	{
		const MemAlcLinear* Owner = nullptr;
		uptr Head = 0;
		uptr Tail = 0;
		u64 Epoch = 0;
	};
	class MemAlcLinear final : public MemBase
	{
	public:
		MemAlcLinear();
		~MemAlcLinear() override;
		[[nodiscard]] bool Initialize(void* Memory, std::size_t ByteCount) override;
		[[nodiscard]] bool Initialize(MemBase& Parent, std::size_t ByteCount);
		void Finalize() noexcept override;
		[[nodiscard]] void* Allocate(std::size_t ByteCount, std::size_t Alignment = BaseAlignment) override;
		[[nodiscard]] void* Alloc(std::size_t ByteCount, std::size_t Alignment = BaseAlignment);
		[[nodiscard]] void* AllocateFromTail(std::size_t ByteCount, std::size_t Alignment = BaseAlignment) override;
		[[nodiscard]] void* AllocFromTail(std::size_t ByteCount, std::size_t Alignment = BaseAlignment);
		void FreeTail() noexcept override;
		void FreeAll() noexcept override;
		[[nodiscard]] State SaveState() const noexcept;
		[[nodiscard]] bool RestoreState(const State& SavedState) noexcept;
		[[nodiscard]] std::size_t GetFreeSize() const noexcept override;
		[[nodiscard]] std::size_t GetHeadUsedSize() const noexcept;
		[[nodiscard]] std::size_t GetTailUsedSize() const noexcept;
		[[nodiscard]] std::size_t GetMaxFreeBlock() const noexcept override;
		[[nodiscard]] std::size_t GetHeapSize() const noexcept override;
		[[nodiscard]] bool IsInitialized() const noexcept override;
	private:
		[[nodiscard]] inline u64 NextEpoch(u64 Value) noexcept;
		static constexpr std::size_t BaseAlignment = alignof(std::max_align_t);
		mutable std::mutex Mutex;
		uptr BeginAddress;
		uptr EndAddress;
		uptr HeadAddress;
		uptr TailAddress;
		MemBase* ParentHeap;
		void* ParentAllocation;
		u64 Epoch;
	};
}