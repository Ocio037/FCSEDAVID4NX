#pragma once
#ifndef GX_BUFFER_H_
#define GX_BUFFER_H_
#include<Macros.h>
#include<Types.h>
#include<MemTypes.h>
namespace gx {
	class Display;
	struct BufferData;
	class Buffer final : private NonCopy<>
	{
	public:
		Buffer();
		~Buffer();
		void Initialize(Display& display);
		void Finalize();
		bool CreateVertexBuffer(const void* data, u64 sizeInBytes);
		bool UpdateVertexBuffer(const void* data, u64 sizeInBytes);
		bool UpdateVertexBuffer(const void* data, u64 sizeInBytes, u64 offsetInBytes);
		bool CreateIndexBuffer(const u32* data, u32 indexCount);
		[[nodiscard]] u32 GetIndexCount() const;
		[[nodiscard]] bool IsValid() const;
	private:
		friend class CommandBuffer;
		Mem::UniquePtr<BufferData>Data;
	};
}
#endif // !GX_BUFFER_H_
