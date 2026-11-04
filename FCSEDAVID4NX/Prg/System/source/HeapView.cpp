#include<HeapView.h>
#ifdef _DEBUG
#include<MemGlobal.h>
#include<algorithm>
namespace
{
	template<typename Callback>
	void VisitAllocations(const Mem::MemAlcExp& Heap, Callback&& OnAllocation)
	{
		constexpr std::size_t batchCapacity = 32;
		std::array<Mem::MemBlockSnapshot, batchCapacity> blocks{};
		std::size_t blockCount = Heap.CopyBlockSnapshot(nullptr, 0);
		for (std::size_t firstIndex = 0; firstIndex < blockCount;)
		{
			const std::size_t requestCount = (std::min)(batchCapacity, blockCount - firstIndex);
			const std::size_t currentBlockCount = Heap.CopyBlockSnapshot(blocks.data(), requestCount, firstIndex);
			if (firstIndex >= currentBlockCount)
			{
				break;
			}
			const std::size_t copiedCount = (std::min)(requestCount, currentBlockCount - firstIndex);
			for (std::size_t i = 0; i < copiedCount; ++i)
			{
				if (!blocks[i].IsFree)
				{
					OnAllocation(blocks[i]);
				}
			}
			firstIndex += copiedCount;
			blockCount = currentBlockCount;
		}
	}
	std::size_t CountAllocations(const Mem::MemAlcExp& Heap)
	{
		std::size_t count = 0;
		VisitAllocations(Heap, [&count](const Mem::MemBlockSnapshot&)
		{
			++count;
		});
		return count;
	}
}
bool Game::HeapView::Initialize(gx::Display& Display, gx::Shader& DrawShader)
{
	if (!Console.Initialize(Display, DrawShader))
	{
		return false;
	}
	Console.SetPosition(24.0f, 0.0f);
	return true;
}
void Game::HeapView::Finalize()
{
	Console.Finalize();
}
void Game::HeapView::Toggle() noexcept
{
	Console.ToggleVisible();
	Console.Clear();
}
void Game::HeapView::Move(s32 HorizontalDirection, s32 VerticalDirection) noexcept
{
	Console.Move(HorizontalDirection, VerticalDirection);
}
bool Game::HeapView::IsVisible() const noexcept
{
	return Console.IsVisible();
}
void Game::HeapView::Draw(gx::CommandBuffer& CommandBuffer, Font::font& DebugFont, const std::array<Mem::MemAlcExp*, 6>& Heaps)
{
	if (!Console.IsVisible())
	{
		return;
	}
	Console.ClearLines();
	auto* rootHeap = static_cast<Mem::MemAlcExp*>(Mem::GetGlobalHeap());
	if (!rootHeap || !rootHeap->IsInitialized())
	{
		Console.Print(u"Memory heap is not initialized");
		Console.Print(u"F5: close heap summary | arrow keys move");
		Console.Draw(CommandBuffer, DebugFont);
		return;
	}
	static constexpr std::array<const char16*, 7> heapNames =
	{
		u"Root", u"System", u"Game", u"World", u"Archive", u"J2D", u"Command"
	};
	std::array<const Mem::MemAlcExp*, 7> allHeaps{};
	allHeaps[0] = rootHeap;
	for (std::size_t i = 0; i < Heaps.size(); ++i)
	{
		allHeaps[i + 1] = Heaps[i];
	}
	Console.Print(u"HEAP: USED/TOTAL KiB");
	for (std::size_t i = 0; i < allHeaps.size(); ++i)
	{
		const Mem::MemAlcExp* heap = allHeaps[i];
		if (!heap || !heap->IsInitialized())
		{
			Console.Print(u"%ls heap: not initialized", heapNames[i]);
			continue;
		}
		const std::size_t used = heap->GetUsedSize();
		const std::size_t size = heap->GetHeapSize();
		const std::size_t percent = size > 0 ? static_cast<std::size_t>((static_cast<u64>(used) * 100ULL) / size) : 0;
		Console.Print(u"%ls %llu/%llu %llu%% %llu %llu %llu", heapNames[i],
			static_cast<unsigned long long>(used / 1024),
			static_cast<unsigned long long>(size / 1024),
			static_cast<unsigned long long>(percent),
			static_cast<unsigned long long>(heap->GetTotalFreeSize() / 1024),
			static_cast<unsigned long long>(heap->GetMaxFreeBlock() / 1024),
			static_cast<unsigned long long>(CountAllocations(*heap)));
	}
	Console.Print(u"F5: close heap summary | arrow keys move");
	Console.Draw(CommandBuffer, DebugFont);
}
#endif
