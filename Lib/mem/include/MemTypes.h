#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <utility>
namespace Mem
{
	class MemBase;
	[[nodiscard]] MemBase* SetCurrentHeap(MemBase* Heap) noexcept;
	[[nodiscard]] void* AllocateFromHeap(MemBase& Heap, std::size_t ByteCount, std::size_t Alignment);
	void DeallocateFromHeap(MemBase& Heap, void* Memory) noexcept;
	namespace Detail
	{
		[[nodiscard]] inline bool IsPowerOfTwo(std::size_t Value) noexcept
		{
			return Value != 0 && (Value & (Value - 1)) == 0;
		}
		[[nodiscard]] inline bool AlignAddressUp(std::uintptr_t Address, std::size_t Alignment, std::uintptr_t& Result) noexcept
		{
			if (!IsPowerOfTwo(Alignment) || Address > (std::numeric_limits<std::uintptr_t>::max)() - (Alignment - 1))
			{
				return false;
			}
			Result = (Address + Alignment - 1) & ~static_cast<std::uintptr_t>(Alignment - 1);
			return true;
		}
		[[nodiscard]] inline std::uintptr_t AlignAddressDown(std::uintptr_t Address, std::size_t Alignment) noexcept
		{
			return Address & ~static_cast<std::uintptr_t>(Alignment - 1);
		}
		[[nodiscard]] inline bool AddAddress(std::uintptr_t Address, std::size_t Size, std::uintptr_t& Result) noexcept
		{
			if (Address > (std::numeric_limits<std::uintptr_t>::max)() - Size)
			{
				return false;
			}
			Result = Address + Size;
			return true;
		}
	}
	template <typename T>
	struct MemDelete
	{
		MemBase* Heap = nullptr;
		void* Allocation = nullptr;
		MemDelete() = default;
		explicit MemDelete(MemBase& HeapRef, void* AllocationAddress = nullptr) noexcept : Heap(&HeapRef), Allocation(AllocationAddress) {}
		template<typename U>
		MemDelete(const MemDelete<U>& Other) noexcept : Heap(Other.Heap), Allocation(Other.Allocation) {}
		void operator()(T* Ptr) const noexcept
		{
			if (Ptr == nullptr)
			{
				return;
			}
			if (Heap == nullptr)
			{
				delete Ptr;
				return;
			}
			Ptr->~T();
			DeallocateFromHeap(*Heap, Allocation != nullptr ? Allocation : Ptr);
		}
		template<typename U>
		friend struct MemDelete;
	};
	template <typename T>
	using UniquePtr = std::unique_ptr<T, MemDelete<T>>;
	template<typename T, typename... Args>
	[[nodiscard]] inline UniquePtr<T> MakeUnique(Args&&... Arguments)
	{
		return UniquePtr<T>(new T(std::forward<Args>(Arguments)...));
	}
	template<typename T, typename... Args>
	[[nodiscard]] inline UniquePtr<T> MakeUnique(MemBase& Heap, Args&&... Arguments)
	{
		MemBase* previousHeap = SetCurrentHeap(&Heap);
		void* memory = nullptr;
		try
		{
			memory = AllocateFromHeap(Heap, sizeof(T), alignof(T));
			if (memory == nullptr)
			{
				throw std::bad_alloc();
			}
			T* object = ::new (memory) T(std::forward<Args>(Arguments)...);
			(void)SetCurrentHeap(previousHeap);
			return UniquePtr<T>(object, MemDelete<T>(Heap, memory));
		}
		catch (...)
		{
			if (memory != nullptr)
			{
				DeallocateFromHeap(Heap, memory);
			}
			(void)SetCurrentHeap(previousHeap);
			throw;
		}
	}
}