#pragma once
#include<Macros.h>
#include<Types.h>
#include<cstddef>
namespace Mem
{
	class MemBase : private NonCopy<>
	{
	public:
		MemBase();
		virtual ~MemBase();
		[[nodiscard]] virtual bool Initialize(void* Memory, std::size_t ByteCount) = 0;
		virtual void Finalize() noexcept = 0;
		[[nodiscard]] virtual bool IsInitialized() const noexcept = 0;
		[[nodiscard]] virtual void* Allocate(std::size_t ByteCount, std::size_t Alignment = alignof(std::max_align_t)) = 0;
		[[nodiscard]] virtual void* AllocateFromTail(std::size_t ByteCount, std::size_t Alignment = alignof(std::max_align_t)) = 0;
		[[nodiscard]] virtual bool Free(void* Memory) noexcept;
		virtual void FreeAll() noexcept = 0;
		virtual void FreeTail() noexcept = 0;
		[[nodiscard]] virtual std::size_t GetSize(const void*) const noexcept;
		[[nodiscard]] virtual std::size_t GetFreeSize() const noexcept = 0;
		[[nodiscard]] virtual std::size_t GetMaxFreeBlock() const noexcept = 0;
		[[nodiscard]] virtual std::size_t GetHeapSize() const noexcept = 0;
		[[nodiscard]] virtual std::size_t GetTotalFreeSize() const noexcept;
		[[nodiscard]] virtual std::size_t GetUsedSize() const noexcept;
		[[nodiscard]] void* Alloc(std::size_t ByteCount, std::size_t Alignment = alignof(std::max_align_t));
		[[nodiscard]] bool Deallocate(void* Memory) noexcept;
	};
}