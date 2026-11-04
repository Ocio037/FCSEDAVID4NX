#pragma once
#include<Types.h>
#include<MemBase.h>
#include<MemTypes.h>
#include<algorithm>
#include<cstddef>
#include<cstring>
#include<limits>
#include<mutex>
#include<new>
#include<utility>
namespace Mem
{
	enum class AllocationMode : u8
	{
		BestFit,
		FirstFit
	};
	struct alignas(std::max_align_t) MemBlock
	{
		[[nodiscard]] bool IsFreeBlock() const noexcept;
		[[nodiscard]] std::size_t GetSize() const noexcept;
		[[nodiscard]] std::size_t GetRequestedSize() const noexcept;
		[[nodiscard]] u8 GetGroupId() const noexcept;
		[[nodiscard]] MemBlock* GetPrevBlock() const noexcept;
		[[nodiscard]] MemBlock* GetNextBlock() const noexcept;
		[[nodiscard]] void* GetContent() noexcept;
		[[nodiscard]] const void* GetContent() const noexcept;
		std::size_t Span = 0;
		MemBlock* Previous = nullptr;
		MemBlock* Next = nullptr;
		std::size_t RequestedSize = 0;
		uptr UserAddress = 0;
		u8 GroupId = 0;
		bool IsFree = true;
		bool FromTail = false;
	};
	struct MemBlockSnapshot
	{
		uptr BlockAddress = 0;
		uptr ContentAddress = 0;
		uptr EndAddress = 0;
		uptr PreviousAddress = 0;
		uptr NextAddress = 0;
		std::size_t Span = 0;
		std::size_t Capacity = 0;
		std::size_t RequestedSize = 0;
		std::size_t Alignment = 0;
		u8 GroupId = 0;
		bool IsFree = true;
		bool FromTail = false;
	};
	struct alignas(std::max_align_t) AllocationPrefix
	{
		MemBlock* Block = nullptr;
	};
	struct Layout
	{
		uptr User = 0;
		uptr UsedStart = 0;
		uptr UsedEnd = 0;
		bool SplitFreeBlock = false;
	};
	static constexpr std::size_t HeaderAlignment = alignof(MemBlock);
	static constexpr std::size_t MinimumBlockSpan = sizeof(MemBlock) + sizeof(AllocationPrefix) + 1 + (HeaderAlignment - 1);
	class MemAlcExp final : public MemBase
	{
	public:
		MemAlcExp();
		~MemAlcExp() override;
		[[nodiscard]] bool Initialize(void* Memory, std::size_t ByteCount) override;
		[[nodiscard]] bool Initialize(MemBase& Parent, std::size_t ByteCount);
		[[nodiscard]] bool InitializeOwned(std::size_t ByteCount);
		void Finalize() noexcept override;
		[[nodiscard]] bool IsInitialized() const noexcept override;
		[[nodiscard]] void* Allocate(std::size_t ByteCount, std::size_t Alignment = alignof(std::max_align_t)) override;
		[[nodiscard]] void* AllocateFromTail(std::size_t ByteCount, std::size_t Alignment = alignof(std::max_align_t)) override;
		[[nodiscard]] bool Free(void* Memory) noexcept override;
		[[nodiscard]] void* Reallocate(void* Memory, std::size_t NewSize, std::size_t Alignment = alignof(std::max_align_t));
		void FreeAll() noexcept override;
		void FreeTail() noexcept override;
		[[nodiscard]] std::size_t FreeGroup(u8 GroupId) noexcept;
		[[nodiscard]] u8 ChangeGroupId(u8 GroupId) noexcept;
		void SetAllocationMode(AllocationMode Mode) noexcept;
		[[nodiscard]] std::size_t GetSize(const void* Memory) const noexcept override;
		[[nodiscard]] std::size_t GetFreeSize() const noexcept override;
		[[nodiscard]] std::size_t GetTotalFreeSize() const noexcept override;
		[[nodiscard]] std::size_t GetMaxFreeBlock() const noexcept override;
		[[nodiscard]] std::size_t GetUsedSize() const noexcept override;
		[[nodiscard]] std::size_t GetHeapSize() const noexcept override;
		[[nodiscard]] std::size_t CopyBlockSnapshot(MemBlockSnapshot* Output, std::size_t Capacity, std::size_t FirstIndex = 0) const noexcept;
		[[nodiscard]] bool Check() const noexcept;
	private:
		[[nodiscard]] inline u64 NextEpoch(u64 Value) noexcept;
		[[nodiscard]] std::size_t GetFreeSizeUnsafe() const noexcept;
		[[nodiscard]] bool BuildLayout(const MemBlock* Block, std::size_t ByteCount, std::size_t Alignment, bool FromTail, Layout& Result) const noexcept;
		[[nodiscard]] void* AllocateUnsafe(std::size_t ByteCount, std::size_t Alignment, bool FromTail, u8 GroupId);
		[[nodiscard]] MemBlock* FindBlockUnsafe(const void* Memory) const noexcept;
		void FreeBlockUnsafe(MemBlock* Block) noexcept;
		mutable std::mutex Mutex;
		uptr BaseAddress;
		uptr EndAddress;
		std::size_t TotalSize;
		MemBlock* FirstBlock;
		MemBase* ParentHeap;
		void* ParentAllocation;
		void* OwnedMemory;
		u8 CurrentGroupId;
		AllocationMode CurrentMode;
		u64 Epoch;
	};
}
