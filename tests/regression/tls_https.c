#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t r = "";
    r = plant_net_harvest_https_url("https://httpbin.org/get", "GET");
    tx_t ok_val = _map_get ( r , "ok" );
    plant_iReport_print(get_report(), ok_val);
    return 0;
}
