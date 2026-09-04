#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <elf.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <capstone/capstone.h>
#include <verilated.h>
#include <verilated_vcd_c.h>
#include <VNpcTop.h>

namespace {

constexpr uint32_t kPmemBase = 0x80000000u;
constexpr size_t kPmemSize = 128u * 1024u * 1024u;
constexpr size_t kItraceDepth = 16;

struct Commit {
  uint32_t pc = 0;
  uint32_t inst = 0;
  uint32_t dnpc = 0;
  std::array<uint32_t, 16> gpr{};
  bool mem_valid = false;
  bool mem_write = false;
  uint32_t mem_addr = 0;
  uint32_t mem_data = 0;
  uint8_t mem_mask = 0;
  bool halt = false;
  uint32_t halt_code = 0;
  bool invalid = false;
};

struct Options {
  std::string image;
  std::string elf;
  std::string diff;
  bool batch = false;
  bool itrace = false;
  bool mtrace = false;
  bool ftrace = false;
};

class PhysicalMemory {
 public:
  PhysicalMemory() : bytes_(kPmemSize, 0) {}

  bool load_image(const std::string &path, std::string *error) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
      *error = "cannot open image '" + path + "'";
      return false;
    }
    const std::streamsize size = input.tellg();
    if (size < 0 || static_cast<uint64_t>(size) > bytes_.size()) {
      *error = "image is larger than physical memory";
      return false;
    }
    input.seekg(0);
    input.read(reinterpret_cast<char *>(bytes_.data()), size);
    if (!input) {
      *error = "cannot read image '" + path + "'";
      return false;
    }
    image_size_ = static_cast<size_t>(size);
    return true;
  }

  bool read_word(uint32_t addr, uint32_t *value, std::string *error) const {
    if ((addr & 0x3u) != 0 || !in_range(addr, 4)) {
      *error = range_error(addr, 4);
      return false;
    }
    const size_t offset = static_cast<size_t>(addr - kPmemBase);
    *value = static_cast<uint32_t>(bytes_[offset]) |
             (static_cast<uint32_t>(bytes_[offset + 1]) << 8) |
             (static_cast<uint32_t>(bytes_[offset + 2]) << 16) |
             (static_cast<uint32_t>(bytes_[offset + 3]) << 24);
    return true;
  }

  bool write_word_masked(uint32_t addr, uint32_t value, uint8_t mask,
                         std::string *error) {
    if ((addr & 0x3u) != 0 || !in_range(addr, 4)) {
      *error = range_error(addr, 4);
      return false;
    }
    const size_t offset = static_cast<size_t>(addr - kPmemBase);
    for (unsigned i = 0; i < 4; ++i) {
      if ((mask & (1u << i)) != 0) {
        bytes_[offset + i] = static_cast<uint8_t>(value >> (i * 8));
      }
    }
    return true;
  }

  bool read(uint32_t addr, unsigned length, uint32_t *value,
            std::string *error) const {
    if (length == 0 || length > 4 || !in_range(addr, length)) {
      *error = range_error(addr, length);
      return false;
    }
    const size_t offset = static_cast<size_t>(addr - kPmemBase);
    uint32_t result = 0;
    for (unsigned i = 0; i < length; ++i) {
      result |= static_cast<uint32_t>(bytes_[offset + i]) << (i * 8);
    }
    *value = result;
    return true;
  }

  const uint8_t *data() const { return bytes_.data(); }
  size_t image_size() const { return image_size_; }

 private:
  bool in_range(uint32_t addr, size_t length) const {
    const uint64_t first = addr;
    const uint64_t last = first + length;
    return first >= kPmemBase && last <= static_cast<uint64_t>(kPmemBase) + bytes_.size();
  }

  static std::string range_error(uint32_t addr, size_t length) {
    std::ostringstream stream;
    stream << "physical memory access out of range: addr=0x" << std::hex << addr
           << " len=" << std::dec << length;
    return stream.str();
  }

  std::vector<uint8_t> bytes_;
  size_t image_size_ = 0;
};

class Trace {
 public:
  explicit Trace(const Options &options)
      : itrace_enabled_(options.itrace), mtrace_enabled_(options.mtrace),
        ftrace_enabled_(options.ftrace) {
    if (itrace_enabled_) {
      if (cs_open(CS_ARCH_RISCV, CS_MODE_RISCV32, &capstone_) != CS_ERR_OK) {
        throw std::runtime_error("cannot initialize Capstone for RV32");
      }
      capstone_ready_ = true;
      cs_option(capstone_, CS_OPT_DETAIL, CS_OPT_OFF);
    }
    if (ftrace_enabled_) {
      if (options.elf.empty()) {
        throw std::runtime_error("--ftrace requires --elf");
      }
      load_elf(options.elf);
    }
  }

  ~Trace() {
    if (capstone_ready_) {
      cs_close(&capstone_);
    }
  }

  void record(const Commit &commit) {
    if (itrace_enabled_) {
      record_itrace(commit);
    }
    if (mtrace_enabled_ && commit.mem_valid) {
      const unsigned length = byte_count(commit.mem_mask);
      std::cout << "MTRACE " << (commit.mem_write ? "W" : "R") << " 0x"
                << std::hex << std::setw(8) << std::setfill('0') << commit.mem_addr
                << " len=" << std::dec << length << " data=0x" << std::hex
                << std::setw(8) << commit.mem_data << std::dec << std::setfill(' ')
                << '\n';
    }
    if (ftrace_enabled_) {
      record_ftrace(commit);
    }
  }

  void dump_recent() const {
    if (recent_.empty()) {
      return;
    }
    std::cerr << "Instruction ring buffer:\n";
    for (const auto &line : recent_) {
      std::cerr << "  " << line << '\n';
    }
  }

 private:
  struct Function {
    uint32_t start;
    uint32_t end;
    std::string name;
  };

  static unsigned byte_count(uint8_t mask) {
    unsigned count = 0;
    for (unsigned i = 0; i < 4; ++i) {
      count += (mask >> i) & 1u;
    }
    return count;
  }

  void record_itrace(const Commit &commit) {
    const uint8_t bytes[4] = {
        static_cast<uint8_t>(commit.inst), static_cast<uint8_t>(commit.inst >> 8),
        static_cast<uint8_t>(commit.inst >> 16), static_cast<uint8_t>(commit.inst >> 24)};
    cs_insn *instruction = nullptr;
    const size_t count = cs_disasm(capstone_, bytes, sizeof(bytes), commit.pc, 1, &instruction);
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(8) << commit.pc << ": "
           << std::setw(8) << commit.inst << " ";
    if (count == 1) {
      stream << instruction[0].mnemonic;
      if (instruction[0].op_str[0] != '\0') {
        stream << ' ' << instruction[0].op_str;
      }
    } else {
      stream << "<invalid>";
    }
    if (instruction != nullptr) {
      cs_free(instruction, count);
    }
    const std::string line = stream.str();
    std::cout << "ITRACE " << line << '\n';
    recent_.push_back(line);
    if (recent_.size() > kItraceDepth) {
      recent_.erase(recent_.begin());
    }
  }

  static bool in_file(size_t offset, size_t length, size_t file_size) {
    return offset <= file_size && length <= file_size - offset;
  }

  void load_elf(const std::string &path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
      throw std::runtime_error("cannot open ELF '" + path + "'");
    }
    const std::streamsize raw_size = input.tellg();
    if (raw_size < static_cast<std::streamsize>(sizeof(Elf32_Ehdr))) {
      throw std::runtime_error("ELF is too small: " + path);
    }
    std::vector<uint8_t> image(static_cast<size_t>(raw_size));
    input.seekg(0);
    input.read(reinterpret_cast<char *>(image.data()), raw_size);
    if (!input) {
      throw std::runtime_error("cannot read ELF '" + path + "'");
    }
    const auto *header = reinterpret_cast<const Elf32_Ehdr *>(image.data());
    if (std::memcmp(header->e_ident, ELFMAG, SELFMAG) != 0 ||
        header->e_ident[EI_CLASS] != ELFCLASS32 ||
        header->e_ident[EI_DATA] != ELFDATA2LSB || header->e_machine != EM_RISCV ||
        header->e_shentsize != sizeof(Elf32_Shdr) || header->e_shnum == 0 ||
        !in_file(header->e_shoff, static_cast<size_t>(header->e_shnum) * sizeof(Elf32_Shdr),
                 image.size())) {
      throw std::runtime_error("invalid RV32 ELF: " + path);
    }
    const auto *sections = reinterpret_cast<const Elf32_Shdr *>(image.data() + header->e_shoff);
    for (unsigned section_index = 0; section_index < header->e_shnum; ++section_index) {
      const Elf32_Shdr &symbols = sections[section_index];
      if (symbols.sh_type != SHT_SYMTAB || symbols.sh_entsize != sizeof(Elf32_Sym) ||
          symbols.sh_link >= header->e_shnum ||
          !in_file(symbols.sh_offset, symbols.sh_size, image.size())) {
        continue;
      }
      const Elf32_Shdr &strings = sections[symbols.sh_link];
      if (strings.sh_type != SHT_STRTAB || !in_file(strings.sh_offset, strings.sh_size, image.size())) {
        continue;
      }
      const auto *table = reinterpret_cast<const Elf32_Sym *>(image.data() + symbols.sh_offset);
      const char *names = reinterpret_cast<const char *>(image.data() + strings.sh_offset);
      const size_t count = symbols.sh_size / sizeof(Elf32_Sym);
      for (size_t i = 0; i < count; ++i) {
        const Elf32_Sym &symbol = table[i];
        if (ELF32_ST_TYPE(symbol.st_info) != STT_FUNC || symbol.st_size == 0 ||
            symbol.st_name >= strings.sh_size) {
          continue;
        }
        const char *name = names + symbol.st_name;
        const size_t remaining = strings.sh_size - symbol.st_name;
        if (std::memchr(name, '\0', remaining) == nullptr) {
          continue;
        }
        functions_.push_back({symbol.st_value, symbol.st_value + symbol.st_size, name});
      }
    }
    std::cout << "FTrace: loaded " << functions_.size() << " function symbols from " << path
              << '\n';
  }

  const Function *function_at(uint32_t address) const {
    for (const auto &function : functions_) {
      if (address >= function.start && address < function.end) {
        return &function;
      }
    }
    return nullptr;
  }

  const Function *function_starting_at(uint32_t address) const {
    for (const auto &function : functions_) {
      if (address == function.start) {
        return &function;
      }
    }
    return nullptr;
  }

  void print_indent() const {
    for (int i = 0; i < call_depth_; ++i) {
      std::cout << "  ";
    }
  }

  void record_ftrace(const Commit &commit) {
    const uint32_t opcode = commit.inst & 0x7f;
    const unsigned rd = (commit.inst >> 7) & 0x1f;
    const unsigned rs1 = (commit.inst >> 15) & 0x1f;
    const bool is_jal = opcode == 0x6f;
    const bool is_jalr = opcode == 0x67;
    const int32_t immediate = static_cast<int32_t>(commit.inst) >> 20;
    const bool is_return = is_jalr && rd == 0 && (rs1 == 1 || rs1 == 5) && immediate == 0;
    const Function *function = function_at(commit.dnpc);
    if (is_return) {
      if (call_depth_ > 0) {
        --call_depth_;
      }
      std::cout << "FTRACE 0x" << std::hex << commit.pc << std::dec << ": ";
      print_indent();
      std::cout << "ret  [" << (function == nullptr ? "??" : function->name) << "]\n";
      return;
    }
    if ((is_jal || is_jalr) && (rd == 1 || rd == 5)) {
      std::cout << "FTRACE 0x" << std::hex << commit.pc << std::dec << ": ";
      print_indent();
      std::cout << "call [" << (function == nullptr ? "??" : function->name) << "@0x"
                << std::hex << commit.dnpc << std::dec << "]\n";
      ++call_depth_;
      return;
    }
    if ((is_jal || is_jalr) && rd == 0) {
      const Function *tail = function_starting_at(commit.dnpc);
      if (tail != nullptr) {
        std::cout << "FTRACE 0x" << std::hex << commit.pc << std::dec << ": ";
        print_indent();
        std::cout << "tail [" << tail->name << "@0x" << std::hex << commit.dnpc << std::dec
                  << "]\n";
      }
    }
  }

  bool itrace_enabled_ = false;
  bool mtrace_enabled_ = false;
  bool ftrace_enabled_ = false;
  csh capstone_ = 0;
  bool capstone_ready_ = false;
  std::vector<std::string> recent_;
  std::vector<Function> functions_;
  int call_depth_ = 0;
};

struct NemuState {
  uint32_t gpr[32];
  uint32_t pc;
};

class Difftest {
 public:
  ~Difftest() {
    if (handle_ != nullptr) {
      dlclose(handle_);
    }
  }

  bool initialize(const std::string &path, const PhysicalMemory &memory,
                  uint32_t reset_pc, std::string *error) {
    handle_ = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (handle_ == nullptr) {
      *error = "cannot load DiffTest reference '" + path + "': " + dlerror();
      return false;
    }
    if (!load_symbol("difftest_memcpy", &memcpy_, error) ||
        !load_symbol("difftest_regcpy", &regcpy_, error) ||
        !load_symbol("difftest_exec", &exec_, error) ||
        !load_symbol("difftest_init", &init_, error)) {
      return false;
    }
    init_(0);
    memcpy_(kPmemBase, const_cast<uint8_t *>(memory.data()), memory.image_size(), kToRef);
    NemuState initial{};
    initial.pc = reset_pc;
    regcpy_(&initial, kToRef);
    enabled_ = true;
    return true;
  }

  bool enabled() const { return enabled_; }

  bool step(const Commit &dut, std::string *error) {
    exec_(1);
    NemuState reference{};
    regcpy_(&reference, kToDut);
    for (unsigned i = 0; i < dut.gpr.size(); ++i) {
      if (reference.gpr[i] != dut.gpr[i]) {
        std::ostringstream stream;
        stream << "DiffTest mismatch at pc=0x" << std::hex << dut.pc << ": x" << std::dec << i
               << " DUT=0x" << std::hex << dut.gpr[i] << " REF=0x" << reference.gpr[i];
        *error = stream.str();
        return false;
      }
    }
    if (reference.pc != dut.dnpc) {
      std::ostringstream stream;
      stream << "DiffTest mismatch at pc=0x" << std::hex << dut.pc << ": next pc DUT=0x"
             << dut.dnpc << " REF=0x" << reference.pc;
      *error = stream.str();
      return false;
    }
    return true;
  }

 private:
  using memcpy_fn = void (*)(uint32_t, void *, size_t, bool);
  using regcpy_fn = void (*)(void *, bool);
  using exec_fn = void (*)(uint64_t);
  using init_fn = void (*)(int);
  static constexpr bool kToDut = false;
  static constexpr bool kToRef = true;

  template <typename T>
  bool load_symbol(const char *name, T *destination, std::string *error) {
    dlerror();
    void *symbol = dlsym(handle_, name);
    const char *message = dlerror();
    if (message != nullptr) {
      *error = std::string("DiffTest reference is missing '") + name + "': " + message;
      return false;
    }
    *destination = reinterpret_cast<T>(symbol);
    return true;
  }

  void *handle_ = nullptr;
  memcpy_fn memcpy_ = nullptr;
  regcpy_fn regcpy_ = nullptr;
  exec_fn exec_ = nullptr;
  init_fn init_ = nullptr;
  bool enabled_ = false;
};

class Simulator;
Simulator *g_simulator = nullptr;

class ExpressionParser {
 public:
  ExpressionParser(const std::string &input, const Commit &state, const PhysicalMemory &memory)
      : input_(input), state_(state), memory_(memory) {}

  uint32_t parse() {
    const uint32_t result = expression();
    skip_space();
    if (position_ != input_.size()) {
      throw std::runtime_error("unexpected token near '" + input_.substr(position_) + "'");
    }
    return result;
  }

 private:
  uint32_t expression() {
    uint32_t value = term();
    while (true) {
      skip_space();
      if (consume('+')) {
        value += term();
      } else if (consume('-')) {
        value -= term();
      } else {
        return value;
      }
    }
  }

  uint32_t term() {
    uint32_t value = factor();
    while (true) {
      skip_space();
      if (consume('*')) {
        value *= factor();
      } else if (consume('/')) {
        const uint32_t divisor = factor();
        if (divisor == 0) {
          throw std::runtime_error("division by zero");
        }
        value /= divisor;
      } else {
        return value;
      }
    }
  }

  uint32_t factor() {
    skip_space();
    if (consume('(')) {
      const uint32_t value = expression();
      if (!consume(')')) {
        throw std::runtime_error("missing ')' in expression");
      }
      return value;
    }
    if (consume('-')) {
      return 0u - factor();
    }
    if (consume('*')) {
      uint32_t value = 0;
      std::string error;
      if (!memory_.read(factor(), 4, &value, &error)) {
        throw std::runtime_error(error);
      }
      return value;
    }
    if (consume('$')) {
      return register_value(identifier());
    }
    return number();
  }

  uint32_t number() {
    skip_space();
    const size_t begin = position_;
    int base = 10;
    if (position_ + 2 <= input_.size() && input_[position_] == '0' &&
        (input_[position_ + 1] == 'x' || input_[position_ + 1] == 'X')) {
      position_ += 2;
      base = 16;
    }
    const size_t digits = position_;
    while (position_ < input_.size() &&
           std::isxdigit(static_cast<unsigned char>(input_[position_]))) {
      ++position_;
    }
    if (digits == position_) {
      throw std::runtime_error("expected a number near '" + input_.substr(begin) + "'");
    }
    try {
      return static_cast<uint32_t>(std::stoul(input_.substr(begin, position_ - begin), nullptr, base));
    } catch (const std::exception &) {
      throw std::runtime_error("invalid number in expression");
    }
  }

  std::string identifier() {
    skip_space();
    const size_t begin = position_;
    while (position_ < input_.size() &&
           (std::isalnum(static_cast<unsigned char>(input_[position_])) || input_[position_] == '_')) {
      ++position_;
    }
    if (begin == position_) {
      throw std::runtime_error("expected a register name after '$'");
    }
    return input_.substr(begin, position_ - begin);
  }

  uint32_t register_value(const std::string &name) const {
    static const std::array<const char *, 16> names = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
        "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5"};
    if (name == "pc") {
      return state_.dnpc;
    }
    for (unsigned i = 0; i < names.size(); ++i) {
      if (name == names[i] || name == std::to_string(i) || name == "x" + std::to_string(i)) {
        return state_.gpr[i];
      }
    }
    throw std::runtime_error("unknown register '$" + name + "'");
  }

  void skip_space() {
    while (position_ < input_.size() &&
           std::isspace(static_cast<unsigned char>(input_[position_]))) {
      ++position_;
    }
  }

  bool consume(char character) {
    skip_space();
    if (position_ < input_.size() && input_[position_] == character) {
      ++position_;
      return true;
    }
    return false;
  }

  const std::string &input_;
  const Commit &state_;
  const PhysicalMemory &memory_;
  size_t position_ = 0;
};

class Simulator {
 public:
  explicit Simulator(Options options) : options_(std::move(options)), trace_(options_) {}

  ~Simulator() {
    if (top_ != nullptr) {
      top_->final();
    }
    if (trace_vcd_ != nullptr) {
      trace_vcd_->close();
    }
    g_simulator = nullptr;
  }

  bool load_image(std::string *error) { return memory_.load_image(options_.image, error); }

  bool initialize_difftest(std::string *error) {
    if (options_.diff.empty()) {
      return true;
    }
    return difftest_.initialize(options_.diff, memory_, kPmemBase, error);
  }

  void initialize(int argc, char **argv) {
    context_ = std::make_unique<VerilatedContext>();
    context_->commandArgs(argc, argv);
    context_->traceEverOn(true);
    top_ = std::make_unique<VNpcTop>(context_.get());
    trace_vcd_ = std::make_unique<VerilatedVcdC>();
    top_->trace(trace_vcd_.get(), 99);
    trace_vcd_->open("build/waveform.vcd");

    // SDB may inspect state before the first retire event.
    state_.pc = kPmemBase;
    state_.dnpc = kPmemBase;
    state_.gpr.fill(0);

    g_simulator = this;
    reset_active_ = true;
    top_->clock = 0;
    top_->reset = 1;
    evaluate();
    top_->clock = 1;
    evaluate();
    top_->clock = 0;
    evaluate();
    top_->reset = 0;
    evaluate();
    reset_active_ = false;
  }

  int run() {
    if (options_.batch) {
      execute();
    } else {
      std::cout << "NPC sdb. Type 'help' for commands.\n";
      sdb();
    }
    return exit_status();
  }

  uint32_t dpi_read(uint32_t addr) {
    uint32_t value = 0;
    std::string error;
    if (!memory_.read_word(addr, &value, &error)) {
      if (reset_active_) {
        return 0;
      }
      fatal(error);
    }
    return value;
  }

  void dpi_write(uint32_t addr, uint32_t value, uint8_t mask) {
    std::string error;
    if (!memory_.write_word_masked(addr, value, mask, &error)) {
      fatal(error);
    }
  }

  void dpi_commit(const Commit &commit) {
    if (fatal_) {
      return;
    }
    state_ = commit;
    state_.gpr[0] = 0;
    ++instructions_;
    trace_.record(state_);
    check_watchpoints();
    if (!state_.invalid && difftest_.enabled()) {
      std::string error;
      if (!difftest_.step(state_, &error)) {
        fatal(error);
        trace_.dump_recent();
      }
    }
    if (state_.invalid) {
      std::ostringstream stream;
      stream << "invalid instruction 0x" << std::hex << state_.inst << " at pc=0x" << state_.pc;
      fatal(stream.str());
      trace_.dump_recent();
      return;
    }
    if (state_.halt) {
      halted_ = true;
      halt_code_ = state_.halt_code;
      stop_requested_ = true;
    }
  }

 private:
  struct Watchpoint {
    int id;
    std::string expression;
    uint32_t value;
  };

  void evaluate() {
    top_->eval();
    trace_vcd_->dump(context_->time());
    context_->timeInc(1);
  }

  void single_cycle() {
    top_->clock = 0;
    evaluate();
    top_->clock = 1;
    evaluate();
    top_->clock = 0;
    evaluate();
    ++cycles_;
  }

  void execute() {
    stop_requested_ = false;
    while (!halted_ && !fatal_ && !quit_ && !stop_requested_ && !context_->gotFinish()) {
      single_cycle();
    }
  }

  void execute(uint64_t count) {
    stop_requested_ = false;
    for (uint64_t executed = 0;
         executed < count && !halted_ && !fatal_ && !quit_ && !stop_requested_ &&
         !context_->gotFinish();
         ++executed) {
      single_cycle();
    }
  }

  uint32_t evaluate_expression(const std::string &expression) const {
    return ExpressionParser(expression, state_, memory_).parse();
  }

  void print_registers() const {
    static const std::array<const char *, 16> names = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
        "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5"};
    for (unsigned i = 0; i < state_.gpr.size(); ++i) {
      std::cout << std::setw(4) << names[i] << " (x" << std::setw(2) << i << ") = 0x"
                << std::hex << std::setw(8) << std::setfill('0') << state_.gpr[i] << std::dec
                << std::setfill(' ') << '\n';
    }
    std::cout << "  pc       = 0x" << std::hex << std::setw(8) << std::setfill('0') << state_.dnpc
              << std::dec << std::setfill(' ') << '\n';
  }

  void print_memory(unsigned count, uint32_t address) const {
    for (unsigned i = 0; i < count; ++i) {
      uint32_t value = 0;
      std::string error;
      const uint32_t current = address + i * 4;
      if (!memory_.read(current, 4, &value, &error)) {
        std::cout << error << '\n';
        return;
      }
      std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << current << ": 0x"
                << std::setw(8) << value << std::dec << std::setfill(' ') << '\n';
    }
  }

  void check_watchpoints() {
    for (auto &watchpoint : watchpoints_) {
      try {
        const uint32_t value = evaluate_expression(watchpoint.expression);
        if (value != watchpoint.value) {
          std::cout << "Watchpoint " << watchpoint.id << ": " << watchpoint.expression << " changed 0x"
                    << std::hex << watchpoint.value << " -> 0x" << value << std::dec << '\n';
          watchpoint.value = value;
          stop_requested_ = true;
        }
      } catch (const std::exception &exception) {
        fatal(std::string("watchpoint evaluation failed: ") + exception.what());
        return;
      }
    }
  }

  void add_watchpoint(const std::string &expression) {
    const uint32_t value = evaluate_expression(expression);
    const int id = next_watchpoint_id_++;
    watchpoints_.push_back({id, expression, value});
    std::cout << "Watchpoint " << id << ": " << expression << " = 0x" << std::hex << value << std::dec
              << '\n';
  }

  void delete_watchpoint(int id) {
    for (auto iterator = watchpoints_.begin(); iterator != watchpoints_.end(); ++iterator) {
      if (iterator->id == id) {
        watchpoints_.erase(iterator);
        std::cout << "Deleted watchpoint " << id << '\n';
        return;
      }
    }
    std::cout << "No watchpoint " << id << '\n';
  }

  void show_watchpoints() const {
    if (watchpoints_.empty()) {
      std::cout << "No watchpoints.\n";
      return;
    }
    for (const auto &watchpoint : watchpoints_) {
      std::cout << watchpoint.id << ": " << watchpoint.expression << " = 0x" << std::hex
                << watchpoint.value << std::dec << '\n';
    }
  }

  static std::string trim_left(std::string value) {
    const size_t first = value.find_first_not_of(" \t");
    return first == std::string::npos ? "" : value.substr(first);
  }

  void sdb() {
    std::string line;
    while (!halted_ && !fatal_ && !quit_ && std::cout << "(npc) " && std::getline(std::cin, line)) {
      std::istringstream input(line);
      std::string command;
      input >> command;
      std::string rest;
      std::getline(input, rest);
      rest = trim_left(rest);
      try {
        if (command.empty()) {
          continue;
        }
        if (command == "c") {
          execute();
        } else if (command == "si") {
          uint64_t count = 1;
          if (!rest.empty()) {
            count = std::stoull(rest, nullptr, 0);
          }
          execute(count);
        } else if (command == "info" && rest == "r") {
          print_registers();
        } else if (command == "info" && rest == "w") {
          show_watchpoints();
        } else if (command == "x") {
          std::istringstream arguments(rest);
          unsigned count = 0;
          arguments >> count;
          std::string expression;
          std::getline(arguments, expression);
          expression = trim_left(expression);
          if (count == 0 || expression.empty()) {
            std::cout << "usage: x N EXPR\n";
          } else {
            print_memory(count, evaluate_expression(expression));
          }
        } else if (command == "p") {
          if (rest.empty()) {
            std::cout << "usage: p EXPR\n";
          } else {
            std::cout << "0x" << std::hex << evaluate_expression(rest) << std::dec << '\n';
          }
        } else if (command == "w") {
          if (rest.empty()) {
            std::cout << "usage: w EXPR\n";
          } else {
            add_watchpoint(rest);
          }
        } else if (command == "d") {
          delete_watchpoint(std::stoi(rest, nullptr, 0));
        } else if (command == "q") {
          quit_ = true;
        } else if (command == "help") {
          std::cout << "c, si [N], info r, info w, x N EXPR, p EXPR, w EXPR, d N, q\n";
        } else {
          std::cout << "Unknown command: " << command << '\n';
        }
      } catch (const std::exception &exception) {
        std::cout << "error: " << exception.what() << '\n';
      }
    }
  }

  void fatal(const std::string &message) {
    if (!fatal_) {
      std::cerr << "NPC error: " << message << '\n';
    }
    fatal_ = true;
    stop_requested_ = true;
  }

  int exit_status() const {
    if (fatal_) {
      return 1;
    }
    if (halted_) {
      if (halt_code_ == 0) {
        std::cout << "HIT GOOD TRAP at pc=0x" << std::hex << state_.pc << std::dec << " after "
                  << instructions_ << " instructions\n";
        return 0;
      }
      std::cerr << "HIT BAD TRAP(code=" << halt_code_ << ") at pc=0x" << std::hex << state_.pc
                << std::dec << '\n';
      return 1;
    }
    return quit_ ? 0 : 1;
  }

  Options options_;
  PhysicalMemory memory_;
  Trace trace_;
  Difftest difftest_;
  std::unique_ptr<VerilatedContext> context_;
  std::unique_ptr<VNpcTop> top_;
  std::unique_ptr<VerilatedVcdC> trace_vcd_;
  Commit state_{};
  std::vector<Watchpoint> watchpoints_;
  int next_watchpoint_id_ = 1;
  uint64_t cycles_ = 0;
  uint64_t instructions_ = 0;
  bool halted_ = false;
  bool fatal_ = false;
  bool reset_active_ = false;
  bool stop_requested_ = false;
  bool quit_ = false;
  uint32_t halt_code_ = 0;
};

Options parse_options(int argc, char **argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    const auto require_value = [&](const char *name) -> std::string {
      if (index + 1 >= argc) {
        throw std::runtime_error(std::string(name) + " requires a value");
      }
      return argv[++index];
    };
    if (argument == "--image") {
      options.image = require_value("--image");
    } else if (argument == "--elf") {
      options.elf = require_value("--elf");
    } else if (argument == "--diff") {
      options.diff = require_value("--diff");
    } else if (argument == "--batch") {
      options.batch = true;
    } else if (argument == "--itrace") {
      options.itrace = true;
    } else if (argument == "--mtrace") {
      options.mtrace = true;
    } else if (argument == "--ftrace") {
      options.ftrace = true;
    } else if (argument == "--trace") {
      options.itrace = true;
      options.mtrace = true;
      options.ftrace = true;
    } else if (argument == "--help") {
      std::cout << "usage: VNpcTop --image IMAGE [--elf ELF] [--batch] [--diff REF_SO] "
                   "[--itrace] [--mtrace] [--ftrace]\n";
      std::exit(0);
    } else {
      throw std::runtime_error("unknown option: " + argument);
    }
  }
  if (options.image.empty()) {
    throw std::runtime_error("--image is required");
  }
  return options;
}

}  // namespace

extern "C" int pmem_read(int raddr) {
  return g_simulator == nullptr ? 0 : static_cast<int>(g_simulator->dpi_read(static_cast<uint32_t>(raddr)));
}

extern "C" void pmem_write(int waddr, int wdata, char wmask) {
  if (g_simulator != nullptr) {
    g_simulator->dpi_write(static_cast<uint32_t>(waddr), static_cast<uint32_t>(wdata),
                           static_cast<uint8_t>(wmask));
  }
}

extern "C" void npc_commit(
    int pc, int inst, int dnpc, int gpr0, int gpr1, int gpr2, int gpr3, int gpr4, int gpr5,
    int gpr6, int gpr7, int gpr8, int gpr9, int gpr10, int gpr11, int gpr12, int gpr13,
    int gpr14, int gpr15, int mem_valid, int mem_write, int mem_addr, int mem_data, int mem_mask,
    int halt, int halt_code, int invalid) {
  if (g_simulator == nullptr) {
    return;
  }
  Commit commit;
  commit.pc = static_cast<uint32_t>(pc);
  commit.inst = static_cast<uint32_t>(inst);
  commit.dnpc = static_cast<uint32_t>(dnpc);
  commit.gpr = {static_cast<uint32_t>(gpr0), static_cast<uint32_t>(gpr1),
                static_cast<uint32_t>(gpr2), static_cast<uint32_t>(gpr3),
                static_cast<uint32_t>(gpr4), static_cast<uint32_t>(gpr5),
                static_cast<uint32_t>(gpr6), static_cast<uint32_t>(gpr7),
                static_cast<uint32_t>(gpr8), static_cast<uint32_t>(gpr9),
                static_cast<uint32_t>(gpr10), static_cast<uint32_t>(gpr11),
                static_cast<uint32_t>(gpr12), static_cast<uint32_t>(gpr13),
                static_cast<uint32_t>(gpr14), static_cast<uint32_t>(gpr15)};
  commit.mem_valid = mem_valid != 0;
  commit.mem_write = mem_write != 0;
  commit.mem_addr = static_cast<uint32_t>(mem_addr);
  commit.mem_data = static_cast<uint32_t>(mem_data);
  commit.mem_mask = static_cast<uint8_t>(mem_mask);
  commit.halt = halt != 0;
  commit.halt_code = static_cast<uint32_t>(halt_code);
  commit.invalid = invalid != 0;
  g_simulator->dpi_commit(commit);
}

int main(int argc, char **argv) {
  try {
    Options options = parse_options(argc, argv);
    Simulator simulator(std::move(options));
    std::string error;
    if (!simulator.load_image(&error)) {
      throw std::runtime_error(error);
    }
    if (!simulator.initialize_difftest(&error)) {
      throw std::runtime_error(error);
    }
    simulator.initialize(argc, argv);
    return simulator.run();
  } catch (const std::exception &exception) {
    std::cerr << "NPC error: " << exception.what() << '\n';
    return 1;
  }
}
