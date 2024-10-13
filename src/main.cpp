#include <stdio.h>

#include "msp430.hpp"

int main(int, char** argv)
{
	puts("=== msp430emu-cli ===");

    MSP430 msp430{};

    try {
        msp430.load_file(argv[1]);
    } catch (std::exception& e) {
        fprintf(stderr, "Failed to load file '%s', reason: %s\n", argv[1], e.what());
        exit(1);
    }

	size_t instruction_counter = 0;

	try {
		for (;;) {
			msp430.step_instruction();
			instruction_counter++;
		}
	} catch (std::exception& e) {
		fprintf(
			stderr, "Terminated after %zu steps\nReason: %s\nState:\n%s\n", 
			instruction_counter, e.what(), msp430.print_array().data());
	}
}