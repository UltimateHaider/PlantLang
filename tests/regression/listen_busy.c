#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t bok = "";
    plant_iReport_print(get_report(), "busy start");
    tx_t req = plant_net_listen(41234);
    bok = _map_get(req, "ok");
    plant_iReport_print(get_report(), _cat("busy ok = ", bok));
    plant_iReport_print(get_report(), "busy done");
    return 0;
}
