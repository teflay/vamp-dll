#pragma once

#include "OSTPlatform/include/Trap.h"
#include "Utils/SteamMetadata/PatternLoader.h"

#include <cstdint>
#include <type_traits>
#include <vector>

namespace VehCommon {

    constexpr uint64_t kMaxX64InsnLen = 15;

    inline bool IsAt(uint64_t rip, const void* target) {
        return rip == reinterpret_cast<uint64_t>(target);
    }

    inline bool IsPostInt3Step(uint64_t rip, const void* target) {
        auto base = reinterpret_cast<uint64_t>(target);
        return rip > base && rip <= base + kMaxX64InsnLen;
    }

    template<typename T>
    inline T GetArg(OSTPlatform::Trap::Context& ctx, int index) {
        static_assert(sizeof(T) <= sizeof(uint64_t),
                      "GetArg<T>: T must fit in a 64-bit register slot");
        uint64_t raw = ctx.Argument(index);
        if constexpr (std::is_pointer_v<T>) {
            return reinterpret_cast<T>(raw);
        } else {
            return static_cast<T>(raw);
        }
    }

    struct Int3Site {
        uint8_t*    target;
        uint8_t     originalByte;
        bool        persistent;
        void      (*onHit)(OSTPlatform::Trap::Context& ctx, const Int3Site& site);
        void*       callbackData;
        const char* label;
    };

    void Arm(Int3Site site);
    bool HasSites();
    bool OnBreakpoint(OSTPlatform::Trap::Context& ctx);
    bool OnSingleStep(OSTPlatform::Trap::Context& ctx);
    void DisarmAll();
    void RemoveHandler();

    inline void CaptureRcxTo(OSTPlatform::Trap::Context& ctx, const Int3Site& site) {
        *static_cast<void**>(site.callbackData) = GetArg<void*>(ctx, 1);
    }
}

#define CAPTURE_THIS_FUNC(name, ret, outVar, ...)          \
    using name##_t = ret(__fastcall*)(__VA_ARGS__);        \
    inline name##_t o##name = nullptr;                     \
    inline void* outVar = nullptr;                         \
    inline void** const _capture_out_##name = &outVar

#define ARM_INT3(module, name, persistent_, onHit_, callbackData_)        \
    do {                                                                  \
        if (auto* _p_ = PatternLoader::FindPattern(module, #name)) {       \
            auto* _t_ = static_cast<uint8_t*>(_p_);                       \
            VehCommon::Arm(VehCommon::Int3Site{                           \
                _t_, *_t_, persistent_, onHit_, callbackData_, #name,     \
            });                                                           \
        }                                                                 \
    } while (0)

#define ARM_INT3_C(name, persistent_, onHit_, callbackData_)              \
    ARM_INT3(client_hModule, name, persistent_, onHit_, callbackData_)

#define ARM_INT3_U(name, persistent_, onHit_, callbackData_)              \
    ARM_INT3(ui_hModule, name, persistent_, onHit_, callbackData_)

#define CAPTURE_READY(name) (*_capture_out_##name && o##name)

#define ARM_CAPTURE(module,name)                                          \
    do {                                                                  \
        if (auto* _p_ = PatternLoader::FindPattern(module, #name)) {       \
            o##name = reinterpret_cast<name##_t>(_p_);                    \
            auto* _t_ = static_cast<uint8_t*>(_p_);                       \
            VehCommon::Arm(VehCommon::Int3Site{                           \
                _t_, *_t_, /*persistent=*/false,                          \
                &VehCommon::CaptureRcxTo,                                 \
                static_cast<void*>(_capture_out_##name),                  \
                #name,                                                    \
            });                                                           \
        }                                                                 \
    } while (0)

#define ARM_CAPTURE_C(name) ARM_CAPTURE(client_hModule, name)
#define ARM_CAPTURE_U(name) ARM_CAPTURE(ui_hModule, name)