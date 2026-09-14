#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#define PLANT_ENUM_Level 1
typedef enum {
  LOW,
  MEDIUM,
  HIGH
} Level;
#endif
/*__PLANT_TYPES_END__*/

tx_t report(tx_t __parent, tx_t __ctx, tx_t lv);
tx_t main();


static int plant_a_report_step(tx_t st);

typedef struct {
  tx_t __self;
  long __pc;
  tx_t lv;
  tx_t mine;
  tx_t next;
} plant_a_report_state;

tx_t report(tx_t __parent, tx_t __ctx, tx_t lv) {
  plant_a_report_state* s = (plant_a_report_state*)plant_async_alloc_state(sizeof(plant_a_report_state), "report");
  s->lv = lv;
  return plant_async_register((tx_t)s, plant_a_report_step, __parent, __ctx, 1, -1, -1, 0, "report");
}

static int plant_a_report_step(tx_t st) {
  plant_a_report_state* s = (plant_a_report_state*)st;
  if (s->__pc > 0) plant_async_await_result(st);
  tx_t lv = s->lv;
  tx_t mine = s->mine;
  tx_t next = s->next;
  switch (s->__pc) {
    case 0: goto plant_a_report_L0;
  }
  plant_a_report_L0:
    plant_iReport_print(get_report(), _from_enum(lv, "LOW,MEDIUM,HIGH"));
    mine = lv;
    plant_iReport_print(get_report(), _from_enum(mine, "LOW,MEDIUM,HIGH"));
    plant_msleep(0);
    plant_iReport_print(get_report(), _from_enum(lv, "LOW,MEDIUM,HIGH"));
    next = MEDIUM;
    plant_iReport_print(get_report(), _from_enum(next, "LOW,MEDIUM,HIGH"));
  plant_async_finish(st, "report");
  return 1;
}

tx_t main() {
    tx_t l = HIGH;
    report(0, 0, l);
  plant_async_drain();
  return main;
}
