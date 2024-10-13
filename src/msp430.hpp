#pragma once
#include <stdint.h>
#include <array>
#include <memory>
#include <span>

struct MSP430 {
    static constexpr size_t RAM_SIZE = 0x10000;
    using RAM = std::array<uint8_t, RAM_SIZE>;

    using Instruction = uint16_t;

    enum InstructionClass {
        invalid,
        single_operand,
        conditional,
        dual_operand,
    };

    struct SingleOpInsn {
        uint16_t target: 4;
        uint16_t as: 2;
        uint16_t bw: 1;
        uint16_t opcode: 3;
        uint16_t b000100: 6;
    };

    struct ConditionalInsn {
        int16_t offset: 10;
        uint16_t condition: 3;
        uint16_t b001: 3;
    };

    struct DualOpInsn {
        uint16_t dest: 4;
        uint16_t as: 2;
        uint16_t bw: 1;
        uint16_t ad: 1;
        uint16_t source: 4;
        uint16_t opcode: 4;
    };

    enum Registers : uint8_t {
        PC, SP, SR, CG
    };

    enum Flags : uint16_t {
        CF = 1U << 0,
        ZF = 1U << 1,
        NF = 1U << 2,
        IF = 1U << 3,
        VF = 1U << 8,

        ALU = CF|ZF|NF|VF,
    };

    uint16_t registers[16] = {};
    std::unique_ptr<RAM> ram = std::make_unique<RAM>();

    void load_file(const char* path); // Throws on failure
    void step_instruction();

    static constexpr size_t PRINT_LENGTH = 157;

    void print(std::span<char, PRINT_LENGTH> out) const;
    auto print_array() {
        std::array<char, PRINT_LENGTH> arr{};
        print(arr);
        return arr;
    }

    // IO for uart - user implementation required
    static void uart_print(char);
    static char uart_read();

private:
    InstructionClass classify(uint16_t instruction) const {
        switch((instruction >> 12) & 0xf) {
            case 0:         return invalid;
            case 1:         return single_operand;
            case 2 ... 3:   return conditional;
            case 4 ... 15:  return dual_operand;
        }
        __builtin_unreachable();
    }
};
