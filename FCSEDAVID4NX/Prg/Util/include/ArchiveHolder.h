#pragma once
#include<Types.h>
#include<MemBase.h>
#include<ArcFile.h>
#include<DynamicString.h>
#include<DynamicUnorderedMap.h>

namespace GameUtil
{
	class ArchiveHolder final
	{
	public:
		void SetAllocationHeap(Mem::MemBase* Heap);
		const ArcFile* Add(const String& Name, ArcFile&& Archive);
		const ArcFile* CreateAndAdd(const String& Name, const void* Data, u64 Size, String* Error = nullptr);
		[[nodiscard]] const ArcFile* Get(const String& Name) const;
		[[nodiscard]] bool IsMounted(const String& Name) const;
		void Remove(const String& Name);
		void Clear();
	private:
		DynamicUnorderedMap<String, ArcFile> Archives;
	};
}
