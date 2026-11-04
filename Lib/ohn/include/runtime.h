#pragma once
#include <Types.h>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <stdexcept>
#include <DynamicArray.h>
#include <DynamicString.h>
namespace ohana {
    class runtime_error : public std::runtime_error {
    public:
        explicit runtime_error(const std::string& message);
    };
    class runtime {
    public:
        struct Yield {};
        using Value = std::variant<std::monostate, s64, f64, bool, std::string_view, std::string, Yield>;
        using NativeFunction = std::function<Value(const std::vector<Value>&)>;
        using OutputFunction = std::function<void(std::string_view)>;
        using WaitFunction = std::function<void(const Value&)>;
        struct NativeRegistration 
        {
            const char* name;
            NativeFunction function;
        };
        runtime(void* Data, uptr Size);
        runtime(const void* Data, uptr Size);
        void reset();
        Value execute();
        Value resume();
        bool is_suspended() const noexcept;
        Value call(std::string_view name, const std::vector<Value>& args = {});
        Value call(u32 function_index, const std::vector<Value>& args = {});
        Value call_preserving_state(std::string_view name, const std::vector<Value>& args = {});
        Value call_preserving_state(u32 function_index, const std::vector<Value>& args = {});
        uptr function_count() const noexcept;
        uptr code_count() const noexcept;
        uptr string_count() const noexcept;
        std::string_view function_name(u32 function_index) const;
        std::string_view string(u32 string_index) const;
        void set_output(OutputFunction function);
        void set_wait(WaitFunction function);
        void register_native(std::string name, NativeFunction function);
        void register_natives(std::initializer_list<NativeRegistration> functions);
    private:
        enum class ArgumentType : u8 
        {
            None = 0,
            Int64 = 1,
            Float64 = 2,
            Bool = 3,
            String = 4,
            U32 = 5
        };
        struct FunctionInfo 
        {
            u32 name_index = 0;
            u32 entry = 0;
            u32 parameter_count = 0;
            u32 local_count = 0;
        };
        struct Frame 
        {
            u32 function_index = 0;
            u32 ip = 0;
            uptr stack_base = 0;
            DynamicArray<Value> locals;
        };
        struct Instruction 
        {
            u8 op = 0;
            ArgumentType argument_type = ArgumentType::None;
            uptr offset = 0;
            uptr size = 0;
        };
        const u8* data_ = nullptr;
        uptr size_ = 0;
        u8 version_ = 0;
        u32 function_count_ = 0;
        u32 code_count_ = 0;
        u32 string_count_ = 0;
        u32 text_bytes_ = 0;
        uptr function_table_offset_ = 0;
        uptr code_offset_ = 0;
        uptr text_pool_offset_ = 0;
        DynamicArray<FunctionInfo> functions_;
        DynamicArray<std::string_view> strings_;
        DynamicArray<uptr> instruction_offsets_;
        DynamicArray<Value> stack_;
        DynamicArray<Value> globals_;
        DynamicArray<Frame> frames_;
        DynamicArray<std::pair<String, NativeFunction>> natives_;
        OutputFunction output_;
        WaitFunction wait_;
        bool halted_ = false;
        bool suspended_ = false;
        Value last_result_{};
        void load();
        void validate_version() const;
        void parse_functions();
        void parse_code();
        void parse_strings();
        u8 read_u8(uptr& offset) const;
        u32 read_u32(uptr& offset) const;
        u64 read_u64(uptr& offset) const;
        s64 read_i64(uptr& offset) const;
        f64 read_f64(uptr& offset) const;
        u32 read_u32_at(uptr offset) const;
        u64 read_u64_at(uptr offset) const;
        void require(uptr offset, uptr count, const char* what) const;
        uptr instruction_offset(u32 index) const;
        u32 instruction_u32(uptr& offset, ArgumentType type) const;
        Value decode_argument(uptr& offset, ArgumentType type) const;
        Value run(u32 entry_function, const std::vector<Value>* arguments);
        void execute_instruction(u32& ip, uptr instruction_offset);
        void push(const Value& value);
        Value pop();
        Value& local(u32 slot);
        const Value& local(u32 slot) const;
        Value& global(u32 slot);
        const Value& global(u32 slot) const;
        Value binary_add(const Value& lhs, const Value& rhs) const;
        Value binary_sub(const Value& lhs, const Value& rhs) const;
        Value binary_mul(const Value& lhs, const Value& rhs) const;
        Value binary_div(const Value& lhs, const Value& rhs) const;
        Value compare_equal(const Value& lhs, const Value& rhs) const;
        Value compare_order(const Value& lhs, const Value& rhs, u8 op) const;
        static bool is_number(const Value& value);
        static f64 to_number(const Value& value);
        static std::string value_to_string(const Value& value);
        static bool truthy(const Value& value);
        NativeFunction* find_native(std::string_view name);
    };
    [[nodiscard]] bool get_string_argument(const std::vector<runtime::Value>& arguments, uptr index, std::string_view& value);
    [[nodiscard]] bool get_integer_argument(const std::vector<runtime::Value>& arguments, uptr index, s64& value);
    [[nodiscard]] bool get_boolean_argument(const std::vector<runtime::Value>& arguments, uptr index, bool& value);
} // namespace ohana
