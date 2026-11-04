#pragma once
#include<Types.h>
#include<Macros.h>
#include<DynamicArray.h>
#include<DynamicString.h>
#include<string_view>
#include"Foca3DGfx/Detail.h"
namespace Foca3DGfx 
{
    class FocaArchive final : NonCopy<>
    {
    public:
        FocaArchive();
        ~FocaArchive();
        bool LoadFromMemory(const void* Data, u64 size);
        detail::FocaModel GetModel(std::string_view Name) const;
        detail::FocaTexture GetTexture(std::string_view Name) const;
        [[nodiscard]] u16 GetVersion() const;
        [[nodiscard]] const DynamicArray<detail::FocaModel>& GetModels() const;
        [[nodiscard]] const DynamicArray<detail::FocaTexture>& GetTextures() const;
    private:
        u16 Version;
        DynamicArray<detail::FocaModel> Models;
        DynamicArray<detail::FocaTexture> Textures;
    };
}
