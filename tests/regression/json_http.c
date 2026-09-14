#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t ok = "";
  tx_t jb = "";
  tx_t key = "";
  tx_t nested = "";
  tx_t x = "";
  tx_t arr = "";
  tx_t el0 = "";
  tx_t back = "";
  tx_t arrback = "";
  tx_t body = "";
    plant_iReport_print(get_report(), "json start");
    tx_t r = plant_net_harvest_json("http://127.0.0.1:41234/json", "GET", "", 0, 0);
    ok = _map_get(r, "ok");
    plant_iReport_print(get_report(), _cat("json ok = ", ok));
    jb = _map_get(r, "body");
    key = json_get(jb, "key");
    plant_iReport_print(get_report(), _cat("json key = ", json_val ( key )));
    nested = json_get(jb, "nested");
    x = json_get(nested, "x");
    plant_iReport_print(get_report(), _cat("json nested x = ", json_val ( x )));
    arr = json_get(jb, "list");
    el0 = json_at(arr, 0);
    plant_iReport_print(get_report(), _cat("json first = ", json_val ( el0 )));
    back = json_stringify(jb);
    plant_iReport_print(get_report(), _cat("json roundtrip = ", back));
    arrback = json_stringify(arr);
    plant_iReport_print(get_report(), _cat("json array = ", arrback));
    tx_t t = plant_net_harvest("http://127.0.0.1:41234/get", "GET", "", 0, 0);
    body = _map_get(t, "body");
    plant_iReport_print(get_report(), _cat("plain body = ", body));
    plant_iReport_print(get_report(), "json done");
    return 0;
}
