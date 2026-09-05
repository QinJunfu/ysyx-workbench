#include <klib.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

typedef void (*format_putc_t)(void *context, char ch);

typedef struct {
  format_putc_t putc;
  void *context;
  int count;
} FormatOutput;

static void emit_char(FormatOutput *output, char ch) {
  output->putc(output->context, ch);
  output->count++;
}

static int decimal_digits(unsigned int value) {
  int digits = 1;

  while (value >= 10) {
    value /= 10;
    digits++;
  }
  return digits;
}

static int hexadecimal_digits(unsigned int value) {
  int digits = 1;

  while (value >= 16) {
    value /= 16;
    digits++;
  }
  return digits;
}

static void emit_unsigned(FormatOutput *output, unsigned int value, int base) {
  static const char digits[] = "0123456789abcdef";
  unsigned int divisor = 1;

  while (value / divisor >= (unsigned int)base) {
    divisor *= (unsigned int)base;
  }

  do {
    emit_char(output, digits[value / divisor]);
    value %= divisor;
    divisor /= (unsigned int)base;
  } while (divisor != 0);
}

static void emit_number(FormatOutput *output, unsigned int value, int base,
                        bool negative, int width, bool zero_padded) {
  int digits = base == 10 ? decimal_digits(value) : hexadecimal_digits(value);
  int length = digits + (negative ? 1 : 0);

  if (!zero_padded) {
    while (length < width) {
      emit_char(output, ' ');
      length++;
    }
  }

  if (negative) {
    emit_char(output, '-');
  }

  if (zero_padded) {
    while (length < width) {
      emit_char(output, '0');
      length++;
    }
  }

  emit_unsigned(output, value, base);
}

static int format(FormatOutput *output, const char *fmt, va_list ap) {
  while (*fmt != '\0') {
    if (*fmt != '%') {
      emit_char(output, *fmt++);
      continue;
    }

    fmt++;
    bool zero_padded = false;
    int width = 0;

    if (*fmt == '0') {
      zero_padded = true;
      fmt++;
    }

    while (*fmt >= '0' && *fmt <= '9') {
      width = width * 10 + (*fmt++ - '0');
    }

    switch (*fmt++) {
      case '%':
        emit_char(output, '%');
        break;
      case 's': {
        const char *str = va_arg(ap, const char *);
        if (str == NULL) {
          str = "(null)";
        }
        while (*str != '\0') {
          emit_char(output, *str++);
        }
        break;
      }
      case 'c':
        emit_char(output, (char)va_arg(ap, int));
        break;
      case 'd': {
        int value = va_arg(ap, int);
        bool negative = value < 0;
        unsigned int magnitude = negative ? 0u - (unsigned int)value : (unsigned int)value;
        emit_number(output, magnitude, 10, negative, width, zero_padded);
        break;
      }
      case 'x':
        emit_number(output, va_arg(ap, unsigned int), 16, false, width, zero_padded);
        break;
      default:
        panic("Unsupported conversion specifier");
    }
  }

  return output->count;
}

static void putch_output(void *context, char ch) {
  (void)context;
  putch(ch);
}

static void string_output(void *context, char ch) {
  char **out = context;
  **out = ch;
  (*out)++;
}

int vprintf(const char *fmt, va_list ap) {
  FormatOutput output = {
    .putc = putch_output,
    .context = NULL,
    .count = 0,
  };
  return format(&output, fmt, ap);
}

int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int result = vprintf(fmt, ap);
  va_end(ap);
  return result;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  char *cursor = out;
  FormatOutput output = {
    .putc = string_output,
    .context = &cursor,
    .count = 0,
  };
  int result = format(&output, fmt, ap);
  *cursor = '\0';
  return result;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int result = vsprintf(out, fmt, ap);
  va_end(ap);
  return result;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  panic("Not implemented");
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

int __am_vsscanf_internal(const char *str, const char **end_pstr, const char *fmt, va_list ap) {
  const char *pstr = str;
  const char *pfmt = fmt;
  int item = -1;
  while (*pfmt) {
    char ch = *pfmt ++;
    if (isspace(ch)) {
      for (ch = *pfmt; isspace(ch); ch = *(++ pfmt));
      for (ch = *pstr; isspace(ch); ch = *(++ pstr));
      item ++;
      continue;
    }
    switch (ch) {
      case '%': break;
      default:
        if (*pstr == ch) { // match
          pstr ++;
          item ++;
          continue;
        }
        goto end; // fail
    }

    char *p;
    ch = *pfmt ++;
    switch (ch) {
      // conversion specifier
      case 'd':
        *(va_arg(ap, int *)) = strtol(pstr, &p, 10);
        if (p == pstr) goto end; // fail
        pstr = p;
        item ++;
        break;

      case 'c':
        *(va_arg(ap, char *)) = *pstr ++;
        item ++;
        break;

      default:
        printf("Unsupported conversion specifier '%c'\n", ch);
        assert(0);
    }
  }

end:
  if (end_pstr) {
    *end_pstr = pstr;
  }
  return item;
}

int vsscanf(const char *str, const char *fmt, va_list ap) {
  return __am_vsscanf_internal(str, NULL, fmt, ap);
}

int sscanf(const char *str, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vsscanf(str, fmt, ap);
  va_end(ap);
  return r;
}

int __isoc99_sscanf(const char *str, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vsscanf(str, fmt, ap);
  va_end(ap);
  return r;
}

#endif
