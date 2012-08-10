#include <string.h>

#include "SlipPersist.h"

static unsigned int num_persists = 0;
static persist_t * persist_base = NULL;
static void * extra_memory = NULL;

void * slipken_simple_malloc(size_t size) {
  void * saved = extra_memory;
  extra_memory += size;
  extra_memory += size % 4 ? 4 - (size % 4) : 0;
  return saved;
}

void persist(void * system, size_t size) {
  NTF(persist_base != NULL);
  persist_t * cur = &persist_base[num_persists];
  cur->system = system; NTF(cur->system != NULL);
  cur->ken = slipken_simple_malloc(size); NTF(cur->ken != NULL);
  memcpy(cur->ken, system, size);
  cur->size = size; NTF(cur->size != 0);
  num_persists++;
}

void load_persists(unsigned int num_persists_, persist_t * persist_base_) {
  unsigned int i;
  num_persists = num_persists_;
  persist_base = persist_base_;

  for (i = 0; i < num_persists; i++) {
    persist_t * pers = & persist_base[i];
    memcpy(pers->system, pers->ken, pers->size);
  }
}

void save_persists(struct Slipken_data * dat) {
  unsigned int i;
  dat->num_persists = num_persists;

  for (i = 0; i < num_persists; i++) {
    persist_t * pers = & persist_base[i];
    memcpy(pers->ken, pers->system, pers->size);
  }
}

struct Slipken_data * create_slipken_data() {
  extra_memory = ken_malloc(Memory_size + Extra_memory_size);
  NTF(extra_memory != NULL);
  memset(extra_memory, 0, Memory_size + Extra_memory_size);
  void * Memory = slipken_simple_malloc(Memory_size);
  NTF(Memory != NULL);

  struct Slipken_data * data = slipken_simple_malloc(sizeof (struct Slipken_data));
  NTF(data != NULL);
  data->my_pid = getpid();
  data->num_persists = 0;
  data->Memory = Memory;
  memset(data->persist_base, 0, 200 * sizeof(persist_t));
  load_persists(0, data->persist_base);

  ken_set_app_data(data);
  return data;
}

void save_slipken_data(struct Slipken_data * data) {
  save_persists(data);
}

void load_slipken_data(struct Slipken_data * data) {
  load_persists(data->num_persists, data->persist_base);
  data->my_pid = getpid();
}
