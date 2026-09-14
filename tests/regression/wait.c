#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    long n = 120;
    long t0 = _to_long ( ffi_now ( ) );
    plant_msleep(n);
    long t1 = _to_long ( ffi_now ( ) );
    long dt = t1 - t0;
    if (dt >= 100 && dt <= 1000) {
    plant_iReport_print(get_report(), "timing-ok");
    }
    long s0 = _to_long ( ffi_now ( ) );
    plant_msleep(0);
    long s1 = _to_long ( ffi_now ( ) );
    long dz = s1 - s0;
    if (dz < 100) {
    plant_iReport_print(get_report(), "zero-ok");
    }
    plant_msleep(- 5);
    plant_iReport_print(get_report(), "neg-ok");
    plant_msleep(0);
    plant_iReport_print(get_report(), "bare-ok");
    return 0;
}
