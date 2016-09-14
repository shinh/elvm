#include <ir/table.h>

#include <stdlib.h>
#include <string.h>

Table* table_add(Table* tbl, const char* key, const void* value) {
  Table* ntbl = malloc(sizeof(Table));
  ntbl->next = tbl;
  ntbl->key = key;
  ntbl->value = value;
  return ntbl;
}

bool table_get(Table* tbl, const char* key, const void** value) {
  for (; tbl; tbl = tbl->next) {
    if (!strcmp(tbl->key, key)) {
      *value = tbl->value;
      return true;
    }
  }
  return false;
}
