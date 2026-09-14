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
  tx_t hd = "";
  tx_t probe = "";
    plant_iReport_print(get_report(), "listen_http start");
    tx_t req = plant_net_listen_timeout(41235, 5);
    lok = _map_get(req, "ok");
    lm = _map_get(req, "method");
    lp = _map_get(req, "path");
    lb = _map_get(req, "body");
    plant_iReport_print(get_report(), _cat("listen_http ok = ", lok));
    plant_iReport_print(get_report(), _cat("listen_http method = ", lm));
    plant_iReport_print(get_report(), _cat("listen_http path = ", lp));
    plant_iReport_print(get_report(), _cat("listen_http body = ", lb));
    hd = _map_get(req, "headers");
    probe = _map_get(hd, "X-Probe");
    plant_iReport_print(get_report(), _cat("listen_http header = ", probe));
    plant_net_respond(req, "Hello from Chloroplast");
    plant_iReport_print(get_report(), "listen_http responded");
    return 0;
}
