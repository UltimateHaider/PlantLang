#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t ok = "";
    plant_iReport_print(get_report(), "listen_json start");
    tx_t req = plant_net_listen(41235);
    ok = _map_get(req, "ok");
    plant_iReport_print(get_report(), _cat("listen_json ok = ", ok));
    PlantArray* m = plant_list_make ( 0 );
    m = plant_link(m, "name", "chloroplast");
    m = plant_link(m, "ok", "true");
    plant_net_respond_json(req, m);
    plant_iReport_print(get_report(), "listen_json responded");
    return 0;
}
