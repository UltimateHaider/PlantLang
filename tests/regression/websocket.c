#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t conn = "";
  tx_t sr = "";
    conn = plant_ws_connect("ws://echo.websocket.org:80");
    plant_iReport_print(get_report(), _cat("connected=", conn));
    sr = plant_ws_send(conn, "Hello WS");
    plant_iReport_print(get_report(), _cat("sent=", sr));
    return 0;
}
