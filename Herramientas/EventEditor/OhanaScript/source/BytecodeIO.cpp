module;
#include<filesystem>
#include<ostream>
#include<cstdint>
#include<cstring>
#include<fstream>
#include<iterator>
#include<vector>
#include<unordered_map>
#include<stdexcept>
#include<type_traits>
#include<variant>
#include<string>
module ohana.bytecode_io;
import ohana.bytecode;
namespace {
    constexpr std::uint8_t OHANA_BYTECODE_VERSION = 2;
}
namespace ohana {
    namespace details {
        void escribir_u8(std::ostream& file, uint8_t value)
        {
            file.put(static_cast<char>(value));
        }
        void escribir_u32(std::ostream& file, uint32_t value)
        {
            for (int i = 0; i < 4; ++i)
            {
                escribir_u8(file, static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
            }
        }
        void escribir_u64(std::ostream& file, uint64_t value)
        {
            for (int i = 0; i < 8; ++i)
            {
                escribir_u8(file, static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
            }
        }
    }
} // namespace
void ohana::escribir_bytecode(const Chunk& chunk, const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::binary);
    if (!file) 
    {
        throw std::runtime_error("no se pudo crear " + path.string());
    }
    escribir_bytecode(chunk, file);
    if (!file)
    {
        throw std::runtime_error("no se pudo escribir " + path.string());
    }
}
void ohana::escribir_bytecode(const Chunk& chunk, std::ostream& file)
{
    std::unordered_map<std::string, uint32_t> unused_string_indices;
    escribir_bytecode(chunk, file, unused_string_indices);
}
void ohana::escribir_bytecode(const Chunk& chunk, std::ostream& file, std::unordered_map<std::string, uint32_t>& string_indices)
{
    std::vector<std::string> text_pool;
    std::unordered_map<std::string, uint32_t> text_indices;
    auto intern_text = [&](const std::string& value) -> uint32_t {
        const auto it = text_indices.find(value);
        if (it != text_indices.end()) 
        {
            return it->second;
        }
        const auto index = static_cast<uint32_t>(text_pool.size());
        text_pool.push_back(value);
        text_indices.emplace(value, index);
        return index;
    };
    std::vector<uint32_t> function_name_indices;
    function_name_indices.reserve(chunk.functions.size());
    for (const auto& function : chunk.functions) 
    {
        function_name_indices.push_back(intern_text(function.name));
    }
    for (const auto& instruction : chunk.code) 
    {
        if (instruction.arg.index() == 4) 
        {
            (void)intern_text(std::get<std::string>(instruction.arg));
        }
    }
    for (const auto& value : chunk.strings)
    {
        (void)intern_text(value);
    }

    uint64_t text_bytes = 0;
    for (const auto& value : text_pool) 
    {
        text_bytes += sizeof(uint32_t) + value.size();
    }
    if (text_bytes > UINT32_MAX) 
    {
        throw std::runtime_error("el pool de texto es demasiado grande");
    }

    details::escribir_u8(file, OHANA_BYTECODE_VERSION);
    details::escribir_u32(file, static_cast<uint32_t>(chunk.functions.size()));
    details::escribir_u32(file, static_cast<uint32_t>(chunk.code.size()));
    details::escribir_u32(file, static_cast<uint32_t>(text_pool.size()));
    details::escribir_u32(file, static_cast<uint32_t>(text_bytes));

    for (std::size_t index = 0; index < chunk.functions.size(); ++index) 
    {
        const auto& function = chunk.functions[index];
        details::escribir_u32(file, function_name_indices[index]);
        details::escribir_u32(file, function.entry);
        details::escribir_u32(file, function.parameter_count);
        details::escribir_u32(file, function.local_count);
    }

    for (const auto& instruction : chunk.code) 
    {
        details::escribir_u8(file, static_cast<uint8_t>(instruction.op));
        details::escribir_u8(file, static_cast<uint8_t>(instruction.arg.index()));

        switch (instruction.arg.index()) 
        {
        case 0:
            break;
        case 1: 
        {
            const auto value = std::get<int64_t>(instruction.arg);
            details::escribir_u64(file, static_cast<uint64_t>(value));
            break;
        }
        case 2: 
        {
            const double value = std::get<double>(instruction.arg);
            uint64_t bits = 0;
            std::memcpy(&bits, &value, sizeof(value));
            details::escribir_u64(file, bits);
            break;
        }
        case 3:
            details::escribir_u8(file, std::get<bool>(instruction.arg) ? 1 : 0);
            break;
        case 4: {
            const auto& value = std::get<std::string>(instruction.arg);
            details::escribir_u32(file, text_indices.at(value));
            break;
        }
        case 5:
            details::escribir_u32(file, std::get<uint32_t>(instruction.arg));
            break;
        default:
            throw std::runtime_error("tipo de operando de bytecode desconocido");
        }
    }
    for (const auto& value : text_pool) 
    {
        details::escribir_u32(file, static_cast<uint32_t>(value.size()));
        if (!value.empty()) 
        {
            file.write(value.data(), static_cast<std::streamsize>(value.size()));
        }
    }
    string_indices = text_indices;
}
void ohana::desensamblar(const Chunk& chunk, std::ostream& output)
{
    for (std::size_t index = 0; index < chunk.functions.size(); ++index) 
    {
        const auto& function = chunk.functions[index];
        output << ".funcion " << function.name << "(" << function.parameter_count << ") entrada=" << function.entry << " locales=" << function.local_count << '\n';
    }
    for (std::size_t index = 0; index < chunk.code.size(); ++index) 
    {
        output << index << "  " << opcode_name(chunk.code[index].op);
        const auto& argument = chunk.code[index].arg;
        std::visit([&](auto&& value) { 
            using T = std::decay_t<decltype(value)>;
                if constexpr (!std::is_same_v<T, std::monostate>) {
                    output << ' ' << value;
                }
            },
        argument);
        output << '\n';
    }
}
