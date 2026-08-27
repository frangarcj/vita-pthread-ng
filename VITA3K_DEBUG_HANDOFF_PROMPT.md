# Prompt de continuidad para depuracion Vita3K

Estoy trabajando en macOS en este repositorio:

`/Users/frangar/Fun/vitasdk-next/vita-pthread-ng`

Objetivo: depurar por que los tests Vita fallan bajo Vita3K. No hagas commit, no reviertas cambios existentes y responde en espanol.

## Restricciones

- Backend host: `sce-sim`.
- Backend Vita: VitaSDK dentro de Docker.
- Imagen Docker: `vitasdk/vitasdk:2026.08`.
- Emulador: `/Applications/Vita3K.app/Contents/MacOS/Vita3K`.
- VitaFS: `$HOME/Library/Application Support/Vita3K/Vita3K/fs/`.
- No uses `--console`.
- Todos los comandos deben estar protegidos con `timeout`.
- Cierra Vita3K despues con `pkill -TERM -x Vita3K`.

## Estado del diagnostico

`vpthread` reserva los mutexes y los TCB mediante `calloc`. El TCB privado esta en `src/internal.h`. Contiene estado, referencias, cancelacion, esperas y cleanup.

El campo relevante era:

```c
_Atomic unsigned async_guard;
```

En ARM, sus operaciones se convierten en `ldrex/strex`.

Con la implementacion normal, Vita3K fallaba inicialmente en:

```text
0x818000d8
```

El TCB empezaba en:

```text
0x818000a8
```

Y:

```text
0x818000a8 + 0x30 = 0x818000d8
```

Por tanto, ese acceso correspondia a `async_guard`.

Se anadio una opcion diagnostica:

```text
-DVPTHREAD_DIAGNOSTIC_NO_ASYNC_GUARD_ATOMIC=ON
```

Cuando esta activa solo para Vita, `async_guard` pasa a ser `unsigned` y se usan incrementos/decrementos normales. El resto de atomics permanece sin modificar.

La variante sin `async_guard` atomico compila correctamente:

```text
vita_basic_no_guard_atomic_build=PASS
```

Tambien se ejecuto en Vita3K. El resultado fue importante:

1. Vita3K arranca correctamente el juego.
2. Ya no falla en `async_guard`.
3. Despues falla en una lectura normal, no atomica:

```text
Invalid read at 0x88000014
PC: 0x8100224c
mutex_after_acquire
```

La direccion corresponde a:

```text
m = 0x88000008
m + 0x0c = 0x88000014
```

Es decir, una lectura normal de un campo de `vp_mutex_t`, concretamente `m->robust`.

Despues tambien falla otra lectura normal de:

```text
0x88000008
PC: 0x81002592
vp_mutex_unlock
```

Ese acceso corresponde a `m->magic`.

Vita3K finalmente informa:

```text
_sceKernelLockMutex returned SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID
```

## Conclusion actual

- El problema no es unicamente `ldrex/strex`.
- Quitar el atomic de `async_guard` permite avanzar, pero Vita3K tambien rechaza lecturas normales de objetos ubicados en el heap.
- Cambiar el tamano del heap de newlib de 128 MiB a 8 MiB solo cambia las direcciones:
  - default: `0x88000000`
  - 8 MiB: `0x81800000`
- El override de `_newlib_heap_size_user` ya fue retirado de `tests/vita_test_main.c`.
- No seguir intentando solucionar esto bajando el heap.

## Build de la variante sin atomic

```sh
timeout 180s docker run --rm \
  -v "$PWD":/src \
  -w /src \
  vitasdk/vitasdk:2026.08 \
  sh -c 'set -e;
    cmake -S /src \
      -B /src/build-docker-vita-no-guard-atomic \
      -DCMAKE_TOOLCHAIN_FILE=/usr/local/vitasdk/share/vita.toolchain.cmake \
      -DVPTHREAD_BACKEND=vita \
      -DVPTHREAD_DIAGNOSTIC_NO_ASYNC_GUARD_ATOMIC=ON \
      -DVITASDK=/usr/local/vitasdk;
    timeout 120s cmake --build \
      /src/build-docker-vita-no-guard-atomic \
      --target vpthread-vita-basic.vpk-vpk -j2'
```

Si CMake usa un compilador incorrecto, el directorio de build esta cacheado. Usa un directorio nuevo o elimina solo ese directorio de build y vuelve a configurar; no hagas reset del repositorio.

## Ejecucion en Vita3K

```sh
timeout 45s sh -c '
  fs="$HOME/Library/Application Support/Vita3K/Vita3K/fs";

  timeout 5s pkill -TERM -x Vita3K >/dev/null 2>&1 || true;
  timeout 10s mkdir -p "$fs/ux0/app/VPTHR0001" "$fs/ux0/data";
  timeout 10s rm -rf "$fs/ux0/app/VPTHR0001"/*;
  timeout 10s unzip -oq \
    "$PWD/build-docker-vita-no-guard-atomic/vpthread-vita-basic.vpk" \
    -d "$fs/ux0/app/VPTHR0001";
  timeout 5s rm -f "$fs/ux0/data/VPTHR0001.result";

  timeout 15s \
    /Applications/Vita3K.app/Contents/MacOS/Vita3K \
    --installed-path VPTHR0001 \
    --archive-log \
    > /tmp/vpthread-vita3k-basic-no-guard-atomic.log 2>&1;
  vita_exit=$?;

  timeout 5s pkill -TERM -x Vita3K >/dev/null 2>&1 || true;

  printf "vita3k_exit=%s\\n" "$vita_exit";
  timeout 10s grep -E \
    "Invalid read|Invalid write|Invalid exclusive|Game started|Game closed|ERROR|CRITICAL" \
    "$HOME/Library/Application Support/Vita3K/Vita3K/logs/VPTHR0001 - [vpthread basic].log" \
    | tail -n 60 || true;

  exit 0
'
```

## Siguiente paso recomendado

Crear un test Vita minimo que haga por separado:

1. Escritura y lectura normal sobre una variable local.
2. Escritura y lectura normal sobre memoria de `malloc`.
3. `atomic_fetch_add` sobre una variable local.
4. `atomic_fetch_add` sobre memoria de `malloc`.
5. Lecturas y atomics sobre un bloque reservado por el heap VitaSDK.

El objetivo es determinar si el fallo esta en:

- cualquier acceso a memoria del heap;
- solo los atomics sobre el heap;
- o el layout/mapeo concreto de memoria que Vita3K asigna al proceso.

No continuar cambiando el tamano del heap hasta tener ese resultado.

## Validaciones ya hechas

- El build Vita normal de `basic` pasa sin el override de heap.
- El build Vita diagnostico sin `async_guard` atomico pasa.
- `git diff --check` no encontro errores.
- Los diagnosticos del editor no muestran errores en los archivos tocados.
- Vita3K se cerro correctamente despues de la prueba.
- El build host limpio en macOS queda bloqueado por problemas previos del simulador con los headers nativos (`pthread_t`, `pthread_attr_t`, `ucontext`, `pthread_timedjoin_np`), no por esta sonda.

## RESUELTO (2026-08-19)

### Causa raiz

El problema nunca fue el heap, ni los atomics, ni `ldrex/strex`. La cadena real:

1. `static _Thread_local vp_thread_t g_current` en `src/thread.c` se compila con
   emutls (GCC). emutls necesita la capa `__gthread_*`, que en vitasdk la provee
   la libreria pthread enlazada. vpthread ES la libreria pthread y no la provee,
   asi que `__gthread_active_p()` devuelve 0 (simbolo debil resuelto a 0; se ve
   en el desensamblado del binario) y emutls degrada a su fallback single-thread:
   `g_current` pasa a ser UN UNICO SLOT GLOBAL compartido por todos los hilos.
2. Con `g_current` compartida, `vp_thread_self()` devuelve el TCB de otro hilo,
   `vp_finish_current()` escribe el retval en un TCB equivocado o fantasma, y
   los checks de owner de mutex se cruzan. Algun `CHECK0`/`EXPECT` de `test.h`
   falla y llama a `exit(1)` (el `fprintf(stderr)` no aparece en el log de
   Vita3K, por eso no se veia el motivo).
3. `exit()` ejecuta `_free_vita_heap` de newlib: libera el memblock del heap
   MIENTRAS los otros hilos siguen corriendo. De ahi los `Invalid read` en
   `0x88000xxx`/`0x818000xx` (siempre heap_base + offset), el
   `UNKNOWN_MUTEX_ID` (el UID se lee como 0 de memoria ya desmapeada), el
   `UNKNOWN_LW_MUTEX_ID` (lwmutex de malloc borrado) y el
   `ILLEGAL_BLOCK_ID` final (doble free del memblock al salir de verdad).

En Linux nunca se reproduce porque el host tiene TLS real.

Sondas que lo demostraron (fuentes en scratchpad de la sesion, reproducibles):
- memprobe: heap de 128 MiB mapeado en 0x88000000; lecturas, escrituras,
  `atomic_fetch_add` (ldrex/strex) sobre heap y memblock manual, llamadas HLE
  y un hilo crudo leyendo heap: TODO funciona en Vita3K.
- basic instrumentado sin `exit()`: todo pasa salvo el retval del join que
  bloquea (TCB fantasma por la g_current compartida).
- Desensamblado de `__gthread_active_p`/`__gthread_getspecific` en el elf:
  stubs debiles vacios; emutls en modo single-thread.

### Fix aplicado

- `include/vpthread/hal_vita.h`: nuevos `vp_hal_current_tcb_slot/get/set`
  usando `vitasdk_get_pthread_data(0)` (slot puntero por-hilo que newlib
  reserva junto al reent de cada hilo, resuelto via TLS del kernel; pensado
  exactamente para los metadatos de la libreria pthread).
- `src/thread.c`: `g_current` pasa a macros `vp_current_get()/vp_current_set()`;
  en backend vita usan el slot del HAL, en sce-sim sigue `_Thread_local`.

### Verificado tras el fix

- Vita3K: `basic PASS` (marker en ux0:data), `mutex_types PASS`, `rwlock PASS`,
  `once PASS`. Sin `Invalid read` en los logs.
- Linux sce-sim (docker gcc:14, build-docker-release): 43/43 tests PASS.

### Notas colaterales descubiertas (Vita3K, no vpthread)

- `sceKernelWaitThreadEnd` en Vita3K IGNORA el puntero de timeout: bloquea
  hasta que el hilo termina y nunca devuelve `SCE_KERNEL_ERROR_WAIT_TIMEOUT`.
  El bucle de `vp_thread_join` (polling de 2 ms para cancelacion) no rota en
  el emulador; en hardware real si. Cancelar a un joiner bajo Vita3K no
  desbloqueara el join hasta que el hilo objetivo muera.
- `sceKernelWaitThreadEnd` tampoco escribe `stat` cuando la espera bloquea
  (solo lo escribe si el hilo ya habia terminado antes de la llamada).
  vpthread no usa `stat`, asi que no afecta.
- `VPTHREAD_DIAGNOSTIC_NO_ASYNC_GUARD_ATOMIC` ya no es necesario para esto
  (los atomics sobre heap funcionan en Vita3K); se puede retirar cuando se
  quiera.

### Pendiente sugerido

- Proveer los hooks `__gthread_*` reales desde vpthread (backend vita) para
  que `_Thread_local` de las apps, emutls y los guards C++ funcionen sobre
  vpthread. Hasta entonces, no usar `_Thread_local` dentro de la libreria.
