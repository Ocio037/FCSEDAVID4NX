#include"FileLoader.h"
#include"FileReader.h"
#include<MemGlobal.h>
#include<algorithm>
#include<cctype>
#include<exception>
#include<functional>
#include<utility>

namespace
{
	template<typename T>
	Mem::Allocator<T> MakeAllocator(Mem::MemBase* Heap)
	{
		return Heap ? Mem::Allocator<T>(*Heap) : Mem::Allocator<T>();
	}
	class FileLoaderHeapScope final
	{
	public:
		explicit FileLoaderHeapScope(Mem::MemBase* Heap) noexcept : Heap(Heap), PreviousHeap(Heap ? Mem::SetCurrentHeap(Heap) : nullptr)
		{
		}
		~FileLoaderHeapScope()
		{
			if (Heap)
			{
				(void)Mem::SetCurrentHeap(PreviousHeap);
			}
		}
	private:
		Mem::MemBase* Heap;
		Mem::MemBase* PreviousHeap;
	};
	void SetError(String* Error, const String& Message)
	{
		if (Error)
		{
			*Error = Message;
		}
	}
}
GameUtil::FileLoader::FileLoader() : LoaderThread([this]() { WorkerLoop(); })
{
}
GameUtil::FileLoader::~FileLoader()
{
	{
		std::lock_guard<std::mutex> lock(Mutex);
		StopRequested = true;
	}
	WorkAvailable.notify_one();
	if (LoaderThread.joinable())
	{
		LoaderThread.join();
	}
	Clear();
}
void GameUtil::FileLoader::SetAllocationHeap(Mem::MemBase* Heap)
{
	std::lock_guard<std::mutex> lock(Mutex);
	DynamicDeque<Request> requests(MakeAllocator<Request>(Heap));
	DynamicUnorderedSet<String> pendingFiles(0, std::hash<String>{}, std::equal_to<String>{}, MakeAllocator<String>(Heap));
	DynamicUnorderedSet<String> pendingArchives(0, std::hash<String>{}, std::equal_to<String>{}, MakeAllocator<String>(Heap));
	DynamicUnorderedMap<String, String> fileErrors(0, std::hash<String>{}, std::equal_to<String>{}, MakeAllocator<std::pair<const String, String>>(Heap));
	DynamicUnorderedMap<String, String> archiveErrors(0, std::hash<String>{}, std::equal_to<String>{}, MakeAllocator<std::pair<const String, String>>(Heap));
	Requests.swap(requests);
	PendingFiles.swap(pendingFiles);
	PendingArchives.swap(pendingArchives);
	FileErrors.swap(fileErrors);
	ArchiveErrors.swap(archiveErrors);
	Files.SetAllocationHeap(Heap);
	Archives.SetAllocationHeap(Heap);
	AllocationHeap = Heap;
}
String GameUtil::FileLoader::NormalizeName(const String& Name)
{
	String normalized = Name;
	for (char& character : normalized)
	{
		if (character == '\\')
		{
			character = '/';
		}
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}
	while (normalized.starts_with("./"))
	{
		normalized.erase(0, 2);
	}
	return normalized;
}
String GameUtil::FileLoader::NormalizeResourceName(String Name, const String& Extension)
{
	const std::size_t separator = Name.find_last_of("/\\");
	if (separator != String::npos)
	{
		Name.erase(0, separator + 1);
	}
	for (char& character : Name)
	{
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}
	String extension = Extension;
	for (char& character : extension)
	{
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}
	if (!extension.empty() && Name.size() >= extension.size() &&
		Name.compare(Name.size() - extension.size(), extension.size(), extension) == 0)
	{
		Name.resize(Name.size() - extension.size());
	}
	return Name;
}
bool GameUtil::FileLoader::RequestFile(const String& Name, const String& FilePath, RequestKind Kind, String* Error)
{
	FileLoaderHeapScope heapScope(AllocationHeap);
	if (Error)
	{
		Error->clear();
	}
	const String key = NormalizeName(Name);
	if (key.empty() || FilePath.empty())
	{
		SetError(Error, "El nombre del recurso o la ruta del archivo estan vacios.");
		return false;
	}
	{
		std::lock_guard<std::mutex> lock(Mutex);
		if (StopRequested)
		{
			SetError(Error, "El cargador de recursos ya se esta cerrando.");
			return false;
		}
		const bool isArchive = Kind == RequestKind::Archive;
		if (isArchive)
		{
			if (Archives.IsMounted(key) || PendingArchives.contains(key))
			{
				return true;
			}
			if (PendingFiles.contains(key))
			{
				SetError(Error, "El mismo recurso ya se esta leyendo como archivo; espera a que termine antes de montarlo: " + Name);
				return false;
			}
			if (const DynamicArray<u8>* file = Files.Get(key))
			{
				ArcFile archive;
				if (!ArcLoad(file->data(), file->size(), archive))
				{
					const String message = "El archivo de recursos no contiene un archivo NEON valido: " + Name;
					ArchiveErrors[key] = message;
					SetError(Error, message);
					return false;
				}
				Archives.Add(key, std::move(archive));
				ArchiveErrors.erase(key);
				return true;
			}
		}
		else if (Files.IsLoaded(key) || PendingFiles.contains(key))
		{
			return true;
		}
		FileErrors.erase(key);
		ArchiveErrors.erase(key);
		PendingFiles.insert(key);
		if (isArchive)
		{
			PendingArchives.insert(key);
		}
		Requests.push_back(Request{ Kind, key, FilePath });
	}
	WorkAvailable.notify_one();
	return true;
}
bool GameUtil::FileLoader::RequestLoadToMainRAM(const String& Name, const String& FilePath, String* Error)
{
	return RequestFile(Name, FilePath, RequestKind::File, Error);
}
bool GameUtil::FileLoader::RequestMountArchive(const String& ArchiveName, const String& FilePath, String* Error)
{
	return RequestFile(ArchiveName, FilePath, RequestKind::Archive, Error);
}
bool GameUtil::FileLoader::IsLoaded(const String& Name) const
{
	FileLoaderHeapScope heapScope(AllocationHeap);
	std::lock_guard<std::mutex> lock(Mutex);
	return Files.IsLoaded(NormalizeName(Name));
}
bool GameUtil::FileLoader::IsNeedToLoad(const String& Name) const
{
	FileLoaderHeapScope heapScope(AllocationHeap);
	const String key = NormalizeName(Name);
	std::lock_guard<std::mutex> lock(Mutex);
	return !Files.IsLoaded(key) && !Archives.IsMounted(key) &&
		!PendingFiles.contains(key) && !PendingArchives.contains(key);
}
bool GameUtil::FileLoader::IsMountedArchive(const String& ArchiveName) const
{
	FileLoaderHeapScope heapScope(AllocationHeap);
	std::lock_guard<std::mutex> lock(Mutex);
	return Archives.IsMounted(NormalizeName(ArchiveName));
}
const DynamicArray<u8>* GameUtil::FileLoader::ReceiveFile(const String& Name, String* Error)
{
	FileLoaderHeapScope heapScope(AllocationHeap);
	if (Error)
	{
		Error->clear();
	}
	const String key = NormalizeName(Name);
	std::unique_lock<std::mutex> lock(Mutex);
	RequestCompleted.wait(lock, [this, &key]()
	{
		return !PendingFiles.contains(key);
	});
	const auto fileError = FileErrors.find(key);
	if (fileError != FileErrors.end())
	{
		SetError(Error, fileError->second);
		return nullptr;
	}
	const DynamicArray<u8>* file = Files.Get(key);
	if (file == nullptr)
	{
		SetError(Error, "El archivo no se ha solicitado: " + Name);
	}
	return file;
}
const GameUtil::ArcFile* GameUtil::FileLoader::ReceiveArchive(const String& ArchiveName, String* Error)
{
	FileLoaderHeapScope heapScope(AllocationHeap);
	if (Error)
	{
		Error->clear();
	}
	const String key = NormalizeName(ArchiveName);
	std::unique_lock<std::mutex> lock(Mutex);
	RequestCompleted.wait(lock, [this, &key]()
	{
		return !PendingFiles.contains(key) && !PendingArchives.contains(key);
	});
	const auto archiveError = ArchiveErrors.find(key);
	if (archiveError != ArchiveErrors.end())
	{
		SetError(Error, archiveError->second);
		return nullptr;
	}
	const ArcFile* archive = Archives.Get(key);
	if (archive == nullptr)
	{
		const auto fileError = FileErrors.find(key);
		SetError(Error, fileError != FileErrors.end()
			? fileError->second
			: "El archivo no se ha montado: " + ArchiveName);
	}
	return archive;
}
void GameUtil::FileLoader::ReceiveAllRequestedFile()
{
	FileLoaderHeapScope heapScope(AllocationHeap);
	std::unique_lock<std::mutex> lock(Mutex);
	RequestCompleted.wait(lock, [this]()
	{
		return PendingFiles.empty() && Requests.empty();
	});
}
const GameUtil::ArcFile* GameUtil::FileLoader::CreateAndAddArchive(const String& ArchiveName,
	const void* Data, u64 Size, String* Error)
{
	FileLoaderHeapScope heapScope(AllocationHeap);
	if (Error)
	{
		Error->clear();
	}
	const String key = NormalizeName(ArchiveName);
	if (key.empty())
	{
		SetError(Error, "El nombre del archivo que se va a montar esta vacio.");
		return nullptr;
	}
	std::unique_lock<std::mutex> lock(Mutex);
	RequestCompleted.wait(lock, [this, &key]()
	{
		return !PendingArchives.contains(key);
	});
	if (const ArcFile* archive = Archives.Get(key))
	{
		return archive;
	}
	const ArcFile* archive = Archives.CreateAndAdd(key, Data, Size, Error);
	if (archive == nullptr && Error && !Error->empty())
	{
		ArchiveErrors[key] = *Error;
	}
	return archive;
}
const GameUtil::ArcEntry* GameUtil::FileLoader::FindResourceById(const String& ArchiveName, u32 Id, String* Error)
{
	FileLoaderHeapScope heapScope(AllocationHeap);
	const ArcFile* archive = ReceiveArchive(ArchiveName, Error);
	if (archive == nullptr)
	{
		return nullptr;
	}
	const auto entry = std::find_if(archive->Entries.begin(), archive->Entries.end(), [Id](const ArcEntry& candidate)
	{
		return candidate.Id == Id;
	});
	return entry == archive->Entries.end() ? nullptr : &*entry;
}
const GameUtil::ArcEntry* GameUtil::FileLoader::FindResourceByName(const String& ArchiveName,
	const String& ResourceName, const String& Extension, String* Error)
{
	FileLoaderHeapScope heapScope(AllocationHeap);
	const ArcFile* archive = ReceiveArchive(ArchiveName, Error);
	if (archive == nullptr)
	{
		return nullptr;
	}
	const String requestedName = NormalizeResourceName(ResourceName, Extension);
	const auto entry = std::find_if(archive->Entries.begin(), archive->Entries.end(), [&](const ArcEntry& candidate)
	{
		return NormalizeResourceName(candidate.FileName, Extension) == requestedName;
	});
	return entry == archive->Entries.end() ? nullptr : &*entry;
}
void GameUtil::FileLoader::RemoveFile(const String& Name)
{
	FileLoaderHeapScope heapScope(AllocationHeap);
	const String key = NormalizeName(Name);
	std::unique_lock<std::mutex> lock(Mutex);
	RequestCompleted.wait(lock, [this, &key]()
	{
		return !PendingFiles.contains(key);
	});
	Files.Remove(key);
	Archives.Remove(key);
	FileErrors.erase(key);
	ArchiveErrors.erase(key);
}
void GameUtil::FileLoader::RemoveArchive(const String& ArchiveName)
{
	FileLoaderHeapScope heapScope(AllocationHeap);
	const String key = NormalizeName(ArchiveName);
	std::unique_lock<std::mutex> lock(Mutex);
	RequestCompleted.wait(lock, [this, &key]()
	{
		return !PendingArchives.contains(key);
	});
	Archives.Remove(key);
	ArchiveErrors.erase(key);
}
void GameUtil::FileLoader::Clear()
{
	FileLoaderHeapScope heapScope(AllocationHeap);
	std::unique_lock<std::mutex> lock(Mutex);
	RequestCompleted.wait(lock, [this]()
	{
		return PendingFiles.empty() && Requests.empty();
	});
	Files.Clear();
	Archives.Clear();
	DynamicDeque<Request>().swap(Requests);
	DynamicUnorderedSet<String>().swap(PendingFiles);
	DynamicUnorderedSet<String>().swap(PendingArchives);
	DynamicUnorderedMap<String, String>().swap(FileErrors);
	DynamicUnorderedMap<String, String>().swap(ArchiveErrors);
}
void GameUtil::FileLoader::WorkerLoop()
{
	for (;;)
	{
		Request request;
		{
			std::unique_lock<std::mutex> lock(Mutex);
			WorkAvailable.wait(lock, [this]()
			{
				return StopRequested || !Requests.empty();
			});
			if (StopRequested && Requests.empty())
			{
				return;
			}
			request = std::move(Requests.front());
			Requests.pop_front();
		}
		FileLoaderHeapScope heapScope(AllocationHeap);
		DynamicArray<u8> data;
		String error;
		ArcFile archive;
		bool fileSuccess = false;
		bool archiveSuccess = request.Kind != RequestKind::Archive;
		try
		{
			fileSuccess = FileRead(request.FilePath, data);
			if (fileSuccess && request.Kind == RequestKind::Archive)
			{
				archiveSuccess = ArcLoad(data.data(), data.size(), archive);
				if (!archiveSuccess)
				{
					error = "El archivo de recursos no contiene un archivo NEON valido: " + request.FilePath;
				}
			}
		}
		catch (const std::exception& exception)
		{
			fileSuccess = false;
			archiveSuccess = false;
			error = exception.what();
		}
		if (!fileSuccess && error.empty())
		{
			error = "No se pudo leer el archivo de recursos: " + request.FilePath;
		}
		{
			std::lock_guard<std::mutex> lock(Mutex);
			if (fileSuccess)
			{
				Files.Add(request.Name, std::move(data));
				FileErrors.erase(request.Name);
			}
			else
			{
				FileErrors[request.Name] = error;
			}
			if (request.Kind == RequestKind::Archive)
			{
				if (archiveSuccess)
				{
					Archives.Add(request.Name, std::move(archive));
					ArchiveErrors.erase(request.Name);
				}
				else
				{
					ArchiveErrors[request.Name] = std::move(error);
				}
			}
			PendingFiles.erase(request.Name);
			if (request.Kind == RequestKind::Archive)
			{
				PendingArchives.erase(request.Name);
			}
		}
		RequestCompleted.notify_all();
	}
}
