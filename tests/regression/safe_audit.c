#include <plant_compat.h>

/*__PLANT_TYPES_BEGIN__*/
#ifndef PLANT_TYPES_INCLUDED
#define PLANT_TYPES_INCLUDED
#endif
/*__PLANT_TYPES_END__*/

tx_t plant_main();


tx_t plant_main() {
  if (plant_boundary_block("main", "SAFE")) return "";
  plant_safe_enter("main");
  plant_safe_channel_init("main");
    plant_iReport_print(get_report(), _cat("read0=", ffi_cap_check ( "FILE_READ" )));
    plant_iReport_print(get_report(), _cat("grant=", ffi_safe_grant ( "FILE_READ" )));
    plant_iReport_print(get_report(), _cat("read1=", ffi_cap_check ( "FILE_READ" )));
    plant_iReport_print(get_report(), _cat("net=", ffi_safe_grant ( "NET_CONNECT" )));
    plant_iReport_print(get_report(), _cat("sys=", ffi_safe_grant ( "SHUTDOWN_ANY" )));
    plant_iReport_print(get_report(), _cat("execve=", ffi_safe_syscall ( "execve" )));
    plant_iReport_print(get_report(), _cat("fork=", ffi_safe_syscall ( "fork" )));
    plant_iReport_print(get_report(), _cat("ptrace=", ffi_safe_syscall ( "ptrace" )));
    plant_iReport_print(get_report(), _cat("readsc=", ffi_safe_syscall ( "read" )));
    plant_iReport_print(get_report(), _cat("chain=", ffi_audit_chain_verify ( )));
    plant_iReport_print(get_report(), _cat("tamper=", ffi_audit_tamper ( )));
    plant_iReport_print(get_report(), _cat("chain2=", ffi_audit_chain_verify ( )));
    plant_iReport_print(get_report(), ffi_audit_dump ( ));
  plant_safe_exit();
  return plant_main;
}
tx_t plant_safe_adapter_main(int argc, tx_t* argv) {
  return (tx_t)main();
}

int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  plant_safe_register("main", plant_safe_adapter_main);
  plant_maybe_run_worker();
  plant_main();
  plant_async_drain();
  return 0;
}
