#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t sa();
tx_t fa();
tx_t pr();
tx_t plant_main();


tx_t sa() {
  if (plant_boundary_block("sa", "SAFE")) return "";
  plant_safe_enter("sa");
  plant_safe_channel_init("sa");
    plant_iReport_print(get_report(), "safe ran");
  plant_safe_exit();
  return sa;
}
tx_t fa() {
  if (plant_boundary_block("fa", "FAST")) return "";
  plant_fast_enter("fa");
    plant_iReport_print(get_report(), "fast ran");
  plant_fast_reset();
  plant_fast_exit();
  return fa;
}
tx_t pr() {
  if (plant_boundary_block("pr", "PERSISTENT")) return "";
  plant_persist_enter("pr");
    plant_safe_call("sa", 0);
    plant_iReport_print(get_report(), "sa blocked ok");
    fa();
    plant_iReport_print(get_report(), "fa ran ok");
    plant_iReport_print(get_report(), "pr done");
  plant_persist_exit();
  return pr;
}
tx_t plant_main() {
    pr();
    plant_iReport_print(get_report(), ffi_audit_dump ( ));
  return plant_main;
}
tx_t plant_safe_adapter_sa(int argc, tx_t* argv) {
  return (tx_t)sa();
}

int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_safe_register("sa", plant_safe_adapter_sa);
  plant_maybe_run_worker();
  plant_main();
  plant_async_drain();
  return 0;
}
