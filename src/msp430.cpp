#include "msp430.hpp"

#include <bit>
#include <cstdint>
#include <elf.h>
#include <string.h>
#include <stdexcept>
#include <stdio.h>

void MSP430::load_file(const char* path)
{
    struct Closer { void operator()(FILE* p) { fclose(p); }};
    auto fp = std::unique_ptr<FILE, Closer>(fopen(path, "rb"));

    if (fp == nullptr)
        throw std::runtime_error(strerror(errno));

    auto read_into = [&](void* ptr, size_t len, size_t offset){
        // fprintf(stderr, "read %zu: %zu  ->  %p\n", offset, len, ptr);

        if (fseek(fp.get(), offset, SEEK_SET) == -1)
            throw std::runtime_error(strerror(errno));

        auto dst = static_cast<char*>(ptr);
        while (len > 0) {
            auto n = fread(dst, 1, len, fp.get());
            if (n == 0) {
                if (feof(fp.get()))
                    throw std::runtime_error("Unexpected end-of-file");
                else
                    throw std::runtime_error(strerror(errno));
            }
            len -= n;
            dst += n;
        }
    };

    Elf32_Ehdr header;
    read_into(&header, sizeof(header), 0);

    if (header.e_machine != EM_MSP430)
        throw std::runtime_error("Bad e_machine value");

    if (header.e_phentsize != sizeof(Elf32_Phdr))
        throw std::runtime_error("Bad e_phentsize value");

    memset(ram->data(), 0, ram->size());

    for (size_t i=0; i<header.e_phnum; i++) {
        Elf32_Phdr program;
        read_into(&program, sizeof(program), header.e_phoff + i * sizeof(program));

        if (program.p_type != PT_LOAD)
            continue;

        if (program.p_filesz + program.p_paddr > RAM_SIZE)
            throw std::runtime_error("LOAD segment too large");

        auto* memp = ram->data();

        read_into(memp + program.p_paddr, program.p_filesz, program.p_offset);
    }

    memset(&registers, 0, sizeof(registers));
    registers[PC] = header.e_entry;
}

void MSP430::print(std::span<char, PRINT_LENGTH> out) const
{
    static constexpr char print_template[PRINT_LENGTH] = 
        " pc ____  sp ____  sr ____ cg2 ____\n"
        " r4 ____  r5 ____  r6 ____  r7 ____\n"
        " r8 ____  r9 ____ r10 ____ r11 ____\n"
        "r12 ____ r13 ____ r14 ____ r15 ____\n"
        "flags _____\n";

    static constexpr char hex[] = "0123456789abcdef";
    
    memcpy(out.data(), print_template, PRINT_LENGTH);

    auto print_u16 = [](char* dest, uint16_t value) {
        for (int i=0; i<4; i++) {
            dest[3-i] = hex[value & 0xf];
            value >>= 4;
        }
    };

    for (int y=0; y<4; y++) {
        for (int x=0; x<4; x++) {
            print_u16(&out[36*y + 9*x + 4], registers[4*y + x]);
        }
    }

    if (registers[SR] & CF) out[150] = 'C';
    if (registers[SR] & ZF) out[151] = 'Z';
    if (registers[SR] & NF) out[152] = 'N';
    if (registers[SR] & VF) out[153] = 'V';
    if (registers[SR] & IF) out[154] = 'I';
}

using Error = std::runtime_error;
using RAM = MSP430::RAM;
using enum MSP430::Registers;
using enum MSP430::Flags;

// Byte/word instruction mode selector

enum ByteWord : bool {
    Byte = true,
    Word = false,
};

template <ByteWord mode>
struct Constants {
    static const uint32_t mask;
    static const uint32_t sign;
    static const uint32_t carry;
    static const uint16_t size;
};

template<> const uint32_t Constants<Byte>::mask = 0xff;
template<> const uint32_t Constants<Byte>::sign = 0x80;
template<> const uint32_t Constants<Byte>::carry = 0x100;
template<> const uint16_t Constants<Byte>::size = 1;

template<> const uint32_t Constants<Word>::mask = 0xffff;
template<> const uint32_t Constants<Word>::sign = 0x8000;
template<> const uint32_t Constants<Word>::carry = 0x10000;
template<> const uint16_t Constants<Word>::size = 2;

// MMIO

static constexpr uint16_t MMIO_BASE = 0xff00;
static constexpr uint16_t MMIO_UART = 0xffa2;
static constexpr uint16_t MMIO_EXIT = 0xfffe;

template <ByteWord mode>
static uint16_t
read_mmio(uint16_t address)
{
    if constexpr (mode == Byte)
        throw Error("MMIO accessed in byte-mode");

    if (address & 1)
        throw Error("Misaligned MMIO read");

    if (address == MMIO_UART) {
        return uint8_t(MSP430::uart_read());
    }

    throw Error("Read from unknown MMIO device");
}

template <ByteWord mode>
static void
write_mmio(uint16_t address, uint16_t value)
{
    if constexpr (mode == Byte)
        throw Error("MMIO accessed in byte-mode");

    if (address & 1)
        throw Error("Misaligned MMIO write");

    switch (address) {
        case MMIO_UART:
            MSP430::uart_print(value);
            return;
        case MMIO_EXIT:
            throw Error("MMIO exit triggered");
    }

    throw Error("Write to unknown MMIO device");
}

// Memory accessors

template <ByteWord mode>
static inline uint16_t
read_ram(const RAM& ram, uint16_t address)
{
    // printf("Read (b=%i) 0x%04x\n", mode==Byte, address);

    if (address >= MMIO_BASE)
        return read_mmio<mode>(address);

    if constexpr (mode == Word) {
        if (address & 1)
            throw Error("Misaligned read");
        return *reinterpret_cast<const uint16_t*>(&ram[address]);
    } else {
        return ram[address];
    }
}

template <ByteWord mode>
static inline void
write_ram(RAM& ram, uint16_t address, uint16_t value)
{
    // printf("Write (b=%i) 0x%04x <- 0x%04x\n", mode==Byte, address, value);

    if (address >= MMIO_BASE)
        return write_mmio<mode>(address, value);

    if constexpr (mode == Word) {
        if (address & 1)
            throw Error("Misaligned write");
        *reinterpret_cast<uint16_t*>(&ram[address]) = value;
    } else {
        ram[address] = value;
    }
}

static inline uint16_t
read_pc_immediate(MSP430& msp)
{
    auto v = read_ram<Word>(*msp.ram, msp.registers[PC]);
    msp.registers[PC] += 2;
    return v;
}

// Argument Decoding

#define unreachable __builtin_trap

template <ByteWord mode>
static inline uint16_t
dual_op_source(MSP430& msp, uint16_t instruction)
{
    auto op = std::bit_cast<MSP430::DualOpInsn>(instruction);

    if (op.source == PC) {
        switch (uint16_t(op.as)) {
            case 2: throw Error("Unsupported @PC immediate mode");
            case 3: return read_pc_immediate(msp);

            /* case 0, 1 as regular register -- fall through */
            default: (void)0;
        }
    }

    if (op.source == SR) { /* a.k.a. CG1 */
        switch (op.as) {
            case 0: return msp.registers[SR];
            case 2: return 4;
            case 3: return 8;
            case 1: {
                auto address = read_pc_immediate(msp);
                return read_ram<mode>(*msp.ram, address);
            }
        }
        unreachable();
    }

    if (op.source == CG) { /* a.k.a. CG2 */
        static constexpr uint16_t constants[4] = { 0, 1, 2, 0xffff };
        return constants[op.as];
    }

    switch (op.as) {
        case 0: return msp.registers[op.source];
        case 1: {
            auto base = msp.registers[op.source];
            auto offset = read_pc_immediate(msp);
            return read_ram<mode>(*msp.ram, base + offset);
        }
        case 2: {
            auto address = msp.registers[op.source];
            return read_ram<mode>(*msp.ram, address);
        }
        case 3: {
            auto address = msp.registers[op.source];
            if (mode == Byte && op.source == SP)
                msp.registers[SP] += 2; // POP always keeps stack aligned
            else
                msp.registers[op.source] += Constants<mode>::size;
            return read_ram<mode>(*msp.ram, address);
        }
    }
    unreachable();
}

struct Destination {
    uint16_t target;
    bool is_memory;

    template <ByteWord mode>
    void write(MSP430& msp, uint16_t value) {
        if (is_memory)
            write_ram<mode>(*msp.ram, target, value);
        else
            msp.registers[target] = Constants<mode>::mask & value;
    }

    template <ByteWord mode>
    uint16_t read(MSP430& msp) {
        if (is_memory)
            return read_ram<mode>(*msp.ram, target);
        else
            return Constants<mode>::mask & msp.registers[target];
    }
};

static Destination
dual_op_dest(MSP430& msp, uint16_t instruction)
{
    auto op = std::bit_cast<MSP430::DualOpInsn>(instruction);

    if (op.ad == 0)
        return { op.dest, false };

    if (op.dest == SR)
        return { read_pc_immediate(msp), true };

    if (op.dest == CG)
        throw Error("Illegal x(CG2) address mode");

    auto base = msp.registers[op.dest];
    auto offset = read_pc_immediate(msp);
    return { uint16_t(base + offset), true };
}

static Destination
single_op_loc(MSP430& msp, uint16_t instruction)
{
    auto op = std::bit_cast<MSP430::SingleOpInsn>(instruction);

    if (op.as == 0)
        return { op.target, false };

    if (op.target == CG)
        throw Error("Illegal target register CG2");

    if (op.target == SR) {
        if (op.as == 1)
            return { read_pc_immediate(msp), true };
        else
            throw Error("Illegal target register CG1");
    }

    switch (op.as) {
        case 1: {
            auto base = msp.registers[op.target];
            auto offset = read_pc_immediate(msp);
            return { uint16_t(base + offset), true };
        }
        case 2:
            return { msp.registers[op.target], true };
        case 3: {
            auto address = msp.registers[op.target];
            if (op.bw && (op.target > SP))
                msp.registers[op.target] += 1;
            else
                msp.registers[op.target] += 2;
            return { address, true };
        }
    }
    unreachable();
}

// Execution

static void
flags_update(MSP430& msp, bool carry, bool zero, bool sign, bool overflow)
{
    msp.registers[SR] =
        (msp.registers[SR] & ~ALU)
        | (carry * CF)
        | (zero * ZF)
        | (sign * NF)
        | (overflow * VF);
}

template <ByteWord mode>
static void
alu_flags_update(MSP430& msp, bool s1_in, bool s2_in, uint32_t out)
{
    bool sign_out = out & Constants<mode>::sign;
    bool carry_out = out & Constants<mode>::carry;
    bool zero_out = !(out & Constants<mode>::mask);
    bool overflow_out = (s1_in ^ sign_out) & (s2_in ^ sign_out);

    flags_update(msp, carry_out, zero_out, sign_out, overflow_out);
}

enum DualOpCode {
    MOV = 0x4,
    ADD,
    ADDC,
    SUBC,
    SUB,
    CMP,
    DADD, /* unsupported */
    BIT,
    BIC,
    BIS, /* a.k.a OR */
    XOR,
    AND,
};

template <ByteWord mode>
static void
execute_decoded_dual_op(MSP430& msp, DualOpCode op, uint16_t source, Destination dest)
{
    if (op == MOV) {
        dest.write<mode>(msp, source);
        return;
    }

    bool carry_in = msp.registers[SR] & CF;
    bool sign1_in = source & Constants<mode>::sign;
    uint32_t target = dest.read<mode>(msp);
    bool sign2_in = target & Constants<mode>::sign;

    switch (op) {
        case MOV:
            unreachable();

        case ADD:
            target = target + source;
            alu_flags_update<mode>(msp, sign1_in, sign2_in, target);
            dest.write<mode>(msp, target);
            break;

        case ADDC:
            target = target + source + carry_in;
            alu_flags_update<mode>(msp, sign1_in, sign2_in, target);
            dest.write<mode>(msp, target);
            break;

        case SUBC:
            target = target + (uint16_t)~source + carry_in;
            alu_flags_update<mode>(msp, not sign1_in, sign2_in, target);
            dest.write<mode>(msp, target);
            break;

        case SUB:
            target = target + (uint16_t)~source + 1;
            alu_flags_update<mode>(msp, not sign1_in, sign2_in, target);
            dest.write<mode>(msp, target);
            break;

        case CMP:
            target = target + (uint16_t)~source + 1;
            alu_flags_update<mode>(msp, not sign1_in, sign2_in, target);
            break;

        case DADD:
            throw Error("DADD not implemented");

        case BIT:
            target = target & source;
            alu_flags_update<mode>(msp, not sign1_in, sign2_in, target);
            break;

        case BIC:
            target = target & ~source;
            dest.write<mode>(msp, target);
            break;

        case BIS:
            target = target | source;
            dest.write<mode>(msp, target);
            break;

        case XOR:
            target = target ^ source;
            alu_flags_update<mode>(msp, sign1_in, sign2_in, target);
            dest.write<mode>(msp, target);
            break;

        case AND:
            target = target & source;
            alu_flags_update<mode>(msp, sign1_in, sign2_in, target);
            dest.write<mode>(msp, target);
            break;

        default:
            throw Error("Invalid opcode for dual operand instruction");
    }
}

enum SingleOpCode {
    RRC = 0x0,
    SWPB,
    RRA,
    SXT,
    PUSH,
    CALL,
    RETI,
};

template <ByteWord mode>
static void
execute_decoded_single_op(MSP430& msp, SingleOpCode op, uint16_t instruction)
{
    switch (op) {
        case PUSH: {
            msp.registers[SP] -= 2;
            auto value = single_op_loc(msp, instruction).read<mode>(msp);
            write_ram<mode>(*msp.ram, msp.registers[SP], value);
            break;
        }
        case CALL: {
            auto dest = single_op_loc(msp, instruction).read<Word>(msp);
            msp.registers[SP] -= 2;
            write_ram<Word>(*msp.ram, msp.registers[SP], msp.registers[PC]);
            msp.registers[PC] = dest;
            break;
        }
        case SWPB: {
            auto target = single_op_loc(msp, instruction);
            target.write<Word>(msp, __builtin_bswap16(target.read<Word>(msp)));
            break;
        }
        case RETI: {
            if (instruction & 0x3f)
                throw Error("Illegal argument for RETI");

            msp.registers[SR] = read_ram<Word>(*msp.ram, msp.registers[SP]);
            msp.registers[PC] = read_ram<Word>(*msp.ram, msp.registers[SP] + 2);
            msp.registers[SP] += 4;
            break;
        }
        case RRC: {
            auto target = single_op_loc(msp, instruction);
            bool carry_in = msp.registers[SR] & CF;
            uint32_t value = target.read<mode>(msp) | carry_in * Constants<mode>::carry;
            bool carry_out = value & 1;
            value >>= 1;
            bool sign_out = value & Constants<mode>::sign;
            bool zero_out = value == 0;
            target.write<mode>(msp, value);
            flags_update(msp, carry_out, zero_out, sign_out, 0);
            break;
        }
        case RRA: {
            auto target = single_op_loc(msp, instruction);
            uint32_t value = target.read<mode>(msp);
            bool carry_in = value & Constants<mode>::sign;
            value |= carry_in * Constants<mode>::carry;
            bool carry_out = value & 1;
            value >>= 1;
            bool sign_out = value & Constants<mode>::sign;
            bool zero_out = value == 0;
            target.write<mode>(msp, value);
            flags_update(msp, carry_out, zero_out, sign_out, 0);
            break;
        }
        case SXT: {
            auto target = single_op_loc(msp, instruction);
            uint16_t value = target.read<Byte>(msp);
            value = int16_t(int8_t(value));
            target.write<Word>(msp, value);
            bool sign_out = value & Constants<Word>::sign;
            bool zero_out = value == 0;
            bool carry_out = value != 0;
            flags_update(msp, carry_out, zero_out, sign_out, 0);
            break;
        }
        default:
            unreachable();
    }
}

enum Condition {
    not_equal = 0x0,
    equal, // == zero
    no_carry, // == lower
    carry, // == higher_or_same
    negative,
    greater_equal,
    less,
    always,
};

static bool
is_condition(uint16_t flags, Condition cond)
{
    switch (cond) {
        case not_equal:     return not(flags & ZF);
        case equal:         return flags & ZF;
        case no_carry:      return not(flags & CF);
        case carry:         return flags & CF;
        case negative:      return flags & NF;
        case greater_equal: return bool(flags & NF) == bool(flags & VF);
        case less:          return bool(flags & NF) != bool(flags & VF);
        case always:        return true;
    }
    unreachable();
}

static void
execute_conditional_op(MSP430& msp, uint16_t instruction)
{
    auto op = std::bit_cast<MSP430::ConditionalInsn>(instruction);

    if (is_condition(msp.registers[SR], Condition(op.condition)))
        msp.registers[PC] += uint16_t(int16_t(op.offset)) << 1;
}

static void
execute_dual_op(MSP430& msp, uint16_t instruction)
{
    auto op = std::bit_cast<MSP430::DualOpInsn>(instruction);

    if (op.bw) {
        auto source = dual_op_source<Byte>(msp, instruction);
        auto dest = dual_op_dest(msp, instruction);
        execute_decoded_dual_op<Byte>(msp, DualOpCode(op.opcode), source, dest);
    } else {
        auto source = dual_op_source<Word>(msp, instruction);
        auto dest = dual_op_dest(msp, instruction);
        execute_decoded_dual_op<Word>(msp, DualOpCode(op.opcode), source, dest);
    }
}

static void
execute_single_op(MSP430& msp, uint16_t instruction)
{
    auto op = std::bit_cast<MSP430::SingleOpInsn>(instruction);
    if (op.bw) {
        execute_decoded_single_op<Byte>(msp, SingleOpCode(op.opcode), instruction);
    } else {
        execute_decoded_single_op<Word>(msp, SingleOpCode(op.opcode), instruction);
    }
}

void MSP430::step_instruction()
{
    // printf("%04x: ", registers[PC]);

    auto instruction = read_pc_immediate(*this);
    auto instruction_type = classify(instruction);

    // printf("%04x %i\n", instruction, instruction_type);

    switch (instruction_type) {
        case invalid:
            throw Error("Illegal instruction");
        case single_operand:
            execute_single_op(*this, instruction);
            break;
        case conditional:
            execute_conditional_op(*this, instruction);
            break;
        case dual_operand:
            execute_dual_op(*this, instruction);
            break;
    }

    // puts(print_array().data());
}

#ifdef MSP430TEST

void MSP430::uart_print(char c) {
    throw Error("IO operation in test");
}

char MSP430::uart_read() {
    throw Error("IO operation in test");
}

static void test_alu2_word()
{
    MSP430 m{};
    int successes{};

    struct TestCase {
        DualOpCode opp;
        uint16_t src, dst, flags;
        uint16_t expected, flags_out;
    };

    static constexpr struct TestCase tests[] = {
        TestCase( ADD, 1, 1, 0, 2, 0 ),
        TestCase( SUB, 1, 2, 0, 1, CF ),
        TestCase( SUB, 1, 1, 0, 0, ZF|CF ),
        TestCase( CMP, 1, -1, 0, -1, NF|CF ),
        TestCase( SUB, 1, -1, 0, -2, NF|CF ),
        TestCase( ADD, 30000, 30000, 0, 60000, VF|NF ),
        TestCase( SUB, 30000, -30000, 0, 5536, VF|CF ),
    };

    for (auto& test : tests) {
        MSP430::DualOpInsn insn = {
            .dest = 5,
            .as = 0,
            .bw = 0,
            .ad = 0,
            .source = 4,
            .opcode = test.opp,
        };
        write_ram<Word>(*m.ram, 0, std::bit_cast<uint16_t>(insn));

        m.registers[PC] = 0;
        m.registers[SR] = test.flags;
        m.registers[4] = test.src;
        m.registers[5] = test.dst;

        try {
            m.step_instruction();
            if (m.registers[5] != test.expected) {
                printf(
                    "ALU2 test fail (result mismatch)\n"
                    "\topp %x src %i (%04x) dst %i (%04x)\n"
                    "\texpected %i (%04x) actual %i (%04x)\n",
                    test.opp, test.src, test.src, test.dst, test.dst,
                    test.expected, test.expected,
                    m.registers[5], m.registers[5]
                );
            } else if (m.registers[SR] != test.flags_out) {
                printf(
                    "ALU2 test fail (flags mismatch)\n"
                    "\topp %x src %i (%04x) dst %i (%04x)\n"
                    "\texpected %04x actual %04x\n",
                    test.opp, test.src, test.src, test.dst, test.dst,
                    test.flags_out, m.registers[SR]
                );
            } else {
                successes++;
            }
        } catch (std::exception& e) {
            printf(
                "ALU2 test fail (exception thrown)\n"
                "\topp %x src %i (%04x) dst %i (%04x)\n"
                "\texception %s\n",
                test.opp, test.src, test.src, test.dst, test.dst,
                e.what()
            );
        }
    };

    printf("ALU2 count %i success %i\n", std::size(tests), successes);
}

int main()
{
    test_alu2_word();
}

#endif
