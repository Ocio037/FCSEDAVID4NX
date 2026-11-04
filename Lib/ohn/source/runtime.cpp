#include <runtime.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
enum OpCode: u8
{
    Nop = 0,
    PushInt = 1,
    PushFloat = 2,
    PushBool = 3,
    PushString = 4,
    Load = 5,
    Store = 6,
    Add = 7,
    Sub = 8,
    Mul = 9,
    Div = 10,
    Equal = 11,
    NotEqual = 12,
    Less = 13,
    LessEqual = 14,
    Greater = 15,
    GreaterEqual = 16,
    Jump = 17,
    JumpIfFalse = 18,
    Call = 19,
    Return = 20,
    Pop = 21,
    Halt = 22,
    Wait = 23,
    ShowString = 24,
    CallNative = 25
};
constexpr u8 kVersion = 2;
ohana::runtime_error::runtime_error(const std::string& message) : std::runtime_error(message)
{

}
ohana::runtime::runtime(void* Data, uptr Size) : runtime(static_cast<const void*>(Data), Size)
{

}
ohana::runtime::runtime(const void* Data, uptr Size) : 
    data_(static_cast<const u8*>(Data)), 
    size_(Size) 
{
    if (data_ == nullptr && size_ != 0) 
    {
        throw ohana::runtime_error("Data no puede ser null cuando Size es distinto de cero");
    }
    load();
    reset();
}
void ohana::runtime::reset()
{
    stack_.clear();
    globals_.clear();
    frames_.clear();
    halted_ = false;
    suspended_ = false;
    last_result_ = std::monostate{};
}
ohana::runtime::Value ohana::runtime::execute()
{
    reset();
    if (code_count_ == 0) 
    {
        return std::monostate{};
    }
    constexpr u32 top_level = std::numeric_limits<u32>::max();
    Frame frame;
    frame.function_index = top_level;
    frame.ip = 0;
    frame.stack_base = 0;
    frames_.push_back(std::move(frame));
    while (!frames_.empty() && !halted_ && !suspended_)
    {
        const uptr frame_index = frames_.size() - 1;
        const u32 ip = frames_.back().ip;
        const auto offset = instruction_offset(ip);
        u32 next_ip = ip;
        execute_instruction(next_ip, offset);
        if (!halted_ && frames_.size() == frame_index + 1) 
        {
            frames_.back().ip = next_ip;
        }
    }
    return last_result_;
}
ohana::runtime::Value ohana::runtime::resume()
{
    if (!suspended_)
    {
        return last_result_;
    }
    suspended_ = false;
    while (!frames_.empty() && !halted_ && !suspended_)
    {
        const uptr frame_index = frames_.size() - 1;
        const u32 ip = frames_.back().ip;
        const auto offset = instruction_offset(ip);
        u32 next_ip = ip;
        execute_instruction(next_ip, offset);
        if (!halted_ && frames_.size() == frame_index + 1)
        {
            frames_.back().ip = next_ip;
        }
    }
    return last_result_;
}
bool ohana::runtime::is_suspended() const noexcept
{
    return suspended_;
}
ohana::runtime::Value ohana::runtime::call(std::string_view name, const std::vector<Value>& args) 
{
    for (u32 i = 0; i < functions_.size(); ++i) 
    {
        if (function_name(i) == name) 
        {
            return call(i, args);
        }
    }
    if (auto* native = find_native(name)) 
    {
        return (*native)(args);
    }
    throw ohana::runtime_error("función no definida: " + std::string(name));
}
ohana::runtime::Value ohana::runtime::call(u32 function_index, const std::vector<Value>& args) 
{
    if (function_index >= functions_.size()) 
    {
        throw ohana::runtime_error("índice de función fuera de rango");
    }
    reset();
    return run(function_index, &args);
}
ohana::runtime::Value ohana::runtime::call_preserving_state(std::string_view name, const std::vector<Value>& args)
{
    for (u32 i = 0; i < functions_.size(); ++i)
    {
        if (function_name(i) == name)
        {
            return call_preserving_state(i, args);
        }
    }
    if (auto* native = find_native(name))
    {
        return (*native)(args);
    }
    throw ohana::runtime_error("función no definida: " + std::string(name));
}
ohana::runtime::Value ohana::runtime::call_preserving_state(u32 function_index, const std::vector<Value>& args)
{
    if (function_index >= functions_.size())
    {
        throw ohana::runtime_error("índice de función fuera de rango");
    }
    stack_.clear();
    frames_.clear();
    halted_ = false;
    suspended_ = false;
    last_result_ = std::monostate{};
    return run(function_index, &args);
}
uptr ohana::runtime::function_count() const noexcept 
{
    return functions_.size();
}
uptr ohana::runtime::code_count() const noexcept 
{
    return code_count_;
}
uptr ohana::runtime::string_count() const noexcept 
{
    return strings_.size();
}
std::string_view ohana::runtime::function_name(u32 function_index) const 
{
    if (function_index >= functions_.size()) 
    {
        throw ohana::runtime_error("índice de función fuera de rango");
    }
    return string(functions_[function_index].name_index);
}
std::string_view ohana::runtime::string(u32 string_index) const 
{
    if (string_index >= strings_.size()) 
    {
        throw ohana::runtime_error("índice de string fuera de rango");
    }
    return strings_[string_index];
}
void ohana::runtime::set_output(OutputFunction function) 
{
    output_ = std::move(function);
}
void ohana::runtime::set_wait(WaitFunction function)
{
    wait_ = std::move(function);
}
void ohana::runtime::register_native(std::string name, NativeFunction function) 
{
    if (name.empty()) 
    {
        throw ohana::runtime_error("el nombre de la función nativa no puede estar vacío");
    }
    if (!function) 
    {
        throw ohana::runtime_error("la función nativa no puede ser vacía");
    }
    if (auto* native = find_native(name)) 
    {
        *native = std::move(function);
        return;
    }
    String storedName(name.begin(), name.end());
    natives_.emplace_back(std::move(storedName), std::move(function));
}
void ohana::runtime::register_natives(std::initializer_list<NativeRegistration> functions) 
{
    for (const auto& native : functions) 
    {
        register_native(native.name, native.function);
    }
}
void ohana::runtime::load() 
{
    if (size_ < 1 + 4 + 4 + 4 + 4)
    {
        throw ohana::runtime_error("binario de Ohana demasiado pequeño");
    }
    uptr offset = 0;
    version_ = read_u8(offset);
    validate_version();
    function_count_ = read_u32(offset);
    code_count_ = read_u32(offset);
    string_count_ = read_u32(offset);
    text_bytes_ = read_u32(offset);
    function_table_offset_ = offset;
    parse_functions();
    code_offset_ = function_table_offset_ + static_cast<uptr>(function_count_) * 16;
    parse_code();
    parse_strings();
    for (const auto& function : functions_) 
    {
        if (function.entry >= code_count_) 
        {
            throw ohana::runtime_error("entrada de función fuera del bytecode");
        }
        if (function.name_index >= strings_.size()) 
        {
            throw ohana::runtime_error("índice de nombre de función fuera del pool");
        }
        if (function.parameter_count > function.local_count) 
        {
            throw ohana::runtime_error("cantidad de parámetros/locales inválida");
        }
    }
}
void ohana::runtime::validate_version() const 
{
    if (version_ != kVersion) 
    {
        throw ohana::runtime_error("versión de binario de Ohana no soportada");
    }
}
void ohana::runtime::parse_functions() 
{
    const uptr bytes = static_cast<uptr>(function_count_) * 16;
    require(function_table_offset_, bytes, "tabla de funciones");
    functions_.clear();
    functions_.reserve(function_count_);
    uptr offset = function_table_offset_;
    for (u32 i = 0; i < function_count_; ++i) 
    {
        FunctionInfo function;
        function.name_index = read_u32(offset);
        function.entry = read_u32(offset);
        function.parameter_count = read_u32(offset);
        function.local_count = read_u32(offset);
        functions_.push_back(function);
    }
}
void ohana::runtime::parse_code() 
{
    instruction_offsets_.clear();
    instruction_offsets_.reserve(code_count_);
    uptr offset = code_offset_;
    for (u32 i = 0; i < code_count_; ++i) 
    {
        const uptr start = offset;
        const auto op = read_u8(offset);
        const auto argument_type = static_cast<ArgumentType>(read_u8(offset));
        switch (argument_type)
        {
        case ArgumentType::None:
        {
            break;
        }
        case ArgumentType::Int64:
        case ArgumentType::Float64:
        {
            require(offset, 8, "operando de 64 bits");
            offset += 8;
            break;
        }
        case ArgumentType::Bool:
        {
            require(offset, 1, "operando booleano");
            ++offset;
            break;
        }
        case ArgumentType::String:
        case ArgumentType::U32:
        {
            require(offset, 4, "operando de 32 bits");
            offset += 4;
            break;
        }
        default:
            throw ohana::runtime_error("tipo de operando de bytecode desconocido");
        }
        if (offset <= start) 
        {
            throw ohana::runtime_error("instrucción de bytecode inválida");
        }
        instruction_offsets_.push_back(start);
        if (op > CallNative) 
        {
            throw ohana::runtime_error("opcode de bytecode desconocido");
        }
    }
    text_pool_offset_ = offset;
    require(text_pool_offset_, text_bytes_, "pool de texto");
    if (text_pool_offset_ + text_bytes_ != size_) 
    {
        throw ohana::runtime_error("el pool de texto no termina exactamente al final del binario");
    }
}
void ohana::runtime::parse_strings()
{
    strings_.clear();
    strings_.reserve(string_count_);
    uptr offset = text_pool_offset_;
    const uptr end = text_pool_offset_ + text_bytes_;
    for (u32 i = 0; i < string_count_; ++i) 
    {
        if (offset > end || end - offset < 4)
        {
            throw ohana::runtime_error("entrada de string incompleta");
        }
        const u32 length = read_u32(offset);
        require(offset, length, "contenido de string");
        strings_.emplace_back(reinterpret_cast<const char*>(data_ + offset), static_cast<uptr>(length));
        offset += length;
    }
    if (offset != end) 
    {
        throw ohana::runtime_error("bytes sobrantes en el pool de texto");
    }
}
u8 ohana::runtime::read_u8(uptr& offset) const 
{
    require(offset, 1, "u8");
    return data_[offset++];
}
u32 ohana::runtime::read_u32(uptr& offset) const 
{
    const auto value = read_u32_at(offset);
    offset += 4;
    return value;
}
u64 ohana::runtime::read_u64(uptr& offset) const 
{
    const auto value = read_u64_at(offset);
    offset += 8;
    return value;
}
s64 ohana::runtime::read_i64(uptr& offset) const 
{
    const auto bits = read_u64(offset);
    s64 value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
f64 ohana::runtime::read_f64(uptr& offset) const 
{
    const auto bits = read_u64(offset);
    f64 value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
u32 ohana::runtime::read_u32_at(uptr offset) const 
{
    require(offset, 4, "u32");
    return static_cast<u32>(data_[offset]) | (static_cast<u32>(data_[offset + 1]) << 8) | (static_cast<u32>(data_[offset + 2]) << 16) | (static_cast<u32>(data_[offset + 3]) << 24);
}
u64 ohana::runtime::read_u64_at(uptr offset) const 
{
    require(offset, 8, "u64");
    u64 value = 0;
    for (int i = 0; i < 8; ++i) 
    {
        value |= static_cast<u64>(data_[offset + i]) << (i * 8);
    }
    return value;
}
void ohana::runtime::require(uptr offset, uptr count, const char* what) const 
{
    if (offset > size_ || count > size_ - offset) 
    {
        throw ohana::runtime_error(std::string("lectura fuera de rango: ") + what);
    }
}
uptr ohana::runtime::instruction_offset(u32 index) const 
{
    if (index >= instruction_offsets_.size()) 
    {
        throw ohana::runtime_error("índice de instrucción fuera de rango");
    }
    return instruction_offsets_[index];
}
u32 ohana::runtime::instruction_u32(uptr& offset, ArgumentType type) const 
{
    if (type != ArgumentType::U32 && type != ArgumentType::String) 
    {
        throw ohana::runtime_error("se esperaba un operando uint32");
    }
    return read_u32(offset);
}
ohana::runtime::Value ohana::runtime::decode_argument(uptr& offset, ArgumentType type) const
{
    switch (type) 
    {
    case ArgumentType::None:
        return std::monostate{};
    case ArgumentType::Int64:
        return read_i64(offset);
    case ArgumentType::Float64:
        return read_f64(offset);
    case ArgumentType::Bool:
        return read_u8(offset) != 0;
    case ArgumentType::String: 
    {
        const auto index = read_u32(offset);
        return string(index);
    }
    case ArgumentType::U32:
        return std::monostate{};
    }
    throw ohana::runtime_error("tipo de operando desconocido");
}
ohana::runtime::Value ohana::runtime::run(u32 entry_function, const std::vector<Value>* arguments)
{
    const auto& function = functions_.at(entry_function);
    if (arguments != nullptr && arguments->size() != function.parameter_count) 
    {
        throw ohana::runtime_error("cantidad incorrecta de argumentos para " + std::string(function_name(entry_function)));
    }
    Frame frame;
    frame.function_index = entry_function;
    frame.ip = function.entry;
    frame.stack_base = 0;
    frame.locals.resize(function.local_count);
    if (arguments != nullptr) 
    {
        for (uptr i = 0; i < arguments->size(); ++i) 
        {
            frame.locals[i] = (*arguments)[i];
        }
    }
    frames_.push_back(std::move(frame));
    halted_ = false;
    last_result_ = std::monostate{};
    while (!frames_.empty() && !halted_ && !suspended_) 
    {
        const uptr frame_index = frames_.size() - 1;
        const u32 ip = frames_.back().ip;
        const auto offset = instruction_offset(ip);
        u32 next_ip = ip;
        execute_instruction(next_ip, offset);
        if (!halted_ && frames_.size() == frame_index + 1) 
        {
            frames_.back().ip = next_ip;
        }
    }
    return last_result_;
}
void ohana::runtime::execute_instruction(u32& ip, uptr instruction_offset_value) 
{
    uptr offset = instruction_offset_value;
    const auto op = read_u8(offset);
    const auto argument_type = static_cast<ArgumentType>(read_u8(offset));
    const auto jump_target = [&]() {
        if (argument_type != ArgumentType::U32) 
        {
            throw ohana::runtime_error("instrucción requiere argumento uint32");
        }
        const auto target = read_u32(offset);
        if (target >= code_count_) 
        {
            throw ohana::runtime_error("salto fuera del bytecode");
        }
        return target;
    };
    switch (op) 
    {
    case Nop:
    {
        break;
    }
    case PushInt:
    {
        if (argument_type != ArgumentType::Int64)
        {
            throw ohana::runtime_error("PUSH_INT inválido");
        }
        push(read_i64(offset));
        break;
    }
    case PushFloat:
    {
        if (argument_type != ArgumentType::Float64)
        {
            throw ohana::runtime_error("PUSH_FLOAT inválido");
        }
        push(read_f64(offset));
        break;
    }
    case PushBool:
    {
        if (argument_type != ArgumentType::Bool)
        {
            throw ohana::runtime_error("PUSH_BOOL inválido");
        }
        push(read_u8(offset) != 0);
        break;
    }
    case PushString:
    {
        if (argument_type != ArgumentType::String)
        {
            throw ohana::runtime_error("PUSH_STRING inválido");
        }
        push(decode_argument(offset, argument_type));
        break;
    }
    case Load: 
    {
        const auto slot = instruction_u32(offset, argument_type);
        if (frames_.back().function_index == std::numeric_limits<u32>::max()) 
        {
            if (slot >= globals_.size()) 
            {
                push(std::monostate{});
            }
            else 
            {
                push(globals_[slot]);
            }
        }
        else 
        {
            push(local(slot));
        }
        break;
    }
    case Store: 
    {
        const auto slot = instruction_u32(offset, argument_type);
        const auto value = pop();
        if (frames_.back().function_index == std::numeric_limits<u32>::max()) 
        {
            global(slot) = value;
        }
        else 
        {
            local(slot) = value;
        }
        break;
    }
    case Add: 
    {
        const auto rhs = pop();
        const auto lhs = pop();
        push(binary_add(lhs, rhs));
        break;
    }
    case Sub: 
    {
        const auto rhs = pop();
        const auto lhs = pop();
        push(binary_sub(lhs, rhs));
        break;
    }
    case Mul: 
    {
        const auto rhs = pop();
        const auto lhs = pop();
        push(binary_mul(lhs, rhs));
        break;
    }
    case Div:
    {
        const auto rhs = pop();
        const auto lhs = pop();
        push(binary_div(lhs, rhs));
        break;
    }
    case Equal: 
    {
        const auto rhs = pop();
        const auto lhs = pop();
        push(compare_equal(lhs, rhs));
        break;
    }
    case NotEqual: 
    {
        const auto rhs = pop();
        const auto lhs = pop();
        push(!std::get<bool>(compare_equal(lhs, rhs)));
        break;
    }
    case Less: 
    {
        const auto rhs = pop();
        const auto lhs = pop();
        push(compare_order(lhs, rhs, Less));
        break;
    }
    case LessEqual: 
    {
        const auto rhs = pop();
        const auto lhs = pop();
        push(compare_order(lhs, rhs, LessEqual));
        break;
    }
    case Greater: 
    {
        const auto rhs = pop();
        const auto lhs = pop();
        push(compare_order(lhs, rhs, Greater));
        break;
    }
    case GreaterEqual:
    {
        const auto rhs = pop();
        const auto lhs = pop();
        push(compare_order(lhs, rhs, GreaterEqual));
        break;
    }
    case Jump:
    {
        ip = jump_target();
        return;
    }
    case JumpIfFalse: 
    {
        const auto target = jump_target();
        const auto condition = pop();
        if (!truthy(condition)) 
        {
            ip = target;
            return;
        }
        break;
    }
    case Call: 
    {
        const auto function_index = instruction_u32(offset, argument_type);
        if (function_index >= functions_.size()) 
        {
            throw ohana::runtime_error("índice de función fuera de rango");
        }
        const auto& function = functions_[function_index];
        if (stack_.size() < function.parameter_count) 
        {
            throw ohana::runtime_error("pila insuficiente para CALL");
        }
        Frame frame;
        frame.function_index = function_index;
        frame.ip = function.entry;
        frame.stack_base = stack_.size() - function.parameter_count;
        frame.locals.resize(function.local_count);
        for (uptr i = 0; i < function.parameter_count; ++i) 
        {
            frame.locals[i] = stack_[frame.stack_base + i];
        }
        stack_.resize(frame.stack_base);
        frames_.back().ip = ip + 1;
        frames_.push_back(std::move(frame));
        return;
    }
    case Return: 
    {
        if (frames_.empty()) 
        {
            throw ohana::runtime_error("RETURN fuera de una función");
        }
        const auto base = frames_.back().stack_base;
        Value result = std::monostate{};
        bool has_result = false;
        if (stack_.size() > base) 
        {
            if (stack_.size() != base + 1) 
            {
                throw ohana::runtime_error("valores sobrantes en la pila al retornar");
            }
            result = pop();
            has_result = true;
        }
        stack_.resize(base);
        const bool top_level = frames_.back().function_index == std::numeric_limits<u32>::max();
        frames_.pop_back();
        if (frames_.empty() || top_level) 
        {
            last_result_ = has_result ? result : Value(std::monostate{});
            halted_ = true;
            return;
        }
        push(result);
        return;
    }
    case Pop:
    {
        (void)pop();
        break;
    }
    case Halt:
    {
        halted_ = true;
        if (!frames_.empty())
        {
            frames_.clear();
        }
        last_result_ = stack_.empty() ? Value(std::monostate{}) : stack_.back();
        return;
    }
    case Wait: 
    {
        const auto value = pop();
        if (wait_) 
        {
            wait_(value);
        }
        break;
    }
    case ShowString: 
    {
        const auto value = pop();
        if (output_) {
            output_(value_to_string(value));
        }
        break;
    }
    case CallNative: 
    {
        if (argument_type != ArgumentType::String) 
        {
            throw ohana::runtime_error("CALL_NATIVE requiere un nombre de función");
        }
        const auto name_value = decode_argument(offset, argument_type);
        const auto& name = std::get<std::string_view>(name_value);
        auto* native = find_native(name);
        if (native == nullptr) 
        {
            throw ohana::runtime_error("función nativa no registrada: " + std::string(name));
        }
        const auto argument_count = pop();
        if (!std::holds_alternative<s64>(argument_count)) 
        {
            throw ohana::runtime_error("CALL_NATIVE requiere una cantidad de argumentos válida");
        }
        const auto count = std::get<s64>(argument_count);
        if (count < 0) 
        {
            throw ohana::runtime_error("CALL_NATIVE requiere una cantidad de argumentos no negativa");
        }
        const auto argument_size = static_cast<uptr>(count);
        if (stack_.size() < argument_size)
        {
            throw ohana::runtime_error("pila insuficiente para CALL_NATIVE");
        }
        const auto stack_base = frames_.back().stack_base;
        if (stack_.size() - stack_base < argument_size) 
        {
            throw ohana::runtime_error("CALL_NATIVE intentaría consumir valores fuera de la llamada");
        }
        std::vector<Value> arguments(argument_size);
        for (uptr index = 0; index < argument_size; ++index) 
        {
            arguments[argument_size - index - 1] = pop();
        }
        Value result = (*native)(arguments);
        if (std::holds_alternative<Yield>(result))
        {
            for (const Value& argument : arguments)
            {
                push(argument);
            }
            push(count);
            suspended_ = true;
            return;
        }
        push(result);
        break;
    }
    default:
        throw ohana::runtime_error("opcode desconocido");
    }
    ++ip;
}
void ohana::runtime::push(const Value& value) 
{
    stack_.push_back(value);
}
ohana::runtime::Value ohana::runtime::pop()
{
    if (stack_.empty()) 
    {
        throw ohana::runtime_error("pila vacía");
    }
    auto value = stack_.back();
    stack_.pop_back();
    return value;
}
ohana::runtime::Value& ohana::runtime::local(u32 slot)
{
    if (frames_.empty() || slot >= frames_.back().locals.size()) 
    {
        throw ohana::runtime_error("local fuera de rango");
    }
    return frames_.back().locals[slot];
}
const ohana::runtime::Value& ohana::runtime::local(u32 slot) const
{
    if (frames_.empty() || slot >= frames_.back().locals.size()) 
    {
        throw ohana::runtime_error("local fuera de rango");
    }
    return frames_.back().locals[slot];
}
ohana::runtime::Value& ohana::runtime::global(u32 slot)
{
    if (slot >= globals_.size()) {
        globals_.resize(static_cast<uptr>(slot) + 1);
    }
    return globals_[slot];
}
const ohana::runtime::Value& ohana::runtime::global(u32 slot) const
{
    if (slot >= globals_.size()) 
    {
        throw ohana::runtime_error("global fuera de rango");
    }
    return globals_[slot];
}
ohana::runtime::Value ohana::runtime::binary_add(const Value& lhs, const Value& rhs) const
{
    if (std::holds_alternative<std::string_view>(lhs) || std::holds_alternative<std::string_view>(rhs)) 
    {
        return std::string(value_to_string(lhs)) + value_to_string(rhs);
    }
    if (!is_number(lhs) || !is_number(rhs)) 
    {
        throw ohana::runtime_error("ADD requiere números o texto");
    }
    if (std::holds_alternative<s64>(lhs) && std::holds_alternative<s64>(rhs)) 
    {
        return std::get<s64>(lhs) + std::get<s64>(rhs);
    }
    return to_number(lhs) + to_number(rhs);
}
ohana::runtime::Value ohana::runtime::binary_sub(const Value& lhs, const Value& rhs) const
{
    if (!is_number(lhs) || !is_number(rhs)) 
    {
        throw ohana::runtime_error("SUB requiere números");
    }
    if (std::holds_alternative<s64>(lhs) && std::holds_alternative<s64>(rhs)) 
    {
        return std::get<s64>(lhs) - std::get<s64>(rhs);
    }
    return to_number(lhs) - to_number(rhs);
}
ohana::runtime::Value ohana::runtime::binary_mul(const Value& lhs, const Value& rhs) const
{
    if (!is_number(lhs) || !is_number(rhs)) 
    {
        throw ohana::runtime_error("MUL requiere números");
    }
    if (std::holds_alternative<s64>(lhs) && std::holds_alternative<s64>(rhs)) 
    {
        return std::get<s64>(lhs) * std::get<s64>(rhs);
    }
    return to_number(lhs) * to_number(rhs);
}
ohana::runtime::Value ohana::runtime::binary_div(const Value& lhs, const Value& rhs) const
{
    if (!is_number(lhs) || !is_number(rhs)) 
    {
        throw ohana::runtime_error("DIV requiere números");
    }
    const f64 divisor = to_number(rhs);
    if (divisor == 0.0) 
    {
        throw ohana::runtime_error("división por cero");
    }
    return to_number(lhs) / divisor;
}
ohana::runtime::Value ohana::runtime::compare_equal(const Value& lhs, const Value& rhs) const
{
    if (lhs.index() == rhs.index()) 
    {
        if (std::holds_alternative<std::monostate>(lhs)) return true;
        if (std::holds_alternative<s64>(lhs)) return std::get<s64>(lhs) == std::get<s64>(rhs);
        if (std::holds_alternative<f64>(lhs)) return std::get<f64>(lhs) == std::get<f64>(rhs);
        if (std::holds_alternative<bool>(lhs)) return std::get<bool>(lhs) == std::get<bool>(rhs);
        if (std::holds_alternative<std::string_view>(lhs)) return std::get<std::string_view>(lhs) == std::get<std::string_view>(rhs);
        if (std::holds_alternative<std::string>(lhs)) return std::get<std::string>(lhs) == std::get<std::string>(rhs);
    }
    if (is_number(lhs) && is_number(rhs)) 
    {
        return to_number(lhs) == to_number(rhs);
    }
    return false;
}
ohana::runtime::Value ohana::runtime::compare_order(const Value& lhs, const Value& rhs, u8 op) const
{
    if (is_number(lhs) && is_number(rhs))
    {
        const auto a = to_number(lhs);
        const auto b = to_number(rhs);
        switch (op) 
        {
        case Less: 
            return a < b;
        case LessEqual: 
            return a <= b;
        case Greater: 
            return a > b;
        case GreaterEqual:
            return a >= b;
        default:
            break;
        }
    }
    if ((std::holds_alternative<std::string_view>(lhs) || std::holds_alternative<std::string>(lhs)) && (std::holds_alternative<std::string_view>(rhs) || std::holds_alternative<std::string>(rhs))) {
        const auto a = value_to_string(lhs);
        const auto b = value_to_string(rhs);
        switch (op) 
        {
        case Less: 
            return a < b;
        case LessEqual:
            return a <= b;
        case Greater: 
            return a > b;
        case GreaterEqual:
            return a >= b;
        default:
            break;
        }
    }
    throw ohana::runtime_error("comparación de orden no soportada");
}
bool ohana::runtime::is_number(const Value& value)
{
    return std::holds_alternative<s64>(value) || std::holds_alternative<f64>(value);
}
f64 ohana::runtime::to_number(const Value& value) 
{
    if (std::holds_alternative<s64>(value)) return static_cast<f64>(std::get<s64>(value));
    if (std::holds_alternative<f64>(value)) return std::get<f64>(value);
    throw ohana::runtime_error("el valor no es numérico");
}
std::string ohana::runtime::value_to_string(const Value& value)
{
    if (std::holds_alternative<std::monostate>(value)) return "none";
    if (std::holds_alternative<s64>(value)) return std::to_string(std::get<s64>(value));
    if (std::holds_alternative<f64>(value)) return std::to_string(std::get<f64>(value));
    if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? "true" : "false";
    if (std::holds_alternative<std::string_view>(value)) return std::string(std::get<std::string_view>(value));
    if (std::holds_alternative<std::string>(value)) return std::get<std::string>(value);
    return std::string();
}
bool ohana::runtime::truthy(const Value& value) 
{
    if (std::holds_alternative<std::monostate>(value)) return false;
    if (std::holds_alternative<bool>(value)) return std::get<bool>(value);
    if (std::holds_alternative<s64>(value)) return std::get<s64>(value) != 0;
    if (std::holds_alternative<f64>(value)) return std::get<f64>(value) != 0.0;
    if (std::holds_alternative<std::string_view>(value)) return !std::get<std::string_view>(value).empty();
    if (std::holds_alternative<std::string>(value)) return !std::get<std::string>(value).empty();
    return false;
}
ohana::runtime::NativeFunction* ohana::runtime::find_native(std::string_view name)
{
    for (auto& native : natives_)
    {
        if (native.first == name) return &native.second;
    }
    return nullptr;
}
bool ohana::get_string_argument(const std::vector<ohana::runtime::Value>& arguments, uptr index, std::string_view& value) 
{
    if (index >= arguments.size()) 
    {
        return false;
    }
    if (const auto* string_value = std::get_if<std::string_view>(&arguments[index])) 
    {
        value = *string_value;
        return true;
    }
    if (const auto* string_value = std::get_if<std::string>(&arguments[index])) 
    {
        value = *string_value;
        return true;
    }
    return false;
}
bool ohana::get_integer_argument(const std::vector<ohana::runtime::Value>& arguments, uptr index, s64& value) 
{
    if (index >= arguments.size()) 
    {
        return false;
    }
    if (const auto* integer_value = std::get_if<s64>(&arguments[index])) 
    {
        value = *integer_value;
        return true;
    }
    if (const auto* float_value = std::get_if<f64>(&arguments[index]); float_value && std::isfinite(*float_value) && *float_value >= static_cast<f64>(std::numeric_limits<s64>::min()) && *float_value < static_cast<f64>(std::numeric_limits<s64>::max())) 
    {
        value = static_cast<s64>(*float_value);
        return true;
    }
    return false;
}
bool ohana::get_boolean_argument(const std::vector<ohana::runtime::Value>& arguments, uptr index, bool& value) 
{
    if (index >= arguments.size()) 
    {
        return false;
    }
    if (const auto* boolean_value = std::get_if<bool>(&arguments[index])) 
    {
        value = *boolean_value;
        return true;
    }
    if (const auto* integer_value = std::get_if<s64>(&arguments[index])) 
    {
        value = *integer_value != 0;
        return true;
    }
    return false;
}