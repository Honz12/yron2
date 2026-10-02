#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <ncurses.h>
#include "helpers.h"


#define NUM_REGS 32
#define REG_PC 0
#define REG_SP 1
#define REG_MS 2
#define INT_TABLE_START (uint32_t)1024
#define INT_TABLE_SIZE (uint32_t)256 // 256 * 4 = 1024
#define SUB_INSTRUCTION_COUNT (1024) // RECOMENDED TO ADJUST FOR BETTER/WORSE COMPUTERS!
#define INT_GEN_ERR 0x00
#define INT_INV_RAM_ADDR_ERR 0x01

#define DISPLAY_TERM_WIDTH 80
#define DISPLAY_TERM_HEIGHT 25

const char *menu_options[] = {
    "  Exit  ",
    " TERM 1 ",
    "Reg Dump",
    "RAM Dump",
};
const int mopt_c = sizeof(menu_options) / sizeof(char *);

typedef enum : char {
    DISPLAY_MODE_TERMINAL
} DisplayMode;

typedef struct {
    DisplayMode display_mode;
    int window_selected;

    WINDOW* options_window;
    WINDOW* display_window;
    int option_w_selected;
    int menu_open;

    bool should_exit;

    union {
        struct {
            int caret;
            int ram_dump_offset;
            unsigned char term_chars[DISPLAY_TERM_WIDTH * DISPLAY_TERM_HEIGHT];
        } term_mode;
    } data;
} DisplayData;

void init_display_data(DisplayData *display_data) {
    int terminal_window_w = (DISPLAY_TERM_WIDTH + 2);
    int terminal_window_h = (DISPLAY_TERM_HEIGHT + 2);

    display_data->display_mode = DISPLAY_MODE_TERMINAL;
    display_data->window_selected = 0;

    display_data->options_window = newwin(3, terminal_window_w, 0, 0);
    display_data->option_w_selected = 0;

    display_data->should_exit = false;

    display_data->data.term_mode.caret = 0;
    display_data->data.term_mode.ram_dump_offset = 0;

    for (int i = 0; i < DISPLAY_TERM_WIDTH * DISPLAY_TERM_HEIGHT; i++) {
        display_data->data.term_mode.term_chars[i] = ' ';
    }

    display_data->display_window = newwin(terminal_window_h, terminal_window_w, 3, 0);
    display_data->menu_open = 0;
}

void display_terminal_mode_putc(DisplayData *display_data, char c) {
    if (display_data->display_mode == DISPLAY_MODE_TERMINAL) {
        int caret = display_data->data.term_mode.caret;
        int max_chars = DISPLAY_TERM_WIDTH * DISPLAY_TERM_HEIGHT;

        if (c == '\n') {
            caret = (caret / DISPLAY_TERM_WIDTH + 1) * DISPLAY_TERM_WIDTH;
        }
        else if (c == '\t') {
            caret = (caret / 8 + 1) * 8;
        }
        else if (c == '\a') {
            beep();
        }
        else {
            if (caret < max_chars) {
                display_data->data.term_mode.term_chars[caret] = c;
            }
            caret++;
        }

        while (caret >= max_chars) {
            memmove(
                display_data->data.term_mode.term_chars,
                display_data->data.term_mode.term_chars + DISPLAY_TERM_WIDTH,
                DISPLAY_TERM_WIDTH * (DISPLAY_TERM_HEIGHT - 1)
            );

            memset(
                display_data->data.term_mode.term_chars + DISPLAY_TERM_WIDTH * (DISPLAY_TERM_HEIGHT - 1),
                ' ',
                DISPLAY_TERM_WIDTH
            );

            caret -= DISPLAY_TERM_WIDTH;
        }

        display_data->data.term_mode.caret = caret;
    }
}

typedef struct {
    uint32_t regs[NUM_REGS];
    uint8_t *ram;
    uint32_t ram_size;
    bool breakpoint_triggered;
} CpuData;

typedef struct {
    uint32_t buffered_input;
    uint32_t last_input;
} TerminalIoDeviceData;

typedef struct {
    uint32_t address;
    FILE* disk_file;
} DiskIoDevice;

typedef struct {
    TerminalIoDeviceData *terminal_io_device_data;
    DiskIoDevice* disk_io_device_data;
} DevicesData;


int get_input(DisplayData *display_data) {
    int i = getch();

    if (i == KEY_RESIZE) {
        clear();
        return -1;
    }

    if (i == '\t') {
        clear();
        display_data->window_selected++;
        return -1;
    }

    if (display_data->window_selected == 0) {
        if (i == KEY_LEFT) {
            display_data->option_w_selected--;
        }
        if (i == KEY_RIGHT) {
            display_data->option_w_selected++;
        }
        display_data->option_w_selected += mopt_c;
        display_data->option_w_selected %= mopt_c;

        if (i == '\n') {
            wclear(display_data->display_window);
            switch (display_data->option_w_selected) {
                case 0:
                    display_data->should_exit = true;
                    break;
                case 1:
                    display_data->menu_open = 0;
                    break;
                case 2:
                    display_data->menu_open = 1;
                    break;
                case 3:
                    display_data->menu_open = 2;
                    break;
            }
        }
        
        return -1;
    }

    if (display_data->display_mode == DISPLAY_MODE_TERMINAL) {
        if (display_data->window_selected == 1) {
            return i;
        }
    }

    return -1;
}


int init_devices_data(DevicesData *devices_data) {
    devices_data->terminal_io_device_data = malloc(sizeof(TerminalIoDeviceData));
    devices_data->disk_io_device_data = malloc(sizeof(DiskIoDevice));

    if (devices_data->terminal_io_device_data == 0) {
        return 1;
    }

    if (devices_data->disk_io_device_data == 0) {
        return 1;
    }

    devices_data->terminal_io_device_data->buffered_input = 0;
    devices_data->disk_io_device_data->address = 0;
    devices_data->disk_io_device_data->disk_file = fopen("disk.bin", "r+b");
    if (!devices_data->disk_io_device_data->disk_file) {
        devices_data->disk_io_device_data->disk_file = fopen("disk.bin", "w+b");
        if (!devices_data->disk_io_device_data->disk_file) {
            return 1;
        }
    }
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

void snd_to_device(DevicesData *devices_data, DisplayData *display_data, uint32_t port, uint32_t msg) {
    switch (port)
    {
        case 0: // NULL device
            break;
    
        case 1: // Terminal IO device
            {
                char c = msg;
                display_terminal_mode_putc(display_data, c);
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


void update_devices(DevicesData *devices_data, DisplayData *display_data) {
    // NULL device
    { }

    // Terminal IO device
    {
        uint32_t input = get_input(display_data);

        if (input != ~(uint32_t)0) {
            devices_data->terminal_io_device_data->buffered_input = input;
            devices_data->terminal_io_device_data->last_input = input;
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
    }

    return 0;
}

int tick_cpu(CpuData *cpu_data, DevicesData *devices_data, DisplayData *display_data) {
    uint32_t pc = cpu_data->regs[REG_PC];
    
    uint8_t opcode = cpu_get_ram(cpu_data, pc);

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
        
        case 0x45: // NEQ
            {
                cpu_data->regs[REG_PC] += 4;

                uint8_t reg_a = cpu_get_ram(cpu_data, pc + 1);
                uint8_t reg_b = cpu_get_ram(cpu_data, pc + 2);
                uint8_t reg_str = cpu_get_ram(cpu_data, pc + 3);

                cpu_set_reg(
                    cpu_data, reg_str,
                    cpu_get_reg(cpu_data, reg_a) != cpu_get_reg(cpu_data, reg_b)
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
            }
            break;
        
        case 0x50: // SND
            {
                cpu_data->regs[REG_PC] += 3;

                uint8_t port_reg = cpu_get_ram(cpu_data, pc + 1);
                uint8_t msg_reg = cpu_get_ram(cpu_data, pc + 2);

                snd_to_device(devices_data, display_data, cpu_get_reg(cpu_data, port_reg), cpu_get_reg(cpu_data, msg_reg));
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

        case 0xFF: // Breakpoint
            beep();
            cpu_data->breakpoint_triggered = true;
            break;

        default:
            beep();
            cpu_data->breakpoint_triggered = true;
            break;
    }
    return 0;
}

void tick_display(DisplayData *display_data, CpuData *cpu_data, DevicesData *devices_data, int ips, int fps) {
    int display_width = 0;
    int display_height = 0;
    
    getmaxyx(stdscr, display_height, display_width);
    display_data->window_selected %= 2;

    WINDOW *display_w = display_data->display_window;

    if (display_data->menu_open == 0) {
        for (int y = 0; y < DISPLAY_TERM_HEIGHT; y++) {
            wmove(display_w, 1 + y, 1);
            for (int x = 0; x < DISPLAY_TERM_WIDTH; x++) {
                waddch(display_w, display_data->data.term_mode.term_chars[x + y * DISPLAY_TERM_WIDTH]);
            }
        }
    }

    if (display_data->window_selected == 1) {
        wattron(display_w, COLOR_PAIR(1));
        box(display_w, 0, 0);
        wattroff(display_w, COLOR_PAIR(1));
    }
    else {
        box(display_w, 0, 0);
    }

    wrefresh(display_w);

    WINDOW *menu_bar_w = display_data->options_window;

    if (display_data->window_selected == 0) wattron(menu_bar_w, COLOR_PAIR(1));

    box(menu_bar_w, 0, 0);

    for (int mopt = 0; mopt < mopt_c; mopt++) {
        mvwprintw(menu_bar_w, 1, 2 + mopt * 10, "%s", menu_options[mopt]);
    }

    if (display_data->window_selected == 0) {
        wattron(menu_bar_w, A_REVERSE);
        mvwprintw(menu_bar_w, 1, 2 + display_data->option_w_selected * 10, "%s", menu_options[display_data->option_w_selected]);
        wattroff(menu_bar_w, A_REVERSE);

        wattroff(menu_bar_w, COLOR_PAIR(1));
    }

    //mvprintw(1, DISPLAY_TERM_WIDTH - 5, "%d", display_data->menu_open);

    wrefresh(menu_bar_w);

    refresh();
}

int main(int argc, char *argv[]) {

    CpuData *cpu_data = malloc(sizeof(CpuData));

    if (cpu_data == 0)
    {
        return 1;
    }
    
    // 64 MiB of RAM, for now
    cpu_data->ram_size = 1024 * 1024 * 64;
    
    cpu_data->ram = malloc(cpu_data->ram_size);

    if (cpu_data->ram == 0)
    {
        return 1;
    }

    DevicesData *devices_data = malloc(sizeof(DevicesData));

    if (devices_data == 0) {
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

    cpu_data->breakpoint_triggered = false;

    // LOAD BINARY FILE

    {
        char *bin_file_path = "rom.bin";

        if (argc > 1) {
            bin_file_path = argv[1];
        }

        FILE *bin_file = fopen(bin_file_path, "rb");
        if (!bin_file) {
            return 1;
        }

        uint32_t file_size = get_file_size(bin_file_path);

        if (file_size > cpu_data->ram_size) {
            fclose(bin_file);
            return 1;
        }

        size_t read_bytes = fread(cpu_data->ram, 1, file_size, bin_file);

        fclose(bin_file);
    }

    // MAIN LOOP

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    if (has_colors() == false) {
        endwin();
        return 1;
    }
    start_color();

    init_pair(1, COLOR_YELLOW, COLOR_BLACK);

    struct timespec last_ips_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_ips_time);

    uint64_t instruction_count = 0;
    uint32_t current_ips = 0;

    DisplayData* display_data = malloc(sizeof(DisplayData));

    init_display_data(display_data);

    int frames = 0;
    int fps = 0;

    while (true) {
        struct timespec loop_start, loop_end;
        clock_gettime(CLOCK_MONOTONIC, &loop_start);

        tick_display(display_data, cpu_data, devices_data, current_ips, fps);
        frames++;

        for (int si = 0; si < SUB_INSTRUCTION_COUNT; si++) {
            if (cpu_data->breakpoint_triggered) {
                if (get_input(display_data) != 0xffffffff) {
                    cpu_data->breakpoint_triggered = false;
                    cpu_data->regs[REG_PC]++;
                }
                break;
            }
            tick_cpu(cpu_data, devices_data, display_data);
            instruction_count++;
        }
        
        update_devices(devices_data, display_data);

        // Calculate IPS once per second
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        double elapsed_sec = (current_time.tv_sec - last_ips_time.tv_sec) + 
                            (current_time.tv_nsec - last_ips_time.tv_nsec) / 1e9;

        if (elapsed_sec >= 1.0) {
            current_ips = (uint32_t)(instruction_count / elapsed_sec);
            instruction_count = 0;
            last_ips_time = current_time;
            fps = frames;
            frames = 0;
        }
        
        // Measure elapsed time of this loop iteration
        clock_gettime(CLOCK_MONOTONIC, &loop_end);
        double loop_duration_us = (loop_end.tv_sec - loop_start.tv_sec) * 1e6 + 
                                  (loop_end.tv_nsec - loop_start.tv_nsec) / 1e3;

        // Target frame time in microseconds (1 second / 60)
        double target_frame_us = 1e6 / 60.0;
        double sleep_time_us = target_frame_us - loop_duration_us;

        if (display_data->should_exit) {
            break;
        }

        // Only sleep if we haven't already exceeded our target time
        if (sleep_time_us > 0) {
            process_sleep(sleep_time_us);
        }
    }

    endwin();
}
