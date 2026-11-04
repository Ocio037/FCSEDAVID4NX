module;
#include<cstdint>
#include<string>
#include<variant>
#include<vector>
export module ohana.bytecode;
export namespace ohana {
    enum class Op : uint8_t 
    {
        Nop = 0,
        PushInt,
        PushFloat,
        PushBool,
        PushString,
        Load,
        Store,
        Add,
        Sub,
        Mul,
        Div,
        Equal,
        NotEqual,
        Less,
        LessEqual,
        Greater,
        GreaterEqual,
        Jump,
        JumpIfFalse,
        Call,
        Return,
        Pop,
        Halt,
        Wait,
        ShowString,
        CallNative
    };
    using InstructionArg = std::variant<std::monostate, int64_t, double, bool, std::string, uint32_t>;
    struct Instruction 
    {
        Op op{};
        InstructionArg arg{};
    };
    struct FunctionInfo 
    {
        std::string name;
        uint32_t entry = 0;
        uint32_t parameter_count = 0;
        uint32_t local_count = 0;
    };
    struct Chunk 
    {
        std::vector<Instruction> code;
        std::vector<std::string> strings;
        std::vector<FunctionInfo> functions;
    };
    std::string opcode_name(Op op);
} // namespace ohana