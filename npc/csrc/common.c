#include "common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void npc_set_error(char *buffer, size_t buffer_size, const char *format, ...) {
  va_list arguments;

  if (buffer == NULL || buffer_size == 0) {
    return;
  }

  va_start(arguments, format);
  (void)vsnprintf(buffer, buffer_size, format, arguments);
  va_end(arguments);
}

char *npc_copy_string(const char *text) {
  size_t length;
  char *copy;

  if (text == NULL) {
    return NULL;
  }
  length = strlen(text) + 1;
  copy = (char *)malloc(length);
  if (copy != NULL) {
    memcpy(copy, text, length);
  }
  return copy;
}
