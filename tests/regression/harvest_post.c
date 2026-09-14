#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t ok = "";
  tx_t st = "";
  tx_t body = "";
  tx_t body2 = "";
  tx_t ok3 = "";
  tx_t ok4 = "";
  tx_t st4 = "";
  tx_t ok5 = "";
  tx_t st5 = "";
  tx_t ok6 = "";
  tx_t body6 = "";
    tx_t p = plant_net_harvest("http://127.0.0.1:41234/post", "POST", "hello=world", 0, 0);
    ok = _map_get(p, "ok");
    st = _map_get(p, "status");
    body = _map_get(p, "body");
    plant_iReport_print(get_report(), _cat("post ok = ", ok));
    plant_iReport_print(get_report(), _cat("post status = ", st));
    plant_iReport_print(get_report(), _cat("post echo = ", body));
    tx_t p2 = plant_net_harvest("http://127.0.0.1:41234/post", "POST", "", 0, 0);
    body2 = _map_get(p2, "body");
    plant_iReport_print(get_report(), _cat("post empty body echo = ", body2));
    tx_t p3 = plant_net_harvest("http://127.0.0.1:41234/get", "GET", "", 0, 5);
    ok3 = _map_get(p3, "ok");
    plant_iReport_print(get_report(), _cat("timeout 5 request ok = ", ok3));
    tx_t p4 = plant_net_harvest("http://127.0.0.1:41234/slow", "GET", "", 0, 1);
    ok4 = _map_get(p4, "ok");
    st4 = _map_get(p4, "status");
    plant_iReport_print(get_report(), _cat("short timeout ok = ", ok4));
    plant_iReport_print(get_report(), _cat("short timeout status = ", st4));
    tx_t p5 = plant_net_harvest("http://127.0.0.1:41234/bad", "GET", "", 0, 0);
    ok5 = _map_get(p5, "ok");
    st5 = _map_get(p5, "status");
    plant_iReport_print(get_report(), _cat("malformed ok = ", ok5));
    plant_iReport_print(get_report(), _cat("malformed status = ", st5));
    tx_t p6 = plant_net_harvest("http://127.0.0.1:41234/empty", "GET", "", 0, 0);
    ok6 = _map_get(p6, "ok");
    body6 = _map_get(p6, "body");
    plant_iReport_print(get_report(), _cat("empty response ok = ", ok6));
    plant_iReport_print(get_report(), _cat("empty response body = ", body6));
    return 0;
}
