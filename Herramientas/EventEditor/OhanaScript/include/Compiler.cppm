module;
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>
export module ohana.compiler;
import ohana.bytecode;
import ohana.ast;
export namespace ohana {
    using CompileArg = InstructionArg;
    class Compiler {
    public:
        Chunk compile(const Program& program);
    private:
        struct Scope 
        {
            std::unordered_map<std::string, uint32_t> locals;
            std::vector<std::string> parameters;
        };
        Chunk chunk_;
        std::unordered_map<std::string, uint32_t> functions_;
        std::unordered_map<std::string, uint32_t> globals_;
        std::vector<Scope> scopes_;
        std::vector<std::vector<std::size_t>> loop_breaks_;
        uint32_t emit(Op op, CompileArg arg = std::monostate{});
        void patch(std::size_t instruction, uint32_t target);
        void collect_functions(const Program& program);
        void compile_top_level(const Program& program);
        void compile_function(const Function& function, uint32_t function_index);
        void compile_event(const Event& event, uint32_t function_index);
        void stmt(const Stmt& statement);
        void expr(const Expr& expression);
        uint32_t declare_local(const std::string& name);
        uint32_t lookup_variable(const std::string& name);
        uint32_t declare_global(const std::string& name);
        bool in_function() const;
        Scope& current_scope();
    };

} // namespace ohana