#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#include "test.h"

static int prepare_count, parent_count, child_count;
static void prep(void) { prepare_count++; }
static void par(void) { parent_count++; }
static void chi(void) { child_count++; }

int main(void)
{
    EXPECT(pthread_atfork(prep, par, chi) == 0);
    pid_t p = fork();
    EXPECT(p >= 0);
    if (p == 0)
        _exit((prepare_count == 1 && child_count == 1) ? 0 : 7);
    int st = 0;
    EXPECT(waitpid(p, &st, 0) == p);
    EXPECT(WIFEXITED(st) && WEXITSTATUS(st) == 0);
    EXPECT(prepare_count == 1 && parent_count == 1);
    return 0;
}