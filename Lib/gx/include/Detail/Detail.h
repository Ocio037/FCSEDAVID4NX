#pragma once
namespace gx {
	enum Format
	{
        Unknown = -1,
        None = 0,
        R8G8B8,
        R6G5B5,
        L8A8,
        L4A4,
        L8,
        A8,
        L4,
        A4,
        D16,
        D16F,
	};
	enum class TextureFormat
	{
		A4,
		A8,
		L4,
		L8,
		L4A4,
		L8A8,
		R5G6B5,
		R5G5B5A1,
		R4G4B4A4,
		R8G8B8A8,
		CTXE,
		CTXE_A,
		CTXE_A4,
		Rgba8Unorm = R8G8B8A8
	};
}