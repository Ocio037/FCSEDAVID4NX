#include "MemBase.h"
Mem::MemBase::MemBase()
{

}
Mem::MemBase::~MemBase()
{

}
bool Mem::MemBase::Free(void* Memory) noexcept
{
	return Memory == nullptr;
}
std::size_t Mem::MemBase::GetSize(const void*) const noexcept
{
	return 0;
}
std::size_t Mem::MemBase::GetTotalFreeSize() const noexcept
{
	return GetFreeSize();
}
std::size_t Mem::MemBase::GetUsedSize() const noexcept
{
	const std::size_t heapSize = GetHeapSize();
	const std::size_t freeSize = GetTotalFreeSize();
	return heapSize >= freeSize ? heapSize - freeSize : 0;
}
void* Mem::MemBase::Alloc(std::size_t ByteCount, std::size_t Alignment)
{
	return Allocate(ByteCount, Alignment);
}
bool Mem::MemBase::Deallocate(void* Memory) noexcept
{
	return Free(Memory);
}