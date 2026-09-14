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
  tx_t hd = "";
  tx_t xm = "";
  tx_t ok2 = "";
  tx_t body2 = "";
  tx_t body3 = "";
  tx_t ok4 = "";
  tx_t st4 = "";
  tx_t body4 = "";
  tx_t ok5 = "";
  tx_t st5 = "";
    tx_t r = plant_net_harvest("http://127.0.0.1:41234/get", "GET", "", 0, 0);
    ok = _map_get(r, "ok");
    st = _map_get(r, "status");
    body = _map_get(r, "body");
    plant_iReport_print(get_report(), _cat("get ok = ", ok));
    plant_iReport_print(get_report(), _cat("get status = ", st));
    plant_iReport_print(get_report(), _cat("get body = ", body));
    hd = _map_get(r, "headers");
    xm = _map_get(hd, "X-Mock");
    plant_iReport_print(get_report(), _cat("get header X-Mock = ", xm));
    PlantArray* hdrs = plant_list_make ( 0 );
    hdrs = plant_link(hdrs, "X-Test", "abc123");
    tx_t r2 = plant_net_harvest("http://127.0.0.1:41234/get", "GET", "", hdrs, 0);
    ok2 = _map_get(r2, "ok");
    body2 = _map_get(r2, "body");
    plant_iReport_print(get_report(), _cat("custom header ok = ", ok2));
    plant_iReport_print(get_report(), _cat("custom header echo = ", body2));
    PlantArray* emptyHdrs = plant_list_make ( 0 );
    tx_t r3 = plant_net_harvest("http://127.0.0.1:41234/get", "GET", "", emptyHdrs, 0);
    body3 = _map_get(r3, "body");
    plant_iReport_print(get_report(), _cat("empty headers body = ", body3));
    tx_t r4 = plant_net_harvest("http://127.0.0.1:41234/status404", "GET", "", 0, 0);
    ok4 = _map_get(r4, "ok");
    st4 = _map_get(r4, "status");
    body4 = _map_get(r4, "body");
    plant_iReport_print(get_report(), _cat("404 ok = ", ok4));
    plant_iReport_print(get_report(), _cat("404 status = ", st4));
    plant_iReport_print(get_report(), _cat("404 body = ", body4));
    tx_t r5 = plant_net_harvest("", "GET", "", 0, 0);
    ok5 = _map_get(r5, "ok");
    st5 = _map_get(r5, "status");
    plant_iReport_print(get_report(), _cat("empty url ok = ", ok5));
    plant_iReport_print(get_report(), _cat("empty url status = ", st5));
    return 0;
}
