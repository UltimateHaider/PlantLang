#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t main();


tx_t main() {
    plant_iReport_print(get_report(), _cat("s0=", plant_map_to_string ( ffi_weather_status ( ) )));
    {
      PlantWeather __w1 = {0};
      plant_weather_enter(&__w1, 1);
      if (setjmp(__w1.buf) == 0) {
    plant_throw("LOCK_STORM", "lock");
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        if (plant_storm_match(__et, "LOCK_STORM")) {
          plant_weather_shelter_enter(&__w1);
          __w1.handled = 1;
    long t1 = ffi_arc_alloc ( 32 );
    long t2 = ffi_arc_alloc ( 48 );
    plant_iReport_print(get_report(), _cat4("reg_t=", ffi_weather_register ( t1 ), ",", ffi_weather_register ( t2 )));
    plant_iReport_print(get_report(), _cat("sin=", plant_map_to_string ( ffi_weather_status ( ) )));
          plant_storm_release(__ev);
          plant_weather_shelter_leave(&__w1);
        }
        plant_weather_handling_end(&__w1);
      }
    plant_iReport_print(get_report(), "calm_after_handler");
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), _cat("s_after=", plant_map_to_string ( ffi_weather_status ( ) )));
  return main;
}
