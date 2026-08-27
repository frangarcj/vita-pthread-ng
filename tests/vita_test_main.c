#include <stdio.h>
#include "vpthread/vpthread.h"

extern int vp_vita_test_main(void);

int main(void)
{
    vp_vita_test_main();
    FILE *marker = fopen(VP_VITA_TEST_MARKER, "w");
    if (marker)
    {
        fprintf(marker, "%s PASS\n", VP_VITA_TEST_NAME);
        fclose(marker);
    }
    printf("%s PASS\n", VP_VITA_TEST_NAME);
    vp_sleep_ms(3000);
    return 0;
}