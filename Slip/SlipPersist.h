#ifndef SLIP_PERSIST_H
#define SLIP_PERSIST_H

#include "ken.h"
#include "kencom.h"

enum { Extra_memory_size = 5  * 1024 * 1024,
       Memory_size       = 10 * 1024 * 1024  };

typedef struct {
  void * system;
  void * ken;
  size_t size;
} persist_t;

struct Slipken_data {
  pid_t my_pid;
  void * Memory;
  unsigned int num_persists;
  persist_t persist_base[200];
};

void persist(void * system, size_t size);
void save_persists(struct Slipken_data * dat);

struct Slipken_data * create_slipken_data();
void save_slipken_data(struct Slipken_data * data);
void load_slipken_data(struct Slipken_data * data);

void * slipken_simple_malloc(size_t size);

#define slipken_persist(system) \
  persist(&(system), sizeof (system))

#define slipken_persist_init(var, val) \
  var = val; slipken_persist(var);
#endif
