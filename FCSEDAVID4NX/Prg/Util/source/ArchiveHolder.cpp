#include"ArchiveHolder.h"
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
void GameUtil::ArchiveHolder::SetAllocationHeap(Mem::MemBase* Heap)
{
	using ArchiveMap = DynamicUnorderedMap<String, ArcFile>;
	ArchiveMap replacement(0, std::hash<String>{}, std::equal_to<String>{},
		MakeAllocator<std::pair<const String, ArcFile>>(Heap));
	Archives.swap(replacement);
}
const GameUtil::ArcFile* GameUtil::ArchiveHolder::Add(const String& Name, ArcFile&& Archive)
{
	return &Archives.insert_or_assign(Name, std::move(Archive)).first->second;
}
const GameUtil::ArcFile* GameUtil::ArchiveHolder::CreateAndAdd(const String& Name, const void* Data, u64 Size, String* Error)
{
	if (Error)
	{
		Error->clear();
	}
	const auto existing = Archives.find(Name);
	if (existing != Archives.end())
	{
		return &existing->second;
	}
	ArcFile archive;
	if (!ArcLoad(Data, Size, archive))
	{
		if (Error)
		{
			*Error = "El recurso en memoria no contiene un archivo NEON valido.";
		}
		return nullptr;
	}
	return Add(Name, std::move(archive));
}
const GameUtil::ArcFile* GameUtil::ArchiveHolder::Get(const String& Name) const
{
	const auto archive = Archives.find(Name);
	return archive == Archives.end() ? nullptr : &archive->second;
}
bool GameUtil::ArchiveHolder::IsMounted(const String& Name) const
{
	return Archives.contains(Name);
}
void GameUtil::ArchiveHolder::Remove(const String& Name)
{
	Archives.erase(Name);
}
void GameUtil::ArchiveHolder::Clear()
{
	DynamicUnorderedMap<String, ArcFile>().swap(Archives);
}
