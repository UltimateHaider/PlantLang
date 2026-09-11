#include <plant_compat.h>

void get_cli_arg(long idx) {
}
int main(int argc, char **argv) {
  plant_init_cli(argc, argv);
  source_path = get_cli_arg(0);
  plant_print(_cat(_cat(, source_path));
  exists = fs_EXISTS(source_path);
  if (!exists) {
      plant_print(_cat(Ê=üc, source_path));
  }
  source_text = fs_READ(source_path);
  plant_print("tokenizing...");
  tokens = scan_tokens(source_text);
  plant_print("parsing...");
  program_ast = parse_program(tokens);
  body = _map_get(program_ast, "body");
  plant_print("generating C...");
  c_code = generate_c(body);
  out_path = get_cli_arg(1);
  if (out_path == "") {
      out_path = strings_REPLACE(source_path, ".plant", ".c");
  }
  written = fs_WRITE(out_path, c_code);
  c_len = strings_LENGTH(c_code);
  plant_print(_cat(_cat(_cat(_cat(, _cat(), _cat(), out_path));
  return 0;
}
