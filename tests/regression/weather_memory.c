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
      plant_weather_enter(&__w1, 0);
      if (setjmp(__w1.buf) == 0) {
    long a = ffi_arc_alloc ( 128 );
    long b = ffi_arc_alloc ( 256 );
    plant_iReport_print(get_report(), _cat4("reg=", ffi_weather_register ( a ), ",", ffi_weather_register ( b )));
    plant_iReport_print(get_report(), _cat("sin=", plant_map_to_string ( ffi_weather_status ( ) )));
      } else {
        const char* __et = plant_exc_type();
        const char* __em = plant_exc_msg();
        tx_t __ev = plant_exc_val();
        plant_weather_leave(&__w1);
        plant_weather_handling_begin(&__w1);
        plant_weather_handling_end(&__w1);
      }
    plant_iReport_print(get_report(), "calm");
      plant_calm(&__w1);
    }
    plant_iReport_print(get_report(), _cat("s_after=", plant_map_to_string ( ffi_weather_status ( ) )));
    plant_iReport_print(get_report(), _cat("heap_after=", plant_map_to_string ( ffi_persist_status ( ) )));
  return main;
}
