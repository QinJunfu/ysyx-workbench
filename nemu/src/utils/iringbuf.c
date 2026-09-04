#include <utils.h>

#define IRINGBUF_SIZE 16
#define IRINGBUF_LOG_SIZE 128

#ifdef CONFIG_IQUEUE
typedef struct {
  vaddr_t pc;
  char logbuf[IRINGBUF_LOG_SIZE];
} ITraceEntry;

static struct {
  ITraceEntry entries[IRINGBUF_SIZE];
  int head;
  int count;
} iringbuf;
#endif

void iringbuf_record(vaddr_t pc, const char *logbuf) {
#ifdef CONFIG_IQUEUE
  ITraceEntry *entry = &iringbuf.entries[iringbuf.head];
  entry->pc = pc;
  snprintf(entry->logbuf, sizeof(entry->logbuf), "%s", logbuf);

  iringbuf.head = (iringbuf.head + 1) % IRINGBUF_SIZE;
  if (iringbuf.count < IRINGBUF_SIZE) {
    iringbuf.count ++;
  }
#else
  (void)pc;
  (void)logbuf;
#endif
}

void iringbuf_display(vaddr_t fault_pc) {
#ifdef CONFIG_IQUEUE
  if (iringbuf.count == 0) return;

  int oldest = (iringbuf.head + IRINGBUF_SIZE - iringbuf.count) % IRINGBUF_SIZE;
  int fault_entry = -1;
  for (int i = 0; i < iringbuf.count; i ++) {
    int entry = (oldest + i) % IRINGBUF_SIZE;
    if (iringbuf.entries[entry].pc == fault_pc) {
      fault_entry = i;
    }
  }

  _Log("Instruction ring buffer:\n");
  for (int i = 0; i < iringbuf.count; i ++) {
    int entry = (oldest + i) % IRINGBUF_SIZE;
    _Log("%s%s\n", i == fault_entry ? "--> " : "    ", iringbuf.entries[entry].logbuf);
  }
#else
  (void)fault_pc;
#endif
}
