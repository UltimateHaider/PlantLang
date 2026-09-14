#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    tx_t s = _cat("abc", "def");
    tx_t t = "xyz";
    s = plant_mem_free((tx_t)s);
    s = plant_mem_free((tx_t)s);
    plant_iReport_print(get_report(), _cat3("after_free=", s, "!"));
    plant_iReport_print(get_report(), _cat("t_alive=", t));
  return main;
}
