#pragma once
#ifndef GX_SHADER_H_
#define GX_SHADER_H_
#include<Macros.h>
#include<Types.h>
#include<MemTypes.h>
#include<glm/mat4x4.hpp>
namespace gx {
	enum class ShaderVertexLayout
	{
		Standard,
		PositionUvLayer
	};
	class CommandBuffer;
	class Display;
	class Texture;
	struct ShaderData;
	class Shader : private NonCopy<>
	{
		friend class CommandBuffer;
		friend class Texture;
	public:
		Shader();
		~Shader();
		void Initialize(Display* display);
		void Finalize();
		bool Load(void* VertexData, u32 VertexSize, void* FragmentData, u32 FragmentSize);
		bool Load(const void* VertexData, u32 VertexSize, const void* FragmentData, u32 FragmentSize,
			ShaderVertexLayout VertexLayout);
		void SetMatrices(const glm::mat4& Mtx, const glm::mat4& Projection, const glm::mat4& World);
	[[nodiscard]] const glm::mat4& GetMtx() const;
	private:
		Mem::UniquePtr<ShaderData>Data;
	};
}
#endif // !GX_SHADER_H_
