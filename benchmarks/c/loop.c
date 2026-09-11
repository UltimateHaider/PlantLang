/* Baseline: arithmetic loop, runtime-opaque bound (time-derived, like the
   plant benchmark) so gcc -O2 cannot fold the loop */
#include <stdio.h>
#include <time.h>

int main(void) {
    long n = 10000000 + (time(NULL) % 1000);
    long acc = 0;
    for (long i = 0; i < n; i++)
        acc += 1;
    printf("loop-n=%ld\n", n);
    printf("loop-acc=%ld\n", acc);
    return 0;
}
