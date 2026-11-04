#pragma once
#include<Types.h>
#include<MemBase.h>
#include<DynamicArray.h>
#include<DynamicString.h>
#include<DynamicUnorderedMap.h>

namespace GameUtil
{
	class FileHolder final
	{
	public:
		void SetAllocationHeap(Mem::MemBase* Heap);
		const DynamicArray<u8>* Add(const String& Name, DynamicArray<u8>&& Data);
		[[nodiscard]] const DynamicArray<u8>* Get(const String& Name) const;
		[[nodiscard]] bool IsLoaded(const String& Name) const;
		void Remove(const String& Name);
		void Clear();
	private:
		DynamicUnorderedMap<String, DynamicArray<u8>> Files;
	};
}
