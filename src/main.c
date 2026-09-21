#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "helpers.h"


#define NUM_REGS 32
#define REG_PC 0
#define REG_SP 1
#define REG_MS 2
#define INT_TABLE_START (uint32_t)1024
#define INT_TABLE_SIZE (uint32_t)256 // 256 * 4 = 1024
#define SUB_INSTRUCTION_COUNT (1024 * 1024 * 4) // RECOMENDED TO ADJUST FOR BETTER/WORSE COMPUTERS!
#define INT_GEN_ERR 0x00
#define INT_INV_RAM_ADDR_ERR 0x01

typedef struct {
    uint32_t regs[NUM_REGS];
    uint8_t *ram;
    uint32_t ram_size;
    bool breakpoint_triggered;
} CpuData;

typedef struct {
    uint32_t buffered_input;
} TerminalIoDeviceData;

typedef struct {
    uint32_t address;
    FILE* disk_file;
} DiskIoDevice;

typedef struct {
    TerminalIoDeviceData *terminal_io_device_data;
    DiskIoDevice* disk_io_device_data;
} DevicesData;


int init_devices_data(DevicesData *devices_data) {
    devices_data->terminal_io_device_data = malloc(sizeof(TerminalIoDeviceData));
    devices_data->disk_io_device_data = malloc(sizeof(DiskIoDevice));

    if (devices_data->terminal_io_device_data == 0) {
        printf("FAILED TO ALLOCATE TerminalIoDeviceData STRUCT\n");
        return 1;
    }

    if (devices_data->disk_io_device_data == 0) {
        printf("FAILED TO ALLOCATE DiskIoDevice STRUCT\n");
        return 1;
    }

    devices_data->terminal_io_device_data->buffered_input = 0;
    devices_data->disk_io_device_data->address = 0;
    printf("LOADING DISK\n");
    devices_data->disk_io_device_data->disk_file = fopen("disk.bin", "r+b");
    if (!devices_data->disk_io_device_data->disk_file) {
        devices_data->disk_io_device_data->disk_file = fopen("disk.bin", "w+b");
        if (!devices_data->disk_io_device_data->disk_file) {
            printf("FAILED TO OPEN OR CREATE DISK FILE\n");
            return 1;
        }
    }
    printf("DISK LOADED\n");

    return 0;
}

int cpu_make_interrupt(CpuData *cpu_data, uint8_t value);

uint8_t cpu_get_ram_raw(CpuData *data, uint32_t addr, bool ignore_ms) {
    if (!ignore_ms) addr += data->regs[REG_MS];
    if (addr >= data->ram_size) {
        cpu_make_interrupt(data, INT_INV_RAM_ADDR_ERR);
        return 0;
    }
    return data->ram[addr];
}

void cpu_set_ram_raw(CpuData *data, uint32_t addr, uint8_t value, bool ignore_ms) {
    if (!ignore_ms) addr += data->regs[REG_MS];
    if (addr >= data->ram_size) {
        cpu_make_interrupt(data, INT_INV_RAM_ADDR_ERR);
        return;
    }
    data->ram[addr] = value;
}

uint8_t cpu_get_ram(CpuData *data, uint32_t addr) {
    return cpu_get_ram_raw(data, addr, false);
}

void cpu_set_ram(CpuData *data, uint32_t addr, uint8_t value) {
    return cpu_set_ram_raw(data, addr, value, false);
}

bool g_debug_mode;
bool g_clean_mode;
bool g_verbose_mode;

void snd_to_device(DevicesData *devices_data, uint32_t port, uint32_t msg) {
    if (g_debug_mode) printf("\nSENT 0x%08x to 0x%02x\n", msg, port);
    switch (port)
    {
        case 0: // NULL device
            break;
    
        case 1: // Terminal IO device
            {
                char c = msg;
                if (g_verbose_mode) {
                    printf("TERMIO OUT: '%c' (0x%02x)\n", c, c);
                }
                else {
                    putc(c, stdout);
                    fflush(stdout);
                }
            }
            break;
        
        case 2: // Disk IO - Seek
            devices_data->disk_io_device_data->address = msg;
            break;
        
        case 3: // Disk IO - Write / Read
            write_byte(devices_data->disk_io_device_data->disk_file, devices_data->disk_io_device_data->address, msg);
            devices_data->disk_io_device_data->address++;
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
        
        case 2: // Disk IO - Seek
            {
                return devices_data->disk_io_device_data->address;
            }
            break;
        
        case 3: // Disk IO - Write / Read
            {
                uint8_t ret = 0x00;
                read_byte(devices_data->disk_io_device_data->disk_file, devices_data->disk_io_device_data->address, &ret);
                devices_data->disk_io_device_data->address++;
                return ret;
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
    cpu_set_ram_raw(cpu_data, sp, value, true);
    cpu_data->regs[REG_SP] = sp; // Update SP register

    return 0;
}

int cpu_push_stack_uint16(CpuData *cpu_data, uint16_t value) {
    uint32_t sp = cpu_data->regs[REG_SP];

    if (cpu_push_stack_uint8(cpu_data, value)) return 1;
    if (cpu_push_stack_uint8(cpu_data, value >> 8)) return 1;

    return 0;
}

int cpu_push_stack_uint32(CpuData *cpu_data, uint32_t value) {
    uint32_t sp = cpu_data->regs[REG_SP];

    if (cpu_push_stack_uint8(cpu_data, value)) return 1;
    if (cpu_push_stack_uint8(cpu_data, value >> 8)) return 1;
    if (cpu_push_stack_uint8(cpu_data, value >> 16)) return 1;
    if (cpu_push_stack_uint8(cpu_data, value >> 24)) return 1;

    return 0;
}

int cpu_pop_stack_uint8(CpuData *cpu_data, uint8_t *value) {
    uint32_t sp = cpu_data->regs[REG_SP];

    if (sp >= cpu_data->ram_size) {
        return 1;
    }

    *value = cpu_get_ram_raw(cpu_data, sp, true);
    sp++;
    cpu_data->regs[REG_SP] = sp;

    return 0;
}

int cpu_pop_stack_uint16(CpuData *cpu_data, uint16_t *value) {
    uint8_t low = 0;
    uint8_t high = 0;

    if (cpu_pop_stack_uint8(cpu_data, &high)) return 1;
    if (cpu_pop_stack_uint8(cpu_data, &low)) return 1;

    *value = (uint16_t)(((uint16_t)high << 8) | low);
    return 0;
}

int cpu_pop_stack_uint32(CpuData *cpu_data, uint32_t *value) {
    uint8_t b0 = 0;
    uint8_t b1 = 0;
    uint8_t b2 = 0;
    uint8_t b3 = 0;

    if (cpu_pop_stack_uint8(cpu_data, &b3)) return 1;
    if (cpu_pop_stack_uint8(cpu_data, &b2)) return 1;
    if (cpu_pop_stack_uint8(cpu_data, &b1)) return 1;
    if (cpu_pop_stack_uint8(cpu_data, &b0)) return 1;

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

    return 0;
}

void cpu_set_reg(CpuData *cpu_data, uint8_t reg, uint32_t value) {
    if (reg < NUM_REGS) {
        cpu_data->regs[(int)reg] = value;
    }
}

int cpu_make_interrupt(CpuData *cpu_data, uint8_t value) {
    uint32_t return_pc = cpu_data->regs[REG_PC];
    uint32_t return_ms = cpu_data->regs[REG_MS];
    cpu_push_stack_uint32(cpu_data, return_pc);
    cpu_push_stack_uint32(cpu_data, return_ms);

    cpu_data->regs[REG_MS] = 0;

    if (value < INT_TABLE_SIZE) {
        uint32_t int_field_start = INT_TABLE_START + value * 4;
        uint32_t int_address = cpu_get_ram(cpu_data, int_field_start) |
        (cpu_get_ram(cpu_data, int_field_start + 1) << 8) |
        (cpu_get_ram(cpu_data, int_field_start + 2) << 16) | ((uint32_t)cpu_get_ram(cpu_data, int_field_start + 3) << 24);

        cpu_data->regs[REG_PC] = int_address;
        printf("\n\x1b[90mINT 0x%02x called, IFS: 0x%08x, return pc: 0x%08x, jumping to 0x%08x\x1b[0m\n", value, int_field_start, return_pc, int_address);
    }
    else {
        printf("INVALID INTERRUPT 0x%02x\n", value);
    }

    return 0;
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
        else if (i == REG_MS) {
            printf("MS   : 0x%08x | %12d\n", cpu_data->regs[i], cpu_data->regs[i]);
        }
        else {
            printf("0x%02x : 0x%08x | %12d\n", i, cpu_data->regs[i], cpu_data->regs[i]);
        }
    }
}

void cpu_dump_ram(CpuData* cpu_data) {
    const uint8_t lines = 24;
    const uint8_t bytes_per_line = 16;

    uint32_t bytes_to_display = lines * bytes_per_line;
    uint32_t raw_pc = cpu_data->regs[REG_PC] + cpu_data->regs[REG_MS];
    uint32_t raw_ms = cpu_data->regs[REG_MS];
    uint32_t raw_sp = cpu_data->regs[REG_SP];
    uint8_t line = 0;

    for (uint32_t i = 0; i < bytes_to_display; i += bytes_per_line)
    {
        uint32_t line_origin = (raw_pc / bytes_per_line - lines / 2) * bytes_per_line + i;

        printf("0x%08x: \x1b[1m", line_origin);

        for (uint8_t o = 0; o < bytes_per_line; o++)
        {
            uint32_t byte_addr = line_origin + o;
            uint8_t byte = 0x00;

            if (byte_addr < cpu_data->ram_size) {
                byte = cpu_data->ram[byte_addr];
            }

            if (byte_addr == raw_pc) printf("\x1b[103;30m%02x\x1b[0;1m ", byte);
            else if (byte_addr == raw_ms) printf("\x1b[105;30m%02x\x1b[0;1m ", byte);
            else if (byte_addr == raw_sp) printf("\x1b[104;30m%02x\x1b[0;1m ", byte);
            else printf("%02x ", byte);
        }
        printf("\x1b[0m\n");
        line++;
    }
    
}

int tick_cpu(CpuData *cpu_data, DevicesData *devices_data) {
    uint32_t pc = cpu_data->regs[REG_PC];
    
    uint8_t opcode = cpu_get_ram(cpu_data, pc);

    if (g_verbose_mode) {
        printf("0x%08x: 0x%02x\n", pc, opcode);
    }

    switch (opcode) {
        case 0x00: // NOP
            cpu_data->regs[REG_PC]++;
            break;

        case 0x01: // CALL
            {
                cpu_data->regs[REG_PC] += 5;
                
                uint32_t value = cpu_get_ram(cpu_data, pc + 1);
                value |= cpu_get_ram(cpu_data, pc + 2) << 8;
                value |= cpu_get_ram(cpu_data, pc + 3) << 16;
                value |= (uint32_t)cpu_get_ram(cpu_data, pc + 4) << 24;

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

                uint8_t value = cpu_get_ram(cpu_data, pc + 1);

                return cpu_make_interrupt(cpu_data, value);
            }
            break;
        
        case 0x04: // IRET
            {
                uint32_t dest_pc;
                uint32_t dest_ms;

                cpu_pop_stack_uint32(cpu_data, &dest_ms);
                cpu_pop_stack_uint32(cpu_data, &dest_pc);

                cpu_data->regs[REG_PC] = dest_pc;
                cpu_data->regs[REG_MS] = dest_ms;
            }
            break;
        
        case 0x05: // MOV
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t from = cpu_get_ram(cpu_data, pc + 1);
                uint8_t to = cpu_get_ram(cpu_data, pc + 2);

                cpu_set_reg(cpu_data, to, cpu_get_reg(cpu_data, from));
            }
            break;
        
        // free space

        case 0x08: // PUSH8
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                cpu_push_stack_uint8(cpu_data, cpu_get_reg(cpu_data, reg));
            }
            break;

        case 0x09: // PUSH16
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                cpu_push_stack_uint16(cpu_data, cpu_get_reg(cpu_data, reg));
            }
            break;

        case 0x0a: // PUSH32
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                cpu_push_stack_uint32(cpu_data, cpu_get_reg(cpu_data, reg));
            }
            break;

        case 0x0b: // POP8
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint8_t val;
                cpu_pop_stack_uint8(cpu_data, &val);

                cpu_set_reg(cpu_data, reg, val);
            }
            break;

        case 0x0c: // POP16
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint16_t val;
                cpu_pop_stack_uint16(cpu_data, &val);

                cpu_set_reg(cpu_data, reg, val);
            }
            break;

        case 0x0d: // POP32
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint32_t val;
                cpu_pop_stack_uint32(cpu_data, &val);

                cpu_set_reg(cpu_data, reg, val);
            }
            break;
        
        case 0x10: // LDI8
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint8_t value = cpu_get_ram(cpu_data, pc + 2);

                cpu_set_reg(cpu_data, reg, value);
            }
            break;
        
        case 0x11: // LDI16
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint16_t value = cpu_get_ram(cpu_data, pc + 2);
                value |= cpu_get_ram(cpu_data, pc + 3) << 8;

                cpu_set_reg(cpu_data, reg, value);
            }
            break;
        
        case 0x12: // LDI32
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint32_t value = cpu_get_ram(cpu_data, pc + 2);
                value |= cpu_get_ram(cpu_data, pc + 3) << 8;
                value |= cpu_get_ram(cpu_data, pc + 4) << 16;
                value |= (uint32_t)cpu_get_ram(cpu_data, pc + 5) << 24;

                cpu_set_reg(cpu_data, reg, value);
            }
            break;
        
        case 0x14: // LD8
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint32_t addr = cpu_get_ram(cpu_data, pc + 2);
                addr |= cpu_get_ram(cpu_data, pc + 3) << 8;
                addr |= cpu_get_ram(cpu_data, pc + 4) << 16;
                addr |= (uint32_t)cpu_get_ram(cpu_data, pc + 5) << 24;

                cpu_set_reg(cpu_data, reg, cpu_get_ram(cpu_data, addr));
            }
            break;
        
        case 0x15: // LD16
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint32_t addr = cpu_get_ram(cpu_data, pc + 2);
                addr |= cpu_get_ram(cpu_data, pc + 3) << 8;
                addr |= cpu_get_ram(cpu_data, pc + 4) << 16;
                addr |= (uint32_t)cpu_get_ram(cpu_data, pc + 5) << 24;

                cpu_set_reg(cpu_data, reg,
                    cpu_get_ram(cpu_data, addr) | (cpu_get_ram(cpu_data, addr + 1) << 8)
                );
            }
            break;
        
        case 0x16: // LD32
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint32_t addr = cpu_get_ram(cpu_data, pc + 2);
                addr |= cpu_get_ram(cpu_data, pc + 3) << 8;
                addr |= cpu_get_ram(cpu_data, pc + 4) << 16;
                addr |= (uint32_t)cpu_get_ram(cpu_data, pc + 5) << 24;

                cpu_set_reg(cpu_data, reg,
                    cpu_get_ram(cpu_data, addr) | (cpu_get_ram(cpu_data, addr + 1) << 8) |
                    (cpu_get_ram(cpu_data, addr + 2) << 16) | ((uint32_t)cpu_get_ram(cpu_data, addr + 3) << 24)
                );
            }
            break;

        case 0x18: // LDP8
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint8_t addr_reg = cpu_get_ram(cpu_data, pc + 2);

                uint32_t addr = cpu_get_reg(cpu_data, addr_reg);

                cpu_set_reg(cpu_data, reg, cpu_get_ram(cpu_data, addr));
            }
            break;

        case 0x19: // LDP16
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint8_t addr_reg = cpu_get_ram(cpu_data, pc + 2);

                uint32_t addr = cpu_get_reg(cpu_data, addr_reg);

                cpu_set_reg(cpu_data, reg,
                    cpu_get_ram(cpu_data, addr) | (cpu_get_ram(cpu_data, addr + 1) << 8)
                );
            }
            break;
        
        case 0x1a: // LDP32
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint8_t addr_reg = cpu_get_ram(cpu_data, pc + 2);

                uint32_t addr = cpu_get_reg(cpu_data, addr_reg);

                cpu_set_reg(cpu_data, reg,
                    cpu_get_ram(cpu_data, addr) | (cpu_get_ram(cpu_data, addr + 1) << 8) |
                    (cpu_get_ram(cpu_data, addr + 2) << 16) | ((uint32_t)cpu_get_ram(cpu_data, addr + 3) << 24)
                );
            }
            break;
        
        case 0x1c: // ST8
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint32_t addr = cpu_get_ram(cpu_data, pc + 2);
                addr |= cpu_get_ram(cpu_data, pc + 3) << 8;
                addr |= cpu_get_ram(cpu_data, pc + 4) << 16;
                addr |= (uint32_t)cpu_get_ram(cpu_data, pc + 5) << 24;

                cpu_set_ram(cpu_data, addr, cpu_get_reg(cpu_data, reg));
            }
            break;
        
        case 0x1d: // ST16
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint32_t addr = cpu_get_ram(cpu_data, pc + 2);
                addr |= cpu_get_ram(cpu_data, pc + 3) << 8;
                addr |= cpu_get_ram(cpu_data, pc + 4) << 16;
                addr |= (uint32_t)cpu_get_ram(cpu_data, pc + 5) << 24;

                uint32_t val = cpu_get_reg(cpu_data, reg);
                cpu_set_ram(cpu_data, addr, val & 0xFF);
                cpu_set_ram(cpu_data, addr + 1, (val >> 8) & 0xFF);
            }
            break;

        case 0x1e: // ST32
            {
                cpu_data->regs[REG_PC] += 6;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint32_t addr = cpu_get_ram(cpu_data, pc + 2);
                addr |= cpu_get_ram(cpu_data, pc + 3) << 8;
                addr |= cpu_get_ram(cpu_data, pc + 4) << 16;
                addr |= (uint32_t)cpu_get_ram(cpu_data, pc + 5) << 24;

                uint32_t val = cpu_get_reg(cpu_data, reg);
                cpu_set_ram(cpu_data, addr, val & 0xFF);
                cpu_set_ram(cpu_data, addr + 1, (val >> 8) & 0xFF);
                cpu_set_ram(cpu_data, addr + 2, (val >> 16) & 0xFF);
                cpu_set_ram(cpu_data, addr + 3, (val >> 24) & 0xFF);
            }
            break;
        
        case 0x20: // STP8
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint8_t addr_reg = cpu_get_ram(cpu_data, pc + 2);

                uint32_t addr = cpu_get_reg(cpu_data, addr_reg);

                cpu_set_ram(cpu_data, addr, cpu_get_reg(cpu_data, reg));
            }
            break;
        
        case 0x21: // STP16
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint8_t addr_reg = cpu_get_ram(cpu_data, pc + 2);

                uint32_t addr = cpu_get_reg(cpu_data, addr_reg);

                uint32_t val = cpu_get_reg(cpu_data, reg);
                cpu_set_ram(cpu_data, addr, val & 0xFF);
                cpu_set_ram(cpu_data, addr + 1, (val >> 8) & 0xFF);
            }
            break;

        case 0x22: // STP32
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);
                uint8_t addr_reg = cpu_get_ram(cpu_data, pc + 2);

                uint32_t addr = cpu_get_reg(cpu_data, addr_reg);

                uint32_t val = cpu_get_reg(cpu_data, reg);
                cpu_set_ram(cpu_data, addr, val & 0xFF);
                cpu_set_ram(cpu_data, addr + 1, (val >> 8) & 0xFF);
                cpu_set_ram(cpu_data, addr + 2, (val >> 16) & 0xFF);
                cpu_set_ram(cpu_data, addr + 3, (val >> 24) & 0xFF);
            }
            break;
        
        case 0x24: // ADD
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) + cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x25: // SUB
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) - cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x26: // MUL
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) * cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x27: // MULS
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    (int)cpu_get_reg(cpu_data, reg_a) * (int)cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x28: // DIV
            {
                cpu_data->regs[REG_PC] += 4;
                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                uint32_t val_b = cpu_get_reg(cpu_data, reg_b);
                if (val_b == 0) {
                    cpu_set_reg(cpu_data, reg_str, 0);
                    break;
                }
                cpu_set_reg(cpu_data, reg_str, cpu_get_reg(cpu_data, reg_a) / val_b);
            }
            break;
        
        case 0x29: // DIVS
            {
                cpu_data->regs[REG_PC] += 4;
                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                int val_a = (int)cpu_get_reg(cpu_data, reg_a);
                int val_b = (int)cpu_get_reg(cpu_data, reg_b);
                if (val_b == 0 || (val_a == INT32_MIN && val_b == -1)) {
                    cpu_set_reg(cpu_data, reg_str, 0);
                    break;
                }
                cpu_set_reg(cpu_data, reg_str, val_a / val_b);
            }
            break;
        
        case 0x2a: // MOD
            {
                cpu_data->regs[REG_PC] += 4;
                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                uint32_t val_b = cpu_get_reg(cpu_data, reg_b);
                if (val_b == 0) {
                    cpu_set_reg(cpu_data, reg_str, 0);
                    break;
                }
                cpu_set_reg(cpu_data, reg_str, cpu_get_reg(cpu_data, reg_a) % val_b);
            }
            break;
        
        case 0x2b: // MODS
            {
                cpu_data->regs[REG_PC] += 4;
                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                int val_a = (int)cpu_get_reg(cpu_data, reg_a);
                int val_b = (int)cpu_get_reg(cpu_data, reg_b);
                if (val_b == 0 || (val_a == INT32_MIN && val_b == -1)) {
                    cpu_set_reg(cpu_data, reg_str, 0);
                    break;
                }
                cpu_set_reg(cpu_data, reg_str, val_a % val_b);
            }
            break;
        
        case 0x2c: // INC
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);

                if (reg < NUM_REGS) cpu_data->regs[reg]++;
            }
            break;
        
        case 0x2d: // DEC
            {
                cpu_data->regs[REG_PC] += 2;

                uint8_t reg = cpu_get_ram(cpu_data, pc + 1);

                if (reg < NUM_REGS) cpu_data->regs[reg]--;
            }
            break;
        
        case 0x30: // AND
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) & cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x31: // NAND
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    ~(cpu_get_reg(cpu_data, reg_a) & cpu_get_reg(cpu_data, reg_b))
                );
            }
            break;
        
        case 0x32: // OR
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) | cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x33: // NOR
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    ~(cpu_get_reg(cpu_data, reg_a) | cpu_get_reg(cpu_data, reg_b))
                );
            }
            break;
        
        case 0x34: // XOR
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) ^ cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x40: // EQ
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) == cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x41: // GT
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) > cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x42: // GTE
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) >= cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x43: // LT
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) < cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x44: // LTE
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) <= cpu_get_reg(cpu_data, reg_b)
                );
            }
            break;
        
        case 0x48: // JMP
            {
                uint32_t value = cpu_get_ram(cpu_data, pc + 1);
                value |= cpu_get_ram(cpu_data, pc + 2) << 8;
                value |= cpu_get_ram(cpu_data, pc + 3) << 16;
                value |= (uint32_t)cpu_get_ram(cpu_data, pc + 4) << 24;

                cpu_data->regs[REG_PC] = value;
            }
            break;
        
        case 0x49: // JZ
            {
                cpu_data->regs[REG_PC] += 6;

                uint32_t value = cpu_get_ram(cpu_data, pc + 1);
                value |= cpu_get_ram(cpu_data, pc + 2) << 8;
                value |= cpu_get_ram(cpu_data, pc + 3) << 16;
                value |= (uint32_t)cpu_get_ram(cpu_data, pc + 4) << 24;
                uint8_t condition_reg = cpu_get_ram(cpu_data, pc + 5);

                if (cpu_get_reg(cpu_data, condition_reg) == 0) {
                    cpu_data->regs[REG_PC] = value;
                }
            }
            break;
        
        case 0x4a: // JNZ
            {
                cpu_data->regs[REG_PC] += 6;

                uint32_t value = cpu_get_ram(cpu_data, pc + 1);
                value |= cpu_get_ram(cpu_data, pc + 2) << 8;
                value |= cpu_get_ram(cpu_data, pc + 3) << 16;
                value |= (uint32_t)cpu_get_ram(cpu_data, pc + 4) << 24;
                uint8_t condition_reg = cpu_get_ram(cpu_data, pc + 5);

                if (cpu_get_reg(cpu_data, condition_reg)) {
                    cpu_data->regs[REG_PC] = value;
                }
            }
            break;
        
        case 0x4b: // LJMP
            {
                uint32_t memory_segment = cpu_get_reg(cpu_data, cpu_get_ram(cpu_data, pc + 1));
                uint32_t program_counter = cpu_get_reg(cpu_data, cpu_get_ram(cpu_data, pc + 2));

                cpu_data->regs[REG_MS] = memory_segment;
                cpu_data->regs[REG_PC] = program_counter;
                
                cpu_data->breakpoint_triggered = true;
            }
            break;
        
        case 0x50: // SND
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t port_reg = cpu_get_ram(cpu_data, pc + 1);
                uint8_t msg_reg = cpu_get_ram(cpu_data, pc + 2);

                snd_to_device(devices_data, cpu_get_reg(cpu_data, port_reg), cpu_get_reg(cpu_data, msg_reg));
            }
            break;
        
        case 0x51: // RCV
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t port_reg = cpu_get_ram(cpu_data, pc + 1);
                uint8_t str_reg = cpu_get_ram(cpu_data, pc + 2);

                cpu_set_reg(cpu_data, str_reg, rcv_from_device(devices_data, cpu_get_reg(cpu_data, port_reg)));
            }
            break;
        
        /*
        case 0xFF: // Breakpoint
            cpu_data->regs[REG_PC]++;
            cpu_data->breakpoint_triggered = true;
            break;
        */

        default:
            printf("\nInstruction 0x%02x at 0x%08x (PC:0x%08x MS:0x%08x) is not supported\n", opcode, cpu_data->regs[REG_PC] + cpu_data->regs[REG_MS], cpu_data->regs[REG_PC], cpu_data->regs[REG_MS]);

            cpu_data->breakpoint_triggered = true;
            cpu_data->regs[REG_PC]++;
            break;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    char *instruction_names[256] = {
        [0x00] = "NOP",
        [0x01] = "CALL",
        [0x02] = "RET",
        [0x03] = "INT",
        [0x04] = "IRET",
        [0x05] = "MOV",

        [0x08] = "PUSH8",
        [0x09] = "PUSH16",
        [0x0A] = "PUSH32",
        [0x0B] = "POP8",
        [0x0C] = "POP16",
        [0x0D] = "POP32",

        [0x10] = "LDI8",
        [0x11] = "LDI16",
        [0x12] = "LDI32",

        [0x14] = "LD8",
        [0x15] = "LD16",
        [0x16] = "LD32",

        [0x18] = "LDP8",
        [0x19] = "LDP16",
        [0x1A] = "LDP32",

        [0x1C] = "ST8",
        [0x1D] = "ST16",
        [0x1E] = "ST32",

        [0x20] = "STP8",
        [0x21] = "STP16",
        [0x22] = "STP32",

        [0x24] = "ADD",
        [0x25] = "SUB",
        [0x26] = "MUL",
        [0x27] = "MULS",
        [0x28] = "DIV",
        [0x29] = "DIVS",
        [0x2A] = "MOD",
        [0x2B] = "MODS",
        [0x2C] = "INC",
        [0x2D] = "DEC",

        [0x30] = "AND",
        [0x31] = "NAND",
        [0x32] = "OR",
        [0x33] = "NOR",
        [0x34] = "XOR",

        [0x40] = "EQ",
        [0x41] = "GT",
        [0x42] = "GTE",
        [0x43] = "LT",
        [0x44] = "LTE",

        [0x48] = "JMP",
        [0x49] = "JZ",
        [0x4A] = "JNZ",
        [0x4B] = "LJMP",

        [0x50] = "SND",
        [0x51] = "RCV"
    };

    printf("\x1b]0;YRON2 CPU VM\x07");

    printf("YRON2\n");

    CpuData *cpu_data = malloc(sizeof(CpuData));

    if (cpu_data == 0)
    {
        printf("FAILED TO ALLOCATE CpuData STRUCT\n");
        return 1;
    }
    printf("Allocated CpuData struct\n");
    
    // 64 MiB of RAM, for now
    cpu_data->ram_size = 1024 * 1024 * 64;
    
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
        printf("INITIALIZING DEVICES\n");
        int init_devices_data_return = init_devices_data(devices_data);
        if (init_devices_data_return) {
            return init_devices_data_return;
        }
        printf("DEVICES INITIALIZED\n");
    }

    for (int i = 0; i < NUM_REGS; i++)
    {
        cpu_data->regs[i] = 0;
    }

    cpu_data->breakpoint_triggered = false;

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

        uint32_t file_size = get_file_size(bin_file_path);

        printf("Reading file '%s' - size: %u bytes\n", bin_file_path, file_size);

        if (file_size > cpu_data->ram_size) {
            printf("ROM size exceeds RAM capacity!\n");
            fclose(bin_file);
            return 1;
        }

        size_t read_bytes = fread(cpu_data->ram, 1, file_size, bin_file);
        printf("Loaded %lu bytes into RAM.\n", read_bytes);

        fclose(bin_file);
        printf("Closed file.\n");
    }

    // MAIN LOOP

    setup_terminal();

    puts(
        "\nPress ENTER to start simulation\n"
        "To start in DEBUG MODE, press 'd'\n"
        "To start in CLEAN MODE, press 'c'\n"
        "To start in VERBOSE MODE press 'v'"
    );
    fflush(stdout);

    char mode_ran = getchar();

    g_debug_mode = mode_ran == 'd';
    g_clean_mode = mode_ran == 'c' || mode_ran == 'v';
    g_verbose_mode = mode_ran == 'v';

    printf("\n------------------ STARTING SIMULATION ------------------\n");
    int i = 0;
    
    struct timespec last_ips_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_ips_time);

    uint64_t instruction_count = 0;
    uint32_t current_ips = 0;

    if (g_debug_mode) system("clear");

    while (true) {
        if (g_debug_mode) {
            cpu_dump_registers(cpu_data);
            cpu_dump_ram(cpu_data);
            if (cpu_data->regs[REG_PC] < cpu_data->ram_size) {
                uint8_t opcode = cpu_get_ram(cpu_data, cpu_data->regs[REG_PC]);
                char* alias = instruction_names[opcode];
                if (alias == NULL) {
                    printf("\nINST (0x%08x): 0x%02x\n", cpu_data->regs[REG_PC], opcode);
                }
                else {
                    printf("\nINST (0x%08x): %s (0x%02x)\n", cpu_data->regs[REG_PC], alias, opcode);
                }
            }
            else {
                printf("\nPC OVERFLOW\n");
            }
            printf(
                "TICK: %10d\n"
                "IPS:  %10u\n"
                "\nPress - Q to quit, B to continue to next breakpoint, SPACE to step ...\n", i, current_ips);

            char c = getchar();

            if (c == 'q') {
                break;
            }
            else if (c == 'b') {
                bool hard_exit = false; 
                while (!cpu_data->breakpoint_triggered) {
                    if (tick_cpu(cpu_data, devices_data) != 0) { hard_exit = true; break; };
                    i++;
                    instruction_count++;
                };
                if (hard_exit) break;
                cpu_data->breakpoint_triggered = false;
                continue;
            }
            else if (c == ' ') {}
            else {
                continue;
            }
        };

        update_devices(devices_data);

        if (!g_debug_mode) {
            bool hard_exit = false; 
            for (int si = 0; si < SUB_INSTRUCTION_COUNT; si++) {
                if (tick_cpu(cpu_data, devices_data) != 0) { hard_exit = true; break; };
                i++;
                instruction_count++;
                if (cpu_data->breakpoint_triggered) {
                    cpu_dump_registers(cpu_data);
                    cpu_dump_ram(cpu_data);
                    printf("\x1b[90mBREAKPOINT\x1b[0m\n");
                    getchar();
                    cpu_data->breakpoint_triggered = false;

                    break;
                }
            }
            if (hard_exit) break;
        }
        else {
            if (tick_cpu(cpu_data, devices_data) != 0) break;
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
            if (!g_debug_mode && !g_clean_mode) {
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

        process_sleep(10);
    }
}
