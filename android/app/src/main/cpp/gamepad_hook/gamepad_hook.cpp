/**
 * gamepad_hook.cpp — PortMaster-style gamepad function override for libyoyo.so on Android.
 *
 * Exact Function_Add symbol (verified via nm -D libyoyo.so):
 *   _Z12Function_AddPKcPFvR6RValueP9CInstanceS4_iPS1_Eib @ 0x003EAE5C
 *
 * YYCreateString: _Z14YYCreateStringP6RValuePKc @ 0x0040703C
 * YYGetInt32:     _Z10YYGetInt32PK6RValuei     @ 0x003EB6B4
 * YYGetReal:      _Z9YYGetRealPK6RValuei      @ 0x003EBB84
 * CreateDsMap:    _Z11CreateDsMapiz           @ 0x0057BADC
 * CreateAsynEventWithDSMap: _Z24CreateAsynEventWithDSMapii @ 0x004B7170
 */

#include <jni.h>
#include <dlfcn.h>
#include <link.h>
#include <android/log.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <atomic>
#include <string>
#include <map>

#define LOG_TAG "GamepadHook"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static uint64_t get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

// ── Offsets in THIS libyoyo.so (verified by nm -D libyoyo.so) ───────────────
#define FUNCTION_ADD_OFFSET                0x003EAE5CULL
#define YY_CREATE_STRING_OFFSET            0x0040703CULL
#define YY_GET_INT32_OFFSET                0x003EB6B4ULL
#define YY_GET_REAL_OFFSET                 0x003EBB84ULL
#define CREATE_DS_MAP_OFFSET               0x0057BADCULL
#define CREATE_ASYNC_EVENT_WITH_DS_MAP_OFF 0x004B7170ULL

// ── Types (matching libyoyo.so ABI) ─────────────────────────────────────────

typedef enum {
    VALUE_REAL      = 0,
    VALUE_STRING    = 1,
    VALUE_ARRAY     = 2,
    VALUE_PTR       = 3,
    VALUE_VEC3      = 4,
    VALUE_UNDEFINED = 5,
    VALUE_OBJECT    = 6,
    VALUE_INT32     = 7,
    VALUE_VEC4      = 8,
    VALUE_MATRIX    = 9,
    VALUE_INT64     = 10,
    VALUE_ACCESSOR  = 11,
    VALUE_JSNULL    = 12,
    VALUE_BOOL      = 13,
    VALUE_ITERATOR  = 14,
    VALUE_REF       = 15,
    VALUE_UNSET     = 0x0ffffff
} RValueKind;

struct CInstance;

struct RValue {
    union {
        int32_t   v32;
        int64_t   v64;
        double    val;
        void*     ptr;
        void*     str;
    } rvalue;
    uint32_t flags;
    RValueKind kind;
};

typedef void (*routine_t)(RValue& ret, CInstance* self, CInstance* other, int argc, RValue* args);
typedef void (*fct_add_t)(const char* name, routine_t func, int argc, bool reg);
typedef void (*yy_create_string_t)(RValue* res, const char* str);
typedef int  (*yy_get_int32_t)(const RValue* args, int idx);
typedef double (*yy_get_real_t)(const RValue* args, int idx);
typedef int  (*create_ds_map_t)(int num, ...);
typedef void (*create_async_event_with_ds_map_t)(int dsMap, int event_id);

static fct_add_t                         gh_Function_Add = nullptr;
static yy_create_string_t                gh_YYCreateString = nullptr;
static yy_get_int32_t                    gh_YYGetInt32 = nullptr;
static yy_get_real_t                     gh_YYGetReal = nullptr;
static create_ds_map_t                   gh_CreateDsMap = nullptr;
static create_async_event_with_ds_map_t  gh_CreateAsynEventWithDSMap = nullptr;

// ── Gamepad State ────────────────────────────────────────────────────────────

#define MAX_GAMEPADS 4
#define MAX_BUTTONS  16
#define MAX_AXES     6

struct GHGamepad {
    int                 is_connected;
    int                 needs_discovery_event;
    std::atomic<bool>   key_down[MAX_BUTTONS];
    std::atomic<bool>   trigger_down[MAX_BUTTONS];
    std::atomic<bool>   pending_press[MAX_BUTTONS];
    std::atomic<bool>   pending_release[MAX_BUTTONS];
    bool                step_pressed[MAX_BUTTONS];
    bool                step_held[MAX_BUTTONS];
    bool                step_released[MAX_BUTTONS];
    bool                prev_down[MAX_BUTTONS];
    uint64_t            last_press_time_ms[MAX_BUTTONS];
    double              axis[MAX_AXES];
    double              deadzone;
};

static GHGamepad gh_gamepads[MAX_GAMEPADS];
static bool      gh_initialized = false;
static uintptr_t gh_libyoyo_base = 0;
static std::atomic<bool>     gh_is_controller_connected(false);
static std::atomic<bool>     gh_touch_active(false);
static std::atomic<bool>     gh_touch_pending_press(false);
static std::atomic<bool>     gh_touch_pending_release(false);
static bool                  gh_touch_step_pressed = false;
static bool                  gh_touch_step_released = false;
static std::atomic<uint64_t> gh_last_in_game_time_ms(0);

static void gh_init_gamepads() {
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        gh_gamepads[i].is_connected = (i == 0) ? 1 : 0; // slot 0 connected by default
        gh_gamepads[i].needs_discovery_event = (i == 0) ? 1 : 0;
        gh_gamepads[i].deadzone = 0.12;
        for (int b = 0; b < MAX_BUTTONS; b++) {
            gh_gamepads[i].key_down[b].store(false);
            gh_gamepads[i].trigger_down[b].store(false);
            gh_gamepads[i].pending_press[b].store(false);
            gh_gamepads[i].pending_release[b].store(false);
            gh_gamepads[i].step_pressed[b] = false;
            gh_gamepads[i].step_held[b] = false;
            gh_gamepads[i].step_released[b] = false;
            gh_gamepads[i].prev_down[b] = false;
            gh_gamepads[i].last_press_time_ms[b] = 0;
        }
        memset(gh_gamepads[i].axis, 0, sizeof(gh_gamepads[i].axis));
    }
}

// ── Safe Value Extraction Helpers ───────────────────────────────────────────

static int get_rvalue_int(const RValue* args, int idx) {
    if (gh_YYGetInt32) {
        return gh_YYGetInt32(args, idx);
    }
    const RValue& r = args[idx];
    switch (r.kind) {
        case VALUE_INT32: return r.rvalue.v32;
        case VALUE_INT64: return (int)r.rvalue.v64;
        case VALUE_REAL:  return (int)r.rvalue.val;
        case VALUE_BOOL:  return r.rvalue.v32 ? 1 : 0;
        default:          return (int)r.rvalue.val;
    }
}

static double get_rvalue_double(const RValue* args, int idx) {
    if (gh_YYGetReal) {
        return gh_YYGetReal(args, idx);
    }
    const RValue& r = args[idx];
    switch (r.kind) {
        case VALUE_REAL:  return r.rvalue.val;
        case VALUE_INT32: return (double)r.rvalue.v32;
        case VALUE_INT64: return (double)r.rvalue.v64;
        default:          return r.rvalue.val;
    }
}

static int translate_button(const RValue* args, int idx) {
    int v = get_rvalue_int(args, idx);
    if (v >= 32769 && v < 32769 + 16) v -= 32769;
    return v;
}

static int translate_axis(const RValue* args, int idx) {
    int v = get_rvalue_int(args, idx);
    if (v >= 32785 && v < 32785 + 4) v -= 32785;
    return v;
}

static int ctrl_id(const RValue* args, int idx) {
    int id = get_rvalue_int(args, idx);
    if (id < 0 || id >= MAX_GAMEPADS) return 0; // Default to slot 0 for negative/undefined GML indices!
    return id;
}

#define IS_CTRL(id)  ((id) >= 0 && (id) < MAX_GAMEPADS)
#define IS_BTN(btn)  ((btn) >= 0 && (btn) < MAX_BUTTONS)
#define IS_AXIS(ax)  ((ax) >= 0 && (ax) < MAX_AXES)

// ── GML Gamepad Built-in Replacements ────────────────────────────────────────

static void gh_gamepad_is_supported(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL; ret.rvalue.val = 1.0;
}

static void gh_gamepad_get_device_count(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL; ret.rvalue.val = (double)MAX_GAMEPADS;
}

static void gh_gamepad_is_connected(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    int id = ctrl_id(args, 0);
    ret.rvalue.val = (IS_CTRL(id) && gh_gamepads[id].is_connected) ? 1.0 : 0.0;
}

static void gh_gamepad_get_description(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    static const char kName[] = "XInput STANDARD GAMEPAD";
    if (gh_YYCreateString) {
        gh_YYCreateString(&ret, kName);
    } else {
        ret.kind = VALUE_STRING;
        ret.rvalue.str = (void*)kName;
        ret.flags = 0;
    }
}

static void gh_gamepad_get_guid(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    static const char kGuid[] = "030000005e0400008e02000010010000";
    if (gh_YYCreateString) {
        gh_YYCreateString(&ret, kGuid);
    } else {
        ret.kind = VALUE_STRING;
        ret.rvalue.str = (void*)kGuid;
        ret.flags = 0;
    }
}

static void gh_gamepad_get_mapping(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    static const char kMapping[] = "030000005e0400008e02000014010000,Xbox 360 Controller (XInput STANDARD GAMEPAD),a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b8,leftshoulder:b4,leftstick:b9,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b10,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3";
    if (gh_YYCreateString) {
        gh_YYCreateString(&ret, kMapping);
    } else {
        ret.kind = VALUE_STRING;
        ret.rvalue.str = (void*)kMapping;
        ret.flags = 0;
    }
}

static void gh_gamepad_get_button_threshold(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL; ret.rvalue.val = 0.5;
}
static void gh_gamepad_set_button_threshold(RValue& ret, CInstance*, CInstance*, int, RValue*) {}

static void gh_gamepad_button_count(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL; ret.rvalue.val = 16.0;
}
static void gh_gamepad_axis_count(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL; ret.rvalue.val = 4.0;
}

static void gh_gamepad_get_axis_deadzone(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    int id = ctrl_id(args, 0);
    ret.rvalue.val = IS_CTRL(id) ? gh_gamepads[id].deadzone : 0.0;
}
static void gh_gamepad_set_axis_deadzone(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    int id = ctrl_id(args, 0);
    if (IS_CTRL(id)) gh_gamepads[id].deadzone = get_rvalue_double(args, 1);
}

static void gh_gamepad_axis_value(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    int id   = ctrl_id(args, 0);
    int axis = translate_axis(args, 1);
    if (!IS_CTRL(id) || !IS_AXIS(axis)) { ret.rvalue.val = 0.0; return; }
    double v = gh_gamepads[id].axis[axis];
    ret.rvalue.val = (fabs(v) < gh_gamepads[id].deadzone) ? 0.0 : v;
}

static void gh_gamepad_button_check(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    int id  = ctrl_id(args, 0);
    int btn = translate_button(args, 1);
    if (!IS_CTRL(id) || !IS_BTN(btn)) { ret.rvalue.val = 0.0; return; }
    ret.rvalue.val = gh_gamepads[id].step_held[btn] ? 1.0 : 0.0;
}

static void gh_gamepad_button_check_pressed(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    int id  = ctrl_id(args, 0);
    int btn = translate_button(args, 1);
    if (!IS_CTRL(id) || !IS_BTN(btn)) { ret.rvalue.val = 0.0; return; }
    ret.rvalue.val = gh_gamepads[id].step_pressed[btn] ? 1.0 : 0.0;
}

static void gh_gamepad_button_check_released(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    int id  = ctrl_id(args, 0);
    int btn = translate_button(args, 1);
    if (!IS_CTRL(id) || !IS_BTN(btn)) { ret.rvalue.val = 0.0; return; }
    ret.rvalue.val = gh_gamepads[id].step_released[btn] ? 1.0 : 0.0;
}

static void gh_gamepad_button_value(RValue& ret, CInstance* s, CInstance* o, int c, RValue* args) {
    gh_gamepad_button_check(ret, s, o, c, args);
}

static JavaVM*   g_jvm          = nullptr;
static jclass    g_bridge_class = nullptr;
static jmethodID g_vibr_mid     = nullptr;

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    g_jvm = vm;
    JNIEnv* env = nullptr;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_OK) {
        jclass localClass = env->FindClass("com/postvoid/port/GamepadBridge");
        if (localClass) {
            g_bridge_class = (jclass)env->NewGlobalRef(localClass);
            g_vibr_mid = env->GetStaticMethodID(g_bridge_class, "onVibration", "(IDD)V");
        }
    }
    return JNI_VERSION_1_6;
}

static void gh_gamepad_set_vibration(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL; ret.rvalue.val = 0.0;
    int id = ctrl_id(args, 0);
    double left = get_rvalue_double(args, 1);
    double right = get_rvalue_double(args, 2);
    if (g_jvm && g_bridge_class && g_vibr_mid) {
        JNIEnv* env = nullptr;
        bool attached = false;
        if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
            if (g_jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                attached = true;
            }
        }
        if (env) {
            env->CallStaticVoidMethod(g_bridge_class, g_vibr_mid, (jint)id, (jdouble)left, (jdouble)right);
        }
        if (attached) {
            g_jvm->DetachCurrentThread();
        }
    }
}

static void gh_gamepad_set_colour(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL; ret.rvalue.val = 0.0;
}

#define IO_CURRENT_KEY_OFFSET    0x00927598ULL
#define IO_KEY_DOWN_OFFSET       0x0092759CULL
#define IO_KEY_PRESSED_OFFSET    0x0092779CULL
#define IO_KEY_RELEASED_OFFSET   0x0092769CULL
#define IO_CURRENT_BUTTON_OFFSET 0x009278C8ULL
#define IO_BUTTON_DOWN_OFFSET    0x009278F0ULL
#define IO_BUTTON_RELEASED_OFF   0x00927922ULL
#define IO_BUTTON_PRESSED_OFFSET 0x00927954ULL

#define YYGML_KEYBOARD_CHECK_OFF 0x0044F0F8ULL
#define YYGML_KEYBOARD_DIR_OFF   0x0044F3B4ULL

#define CURRENT_ROOM_OFFSET      0x00940060ULL
static int32_t* gh_pCurrent_Room = nullptr;

static inline bool is_in_game_now() {
    int32_t room = -1;
    if (gh_pCurrent_Room) {
        room = *gh_pCurrent_Room;
    } else if (gh_libyoyo_base) {
        room = *(int32_t*)(gh_libyoyo_base + CURRENT_ROOM_OFFSET);
    }
    if (room >= 0) {
        // Active first-person 3D gameplay rooms: 5 (room_game), 6 (room_tutorial)
        // All other screens (0..4 menus/preload, 7 level_end stats, 8 death, 9 upgrade selection, 10 score menu, 11 world intro, 12 options, 13 cutscene, 14 credits) are UI/menus
        return (room == 5 || room == 6);
    }
    uint64_t now = get_time_ms();
    uint64_t last = gh_last_in_game_time_ms.load(std::memory_order_relaxed);
    return (now - last < 500ULL);
}

static void gh_clear_ghost_io() {
    if (!gh_libyoyo_base) return;
    int32_t* io_current_key = (int32_t*)(gh_libyoyo_base + IO_CURRENT_KEY_OFFSET);
    uint8_t* io_key_down    = (uint8_t*)(gh_libyoyo_base + IO_KEY_DOWN_OFFSET);
    uint8_t* io_key_press   = (uint8_t*)(gh_libyoyo_base + IO_KEY_PRESSED_OFFSET);
    uint8_t* io_key_rel     = (uint8_t*)(gh_libyoyo_base + IO_KEY_RELEASED_OFFSET);
    int32_t* io_curr_btn    = (int32_t*)(gh_libyoyo_base + IO_CURRENT_BUTTON_OFFSET);
    uint8_t* io_btn_down    = (uint8_t*)(gh_libyoyo_base + IO_BUTTON_DOWN_OFFSET);
    uint8_t* io_btn_rel     = (uint8_t*)(gh_libyoyo_base + IO_BUTTON_RELEASED_OFF);
    uint8_t* io_btn_press   = (uint8_t*)(gh_libyoyo_base + IO_BUTTON_PRESSED_OFFSET);

    if (io_current_key) *io_current_key = -1;
    if (io_key_down)    memset(io_key_down, 0, 256);
    if (io_key_press)   memset(io_key_press, 0, 256);
    if (io_key_rel)     memset(io_key_rel, 0, 256);
    if (io_curr_btn)    *io_curr_btn = 0;
    if (io_btn_down)    memset(io_btn_down, 0, 50);
    if (io_btn_press)   memset(io_btn_press, 0, 50);
    if (io_btn_rel)     memset(io_btn_rel, 0, 50);

    // If a physical controller is connected, keep all keyboard/mouse buffers strictly empty
    if (gh_is_controller_connected.load(std::memory_order_relaxed)) {
        return;
    }

    // Touch Mode: Populate synthetic mouse buttons into GameMaker internal IO buffers ONLY when in menus/UI
    // In gameplay (is_in_game_now), touch buttons act exclusively as native gamepad controls (RT/A/LT/X).
    // Injecting mouse clicks in gameplay causes obj_input_controller to flip global.input_activeDevice to 0 (Mouse/KB),
    // which drops the analog joystick and freezes player movement while shooting.
    if (io_btn_down && io_btn_press && !is_in_game_now()) {
        bool m_down  = gh_touch_active.load(std::memory_order_relaxed);
        bool m_press = gh_touch_step_pressed;

        if (m_down) {
            io_btn_down[1] = 1;
            if (io_curr_btn) *io_curr_btn = 1;
        }
        if (m_press) {
            io_btn_press[1] = 1;
            if (io_curr_btn) *io_curr_btn = 1;
        }
    }
}

static int gh_step_tick_count = 0;

static void gh_IO_Start_Step() {
    gh_step_tick_count++;

    // 1. Latch touch press/release step states first
    gh_touch_step_pressed  = gh_touch_pending_press.exchange(false, std::memory_order_acq_rel);
    gh_touch_step_released = gh_touch_pending_release.exchange(false, std::memory_order_acq_rel);

    // 2. Atomically consume pending edges and latch step-stable states for GML step
    for (int id = 0; id < MAX_GAMEPADS; id++) {
        if (!gh_gamepads[id].is_connected) continue;

        for (int b = 0; b < MAX_BUTTONS; b++) {
            bool k_down       = gh_gamepads[id].key_down[b].load(std::memory_order_relaxed);
            bool t_down       = gh_gamepads[id].trigger_down[b].load(std::memory_order_relaxed);
            bool cur_down     = k_down || t_down;
            bool was_pressed  = gh_gamepads[id].pending_press[b].exchange(false, std::memory_order_acq_rel);
            bool was_released = gh_gamepads[id].pending_release[b].exchange(false, std::memory_order_acq_rel);

            gh_gamepads[id].step_pressed[b]  = was_pressed || (cur_down && !gh_gamepads[id].prev_down[b]);
            gh_gamepads[id].step_held[b]     = cur_down || was_pressed;
            gh_gamepads[id].step_released[b] = was_released || (!cur_down && gh_gamepads[id].prev_down[b]);

            gh_gamepads[id].prev_down[b] = cur_down;
        }
    }

    // 3. Clear and sync GameMaker internal IO buffers
    gh_clear_ghost_io();

    // 4. Dispatch Async System Event 75 ("gamepad discovered") during startup (first 240 GML steps / ~4s)
    // Ensures obj_input_controller in both room 0 and room 1 fully registers slot 0 in global.Input_Device_Info
    if (gh_CreateDsMap && gh_CreateAsynEventWithDSMap) {
        if (gh_step_tick_count <= 240) {
            int dsMap = gh_CreateDsMap(2, "event_type", 0.0, "gamepad discovered", "pad_index", 0.0, (char*)NULL);
            gh_CreateAsynEventWithDSMap(dsMap, 75);
        }
    }

    // Pulse button 0 (A) on step 30 so scr_input_check_pressed_any(true) locks into Gamepad mode (1)
    if (gh_step_tick_count == 30) {
        gh_gamepads[0].step_pressed[0] = true;
    }
}

extern "C" void gh_tick_gamepad_states() {
    // Handled synchronously in gh_IO_Start_Step
}


// ── Install Hooks into libyoyo.so ───────────────────────────────────────────

static int find_libyoyo_callback(struct dl_phdr_info* info, size_t, void*) {
    if (info->dlpi_name && strstr(info->dlpi_name, "libyoyo.so")) {
        gh_libyoyo_base = (uintptr_t)info->dlpi_addr;
        LOGI("dl_iterate_phdr: libyoyo.so base=0x%lx path=%s",
             (unsigned long)gh_libyoyo_base, info->dlpi_name);
        return 1;
    }
    return 0;
}

#define YOYO_GET_PLATFORM_DO_WORK_OFF  0x0047E044
#define YOYO_F_GET_PLATFORM_OFF        0x0047E04C
#define YOYO_GET_PLATFORM_GML_OFF      0x004807C0
#define IO_START_STEP_OFFSET           0x004B4744ULL

static double gh_force_platform() {
    return 0.0; // os_windows
}

static void gh_force_platform_gml(void*, int, RValue* args) {
    if (args) {
        args[0].kind = VALUE_REAL;
        args[0].rvalue.val = 0.0;
    }
}

static void gh_F_YoYo_GetPlatform(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL;
    ret.rvalue.val = 0.0;
}

static bool is_key_down_synthetic(int k) {
    if (gh_is_controller_connected.load(std::memory_order_relaxed)) {
        return false;
    }
    if (k == 1) { // vk_anykey: Return false so GameMaker does not switch out of Gamepad mode
        return false;
    }
    if (k == 32) return gh_gamepads[0].step_held[0]; // Space / Jump -> A (btn 0)
    if (k == 17 || k == 160 || k == 16) return gh_gamepads[0].step_held[6]; // Ctrl/Shift / Slide -> L2 (btn 6)
    if (k == 82) return gh_gamepads[0].step_held[2]; // 'R' / Reload -> X (btn 2)
    if (k == 27) return gh_gamepads[0].step_held[9] || gh_gamepads[0].step_held[1]; // Escape / Pause -> Start / B
    if (k == 13) return gh_gamepads[0].step_held[0] || gh_gamepads[0].step_held[9]; // Enter -> A / Start
    if (k == 87 || k == 38) return (gh_gamepads[0].axis[1] < -0.20 || gh_gamepads[0].step_held[12]); // W / Up
    if (k == 83 || k == 40) return (gh_gamepads[0].axis[1] > 0.20 || gh_gamepads[0].step_held[13]); // S / Down
    if (k == 65 || k == 37) return (gh_gamepads[0].axis[0] < -0.20 || gh_gamepads[0].step_held[14]); // A / Left
    if (k == 68 || k == 39) return (gh_gamepads[0].axis[0] > 0.20 || gh_gamepads[0].step_held[15]); // D / Right
    return false;
}

static bool is_key_pressed_synthetic(int k) {
    if (gh_is_controller_connected.load(std::memory_order_relaxed)) {
        return false;
    }
    if (k == 1) { // vk_anykey
        return false;
    }
    if (k == 32) return gh_gamepads[0].step_pressed[0]; // Space / Jump -> A (btn 0)
    if (k == 17 || k == 160 || k == 16) return gh_gamepads[0].step_pressed[6]; // Ctrl/Shift / Slide -> L2 (btn 6)
    if (k == 82) return gh_gamepads[0].step_pressed[2]; // 'R' / Reload -> X (btn 2)
    if (k == 27) return gh_gamepads[0].step_pressed[9] || gh_gamepads[0].step_pressed[1]; // Escape / Pause -> Start / B
    if (k == 13) return gh_gamepads[0].step_pressed[0] || gh_gamepads[0].step_pressed[9]; // Enter -> A / Start
    if (k == 87 || k == 38) return gh_gamepads[0].step_pressed[12];
    if (k == 83 || k == 40) return gh_gamepads[0].step_pressed[13];
    if (k == 65 || k == 37) return gh_gamepads[0].step_pressed[14];
    if (k == 68 || k == 39) return gh_gamepads[0].step_pressed[15];
    return false;
}

static bool is_key_released_synthetic(int k) {
    if (gh_is_controller_connected.load(std::memory_order_relaxed)) {
        return false;
    }
    if (k == 32) return gh_gamepads[0].step_released[0];
    if (k == 17 || k == 160 || k == 16) return gh_gamepads[0].step_released[6];
    if (k == 82) return gh_gamepads[0].step_released[2];
    if (k == 27) return gh_gamepads[0].step_released[9] || gh_gamepads[0].step_released[1];
    return false;
}

static bool is_mouse_button_down_synthetic(int btn) {
    if (gh_is_controller_connected.load(std::memory_order_relaxed)) {
        return false;
    }
    if (is_in_game_now()) {
        // In gameplay: Firing is handled natively via gamepad_button_check (RT / btn 7).
        // Reporting mouse down here tricks obj_input_controller into switching to PC Mouse & Keyboard mode,
        // which disables gamepad analog movement and freezes the player in place.
        return false;
    } else {
        // In menus / UI / stats / victory / death:
        if (btn == 1 || btn == -1) {
            return gh_touch_active.load(std::memory_order_relaxed);
        }
        return false;
    }
}

static bool is_mouse_button_pressed_synthetic(int btn) {
    if (gh_is_controller_connected.load(std::memory_order_relaxed)) {
        return false;
    }
    if (is_in_game_now()) {
        // In gameplay: Handled natively via gamepad_button_check_pressed (RT / btn 7).
        return false;
    } else {
        // In menus / UI: Tapping screen confirms menu selection
        if (btn == 1 || btn == -1) {
            return gh_touch_step_pressed;
        }
        return false;
    }
}

static bool is_mouse_button_released_synthetic(int btn) {
    if (gh_is_controller_connected.load(std::memory_order_relaxed)) {
        return false;
    }
    if (is_in_game_now()) {
        return false;
    } else {
        if (btn == 1 || btn == -1) {
            return gh_touch_step_released;
        }
        return false;
    }
}

static bool gh_YYGML_keyboard_check(int k) {
    return is_key_down_synthetic(k);
}

static bool gh_YYGML_keyboard_check_direct(int k) {
    return is_key_down_synthetic(k);
}

static void hook_arm64_function(uintptr_t target_addr, void* hook_func) {
    long page_size = sysconf(_SC_PAGESIZE);
    uintptr_t page_start = target_addr & ~(page_size - 1);
    if (mprotect((void*)page_start, page_size * 2, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        LOGE("mprotect failed for 0x%lx", (unsigned long)target_addr);
        return;
    }

    uint32_t* code = (uint32_t*)target_addr;
    code[0] = 0x58000050; // ldr x16, #8 (PC + 8)
    code[1] = 0xd61f0200; // br x16
    *((uint64_t*)&code[2]) = (uint64_t)hook_func;

    __builtin___clear_cache((char*)target_addr, (char*)target_addr + 16);
    LOGI("Hooked arm64 function at 0x%lx -> %p", (unsigned long)target_addr, hook_func);
}

static void gh_keyboard_check(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    int k = args ? get_rvalue_int(args, 0) : 1;
    ret.rvalue.val = is_key_down_synthetic(k) ? 1.0 : 0.0;
}

static void gh_keyboard_check_pressed(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    int k = args ? get_rvalue_int(args, 0) : 1;
    ret.rvalue.val = is_key_pressed_synthetic(k) ? 1.0 : 0.0;
}

static void gh_keyboard_check_released(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    int k = args ? get_rvalue_int(args, 0) : 1;
    ret.rvalue.val = is_key_released_synthetic(k) ? 1.0 : 0.0;
}

static void gh_mouse_check_button(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    int b = args ? get_rvalue_int(args, 0) : 1;
    ret.rvalue.val = is_mouse_button_down_synthetic(b) ? 1.0 : 0.0;
}

static void gh_mouse_check_button_pressed(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    int b = args ? get_rvalue_int(args, 0) : 1;
    ret.rvalue.val = is_mouse_button_pressed_synthetic(b) ? 1.0 : 0.0;
}

static void gh_mouse_check_button_released(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    int b = args ? get_rvalue_int(args, 0) : 1;
    ret.rvalue.val = is_mouse_button_released_synthetic(b) ? 1.0 : 0.0;
}

static std::atomic<double> gh_surface_width(2376.0);
static std::atomic<double> gh_surface_height(1080.0);
static std::atomic<double> gh_mouse_x(1188.0);
static std::atomic<double> gh_mouse_y(540.0);
static std::atomic<double> gh_touch_dx(0.0);
static std::atomic<double> gh_touch_dy(0.0);

static void gh_display_get_width(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL;
    ret.rvalue.val = gh_surface_width.load(std::memory_order_relaxed);
}

static void gh_display_get_height(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL;
    ret.rvalue.val = gh_surface_height.load(std::memory_order_relaxed);
}

static void gh_display_mouse_get_x(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL;
    if (is_in_game_now()) {
        gh_last_in_game_time_ms.store(get_time_ms(), std::memory_order_relaxed);
        double center = floor(gh_surface_width.load(std::memory_order_relaxed) * 0.5);
        if (gh_is_controller_connected.load(std::memory_order_relaxed)) {
            ret.rvalue.val = center;
            return;
        }
        double t_dx = gh_touch_dx.exchange(0.0, std::memory_order_acq_rel);
        ret.rvalue.val = center + t_dx;
    } else {
        ret.rvalue.val = gh_mouse_x.load(std::memory_order_relaxed);
    }
}

static void gh_display_mouse_get_y(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL;
    if (is_in_game_now()) {
        gh_last_in_game_time_ms.store(get_time_ms(), std::memory_order_relaxed);
        double center = floor(gh_surface_height.load(std::memory_order_relaxed) * 0.5);
        if (gh_is_controller_connected.load(std::memory_order_relaxed)) {
            ret.rvalue.val = center;
            return;
        }
        double t_dy = gh_touch_dy.exchange(0.0, std::memory_order_acq_rel);
        ret.rvalue.val = center + t_dy;
    } else {
        ret.rvalue.val = gh_mouse_y.load(std::memory_order_relaxed);
    }
}

static void gh_display_mouse_set(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    ret.rvalue.val = 0.0;
    if (args) {
        gh_mouse_x.store(get_rvalue_double(args, 0), std::memory_order_relaxed);
        gh_mouse_y.store(get_rvalue_double(args, 1), std::memory_order_relaxed);
    }
}

static void gh_window_mouse_set(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    ret.rvalue.val = 0.0;
    if (args) {
        gh_mouse_x.store(get_rvalue_double(args, 0), std::memory_order_relaxed);
        gh_mouse_y.store(get_rvalue_double(args, 1), std::memory_order_relaxed);
    }
}

static void gh_mouse_check_button_common(RValue& ret, CInstance*, CInstance*, int, int btn) {
    ret.kind = VALUE_REAL;
    ret.rvalue.val = is_mouse_button_down_synthetic(btn) ? 1.0 : 0.0;
}

static void gh_mouse_check_button_pressed_common(RValue& ret, CInstance*, CInstance*, int, int btn) {
    ret.kind = VALUE_REAL;
    ret.rvalue.val = is_mouse_button_pressed_synthetic(btn) ? 1.0 : 0.0;
}

static void gh_mouse_check_button_released_common(RValue& ret, CInstance*, CInstance*, int, int btn) {
    ret.kind = VALUE_REAL;
    ret.rvalue.val = is_mouse_button_released_synthetic(btn) ? 1.0 : 0.0;
}

#define F_GAMEPAD_SUPPORTED_OFF             0x0047AAB4ULL
#define F_GAMEPAD_GET_DEVICE_COUNT_OFF      0x0047AAECULL
#define F_GAMEPAD_IS_CONNECTED_OFF          0x0047AB0CULL
#define F_GAMEPAD_GET_DESCRIPTION_OFF       0x0047AB7CULL
#define F_GAMEPAD_GET_BUTTON_THRESHOLD_OFF  0x0047ABECULL
#define F_GAMEPAD_SET_BUTTON_THRESHOLD_OFF  0x0047AC50ULL
#define F_GAMEPAD_GET_AXIS_DEADZONE_OFF     0x0047ACE8ULL
#define F_GAMEPAD_SET_AXIS_DEADZONE_OFF     0x0047AD4CULL
#define F_GAMEPAD_BUTTON_COUNT_OFF          0x0047ADE4ULL
#define F_GAMEPAD_BUTTON_CHECK_OFF          0x0047AE48ULL
#define F_GAMEPAD_BUTTON_CHECK_PRESSED_OFF  0x0047AEECULL
#define F_GAMEPAD_BUTTON_CHECK_RELEASED_OFF 0x0047AF90ULL
#define F_GAMEPAD_BUTTON_VALUE_OFF          0x0047B034ULL
#define F_GAMEPAD_AXIS_COUNT_OFF            0x0047B0D4ULL
#define F_GAMEPAD_AXIS_VALUE_OFF            0x0047B450ULL
#define F_GAMEPAD_GET_MAPPING_OFF           0x0047B644ULL
#define F_GAMEPAD_GET_GUID_OFF              0x0047B6D8ULL
#define F_GAMEPAD_SET_VIBRATION_OFF         0x0047B750ULL
#define F_GAMEPAD_SET_COLOUR_OFF            0x0047B19CULL

#define F_KEYBOARD_CHECK_OFF                0x0044F044ULL
#define F_CHECK_MOUSE_BUTTON_OFF            0x00486260ULL
#define F_CHECK_MOUSE_BUTTON_PRESSED_OFF    0x004862ACULL
#define F_CHECK_MOUSE_BUTTON_RELEASED_OFF   0x004862F8ULL
#define F_CHECK_MOUSE_BUTTON_COMMON_OFF     0x0044F440ULL
#define F_CHECK_MOUSE_BUTTON_PRESSED_COM_OFF 0x0044F580ULL
#define F_CHECK_MOUSE_BUTTON_REL_COM_OFF    0x0044F6C0ULL
#define F_DISPLAY_GET_WIDTH_OFF             0x00442198ULL
#define F_DISPLAY_GET_HEIGHT_OFF            0x004421C4ULL
#define F_DISPLAY_MOUSE_GET_X_OFF           0x0044228CULL
#define F_DISPLAY_MOUSE_GET_Y_OFF           0x004422B8ULL
#define F_DISPLAY_MOUSE_SET_OFF             0x004422E4ULL
#define F_WINDOW_GET_WIDTH_OFF              0x00442838ULL
#define F_WINDOW_GET_HEIGHT_OFF             0x00442864ULL
#define F_WINDOW_MOUSE_GET_X_OFF            0x004429ECULL
#define F_WINDOW_MOUSE_GET_Y_OFF            0x00442A18ULL
#define F_WINDOW_MOUSE_SET_OFF              0x00442A44ULL

static std::map<std::string, int64_t> gh_steam_stats_int;
static std::map<std::string, double>  gh_steam_stats_float;

static void gh_steam_get_stat_int(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    if (args && args[0].kind == VALUE_STRING && args[0].rvalue.str) {
        const char* name = (const char*)args[0].rvalue.str;
        auto it = gh_steam_stats_int.find(name);
        ret.rvalue.val = (it != gh_steam_stats_int.end()) ? (double)it->second : 0.0;
    } else {
        ret.rvalue.val = 0.0;
    }
}

static void gh_steam_set_stat_int(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    ret.rvalue.val = 1.0;
    if (args && args[0].kind == VALUE_STRING && args[0].rvalue.str) {
        const char* name = (const char*)args[0].rvalue.str;
        int64_t val = (int64_t)get_rvalue_double(args, 1);
        gh_steam_stats_int[name] = val;
    }
}

static void gh_steam_get_stat_float(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    if (args && args[0].kind == VALUE_STRING && args[0].rvalue.str) {
        const char* name = (const char*)args[0].rvalue.str;
        auto it = gh_steam_stats_float.find(name);
        ret.rvalue.val = (it != gh_steam_stats_float.end()) ? it->second : 0.0;
    } else {
        ret.rvalue.val = 0.0;
    }
}

static void gh_steam_set_stat_float(RValue& ret, CInstance*, CInstance*, int, RValue* args) {
    ret.kind = VALUE_REAL;
    ret.rvalue.val = 1.0;
    if (args && args[0].kind == VALUE_STRING && args[0].rvalue.str) {
        const char* name = (const char*)args[0].rvalue.str;
        double val = get_rvalue_double(args, 1);
        gh_steam_stats_float[name] = val;
    }
}

static void gh_steam_set_achievement(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL; ret.rvalue.val = 1.0;
}

static void gh_steam_get_achievement(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL; ret.rvalue.val = 0.0;
}

static void gh_steam_clear_achievement(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL; ret.rvalue.val = 1.0;
}

static void gh_steam_stats_ready(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL; ret.rvalue.val = 1.0;
}

static void gh_steam_initialised(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL; ret.rvalue.val = 1.0;
}

static void gh_steam_get_user_steam_id(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    static const char kId[] = "76561198000000000";
    if (gh_YYCreateString) gh_YYCreateString(&ret, kId);
    else { ret.kind = VALUE_STRING; ret.rvalue.str = (void*)kId; ret.flags = 0; }
}

static void gh_steam_get_persona_name(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    static const char kName[] = "Player";
    if (gh_YYCreateString) gh_YYCreateString(&ret, kName);
    else { ret.kind = VALUE_STRING; ret.rvalue.str = (void*)kName; ret.flags = 0; }
}

static void gh_steam_dummy_zero(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL; ret.rvalue.val = 0.0;
}

static void gh_steam_dummy_neg_one(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL; ret.rvalue.val = -1.0;
}

extern "C" void gh_install_gamepad_hooks() {
    gh_libyoyo_base = 0;
    dl_iterate_phdr(find_libyoyo_callback, nullptr);

    if (gh_libyoyo_base == 0) {
        LOGE("libyoyo.so not found via dl_iterate_phdr — hooks NOT installed");
        return;
    }

    gh_Function_Add             = (fct_add_t)(gh_libyoyo_base + FUNCTION_ADD_OFFSET);
    gh_YYCreateString           = (yy_create_string_t)(gh_libyoyo_base + YY_CREATE_STRING_OFFSET);
    gh_YYGetInt32               = (yy_get_int32_t)(gh_libyoyo_base + YY_GET_INT32_OFFSET);
    gh_YYGetReal                = (yy_get_real_t)(gh_libyoyo_base + YY_GET_REAL_OFFSET);
    gh_CreateDsMap              = (create_ds_map_t)(gh_libyoyo_base + CREATE_DS_MAP_OFFSET);
    gh_CreateAsynEventWithDSMap = (create_async_event_with_ds_map_t)(gh_libyoyo_base + CREATE_ASYNC_EVENT_WITH_DS_MAP_OFF);

    LOGI("Symbols resolved: Function_Add=%p, YYCreateString=%p, YYGetInt32=%p, CreateDsMap=%p, CreateAsyncEvent=%p",
         gh_Function_Add, gh_YYCreateString, gh_YYGetInt32, gh_CreateDsMap, gh_CreateAsynEventWithDSMap);

    gh_init_gamepads();

    // 1. Direct ARM64 inline hooks into libyoyo.so built-in gamepad functions (called by GML bytecode directly):
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_SUPPORTED_OFF,             (void*)gh_gamepad_is_supported);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_GET_DEVICE_COUNT_OFF,      (void*)gh_gamepad_get_device_count);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_IS_CONNECTED_OFF,          (void*)gh_gamepad_is_connected);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_GET_DESCRIPTION_OFF,       (void*)gh_gamepad_get_description);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_GET_BUTTON_THRESHOLD_OFF,  (void*)gh_gamepad_get_button_threshold);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_SET_BUTTON_THRESHOLD_OFF,  (void*)gh_gamepad_set_button_threshold);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_GET_AXIS_DEADZONE_OFF,     (void*)gh_gamepad_get_axis_deadzone);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_SET_AXIS_DEADZONE_OFF,     (void*)gh_gamepad_set_axis_deadzone);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_BUTTON_COUNT_OFF,          (void*)gh_gamepad_button_count);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_BUTTON_CHECK_OFF,          (void*)gh_gamepad_button_check);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_BUTTON_CHECK_PRESSED_OFF,  (void*)gh_gamepad_button_check_pressed);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_BUTTON_CHECK_RELEASED_OFF, (void*)gh_gamepad_button_check_released);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_BUTTON_VALUE_OFF,          (void*)gh_gamepad_button_value);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_AXIS_COUNT_OFF,            (void*)gh_gamepad_axis_count);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_AXIS_VALUE_OFF,            (void*)gh_gamepad_axis_value);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_GET_MAPPING_OFF,           (void*)gh_gamepad_get_mapping);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_GET_GUID_OFF,              (void*)gh_gamepad_get_guid);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_SET_VIBRATION_OFF,         (void*)gh_gamepad_set_vibration);
    hook_arm64_function(gh_libyoyo_base + F_GAMEPAD_SET_COLOUR_OFF,            (void*)gh_gamepad_set_colour);

    // 2. Also register via Function_Add for dynamic script calls:
    gh_Function_Add("gamepad_is_supported",          gh_gamepad_is_supported,          0, true);
    gh_Function_Add("gamepad_get_device_count",      gh_gamepad_get_device_count,      0, true);
    gh_Function_Add("gamepad_is_connected",          gh_gamepad_is_connected,          1, true);
    gh_Function_Add("gamepad_get_description",       gh_gamepad_get_description,       1, true);
    gh_Function_Add("gamepad_get_guid",              gh_gamepad_get_guid,              1, true);
    gh_Function_Add("gamepad_get_mapping",           gh_gamepad_get_mapping,           1, true);
    gh_Function_Add("gamepad_get_button_threshold",  gh_gamepad_get_button_threshold,  1, true);
    gh_Function_Add("gamepad_set_button_threshold",  gh_gamepad_set_button_threshold,  2, true);
    gh_Function_Add("gamepad_get_axis_deadzone",     gh_gamepad_get_axis_deadzone,     1, true);
    gh_Function_Add("gamepad_set_axis_deadzone",     gh_gamepad_set_axis_deadzone,     2, true);
    gh_Function_Add("gamepad_button_count",          gh_gamepad_button_count,          1, true);
    gh_Function_Add("gamepad_button_check",          gh_gamepad_button_check,          2, true);
    gh_Function_Add("gamepad_button_check_pressed",  gh_gamepad_button_check_pressed,  2, true);
    gh_Function_Add("gamepad_button_check_released", gh_gamepad_button_check_released, 2, true);
    gh_Function_Add("gamepad_button_value",          gh_gamepad_button_value,          2, true);
    gh_Function_Add("gamepad_axis_count",            gh_gamepad_axis_count,            1, true);
    gh_Function_Add("gamepad_axis_value",            gh_gamepad_axis_value,            2, true);
    gh_Function_Add("gamepad_set_vibration",         gh_gamepad_set_vibration,         3, true);
    gh_Function_Add("gamepad_set_color",             gh_gamepad_set_colour,            2, true);
    gh_Function_Add("gamepad_set_colour",            gh_gamepad_set_colour,            2, true);

    // Overrides for keyboard and mouse to prevent latching into Mouse mode
    gh_Function_Add("keyboard_check",                gh_keyboard_check,                1, true);
    gh_Function_Add("keyboard_check_pressed",        gh_keyboard_check_pressed,        1, true);
    gh_Function_Add("keyboard_check_released",       gh_keyboard_check_released,       1, true);
    gh_Function_Add("keyboard_check_direct",         gh_keyboard_check,                1, true);
    gh_Function_Add("mouse_check_button",            gh_mouse_check_button,            1, true);
    gh_Function_Add("mouse_check_button_pressed",    gh_mouse_check_button_pressed,    1, true);
    gh_Function_Add("mouse_check_button_released",   gh_mouse_check_button_released,   1, true);

    // Steamworks API mocks so stats/achievements/death screens don't throw undefined errors
    gh_Function_Add("steam_get_stat_int",                    gh_steam_get_stat_int,            1, true);
    gh_Function_Add("steam_set_stat_int",                    gh_steam_set_stat_int,            2, true);
    gh_Function_Add("steam_get_stat_float",                  gh_steam_get_stat_float,          1, true);
    gh_Function_Add("steam_set_stat_float",                  gh_steam_set_stat_float,          2, true);
    gh_Function_Add("steam_set_achievement",                 gh_steam_set_achievement,         1, true);
    gh_Function_Add("steam_get_achievement",                 gh_steam_get_achievement,         1, true);
    gh_Function_Add("steam_clear_achievement",               gh_steam_clear_achievement,       1, true);
    gh_Function_Add("steam_stats_ready",                     gh_steam_stats_ready,             0, true);
    gh_Function_Add("steam_initialised",                     gh_steam_initialised,             0, true);
    gh_Function_Add("steam_is_initialised",                  gh_steam_initialised,             0, true);
    gh_Function_Add("steam_get_user_steam_id",               gh_steam_get_user_steam_id,       0, true);
    gh_Function_Add("steam_get_persona_name",                gh_steam_get_persona_name,        0, true);
    gh_Function_Add("steam_create_leaderboard",              gh_steam_dummy_zero,              3, true);
    gh_Function_Add("steam_upload_score",                    gh_steam_dummy_zero,              3, true);
    gh_Function_Add("steam_upload_score_ext",                gh_steam_dummy_zero,              4, true);
    gh_Function_Add("steam_upload_score_buffer",             gh_steam_dummy_zero,              3, true);
    gh_Function_Add("steam_upload_score_buffer_ext",         gh_steam_dummy_zero,              4, true);
    gh_Function_Add("steam_download_scores",                 gh_steam_dummy_zero,              3, true);
    gh_Function_Add("steam_download_scores_around_user",     gh_steam_dummy_zero,              3, true);
    gh_Function_Add("steam_download_friends_scores",         gh_steam_dummy_zero,              1, true);
    gh_Function_Add("steam_inventory_get_all_items",         gh_steam_dummy_neg_one,           0, true);
    gh_Function_Add("steam_inventory_result_get_unix_timestamp", gh_steam_dummy_zero,         1, true);
    gh_Function_Add("steam_update",                          gh_steam_dummy_zero,              0, true);

    // Force os_type to os_windows (0.0) matching PortMaster
    hook_arm64_function(gh_libyoyo_base + YOYO_GET_PLATFORM_DO_WORK_OFF, (void*)gh_force_platform);
    hook_arm64_function(gh_libyoyo_base + YOYO_F_GET_PLATFORM_OFF,       (void*)gh_F_YoYo_GetPlatform);
    hook_arm64_function(gh_libyoyo_base + YOYO_GET_PLATFORM_GML_OFF,     (void*)gh_force_platform_gml);

    // Hook IO_Start_Step to match PortMaster input pipeline
    hook_arm64_function(gh_libyoyo_base + IO_START_STEP_OFFSET,          (void*)gh_IO_Start_Step);

    // Hook internal YYGML keyboard checks so GML bytecode never sees phantom keys
    hook_arm64_function(gh_libyoyo_base + YYGML_KEYBOARD_CHECK_OFF,      (void*)gh_YYGML_keyboard_check);
    hook_arm64_function(gh_libyoyo_base + YYGML_KEYBOARD_DIR_OFF,        (void*)gh_YYGML_keyboard_check_direct);
    hook_arm64_function(gh_libyoyo_base + F_KEYBOARD_CHECK_OFF,          (void*)gh_keyboard_check);

    // Hook internal mouse check functions to prevent phantom mouse mode
    hook_arm64_function(gh_libyoyo_base + F_CHECK_MOUSE_BUTTON_OFF,             (void*)gh_mouse_check_button);
    hook_arm64_function(gh_libyoyo_base + F_CHECK_MOUSE_BUTTON_PRESSED_OFF,     (void*)gh_mouse_check_button_pressed);
    hook_arm64_function(gh_libyoyo_base + F_CHECK_MOUSE_BUTTON_RELEASED_OFF,    (void*)gh_mouse_check_button_released);
    hook_arm64_function(gh_libyoyo_base + F_CHECK_MOUSE_BUTTON_COMMON_OFF,      (void*)gh_mouse_check_button_common);
    hook_arm64_function(gh_libyoyo_base + F_CHECK_MOUSE_BUTTON_PRESSED_COM_OFF, (void*)gh_mouse_check_button_pressed_common);
    hook_arm64_function(gh_libyoyo_base + F_CHECK_MOUSE_BUTTON_REL_COM_OFF,     (void*)gh_mouse_check_button_released_common);
    hook_arm64_function(gh_libyoyo_base + F_DISPLAY_GET_WIDTH_OFF,              (void*)gh_display_get_width);
    hook_arm64_function(gh_libyoyo_base + F_DISPLAY_GET_HEIGHT_OFF,             (void*)gh_display_get_height);
    hook_arm64_function(gh_libyoyo_base + F_WINDOW_GET_WIDTH_OFF,               (void*)gh_display_get_width);
    hook_arm64_function(gh_libyoyo_base + F_WINDOW_GET_HEIGHT_OFF,              (void*)gh_display_get_height);
    hook_arm64_function(gh_libyoyo_base + F_DISPLAY_MOUSE_GET_X_OFF,            (void*)gh_display_mouse_get_x);
    hook_arm64_function(gh_libyoyo_base + F_DISPLAY_MOUSE_GET_Y_OFF,            (void*)gh_display_mouse_get_y);
    hook_arm64_function(gh_libyoyo_base + F_WINDOW_MOUSE_GET_X_OFF,             (void*)gh_display_mouse_get_x);
    hook_arm64_function(gh_libyoyo_base + F_WINDOW_MOUSE_GET_Y_OFF,             (void*)gh_display_mouse_get_y);
    hook_arm64_function(gh_libyoyo_base + F_DISPLAY_MOUSE_SET_OFF,              (void*)gh_display_mouse_set);
    hook_arm64_function(gh_libyoyo_base + F_WINDOW_MOUSE_SET_OFF,               (void*)gh_window_mouse_set);

    gh_Function_Add("display_get_width",             gh_display_get_width,             0, true);
    gh_Function_Add("display_get_height",            gh_display_get_height,            0, true);
    gh_Function_Add("window_get_width",              gh_display_get_width,             0, true);
    gh_Function_Add("window_get_height",             gh_display_get_height,            0, true);
    gh_Function_Add("display_mouse_get_x",           gh_display_mouse_get_x,           0, true);
    gh_Function_Add("display_mouse_get_y",           gh_display_mouse_get_y,           0, true);
    gh_Function_Add("window_mouse_get_x",            gh_display_mouse_get_x,           0, true);
    gh_Function_Add("window_mouse_get_y",            gh_display_mouse_get_y,           0, true);
    gh_Function_Add("display_mouse_set",             gh_display_mouse_set,             2, true);
    gh_Function_Add("window_mouse_set",              gh_window_mouse_set,              2, true);

    void* sym_room = dlsym(RTLD_DEFAULT, "Current_Room");
    if (sym_room) {
        gh_pCurrent_Room = (int32_t*)sym_room;
        LOGI("Resolved Current_Room via dlsym: %p", gh_pCurrent_Room);
    } else if (gh_libyoyo_base) {
        gh_pCurrent_Room = (int32_t*)(gh_libyoyo_base + CURRENT_ROOM_OFFSET);
        LOGI("Resolved Current_Room via offset: %p", gh_pCurrent_Room);
    }

    gh_initialized = true;
    LOGI("Gamepad hooks installed — 35 built-ins inline hooked, 55 GML functions replaced, os_windows forced, slot 0 connected");
}

// ── JNI: Java → C++ state push ───────────────────────────────────────────────

static int android_keycode_to_gm_btn(int keycode, int scancode) {
    // 1. Try Android KeyCode first
    switch (keycode) {
        case 96:  return 0;   // KEYCODE_BUTTON_A      → gp_face1 (32769)
        case 97:  return 1;   // KEYCODE_BUTTON_B      → gp_face2 (32770)
        case 99:  return 2;   // KEYCODE_BUTTON_X      → gp_face3 (32771)
        case 100: return 3;   // KEYCODE_BUTTON_Y      → gp_face4 (32772)
        case 102: return 4;   // KEYCODE_BUTTON_L1     → gp_shoulderl (32773)
        case 103: return 5;   // KEYCODE_BUTTON_R1     → gp_shoulderr (32774)
        case 104: return 6;   // KEYCODE_BUTTON_L2     → gp_shoulderlb (32775)
        case 105: return 7;   // KEYCODE_BUTTON_R2     → gp_shoulderrb (32776)
        case 109: return 8;   // KEYCODE_BUTTON_SELECT → gp_select (32777)
        case 108: return 9;   // KEYCODE_BUTTON_START  → gp_start (32778)
        case 106: return 10;  // KEYCODE_BUTTON_THUMBL → gp_stickl (32779)
        case 107: return 11;  // KEYCODE_BUTTON_THUMBR → gp_stickr (32780)
        case 19:  return 12;  // KEYCODE_DPAD_UP       → gp_padu (32781)
        case 20:  return 13;  // KEYCODE_DPAD_DOWN     → gp_padd (32782)
        case 21:  return 14;  // KEYCODE_DPAD_LEFT     → gp_padl (32783)
        case 22:  return 15;  // KEYCODE_DPAD_RIGHT    → gp_padr (32784)
    }

    // 2. Fallback to Linux evdev scancode (crucial for ColorOS/OnePlus NO-SECURE key events where keyCode=-1)
    switch (scancode) {
        case 304: return 0;   // BTN_SOUTH / BTN_A
        case 305: return 1;   // BTN_EAST / BTN_B
        case 307: return 2;   // BTN_NORTH / BTN_X
        case 308: return 3;   // BTN_WEST / BTN_Y
        case 310: return 4;   // BTN_TL
        case 311: return 5;   // BTN_TR
        case 312: return 6;   // BTN_TL2
        case 313: return 7;   // BTN_TR2
        case 314: return 8;   // BTN_SELECT
        case 315: return 9;   // BTN_START
        case 316: return 8;   // BTN_MODE
        case 317: return 10;  // BTN_THUMBL
        case 318: return 11;  // BTN_THUMBR
        case 103: return 12;  // KEY_UP
        case 108: return 13;  // KEY_DOWN
        case 105: return 14;  // KEY_LEFT
        case 106: return 15;  // KEY_RIGHT
        default:  return -1;
    }
}

static void button_down_internal(int padIdx, int btn) {
    if (!IS_CTRL(padIdx) || !IS_BTN(btn)) return;

    // If button or trigger is already considered down, ignore repeated down calls (prevents repeat spam while held)
    if (gh_gamepads[padIdx].key_down[btn].load(std::memory_order_relaxed) ||
        gh_gamepads[padIdx].trigger_down[btn].load(std::memory_order_relaxed)) {
        return;
    }

    uint64_t now_ms = get_time_ms();
    uint64_t min_interval = (btn == 7) ? 100ULL : 10ULL;
    if (now_ms - gh_gamepads[padIdx].last_press_time_ms[btn] < min_interval) {
        return; // Cooldown: must wait at least 100ms since last press before next press is registered
    }
    gh_gamepads[padIdx].last_press_time_ms[btn] = now_ms;
    gh_gamepads[padIdx].key_down[btn].store(true, std::memory_order_relaxed);
    gh_gamepads[padIdx].pending_press[btn].store(true, std::memory_order_release);
}

static void button_up_internal(int padIdx, int btn) {
    if (!IS_CTRL(padIdx) || !IS_BTN(btn)) return;
    gh_gamepads[padIdx].key_down[btn].store(false, std::memory_order_relaxed);
    if (!gh_gamepads[padIdx].trigger_down[btn].load(std::memory_order_relaxed)) {
        gh_gamepads[padIdx].pending_release[btn].store(true, std::memory_order_release);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_postvoid_port_GamepadBridge_nativeButtonDown(JNIEnv*, jclass,
                                                       jint padIdx, jint keycode, jint scancode) {
    int btn = android_keycode_to_gm_btn(keycode, scancode);
    if (btn >= 0) button_down_internal(padIdx, btn);
}

extern "C" JNIEXPORT void JNICALL
Java_com_postvoid_port_GamepadBridge_nativeButtonUp(JNIEnv*, jclass,
                                                     jint padIdx, jint keycode, jint scancode) {
    int btn = android_keycode_to_gm_btn(keycode, scancode);
    if (btn >= 0) button_up_internal(padIdx, btn);
}

extern "C" JNIEXPORT void JNICALL
Java_com_postvoid_port_GamepadBridge_nativeAxis(JNIEnv*, jclass,
                                                 jint padIdx, jint axisIdx, jfloat value) {
    if (!IS_CTRL(padIdx) || !IS_AXIS(axisIdx)) return;
    gh_gamepads[padIdx].axis[axisIdx] = (double)value;

    // Left Trigger (Slide/Melee - btn 6 / gp_shoulderlb)
    if (axisIdx == 4) {
        if (value > 0.20f) {
            if (!gh_gamepads[padIdx].trigger_down[6].load(std::memory_order_relaxed) &&
                !gh_gamepads[padIdx].key_down[6].load(std::memory_order_relaxed)) {
                gh_gamepads[padIdx].trigger_down[6].store(true, std::memory_order_relaxed);
                gh_gamepads[padIdx].pending_press[6].store(true, std::memory_order_release);
            }
        } else if (value < 0.08f) {
            if (gh_gamepads[padIdx].trigger_down[6].load(std::memory_order_relaxed)) {
                gh_gamepads[padIdx].trigger_down[6].store(false, std::memory_order_relaxed);
                if (!gh_gamepads[padIdx].key_down[6].load(std::memory_order_relaxed)) {
                    gh_gamepads[padIdx].pending_release[6].store(true, std::memory_order_release);
                }
            }
        }
    }
    // Right Trigger (Shoot - btn 7 / gp_shoulderrb) with 100ms minimum cooldown between presses
    if (axisIdx == 5) {
        if (value > 0.20f) {
            if (!gh_gamepads[padIdx].trigger_down[7].load(std::memory_order_relaxed) &&
                !gh_gamepads[padIdx].key_down[7].load(std::memory_order_relaxed)) {
                uint64_t now_ms = get_time_ms();
                if (now_ms - gh_gamepads[padIdx].last_press_time_ms[7] >= 100ULL) {
                    gh_gamepads[padIdx].last_press_time_ms[7] = now_ms;
                    gh_gamepads[padIdx].trigger_down[7].store(true, std::memory_order_relaxed);
                    gh_gamepads[padIdx].pending_press[7].store(true, std::memory_order_release);
                }
            }
        } else if (value < 0.08f) {
            if (gh_gamepads[padIdx].trigger_down[7].load(std::memory_order_relaxed)) {
                gh_gamepads[padIdx].trigger_down[7].store(false, std::memory_order_relaxed);
                if (!gh_gamepads[padIdx].key_down[7].load(std::memory_order_relaxed)) {
                    gh_gamepads[padIdx].pending_release[7].store(true, std::memory_order_release);
                }
            }
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_postvoid_port_GamepadBridge_nativeSetConnected(JNIEnv*, jclass,
                                                         jint padIdx, jboolean connected) {
    if (!IS_CTRL(padIdx)) return;
    gh_gamepads[padIdx].is_connected = connected ? 1 : 0;
    if (connected) {
        gh_gamepads[padIdx].needs_discovery_event = 1;
    } else {
        for (int b = 0; b < MAX_BUTTONS; b++) {
            gh_gamepads[padIdx].key_down[b].store(false);
            gh_gamepads[padIdx].trigger_down[b].store(false);
            gh_gamepads[padIdx].pending_press[b].store(false);
            gh_gamepads[padIdx].pending_release[b].store(false);
            gh_gamepads[padIdx].step_pressed[b] = false;
            gh_gamepads[padIdx].step_held[b] = false;
            gh_gamepads[padIdx].step_released[b] = false;
            gh_gamepads[padIdx].prev_down[b] = false;
            gh_gamepads[padIdx].last_press_time_ms[b] = 0;
        }
        memset(gh_gamepads[padIdx].axis, 0, sizeof(gh_gamepads[padIdx].axis));
    }
    LOGI("Gamepad slot %d: %s", padIdx, connected ? "CONNECTED" : "DISCONNECTED");
}

extern "C" JNIEXPORT void JNICALL
Java_com_postvoid_port_GamepadBridge_nativeInstallHooks(JNIEnv*, jclass) {
    gh_install_gamepad_hooks();
}

extern "C" JNIEXPORT void JNICALL
Java_com_postvoid_port_GamepadBridge_nativeTick(JNIEnv*, jclass) {
    gh_tick_gamepad_states();
}

extern "C" JNIEXPORT void JNICALL
Java_com_postvoid_port_GamepadBridge_nativeSetTouchActive(JNIEnv*, jclass, jboolean active) {
    bool is_act = active ? true : false;
    bool was_act = gh_touch_active.exchange(is_act, std::memory_order_acq_rel);
    if (is_act && !was_act) {
        gh_touch_pending_press.store(true, std::memory_order_release);
    } else if (!is_act && was_act) {
        gh_touch_pending_release.store(true, std::memory_order_release);
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_postvoid_port_GamepadBridge_nativeIsInGame(JNIEnv*, jclass) {
    return is_in_game_now() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_postvoid_port_GamepadBridge_nativeIsReady(JNIEnv*, jclass) {
    return gh_initialized ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_postvoid_port_GamepadBridge_nativeSetSurfaceSize(JNIEnv*, jclass, jint width, jint height) {
    gh_surface_width.store((double)width, std::memory_order_relaxed);
    gh_surface_height.store((double)height, std::memory_order_relaxed);
    double cx = floor((double)width * 0.5);
    double cy = floor((double)height * 0.5);
    gh_mouse_x.store(cx, std::memory_order_relaxed);
    gh_mouse_y.store(cy, std::memory_order_relaxed);
    LOGI("Native surface size set: %dx%d -> center (%.1f, %.1f)", width, height, cx, cy);
}

static inline void atomic_add_double(std::atomic<double>& target, double delta) {
    double current = target.load(std::memory_order_relaxed);
    while (!target.compare_exchange_weak(current, current + delta, std::memory_order_relaxed)) {}
}

extern "C" JNIEXPORT void JNICALL
Java_com_postvoid_port_GamepadBridge_nativeRelativeLook(JNIEnv*, jclass, jfloat dx, jfloat dy) {
    // Forward touch swipe displacement directly into atomic look registers
    atomic_add_double(gh_touch_dx, (double)dx);
    atomic_add_double(gh_touch_dy, (double)dy);
}

extern "C" JNIEXPORT void JNICALL
Java_com_postvoid_port_GamepadBridge_nativeSetControllerConnected(JNIEnv*, jclass, jboolean connected) {
    gh_is_controller_connected.store(connected ? true : false, std::memory_order_release);
    LOGI("Native controller connected flag set: %d", connected ? 1 : 0);
}

// ── Native xdelta3 On-The-Fly Patching for BYOD ─────────────────────────────
extern "C" int xd3_main_cmdline(int argc, char **argv);

extern "C" JNIEXPORT jint JNICALL
Java_com_postvoid_port_BYODManager_nativeApplyPatch(
    JNIEnv* env, jobject /* this */,
    jstring jSrcPath, jstring jPatchPath, jstring jOutPath) {
    if (!jSrcPath || !jPatchPath || !jOutPath) {
        LOGE("BYOD: Null argument passed to nativeApplyPatch");
        return -1;
    }
    const char* srcPath = env->GetStringUTFChars(jSrcPath, nullptr);
    const char* patchPath = env->GetStringUTFChars(jPatchPath, nullptr);
    const char* outPath = env->GetStringUTFChars(jOutPath, nullptr);

    LOGI("BYOD: Starting native xdelta3 patch: src=%s, patch=%s, out=%s", srcPath, patchPath, outPath);
    LOGI("BYOD: Access check: src R_OK=%d, patch R_OK=%d", access(srcPath, R_OK), access(patchPath, R_OK));

    char* argv[] = {
        (char*)"xdelta3", (char*)"-d", (char*)"-f",
        (char*)"-s", (char*)srcPath, (char*)patchPath, (char*)outPath, nullptr
    };
    int ret = xd3_main_cmdline(7, argv);

    LOGI("BYOD: xdelta3 completed with return code: %d", ret);

    env->ReleaseStringUTFChars(jSrcPath, srcPath);
    env->ReleaseStringUTFChars(jPatchPath, patchPath);
    env->ReleaseStringUTFChars(jOutPath, outPath);
    return ret;
}

