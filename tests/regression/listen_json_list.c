#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t ok = "";
    plant_iReport_print(get_report(), "listen_json_list start");
    tx_t req = plant_net_listen(41235);
    ok = _map_get(req, "ok");
    plant_iReport_print(get_report(), _cat("listen_json_list ok = ", ok));
    PlantArray* l = plant_list_make ( 3 , "a" , "b" , "c" );
    plant_net_respond_json(req, l);
    plant_iReport_print(get_report(), "listen_json_list responded");
    return 0;
}
