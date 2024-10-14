#include "msp430.hpp"
#include <string>
#include <termbox2.h>

static std::string uart_out{};
static MSP430 msp430{};

void MSP430::uart_print(char c) {
    uart_out += c;
}

char MSP430::uart_read() {
    return -1;
}

static void fill(int x, int y, int w, int h, uintattr_t bg)
{
    for (int i=0; i<w; i++) {
        for (int j=0; j<h; j++) {
            tb_set_cell(x+i, y+j, ' ', bg, bg);
        }
    }
}

static void box(int x, int y, int w, int h, uintattr_t fg, uintattr_t bg)
{
    w--, h--;

    tb_set_cell(x  , y  , '+', fg, bg);
    tb_set_cell(x+w, y  , '+', fg, bg);
    tb_set_cell(x  , y+h, '+', fg, bg);
    tb_set_cell(x+w, y+h, '+', fg, bg);

    for (int i=1; i<w; i++) {
        tb_set_cell(x+i, y  , '-', fg, bg);
        tb_set_cell(x+i, y+h, '-', fg, bg);
    }

    for (int i=1; i<h; i++) {
        tb_set_cell(x  , y+i, '|', fg, bg);
        tb_set_cell(x+w, y+i, '|', fg, bg);
    }
}

static uint16_t memdump_address{};

static void memdump()
{

    for (unsigned i=0; i<16; i++) {
        auto line_start = memdump_address + i * 16;

        if (line_start >= MSP430::RAM_SIZE) {
            char line[100];
            memset(line, ' ', sizeof(line));
            line[sizeof(line)-1] = 0;
            tb_print(2, 12+i, TB_BLACK, TB_BLACK, line);
            continue;
        }

        tb_printf(2, 12+i, TB_DEFAULT, TB_BLACK, "% 4x:", line_start);

        for (int j=0; j<16; j++) {
            unsigned char ch = msp430.ram->data()[line_start + j];
            unsigned char pch = ch;
            uintattr_t fg = TB_GREEN;

            if (ch==0) {
                pch = '.';
                fg = TB_DEFAULT|TB_DIM;
            } else if (not isprint(ch)) {
                pch = ',';
                fg = TB_DEFAULT;
            }

            tb_printf(2+6+3*j, 12+i, fg, TB_BLACK, "%02x", ch);
            tb_set_cell(2+6+3*16+2+j, 12+i, pch, fg, TB_BLACK);
        }
    }
}

static bool handle_event(tb_event e)
{
    switch (e.ch) {
        case 's': 
            try {
                msp430.step_instruction();
            } catch (std::runtime_error& e) {
                tb_print(2, 10, TB_BLACK, TB_RED, e.what());
            }
            break;
        case 'c':
        case 'q':
            return false;
        case 'r':
            msp430.registers[MSP430::PC] = 0;
            fill(2, 10, 40, 1, TB_BLACK);
            break;
        case 'j':
            memdump_address += 16;
            break;
        case 'k':
            memdump_address -= 16;
            break;
        case 'u':
            memdump_address -= 16*16;
            break;
        case 'd':
            memdump_address += 16*16;
            break;
    }
    return true;
}

static void console_run()
{
    fill(1, 1, 39, 7, TB_BLUE);
    box(1, 1, 39, 7, TB_YELLOW, TB_BLUE);
    for (int i=0;; i++) {
        tb_printf(0, 0, TB_DEFAULT, TB_BLACK, "%i", i);
        tb_print(3, 2, TB_WHITE, TB_BLUE, msp430.print_array().data());
        tb_print(2, 8, TB_GREEN, TB_BLACK, uart_out.c_str());
        memdump();
        tb_present();

        tb_event ev;
        if (tb_poll_event(&ev) != TB_OK)
            return;

        if (not handle_event(ev))
            return;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 0;
    }

    try {
        msp430.load_file(argv[1]);
    } catch (std::exception& e) {
        fprintf(stderr, "Failed to load file '%s', reason: %s\n", argv[1], e.what());
        return 1;
    }

    if (tb_init() != TB_OK) {
        fprintf(stderr, "Failed to initialise termbox\n");
        exit(1);
    }

    console_run();
    tb_shutdown();
}