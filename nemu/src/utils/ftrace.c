#include <utils.h>

#ifdef CONFIG_FTRACE

#include <elf.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  vaddr_t start;
  uint32_t size;
  char *name;
} FtraceFunction;

static FtraceFunction *functions = NULL;
static size_t nr_functions = 0;
static int call_depth = 0;

static bool range_in_file(size_t offset, size_t length, size_t file_size) {
  return offset <= file_size && length <= file_size - offset;
}

static bool ftrace_error(const char *elf_file, const char *reason) {
  Assert(0, "FTrace: invalid ELF '%s': %s", elf_file, reason);
  return false;
}

static bool read_section(const uint8_t *image, size_t image_size,
    const Elf32_Ehdr *ehdr, size_t index, Elf32_Shdr *section) {
  if (index > (SIZE_MAX - (size_t)ehdr->e_shoff) / sizeof(*section)) {
    return false;
  }
  size_t offset = (size_t)ehdr->e_shoff + index * sizeof(*section);
  if (!range_in_file(offset, sizeof(*section), image_size)) {
    return false;
  }
  memcpy(section, image + offset, sizeof(*section));
  return true;
}

static void free_functions(void) {
  for (size_t i = 0; i < nr_functions; i ++) {
    free(functions[i].name);
  }
  free(functions);
  functions = NULL;
  nr_functions = 0;
  call_depth = 0;
}

static bool get_symbol_table(const uint8_t *image, size_t image_size,
    const Elf32_Ehdr *ehdr, size_t index, Elf32_Shdr *symtab,
    Elf32_Shdr *strtab, const char *elf_file) {
  if (!read_section(image, image_size, ehdr, index, symtab)) {
    return ftrace_error(elf_file, "cannot read a symbol table section header");
  }
  if (symtab->sh_entsize != sizeof(Elf32_Sym) ||
      symtab->sh_size % symtab->sh_entsize != 0) {
    return ftrace_error(elf_file, "malformed .symtab entry size");
  }
  if (!range_in_file(symtab->sh_offset, symtab->sh_size, image_size)) {
    return ftrace_error(elf_file, ".symtab extends past the end of the file");
  }
  if (symtab->sh_link >= ehdr->e_shnum ||
      !read_section(image, image_size, ehdr, symtab->sh_link, strtab)) {
    return ftrace_error(elf_file, ".symtab has an invalid string table link");
  }
  if (strtab->sh_type != SHT_STRTAB ||
      !range_in_file(strtab->sh_offset, strtab->sh_size, image_size)) {
    return ftrace_error(elf_file, ".symtab has an invalid string table");
  }
  return true;
}

static bool valid_symbol_name(const uint8_t *image, const Elf32_Shdr *strtab,
    const Elf32_Sym *symbol) {
  if (symbol->st_name >= strtab->sh_size) {
    return false;
  }
  const char *name = (const char *)image + strtab->sh_offset + symbol->st_name;
  size_t available = strtab->sh_size - symbol->st_name;
  return memchr(name, '\0', available) != NULL;
}

static bool count_functions(const uint8_t *image, size_t image_size,
    const Elf32_Ehdr *ehdr, const char *elf_file, size_t *count) {
  *count = 0;
  bool found_symtab = false;

  for (size_t i = 0; i < ehdr->e_shnum; i ++) {
    Elf32_Shdr section;
    if (!read_section(image, image_size, ehdr, i, &section)) {
      return ftrace_error(elf_file, "cannot read a section header");
    }
    if (section.sh_type != SHT_SYMTAB) {
      continue;
    }

    found_symtab = true;
    Elf32_Shdr symtab;
    Elf32_Shdr strtab;
    if (!get_symbol_table(image, image_size, ehdr, i, &symtab, &strtab, elf_file)) {
      return false;
    }

    size_t symbol_count = symtab.sh_size / sizeof(Elf32_Sym);
    for (size_t j = 0; j < symbol_count; j ++) {
      Elf32_Sym symbol;
      memcpy(&symbol, image + symtab.sh_offset + j * sizeof(symbol), sizeof(symbol));
      if (ELF32_ST_TYPE(symbol.st_info) != STT_FUNC || symbol.st_size == 0) {
        continue;
      }
      if (!valid_symbol_name(image, &strtab, &symbol)) {
        return ftrace_error(elf_file, "a function symbol has an invalid name offset");
      }
      if (*count == SIZE_MAX) {
        return ftrace_error(elf_file, "too many function symbols");
      }
      (*count) ++;
    }
  }

  if (!found_symtab) {
    return ftrace_error(elf_file, "no .symtab section was found");
  }
  return true;
}

static bool copy_functions(const uint8_t *image, size_t image_size,
    const Elf32_Ehdr *ehdr, const char *elf_file) {
  size_t function_index = 0;

  for (size_t i = 0; i < ehdr->e_shnum; i ++) {
    Elf32_Shdr section;
    if (!read_section(image, image_size, ehdr, i, &section)) {
      return ftrace_error(elf_file, "cannot read a section header");
    }
    if (section.sh_type != SHT_SYMTAB) {
      continue;
    }

    Elf32_Shdr symtab;
    Elf32_Shdr strtab;
    if (!get_symbol_table(image, image_size, ehdr, i, &symtab, &strtab, elf_file)) {
      return false;
    }

    size_t symbol_count = symtab.sh_size / sizeof(Elf32_Sym);
    for (size_t j = 0; j < symbol_count; j ++) {
      Elf32_Sym symbol;
      memcpy(&symbol, image + symtab.sh_offset + j * sizeof(symbol), sizeof(symbol));
      if (ELF32_ST_TYPE(symbol.st_info) != STT_FUNC || symbol.st_size == 0) {
        continue;
      }
      if (!valid_symbol_name(image, &strtab, &symbol)) {
        return ftrace_error(elf_file, "a function symbol has an invalid name offset");
      }

      const char *name = (const char *)image + strtab.sh_offset + symbol.st_name;
      size_t name_length = strlen(name);
      if (name_length == SIZE_MAX) {
        return ftrace_error(elf_file, "a function name is too long");
      }
      char *name_copy = malloc(name_length + 1);
      if (name_copy == NULL) {
        return ftrace_error(elf_file, "out of memory while copying function names");
      }
      memcpy(name_copy, name, name_length + 1);

      functions[function_index].start = symbol.st_value;
      functions[function_index].size = symbol.st_size;
      functions[function_index].name = name_copy;
      function_index ++;
    }
  }

  Assert(function_index == nr_functions,
      "FTrace: symbol table changed while parsing '%s'", elf_file);
  return true;
}

static bool parse_elf(const uint8_t *image, size_t image_size,
    const char *elf_file) {
  if (image_size < sizeof(Elf32_Ehdr)) {
    return ftrace_error(elf_file, "file is smaller than an ELF32 header");
  }

  Elf32_Ehdr ehdr;
  memcpy(&ehdr, image, sizeof(ehdr));
  if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0 ||
      ehdr.e_ident[EI_CLASS] != ELFCLASS32 ||
      ehdr.e_ident[EI_DATA] != ELFDATA2LSB ||
      ehdr.e_ident[EI_VERSION] != EV_CURRENT ||
      ehdr.e_version != EV_CURRENT || ehdr.e_machine != EM_RISCV) {
    return ftrace_error(elf_file, "expected an ELF32 little-endian RISC-V file");
  }
  if (ehdr.e_ehsize != sizeof(Elf32_Ehdr) ||
      ehdr.e_shentsize != sizeof(Elf32_Shdr) ||
      ehdr.e_shnum == 0 || ehdr.e_shnum == SHN_XINDEX ||
      ehdr.e_shoff == 0) {
    return ftrace_error(elf_file, "malformed section header table");
  }
  if (ehdr.e_shstrndx != SHN_UNDEF && ehdr.e_shstrndx >= ehdr.e_shnum) {
    return ftrace_error(elf_file, "invalid section name string table index");
  }

  size_t section_table_offset = ehdr.e_shoff;
  size_t section_table_size = (size_t)ehdr.e_shnum * sizeof(Elf32_Shdr);
  if (!range_in_file(section_table_offset, section_table_size, image_size)) {
    return ftrace_error(elf_file, "section header table extends past the end of the file");
  }

  size_t function_count;
  if (!count_functions(image, image_size, &ehdr, elf_file, &function_count)) {
    return false;
  }
  if (function_count > SIZE_MAX / sizeof(*functions)) {
    return ftrace_error(elf_file, "too many function symbols");
  }

  functions = function_count == 0 ? NULL : calloc(function_count, sizeof(*functions));
  if (function_count != 0 && functions == NULL) {
    return ftrace_error(elf_file, "out of memory while reading the symbol table");
  }
  nr_functions = function_count;
  if (!copy_functions(image, image_size, &ehdr, elf_file)) {
    free_functions();
    return false;
  }
  return true;
}

void init_ftrace(const char *elf_file) {
  Assert(elf_file != NULL,
      "FTrace is enabled, but no ELF file was supplied. Use -f FILE or --elf=FILE");
  if (elf_file == NULL) {
    return;
  }

  FILE *fp = fopen(elf_file, "rb");
  Assert(fp != NULL, "FTrace: cannot open ELF file '%s'", elf_file);
  if (fp == NULL) {
    return;
  }
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    ftrace_error(elf_file, "cannot seek to the end of the file");
    return;
  }
  long file_size = ftell(fp);
  if (file_size <= 0 || (uintmax_t)file_size > SIZE_MAX) {
    fclose(fp);
    ftrace_error(elf_file, "invalid file size");
    return;
  }
  if (fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    ftrace_error(elf_file, "cannot rewind the file");
    return;
  }

  size_t image_size = (size_t)file_size;
  uint8_t *image = malloc(image_size);
  if (image == NULL) {
    fclose(fp);
    ftrace_error(elf_file, "out of memory while reading the file");
    return;
  }
  size_t bytes_read = fread(image, 1, image_size, fp);
  fclose(fp);
  if (bytes_read != image_size) {
    free(image);
    ftrace_error(elf_file, "cannot read the complete file");
    return;
  }

  free_functions();
  bool parsed = parse_elf(image, image_size, elf_file);
  free(image);
  if (!parsed) {
    return;
  }
  Log("FTrace: loaded %zu function symbols from %s", nr_functions, elf_file);
}

static const FtraceFunction *find_function(vaddr_t address) {
  uint64_t target = address;
  for (size_t i = 0; i < nr_functions; i ++) {
    uint64_t start = functions[i].start;
    uint64_t end = start + functions[i].size;
    if (target >= start && target < end) {
      return &functions[i];
    }
  }
  return NULL;
}

static const FtraceFunction *find_function_start(vaddr_t address) {
  for (size_t i = 0; i < nr_functions; i ++) {
    if (functions[i].start == address) {
      return &functions[i];
    }
  }
  return NULL;
}

static void write_indent(void) {
  for (int i = 0; i < call_depth; i ++) {
    trace_write("  ");
  }
}

static void write_call(vaddr_t pc, vaddr_t target, const char *kind,
    const FtraceFunction *function) {
  trace_write(FMT_WORD ": ", pc);
  write_indent();
  trace_write("%s [%s@" FMT_WORD "]\n", kind,
      function == NULL ? "???" : function->name, target);
}

static void write_return(vaddr_t pc) {
  const FtraceFunction *function = find_function(pc);
  trace_write(FMT_WORD ": ", pc);
  write_indent();
  trace_write("ret  [%s]\n", function == NULL ? "???" : function->name);
}

void ftrace(vaddr_t pc, vaddr_t target, uint32_t inst, int rd, int rs1,
    word_t imm) {
  uint32_t opcode = inst & 0x7f;
  bool is_jal = opcode == 0x6f;
  bool is_jalr = opcode == 0x67;
  bool link_register = rd == 1 || rd == 5;
  bool return_instruction = is_jalr && rd == 0 &&
      (rs1 == 1 || rs1 == 5) && imm == 0;

  if (return_instruction) {
    if (call_depth > 0) {
      call_depth --;
    }
    write_return(pc);
    return;
  }
  if ((is_jal || is_jalr) && link_register) {
    write_call(pc, target, "call", find_function(target));
    if (call_depth < INT32_MAX) {
      call_depth ++;
    }
    return;
  }

  // A jump to another function without a link register preserves the caller.
  if ((is_jal || is_jalr) && rd == 0) {
    const FtraceFunction *function = find_function_start(target);
    if (function != NULL) {
      write_call(pc, target, "tail", function);
    }
  }
}

#else

void init_ftrace(const char *elf_file) {
  (void)elf_file;
}

void ftrace(vaddr_t pc, vaddr_t target, uint32_t inst, int rd, int rs1,
    word_t imm) {
  (void)pc;
  (void)target;
  (void)inst;
  (void)rd;
  (void)rs1;
  (void)imm;
}

#endif
