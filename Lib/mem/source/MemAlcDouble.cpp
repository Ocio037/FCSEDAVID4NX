#include "MemAlcDouble.h"
Mem::MemAlcDouble::MemAlcDouble() : BeginAddress(0x0), EndAddress(0x0),
	LeftAddress(0x0),
	RightAddress(0x0),
	ParentHeap(nullptr),
	ParentAllocation(nullptr),
	Epoch(0)
{

}
Mem::MemAlcDouble::~MemAlcDouble()
{
	Finalize();
}
bool Mem::MemAlcDouble::Initialize(void* Memory, std::size_t ByteCount)
{
	Finalize();
	if (Memory == nullptr || ByteCount < sizeof(AllocationHeader) + BaseAlignment)
	{
		return false;
	}
	const uptr rawBegin = reinterpret_cast<uptr>(Memory);
	uptr rawEnd = 0;
	uptr begin = 0;
	if (!Detail::AddAddress(rawBegin, ByteCount, rawEnd) || !Detail::AlignAddressUp(rawBegin, BaseAlignment, begin) || begin >= rawEnd)
	{
		return false;
	}
	const uptr end = Detail::AlignAddressDown(rawEnd, BaseAlignment);
	if (end <= begin || end - begin < sizeof(AllocationHeader) + BaseAlignment)
	{
		return false;
	}
	std::lock_guard<std::mutex> lock(Mutex);
	BeginAddress = begin;
	EndAddress = end;
	LeftAddress = begin;
	RightAddress = end;
	Epoch = NextNonZero(Epoch);
	return true;
}
bool Mem::MemAlcDouble::Initialize(MemBase& Parent, std::size_t ByteCount)
{
	if (&Parent == static_cast<MemBase*>(this))
	{
		return false;
	}
	Finalize();
	void* memory = Parent.Allocate(ByteCount, BaseAlignment);
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
void Mem::MemAlcDouble::Finalize() noexcept
{
	MemBase* parent = nullptr;
	void* allocation = nullptr;
	{
		std::lock_guard<std::mutex> lock(Mutex);
		parent = ParentHeap;
		allocation = ParentAllocation;
		ParentHeap = nullptr;
		ParentAllocation = nullptr;
		BeginAddress = 0;
		EndAddress = 0;
		LeftAddress = 0;
		RightAddress = 0;
		Epoch = NextNonZero(Epoch);
	}
	if (parent != nullptr && allocation != nullptr)
	{
		parent->Free(allocation);
	}
}
void* Mem::MemAlcDouble::Allocate(std::size_t ByteCount, Side From, std::size_t Alignment)
{
	std::lock_guard<std::mutex> lock(Mutex);
	if (!IsInitialized() || ByteCount == 0 || !Detail::IsPowerOfTwo(Alignment) || (From != Side::Left && From != Side::Right))
	{
		return nullptr;
	}
	Alignment = (std::max)(Alignment, BaseAlignment);
	return From == Side::Left ? AllocateLeftUnsafe(ByteCount, Alignment) : AllocateRightUnsafe(ByteCount, Alignment);
}
void* Mem::MemAlcDouble::Allocate(std::size_t ByteCount, std::size_t Alignment)
{
	return Allocate(ByteCount, Side::Left, Alignment);
}
void* Mem::MemAlcDouble::AllocateFromLeft(std::size_t ByteCount, std::size_t Alignment)
{
	return Allocate(ByteCount, Side::Left, Alignment);
}
void* Mem::MemAlcDouble::AllocateFromRight(std::size_t ByteCount, std::size_t Alignment)
{
	return Allocate(ByteCount, Side::Right, Alignment);
}
void* Mem::MemAlcDouble::AllocateFromTail(std::size_t ByteCount, std::size_t Alignment)
{
	return AllocateFromRight(ByteCount, Alignment);
}
void* Mem::MemAlcDouble::AllocateFrom(std::size_t ByteCount, Side From, std::size_t Alignment)
{
	return Allocate(ByteCount, From, Alignment);
}
bool Mem::MemAlcDouble::Free(void* Memory) noexcept
{
	if (Memory == nullptr)
	{
		return true;
	}
	std::lock_guard<std::mutex> lock(Mutex);
	AllocationHeader* header = FindAllocationUnsafe(Memory);
	if (header == nullptr)
	{
		return false;
	}
	if (header->AllocationSide == Side::Left)
	{
		LeftAddress = header->PreviousCursor;
	}
	else
	{
		RightAddress = header->PreviousCursor;
	}
	return true;
}
bool Mem::MemAlcDouble::Deallocate(void* Memory) noexcept
{
	return Free(Memory);
}
void Mem::MemAlcDouble::FreeSide(Side Which) noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	if (!IsInitialized())
	{
		return;
	}
	if (Which == Side::Left)
	{
		LeftAddress = BeginAddress;
	}
	else if (Which == Side::Right)
	{
		RightAddress = EndAddress;
	}
}
void Mem::MemAlcDouble::FreeAll() noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	if (IsInitialized())
	{
		LeftAddress = BeginAddress;
		RightAddress = EndAddress;
		Epoch = NextNonZero(Epoch);
	}
}
Mem::State Mem::MemAlcDouble::SaveState() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	return State{ this, LeftAddress, RightAddress, Epoch };
}
bool Mem::MemAlcDouble::RestoreState(const State& SavedState) noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	if (!IsInitialized() || SavedState.Owner != this || SavedState.Epoch != Epoch || SavedState.Left < BeginAddress || SavedState.Left > LeftAddress || SavedState.Right < RightAddress || SavedState.Right > EndAddress || SavedState.Left > SavedState.Right)
	{
		return false;
	}
	LeftAddress = SavedState.Left;
	RightAddress = SavedState.Right;
	return true;
}
std::size_t Mem::MemAlcDouble::GetFreeSize() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	return IsInitialized() ? static_cast<std::size_t>(RightAddress - LeftAddress) : 0;
}
std::size_t Mem::MemAlcDouble::GetLeftUsedSize() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	return IsInitialized() ? static_cast<std::size_t>(LeftAddress - BeginAddress) : 0;
}
std::size_t Mem::MemAlcDouble::GetRightUsedSize() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	return IsInitialized() ? static_cast<std::size_t>(EndAddress - RightAddress) : 0;
}
std::size_t Mem::MemAlcDouble::GetMaxFreeBlock() const noexcept
{
	return GetFreeSize();
}
std::size_t Mem::MemAlcDouble::GetHeapSize() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	return IsInitialized() ? static_cast<std::size_t>(EndAddress - BeginAddress) : 0;
}
bool Mem::MemAlcDouble::IsInitialized() const noexcept
{
	return BeginAddress != 0 && EndAddress > BeginAddress;
}
void Mem::MemAlcDouble::FreeTail() noexcept
{
	FreeSide(Side::Right);
}
inline u64 Mem::MemAlcDouble::NextNonZero(u64 Value) noexcept
{
	++Value;
	return Value == 0 ? 1 : Value;
}
void* Mem::MemAlcDouble::AllocateLeftUnsafe(std::size_t ByteCount, std::size_t Alignment) noexcept
{
	uptr minimumUser = 0;
	uptr user = 0;
	uptr dataEnd = 0;
	uptr newHead = 0;
	if (!Detail::AddAddress(LeftAddress, sizeof(AllocationHeader), minimumUser) || !Detail::AlignAddressUp(minimumUser, Alignment, user) || !Detail::AddAddress(user, ByteCount, dataEnd) || !Detail::AlignAddressUp(dataEnd, BaseAlignment, newHead) || newHead > RightAddress)
	{
		return nullptr;
	}
	const uptr headerAddress = user - sizeof(AllocationHeader);
	AllocationHeader header{};
	header.PreviousCursor = LeftAddress;
	header.UserAddress = user;
	header.RequestedSize = ByteCount;
	header.AllocationSide = Side::Left;
	::new (reinterpret_cast<void*>(headerAddress)) AllocationHeader(header);
	LeftAddress = newHead;
	return reinterpret_cast<void*>(user);
}
void* Mem::MemAlcDouble::AllocateRightUnsafe(std::size_t ByteCount, std::size_t Alignment) noexcept
{
	if (ByteCount > RightAddress - BeginAddress || sizeof(AllocationHeader) > RightAddress - BeginAddress - ByteCount)
	{
		return nullptr;
	}
	const uptr user = Detail::AlignAddressDown(RightAddress - ByteCount, Alignment);
	if (user < sizeof(AllocationHeader))
	{
		return nullptr;
	}
	const uptr headerAddress = user - sizeof(AllocationHeader);
	if (headerAddress < LeftAddress || headerAddress < BeginAddress)
	{
		return nullptr;
	}
	AllocationHeader header{};
	header.PreviousCursor = RightAddress;
	header.UserAddress = user;
	header.RequestedSize = ByteCount;
	header.AllocationSide = Side::Right;
	::new (reinterpret_cast<void*>(headerAddress)) AllocationHeader(header);
	RightAddress = headerAddress;
	return reinterpret_cast<void*>(user);
}
Mem::AllocationHeader* Mem::MemAlcDouble::FindAllocationUnsafe(const void* Memory) const noexcept
{
	if (!IsInitialized())
	{
		return nullptr;
	}
	const uptr user = reinterpret_cast<uptr>(Memory);
	if (user < BeginAddress + sizeof(AllocationHeader) || user >= EndAddress)
	{
		return nullptr;
	}
	const uptr headerAddress = user - sizeof(AllocationHeader);
	if (headerAddress < BeginAddress || headerAddress > EndAddress - sizeof(AllocationHeader) || headerAddress % alignof(AllocationHeader) != 0)
	{
		return nullptr;
	}
	const AllocationHeader* header = reinterpret_cast<const AllocationHeader*>(headerAddress);
	if (header->UserAddress != user || header->RequestedSize == 0 || header->RequestedSize > EndAddress - user)
	{
		return nullptr;
	}
	if (header->AllocationSide == Side::Left)
	{
		if (header->PreviousCursor < BeginAddress || header->PreviousCursor > headerAddress || header->PreviousCursor >= LeftAddress || user + header->RequestedSize > LeftAddress)
		{
			return nullptr;
		}
	}
	else if (header->AllocationSide == Side::Right)
	{
		if (headerAddress < RightAddress || header->PreviousCursor > EndAddress || header->PreviousCursor <= headerAddress || user + header->RequestedSize > header->PreviousCursor)
		{
			return nullptr;
		}
	}
	else
	{
		return nullptr;
	}
	return const_cast<AllocationHeader*>(header);
}
