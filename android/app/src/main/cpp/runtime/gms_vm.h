#pragma once

#include "gms_types.h"
#include "gms_ds.h"
#include "gms_math.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <stack>

enum class GMS_Opcode {
    NOP,
    CONV,
    MUL,
    DIV,
    REM,
    MOD,
    ADD,
    SUB,
    AND,
    OR,
    XOR,
    NEG,
    NOT,
    SAL,
    SAR,
    CMP,
    POP,
    DUP,
    RET,
    EXIT,
    POPAF,
    B,
    BT,
    BF,
    PUSHENV,
    POPENV,
    PUSH,
    PUSHLOC,
    PUSHGLB,
    PUSHBLTN,
    PUSHINT,
    CALL,
    CALLV,
    CALLI,
    SETONER,
    METHOD
};

struct GMS_Op {
    GMS_Opcode opcode = GMS_Opcode::NOP;
    int type1 = 0;
    int type2 = 0;
    int comparison = 0; // 1=LT, 2=LTE, 3=EQ, 4=NEQ, 5=GTE, 6=GT
    int intVal = 0;
    double doubleVal = 0.0;
    std::string strVal;
    int branchTarget = 0;
    int argc = 0;
    std::string symbol;
};

struct GMS_VMContext {
    GMS_Instance* self = nullptr;
    GMS_Instance* other = nullptr;
    std::vector<GMS_Value> locals;
    std::vector<GMS_Value> args;
    std::vector<GMS_Value> stack;
    int pc = 0;
};

class GMS_BytecodeVM {
public:
    static GMS_BytecodeVM& get() {
        static GMS_BytecodeVM instance;
        return instance;
    }

    void initialize();
    void registerFunction(const std::string& name, std::function<GMS_Value(GMS_VMContext&, const std::vector<GMS_Value>&)> func);

    GMS_Value executeCode(const std::string& codeName, GMS_Instance* self, GMS_Instance* other, const std::vector<GMS_Value>& args);
    GMS_Value executeInstructionStream(const std::vector<GMS_Op>& ops, GMS_VMContext& ctx);

    void loadCodeBlock(const std::string& name, const std::vector<GMS_Op>& ops);

private:
    GMS_BytecodeVM();
    ~GMS_BytecodeVM();

    std::unordered_map<std::string, std::vector<GMS_Op>> m_codeBlocks;
    std::unordered_map<std::string, std::function<GMS_Value(GMS_VMContext&, const std::vector<GMS_Value>&)>> m_builtins;
};
