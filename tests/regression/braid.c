#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
  tx_t va = "";
  tx_t vb = "";
  tx_t vc = "";
  tx_t vd = "";
  tx_t sa = "";
  tx_t sb = "";
  tx_t sc = "";
  tx_t px = "";
  tx_t py = "";
  tx_t p1 = "";
  tx_t ma = "";
  tx_t mb = "";
  tx_t mc = "";
  tx_t dp = "";
  tx_t dq = "";
  tx_t e1 = "";
  tx_t e2 = "";
    PlantArray* keys = plant_list_make ( 0 );
    keys = plant_list_add(keys, "a");
    keys = plant_list_add(keys, "b");
    keys = plant_list_add(keys, "c");
    PlantArray* vals = plant_list_make ( 0 );
    vals = plant_list_add(vals, "1");
    vals = plant_list_add(vals, "2");
    vals = plant_list_add(vals, "3");
    PlantArray* pairs = plant_braid(keys, vals);
    va = _map_get(pairs, "a");
    vb = _map_get(pairs, "b");
    vc = _map_get(pairs, "c");
    vd = _map_get(pairs, "d");
    plant_iReport_print(get_report(), _cat("braid a = ", va));
    plant_iReport_print(get_report(), _cat("braid b = ", vb));
    plant_iReport_print(get_report(), _cat("braid c = ", vc));
    plant_iReport_print(get_report(), _cat("braid missing d = ", vd));
    PlantArray* shortVals = plant_list_make ( 0 );
    shortVals = plant_list_add(shortVals, "1");
    shortVals = plant_list_add(shortVals, "2");
    PlantArray* pairs2 = plant_braid(keys, shortVals);
    sa = _map_get(pairs2, "a");
    sb = _map_get(pairs2, "b");
    sc = _map_get(pairs2, "c");
    plant_iReport_print(get_report(), _cat("shorter values a = ", sa));
    plant_iReport_print(get_report(), _cat("shorter values b = ", sb));
    plant_iReport_print(get_report(), _cat("shorter values c (excess key ignored) = ", sc));
    PlantArray* shortKeys = plant_list_make ( 0 );
    shortKeys = plant_list_add(shortKeys, "x");
    shortKeys = plant_list_add(shortKeys, "y");
    PlantArray* pairs3 = plant_braid(shortKeys, vals);
    px = _map_get(pairs3, "x");
    py = _map_get(pairs3, "y");
    p1 = _map_get(pairs3, "1");
    plant_iReport_print(get_report(), _cat("shorter keys x = ", px));
    plant_iReport_print(get_report(), _cat("shorter keys y = ", py));
    plant_iReport_print(get_report(), _cat("shorter keys value as key = ", p1));
    PlantArray* m = plant_braid_map(keys, shortVals);
    ma = _map_get(m, "a");
    mb = _map_get(m, "b");
    mc = _map_get(m, "c");
    plant_iReport_print(get_report(), _cat("map braid a = ", ma));
    plant_iReport_print(get_report(), _cat("map braid b = ", mb));
    plant_iReport_print(get_report(), _cat("map braid c (absent, excess key dropped) = ", mc));
    PlantArray* dk = plant_list_make ( 0 );
    dk = plant_list_add(dk, "p");
    dk = plant_list_add(dk, "q");
    dk = plant_list_add(dk, "p");
    PlantArray* dv = plant_list_make ( 0 );
    dv = plant_list_add(dv, "1");
    dv = plant_list_add(dv, "2");
    dv = plant_list_add(dv, "9");
    PlantArray* dm = plant_braid_map(dk, dv);
    dp = _map_get(dm, "p");
    dq = _map_get(dm, "q");
    plant_iReport_print(get_report(), _cat("dup key p (last wins) = ", dp));
    plant_iReport_print(get_report(), _cat("dup key q = ", dq));
    PlantArray* empty = plant_list_make ( 0 );
    PlantArray* pe = plant_braid(empty, vals);
    e1 = _map_get(pe, "a");
    plant_iReport_print(get_report(), _cat("empty braid = done ", e1));
    PlantArray* me = plant_braid_map(empty, empty);
    e2 = _map_get(me, "a");
    plant_iReport_print(get_report(), _cat("empty map braid = done ", e2));
    return 0;
}
