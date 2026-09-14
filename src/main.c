#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <time.h>
#include "input.c"


#define NUM_REGS 32
#define REG_PC 0
#define REG_SP 1
#define INT_TABLE_START (uint32_t)1024
#define INT_TABLE_SIZE (uint32_t)256 // 256 * 4 = 1024
#define SUB_INSTRUCTION_COUNT (1024 * 1024 * 4) // RECOMENDED TO ADJUST FOR BETTER/WORSE COMPUTERS!

typedef struct {
    uint32_t regs[NUM_REGS];
    uint8_t *ram;
    uint32_t ram_size;
} CpuData;

typedef struct {
    uint32_t buffered_input;
} TerminalIoDeviceData;

typedef struct {
    TerminalIoDeviceData *terminal_io_device_data;
} DevicesData;


int init_devices_data(DevicesData *devices_data) {
    devices_data->terminal_io_device_data = malloc(sizeof(TerminalIoDeviceData));

    if (devices_data->terminal_io_device_data == 0) {
        printf("FAILED TO ALLOCATE TerminalIoDeviceData STRUCT\n");
        return 1;
    }

    return 0;
}


void snd_to_device(DevicesData *devices_data, uint32_t port, uint32_t msg_reg) {
    switch (port)
    {
        case 0: // NULL device
            break;
    
        case 1: // Terminal IO device
            break;
        
        default:
            break;
    }
}


uint32_t rcv_from_device(DevicesData *devices_data, uint32_t port) {
    switch (port)
    {
        case 0: // NULL device
            break;
        
        case 1: // Terminal IO device
            {
                uint32_t input = devices_data->terminal_io_device_data->buffered_input;
                devices_data->terminal_io_device_data->buffered_input = 0;
                return input;
            }
            break;
        
        default:
            break;
    }
    return 0;
}


void update_devices(DevicesData *devices_data) {
    // NULL device
    { }

    // Terminal IO device
    {
        uint32_t input = get_input_nb();

        if (input) {
            devices_data->terminal_io_device_data->buffered_input = input;
        }
    }
}


int cpu_push_stack_uint8(CpuData *cpu_data, uint8_t value) {
    uint32_t sp = cpu_data->regs[REG_SP];

    if (sp == 0) {
        return 1; // Prevent stack underflow
    }

    sp--;
    cpu_data->ram[sp] = value;
    cpu_data->regs[REG_SP] = sp; // Update SP register

    return 0;
}

int cpu_push_stack_uint16(CpuData *cpu_data, uint16_t value) {
    uint32_t sp = cpu_data->regs[REG_SP];

    if (cpu_push_stack_uint8(cpu_data, value)) return 1;
    if (cpu_push_stack_uint8(cpu_data, value >> 8)) return 1;
}

int cpu_push_stack_uint32(CpuData *cpu_data, uint32_t value) {
    uint32_t sp = cpu_data->regs[REG_SP];

    if (cpu_push_stack_uint8(cpu_data, value)) return 1;
    if (cpu_push_stack_uint8(cpu_data, value >> 8)) return 1;
    if (cpu_push_stack_uint8(cpu_data, value >> 16)) return 1;
    if (cpu_push_stack_uint8(cpu_data, value >> 24)) return 1;
}

int cpu_pop_stack_uint8(CpuData *cpu_data, uint8_t *value) {
    uint32_t sp = cpu_data->regs[REG_SP];

    if (sp >= cpu_data->ram_size) {
        return 1;
    }

    *value = cpu_data->ram[sp];
    sp++;
    cpu_data->regs[REG_SP] = sp;

    return 0;
}

int cpu_pop_stack_uint16(CpuData *cpu_data, uint16_t *value) {
    uint8_t low = 0;
    uint8_t high = 0;

    if (cpu_pop_stack_uint8(cpu_data, &low)) return 1;
    if (cpu_pop_stack_uint8(cpu_data, &high)) return 1;

    *value = (uint16_t)(((uint16_t)high << 8) | low);
    return 0;
}

int cpu_pop_stack_uint32(CpuData *cpu_data, uint32_t *value) {
    uint8_t b0 = 0;
    uint8_t b1 = 0;
    uint8_t b2 = 0;
    uint8_t b3 = 0;

    if (cpu_pop_stack_uint8(cpu_data, &b0)) return 1;
    if (cpu_pop_stack_uint8(cpu_data, &b1)) return 1;
    if (cpu_pop_stack_uint8(cpu_data, &b2)) return 1;
    if (cpu_pop_stack_uint8(cpu_data, &b3)) return 1;

    *value = ((uint32_t)b3 << 24) |
             ((uint32_t)b2 << 16) |
             ((uint32_t)b1 << 8) |
             (uint32_t)b0;

    return 0;
}

uint32_t cpu_get_reg(CpuData *cpu_data, uint8_t reg) {
    if (reg < NUM_REGS) {
        return cpu_data->regs[(int)reg];
    }
}

void cpu_set_reg(CpuData *cpu_data, uint8_t reg, uint32_t value) {
    if (reg < NUM_REGS) {
        cpu_data->regs[(int)reg] = value;
    }
}

int cpu_make_interrupt(CpuData *cpu_data, uint8_t value) {
    cpu_push_stack_uint32(cpu_data, cpu_data->regs[REG_PC]);

    if (value < INT_TABLE_SIZE) {
        uint32_t int_field_start = INT_TABLE_START + value * 4;
        uint32_t int_address = cpu_data->ram[int_field_start] | (cpu_data->ram[int_field_start + 1] << 8) | (cpu_data->ram[int_field_start + 2] << 16) | (cpu_data->ram[int_field_start + 3] << 24);

        cpu_data->regs[REG_PC] = int_address;
    }
    else {
        printf("INVALID INTERRUPT 0x%02x\n", value);
    }
}

void cpu_dump_registers(CpuData *cpu_data) {
    printf("\n-------- CPU REG DUMP --------\n");
    for (int i = 0; i < NUM_REGS; i++) {
        if (i == REG_PC) {
            printf("PC   : 0x%08x | %12d\n", cpu_data->regs[i], cpu_data->regs[i]);
        }
        else if (i == REG_SP) {
            printf("SP   : 0x%08x | %12d\n", cpu_data->regs[i], cpu_data->regs[i]);
        }
        else {
            printf("0x%02x : 0x%08x | %12d\n", i, cpu_data->regs[i], cpu_data->regs[i]);
        }
    }
}

int tick_cpu(CpuData *cpu_data, DevicesData *devices_data, bool debug) {
    if (cpu_data->regs[REG_PC] > cpu_data->ram_size) {
        cpu_data->regs[REG_PC] = 0;
    }

    uint32_t pc = cpu_data->regs[REG_PC];
    
    uint8_t opcode = cpu_data->ram[pc];

    if (debug) {
        cpu_dump_registers(cpu_data);
        printf("INST: %02x\n", opcode);
    }

    switch (opcode) {
        case 0x00: // NOP
            cpu_data->regs[REG_PC]++;
            break;

        case 0x01: // CALL
            {
                cpu_data->regs[REG_PC] += 5;
                
                uint32_t value = cpu_data->ram[pc + 1];
                value |= cpu_data->ram[pc + 2] << 8;
                value |= cpu_data->ram[pc + 3] << 16;
                value |= cpu_data->ram[pc + 4] << 24;

                cpu_push_stack_uint32(cpu_data, cpu_data->regs[REG_PC]);

                cpu_data->regs[REG_PC] = value;
            }
            break;
        
        case 0x02: // RET
            {
                uint32_t jump;

                cpu_pop_stack_uint32(cpu_data, &jump);

                cpu_data->regs[REG_PC] = jump;
            }
            break;

        case 0x03: // INT
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t value = cpu_data->ram[pc + 1];

                return cpu_make_interrupt(cpu_data, value);
            }
            break;
        
        case 0x04: // MOV
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t from = cpu_data->ram[pc + 1];
                uint8_t to = cpu_data->ram[pc + 2];

                cpu_set_reg(cpu_data, to, cpu_get_reg(cpu_data, from));
            }
            break;
        
        // free space

        case 0x08: // PUSH8
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_data->ram[pc + 1];
                cpu_push_stack_uint8(cpu_data, cpu_get_reg(cpu_data, reg));
            }
            break;

        case 0x09: // PUSH16
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_data->ram[pc + 1];
                cpu_push_stack_uint16(cpu_data, cpu_get_reg(cpu_data, reg));
            }
            break;

        case 0x0a: // PUSH32
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_data->ram[pc + 1];
                cpu_push_stack_uint32(cpu_data, cpu_get_reg(cpu_data, reg));
            }
            break;

        case 0x0b: // POP8
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint8_t val;
                cpu_pop_stack_uint8(cpu_data, &val);

                cpu_set_reg(cpu_data, reg, val);
            }
            break;

        case 0x0c: // POP16
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint16_t val;
                cpu_pop_stack_uint16(cpu_data, &val);

                cpu_set_reg(cpu_data, reg, val);
            }
            break;

        case 0x0d: // POP32
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint32_t val;
                cpu_pop_stack_uint32(cpu_data, &val);

                cpu_set_reg(cpu_data, reg, val);
            }
            break;
        
        case 0x10: // LDI8
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint8_t value = cpu_data->ram[pc + 2];

                cpu_set_reg(cpu_data, reg, value);
            }
            break;
        
        case 0x11: // LDI16
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint8_t value = cpu_data->ram[pc + 2];
                value |= cpu_data->ram[pc + 3] << 8;

                cpu_set_reg(cpu_data, reg, value);
            }
            break;
        
        case 0x12: // LDI32
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint32_t value = cpu_data->ram[pc + 2];
                value |= cpu_data->ram[pc + 3] << 8;
                value |= cpu_data->ram[pc + 4] << 16;
                value |= cpu_data->ram[pc + 5] << 24;

                cpu_set_reg(cpu_data, reg, value);
            }
            break;
        
        case 0x14: // LD8
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint32_t addr = cpu_data->ram[pc + 2];
                addr |= cpu_data->ram[pc + 3] << 8;
                addr |= cpu_data->ram[pc + 4] << 16;
                addr |= cpu_data->ram[pc + 5] << 24;

                cpu_set_reg(cpu_data, reg, cpu_data->ram[addr]);
            }
            break;
        
        case 0x15: // LD16
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint32_t addr = cpu_data->ram[pc + 2];
                addr |= cpu_data->ram[pc + 3] << 8;
                addr |= cpu_data->ram[pc + 4] << 16;
                addr |= cpu_data->ram[pc + 5] << 24;

                cpu_set_reg(cpu_data, reg,
                    cpu_data->ram[addr] | (cpu_data->ram[addr + 1] << 8)
                );
            }
            break;
        
        case 0x16: // LD32
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint32_t addr = cpu_data->ram[pc + 2];
                addr |= cpu_data->ram[pc + 3] << 8;
                addr |= cpu_data->ram[pc + 4] << 16;
                addr |= cpu_data->ram[pc + 5] << 24;

                cpu_set_reg(cpu_data, reg,
                    cpu_data->ram[addr] | (cpu_data->ram[addr + 1] << 8) |
                    (cpu_data->ram[addr + 2] << 16) | (cpu_data->ram[addr + 3] << 24)
                );
            }
            break;

        case 0x18: // LDP8
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint8_t addr_reg = cpu_data->ram[pc + 2];

                uint32_t addr = cpu_get_reg(cpu_data, addr_reg);

                cpu_set_reg(cpu_data, reg, cpu_data->ram[addr]);
            }
            break;

        case 0x19: // LDP16
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint8_t addr_reg = cpu_data->ram[pc + 2];

                uint32_t addr = cpu_get_reg(cpu_data, addr_reg);

                cpu_set_reg(cpu_data, reg,
                    cpu_data->ram[addr] | (cpu_data->ram[addr + 1] << 8)
                );
            }
            break;
        
        case 0x1a: // LDP32
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint8_t addr_reg = cpu_data->ram[pc + 2];

                uint32_t addr = cpu_get_reg(cpu_data, addr_reg);

                cpu_set_reg(cpu_data, reg,
                    cpu_data->ram[addr] | (cpu_data->ram[addr + 1] << 8) |
                    (cpu_data->ram[addr + 2] << 16) | (cpu_data->ram[addr + 3] << 24)
                );
            }
            break;
        
        case 0x1c: // ST8
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint32_t addr = cpu_data->ram[pc + 2];
                addr |= cpu_data->ram[pc + 3] << 8;
                addr |= cpu_data->ram[pc + 4] << 16;
                addr |= cpu_data->ram[pc + 5] << 24;

                cpu_data->ram[addr] = cpu_get_reg(cpu_data, reg);
            }
            break;
        
        case 0x1d: // ST16
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint32_t addr = cpu_data->ram[pc + 2];
                addr |= cpu_data->ram[pc + 3] << 8;
                addr |= cpu_data->ram[pc + 4] << 16;
                addr |= cpu_data->ram[pc + 5] << 24;

                uint32_t val = cpu_get_reg(cpu_data, reg);
                cpu_data->ram[addr] = val & 0xFF;
                cpu_data->ram[addr + 1] = (val >> 8) & 0xFF;
            }
            break;

        case 0x1e: // ST32
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint32_t addr = cpu_data->ram[pc + 2];
                addr |= cpu_data->ram[pc + 3] << 8;
                addr |= cpu_data->ram[pc + 4] << 16;
                addr |= cpu_data->ram[pc + 5] << 24;

                uint32_t val = cpu_get_reg(cpu_data, reg);
                cpu_data->ram[addr] = val & 0xFF;
                cpu_data->ram[addr + 1] = (val >> 8) & 0xFF;
                cpu_data->ram[addr + 2] = (val >> 16) & 0xFF;
                cpu_data->ram[addr + 3] = (val >> 24) & 0xFF;
            }
            break;
        
        case 0x20: // STP8
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint8_t addr_reg = cpu_data->ram[pc + 2];

                uint32_t addr = cpu_get_reg(cpu_data, addr_reg);

                cpu_data->ram[addr] = cpu_get_reg(cpu_data, reg);
            }
            break;
        
        case 0x21: // ST16
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint8_t addr_reg = cpu_data->ram[pc + 2];

                uint32_t addr = cpu_get_reg(cpu_data, addr_reg);

                uint32_t val = cpu_get_reg(cpu_data, reg);
                cpu_data->ram[addr] = val & 0xFF;
                cpu_data->ram[addr + 1] = (val >> 8) & 0xFF;
            }
            break;

        case 0x22: // STP32
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint8_t addr_reg = cpu_data->ram[pc + 2];

                uint32_t addr = cpu_get_reg(cpu_data, addr_reg);

                uint32_t val = cpu_get_reg(cpu_data, reg);
                cpu_data->ram[addr] = val & 0xFF;
                cpu_data->ram[addr + 1] = (val >> 8) & 0xFF;
                cpu_data->ram[addr + 2] = (val >> 16) & 0xFF;
                cpu_data->ram[addr + 3] = (val >> 24) & 0xFF;
            }
            break;
        
        case 0x24: // ADD
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) + cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x25: // SUB
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) - cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x26: // MUL
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) * cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x27: // MULS
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    (int)cpu_get_reg(cpu_data, reg_a) * (int)cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x28: // DIV
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) / cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x29: // DIVS
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    (int)cpu_get_reg(cpu_data, reg_a) / (int)cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x2a: // MOD
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) % cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x2b: // MODS
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    (int)cpu_get_reg(cpu_data, reg_a) % (int)cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x2c: // INC
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_data->ram[pc + 1];

                if (reg < NUM_REGS) cpu_data->regs[reg]++;
            }
            break;
        
        case 0x2d: // DEC
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_data->ram[pc + 1];

                if (reg < NUM_REGS) cpu_data->regs[reg]--;
            }
            break;
        
        case 0x30: // AND
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) & cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x31: // NAND
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    ~(cpu_get_reg(cpu_data, reg_a) & cpu_get_reg(cpu_data, reg_b))
                );
            }
            break;
        
        case 0x32: // OR
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) | cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x33: // NOR
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    ~(cpu_get_reg(cpu_data, reg_a) | cpu_get_reg(cpu_data, reg_b))
                );
            }
            break;
        
        case 0x34: // XOR
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) ^ cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x40: // EQ
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) == cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x41: // GT
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) > cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x42: // GTE
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) >= cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x43: // LT
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) < cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x44: // LTE
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_data->ram[pc + 1];
                uint8_t reg_b = cpu_data->ram[pc + 2];
                uint8_t reg_str = cpu_data->ram[pc + 3];

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) <= cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x48: // JMP
            {
                uint32_t value = cpu_data->ram[pc + 1];
                value |= cpu_data->ram[pc + 2] << 8;
                value |= cpu_data->ram[pc + 3] << 16;
                value |= cpu_data->ram[pc + 4] << 24;

                cpu_data->regs[REG_PC] = value;
            }
            break;
        
        case 0x49: // JZ
            {
                cpu_data->regs[REG_PC] += 6;

                uint32_t value = cpu_data->ram[pc + 1];
                value |= cpu_data->ram[pc + 2] << 8;
                value |= cpu_data->ram[pc + 3] << 16;
                value |= cpu_data->ram[pc + 4] << 24;
                uint8_t condition_reg = cpu_data->ram[pc + 5];

                if (cpu_get_reg(cpu_data, condition_reg) == 0) {
                    cpu_data->regs[REG_PC] = value;
                }
            }
            break;
        
        case 0x4a: // JNZ
            {
                cpu_data->regs[REG_PC] += 6;

                uint32_t value = cpu_data->ram[pc + 1];
                value |= cpu_data->ram[pc + 2] << 8;
                value |= cpu_data->ram[pc + 3] << 16;
                value |= cpu_data->ram[pc + 4] << 24;
                uint8_t condition_reg = cpu_data->ram[pc + 5];

                if (cpu_get_reg(cpu_data, condition_reg)) {
                    cpu_data->regs[REG_PC] = value;
                }
            }
            break;
        
        case 0x50: // SND
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t port_reg = cpu_data->ram[pc + 1];
                uint8_t msg_reg = cpu_data->ram[pc + 2];

                snd_to_device(devices_data, cpu_get_reg(cpu_data, port_reg), cpu_get_reg(cpu_data, msg_reg));
            }
            break;
        
        case 0x51: // RCV
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t port_reg = cpu_data->ram[pc + 1];
                uint8_t str_reg = cpu_data->ram[pc + 2];

                cpu_set_reg(cpu_data, str_reg, rcv_from_device(devices_data, cpu_get_reg(cpu_data, port_reg)));
            }
            break;

        default:
            printf("WTF\n");
            printf("Instruction 0x%02x is not supported\n", opcode);
            cpu_data->regs[REG_PC]++;
            return 1;
    }
    return 0;
}

double get_elapsed_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 + 
           (end.tv_nsec - start.tv_nsec) / 1000000.0;
}

int main(int argc, char *argv[]) {
    printf("\x1b]0;YRON2 CPU VM\x07");

    printf("YRON2\n");

    CpuData *cpu_data = malloc(sizeof(CpuData));

    if (cpu_data == 0)
    {
        printf("FAILED TO ALLOCATE CpuData STRUCT\n");
        return 1;
    }
    printf("Allocated CpuData struct\n");
    
    // 1 KiB of RAM, for now
    cpu_data->ram_size = 1 * 1024;
    
    cpu_data->ram = malloc(cpu_data->ram_size);

    if (cpu_data->ram == 0)
    {
        printf("FAILED TO ALLOCATE RAM\n");
        return 1;
    }
    printf("Allocated %d bytes of RAM\n", cpu_data->ram_size);

    DevicesData *devices_data = malloc(sizeof(DevicesData));

    if (devices_data == 0) {
        printf("FAILED TO ALLOCATE DevicesData STRUCT\n");
        return 1;
    }

    {
        int init_devices_data_return = init_devices_data(devices_data);
        if (init_devices_data_return) {
            return init_devices_data_return;
        }
    }

    for (int i = 0; i < NUM_REGS; i++)
    {
        cpu_data->regs[i] = 0;
    }

    // LOAD BINARY FILE

    printf("\n------------------- LOADING ROM FILE -------------------\n");

    {
        char *bin_file_path = "rom.bin";

        if (argc > 1) {
            bin_file_path = argv[1];
        }

        FILE *bin_file = fopen(bin_file_path, "rb");
        if (!bin_file) {
            printf("Failed to open binary file: %s\n", bin_file_path);
            return 1;
        }

        struct stat st;
        if (stat(bin_file_path, &st) != 0) {
            printf("Failed to stat binary file.\n");
            fclose(bin_file);
            return 1;
        }

        printf("Reading file '%s' - size: %ld bytes\n", bin_file_path, st.st_size);

        if (st.st_size > cpu_data->ram_size) {
            printf("ROM size exceeds RAM capacity!\n");
            fclose(bin_file);
            return 1;
        }

        size_t read_bytes = fread(cpu_data->ram, 1, st.st_size, bin_file);
        printf("Loaded %lu bytes into RAM.\n", read_bytes);

        fclose(bin_file);
        printf("Closed file.\n");
    }

    // MAIN LOOP

    puts("Press ENTER to start simulation, to start in DEBUG MODE, press 'd' (must be lower case) and then ENTER: ");

    bool debug_mode = getchar() == 'd';

    printf("\n------------------ STARTING SIMULATION ------------------\n");
    int i = 0;
    
    struct timespec last_ips_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_ips_time);

    uint64_t instruction_count = 0;
    uint32_t current_ips = 0;

    if (debug_mode) system("clear");

    while (true) {
        if (debug_mode) puts("\x1b[H");

        update_devices(devices_data);

        if (!debug_mode) {
            bool hard_exit = false; 
            for (int si = 0; si < SUB_INSTRUCTION_COUNT; si++) {
                if (tick_cpu(cpu_data, devices_data, debug_mode) != 0) { break; hard_exit = true; };
                i++;
                instruction_count++;
            }
            if (hard_exit) break;
        }
        else {
            if (tick_cpu(cpu_data, devices_data, debug_mode) != 0) break;
            i++;
            instruction_count++;
        }

        // Calculate IPS once per second
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        double elapsed_sec = (current_time.tv_sec - last_ips_time.tv_sec) + 
                            (current_time.tv_nsec - last_ips_time.tv_nsec) / 1e9;

        if (elapsed_sec >= 1.0) {
            current_ips = (uint32_t)(instruction_count / elapsed_sec);
            instruction_count = 0;
            last_ips_time = current_time;
            if (!debug_mode) {
                if (current_ips > 10e9) {
                    printf("SPEED: %.2f GI/s\n", current_ips / 1e9);
                }
                else if (current_ips > 10e6) {
                    printf("SPEED: %.2f MI/s\n", current_ips / 1e6);
                }
                else if (current_ips > 10e3) {
                    printf("SPEED: %.2f kI/s\n", current_ips / 1e3);
                }
                else {
                    printf("SPEED: %u I/s\n", current_ips);
                }
            }
        }

        if (debug_mode) {
            printf("----------------------------------------------\n"
                "TICK: %10d\n"
                "IPS:  %10u\n", i, current_ips);
        }

        usleep(10);
    }
}
