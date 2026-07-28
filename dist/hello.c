#include <plant_runtime.h>
#include <string.h>
#include <stdio.h>
static void plant_print(const char *s) { printf("%s\n", s); }

int main() {
  plant_print("Hello_World!");
  return 0;
}
