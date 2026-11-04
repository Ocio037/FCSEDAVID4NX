#pragma once
#include<MemBase.h>
#include<MemTypes.h>
#include<algorithm>
#include<cstddef>
#include<cstdint>
#include<mutex>
#include<new>
namespace Mem
{
	enum class Side : u8
	{
		Left,
		Right
	};
	class MemAlcDouble;
	struct State
	{
		const MemAlcDouble* Owner = nullptr;
		uptr Left = 0;
		uptr Right = 0;
		u64 Epoch = 0;
	};
	struct alignas(std::max_align_t) AllocationHeader
	{
		uptr PreviousCursor = 0;
		uptr UserAddress = 0;
		std::size_t RequestedSize = 0;
		Side AllocationSide = Side::Left;
	};
	class MemAlcDouble final : public MemBase
	{
	public:
		MemAlcDouble();
		~MemAlcDouble() override;
		[[nodiscard]] bool Initialize(void* Memory, std::size_t ByteCount) override;
		[[nodiscard]] bool Initialize(MemBase& Parent, std::size_t ByteCount);
		void Finalize() noexcept override;
		[[nodiscard]] void* Allocate(std::size_t ByteCount, Side From, std::size_t Alignment = BaseAlignment);
		[[nodiscard]] void* Allocate(std::size_t ByteCount, std::size_t Alignment = BaseAlignment) override;
		[[nodiscard]] void* AllocateFromLeft(std::size_t ByteCount, std::size_t Alignment = BaseAlignment);
		[[nodiscard]] void* AllocateFromRight(std::size_t ByteCount, std::size_t Alignment = BaseAlignment);
		[[nodiscard]] void* AllocateFromTail(std::size_t ByteCount, std::size_t Alignment = BaseAlignment) override;
		[[nodiscard]] void* AllocateFrom(std::size_t ByteCount, Side From, std::size_t Alignment = BaseAlignment);
		[[nodiscard]] bool Free(void* Memory) noexcept override;
		[[nodiscard]] bool Deallocate(void* Memory) noexcept;
		void FreeSide(Side Which) noexcept;
		void FreeAll() noexcept override;
		[[nodiscard]] State SaveState() const noexcept;
		[[nodiscard]] bool RestoreState(const State& SavedState) noexcept;
		[[nodiscard]] std::size_t GetFreeSize() const noexcept override;
		[[nodiscard]] std::size_t GetLeftUsedSize() const noexcept;
		[[nodiscard]] std::size_t GetRightUsedSize() const noexcept;
		[[nodiscard]] std::size_t GetMaxFreeBlock() const noexcept override;
		[[nodiscard]] std::size_t GetHeapSize() const noexcept override;
		[[nodiscard]] bool IsInitialized() const noexcept override;
		void FreeTail() noexcept override;
	private:
		[[nodiscard]] inline u64 NextNonZero(u64 Value) noexcept;
		[[nodiscard]] void* AllocateLeftUnsafe(std::size_t ByteCount, std::size_t Alignment) noexcept;
		[[nodiscard]] void* AllocateRightUnsafe(std::size_t ByteCount, std::size_t Alignment) noexcept;
		[[nodiscard]] AllocationHeader* FindAllocationUnsafe(const void* Memory) const noexcept;
		static constexpr std::size_t BaseAlignment = alignof(std::max_align_t);
		mutable std::mutex Mutex;
		uptr BeginAddress;
		uptr EndAddress;
		uptr LeftAddress;
		uptr RightAddress;
		MemBase* ParentHeap;
		void* ParentAllocation;
		u64 Epoch;
	};
}