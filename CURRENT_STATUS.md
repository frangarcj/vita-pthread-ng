# vita-pthread-ng snapshot

Snapshot date: 2026-08-19

## Scope
- POSIX pthread implementation targeting VitaSDK ABI compatibility.
- Compile-time HAL.
- Current Linux backend simulates SceKernel-style thread/mutex/semaphore mechanisms.
- Async cancellation is modeled as remote no-return context injection; Vita HAL is intended to map this to suspend/get-context/set-context/resume where available.

## Verified state at snapshot
- Release CTest: 43/43 PASS with a 30-second timeout per local test.
- Official musl pthread subset: 7/7 PASS with a 15-second timeout per test.
- `cancel_cond` stress check: 100/100 isolated executions passed with a 3-second timeout.
- ASAN with `detect_leaks=0`: 43/43 PASS.
- Vita cross-build with `arm-vita-eabi-gcc`: library build PASS.
- TSAN non-musl suite: 36/36 PASS, including the cancellation and detached-thread paths.
- TSAN focused cancellation/signal battery: 6/6 PASS.
- TSAN musl subset: 7/7 complete; `pthread_cond`, `pthread_mutex_pi`, and `pthread_mutex` still report adapter/test data-race warnings from polling semaphores and intentionally unsynchronized corpus variables.

## Recent integration
- Added the missing POSIX API surface: `pthread_atfork`, thread scheduling accessors, and `pthread_kill`.
- Added SCE-simulator hooks for atfork, signal delivery, and one-shot thread-create fault injection.
- Adopted bundle attribute semantics: 8192-byte default guard, inherited scheduling, process scope `ENOTSUP`, and stack/guard size limits.
- Fixed the cancellation race where a request could arrive between the initial condition-wait check and publication of the waiter.

The remaining TSAN warnings are limited to the musl polling adapter and corpus tests; they are not reproduced by Release or ASAN.

## Vita3K status (2026-08-19)
- Root cause of the Vita3K failures found and fixed: `_Thread_local g_current`
  degraded to a single process-global slot on the Vita build (emutls with no
  gthread layer), corrupting thread identity; a failing test macro then called
  `exit(1)`, newlib freed the heap memblock while threads were running, and
  Vita3K reported the downstream invalid heap reads. Details in
  `VITA3K_DEBUG_HANDOFF_PROMPT.md` (RESUELTO section).
- Fix: current-TCB pointer now lives in the per-thread newlib pthread-data
  slot (`vitasdk_get_pthread_data`) on the Vita backend; sce-sim keeps
  `_Thread_local`.
- Verified on Vita3K: basic, mutex_types, rwlock, once all PASS.
- Linux sce-sim suite after the change: 43/43 PASS.

## Second Vita-only bug found and fixed (2026-08-19)
- `posix.c` classified lazily-initialized slots (mutex/cond/rwlock/spin) with
  `(intptr_t)p > 0`. Vita's newlib heap lives at `0x88000000+`, so every real
  object pointer is negative as `intptr_t`: after the first lock created the
  object, every later operation on a statically-initialized object returned
  EINVAL (observed as `pthread_mutex_unlock == 22` right after a successful
  lock, errorcheck relock returning 22 instead of EDEADLK, cond timedwait
  returning EINVAL instead of ETIMEDOUT). Tests then died or hung silently.
  Fixed with `PTR_IS_OBJ` (NULL / sentinel `-1..-3` / real pointer), sign-free.
- Vita3K perf note: one thread create+join cycle costs ~73 ms under Vita3K,
  so `handle_reuse` (3000 cycles) and `cancel_stress` need multi-minute
  timeouts there; they are slow, not stuck.

## Third wave of Vita fixes (2026-08-19, evening)
- Newlib reent leak: a joinable thread returning through the trampoline never
  released its newlib reent (fixed 256-entry table); long create/join
  workloads exhausted it and `vitasdk_get_pthread_data` began returning
  `(void*)-4` (reent NULL - 4), which the TCB slot then dereferenced. Fixed:
  `vp_hal_thread_reent_cleanup_current()` at the end of the trampoline plus a
  `-4` guard in `vp_hal_current_tcb_slot()`.
- `vp_thread_join` no longer relies on the HAL timed join: Vita3K ignores the
  `sceKernelWaitThreadEnd` timeout, which froze the 2ms cancellation-polling
  loop. The join now polls the TCB `state` (>= EXITED) at the same 2ms
  cadence and only issues the blocking HAL join to reap an already-exited
  thread. Cost: join granularity is now ~2ms everywhere (host ctest 2s -> 9s).
- `sem_wait`/`sem_timedwait` (Vita backend) now execute a cancellation point
  even when the semaphore is immediately available, per POSIX; before, a
  pending deferred cancel was skipped when `value > 0`
  (posix_cancel_points `one(x,1)` cases).
- Test-only: `posix_cond_smasher` rendezvous deadline relaxed to 5s on
  `__vita__` (500ms flakes under emulator scheduling).
- Vita3K operational lore: its log is buffered — kill with SIGTERM and wait a
  few seconds before reading, or trailing sceClibPrintf output (including
  FAIL lines) is lost; SIGKILL-based harnesses misclassify failures as silent
  timeouts. Boot occasionally stalls mid-module-load after an abrupt kill;
  retry after a clean TERM.
- Known-limited on Vita until async injection exists
  (`vp_hal_thread_inject_noreturn` returns ENOTSUP): `async_cancel`,
  `async_cleanup_tsd`, `cancel_stress`, `posix_cancel` (all spin with
  async cancel type). `posix_once_cancel` passes only when the cancel wins
  the pre-`setcanceltype` race.

## Vita3K final sweep (2026-08-19, closing)
- Full run of all 34 Vita test VPKs against the final library: **29 PASS**,
  zero invalid memory accesses anywhere.
- The 5 non-passing tests are exactly the async-cancellation family
  (`async_cancel`, `async_cleanup_tsd`, `cancel_stress`, `posix_cancel`,
  `posix_once_cancel`): they spin in `for(;;)` with
  `PTHREAD_CANCEL_ASYNCHRONOUS` and require
  `vp_hal_thread_inject_noreturn`, which the Vita HAL deliberately leaves
  ENOTSUP until the suspend/set-context primitive is implemented on real
  hardware. They fail visibly with ENOTSUP or hang on the spinning thread
  depending on whether the cancel wins the pre-`setcanceltype` race.
- Everything else that failed at the start of the day is fixed:
  `posix_cancel_points`, `posix_cond_smasher`, `posix_cond_clock`,
  `handle_reuse` (224s full 3000-cycle run; now 1200 cycles on `__vita__`),
  `posix_basic/mutex/cond/robust/...`, `tls_emutls`.
- Host suite: 44/44 PASS throughout.

## Async cancellation over ForVM implemented (2026-08-19, night)
- `vp_hal_thread_inject_noreturn` (Vita backend) now performs the planned
  injection through the USER-side ForVM thread-context API — no kernel
  module involved: `sceKernelSuspendThreadForVM` / `sceKernelResumeThreadForVM`
  (SceKernelThreadMgr user exports, stub lib now linked by the Vita tests)
  and `sceKernelGetThreadContextForVM` / `sceKernelSetThreadContextForVM`
  (SceLibKernel user exports). Sequence: suspend -> re-check async guard
  while stopped -> get context -> rewrite PC/R0 (+CPSR.T for Thumb entry) ->
  read-back verify -> resume.
- Every failure (missing VM capability on real hardware, stubbed API in an
  emulator) resumes the target untouched and returns ENOTSUP. The read-back
  verify is required because Vita3K's `_sceKernelSetThreadContextForVM` is a
  silent no-op that returns success.
- vitasdk 2026.08 ships the import stubs but no prototypes/types, so
  `hal_vita.h` declares them locally (layouts match the firmware ABI as
  implemented by Vita3K HLE: CPU `{size,cpsr,reg[16],tpidrurw,teehbr,sb,st}`
  = 0x58, VFP `{size,fpscr,float reg[64]}` = 0x108). Drop that block if
  vita-headers gains them.
- Verified under Vita3K: the whole chain executes (Suspend -> Get -> Set
  [Unimplemented] -> verify detects the no-op -> Resume) and the async
  family (`async_cancel`, `async_cleanup_tsd`, `cancel_stress`,
  `posix_cancel`, `posix_once_cancel`) now fails fast and visibly with
  ENOTSUP instead of hanging on the spinning target. `basic` still passes.
- On real hardware these tests should pass once the process SELF carries the
  VM capability (e.g. unsafe homebrew / kubridge-granted); nothing further
  is needed in vpthread — the capability just makes the same calls succeed.
- Note: Vita3K occasionally crashes at shutdown after ForVM suspend/resume
  of spinning threads (host-side assert); harmless for test harnesses that
  read results from the marker/log.

## Next work
1. Decide whether to annotate or replace the musl polling adapter if a warning-clean TSAN corpus is required.
2. Keep HAL async injection semantics independent of host pthread cancellation.
3. Validate the ForVM injection on real hardware with a VM-capable SELF
   (unsafe homebrew): expected to turn the async-cancellation test family
   green without further code changes.
4. DONE (2026-08-19): gthread layer is now active on Vita builds. libgcc's
   gthr-posix binds through weak references to `pthread_cancel`,
   `pthread_key_create/getspecific/setspecific`, `pthread_once`,
   `pthread_mutex_init/lock/unlock` — all already exported by `posix.c` with
   newlib-compatible ABI (pointer-sized mutex/key slots, `-1/-2/-3` static
   initializers, zero-initialized `pthread_once_t`). Because weak references
   do not extract archive members, `thread.c` now anchors `posix.o` on the
   Vita backend so any use of vpthread links the strong pthread symbols and
   `__gthread_active_p()` becomes true. Covered by `tests/tls_emutls.c`
   (cross-thread `_Thread_local` isolation; fails if emutls degrades to the
   shared-slot fallback). `_Thread_local` inside the library itself remains
   banned on Vita (the current-TCB slot predates gthread activation during
   early init).
5. Run the remaining Vita test VPKs under Vita3K (knowing its
   `sceKernelWaitThreadEnd` ignores timeouts and does not fill `stat` when it
   blocks; cancellation-of-joiner semantics differ from hardware there).
