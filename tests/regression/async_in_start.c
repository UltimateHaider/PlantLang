#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t worker(tx_t __parent, tx_t __ctx, tx_t tag);
tx_t main();


static int plant_a_worker_step(tx_t st);

typedef struct {
  tx_t __self;
  long __pc;
  tx_t tag;
} plant_a_worker_state;

tx_t worker(tx_t __parent, tx_t __ctx, tx_t tag) {
  plant_a_worker_state* s = (plant_a_worker_state*)plant_async_alloc_state(sizeof(plant_a_worker_state), "worker");
  s->tag = tag;
  return plant_async_register((tx_t)s, plant_a_worker_step, __parent, __ctx, 1, -1, -1, 0, "worker");
}

static int plant_a_worker_step(tx_t st) {
  plant_a_worker_state* s = (plant_a_worker_state*)st;
  if (s->__pc > 0) plant_async_await_result(st);
  tx_t tag = s->tag;
  switch (s->__pc) {
    case 0: goto plant_a_worker_L0;
  }
  plant_a_worker_L0:
    plant_iReport_print(get_report(), _cat("work ", tag));
  plant_async_finish(st, "worker");
  return 1;
}

tx_t main() {
  tx_t c = "";
  tx_t n = "";
    c = ffi_ctx_make(1, 64, "wctx");
    plant_async_start_in(c, worker, "a");
    worker(0, 0, "b");
    n = ffi_ctx_tasks(c);
    plant_iReport_print(get_report(), _cat("in ctx: ", n));
  plant_async_drain();
  return main;
}
