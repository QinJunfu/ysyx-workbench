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

typedef enum {
  FORMAT_LENGTH_NONE,
  FORMAT_LENGTH_LONG,
  FORMAT_LENGTH_LONG_LONG,
} FormatLength;

static int decimal_digits(unsigned long long value) {
  int digits = 1;

  while (value >= 10) {
    value /= 10;
    digits++;
  }
  return digits;
}

static int hexadecimal_digits(unsigned long long value) {
  int digits = 1;

  while (value >= 16) {
    value /= 16;
    digits++;
  }
  return digits;
}

static void emit_unsigned(FormatOutput *output, unsigned long long value, int base) {
  static const char digits[] = "0123456789abcdef";
  unsigned long long divisor = 1;

  while (value / divisor >= (unsigned long long)base) {
    divisor *= (unsigned long long)base;
  }

  do {
    emit_char(output, digits[value / divisor]);
    value %= divisor;
    divisor /= (unsigned long long)base;
  } while (divisor != 0);
}

static void emit_number(FormatOutput *output, unsigned long long value, int base,
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

    FormatLength length = FORMAT_LENGTH_NONE;
    if (*fmt == 'l') {
      fmt++;
      length = FORMAT_LENGTH_LONG;
      if (*fmt == 'l') {
        fmt++;
        length = FORMAT_LENGTH_LONG_LONG;
      }
    }

    char specifier = *fmt++;
    switch (specifier) {
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
        long long value;
        switch (length) {
          case FORMAT_LENGTH_NONE: value = va_arg(ap, int); break;
          case FORMAT_LENGTH_LONG: value = va_arg(ap, long); break;
          case FORMAT_LENGTH_LONG_LONG: value = va_arg(ap, long long); break;
        }
        bool negative = value < 0;
        unsigned long long magnitude = negative ? 0ull - (unsigned long long)value : (unsigned long long)value;
        emit_number(output, magnitude, 10, negative, width, zero_padded);
        break;
      }
      case 'u':
      case 'x': {
        unsigned long long value;
        switch (length) {
          case FORMAT_LENGTH_NONE: value = va_arg(ap, unsigned int); break;
          case FORMAT_LENGTH_LONG: value = va_arg(ap, unsigned long); break;
          case FORMAT_LENGTH_LONG_LONG: value = va_arg(ap, unsigned long long); break;
        }
        emit_number(output, value, specifier == 'u' ? 10 : 16, false, width, zero_padded);
        break;
      }
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

typedef struct {
  char *out;
  size_t size;
  size_t count;
} BoundedStringOutput;

static void bounded_string_output(void *context, char ch) {
  BoundedStringOutput *output = context;

  if (output->count + 1 < output->size) {
    output->out[output->count] = ch;
  }
  output->count++;
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
  va_list ap;
  va_start(ap, fmt);
  int result = vsnprintf(out, n, fmt, ap);
  va_end(ap);
  return result;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  BoundedStringOutput buffer = {
    .out = out,
    .size = n,
    .count = 0,
  };
  FormatOutput output = {
    .putc = bounded_string_output,
    .context = &buffer,
    .count = 0,
  };
  int result = format(&output, fmt, ap);

  if (n != 0) {
    out[buffer.count < n ? buffer.count : n - 1] = '\0';
  }
  return result;
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
