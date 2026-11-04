#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include<Windows.h>
#include<MemAlcExp.h>
#include<MemGlobal.h>
#include<algorithm>
#include<atomic>
#include<cstddef>
#include<cstdint>
#include<limits>
#include<new>

namespace
{
	struct GlobalAllocationHeader
	{
		Mem::MemBase* Heap = nullptr;
		void* Allocation = nullptr;
	};
	thread_local Mem::MemBase* CurrentHeap = nullptr;

	Mem::MemAlcExp* GetGlobalExpHeap() noexcept
	{
		alignas(Mem::MemAlcExp) static std::byte HeapStorage[sizeof(Mem::MemAlcExp)];
		static std::atomic<Mem::MemAlcExp*> Heap{ nullptr };
		static std::atomic_flag CreateLock = ATOMIC_FLAG_INIT;
		Mem::MemAlcExp* heap = Heap.load(std::memory_order_acquire);
		if (heap != nullptr)
		{
			return heap;
		}
		while (CreateLock.test_and_set(std::memory_order_acquire))
		{
		}
		heap = Heap.load(std::memory_order_relaxed);
		if (heap == nullptr)
		{
			heap = ::new (static_cast<void*>(HeapStorage)) Mem::MemAlcExp();
			Heap.store(heap, std::memory_order_release);
		}
		CreateLock.clear(std::memory_order_release);
		return heap;
	}

	[[nodiscard]] void* AllocateGlobalMemory(std::size_t ByteCount, std::size_t Alignment)
	{
		Mem::MemBase* heap = CurrentHeap;
		Mem::MemAlcExp* globalHeap = GetGlobalExpHeap();
		if (heap == nullptr)
		{
			heap = globalHeap;
		}
		if (heap == globalHeap && !globalHeap->IsInitialized() && !Mem::InitializeGlobalHeap())
		{
			throw std::bad_alloc();
		}
		Alignment = (std::max)(Alignment, alignof(GlobalAllocationHeader));
		if (!Mem::Detail::IsPowerOfTwo(Alignment))
		{
			throw std::bad_alloc();
		}
		if (ByteCount == 0)
		{
			ByteCount = 1;
		}
		const std::size_t overhead = sizeof(GlobalAllocationHeader) + Alignment - 1;
		if (ByteCount > (std::numeric_limits<std::size_t>::max)() - overhead)
		{
			throw std::bad_alloc();
		}
		void* allocation = heap->Allocate(ByteCount + overhead, alignof(GlobalAllocationHeader));
		if (allocation == nullptr)
		{
			throw std::bad_alloc();
		}
		const std::uintptr_t firstUserAddress = reinterpret_cast<std::uintptr_t>(allocation) + sizeof(GlobalAllocationHeader);
		std::uintptr_t userAddress = 0;
		if (!Mem::Detail::AlignAddressUp(firstUserAddress, Alignment, userAddress))
		{
			(void)heap->Free(allocation);
			throw std::bad_alloc();
		}
		auto* header = reinterpret_cast<GlobalAllocationHeader*>(userAddress - sizeof(GlobalAllocationHeader));
		::new (static_cast<void*>(header)) GlobalAllocationHeader{ heap, allocation };
		return reinterpret_cast<void*>(userAddress);
	}

	void FreeGlobalMemory(void* Memory) noexcept
	{
		if (Memory != nullptr)
		{
			auto* header = reinterpret_cast<GlobalAllocationHeader*>(
				reinterpret_cast<std::uintptr_t>(Memory) - sizeof(GlobalAllocationHeader));
			Mem::MemBase* heap = header->Heap;
			void* allocation = header->Allocation;
			header->~GlobalAllocationHeader();
			if (heap != nullptr)
			{
				(void)heap->Free(allocation);
			}
		}
	}
}

namespace Mem
{
	MemBase* GetGlobalHeap() noexcept
	{
		return GetGlobalExpHeap();
	}

	MemBase* GetCurrentHeap() noexcept
	{
		return CurrentHeap;
	}

	MemBase* SetCurrentHeap(MemBase* Heap) noexcept
	{
		MemBase* previous = CurrentHeap;
		CurrentHeap = Heap;
		return previous;
	}

	CurrentHeapScope::CurrentHeapScope(MemBase& Heap) noexcept : PreviousHeap(SetCurrentHeap(&Heap))
	{
	}

	CurrentHeapScope::~CurrentHeapScope()
	{
		(void)SetCurrentHeap(PreviousHeap);
	}

	void* AllocateFromHeap(MemBase& Heap, std::size_t ByteCount, std::size_t Alignment)
	{
		return Heap.Allocate(ByteCount, Alignment);
	}

	void DeallocateFromHeap(MemBase& Heap, void* Memory) noexcept
	{
		(void)Heap.Free(Memory);
	}

	bool InitializeGlobalHeap(std::size_t ByteCount) noexcept
	{
		if (ByteCount == 0)
		{
			return false;
		}
		static std::atomic_flag InitializeLock = ATOMIC_FLAG_INIT;
		while (InitializeLock.test_and_set(std::memory_order_acquire))
		{
		}
		MemAlcExp* heap = GetGlobalExpHeap();
		const bool result = heap->IsInitialized()
			? heap->GetHeapSize() >= ByteCount
			: heap->InitializeOwned(ByteCount);
		InitializeLock.clear(std::memory_order_release);
		return result;
	}
}

void* operator new(std::size_t ByteCount)
{
	return AllocateGlobalMemory(ByteCount, alignof(std::max_align_t));
}

void* operator new[](std::size_t ByteCount)
{
	return AllocateGlobalMemory(ByteCount, alignof(std::max_align_t));
}

void* operator new(std::size_t ByteCount, std::align_val_t Alignment)
{
	return AllocateGlobalMemory(ByteCount, static_cast<std::size_t>(Alignment));
}

void* operator new[](std::size_t ByteCount, std::align_val_t Alignment)
{
	return AllocateGlobalMemory(ByteCount, static_cast<std::size_t>(Alignment));
}

void* operator new(std::size_t ByteCount, const std::nothrow_t&) noexcept
{
	try
	{
		return ::operator new(ByteCount);
	}
	catch (...)
	{
		return nullptr;
	}
}

void* operator new[](std::size_t ByteCount, const std::nothrow_t&) noexcept
{
	try
	{
		return ::operator new[](ByteCount);
	}
	catch (...)
	{
		return nullptr;
	}
}

void* operator new(std::size_t ByteCount, std::align_val_t Alignment, const std::nothrow_t&) noexcept
{
	try
	{
		return ::operator new(ByteCount, Alignment);
	}
	catch (...)
	{
		return nullptr;
	}
}

void* operator new[](std::size_t ByteCount, std::align_val_t Alignment, const std::nothrow_t&) noexcept
{
	try
	{
		return ::operator new[](ByteCount, Alignment);
	}
	catch (...)
	{
		return nullptr;
	}
}

void operator delete(void* Memory) noexcept { FreeGlobalMemory(Memory); }
void operator delete[](void* Memory) noexcept { FreeGlobalMemory(Memory); }
void operator delete(void* Memory, std::size_t) noexcept { FreeGlobalMemory(Memory); }
void operator delete[](void* Memory, std::size_t) noexcept { FreeGlobalMemory(Memory); }
void operator delete(void* Memory, std::align_val_t) noexcept { FreeGlobalMemory(Memory); }
void operator delete[](void* Memory, std::align_val_t) noexcept { FreeGlobalMemory(Memory); }
void operator delete(void* Memory, std::size_t, std::align_val_t) noexcept { FreeGlobalMemory(Memory); }
void operator delete[](void* Memory, std::size_t, std::align_val_t) noexcept { FreeGlobalMemory(Memory); }
void operator delete(void* Memory, const std::nothrow_t&) noexcept { FreeGlobalMemory(Memory); }
void operator delete[](void* Memory, const std::nothrow_t&) noexcept { FreeGlobalMemory(Memory); }
void operator delete(void* Memory, std::align_val_t, const std::nothrow_t&) noexcept { FreeGlobalMemory(Memory); }
void operator delete[](void* Memory, std::align_val_t, const std::nothrow_t&) noexcept { FreeGlobalMemory(Memory); }
