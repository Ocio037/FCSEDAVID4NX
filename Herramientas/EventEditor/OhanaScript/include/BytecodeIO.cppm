module;
#include<filesystem>
#include<ostream>
#include<cstdint>
#include<string>
#include<unordered_map>
export module ohana.bytecode_io;
import ohana.bytecode;
export namespace ohana {
    void escribir_bytecode(const Chunk& chunk, const std::filesystem::path& path);
    void escribir_bytecode(const Chunk& chunk, std::ostream& output);
    void escribir_bytecode(const Chunk& chunk, std::ostream& output, std::unordered_map<std::string, std::uint32_t>& string_indices);
    void desensamblar(const Chunk& chunk, std::ostream& output);
} // namespace ohana
