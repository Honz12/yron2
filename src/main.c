#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


#define NUM_REGS 32
#define REG_PC 0
#define REG_SP 1

typedef struct {
    uint32_t regs[NUM_REGS];
    uint8_t* ram;
    uint32_t ram_size;
} CpuData;


int cpu_push_stack_uint8(CpuData* cpu_data, uint8_t value) {
    uint32_t sp = cpu_data->regs[REG_SP];

    sp--; // Make it so stack won't overrite start position

    if (sp < 0) {
        return 1;
    }

    cpu_data->ram[sp] = value;
}

int cpu_push_stack_uint16(CpuData* cpu_data, uint16_t value) {
    uint32_t sp = cpu_data->regs[REG_SP];

    if (cpu_push_stack_uint8(cpu_data, value)) return 1;
    if (cpu_push_stack_uint8(cpu_data, value >> 8)) return 1;
}

int cpu_push_stack_uint32(CpuData* cpu_data, uint32_t value) {
    uint32_t sp = cpu_data->regs[REG_SP];

    if (cpu_push_stack_uint8(cpu_data, value)) return 1;
    if (cpu_push_stack_uint8(cpu_data, value >> 8)) return 1;
    if (cpu_push_stack_uint8(cpu_data, value >> 16)) return 1;
    if (cpu_push_stack_uint8(cpu_data, value >> 24)) return 1;
}

int cpu_pop_stack_uint8(CpuData* cpu_data, uint8_t* value) {
    uint32_t sp = cpu_data->regs[REG_SP];

    if (sp >= cpu_data->ram_size) {
        return 1;
    }

    *value = cpu_data->ram[sp];
    sp++;
    cpu_data->regs[REG_SP] = sp;

    return 0;
}

int cpu_pop_stack_uint16(CpuData* cpu_data, uint16_t* value) {
    uint8_t low = 0;
    uint8_t high = 0;

    if (cpu_pop_stack_uint8(cpu_data, &low)) return 1;
    if (cpu_pop_stack_uint8(cpu_data, &high)) return 1;

    *value = (uint16_t)(((uint16_t)high << 8) | low);
    return 0;
}

int cpu_pop_stack_uint32(CpuData* cpu_data, uint32_t* value) {
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

uint32_t cpu_get_reg(CpuData* cpu_data, uint8_t reg) {
    if (reg < NUM_REGS) {
        return cpu_data->regs[(int)reg];
    }
}

void cpu_set_reg(CpuData* cpu_data, uint8_t reg, uint32_t value) {
    if (reg < NUM_REGS) {
        cpu_data->regs[(int)reg] = value;
    }
}

int tick_cpu(CpuData* cpu_data, bool debug) {
    if (cpu_data->regs[REG_PC] > cpu_data->ram_size) {
        cpu_data->regs[REG_PC] = 0;
    }

    uint32_t pc = cpu_data->regs[REG_PC];
    
    uint8_t opcode = cpu_data->ram[pc];

    if (debug) printf("0x%08x: 0x%02x\n", pc, opcode);

    switch (opcode) {
        case 0x00: // NOP
            cpu_data->regs[REG_PC]++;
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
                uint8_t value = cpu_data->ram[pc + 2];
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
                uint8_t addr = cpu_data->ram[pc + 2];
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
                uint8_t addr = cpu_data->ram[pc + 2];
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
                uint8_t addr = cpu_data->ram[pc + 2];
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
                uint8_t addr = cpu_data->ram[pc + 2];
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
                uint8_t addr = cpu_data->ram[pc + 2];
                addr |= cpu_data->ram[pc + 3] << 8;
                addr |= cpu_data->ram[pc + 4] << 16;
                addr |= cpu_data->ram[pc + 5] << 24;

                cpu_data->ram[addr] = cpu_get_reg(cpu_data, reg);
                cpu_data->ram[addr] = cpu_get_reg(cpu_data, reg) >> 8;
            }
            break;

        case 0x1e: // ST32
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint8_t addr = cpu_data->ram[pc + 2];
                addr |= cpu_data->ram[pc + 3] << 8;
                addr |= cpu_data->ram[pc + 4] << 16;
                addr |= cpu_data->ram[pc + 5] << 24;

                cpu_data->ram[addr] = cpu_get_reg(cpu_data, reg);
                cpu_data->ram[addr] = cpu_get_reg(cpu_data, reg) >> 8;
                cpu_data->ram[addr] = cpu_get_reg(cpu_data, reg) >> 16;
                cpu_data->ram[addr] = cpu_get_reg(cpu_data, reg) >> 24;
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
        
        case 0x21: // STP16
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint8_t addr_reg = cpu_data->ram[pc + 2];

                uint32_t addr = cpu_get_reg(cpu_data, addr_reg);

                cpu_data->ram[addr] = cpu_get_reg(cpu_data, reg);
                cpu_data->ram[addr] = cpu_get_reg(cpu_data, reg) >> 8;
            }
            break;

        case 0x22: // STP32
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_data->ram[pc + 1];
                uint8_t addr_reg = cpu_data->ram[pc + 2];

                uint32_t addr = cpu_get_reg(cpu_data, addr_reg);

                cpu_data->ram[addr] = cpu_get_reg(cpu_data, reg);
                cpu_data->ram[addr] = cpu_get_reg(cpu_data, reg) >> 8;
                cpu_data->ram[addr] = cpu_get_reg(cpu_data, reg) >> 16;
                cpu_data->ram[addr] = cpu_get_reg(cpu_data, reg) >> 24;
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

        default:
            printf("Instruction 0x%02x is not supported\n", opcode);
            cpu_data->regs[REG_PC]++;
            return 1;
    }
    return 0;
}


int main() {
    printf("YRON2\n");

    CpuData* cpu_data = malloc(sizeof(CpuData));

    if (cpu_data == 0)
    {
        printf("FAILED TO ALLOCATE CpuData STRUCT\n");
        return 1;
    }
    printf("Allocated CpuData struct\n");
    
    // 16 KiB of RAM, for now
    cpu_data->ram_size = 16 * 1024;
    
    cpu_data->ram = malloc(cpu_data->ram_size);

    if (cpu_data->ram == 0)
    {
        printf("FAILED TO ALLOCATE RAM\n");
        return 1;
    }
    printf("Allocated %d bytes of RAM\n", cpu_data->ram_size);

    for (int i = 0; i < NUM_REGS; i++)
    {
        cpu_data->regs[i] = 0;
    }

    while (true) {
        if ("%d\n", tick_cpu(cpu_data, true)) break;
    }

    free(cpu_data->ram);
    printf("Freed RAM\n");
    free(cpu_data);
    printf("Freed CpuData struct\n");
}
