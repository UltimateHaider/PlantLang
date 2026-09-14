#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#define PLANT_ENUM_Suit 1
typedef enum {
  HEARTS,
  SPADES,
  DIAMONDS,
  CLUBS
} Suit;
#endif
/*__PLANT_TYPES_END__*/

tx_t tag(tx_t __parent, tx_t __ctx, tx_t su, long n);
tx_t echo(tx_t __parent, tx_t __ctx, tx_t su);
tx_t main();


static int plant_a_tag_step(tx_t st);

typedef struct {
  tx_t __self;
  long __pc;
  tx_t su;
  long n;
} plant_a_tag_state;

tx_t tag(tx_t __parent, tx_t __ctx, tx_t su, long n) {
  plant_a_tag_state* s = (plant_a_tag_state*)plant_async_alloc_state(sizeof(plant_a_tag_state), "tag");
  s->su = su;
  s->n = n;
  return plant_async_register((tx_t)s, plant_a_tag_step, __parent, __ctx, 1, -1, -1, 0, "tag");
}

static int plant_a_tag_step(tx_t st) {
  plant_a_tag_state* s = (plant_a_tag_state*)st;
  if (s->__pc > 0) plant_async_await_result(st);
  tx_t su = s->su;
  long n = s->n;
  switch (s->__pc) {
    case 0: goto plant_a_tag_L0;
    case 1: goto plant_a_tag_L1;
  }
  plant_a_tag_L0:
    plant_iReport_print(get_report(), _cat4("suit ", _from_enum(su, "HEARTS,SPADES,DIAMONDS,CLUBS"), " n=", _from_long(n)));
  s->su = su;
  s->n = n;
  s->__pc = 1;
  plant_async_suspend(st, echo(st, 0, su));
  return 0;
  plant_a_tag_L1:
    plant_iReport_print(get_report(), _cat("done ", _from_enum(su, "HEARTS,SPADES,DIAMONDS,CLUBS")));
  plant_async_finish(st, "tag");
  return 1;
}

static int plant_a_echo_step(tx_t st);

typedef struct {
  tx_t __self;
  long __pc;
  tx_t su;
} plant_a_echo_state;

tx_t echo(tx_t __parent, tx_t __ctx, tx_t su) {
  plant_a_echo_state* s = (plant_a_echo_state*)plant_async_alloc_state(sizeof(plant_a_echo_state), "echo");
  s->su = su;
  return plant_async_register((tx_t)s, plant_a_echo_step, __parent, __ctx, 1, -1, -1, 0, "echo");
}

static int plant_a_echo_step(tx_t st) {
  plant_a_echo_state* s = (plant_a_echo_state*)st;
  if (s->__pc > 0) plant_async_await_result(st);
  tx_t su = s->su;
  switch (s->__pc) {
    case 0: goto plant_a_echo_L0;
  }
  plant_a_echo_L0:
    plant_iReport_print(get_report(), _cat("got ", _from_enum(su, "HEARTS,SPADES,DIAMONDS,CLUBS")));
    plant_async_finish(st, "ok");
  return 1;
  plant_async_finish(st, "echo");
  return 1;
}

tx_t main() {
    tx_t su = DIAMONDS;
    tag(0, 0, su, 3);
  plant_async_drain();
  return main;
}
