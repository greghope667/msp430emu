#include "msp430.hpp"
#include <notcurses/notcurses.h>
#include <stdio.h>

static notcurses* nc;
static ncplane* nplane;
static MSP430 msp430;
static std::string uart_out{};

void MSP430::uart_print(char c) {
    uart_out += c;
}

char MSP430::uart_read() {
    return -1;
}

static bool console_init()
{
    if (not (nc = notcurses_core_init(nullptr, nullptr))) {
        fprintf(stderr, "Failed to initialise notcurses\n");
        return false;
    }

    if (not (nplane = notcurses_stdplane(nc))) {
        fprintf(stderr, "Failed to create ncplane\n");
        return false;
    }

    return true;
}

static void console_run()
{
    for (;;) {
        ncplane_cursor_move_yx(nplane, 0, 0);
        ncplane_puttext(nplane, 2, NCALIGN_LEFT, msp430.print_array().data(), nullptr);
        ncplane_puttext(nplane, 8, NCALIGN_LEFT, uart_out.c_str(), nullptr);
        notcurses_render(nc);

        ncinput input;
        if (notcurses_get_blocking(nc, &input) == uint32_t(-1))
            break;

        if (input.id == 's') {
            try {
                msp430.step_instruction();
            } catch (std::runtime_error& e) {
                ncplane_puttext(nplane, 0, NCALIGN_LEFT, e.what(), nullptr);
            }
        }
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

    if (console_init())
        console_run();

    if (nc)
        notcurses_stop(nc);
}