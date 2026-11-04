module;
#include<string>
module ohana.bytecode;
std::string ohana::opcode_name(Op op) {
    switch (op) {
    case Op::Nop:
        return "NOP";
    case Op::PushInt:
        return "PUSH_INT";
    case Op::PushFloat:
        return "PUSH_FLOAT";
    case Op::PushBool:
        return "PUSH_BOOL";
    case Op::PushString:
        return "PUSH_STRING";
    case Op::Load:
        return "LOAD";
    case Op::Store:
        return "STORE";
    case Op::Add:
        return "ADD";
    case Op::Sub:
        return "SUB";
    case Op::Mul:
        return "MUL";
    case Op::Div:
        return "DIV";
    case Op::Equal:
        return "EQUAL";
    case Op::NotEqual:
        return "NOT_EQUAL";
    case Op::Less:
        return "LESS";
    case Op::LessEqual:
        return "LESS_EQUAL";
    case Op::Greater:
        return "GREATER";
    case Op::GreaterEqual:
        return "GREATER_EQUAL";
    case Op::Jump:
        return "JUMP";
    case Op::JumpIfFalse:
        return "JUMP_IF_FALSE";
    case Op::Call:
        return "CALL";
    case Op::Return:
        return "RETURN";
    case Op::Pop:
        return "POP";
    case Op::Halt:
        return "HALT";
    case Op::Wait:
        return "WAIT";
    case Op::ShowString:
        return "SHOW_STRING";
    case Op::CallNative:
        return "CALL_NATIVE";
    }

    return "UNKNOWN";
}
