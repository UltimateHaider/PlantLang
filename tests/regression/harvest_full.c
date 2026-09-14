#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t ok1 = "";
  tx_t st1 = "";
  tx_t bd1 = "";
  tx_t hd1 = "";
  tx_t cl1 = "";
  tx_t fd = "";
  tx_t push = "";
  tx_t w1 = "";
  tx_t ack = "";
  tx_t c1 = "";
  tx_t c2 = "";
  tx_t pr = "";
  tx_t pw = "";
  tx_t bf = "";
    plant_iReport_print(get_report(), "full start");
    tx_t r = plant_net_harvest_map("http://127.0.0.1:41234/readback", "GET", "", 0, 0);
    ok1 = _map_get(r, "ok");
    st1 = _map_get(r, "status");
    bd1 = _map_get(r, "body");
    plant_iReport_print(get_report(), _cat("full ok = ", ok1));
    plant_iReport_print(get_report(), _cat("full status = ", st1));
    plant_iReport_print(get_report(), _cat("full body = ", bd1));
    hd1 = _map_get(r, "headers");
    cl1 = _map_get(hd1, "Content-Length");
    plant_iReport_print(get_report(), _cat("full clen = ", cl1));
    fd = _map_get(r, "sock");
    push = plant_net_read(fd);
    plant_iReport_print(get_report(), _cat("full read = ", push));
    w1 = plant_net_write(fd, "client-push");
    plant_iReport_print(get_report(), _cat("full write = ", w1));
    ack = plant_net_read(fd);
    plant_iReport_print(get_report(), _cat("full ack = ", ack));
    c1 = plant_net_close(fd);
    c2 = plant_net_close(fd);
    plant_iReport_print(get_report(), _cat("full close1 = ", c1));
    plant_iReport_print(get_report(), _cat("full close2 = ", c2));
    pr = plant_net_read(fd);
    plant_iReport_print(get_report(), _cat3("full read-after-close = ", pr, " (empty)"));
    pw = plant_net_write(fd, "zzz");
    plant_iReport_print(get_report(), _cat("full write-after-close = ", pw));
    bf = plant_net_read("bogus");
    plant_iReport_print(get_report(), _cat3("full read-bogus-fd = ", bf, " (empty)"));
    plant_iReport_print(get_report(), "full done");
    return 0;
}
