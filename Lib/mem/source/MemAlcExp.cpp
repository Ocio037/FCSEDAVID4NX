#include<MemAlcExp.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include<Windows.h>
bool Mem::MemBlock::IsFreeBlock() const noexcept
{
	return IsFree;
}
std::size_t Mem::MemBlock::GetSize() const noexcept
{
	return Span > sizeof(MemBlock) ? Span - sizeof(MemBlock) : 0;
}
std::size_t Mem::MemBlock::GetRequestedSize() const noexcept
{
	return RequestedSize;
}
u8 Mem::MemBlock::GetGroupId() const noexcept
{
	return GroupId;
}
Mem::MemBlock* Mem::MemBlock::GetPrevBlock() const noexcept
{
	return Previous;
}
Mem::MemBlock* Mem::MemBlock::GetNextBlock() const noexcept
{
	return Next;
}
void* Mem::MemBlock::GetContent() noexcept
{
	return IsFree ? reinterpret_cast<void*>(reinterpret_cast<uptr>(this) + sizeof(MemBlock)) : reinterpret_cast<void*>(UserAddress);
}
const void* Mem::MemBlock::GetContent() const noexcept
{
	return IsFree ? reinterpret_cast<const void*>(reinterpret_cast<uptr>(this) + sizeof(MemBlock)) : reinterpret_cast<const void*>(UserAddress);
}
Mem::MemAlcExp::MemAlcExp() : BaseAddress(0x0), EndAddress(0x0),
	TotalSize(0),
	FirstBlock(nullptr),
	ParentHeap(nullptr),
	ParentAllocation(nullptr),
	OwnedMemory(nullptr),
	CurrentGroupId(0xFF),
	CurrentMode(AllocationMode::BestFit),
	Epoch(0)
{

}
Mem::MemAlcExp::~MemAlcExp()
{
	Finalize();
}
bool Mem::MemAlcExp::Initialize(void* Memory, std::size_t ByteCount)
{
	Finalize();
	if (Memory == nullptr || ByteCount == 0)
	{
		return false;
	}
	const uptr rawBegin = reinterpret_cast<uptr>(Memory);
	uptr rawEnd = 0;
	uptr begin = 0;
	if (!Detail::AddAddress(rawBegin, ByteCount, rawEnd) || !Detail::AlignAddressUp(rawBegin, HeaderAlignment, begin) || begin >= rawEnd)
	{
		return false;
	}
	const uptr end = Detail::AlignAddressDown(rawEnd, HeaderAlignment);
	if (end <= begin || end - begin < MinimumBlockSpan)
	{
		return false;
	}
	std::lock_guard<std::mutex> lock(Mutex);
	BaseAddress = begin;
	EndAddress = end;
	TotalSize = static_cast<std::size_t>(end - begin);
	Epoch = NextEpoch(Epoch);
	FirstBlock = ::new (reinterpret_cast<void*>(begin)) MemBlock{};
	FirstBlock->Span = TotalSize;
	return true;
}
bool Mem::MemAlcExp::Initialize(MemBase& Parent, std::size_t ByteCount)
{
	if (&Parent == this)
	{
		return false;
	}
	Finalize();
	void* memory = Parent.Allocate(ByteCount, HeaderAlignment);
	if (memory == nullptr)
	{
		return false;
	}
	if (!Initialize(memory, ByteCount))
	{
		Parent.Free(memory);
		return false;
	}
	ParentHeap = &Parent;
	ParentAllocation = memory;
	return true;
}
bool Mem::MemAlcExp::InitializeOwned(std::size_t ByteCount)
{
	if (ByteCount == 0)
	{
		return false;
	}
	void* memory = ::HeapAlloc(::GetProcessHeap(), 0, ByteCount);
	if (memory == nullptr)
	{
		return false;
	}
	if (!Initialize(memory, ByteCount))
	{
		::HeapFree(::GetProcessHeap(), 0, memory);
		return false;
	}
	OwnedMemory = memory;
	return true;
}
void Mem::MemAlcExp::Finalize() noexcept
{
	MemBase* parent = nullptr;
	void* parentAllocation = nullptr;
	void* ownedMemory = nullptr;
	{
		std::lock_guard<std::mutex> lock(Mutex);
		parent = ParentHeap;
		parentAllocation = ParentAllocation;
		ParentHeap = nullptr;
		ParentAllocation = nullptr;
		ownedMemory = OwnedMemory;
		OwnedMemory = nullptr;
		BaseAddress = 0;
		EndAddress = 0;
		TotalSize = 0;
		FirstBlock = nullptr;
		CurrentGroupId = 0xFF;
		Epoch = NextEpoch(Epoch);
	}
	if (parent != nullptr && parentAllocation != nullptr)
	{
		parent->Free(parentAllocation);
	}
	if (ownedMemory != nullptr)
	{
		::HeapFree(::GetProcessHeap(), 0, ownedMemory);
	}
}
bool Mem::MemAlcExp::IsInitialized() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	return BaseAddress != 0 && EndAddress > BaseAddress;
}
void* Mem::MemAlcExp::Allocate(std::size_t ByteCount, std::size_t Alignment)
{
	std::lock_guard<std::mutex> lock(Mutex);
	return AllocateUnsafe(ByteCount, Alignment, false, CurrentGroupId);
}
void* Mem::MemAlcExp::AllocateFromTail(std::size_t ByteCount, std::size_t Alignment)
{
	std::lock_guard<std::mutex> lock(Mutex);
	return AllocateUnsafe(ByteCount, Alignment, true, CurrentGroupId);
}
bool Mem::MemAlcExp::Free(void* Memory) noexcept
{
	if (Memory == nullptr)
	{
		return true;
	}
	std::lock_guard<std::mutex> lock(Mutex);
	MemBlock* block = FindBlockUnsafe(Memory);
	if (block == nullptr)
	{
		return false;
	}
	FreeBlockUnsafe(block);
	return true;
}
void* Mem::MemAlcExp::Reallocate(void* Memory, std::size_t NewSize, std::size_t Alignment)
{
	std::lock_guard<std::mutex> lock(Mutex);
	if (Memory == nullptr)
	{
		return AllocateUnsafe(NewSize, Alignment, false, CurrentGroupId);
	}
	MemBlock* oldBlock = FindBlockUnsafe(Memory);
	if (oldBlock == nullptr)
	{
		return nullptr;
	}
	if (NewSize == 0)
	{
		FreeBlockUnsafe(oldBlock);
		return nullptr;
	}
	if (!Detail::IsPowerOfTwo(Alignment))
	{
		return nullptr;
	}
	const std::size_t oldSize = oldBlock->RequestedSize;
	if (NewSize <= oldSize && reinterpret_cast<uptr>(Memory) % Alignment == 0)
	{
		oldBlock->RequestedSize = NewSize;
		return Memory;
	}
	const u8 groupId = oldBlock->GroupId;
	void* replacement = AllocateUnsafe(NewSize, Alignment, false, groupId);
	if (replacement == nullptr)
	{
		return nullptr;
	}
	std::memcpy(replacement, Memory, (std::min)(oldSize, NewSize));
	FreeBlockUnsafe(oldBlock);
	return replacement;
}
void Mem::MemAlcExp::FreeAll() noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	if (BaseAddress == 0 || TotalSize < MinimumBlockSpan)
	{
		return;
	}
	Epoch = NextEpoch(Epoch);
	FirstBlock = ::new (reinterpret_cast<void*>(BaseAddress)) MemBlock{};
	FirstBlock->Span = TotalSize;
}
void Mem::MemAlcExp::FreeTail() noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	MemBlock* block = FirstBlock;
	while (block != nullptr)
	{
		if (!block->IsFree && block->FromTail)
		{
			FreeBlockUnsafe(block);
			block = FirstBlock;
		}
		else
		{
			block = block->Next;
		}
	}
}
std::size_t Mem::MemAlcExp::FreeGroup(u8 GroupId) noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	std::size_t freedBlocks = 0;
	MemBlock* block = FirstBlock;
	while (block != nullptr)
	{
		if (!block->IsFree && block->GroupId == GroupId)
		{
			FreeBlockUnsafe(block);
			++freedBlocks;
			block = FirstBlock;
		}
		else
		{
			block = block->Next;
		}
	}
	return freedBlocks;
}
u8 Mem::MemAlcExp::ChangeGroupId(u8 GroupId) noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	const u8 previous = CurrentGroupId;
	CurrentGroupId = GroupId;
	return previous;
}
void Mem::MemAlcExp::SetAllocationMode(AllocationMode Mode) noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	CurrentMode = Mode;
}
std::size_t Mem::MemAlcExp::GetSize(const void* Memory) const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	MemBlock* block = FindBlockUnsafe(Memory);
	return block != nullptr ? block->RequestedSize : 0;
}
std::size_t Mem::MemAlcExp::GetFreeSize() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	std::size_t freeSize = 0;
	for (MemBlock* block = FirstBlock; block != nullptr; block = block->Next)
	{
		if (block->IsFree && block->Span > sizeof(MemBlock))
		{
			freeSize += block->Span - sizeof(MemBlock);
		}
	}
	return freeSize;
}
std::size_t Mem::MemAlcExp::GetTotalFreeSize() const noexcept
{
	return GetFreeSize();
}
std::size_t Mem::MemAlcExp::GetMaxFreeBlock() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	std::size_t maxSize = 0;
	constexpr std::size_t allocationOverhead = sizeof(MemBlock) + sizeof(AllocationPrefix) + (HeaderAlignment - 1);
	for (MemBlock* block = FirstBlock; block != nullptr; block = block->Next)
	{
		if (!block->IsFree || block->Span <= allocationOverhead)
		{
			continue;
		}
		const std::size_t size = block->Span - allocationOverhead;
		maxSize = (std::max)(maxSize, size);
	}
	return maxSize;
}
std::size_t Mem::MemAlcExp::GetUsedSize() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	const std::size_t freeSize = GetFreeSizeUnsafe();
	return TotalSize >= freeSize ? TotalSize - freeSize : 0;
}
std::size_t Mem::MemAlcExp::GetHeapSize() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	return TotalSize;
}
std::size_t Mem::MemAlcExp::CopyBlockSnapshot(MemBlockSnapshot* Output, std::size_t Capacity, std::size_t FirstIndex) const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	std::size_t blockCount = 0;
	std::size_t copiedCount = 0;
	for (MemBlock* block = FirstBlock; block != nullptr; block = block->Next, ++blockCount)
	{
		if (Output == nullptr || blockCount < FirstIndex || copiedCount >= Capacity)
		{
			continue;
		}
		const uptr blockAddress = reinterpret_cast<uptr>(block);
		const std::size_t blockCapacity = block->Span > sizeof(MemBlock) ? block->Span - sizeof(MemBlock) : 0;
		const uptr minimumUserAddress = blockAddress + sizeof(MemBlock) + sizeof(AllocationPrefix);
		Output[copiedCount++] = MemBlockSnapshot{
			blockAddress,
			block->IsFree ? blockAddress + sizeof(MemBlock) : block->UserAddress,
			blockAddress + block->Span,
			reinterpret_cast<uptr>(block->Previous),
			reinterpret_cast<uptr>(block->Next),
			block->Span,
			blockCapacity,
			block->RequestedSize,
			block->IsFree || block->UserAddress < minimumUserAddress ? 0 : static_cast<std::size_t>(block->UserAddress - minimumUserAddress),
			block->GroupId,
			block->IsFree,
			block->FromTail
		};
	}
	return blockCount;
}
bool Mem::MemAlcExp::Check() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	if (BaseAddress == 0 || EndAddress <= BaseAddress || FirstBlock == nullptr)
	{
		return false;
	}
	uptr cursor = BaseAddress;
	MemBlock* previous = nullptr;
	for (MemBlock* block = FirstBlock; block != nullptr; block = block->Next)
	{
		const uptr address = reinterpret_cast<uptr>(block);
		if (address != cursor || address < BaseAddress || address >= EndAddress || address % HeaderAlignment != 0 || block->Previous != previous || block->Span < sizeof(MemBlock) || block->Span > EndAddress - address)
		{
			return false;
		}
		if (!block->IsFree)
		{
			const uptr blockEnd = address + block->Span;
			if (block->UserAddress < address + sizeof(MemBlock) + sizeof(AllocationPrefix) || block->UserAddress > blockEnd || block->RequestedSize > blockEnd - block->UserAddress)
			{
				return false;
			}
		}
		cursor += block->Span;
		previous = block;
	}
	return cursor == EndAddress;
}
inline u64 Mem::MemAlcExp::NextEpoch(u64 Value) noexcept
{
	++Value;
	return Value == 0 ? 1 : Value;
}
std::size_t Mem::MemAlcExp::GetFreeSizeUnsafe() const noexcept
{
	std::size_t freeSize = 0;
	for (MemBlock* block = FirstBlock; block != nullptr; block = block->Next)
	{
		if (block->IsFree && block->Span > sizeof(MemBlock))
		{
			freeSize += block->Span - sizeof(MemBlock);
		}
	}
	return freeSize;
}
bool Mem::MemAlcExp::BuildLayout(const MemBlock* Block, std::size_t ByteCount, std::size_t Alignment, bool FromTail, Layout& Result) const noexcept
{
	const uptr begin = reinterpret_cast<uptr>(Block);
	uptr end = 0;
	if (!Detail::AddAddress(begin, Block->Span, end))
	{
		return false;
	}
	if (!FromTail)
	{
		uptr minimumUser = 0;
		uptr user = 0;
		uptr dataEnd = 0;
		uptr usedEnd = 0;
		if (!Detail::AddAddress(begin, sizeof(MemBlock) + sizeof(AllocationPrefix), minimumUser) || !Detail::AlignAddressUp(minimumUser, Alignment, user) || !Detail::AddAddress(user, ByteCount, dataEnd) || !Detail::AlignAddressUp(dataEnd, HeaderAlignment, usedEnd) || usedEnd > end)
		{
			return false;
		}
		const std::size_t remainder = static_cast<std::size_t>(end - usedEnd);
		Result.User = user;
		Result.UsedStart = begin;
		Result.UsedEnd = remainder >= MinimumBlockSpan ? usedEnd : end;
		Result.SplitFreeBlock = remainder >= MinimumBlockSpan;
		return true;
	}
	if (ByteCount > Block->Span)
	{
		return false;
	}
	const uptr user = Detail::AlignAddressDown(end - ByteCount, Alignment);
	if (user < sizeof(AllocationPrefix))
	{
		return false;
	}
	const uptr prefixAddress = user - sizeof(AllocationPrefix);
	if (prefixAddress < sizeof(MemBlock))
	{
		return false;
	}
	uptr usedStart = Detail::AlignAddressDown(prefixAddress - sizeof(MemBlock), HeaderAlignment);
	if (usedStart < begin || prefixAddress < usedStart + sizeof(MemBlock))
	{
		return false;
	}
	if (usedStart - begin < MinimumBlockSpan)
	{
		usedStart = begin;
	}
	Result.User = user;
	Result.UsedStart = usedStart;
	Result.UsedEnd = end;
	Result.SplitFreeBlock = usedStart > begin;
	return true;
}
void* Mem::MemAlcExp::AllocateUnsafe(std::size_t ByteCount, std::size_t Alignment, bool FromTail, u8 GroupId)
{
	if (BaseAddress == 0 || FirstBlock == nullptr || !Detail::IsPowerOfTwo(Alignment))
	{
		return nullptr;
	}
	ByteCount = (std::max)(ByteCount, static_cast<std::size_t>(1));
	Alignment = (std::max)(Alignment, alignof(AllocationPrefix));
	MemBlock* selected = nullptr;
	Layout selectedLayout{};
	for (MemBlock* block = FirstBlock; block != nullptr; block = block->Next)
	{
		if (!block->IsFree)
		{
			continue;
		}
		Layout layout{};
		if (!BuildLayout(block, ByteCount, Alignment, FromTail, layout))
		{
			continue;
		}
		if (selected == nullptr || block->Span < selected->Span)
		{
			selected = block;
			selectedLayout = layout;
			if (CurrentMode == AllocationMode::FirstFit)
			{
				break;
			}
		}
	}
	if (selected == nullptr)
	{
		return nullptr;
	}
	MemBlock* usedBlock = selected;
	const uptr oldBegin = reinterpret_cast<uptr>(selected);
	const uptr oldEnd = oldBegin + selected->Span;
	if (!FromTail)
	{
		if (selectedLayout.SplitFreeBlock)
		{
			const std::size_t usedSpan = static_cast<std::size_t>(selectedLayout.UsedEnd - oldBegin);
			MemBlock* remainder = ::new (reinterpret_cast<void*>(selectedLayout.UsedEnd)) MemBlock{};
			remainder->Span = static_cast<std::size_t>(oldEnd - selectedLayout.UsedEnd);
			remainder->Previous = selected;
			remainder->Next = selected->Next;
			if (remainder->Next != nullptr)
			{
				remainder->Next->Previous = remainder;
			}
			selected->Next = remainder;
			selected->Span = usedSpan;
		}
	}
	else if (selectedLayout.SplitFreeBlock)
	{
		const std::size_t freeSpan = static_cast<std::size_t>(selectedLayout.UsedStart - oldBegin);
		MemBlock* used = ::new (reinterpret_cast<void*>(selectedLayout.UsedStart)) MemBlock{};
		used->Span = static_cast<std::size_t>(oldEnd - selectedLayout.UsedStart);
		used->Previous = selected;
		used->Next = selected->Next;
		if (used->Next != nullptr)
		{
			used->Next->Previous = used;
		}
		selected->Next = used;
		selected->Span = freeSpan;
		usedBlock = used;
	}
	usedBlock->IsFree = false;
	usedBlock->RequestedSize = ByteCount;
	usedBlock->UserAddress = selectedLayout.User;
	usedBlock->GroupId = GroupId;
	usedBlock->FromTail = FromTail;
	AllocationPrefix prefix{};
	prefix.Block = usedBlock;
	void* user = reinterpret_cast<void*>(selectedLayout.User);
	std::memcpy(reinterpret_cast<void*>(selectedLayout.User - sizeof(AllocationPrefix)), &prefix, sizeof(prefix));
	return user;
}
Mem::MemBlock* Mem::MemAlcExp::FindBlockUnsafe(const void* Memory) const noexcept
{
	if (Memory == nullptr || BaseAddress == 0)
	{
		return nullptr;
	}
	const uptr user = reinterpret_cast<uptr>(Memory);
	if (user < BaseAddress || user - BaseAddress < sizeof(MemBlock) + sizeof(AllocationPrefix) || user >= EndAddress)
	{
		return nullptr;
	}
	AllocationPrefix prefix{};
	std::memcpy(&prefix, reinterpret_cast<const void*>(user - sizeof(AllocationPrefix)), sizeof(prefix));
	const uptr blockAddress = reinterpret_cast<uptr>(prefix.Block);
	if (blockAddress < BaseAddress || blockAddress >= EndAddress || EndAddress - blockAddress < sizeof(MemBlock) || blockAddress % HeaderAlignment != 0)
	{
		return nullptr;
	}
	MemBlock* block = nullptr;
	for (MemBlock* candidate = FirstBlock; candidate != nullptr; candidate = candidate->Next)
	{
		if (candidate == prefix.Block)
		{
			block = candidate;
			break;
		}
	}
	if (block == nullptr || block->IsFree || block->UserAddress != user)
	{
		return nullptr;
	}
	if (block->Span > EndAddress - blockAddress)
	{
		return nullptr;
	}
	const uptr blockEnd = blockAddress + block->Span;
	if (user > blockEnd || block->RequestedSize > blockEnd - user)
	{
		return nullptr;
	}
	return block;
}
void Mem::MemAlcExp::FreeBlockUnsafe(MemBlock* Block) noexcept
{
	Block->IsFree = true;
	Block->RequestedSize = 0;
	Block->UserAddress = 0;
	Block->GroupId = 0;
	Block->FromTail = false;
	if (Block->Previous != nullptr && Block->Previous->IsFree)
	{
		MemBlock* previous = Block->Previous;
		previous->Span += Block->Span;
		previous->Next = Block->Next;
		if (Block->Next != nullptr)
		{
			Block->Next->Previous = previous;
		}
		Block = previous;
	}
	if (Block->Next != nullptr && Block->Next->IsFree)
	{
		MemBlock* next = Block->Next;
		Block->Span += next->Span;
		Block->Next = next->Next;
		if (next->Next != nullptr)
		{
			next->Next->Previous = Block;
		}
	}
}
