#include "record.h"


static uint64_t board_record[N_BOARDS + 1];

static uint64_t now_board;

static uint8_t now_idx;

static uint8_t head, tail;

void record_init(void)
{
    head = tail = 0;
    pr_info("record_init: init\n");
}

void record_board_init(void)
{
    now_board = 0;
    now_idx = 0;
    pr_info("record_board_init: board init\n");
}

void record_board_update(int move)
{
    now_board |= ((uint64_t) move) << now_idx;
    now_idx += 4;
}

void record_append_board(void)
{
    if (((head - tail) & N_BOARDS) == 1)
        head = (head + 1) & N_BOARDS;

    board_record[tail] = now_board;

    now_idx = 0;
    tail = (tail + 1) & N_BOARDS;
    pr_info("record_append_board: %d %llx\n", tail, now_board);
}

uint64_t record_get_board(unsigned int index)
{
    return board_record[(head + index) & N_BOARDS];
}

int record_get_size(void)
{
    return (tail - head) & N_BOARDS;
}