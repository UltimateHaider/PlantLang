#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t greet(tx_t __parent, tx_t __ctx, tx_t tag, tx_t c);
tx_t boss(tx_t __parent, tx_t __ctx, tx_t c);
tx_t main();


static int plant_a_greet_step(tx_t st);

typedef struct {
  tx_t __self;
  long __pc;
  tx_t tag;
  tx_t c;
  tx_t n;
} plant_a_greet_state;

tx_t greet(tx_t __parent, tx_t __ctx, tx_t tag, tx_t c) {
  plant_a_greet_state* s = (plant_a_greet_state*)plant_async_alloc_state(sizeof(plant_a_greet_state), "greet");
  s->tag = tag;
  s->c = c;
  return plant_async_register((tx_t)s, plant_a_greet_step, __parent, __ctx, 1, -1, -1, 0, "greet");
}

static int plant_a_greet_step(tx_t st) {
  plant_a_greet_state* s = (plant_a_greet_state*)st;
  if (s->__pc > 0) plant_async_await_result(st);
  tx_t tag = s->tag;
  tx_t c = s->c;
  tx_t n = s->n;
  switch (s->__pc) {
    case 0: goto plant_a_greet_L0;
  }
  plant_a_greet_L0:
    n = ffi_ctx_tasks(c);
    plant_iReport_print(get_report(), _cat("greet in ctx: ", n));
    plant_async_finish(st, "hi");
  return 1;
  plant_async_finish(st, "greet");
  return 1;
}

static int plant_a_boss_step(tx_t st);

typedef struct {
  tx_t __self;
  long __pc;
  tx_t c;
} plant_a_boss_state;

tx_t boss(tx_t __parent, tx_t __ctx, tx_t c) {
  plant_a_boss_state* s = (plant_a_boss_state*)plant_async_alloc_state(sizeof(plant_a_boss_state), "boss");
  s->c = c;
  return plant_async_register((tx_t)s, plant_a_boss_step, __parent, __ctx, 1, -1, -1, 0, "boss");
}

static int plant_a_boss_step(tx_t st) {
  plant_a_boss_state* s = (plant_a_boss_state*)st;
  if (s->__pc > 0) plant_async_await_result(st);
  tx_t c = s->c;
  switch (s->__pc) {
    case 0: goto plant_a_boss_L0;
    case 1: goto plant_a_boss_L1;
  }
  plant_a_boss_L0:
    plant_iReport_print(get_report(), "boss start");
  s->c = c;
  s->__pc = 1;
  plant_async_await_in(st, c, greet(st, c, "z", c));
  return 0;
  plant_a_boss_L1:
    plant_iReport_print(get_report(), "boss end");
  plant_async_finish(st, "boss");
  return 1;
}

tx_t main() {
  tx_t c = "";
  tx_t n = "";
    c = ffi_ctx_make(1, 64, "wctx");
    plant_async_start_in(c, boss, c);
    n = ffi_ctx_tasks(c);
    plant_iReport_print(get_report(), _cat("in ctx: ", n));
  plant_async_drain();
  return main;
}
