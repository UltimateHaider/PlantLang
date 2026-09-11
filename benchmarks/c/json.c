/* Baseline: 5000 JSON parse+stringify cycles via the native runtime
   (same json_parse/json_stringify used by the plant benchmark) */
#include <plant_compat.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char* doc = "{\"name\": \"bench\", \"n\": 42, \"ok\": true, \"tags\": [\"a\", \"b\", \"c\"], \"addr\": {\"city\": \"Zurich\", \"zip\": 8000}}";
    tx_t out = "";
    for (long i = 0; i < 5000; i++) {
        tx_t j = json_parse((tx_t)doc);
        tx_t o = json_stringify(j);
        out = o;
    }
    printf("jsonlen=%zu\n", strlen(_S(out)));
    return 0;
}
