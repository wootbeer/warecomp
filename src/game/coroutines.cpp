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
// Here each coroutine body runs on a fiber of its own (Windows fibers,
// ucontext elsewhere), all on the game thread that started it, so the
// runtime's per-thread state stays right. The three functions above are
// replaced by hooks (wa.us.toml) that switch fibers and save/restore exactly
// the registers the game's setjmp does. Everything in N64 memory (the carved
// stack, the stack slots, the object's state fields) is written as the
// original code writes it.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

#include "recomp.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif defined(__ANDROID__)
// TODO(android): bionic has no makecontext/swapcontext; this needs a small
// context switch of its own (aarch64) before the match works there.
#error "Coroutine fibers are not implemented for Android yet (src/game/coroutines.cpp)"
#else
#include <ucontext.h>
#endif

extern "C" recomp_func_t* get_function(int32_t vram);

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
#else
        swapcontext(&co->co_uc, &co->caller_uc);
#endif
    }

    // Into the coroutine: from start, or to resume it (longjmp(obj + 0x88, 1)).
    void switch_to_coroutine(Coroutine* co) {
#if defined(_WIN32)
        co->caller_fiber = GetCurrentFiber();
        SwitchToFiber(co->fiber);
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
    co->stack = std::malloc(kFiberStackSize);
    getcontext(&co->co_uc);
    co->co_uc.uc_stack.ss_sp = co->stack;
    co->co_uc.uc_stack.ss_size = kFiberStackSize;
    co->co_uc.uc_link = nullptr;
    makecontext(&co->co_uc, uc_entry, 0);
    starting = co;
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
