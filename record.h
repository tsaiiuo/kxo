#include <linux/printk.h>

#include <linux/types.h>

#define N_BOARDS 15 /* must be power of 2 minus 1*/


void record_init(void);

void record_board_init(void);

void record_board_update(int move);

void record_append_board(void);

uint64_t record_get_board(unsigned int index);

int record_get_size(void);