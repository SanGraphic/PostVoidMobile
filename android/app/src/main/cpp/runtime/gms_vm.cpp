#include "gms_vm.h"
#include "gms_runtime.h"
#include "renderer_gles.h"
#include "oboe_audio_engine.h"
#include <cmath>

GMS_BytecodeVM::GMS_BytecodeVM() {}
GMS_BytecodeVM::~GMS_BytecodeVM() {}

void GMS_BytecodeVM::initialize() {
    LOGI("[GMS_BytecodeVM] Initializing GMS2 Stack Machine & Standard Library...");

    // Math Functions
    registerFunction("dsin", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        return GMS_Value(GMSMath::dsin(args.empty() ? 0.0 : args[0].asReal()));
    });
    registerFunction("dcos", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        return GMS_Value(GMSMath::dcos(args.empty() ? 0.0 : args[0].asReal()));
    });
    registerFunction("point_distance", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        if (args.size() < 4) return GMS_Value(0.0);
        return GMS_Value(GMSMath::point_distance(args[0].asReal(), args[1].asReal(), args[2].asReal(), args[3].asReal()));
    });
    registerFunction("point_direction", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        if (args.size() < 4) return GMS_Value(0.0);
        return GMS_Value(GMSMath::point_direction(args[0].asReal(), args[1].asReal(), args[2].asReal(), args[3].asReal()));
    });
    registerFunction("lengthdir_x", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        if (args.size() < 2) return GMS_Value(0.0);
        return GMS_Value(GMSMath::lengthdir_x(args[0].asReal(), args[1].asReal()));
    });
    registerFunction("lengthdir_y", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        if (args.size() < 2) return GMS_Value(0.0);
        return GMS_Value(GMSMath::lengthdir_y(args[0].asReal(), args[1].asReal()));
    });
    registerFunction("lengthdir_z", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        if (args.size() < 2) return GMS_Value(0.0);
        return GMS_Value(GMSMath::lengthdir_z(args[0].asReal(), args[1].asReal()));
    });
    registerFunction("clamp", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        if (args.size() < 3) return GMS_Value(0.0);
        return GMS_Value(GMSMath::clamp(args[0].asReal(), args[1].asReal(), args[2].asReal()));
    });
    registerFunction("lerp", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        if (args.size() < 3) return GMS_Value(0.0);
        return GMS_Value(GMSMath::lerp(args[0].asReal(), args[1].asReal(), args[2].asReal()));
    });
    registerFunction("random", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        return GMS_Value(GMSMath::gms_random(args.empty() ? 1.0 : args[0].asReal()));
    });
    registerFunction("irandom", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        return GMS_Value(GMSMath::gms_irandom(args.empty() ? 1 : args[0].asInt()));
    });
    registerFunction("random_range", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        if (args.size() < 2) return GMS_Value(0.0);
        return GMS_Value(GMSMath::gms_random_range(args[0].asReal(), args[1].asReal()));
    });

    // Array Functions
    registerFunction("@@NewGMLArray@@", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        return GMS_Value::CreateArray(args.size());
    });
    registerFunction("array_length", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        if (args.empty() || args[0].type != GMS_Type::ARRAY || !args[0].aVal) return GMS_Value(0);
        return GMS_Value(static_cast<int>(args[0].aVal->size()));
    });
    registerFunction("array_create", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        size_t size = args.empty() ? 0 : static_cast<size_t>(args[0].asInt());
        return GMS_Value::CreateArray(size);
    });

    // DS Map / List Functions
    registerFunction("ds_map_create", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        return GMS_Value(GMS_DataStructures::get().ds_map_create());
    });
    registerFunction("ds_map_destroy", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        if (!args.empty()) GMS_DataStructures::get().ds_map_destroy(args[0].asInt());
        return GMS_Value();
    });
    registerFunction("ds_list_create", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        return GMS_Value(GMS_DataStructures::get().ds_list_create());
    });

    // Audio & Draw Functions
    registerFunction("audio_play_sound", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        int soundId = args.empty() ? 0 : args[0].asInt();
        bool loop = args.size() > 2 ? args[2].asBool() : false;
        return GMS_Value(OboeAudioEngine::get().playSound(soundId, 1.0f, loop));
    });
    registerFunction("audio_stop_sound", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        if (!args.empty()) OboeAudioEngine::get().stopSound(args[0].asInt());
        return GMS_Value();
    });

    // Vertex buffer & Shader functions
    registerFunction("vertex_format_begin", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) { return GMS_Value(); });
    registerFunction("vertex_format_add_position_3d", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) { return GMS_Value(); });
    registerFunction("vertex_format_add_colour", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) { return GMS_Value(); });
    registerFunction("vertex_format_add_texcoord", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) { return GMS_Value(); });
    registerFunction("vertex_format_end", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) { return GMS_Value(1); });

    // Method / Closure support
    registerFunction("method", [](GMS_VMContext& ctx, const std::vector<GMS_Value>& args) {
        if (!args.empty()) return args[0];
        return GMS_Value();
    });

    LOGI("[GMS_BytecodeVM] %zu built-in GML library functions registered.", m_builtins.size());
}

void GMS_BytecodeVM::registerFunction(const std::string& name, std::function<GMS_Value(GMS_VMContext&, const std::vector<GMS_Value>&)> func) {
    m_builtins[name] = func;
}

void GMS_BytecodeVM::loadCodeBlock(const std::string& name, const std::vector<GMS_Op>& ops) {
    m_codeBlocks[name] = ops;
}

GMS_Value GMS_BytecodeVM::executeCode(const std::string& codeName, GMS_Instance* self, GMS_Instance* other, const std::vector<GMS_Value>& args) {
    auto it = m_codeBlocks.find(codeName);
    if (it == m_codeBlocks.end()) {
        return GMS_Value();
    }

    GMS_VMContext ctx;
    ctx.self = self;
    ctx.other = other;
    ctx.args = args;
    ctx.locals.resize(32);
    ctx.stack.reserve(64);

    return executeInstructionStream(it->second, ctx);
}

GMS_Value GMS_BytecodeVM::executeInstructionStream(const std::vector<GMS_Op>& ops, GMS_VMContext& ctx) {
    ctx.pc = 0;
    size_t numOps = ops.size();

    while (ctx.pc < static_cast<int>(numOps)) {
        const GMS_Op& op = ops[ctx.pc];

        switch (op.opcode) {
            case GMS_Opcode::PUSH:
            case GMS_Opcode::PUSHINT:
                ctx.stack.push_back(GMS_Value(op.intVal));
                break;

            case GMS_Opcode::PUSHLOC:
                if (op.intVal >= 0 && op.intVal < static_cast<int>(ctx.locals.size())) {
                    ctx.stack.push_back(ctx.locals[op.intVal]);
                } else {
                    ctx.stack.push_back(GMS_Value());
                }
                break;

            case GMS_Opcode::PUSHGLB:
                ctx.stack.push_back(GMS_Runtime::get().getGlobal(op.strVal));
                break;

            case GMS_Opcode::POP:
                if (!ctx.stack.empty()) {
                    GMS_Value val = ctx.stack.back();
                    ctx.stack.pop_back();
                    if (op.intVal >= 0 && op.intVal < static_cast<int>(ctx.locals.size())) {
                        ctx.locals[op.intVal] = val;
                    }
                }
                break;

            case GMS_Opcode::ADD: {
                if (ctx.stack.size() >= 2) {
                    GMS_Value b = ctx.stack.back(); ctx.stack.pop_back();
                    GMS_Value a = ctx.stack.back(); ctx.stack.pop_back();
                    if (a.type == GMS_Type::STRING || b.type == GMS_Type::STRING) {
                        ctx.stack.push_back(GMS_Value(a.asString() + b.asString()));
                    } else {
                        ctx.stack.push_back(GMS_Value(a.asReal() + b.asReal()));
                    }
                }
                break;
            }

            case GMS_Opcode::SUB: {
                if (ctx.stack.size() >= 2) {
                    GMS_Value b = ctx.stack.back(); ctx.stack.pop_back();
                    GMS_Value a = ctx.stack.back(); ctx.stack.pop_back();
                    ctx.stack.push_back(GMS_Value(a.asReal() - b.asReal()));
                }
                break;
            }

            case GMS_Opcode::MUL: {
                if (ctx.stack.size() >= 2) {
                    GMS_Value b = ctx.stack.back(); ctx.stack.pop_back();
                    GMS_Value a = ctx.stack.back(); ctx.stack.pop_back();
                    ctx.stack.push_back(GMS_Value(a.asReal() * b.asReal()));
                }
                break;
            }

            case GMS_Opcode::DIV: {
                if (ctx.stack.size() >= 2) {
                    GMS_Value b = ctx.stack.back(); ctx.stack.pop_back();
                    GMS_Value a = ctx.stack.back(); ctx.stack.pop_back();
                    double denom = b.asReal();
                    ctx.stack.push_back(GMS_Value(denom != 0.0 ? a.asReal() / denom : 0.0));
                }
                break;
            }

            case GMS_Opcode::CMP: {
                if (ctx.stack.size() >= 2) {
                    GMS_Value b = ctx.stack.back(); ctx.stack.pop_back();
                    GMS_Value a = ctx.stack.back(); ctx.stack.pop_back();
                    bool res = false;
                    switch (op.comparison) {
                        case 1: res = a.asReal() < b.asReal(); break;  // LT
                        case 2: res = a.asReal() <= b.asReal(); break; // LTE
                        case 3: res = a.asReal() == b.asReal(); break; // EQ
                        case 4: res = a.asReal() != b.asReal(); break; // NEQ
                        case 5: res = a.asReal() >= b.asReal(); break; // GTE
                        case 6: res = a.asReal() > b.asReal(); break;  // GT
                    }
                    ctx.stack.push_back(GMS_Value(res));
                }
                break;
            }

            case GMS_Opcode::DUP:
                if (!ctx.stack.empty()) {
                    ctx.stack.push_back(ctx.stack.back());
                }
                break;

            case GMS_Opcode::B:
                ctx.pc = op.branchTarget;
                continue;

            case GMS_Opcode::BT:
                if (!ctx.stack.empty()) {
                    bool cond = ctx.stack.back().asBool();
                    ctx.stack.pop_back();
                    if (cond) {
                        ctx.pc = op.branchTarget;
                        continue;
                    }
                }
                break;

            case GMS_Opcode::BF:
                if (!ctx.stack.empty()) {
                    bool cond = ctx.stack.back().asBool();
                    ctx.stack.pop_back();
                    if (!cond) {
                        ctx.pc = op.branchTarget;
                        continue;
                    }
                }
                break;

            case GMS_Opcode::CALL:
            case GMS_Opcode::CALLI: {
                std::vector<GMS_Value> callArgs(op.argc);
                for (int i = op.argc - 1; i >= 0; i--) {
                    if (!ctx.stack.empty()) {
                        callArgs[i] = ctx.stack.back();
                        ctx.stack.pop_back();
                    }
                }
                auto fnIt = m_builtins.find(op.strVal);
                if (fnIt != m_builtins.end()) {
                    GMS_Value retVal = fnIt->second(ctx, callArgs);
                    ctx.stack.push_back(retVal);
                } else {
                    ctx.stack.push_back(GMS_Value());
                }
                break;
            }

            case GMS_Opcode::RET:
                if (!ctx.stack.empty()) return ctx.stack.back();
                return GMS_Value();

            case GMS_Opcode::EXIT:
                return GMS_Value();

            default:
                break;
        }

        ctx.pc++;
    }

    if (!ctx.stack.empty()) return ctx.stack.back();
    return GMS_Value();
}
