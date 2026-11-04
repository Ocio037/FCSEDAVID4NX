#include "MemAlcLinear.h"
Mem::MemAlcLinear::MemAlcLinear() :  BeginAddress(0x0), EndAddress(0x0),
	HeadAddress(0x0),
	TailAddress(0x0),
	ParentHeap(nullptr),
	ParentAllocation(nullptr),
	Epoch(0)
{

}
Mem::MemAlcLinear::~MemAlcLinear()
{
	Finalize();
}
bool Mem::MemAlcLinear::Initialize(void* Memory, std::size_t ByteCount)
{
	Finalize();
	if (Memory == nullptr || ByteCount == 0)
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
	if (end <= begin)
	{
		return false;
	}
	std::lock_guard<std::mutex> lock(Mutex);
	BeginAddress = begin;
	EndAddress = end;
	HeadAddress = begin;
	TailAddress = end;
	Epoch = NextEpoch(Epoch);
	return true;
}
bool Mem::MemAlcLinear::Initialize(MemBase& Parent, std::size_t ByteCount)
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
void Mem::MemAlcLinear::Finalize() noexcept
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
		HeadAddress = 0;
		TailAddress = 0;
		Epoch = NextEpoch(Epoch);
	}
	if (parent != nullptr && allocation != nullptr)
	{
		parent->Free(allocation);
	}
}
void* Mem::MemAlcLinear::Allocate(std::size_t ByteCount, std::size_t Alignment)
{
	std::lock_guard<std::mutex> lock(Mutex);
	if (!IsInitialized() || ByteCount == 0 || !Detail::IsPowerOfTwo(Alignment))
	{
		return nullptr;
	}
	Alignment = (std::max)(Alignment, BaseAlignment);
	uptr user = 0;
	uptr dataEnd = 0;
	uptr newHead = 0;
	if (!Detail::AlignAddressUp(HeadAddress, Alignment, user) || !Detail::AddAddress(user, ByteCount, dataEnd) || !Detail::AlignAddressUp(dataEnd, BaseAlignment, newHead) || newHead > TailAddress)
	{
		return nullptr;
	}
	HeadAddress = newHead;
	return reinterpret_cast<void*>(user);
}
void* Mem::MemAlcLinear::Alloc(std::size_t ByteCount, std::size_t Alignment)
{
	return Allocate(ByteCount, Alignment);
}
void* Mem::MemAlcLinear::AllocateFromTail(std::size_t ByteCount, std::size_t Alignment)
{
	std::lock_guard<std::mutex> lock(Mutex);
	if (!IsInitialized() || ByteCount == 0 || !Detail::IsPowerOfTwo(Alignment))
	{
		return nullptr;
	}
	Alignment = (std::max)(Alignment, BaseAlignment);
	if (ByteCount > TailAddress - BeginAddress)
	{
		return nullptr;
	}
	const uptr user = Detail::AlignAddressDown(TailAddress - ByteCount, Alignment);
	if (user < HeadAddress)
	{
		return nullptr;
	}
	TailAddress = user;
	return reinterpret_cast<void*>(user);
}
void* Mem::MemAlcLinear::AllocFromTail(std::size_t ByteCount, std::size_t Alignment)
{
	return AllocateFromTail(ByteCount, Alignment);
}
void Mem::MemAlcLinear::FreeTail() noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	if (IsInitialized())
	{
		TailAddress = EndAddress;
		Epoch = NextEpoch(Epoch);
	}
}
void Mem::MemAlcLinear::FreeAll() noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	if (IsInitialized())
	{
		HeadAddress = BeginAddress;
		TailAddress = EndAddress;
		Epoch = NextEpoch(Epoch);
	}
}
Mem::State Mem::MemAlcLinear::SaveState() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	return State{ this, HeadAddress, TailAddress, Epoch };
}
bool Mem::MemAlcLinear::RestoreState(const State& SavedState) noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	if (SavedState.Owner != this || SavedState.Epoch != Epoch || !IsInitialized() || SavedState.Head < BeginAddress || SavedState.Head > HeadAddress || SavedState.Tail < TailAddress || SavedState.Tail > EndAddress || SavedState.Head > SavedState.Tail)
	{
		return false;
	}
	HeadAddress = SavedState.Head;
	TailAddress = SavedState.Tail;
	return true;
}
std::size_t Mem::MemAlcLinear::GetFreeSize() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	return IsInitialized() ? static_cast<std::size_t>(TailAddress - HeadAddress) : 0;
}
std::size_t Mem::MemAlcLinear::GetHeadUsedSize() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	return IsInitialized() ? static_cast<std::size_t>(HeadAddress - BeginAddress) : 0;
}
std::size_t Mem::MemAlcLinear::GetTailUsedSize() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	return IsInitialized() ? static_cast<std::size_t>(EndAddress - TailAddress) : 0;
}
std::size_t Mem::MemAlcLinear::GetMaxFreeBlock() const noexcept
{
	return GetFreeSize();
}
std::size_t Mem::MemAlcLinear::GetHeapSize() const noexcept
{
	std::lock_guard<std::mutex> lock(Mutex);
	return IsInitialized() ? static_cast<std::size_t>(EndAddress - BeginAddress) : 0;
}
bool Mem::MemAlcLinear::IsInitialized() const noexcept
{
	return BeginAddress != 0 && EndAddress > BeginAddress;
}
u64 Mem::MemAlcLinear::NextEpoch(u64 Value) noexcept
{
	++Value;
	return Value == 0 ? 1 : Value;
}
