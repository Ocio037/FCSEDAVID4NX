#include"FileHolder.h"
#include<functional>
#include<utility>
namespace
{
	template<typename T>
	Mem::Allocator<T> MakeAllocator(Mem::MemBase* Heap)
	{
		return Heap ? Mem::Allocator<T>(*Heap) : Mem::Allocator<T>();
	}
}
void GameUtil::FileHolder::SetAllocationHeap(Mem::MemBase* Heap)
{
	using FileMap = DynamicUnorderedMap<String, DynamicArray<u8>>;
	FileMap replacement(0, std::hash<String>{}, std::equal_to<String>{},
		MakeAllocator<std::pair<const String, DynamicArray<u8>>>(Heap));
	Files.swap(replacement);
}
const DynamicArray<u8>* GameUtil::FileHolder::Add(const String& Name, DynamicArray<u8>&& Data)
{
	return &Files.insert_or_assign(Name, std::move(Data)).first->second;
}
const DynamicArray<u8>* GameUtil::FileHolder::Get(const String& Name) const
{
	const auto file = Files.find(Name);
	return file == Files.end() ? nullptr : &file->second;
}
bool GameUtil::FileHolder::IsLoaded(const String& Name) const
{
	return Files.contains(Name);
}
void GameUtil::FileHolder::Remove(const String& Name)
{
	Files.erase(Name);
}
void GameUtil::FileHolder::Clear()
{
	DynamicUnorderedMap<String, DynamicArray<u8>>().swap(Files);
}
