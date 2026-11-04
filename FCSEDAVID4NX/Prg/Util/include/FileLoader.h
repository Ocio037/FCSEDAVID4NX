#pragma once
#include<Types.h>
#include<Macros.h>
#include<MemBase.h>
#include<ArchiveHolder.h>
#include<FileHolder.h>
#include<condition_variable>
#include<DynamicArray.h>
#include<DynamicDeque.h>
#include<DynamicString.h>
#include<DynamicUnorderedMap.h>
#include<DynamicUnorderedSet.h>
#include<mutex>
#include<thread>

namespace GameUtil
{
	inline constexpr char DataArchiveName[] = "data";
	class FileLoader final : NonCopy<>
	{
	public:
		FileLoader();
		~FileLoader();
		void SetAllocationHeap(Mem::MemBase* Heap);
		bool RequestLoadToMainRAM(const String& Name, const String& FilePath, String* Error = nullptr);
		bool RequestMountArchive(const String& ArchiveName, const String& FilePath, String* Error = nullptr);
		[[nodiscard]] bool IsNeedToLoad(const String& Name) const;
		[[nodiscard]] bool IsLoaded(const String& Name) const;
		[[nodiscard]] bool IsMountedArchive(const String& ArchiveName) const;
		[[nodiscard]] const DynamicArray<u8>* ReceiveFile(const String& Name, String* Error = nullptr);
		[[nodiscard]] const ArcFile* ReceiveArchive(const String& ArchiveName, String* Error = nullptr);
		void ReceiveAllRequestedFile();
		const ArcFile* CreateAndAddArchive(const String& ArchiveName, const void* Data, u64 Size, String* Error = nullptr);
		[[nodiscard]] const ArcEntry* FindResourceById(const String& ArchiveName, u32 Id, String* Error = nullptr);
		[[nodiscard]] const ArcEntry* FindResourceByName(const String& ArchiveName, const String& ResourceName, const String& Extension = {}, String* Error = nullptr);
		void RemoveFile(const String& Name);
		void RemoveArchive(const String& ArchiveName);
		void Clear();
	private:
		enum class RequestKind : u8
		{
			File,
			Archive
		};
		struct Request
		{
			RequestKind Kind = RequestKind::File;
			String Name;
			String FilePath;
		};
		void WorkerLoop();
		bool RequestFile(const String& Name, const String& FilePath, RequestKind Kind, String* Error);
		static String NormalizeName(const String& Name);
		static String NormalizeResourceName(String Name, const String& Extension);
		mutable std::mutex Mutex;
		std::condition_variable WorkAvailable;
		std::condition_variable RequestCompleted;
		DynamicDeque<Request> Requests;
		DynamicUnorderedSet<String> PendingFiles;
		DynamicUnorderedSet<String> PendingArchives;
		DynamicUnorderedMap<String, String> FileErrors;
		DynamicUnorderedMap<String, String> ArchiveErrors;
		FileHolder Files;
		ArchiveHolder Archives;
		bool StopRequested = false;
		Mem::MemBase* AllocationHeap = nullptr;
		std::thread LoaderThread;
	};
}
