#include<MemoryDebugConsole.h>
#ifdef _DEBUG
#include<algorithm>
#include<array>
namespace
{
	constexpr std::size_t SnapshotBatchCapacity = 32;
	constexpr std::size_t VisibleReportLineCount = 26;
	struct HeapReport
	{
		const Mem::MemAlcExp* Heap = nullptr;
		const char16* Name = nullptr;
		std::size_t AllocationCount = 0;
		std::size_t FreeBlockCount = 0;
		std::size_t UsedBytes = 0;
		std::size_t TotalLines = 0;
	};
	template<typename Callback>
	void VisitBlocks(const Mem::MemAlcExp& Heap, Callback&& OnBlock)
	{
		std::array<Mem::MemBlockSnapshot, SnapshotBatchCapacity> blocks{};
		std::size_t blockCount = Heap.CopyBlockSnapshot(nullptr, 0);
		for (std::size_t firstIndex = 0; firstIndex < blockCount;)
		{
			const std::size_t requestCount = (std::min)(SnapshotBatchCapacity, blockCount - firstIndex);
			const std::size_t currentBlockCount = Heap.CopyBlockSnapshot(blocks.data(), requestCount, firstIndex);
			if (firstIndex >= currentBlockCount)
			{
				break;
			}
			const std::size_t copiedCount = (std::min)(requestCount, currentBlockCount - firstIndex);
			for (std::size_t i = 0; i < copiedCount; ++i)
			{
				OnBlock(blocks[i]);
			}
			firstIndex += copiedCount;
			blockCount = currentBlockCount;
		}
	}
	void CollectHeapReport(HeapReport& Report)
	{
		if (!Report.Heap || !Report.Heap->IsInitialized())
		{
			return;
		}
		VisitBlocks(*Report.Heap, [&Report](const Mem::MemBlockSnapshot& Block)
		{
			if (Block.IsFree)
			{
				++Report.FreeBlockCount;
				return;
			}
			++Report.AllocationCount;
			Report.UsedBytes += sizeof(Mem::MemBlock) + Block.RequestedSize + Block.Alignment;
		});
		Report.TotalLines = 5 + (std::max)(static_cast<std::size_t>(1), Report.AllocationCount)
			+ (std::max)(static_cast<std::size_t>(1), Report.FreeBlockCount);
	}
}
bool Game::MemoryDebugConsole::Initialize(gx::Display& Display, gx::Shader& DrawShader)
{
	if (!Console.Initialize(Display, DrawShader))
	{
		return false;
	}
	Console.SetPosition(-8.0f, 0.0f);
	Console.SetTextScale(0.75f);
	return true;
}
void Game::MemoryDebugConsole::Finalize()
{
	Console.Finalize();
}
void Game::MemoryDebugConsole::Toggle() noexcept
{
	Console.ToggleVisible();
	Console.Clear();
}
void Game::MemoryDebugConsole::Move(s32 HorizontalDirection, s32 VerticalDirection) noexcept
{
	Console.Move(HorizontalDirection, VerticalDirection);
}
void Game::MemoryDebugConsole::ScrollPage(s32 Direction) noexcept
{
	Console.ScrollPage(Direction);
}
bool Game::MemoryDebugConsole::IsVisible() const noexcept
{
	return Console.IsVisible();
}
void Game::MemoryDebugConsole::Draw(gx::CommandBuffer& CommandBuffer, Font::font& DebugFont, const std::array<Mem::MemAlcExp*, 6>& Heaps)
{
	if (!Console.IsVisible())
	{
		return;
	}
	Console.ClearLines();
	auto* rootHeap = static_cast<Mem::MemAlcExp*>(Mem::GetGlobalHeap());
	if (!rootHeap || !rootHeap->IsInitialized())
	{
		Console.SetScrollRange(0);
		Console.PrintLine(u" attr  address:   size    gid aln   prev_ptr next_ptr");
		Console.PrintLine(u"(Used Blocks)");
		Console.PrintLine(u" NONE");
		Console.PrintLine(u"(Free Blocks)");
		Console.PrintLine(u" NONE");
		Console.PrintLine(u"Heap is not initialized");
		Console.Draw(CommandBuffer, DebugFont);
		return;
	}
	static constexpr std::array<const char16*, 7> heapNames =
	{
		u"Root Heap", u"System Heap", u"Game Heap", u"World Heap", u"Archive Heap", u"J2D Heap", u"Command Heap"
	};
	std::array<HeapReport, 7> reports{};
	reports[0] = HeapReport{ rootHeap, heapNames[0] };
	for (std::size_t i = 0; i < Heaps.size(); ++i)
	{
		reports[i + 1] = HeapReport{ Heaps[i], heapNames[i + 1] };
	}
	std::size_t totalLineCount = 1;
	for (HeapReport& report : reports)
	{
		CollectHeapReport(report);
		totalLineCount += report.TotalLines;
	}
	const std::size_t maximumFirstLine = totalLineCount > VisibleReportLineCount
		? totalLineCount - VisibleReportLineCount
		: 0;
	Console.SetScrollRange(maximumFirstLine);
	const std::size_t firstLine = Console.GetScrollOffset();
	const std::size_t endLine = firstLine + VisibleReportLineCount;
	std::size_t reportLine = 0;
	auto addLine = [this, &reportLine, firstLine, endLine](const char16* Text)
	{
		if (reportLine >= firstLine && reportLine < endLine)
		{
			Console.PrintLine(Text);
		}
		++reportLine;
	};
	auto addFormattedLine = [this, &reportLine, firstLine, endLine](const char16* Format, auto... Arguments)
	{
		if (reportLine >= firstLine && reportLine < endLine)
		{
			Console.Print(Format, Arguments...);
		}
		++reportLine;
	};
	for (const HeapReport& report : reports)
	{
		if (!report.Heap || !report.Heap->IsInitialized())
		{
			continue;
		}
		const std::size_t heapSize = report.Heap->GetHeapSize();
		const f32 usedPercent = heapSize != 0
			? static_cast<f32>(report.UsedBytes) * 100.0f / static_cast<f32>(heapSize)
			: 0.0f;
		addLine(report.Name);
		addLine(u" attr  address:   size    gid aln   prev_ptr next_ptr");
		addLine(u"(Used Blocks)");
		if (report.AllocationCount == 0)
		{
			addLine(u" NONE");
		}
		else
		{
			VisitBlocks(*report.Heap, [&addFormattedLine](const Mem::MemBlockSnapshot& Block)
			{
				if (Block.IsFree)
				{
					return;
				}
				addFormattedLine(u"alloc %08llX: %08llX  %3u %3llu  (%08llX %08llX)",
					static_cast<unsigned long long>(Block.ContentAddress),
					static_cast<unsigned long long>(Block.RequestedSize),
					static_cast<unsigned int>(Block.GroupId),
					static_cast<unsigned long long>(Block.Alignment),
					static_cast<unsigned long long>(Block.PreviousAddress),
					static_cast<unsigned long long>(Block.NextAddress));
			});
		}
		addLine(u"(Free Blocks)");
		if (report.FreeBlockCount == 0)
		{
			addLine(u" NONE");
		}
		else
		{
			VisitBlocks(*report.Heap, [&addFormattedLine](const Mem::MemBlockSnapshot& Block)
			{
				if (!Block.IsFree)
				{
					return;
				}
				const std::size_t freeSize = Block.Span > sizeof(Mem::MemBlock)
					? Block.Span - sizeof(Mem::MemBlock)
					: 0;
				addFormattedLine(u" free  %08llX: %08llX  %3u %3u  (%08llX %08llX)",
					static_cast<unsigned long long>(Block.ContentAddress),
					static_cast<unsigned long long>(freeSize),
					static_cast<unsigned int>(Block.GroupId),
					0u,
					static_cast<unsigned long long>(Block.PreviousAddress),
					static_cast<unsigned long long>(Block.NextAddress));
			});
		}
		addFormattedLine(u"%llu / %llu bytes (%6.2f%%) used (U:%u F:%u)",
			static_cast<unsigned long long>(report.UsedBytes),
			static_cast<unsigned long long>(heapSize),
			static_cast<double>(usedPercent),
			static_cast<unsigned int>(report.AllocationCount),
			static_cast<unsigned int>(report.FreeBlockCount));
	}
	addFormattedLine(u"PgUp/PgDn scroll %llu/%llu | F4 close",
		static_cast<unsigned long long>(firstLine + 1),
		static_cast<unsigned long long>(totalLineCount));
	Console.Draw(CommandBuffer, DebugFont);
}
#endif
