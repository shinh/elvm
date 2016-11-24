#include <ir/ir.h>
#include <target/util.h>

const char *alphabet[] = {"_", "0", "1"};
const int alphabet_size = sizeof(alphabet)/sizeof(*alphabet);

void target_tm(Module* module) {
  for (Data* data = module->data; data; data = data->next) {
  }
  for (Inst* inst = module->text; inst; inst = inst->next) {
  }
}
