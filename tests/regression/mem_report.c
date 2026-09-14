#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    tx_t s = _cat("abc", "def");
    plant_iReport_print(get_report(), plant_map_to_string ( ffi_mem_report ( ) ));
    s = plant_mem_free((tx_t)s);
  return main;
}
