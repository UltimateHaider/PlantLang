#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t mok = "";
  tx_t mm = "";
  tx_t mp = "";
    plant_iReport_print(get_report(), "malformed start");
    tx_t req = plant_net_listen(41237);
    mok = _map_get(req, "ok");
    mm = _map_get(req, "method");
    mp = _map_get(req, "path");
    plant_iReport_print(get_report(), _cat("malformed ok = ", mok));
    plant_iReport_print(get_report(), _cat("malformed method = ", mm));
    plant_iReport_print(get_report(), _cat("malformed path = ", mp));
    plant_net_respond(req, "");
    plant_iReport_print(get_report(), "malformed responded");
    return 0;
}
