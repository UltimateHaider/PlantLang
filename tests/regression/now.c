#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t NOW = "";
    plant_iReport_print(get_report(), plant_map_to_string(plant_analyze(plant_now("DATE"))));
    plant_iReport_print(get_report(), plant_map_to_string(plant_analyze(plant_now("TIME"))));
    plant_iReport_print(get_report(), plant_map_to_string(plant_analyze(plant_now("YEAR"))));
    plant_iReport_print(get_report(), plant_map_to_string(plant_analyze(plant_now("STAMP"))));
    plant_iReport_print(get_report(), plant_now("BOGUS"));
    plant_iReport_print(get_report(), plant_typeof(plant_now("")));
    plant_iReport_print(get_report(), _cat("d", plant_typeof(plant_now("DATE"))));
    plant_iReport_print(get_report(), _cat("t", plant_typeof(plant_now("TIME"))));
    return 0;
}
