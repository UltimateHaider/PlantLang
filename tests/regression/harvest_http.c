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
  tx_t pok = "";
  tx_t pst = "";
  tx_t pbody = "";
  tx_t hbody = "";
  tx_t sok = "";
  tx_t sbody = "";
  tx_t fd = "";
  tx_t push = "";
  tx_t w = "";
  tx_t ack = "";
  tx_t c = "";
    plant_iReport_print(get_report(), "http start");
    tx_t res = plant_net_harvest("http://127.0.0.1:41234/get", "GET", "", 0, 0);
    ok = _map_get(res, "ok");
    st = _map_get(res, "status");
    body = _map_get(res, "body");
    plant_iReport_print(get_report(), _cat("get ok = ", ok));
    plant_iReport_print(get_report(), _cat("get status = ", st));
    plant_iReport_print(get_report(), _cat("get body = ", body));
    PlantArray* hdrs = plant_list_make ( 0 );
    hdrs = plant_link(hdrs, "Content-Type", "application/json");
    hdrs = plant_link(hdrs, "X-Test", "plantlang");
    tx_t post_res = plant_net_harvest("http://127.0.0.1:41234/post", "POST", "hello=world", hdrs, 5);
    pok = _map_get(post_res, "ok");
    pst = _map_get(post_res, "status");
    pbody = _map_get(post_res, "body");
    plant_iReport_print(get_report(), _cat("post ok = ", pok));
    plant_iReport_print(get_report(), _cat("post status = ", pst));
    plant_iReport_print(get_report(), _cat("post echo = ", pbody));
    tx_t hdr_res = plant_net_harvest("http://127.0.0.1:41234/get", "GET", "", hdrs, 0);
    hbody = _map_get(hdr_res, "body");
    plant_iReport_print(get_report(), _cat("header echo = ", hbody));
    tx_t stream_res = plant_net_harvest_map("http://127.0.0.1:41234/readback", "GET", "", 0, 0);
    sok = _map_get(stream_res, "ok");
    sbody = _map_get(stream_res, "body");
    plant_iReport_print(get_report(), _cat("map ok = ", sok));
    plant_iReport_print(get_report(), _cat("map body = ", sbody));
    fd = _map_get(stream_res, "sock");
    push = plant_net_read(fd);
    plant_iReport_print(get_report(), _cat("stream read = ", push));
    w = plant_net_write(fd, "harvest-http-push");
    plant_iReport_print(get_report(), _cat("stream write = ", w));
    ack = plant_net_read(fd);
    plant_iReport_print(get_report(), _cat("stream ack = ", ack));
    c = plant_net_close(fd);
    plant_iReport_print(get_report(), _cat("stream close = ", c));
    plant_iReport_print(get_report(), "http done");
    return 0;
}
