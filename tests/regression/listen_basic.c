#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t lok = "";
  tx_t lm = "";
  tx_t lp = "";
  tx_t lb = "";
  tx_t hd2 = "";
  tx_t probe = "";
    plant_iReport_print(get_report(), "listen start");
    tx_t req = plant_net_listen(41235);
    lok = _map_get(req, "ok");
    lm = _map_get(req, "method");
    lp = _map_get(req, "path");
    lb = _map_get(req, "body");
    plant_iReport_print(get_report(), _cat("listen ok = ", lok));
    plant_iReport_print(get_report(), _cat("listen method = ", lm));
    plant_iReport_print(get_report(), _cat("listen path = ", lp));
    plant_iReport_print(get_report(), _cat("listen body = ", lb));
    hd2 = _map_get(req, "headers");
    probe = _map_get(hd2, "X-Probe");
    plant_iReport_print(get_report(), _cat("listen header = ", probe));
    plant_net_respond(req, "hello from chloroplast");
    plant_iReport_print(get_report(), "listen responded");
    return 0;
}
