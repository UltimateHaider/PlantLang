#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/



int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  tx_t c = "";
  tx_t lines = "";
  plant_async_config("TRACE", "ON");
  plant_async_config("TRACE_FILE", "/tmp/plantlang_async_in_trace.log");
  c = ffi_ctx_make(1, 64, "wctx");
  plant_async_trace_in(c, 0, "hello ctx");
  plant_trace(0, "", "hello plain");
  lines = ffi_read_trace("/tmp/plantlang_async_in_trace.log");
  plant_iReport_print(get_report(), _cat("trace: ", lines));
  plant_async_drain();
  return 0;
}
