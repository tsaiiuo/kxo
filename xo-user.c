#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "game.h"

#define XO_STATUS_FILE "/sys/module/kxo/initstate"
#define XO_DEVICE_FILE "/dev/kxo"
#define XO_DEVICE_ATTR_FILE "/sys/class/kxo/kxo/kxo_state"

#define IOCTL_READ_SIZE 0

#define IOCTL_READ_LIST 1

static char draw_buffer[DRAWBUFFER_SIZE];

static bool status_check(void)
{
    FILE *fp = fopen(XO_STATUS_FILE, "r");
    if (!fp) {
        printf("kxo status : not loaded\n");
        return false;
    }

    char read_buf[20];
    fgets(read_buf, 20, fp);
    read_buf[strcspn(read_buf, "\n")] = 0;
    if (strcmp("live", read_buf)) {
        printf("kxo status : %s\n", read_buf);
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;
}

static struct termios orig_termios;

static void raw_mode_disable(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void raw_mode_enable(void)
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(raw_mode_disable);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~IXON;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static bool read_attr, end_attr;

static void listen_keyboard_handler(void)
{
    int attr_fd = open(XO_DEVICE_ATTR_FILE, O_RDWR);
    char input;
    if (read(STDIN_FILENO, &input, 1) == 1) {
        char buf[20];
        switch (input) {
        case 16: /* Ctrl-P */
            read(attr_fd, buf, 6);
            buf[0] = (buf[0] - '0') ? '0' : '1';
            read_attr ^= 1;
            write(attr_fd, buf, 6);
            if (!read_attr)
                printf("Stopping to display the chess board...\n");
            break;
        case 17: /* Ctrl-Q */
            read(attr_fd, buf, 6);
            buf[4] = '1';
            read_attr = false;
            end_attr = true;
            write(attr_fd, buf, 6);
            printf("Stopping the kernel space tic-tac-toe game...\n");
            break;
        }
    }
    close(attr_fd);
}

static void get_timestamp(char *buf, size_t len)
{
    time_t t = time(NULL);
    struct tm tm;
    if (!buf)
        return;
    localtime_r(&t, &tm);
    /* format：YYYY-MM-DD HH:MM:SS */
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", &tm);
}


static int draw_board(char *table)
{
    int i = 0, k = 0;

    draw_buffer[i++] = '\n';
    draw_buffer[i++] = '\n';

    while (i < DRAWBUFFER_SIZE) {
        for (int j = 0; j < (BOARD_SIZE << 1) - 1 && k < N_GRIDS; j++) {
            draw_buffer[i++] = j & 1 ? '|' : table[k++];
        }
        draw_buffer[i++] = '\n';
        for (int j = 0; j < (BOARD_SIZE << 1) - 1; j++) {
            draw_buffer[i++] = '-';
        }
        draw_buffer[i++] = '\n';
    }

    return 0;
}

void print_record(uint64_t record)
{
    uint64_t record_size = record;

    printf("Moves: ");

    for (int i = 0; record_size; i += 1, record_size = record_size >> 4) {
        unsigned int move = record_size & 15;
        char turn = i & 1 ? 'X' : 'O';
        printf("[%c] ", turn);
        printf("%c%u", 'A' + (move >> 2), move & 3);

        if (record_size >> 4)
            printf(" -> ");
    }
    puts("");
}



void show_record(int device_fd)
{
    int size = ioctl(device_fd, IOCTL_READ_SIZE, 0);
    if (size < 0) {
        puts("Read board record fail");
        return;
    }

    for (int i = 0; i < size; i++) {
        uint64_t record;
        if (ioctl(device_fd, (((unsigned) i) << 1) | IOCTL_READ_LIST,
                  &record)) {
            puts("Read board record fail");
            return;
        }
        print_record(record);
    }
}

int main(int argc, char *argv[])
{
    if (!status_check())
        exit(1);

    raw_mode_enable();
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    fd_set readset;
    int device_fd = open(XO_DEVICE_FILE, O_RDONLY);
    int max_fd = device_fd > STDIN_FILENO ? device_fd : STDIN_FILENO;
    read_attr = true;
    end_attr = false;

    char display_buf[DRAWBUFFER_SIZE];
    char timestr[64];

    while (!end_attr) {
        FD_ZERO(&readset);
        FD_SET(STDIN_FILENO, &readset);
        FD_SET(device_fd, &readset);

        int result = select(max_fd + 1, &readset, NULL, NULL, NULL);
        if (result < 0) {
            printf("Error with select system call\n");
            exit(1);
        }

        if (FD_ISSET(STDIN_FILENO, &readset)) {
            FD_CLR(STDIN_FILENO, &readset);
            listen_keyboard_handler();
        } else if (read_attr && FD_ISSET(device_fd, &readset)) {
            FD_CLR(device_fd, &readset);
            printf("\033[H\033[J"); /* ASCII escape code to clear the screen
            // */
            read(device_fd, display_buf, DRAWBUFFER_SIZE);

            draw_board(display_buf);
            printf("%s", draw_buffer);

            get_timestamp(timestr, sizeof(timestr));
            printf("Current time: %s\n\n", timestr);
        }
    }

    raw_mode_disable();
    fcntl(STDIN_FILENO, F_SETFL, flags);
    show_record(device_fd);
    close(device_fd);

    return 0;
}
