#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t build_list();
tx_t plant_main();


tx_t build_list() {
  if (plant_boundary_block("build_list", "SAFE")) return "";
  plant_safe_enter("build_list");
  plant_safe_channel_init("build_list");
    PlantArray* l = plant_list_make ( 0 );
    l = plant_list_add(l, "alpha");
    l = plant_list_add(l, "beta");
    l = plant_list_add(l, "gamma");
    plant_safe_exit();
  return l;
}
tx_t plant_main() {
  tx_t out = "";
    out = plant_safe_call("build_list", 0);
    plant_iReport_print(get_report(), _cat("count=", ffi_list_count ( out )));
    plant_iReport_print(get_report(), _cat("a=", ffi_list_get ( out , 0 )));
    plant_iReport_print(get_report(), _cat("b=", ffi_list_get ( out , 1 )));
    plant_iReport_print(get_report(), _cat("c=", ffi_list_get ( out , 2 )));
    plant_iReport_print(get_report(), ffi_safe_status ( ));
  return plant_main;
}
tx_t plant_safe_adapter_build_list(int argc, tx_t* argv) {
  return (tx_t)build_list();
}

int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_safe_register("build_list", plant_safe_adapter_build_list);
  plant_maybe_run_worker();
  plant_main();
  plant_async_drain();
  return 0;
}
