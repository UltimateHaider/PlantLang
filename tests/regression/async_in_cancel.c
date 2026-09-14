#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t worker(tx_t __parent, tx_t __ctx, tx_t tag);
tx_t sleeper(tx_t __parent, tx_t __ctx, tx_t tag);
tx_t phase2(tx_t __parent, tx_t __ctx, tx_t tag);
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

static int plant_a_sleeper_step(tx_t st);

typedef struct {
  tx_t __self;
  long __pc;
  tx_t tag;
} plant_a_sleeper_state;

tx_t sleeper(tx_t __parent, tx_t __ctx, tx_t tag) {
  plant_a_sleeper_state* s = (plant_a_sleeper_state*)plant_async_alloc_state(sizeof(plant_a_sleeper_state), "sleeper");
  s->tag = tag;
  return plant_async_register((tx_t)s, plant_a_sleeper_step, __parent, __ctx, 1, -1, -1, 0, "sleeper");
}

static int plant_a_sleeper_step(tx_t st) {
  plant_a_sleeper_state* s = (plant_a_sleeper_state*)st;
  if (s->__pc > 0) plant_async_await_result(st);
  tx_t tag = s->tag;
  switch (s->__pc) {
    case 0: goto plant_a_sleeper_L0;
    case 1: goto plant_a_sleeper_L1;
  }
  plant_a_sleeper_L0:
    plant_iReport_print(get_report(), _cat("start ", tag));
  s->tag = tag;
  s->__pc = 1;
  plant_async_suspend(st, phase2(st, 0, tag));
  return 0;
  plant_a_sleeper_L1:
    plant_iReport_print(get_report(), _cat("end ", tag));
  plant_async_finish(st, "sleeper");
  return 1;
}

static int plant_a_phase2_step(tx_t st);

typedef struct {
  tx_t __self;
  long __pc;
  tx_t tag;
} plant_a_phase2_state;

tx_t phase2(tx_t __parent, tx_t __ctx, tx_t tag) {
  plant_a_phase2_state* s = (plant_a_phase2_state*)plant_async_alloc_state(sizeof(plant_a_phase2_state), "phase2");
  s->tag = tag;
  return plant_async_register((tx_t)s, plant_a_phase2_step, __parent, __ctx, 1, -1, -1, 0, "phase2");
}

static int plant_a_phase2_step(tx_t st) {
  plant_a_phase2_state* s = (plant_a_phase2_state*)st;
  if (s->__pc > 0) plant_async_await_result(st);
  tx_t tag = s->tag;
  switch (s->__pc) {
    case 0: goto plant_a_phase2_L0;
  }
  plant_a_phase2_L0:
    plant_iReport_print(get_report(), _cat("p2 ", tag));
    plant_async_finish(st, "done");
  return 1;
  plant_async_finish(st, "phase2");
  return 1;
}

tx_t main() {
  tx_t c = "";
  tx_t n = "";
  tx_t h = "";
  tx_t n2 = "";
  tx_t n3 = "";
    c = ffi_ctx_make(1, 64, "wctx");
    plant_async_start_in(c, worker, "a");
    worker(0, 0, "b");
    n = ffi_ctx_tasks(c);
    plant_iReport_print(get_report(), _cat("in ctx: ", n));
    h = sleeper(0, c, "x");
    n2 = ffi_ctx_tasks(c);
    plant_iReport_print(get_report(), _cat("after reap: ", n2));
    plant_async_cancel_in(c, h);
    n3 = ffi_ctx_tasks(c);
    plant_iReport_print(get_report(), _cat("after cancel: ", n3));
  plant_async_drain();
  return main;
}
