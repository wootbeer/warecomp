// The match's coroutines (STATUS.md round 5).
//
// The match overlay runs some of its game logic as coroutines built on its own
// setjmp/longjmp (func_8001DA10 saves s0-s7, sp, fp and ra and returns 0;
// func_8001DA50 restores them and returns its second argument, or 1):
//
//   game_800AAD00  recurses until it has carved out the coroutine's stack
//                  (0x418 bytes a level), then starts it;
//   game_800AAC9C  start: setjmp(obj + 0x48) = where to come back to, then
//                  runs the body (a virtual method, vtable +0x50/+0x54) and,
//                  when it ends, longjmps back;
//   game_800AAC68  yield: setjmp(obj + 0x88) = where to resume, then
//                  longjmp(obj + 0x48, 1);
//   game_800AABB0  step: in state 1 (suspended), setjmp(obj + 0x48) and
//                  longjmp(obj + 0x88, 1) to resume it.
//
// On the N64 a longjmp is just a register reload. Recompiled, restoring the
// MIPS registers doesn't move execution on the host: the host carries on in
// the function that called longjmp, with another frame's registers (the first
// A press in a match crashed with a null `this` in game_8009E818).
//
// Here each coroutine body runs on a fiber of its own (Windows fibers; on
// Android, which has no ucontext, a small ARM64 stack switch below; ucontext
// elsewhere), all on the game thread that started it, so the
// runtime's per-thread state stays right. The three functions above are
// replaced by hooks (wa.us.toml) that switch fibers and save/restore exactly
// the registers the game's setjmp does. Everything in N64 memory (the carved
// stack, the stack slots, the object's state fields) is written as the
// original code writes it.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

#include "recomp.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif defined(__ANDROID__)
#if !defined(__aarch64__)
#error "The Android build is ARM64 only (android/app/build.gradle.kts)."
#endif
#define WA_ARM64_SWITCH 1
#else
#include <ucontext.h>
#endif

extern "C" recomp_func_t* get_function(int32_t vram);

#if defined(WA_ARM64_SWITCH)
// Android (bionic has no getcontext/makecontext/swapcontext). wa_arm64_switch(from, to) saves
// the callee-saved registers (x19-x30, d8-d15: AAPCS64) on the current stack, stores the stack
// pointer in *from, and resumes the context whose stack pointer is to, as saved the same way.
// A new context's stack starts as such a saved frame whose x30 (where `ret` goes) is
// wa_arm64_trampoline and whose x19 is the argument: it calls wa_arm64_coroutine_entry(x19).
extern "C" void wa_arm64_switch(void** from_sp, void* to_sp);
extern "C" void wa_arm64_trampoline();
extern "C" void wa_arm64_coroutine_entry(void* arg);
__asm__(
    ".text\n"
    ".p2align 4\n"
    ".globl wa_arm64_switch\n"
    ".type wa_arm64_switch, %function\n"
    "wa_arm64_switch:\n"
    "    sub sp, sp, #176\n"
    "    stp x19, x20, [sp, #0]\n"
    "    stp x21, x22, [sp, #16]\n"
    "    stp x23, x24, [sp, #32]\n"
    "    stp x25, x26, [sp, #48]\n"
    "    stp x27, x28, [sp, #64]\n"
    "    stp x29, x30, [sp, #80]\n"
    "    stp d8, d9, [sp, #96]\n"
    "    stp d10, d11, [sp, #112]\n"
    "    stp d12, d13, [sp, #128]\n"
    "    stp d14, d15, [sp, #144]\n"
    "    mov x9, sp\n"
    "    str x9, [x0]\n"
    "    mov sp, x1\n"
    "    ldp x19, x20, [sp, #0]\n"
    "    ldp x21, x22, [sp, #16]\n"
    "    ldp x23, x24, [sp, #32]\n"
    "    ldp x25, x26, [sp, #48]\n"
    "    ldp x27, x28, [sp, #64]\n"
    "    ldp x29, x30, [sp, #80]\n"
    "    ldp d8, d9, [sp, #96]\n"
    "    ldp d10, d11, [sp, #112]\n"
    "    ldp d12, d13, [sp, #128]\n"
    "    ldp d14, d15, [sp, #144]\n"
    "    add sp, sp, #176\n"
    "    ret\n"
    ".size wa_arm64_switch, .-wa_arm64_switch\n"
    ".p2align 4\n"
    ".globl wa_arm64_trampoline\n"
    ".type wa_arm64_trampoline, %function\n"
    "wa_arm64_trampoline:\n"
    "    mov x0, x19\n"
    "    bl wa_arm64_coroutine_entry\n"
    "    brk #0\n"
    ".size wa_arm64_trampoline, .-wa_arm64_trampoline\n"
);
#endif

namespace {
    // What the game's setjmp saves.
    struct SavedRegs {
        gpr s[8];
        gpr sp;
        gpr fp;
        gpr ra;
    };

    void save_regs(const recomp_context* ctx, SavedRegs& r) {
        r.s[0] = ctx->r16; r.s[1] = ctx->r17; r.s[2] = ctx->r18; r.s[3] = ctx->r19;
        r.s[4] = ctx->r20; r.s[5] = ctx->r21; r.s[6] = ctx->r22; r.s[7] = ctx->r23;
        r.sp = ctx->r29; r.fp = ctx->r30; r.ra = ctx->r31;
    }

    void load_regs(recomp_context* ctx, const SavedRegs& r) {
        ctx->r16 = r.s[0]; ctx->r17 = r.s[1]; ctx->r18 = r.s[2]; ctx->r19 = r.s[3];
        ctx->r20 = r.s[4]; ctx->r21 = r.s[5]; ctx->r22 = r.s[6]; ctx->r23 = r.s[7];
        ctx->r29 = r.sp; ctx->r30 = r.fp; ctx->r31 = r.ra;
    }

    // Writes a jmp_buf in N64 memory as the game's setjmp would (nothing is
    // known to read it, but it costs nothing to keep memory as on the N64).
    void write_jmp_buf(uint8_t* rdram, gpr buf, const SavedRegs& r) {
        for (int i = 0; i < 8; i++) {
            MEM_W(i * 4, buf) = (int32_t)r.s[i];
        }
        MEM_W(0x20, buf) = (int32_t)r.sp;
        MEM_W(0x24, buf) = (int32_t)r.fp;
        MEM_W(0x28, buf) = (int32_t)r.ra;
    }

    constexpr size_t kFiberStackSize = 1024 * 1024;

    struct Coroutine {
        // The side that started or resumed it (the game's obj + 0x48).
        SavedRegs caller_regs{};
        // The coroutine where it yielded (the game's obj + 0x88).
        SavedRegs co_regs{};
        bool finished = false;
        uint8_t* rdram = nullptr;
        recomp_context* ctx = nullptr;
        gpr obj = 0;
#if defined(_WIN32)
        LPVOID fiber = nullptr;
        LPVOID caller_fiber = nullptr;
#elif defined(WA_ARM64_SWITCH)
        void* co_sp = nullptr;
        void* caller_sp = nullptr;
        void* stack = nullptr;
#else
        ucontext_t co_uc{};
        ucontext_t caller_uc{};
        void* stack = nullptr;
#endif
    };

    // By object address. Only touched from the game thread that runs the match.
    std::unordered_map<uint32_t, Coroutine*> coroutines;

    void destroy(Coroutine* co) {
#if defined(_WIN32)
        if (co->fiber != nullptr) {
            DeleteFiber(co->fiber);
        }
#else
        std::free(co->stack);
#endif
        delete co;
    }

    // Back to whoever started or resumed the coroutine (the game's
    // longjmp(obj + 0x48, 1)).
    void switch_to_caller(Coroutine* co) {
#if defined(_WIN32)
        SwitchToFiber(co->caller_fiber);
#elif defined(WA_ARM64_SWITCH)
        wa_arm64_switch(&co->co_sp, co->caller_sp);
#else
        swapcontext(&co->co_uc, &co->caller_uc);
#endif
    }

    // Into the coroutine: from start, or to resume it (longjmp(obj + 0x88, 1)).
    void switch_to_coroutine(Coroutine* co) {
#if defined(_WIN32)
        co->caller_fiber = GetCurrentFiber();
        SwitchToFiber(co->fiber);
#elif defined(WA_ARM64_SWITCH)
        wa_arm64_switch(&co->caller_sp, co->co_sp);
#else
        swapcontext(&co->caller_uc, &co->co_uc);
#endif
    }

    // The body, as game_800AAC9C runs it once setjmp has returned 0.
    void run_body(Coroutine* co) {
        uint8_t* rdram = co->rdram;
        recomp_context* ctx = co->ctx;
        gpr obj = co->obj;

        // obj->state = 1 (running), obj->0xC8 = 1, then the body: the virtual
        // at vtable +0x54, `this` adjusted by the halfword at +0x50.
        gpr vtable = MEM_W(0x0, obj);
        MEM_W(0x38, obj) = 1;
        MEM_W(0xC8, obj) = 1;
        ctx->r4 = ADD32(obj, MEM_H(0x50, vtable));
        get_function((int32_t)MEM_W(0x54, vtable))(rdram, ctx);

        // The body returned: obj->0xC8 = 0, obj->state = 2 (finished), and
        // longjmp(obj + 0x48, 1) back to the start's caller for good.
        obj = co->obj;
        MEM_W(0xC8, obj) = 0;
        MEM_W(0x38, obj) = 2;
        co->finished = true;
        switch_to_caller(co);
        // Never resumed after finishing.
        std::fprintf(stderr, "[WA] finished coroutine resumed (object 0x%08X)\n", (uint32_t)obj);
        std::abort();
    }

#if defined(_WIN32)
    void WINAPI fiber_entry(LPVOID param) {
        run_body(static_cast<Coroutine*>(param));
    }

    void ensure_thread_is_fiber() {
        if (!IsThreadAFiber()) {
            ConvertThreadToFiber(nullptr);
        }
    }
#elif defined(WA_ARM64_SWITCH)
    // The saved frame wa_arm64_switch restores: x19-x28, x29, x30, d8-d15 (176 bytes).
    constexpr size_t kArm64FrameSize = 176;
#else
    Coroutine* starting = nullptr;

    void uc_entry() {
        run_body(starting);
    }
#endif

    // After switching back from the coroutine to the side that started or
    // resumed it: that side's registers, as its longjmp would restore them,
    // and the fiber freed if the body has finished.
    void back_in_caller(uint8_t* rdram, recomp_context* ctx, uint32_t key, Coroutine* co) {
        load_regs(ctx, co->caller_regs);
        if (co->finished) {
            coroutines.erase(key);
            destroy(co);
        }
        (void)rdram;
    }
}

#if defined(WA_ARM64_SWITCH)
extern "C" void wa_arm64_coroutine_entry(void* arg) {
    run_body(static_cast<Coroutine*>(arg));
}
#endif

// Replaces game_800AAC9C (start).
extern "C" void wa_coro_start(uint8_t* rdram, recomp_context* ctx) {
    gpr obj = ctx->r4;
    uint32_t key = (uint32_t)obj;

    // Prologue: sp -= 0x18; `this` in the caller's argument slot, ra saved.
    ctx->r29 = ADD32(ctx->r29, -0x18);
    MEM_W(0x18, ctx->r29) = (int32_t)obj;
    MEM_W(0x10, ctx->r29) = (int32_t)ctx->r31;

    // A coroutine object can be reused (or freed and its memory reused) once
    // its previous body is over or abandoned: start afresh.
    if (auto it = coroutines.find(key); it != coroutines.end()) {
        destroy(it->second);
        coroutines.erase(it);
    }

    Coroutine* co = new Coroutine{};
    co->rdram = rdram;
    co->ctx = ctx;
    co->obj = obj;
    // setjmp(obj + 0x48): here, with ra = the return point after the call.
    save_regs(ctx, co->caller_regs);
    co->caller_regs.ra = (gpr)(int32_t)0x800AACB0;
    write_jmp_buf(rdram, ADD32(obj, 0x48), co->caller_regs);
    coroutines[key] = co;

#if defined(_WIN32)
    ensure_thread_is_fiber();
    co->fiber = CreateFiber(kFiberStackSize, fiber_entry, co);
    if (co->fiber == nullptr) {
        std::fprintf(stderr, "[WA] CreateFiber failed (%lu)\n", GetLastError());
        std::abort();
    }
#else
#if defined(WA_ARM64_SWITCH)
    co->stack = std::malloc(kFiberStackSize);
    {
        uintptr_t top = ((uintptr_t)co->stack + kFiberStackSize) & ~(uintptr_t)15;
        uint64_t* frame = reinterpret_cast<uint64_t*>(top - kArm64FrameSize);
        std::memset(frame, 0, kArm64FrameSize);
        frame[0] = (uint64_t)(uintptr_t)co;                          // x19: the argument
        frame[11] = (uint64_t)(uintptr_t)&wa_arm64_trampoline;      // x30: where `ret` goes
        co->co_sp = frame;
    }
#else
    co->stack = std::malloc(kFiberStackSize);
    getcontext(&co->co_uc);
    co->co_uc.uc_stack.ss_sp = co->stack;
    co->co_uc.uc_stack.ss_size = kFiberStackSize;
    co->co_uc.uc_link = nullptr;
    makecontext(&co->co_uc, uc_entry, 0);
    starting = co;
#endif
#endif
    switch_to_coroutine(co);

    // Back from a yield or the end of the body: setjmp returned 1.
    back_in_caller(rdram, ctx, key, co);
    // Epilogue: return 1.
    ctx->r2 = 1;
    ctx->r31 = MEM_W(0x10, ctx->r29);
    ctx->r29 = ADD32(ctx->r29, 0x18);
}

// Replaces game_800AAC68 (yield), called from inside a coroutine body.
extern "C" void wa_coro_yield(uint8_t* rdram, recomp_context* ctx) {
    gpr obj = ctx->r4;
    auto it = coroutines.find((uint32_t)obj);
    if (it == coroutines.end()) {
        std::fprintf(stderr, "[WA] yield from an unknown coroutine (object 0x%08X)\n", (uint32_t)obj);
        std::abort();
    }
    Coroutine* co = it->second;

    // Prologue: sp -= 0x18; `this` in the argument slot, ra saved.
    ctx->r29 = ADD32(ctx->r29, -0x18);
    MEM_W(0x18, ctx->r29) = (int32_t)obj;
    MEM_W(0x10, ctx->r29) = (int32_t)ctx->r31;

    // setjmp(obj + 0x88), then longjmp(obj + 0x48, 1).
    save_regs(ctx, co->co_regs);
    co->co_regs.ra = (gpr)(int32_t)0x800AAC7C;
    write_jmp_buf(rdram, ADD32(obj, 0x88), co->co_regs);
    switch_to_caller(co);

    // Resumed (setjmp returned 1): this side's registers, then the epilogue.
    load_regs(ctx, co->co_regs);
    ctx->r2 = 1;
    ctx->r31 = MEM_W(0x10, ctx->r29);
    ctx->r29 = ADD32(ctx->r29, 0x18);
}

// Replaces game_800AABB0's state-1 path (0x800AAC18-0x800AAC3C): setjmp(obj +
// 0x48), obj->0xC8 = 1, longjmp(obj + 0x88, 1). The hook then continues at
// 0x800AAC40, where setjmp's nonzero return goes.
extern "C" void wa_coro_resume(uint8_t* rdram, recomp_context* ctx) {
    // a2 = `this`, from the function's own stack slot.
    gpr obj = MEM_W(0x20, ctx->r29);
    ctx->r6 = obj;
    uint32_t key = (uint32_t)obj;
    auto it = coroutines.find(key);
    if (it == coroutines.end()) {
        std::fprintf(stderr, "[WA] resume of an unknown coroutine (object 0x%08X)\n", key);
        std::abort();
    }
    Coroutine* co = it->second;

    save_regs(ctx, co->caller_regs);
    co->caller_regs.ra = (gpr)(int32_t)0x800AAC24;
    write_jmp_buf(rdram, ADD32(obj, 0x48), co->caller_regs);
    MEM_W(0xC8, obj) = 1;
    switch_to_coroutine(co);

    back_in_caller(rdram, ctx, key, co);
    ctx->r2 = 1;
}
