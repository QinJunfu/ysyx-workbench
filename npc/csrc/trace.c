#include "trace.h"

#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <capstone/capstone.h>
#include <generated/autoconf.h>

#include "common.h"

#define NPC_TRACE_LINE_SIZE 512u

typedef struct {
  uint32_t start;
  uint32_t end;
  char *name;
} NpcTraceFunction;

struct NpcTrace {
  int itrace_enabled;
  int mtrace_enabled;
  int ftrace_enabled;
  csh capstone;
  int capstone_ready;
  char *recent[NPC_ITRACE_DEPTH];
  size_t recent_count;
  NpcTraceFunction *functions;
  size_t function_count;
  size_t function_capacity;
  int call_depth;
};

static unsigned int npc_trace_byte_count(uint8_t mask) {
  unsigned int count;
  unsigned int index;

  count = 0;
  for (index = 0; index < 4; ++index) {
    count += (mask >> index) & 1u;
  }
  return count;
}

static int npc_trace_in_file(size_t offset, size_t length, size_t file_size) {
  return offset <= file_size && length <= file_size - offset;
}

static int npc_trace_read_section(const uint8_t *image, size_t image_size,
                                  const Elf32_Ehdr *header, unsigned int index,
                                  Elf32_Shdr *section) {
  size_t offset;

  offset = (size_t)header->e_shoff + (size_t)index * sizeof(*section);
  if (!npc_trace_in_file(offset, sizeof(*section), image_size)) {
    return 0;
  }
  memcpy(section, image + offset, sizeof(*section));
  return 1;
}

static int npc_trace_append_function(NpcTrace *trace, uint32_t start, uint32_t size,
                                     const char *name, char *error, size_t error_size) {
  NpcTraceFunction *functions;
  size_t new_capacity;
  char *name_copy;

  if (trace->function_count == trace->function_capacity) {
    new_capacity = trace->function_capacity == 0 ? 64 : trace->function_capacity * 2;
    functions = (NpcTraceFunction *)realloc(trace->functions,
                                             new_capacity * sizeof(*trace->functions));
    if (functions == NULL) {
      npc_set_error(error, error_size, "cannot allocate FTrace function table");
      return 0;
    }
    trace->functions = functions;
    trace->function_capacity = new_capacity;
  }
  name_copy = npc_copy_string(name);
  if (name_copy == NULL) {
    npc_set_error(error, error_size, "cannot allocate FTrace function name");
    return 0;
  }
  trace->functions[trace->function_count].start = start;
  trace->functions[trace->function_count].end = start + size;
  trace->functions[trace->function_count].name = name_copy;
  trace->function_count += 1;
  return 1;
}

static int npc_trace_load_elf(NpcTrace *trace, const char *path, char *error,
                              size_t error_size) {
  FILE *input;
  long raw_size;
  size_t image_size;
  uint8_t *image;
  Elf32_Ehdr header;
  unsigned int section_index;

  input = fopen(path, "rb");
  if (input == NULL) {
    npc_set_error(error, error_size, "cannot open ELF '%s'", path);
    return 0;
  }
  if (fseek(input, 0, SEEK_END) != 0 || (raw_size = ftell(input)) < 0) {
    fclose(input);
    npc_set_error(error, error_size, "cannot read ELF '%s'", path);
    return 0;
  }
  image_size = (size_t)raw_size;
  if (image_size < sizeof(header)) {
    fclose(input);
    npc_set_error(error, error_size, "ELF is too small: %s", path);
    return 0;
  }
  image = (uint8_t *)malloc(image_size);
  if (image == NULL) {
    fclose(input);
    npc_set_error(error, error_size, "cannot allocate ELF image: %s", path);
    return 0;
  }
  if (fseek(input, 0, SEEK_SET) != 0 || fread(image, 1, image_size, input) != image_size) {
    fclose(input);
    free(image);
    npc_set_error(error, error_size, "cannot read ELF '%s'", path);
    return 0;
  }
  fclose(input);
  memcpy(&header, image, sizeof(header));
  if (memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
      header.e_ident[EI_CLASS] != ELFCLASS32 ||
      header.e_ident[EI_DATA] != ELFDATA2LSB || header.e_machine != EM_RISCV ||
      header.e_shentsize != sizeof(Elf32_Shdr) || header.e_shnum == 0 ||
      !npc_trace_in_file((size_t)header.e_shoff,
                         (size_t)header.e_shnum * sizeof(Elf32_Shdr), image_size)) {
    free(image);
    npc_set_error(error, error_size, "invalid RV32 ELF: %s", path);
    return 0;
  }

  for (section_index = 0; section_index < header.e_shnum; ++section_index) {
    Elf32_Shdr symbols;
    Elf32_Shdr strings;
    size_t symbol_count;
    size_t symbol_index;

    if (!npc_trace_read_section(image, image_size, &header, section_index, &symbols) ||
        symbols.sh_type != SHT_SYMTAB || symbols.sh_entsize != sizeof(Elf32_Sym) ||
        symbols.sh_link >= header.e_shnum ||
        !npc_trace_in_file((size_t)symbols.sh_offset, (size_t)symbols.sh_size, image_size) ||
        !npc_trace_read_section(image, image_size, &header, symbols.sh_link, &strings) ||
        strings.sh_type != SHT_STRTAB ||
        !npc_trace_in_file((size_t)strings.sh_offset, (size_t)strings.sh_size, image_size)) {
      continue;
    }
    symbol_count = (size_t)symbols.sh_size / sizeof(Elf32_Sym);
    for (symbol_index = 0; symbol_index < symbol_count; ++symbol_index) {
      Elf32_Sym symbol;
      size_t symbol_offset;
      const char *name;
      size_t remaining;

      symbol_offset = (size_t)symbols.sh_offset + symbol_index * sizeof(symbol);
      if (!npc_trace_in_file(symbol_offset, sizeof(symbol), image_size)) {
        continue;
      }
      memcpy(&symbol, image + symbol_offset, sizeof(symbol));
      if (ELF32_ST_TYPE(symbol.st_info) != STT_FUNC || symbol.st_size == 0 ||
          symbol.st_name >= strings.sh_size) {
        continue;
      }
      name = (const char *)(image + strings.sh_offset + symbol.st_name);
      remaining = (size_t)strings.sh_size - symbol.st_name;
      if (memchr(name, '\0', remaining) == NULL) {
        continue;
      }
      if (!npc_trace_append_function(trace, symbol.st_value, symbol.st_size, name, error,
                                     error_size)) {
        free(image);
        return 0;
      }
    }
  }
  free(image);
  printf("FTrace: loaded %zu function symbols from %s\n", trace->function_count, path);
  return 1;
}

static void npc_trace_append_recent(NpcTrace *trace, const char *line) {
  char *copy;

  copy = npc_copy_string(line);
  if (copy == NULL) {
    return;
  }
  if (trace->recent_count == NPC_ITRACE_DEPTH) {
    free(trace->recent[0]);
    memmove(&trace->recent[0], &trace->recent[1],
            (NPC_ITRACE_DEPTH - 1) * sizeof(trace->recent[0]));
    trace->recent_count -= 1;
  }
  trace->recent[trace->recent_count] = copy;
  trace->recent_count += 1;
}

static void npc_trace_record_itrace(NpcTrace *trace, const NpcCommit *commit) {
  const uint8_t bytes[4] = {
      (uint8_t)commit->inst, (uint8_t)(commit->inst >> 8),
      (uint8_t)(commit->inst >> 16), (uint8_t)(commit->inst >> 24)};
  cs_insn *instruction;
  size_t count;
  char line[NPC_TRACE_LINE_SIZE];

  instruction = NULL;
  count = cs_disasm(trace->capstone, bytes, sizeof(bytes), commit->pc, 1, &instruction);
  if (count == 1) {
    if (instruction[0].op_str[0] == '\0') {
      (void)snprintf(line, sizeof(line), "%08x: %08x %s", commit->pc, commit->inst,
                     instruction[0].mnemonic);
    } else {
      (void)snprintf(line, sizeof(line), "%08x: %08x %s %s", commit->pc, commit->inst,
                     instruction[0].mnemonic, instruction[0].op_str);
    }
  } else {
    (void)snprintf(line, sizeof(line), "%08x: %08x <invalid>", commit->pc, commit->inst);
  }
  if (instruction != NULL) {
    cs_free(instruction, count);
  }
  printf("ITRACE %s\n", line);
  npc_trace_append_recent(trace, line);
}

static const NpcTraceFunction *npc_trace_function_at(const NpcTrace *trace,
                                                      uint32_t address) {
  size_t index;

  for (index = 0; index < trace->function_count; ++index) {
    const NpcTraceFunction *function;

    function = &trace->functions[index];
    if (address >= function->start && address < function->end) {
      return function;
    }
  }
  return NULL;
}

static const NpcTraceFunction *npc_trace_function_starting_at(const NpcTrace *trace,
                                                               uint32_t address) {
  size_t index;

  for (index = 0; index < trace->function_count; ++index) {
    if (address == trace->functions[index].start) {
      return &trace->functions[index];
    }
  }
  return NULL;
}

static void npc_trace_print_indent(const NpcTrace *trace) {
  int index;

  for (index = 0; index < trace->call_depth; ++index) {
    printf("  ");
  }
}

static void npc_trace_record_ftrace(NpcTrace *trace, const NpcCommit *commit) {
  uint32_t opcode;
  unsigned int rd;
  unsigned int rs1;
  int is_jal;
  int is_jalr;
  int32_t immediate;
  int is_return;
  const NpcTraceFunction *function;

  opcode = commit->inst & 0x7fu;
  rd = (commit->inst >> 7) & 0x1fu;
  rs1 = (commit->inst >> 15) & 0x1fu;
  is_jal = opcode == 0x6fu;
  is_jalr = opcode == 0x67u;
  immediate = (int32_t)commit->inst >> 20;
  is_return = is_jalr && rd == 0 && (rs1 == 1 || rs1 == 5) && immediate == 0;
  function = npc_trace_function_at(trace, commit->dnpc);
  if (is_return) {
    if (trace->call_depth > 0) {
      trace->call_depth -= 1;
    }
    printf("FTRACE 0x%x: ", commit->pc);
    npc_trace_print_indent(trace);
    printf("ret  [%s]\n", function == NULL ? "??" : function->name);
    return;
  }
  if ((is_jal || is_jalr) && (rd == 1 || rd == 5)) {
    printf("FTRACE 0x%x: ", commit->pc);
    npc_trace_print_indent(trace);
    printf("call [%s@0x%x]\n", function == NULL ? "??" : function->name, commit->dnpc);
    trace->call_depth += 1;
    return;
  }
  if ((is_jal || is_jalr) && rd == 0) {
    const NpcTraceFunction *tail;

    tail = npc_trace_function_starting_at(trace, commit->dnpc);
    if (tail != NULL) {
      printf("FTRACE 0x%x: ", commit->pc);
      npc_trace_print_indent(trace);
      printf("tail [%s@0x%x]\n", tail->name, commit->dnpc);
    }
  }
}

NpcTrace *npc_trace_create(const NpcOptions *options, char *error, size_t error_size) {
  NpcTrace *trace;

  trace = (NpcTrace *)calloc(1, sizeof(*trace));
  if (trace == NULL) {
    npc_set_error(error, error_size, "cannot allocate trace state");
    return NULL;
  }
#ifdef CONFIG_NPC_ITRACE
  trace->itrace_enabled = 1;
#endif
#ifdef CONFIG_NPC_MTRACE
  trace->mtrace_enabled = 1;
#endif
#ifdef CONFIG_NPC_FTRACE
  trace->ftrace_enabled = 1;
#endif
  if (trace->itrace_enabled) {
    if (cs_open(CS_ARCH_RISCV, CS_MODE_RISCV32, &trace->capstone) != CS_ERR_OK) {
      free(trace);
      npc_set_error(error, error_size, "cannot initialize Capstone for RV32");
      return NULL;
    }
    trace->capstone_ready = 1;
    (void)cs_option(trace->capstone, CS_OPT_DETAIL, CS_OPT_OFF);
  }
  if (trace->ftrace_enabled) {
    if (options->elf[0] == '\0') {
      npc_trace_destroy(trace);
      npc_set_error(error, error_size, "CONFIG_NPC_FTRACE requires --elf");
      return NULL;
    }
    if (!npc_trace_load_elf(trace, options->elf, error, error_size)) {
      npc_trace_destroy(trace);
      return NULL;
    }
  }
  return trace;
}

void npc_trace_destroy(NpcTrace *trace) {
  size_t index;

  if (trace == NULL) {
    return;
  }
  if (trace->capstone_ready) {
    cs_close(&trace->capstone);
  }
  for (index = 0; index < trace->recent_count; ++index) {
    free(trace->recent[index]);
  }
  for (index = 0; index < trace->function_count; ++index) {
    free(trace->functions[index].name);
  }
  free(trace->functions);
  free(trace);
}

void npc_trace_record(NpcTrace *trace, const NpcCommit *commit) {
  if (trace->itrace_enabled) {
    npc_trace_record_itrace(trace, commit);
  }
  if (trace->mtrace_enabled && commit->mem_valid) {
    printf("MTRACE %c 0x%08x len=%u data=0x%08x\n", commit->mem_write ? 'W' : 'R',
           commit->mem_addr, npc_trace_byte_count(commit->mem_mask), commit->mem_data);
  }
  if (trace->ftrace_enabled) {
    npc_trace_record_ftrace(trace, commit);
  }
}

void npc_trace_dump_recent(const NpcTrace *trace) {
  size_t index;

  if (trace->recent_count == 0) {
    return;
  }
  fprintf(stderr, "Instruction ring buffer:\n");
  for (index = 0; index < trace->recent_count; ++index) {
    fprintf(stderr, "  %s\n", trace->recent[index]);
  }
}
