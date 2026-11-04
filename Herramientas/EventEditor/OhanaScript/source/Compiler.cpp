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
module ohana.compiler;
import ohana.bytecode;
import ohana.ast;
ohana::Chunk ohana::Compiler::compile(const Program& program) 
{
    chunk_ = {};
    functions_.clear();
    globals_.clear();
    scopes_.clear();
    loop_breaks_.clear();
    collect_functions(program);
    compile_top_level(program);
    emit(Op::Halt);
    for (const auto& statement : program.statements) 
    {
        if (const auto* function = std::get_if<Function>(&statement->node)) 
        {
            compile_function(*function, functions_.at(function->name));
        }
        else if (const auto* event = std::get_if<Event>(&statement->node)) 
        {
            compile_event(*event, functions_.at("evento:" + event->name));
        }
    }
    return std::move(chunk_);
}
uint32_t ohana::Compiler::emit(Op op, ohana::CompileArg arg)
{
    chunk_.code.push_back(Instruction{ op, std::move(arg) });
    return static_cast<uint32_t>(chunk_.code.size() - 1);
}
void ohana::Compiler::patch(std::size_t instruction, uint32_t target) 
{
    chunk_.code.at(instruction).arg = target;
}
void ohana::Compiler::collect_functions(const Program& program) 
{
    for (const auto& statement : program.statements) 
    {
        if (const auto* function = std::get_if<Function>(&statement->node)) 
        {
            if (functions_.contains(function->name)) 
            {
                throw std::runtime_error("función duplicada: " + function->name);
            }
            const auto index = static_cast<uint32_t>(chunk_.functions.size());
            functions_[function->name] = index;
            chunk_.functions.push_back(FunctionInfo{ function->name,0,static_cast<uint32_t>(function->params.size()),0 });
        }
        else if (const auto* event = std::get_if<Event>(&statement->node)) 
        {
            const std::string key = "evento:" + event->name;
            if (functions_.contains(key)) 
            {
                throw std::runtime_error("evento duplicado: " + event->name);
            }
            const auto index = static_cast<uint32_t>(chunk_.functions.size());
            functions_[key] = index;
            chunk_.functions.push_back(FunctionInfo{ event->name,0,0,0 });
        }
    }
}
void ohana::Compiler::compile_top_level(const Program& program) 
{
    for (const auto& statement : program.statements) 
    {
        if (std::holds_alternative<Function>(statement->node) || std::holds_alternative<Event>(statement->node)) 
        {
            continue;
        }
        stmt(*statement);
    }
}
void ohana::Compiler::compile_function(const Function& function, uint32_t function_index) 
{
    Scope scope;
    scope.parameters = function.params;
    for (const auto& parameter : function.params) 
    {
        if (scope.locals.contains(parameter)) 
        {
            throw std::runtime_error("parámetro duplicado en función " + function.name + ": " + parameter);
        }
        scope.locals.emplace(parameter, static_cast<uint32_t>(scope.locals.size()));
    }
    scopes_.push_back(std::move(scope));
    chunk_.functions.at(function_index).entry = static_cast<uint32_t>(chunk_.code.size());
    for (const auto& statement : function.body) 
    {
        stmt(*statement);
    }
    if (chunk_.code.empty() || chunk_.code.back().op != Op::Return) 
    {
        emit(Op::Return);
    }
    chunk_.functions.at(function_index).local_count = static_cast<uint32_t>(current_scope().locals.size());
    scopes_.pop_back();
}
void ohana::Compiler::compile_event(const Event& event, uint32_t function_index)
{
    Scope scope;
    scopes_.push_back(std::move(scope));
    chunk_.functions.at(function_index).entry = static_cast<uint32_t>(chunk_.code.size());
    for (const auto& statement : event.body) 
    {
        stmt(*statement);
    }
    if (chunk_.code.empty() || chunk_.code.back().op != Op::Return) 
    {
        emit(Op::Return);
    }
    chunk_.functions.at(function_index).local_count = static_cast<uint32_t>(current_scope().locals.size());
    scopes_.pop_back();
}
void ohana::Compiler::stmt(const Stmt& statement) 
{
    std::visit([&](auto&& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, ExprStmt>) 
            {
                const auto* call = std::get_if<Call>(&node.expr->node);
                bool consumes_result = false;
                if (call != nullptr)
                {
                    const auto* name = std::get_if<Name>(&call->callee->node);
                    consumes_result = name != nullptr && (name->value == "mostrar" || name->value == "esperar");
                }
                expr(*node.expr);
                if (!consumes_result)
                {
                    emit(Op::Pop);
                }
            }
            else if constexpr (std::is_same_v<T, Assign>) 
            {
                const auto slot = lookup_variable(node.name);
                if (node.op == "=") 
                {
                    expr(*node.value);
                }
                else {
                    emit(Op::Load, slot);
                    expr(*node.value);

                    if (node.op == "+=") 
                    {
                        emit(Op::Add);
                    }
                    else if (node.op == "-=") 
                    {
                        emit(Op::Sub);
                    }
                    else 
                    {
                        throw std::runtime_error("operador de asignación no implementado: " + node.op);
                    }
                }
                emit(Op::Store, slot);
            }
            else if constexpr (std::is_same_v<T, If>)
            {
                expr(*node.condition);
                const auto jump_false = emit(Op::JumpIfFalse, uint32_t{ 0 });
                for (const auto& body_statement : node.thenBranch) 
                {
                    stmt(*body_statement);
                }
                if (!node.elseBranch.empty()) 
                {
                    const auto jump_end = emit(Op::Jump, uint32_t{ 0 });
                    patch(jump_false, static_cast<uint32_t>(chunk_.code.size()));
                    for (const auto& body_statement : node.elseBranch) 
                    {
                        stmt(*body_statement);
                    }
                    patch(jump_end, static_cast<uint32_t>(chunk_.code.size()));
                }
                else 
                {
                    patch(jump_false, static_cast<uint32_t>(chunk_.code.size()));
                }
            }
            else if constexpr (std::is_same_v<T, While>) 
            {
                const auto start = static_cast<uint32_t>(chunk_.code.size());
                expr(*node.condition);
                const auto jump_false = emit(Op::JumpIfFalse, uint32_t{ 0 });
                loop_breaks_.emplace_back();
                for (const auto& body_statement : node.body) 
                {
                    stmt(*body_statement);
                }
                emit(Op::Jump, start);
                const auto loop_end = static_cast<uint32_t>(chunk_.code.size());
                patch(jump_false, loop_end);
                for (const auto jump : loop_breaks_.back())
                {
                    patch(jump, loop_end);
                }
                loop_breaks_.pop_back();
            }
            else if constexpr (std::is_same_v<T, Break>)
            {
                if (loop_breaks_.empty())
                {
                    throw std::runtime_error("romper solo puede usarse dentro de mientras");
                }
                loop_breaks_.back().push_back(emit(Op::Jump, uint32_t{ 0 }));
            }
            else if constexpr (std::is_same_v<T, Return>) 
            {
                if (node.value) 
                {
                    expr(*node.value);
                }
                emit(Op::Return);
            }
            else if constexpr (std::is_same_v<T, Function> || std::is_same_v<T, Event>) 
            {
                throw std::runtime_error("las funciones y eventos no pueden anidarse");
            }
        },
        statement.node);
}
void ohana::Compiler::expr(const Expr& expression)
{
    std::visit([&](auto&& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, Literal>) 
            {
                if (std::holds_alternative<int64_t>(node.value)) 
                {
                    emit(Op::PushInt, std::get<int64_t>(node.value));
                }
                else if (std::holds_alternative<double>(node.value)) 
                {
                    emit(Op::PushFloat, std::get<double>(node.value));
                }
                else if (std::holds_alternative<bool>(node.value)) 
                {
                    emit(Op::PushBool, std::get<bool>(node.value));
                }
                else if (std::holds_alternative<std::string>(node.value)) 
                {
                    emit(Op::PushString, std::get<std::string>(node.value));
                }
            }
            else if constexpr (std::is_same_v<T, Name>) 
            {
                emit(Op::Load, lookup_variable(node.value));
            }
            else if constexpr (std::is_same_v<T, Unary>) 
            {
                expr(*node.rhs);
                if (node.op == "-") 
                {
                    emit(Op::PushInt, int64_t{ -1 });
                    emit(Op::Mul);
                }
                else 
                {
                    throw std::runtime_error("operador unario no implementado: " + node.op);
                }
            }
            else if constexpr (std::is_same_v<T, Binary>) 
            {
                expr(*node.lhs);
                expr(*node.rhs);

                if (node.op == "+") 
                {
                    emit(Op::Add);
                }
                else if (node.op == "-") 
                {
                    emit(Op::Sub);
                }
                else if (node.op == "*") 
                {
                    emit(Op::Mul);
                }
                else if (node.op == "/") 
                {
                    emit(Op::Div);
                }
                else if (node.op == "==") 
                {
                    emit(Op::Equal);
                }
                else if (node.op == "!=") 
                {
                    emit(Op::NotEqual);
                }
                else if (node.op == "<") 
                {
                    emit(Op::Less);
                }
                else if (node.op == "<=") 
                {
                    emit(Op::LessEqual);
                }
                else if (node.op == ">") 
                {
                    emit(Op::Greater);
                }
                else if (node.op == ">=") 
                {
                    emit(Op::GreaterEqual);
                }
                else 
                {
                    throw std::runtime_error("operador no implementado: " + node.op);
                }
            }
            else if constexpr (std::is_same_v<T, Call>) 
            {
                const auto* name = std::get_if<Name>(&node.callee->node);
                if (name == nullptr) 
                {
                    throw std::runtime_error("solo se pueden llamar nombres de función");
                }
                for (const auto& argument : node.args) 
                {
                    expr(*argument);
                }
                if (name->value == "mostrar") 
                {
                    if (node.args.size() != 1) 
                    {
                        throw std::runtime_error("mostrar() recibe exactamente un argumento");
                    }
                    emit(Op::ShowString);
                    return;
                }
                if (name->value == "esperar") 
                {
                    if (node.args.size() != 1) 
                    {
                        throw std::runtime_error("esperar() recibe exactamente un argumento");
                    }
                    emit(Op::Wait);
                    return;
                }
                if (name->value == "entrada" || name->value == "contiene" || name->value == "minusculas" ||
                    name->value == "menu_crear" || name->value == "menu_opcion" ||
                    name->value == "menu_habilitar" || name->value == "menu_mostrar" ||
                    name->value == "menu_cerrar" || name->value == "menu_seleccion" ||
                    name->value == "menu_esperar" || name->value == "mapa_cargar" ||
                    name->value == "jugador_habilitar" ||
                    name->value == "compilacion_fecha" || name->value == "compilacion_hora" ||
                    name->value == "compilacion_autor" || name->value == "compilacion_modo" ||
                    name->value == "compilacion_info")
                {
                    if (name->value == "entrada" && (node.args.size() > 1))
                    {
                        throw std::runtime_error("entrada() recibe cero o un argumento");
                    }
                    if (name->value == "contiene" && node.args.size() != 2)
                    {
                        throw std::runtime_error("contiene() recibe exactamente dos argumentos");
                    }
                    if (name->value == "minusculas" && node.args.size() != 1)
                    {
                        throw std::runtime_error("minusculas() recibe exactamente un argumento");
                    }
                    if ((name->value == "menu_crear" || name->value == "menu_opcion") && node.args.size() != 1)
                    {
                        throw std::runtime_error(name->value + "() recibe exactamente un argumento");
                    }
                    if (name->value == "menu_habilitar" && node.args.size() != 2)
                    {
                        throw std::runtime_error("menu_habilitar() recibe exactamente dos argumentos");
                    }
                    if (name->value == "mapa_cargar" && node.args.size() != 1)
                    {
                        throw std::runtime_error("mapa_cargar() recibe exactamente un ID");
                    }
                    if (name->value == "jugador_habilitar" && node.args.size() != 1)
                    {
                        throw std::runtime_error("jugador_habilitar() recibe exactamente un booleano");
                    }
                    if ((name->value == "menu_mostrar" || name->value == "menu_cerrar" ||
                        name->value == "menu_seleccion" || name->value == "menu_esperar" ||
                        name->value == "compilacion_fecha" || name->value == "compilacion_hora" ||
                        name->value == "compilacion_autor" || name->value == "compilacion_modo" ||
                        name->value == "compilacion_info") && !node.args.empty())
                    {
                        throw std::runtime_error(name->value + "() no recibe argumentos");
                    }
                    emit(Op::PushInt, static_cast<int64_t>(node.args.size()));
                    emit(Op::CallNative, name->value);
                    return;
                }
                const auto it = functions_.find(name->value);
                if (it == functions_.end()) 
                {
                    throw std::runtime_error("función no definida: " + name->value);
                }
                const auto& info = chunk_.functions.at(it->second);
                if (info.parameter_count != node.args.size()) 
                {
                    throw std::runtime_error("número incorrecto de argumentos para " + name->value + ": se esperaban " + std::to_string(info.parameter_count) + ", se recibieron " + std::to_string(node.args.size()));
                }
                emit(Op::Call, it->second);
            }
        },
        expression.node);
}
uint32_t ohana::Compiler::declare_local(const std::string& name) 
{
    auto& scope = current_scope();
    if (const auto it = scope.locals.find(name); it != scope.locals.end()) 
    {
        return it->second;
    }
    const auto slot = static_cast<uint32_t>(scope.locals.size());
    scope.locals.emplace(name, slot);
    return slot;
}
uint32_t ohana::Compiler::lookup_variable(const std::string& name) 
{
    if (in_function()) 
    {
        if (const auto it = current_scope().locals.find(name); it != current_scope().locals.end()) 
        {
            return it->second;
        }
        return declare_local(name);
    }
    return declare_global(name);
}
uint32_t ohana::Compiler::declare_global(const std::string& name) 
{
    if (const auto it = globals_.find(name); it != globals_.end()) 
    {
        return it->second;
    }
    const auto slot = static_cast<uint32_t>(globals_.size());
    globals_.emplace(name, slot);
    return slot;
}
bool ohana::Compiler::in_function() const 
{
    return !scopes_.empty();
}
ohana::Compiler::Scope& ohana::Compiler::current_scope()
{
    if (!in_function()) 
    {
        throw std::runtime_error("no existe un ámbito de función activo");
    }
    return scopes_.back();
}
