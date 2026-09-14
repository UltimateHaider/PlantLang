#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#define PLANT_ENUM_Color 1
typedef enum {
  RED,
  GREEN,
  BLUE
} Color;
#endif
/*__PLANT_TYPES_END__*/

tx_t greet(tx_t __parent, tx_t __ctx, tx_t c);
tx_t phase2(tx_t __parent, tx_t __ctx, tx_t tag);
tx_t main();


static int plant_a_greet_step(tx_t st);

typedef struct {
  tx_t __self;
  long __pc;
  tx_t c;
} plant_a_greet_state;

tx_t greet(tx_t __parent, tx_t __ctx, tx_t c) {
  plant_a_greet_state* s = (plant_a_greet_state*)plant_async_alloc_state(sizeof(plant_a_greet_state), "greet");
  s->c = c;
  return plant_async_register((tx_t)s, plant_a_greet_step, __parent, __ctx, 1, -1, -1, 0, "greet");
}

static int plant_a_greet_step(tx_t st) {
  plant_a_greet_state* s = (plant_a_greet_state*)st;
  if (s->__pc > 0) plant_async_await_result(st);
  tx_t c = s->c;
  switch (s->__pc) {
    case 0: goto plant_a_greet_L0;
    case 1: goto plant_a_greet_L1;
  }
  plant_a_greet_L0:
    plant_iReport_print(get_report(), _cat("color=", _from_enum(c, "RED,GREEN,BLUE")));
  s->c = c;
  s->__pc = 1;
  plant_async_suspend(st, phase2(st, 0, c));
  return 0;
  plant_a_greet_L1:
    plant_iReport_print(get_report(), _from_enum(c, "RED,GREEN,BLUE"));
  plant_async_finish(st, "greet");
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
    plant_iReport_print(get_report(), _cat("p2 ", _from_enum(tag, "RED,GREEN,BLUE")));
    plant_async_finish(st, "done");
  return 1;
  plant_async_finish(st, "phase2");
  return 1;
}

tx_t main() {
    tx_t c = GREEN;
    greet(0, 0, c);
  plant_async_drain();
  return main;
}
