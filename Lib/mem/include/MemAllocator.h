#pragma once
#include <MemGlobal.h>
#include <cstddef>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>
namespace Mem
{
	template<typename T>
	class Allocator
	{
	public:
		using value_type = T;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using propagate_on_container_copy_assignment = std::false_type;
		using propagate_on_container_move_assignment = std::true_type;
		using propagate_on_container_swap = std::true_type;
		using is_always_equal = std::false_type;
		template<typename U>
		struct rebind
		{
			using other = Allocator<U>;
		};
		Allocator() noexcept : Heap(GetCurrentHeap()) {}
		explicit Allocator(MemBase& HeapRef) noexcept : Heap(&HeapRef) {}
		template<typename U>
		Allocator(const Allocator<U>& Other) noexcept : Heap(Other.Heap) {}
		[[nodiscard]] T* allocate(std::size_t Count)
		{
			if (Count > max_size())
			{
				throw std::bad_array_new_length();
			}
			const std::size_t byteCount = (Count == 0 ? 1 : Count) * sizeof(T);
			void* memory = Heap != nullptr ? Heap->Allocate(byteCount, alignof(T)) : AllocateFromGlobal(byteCount);
			if (memory == nullptr)
			{
				throw std::bad_alloc();
			}
			return static_cast<T*>(memory);
		}
		void deallocate(T* Memory, std::size_t) noexcept
		{
			if (Memory == nullptr)
			{
				return;
			}
			if (Heap != nullptr)
			{
				(void)Heap->Free(Memory);
			}
			else
			{
				FreeGlobal(Memory);
			}
		}
		template<typename U, typename... Args>
		void construct(U* Object, Args&&... Arguments)
		{
			MemBase* previousHeap = Heap != nullptr ? SetCurrentHeap(Heap) : nullptr;
			try
			{
				::new (static_cast<void*>(Object)) U(std::forward<Args>(Arguments)...);
			}
			catch (...)
			{
				if (Heap != nullptr)
				{
					(void)SetCurrentHeap(previousHeap);
				}
				throw;
			}
			if (Heap != nullptr)
			{
				(void)SetCurrentHeap(previousHeap);
			}
		}
		template<typename U>
		void destroy(U* Object) noexcept
		{
			Object->~U();
		}
		[[nodiscard]] constexpr std::size_t max_size() const noexcept
		{
			return (std::numeric_limits<std::size_t>::max)() / sizeof(T);
		}
		template<typename U>
		[[nodiscard]] bool operator==(const Allocator<U>& Other) const noexcept
		{
			return Heap == Other.Heap;
		}
		template<typename U>
		[[nodiscard]] bool operator!=(const Allocator<U>& Other) const noexcept
		{
			return !(*this == Other);
		}
		MemBase* Heap = nullptr;
	private:
		[[nodiscard]] static void* AllocateFromGlobal(std::size_t ByteCount)
		{
			if constexpr (alignof(T) > alignof(std::max_align_t))
			{
				return ::operator new(ByteCount, std::align_val_t(alignof(T)));
			}
			else
			{
				return ::operator new(ByteCount);
			}
		}
		static void FreeGlobal(T* Memory) noexcept
		{
			if constexpr (alignof(T) > alignof(std::max_align_t))
			{
				::operator delete(Memory, std::align_val_t(alignof(T)));
			}
			else
			{
				::operator delete(Memory);
			}
		}
		template<typename U>
		friend class Allocator;
	};
}