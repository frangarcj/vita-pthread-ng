#ifndef VP_TEST_H
#define VP_TEST_H
#include <stdio.h>
#include <stdlib.h>
#if defined(__vita__)
/* Newlib stderr never reaches the Vita3K log, so mirror failures through
 * sceClibPrintf; without this a failing test dies silently (and exit() frees
 * the newlib heap under still-running threads, flooding the log with invalid
 * accesses that hide the real cause). */
#include <psp2/kernel/clib.h>
#define VP_TEST_FAIL_LOG(...) do{sceClibPrintf(__VA_ARGS__);fprintf(stderr,__VA_ARGS__);}while(0)
#else
#define VP_TEST_FAIL_LOG(...) fprintf(stderr,__VA_ARGS__)
#endif
#define CHECK0(x) do{int _r=(x);if(_r){VP_TEST_FAIL_LOG("FAIL %s -> %d at %s:%d\n",#x,_r,__FILE__,__LINE__);exit(1);}}while(0)
#define EXPECT(x) do{if(!(x)){VP_TEST_FAIL_LOG("FAIL %s at %s:%d\n",#x,__FILE__,__LINE__);exit(1);}}while(0)
#endif
