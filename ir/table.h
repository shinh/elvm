#ifndef ELVM_TABLE_H_
#define ELVM_TABLE_H_

#include <stdbool.h>

typedef struct Table_ {
  const char* key;
  const void* value;
  struct Table_* next;
} Table;

Table* table_add(Table* tbl, const char* key, const void* value);

bool table_get(Table* tbl, const char* key, const void** value);

#endif  // ELVM_TABLE_H_
