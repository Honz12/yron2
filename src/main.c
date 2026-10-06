#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <ncurses.h>
#include "helpers.h"
#include "cjson/cJSON.h"

#undef TRUE
#undef FALSE


bool g_plain_mode = false;


#define NUM_REGS 32
#define REG_PC 0
#define REG_SP 1
#define REG_MS 2
#define INT_TABLE_START (uint32_t)1024
#define INT_TABLE_SIZE (uint32_t)256 // 256 * 4 = 1024
#define INT_GEN_ERR 0x00
#define INT_INV_RAM_ADDR_ERR 0x01

#define DISPLAY_TERM_WIDTH 80
#define DISPLAY_TERM_HEIGHT 25

#define DISPLAY_WIDTH DISPLAY_TERM_WIDTH
#define DISPLAY_HEIGHT DISPLAY_TERM_HEIGHT

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
    [0x45] = "NEQ",

    [0x48] = "JMP",
    [0x49] = "JZ",
    [0x4A] = "JNZ",
    [0x4B] = "LJMP",

    [0x50] = "SND",
    [0x51] = "RCV"
};

const char *menu_options[] = {
    "  Exit  ",
    " TERM 1 ",
    "Reg Dump",
    "RAM Dump",
};
const int mopt_c = sizeof(menu_options) / sizeof(char *);

const uint32_t ram_dump_bytes_per_line = 16;
const uint32_t ram_dump_lines = DISPLAY_HEIGHT - 2;

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

    struct {
        uint32_t ram_view_scroll;
    } inbuild_menus_info;

    union {
        struct {
            int caret;
            unsigned char term_chars[DISPLAY_TERM_WIDTH * DISPLAY_TERM_HEIGHT];
        } term_mode;
    } data;
} DisplayData;

void init_display_data(DisplayData *display_data) {
    int display_window_w = (DISPLAY_WIDTH + 2);
    int display_window_h = (DISPLAY_HEIGHT + 2);

    display_data->display_mode = DISPLAY_MODE_TERMINAL;
    display_data->window_selected = 0;

    display_data->inbuild_menus_info.ram_view_scroll = 0;

    display_data->options_window = newwin(3, display_window_w, 0, 0);
    display_data->option_w_selected = 0;

    display_data->should_exit = false;

    display_data->data.term_mode.caret = 0;

    // NOTE: DISPLAY_* is the full display size (e.g. for a future RGB display),
    // DISPLAY_TERM_* is the size of the simulated terminal. term_chars backs
    // the terminal, so it is sized by DISPLAY_TERM_* (as everywhere else:
    // display_terminal_mode_putc and tick_display).
    memset(
        display_data->data.term_mode.term_chars, ' ',
        sizeof display_data->data.term_mode.term_chars
    );

    display_data->display_window = newwin(display_window_h, display_window_w, 3, 0);
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
        else if (c == '\b') {
            if (caret > 0) caret--;
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
    uint32_t sub_inst_count;
    bool single_step;
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


int get_input(CpuData *cpu_data, DisplayData *display_data) {
    if (g_plain_mode) return -1;

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

    if (display_data->window_selected == 1) {
        if (display_data->menu_open == 0) {
            if (display_data->display_mode == DISPLAY_MODE_TERMINAL) {
                if (i == KEY_BACKSPACE) {
                    return '\b';
                }
                return i;
            }
        }
        if (display_data->menu_open == 1) {
            if (i == ' ') {
                cpu_data->single_step = true;
            }
        }
        if (display_data->menu_open == 2) {
            uint32_t *scroll = &display_data->inbuild_menus_info.ram_view_scroll;

            const uint32_t total_lines = cpu_data->ram_size / ram_dump_bytes_per_line;
            const uint32_t max_scroll = total_lines > ram_dump_lines ? total_lines - ram_dump_lines : 0;

            if (i == KEY_UP) {
                if (*scroll > 0) (*scroll)--;
            }
            if (i == KEY_DOWN) {
                if (*scroll < max_scroll) (*scroll)++;
            }
            if (i == KEY_PPAGE) {
                *scroll = *scroll > ram_dump_lines ? *scroll - ram_dump_lines : 0;
            }
            if (i == KEY_NPAGE) {
                *scroll = *scroll + ram_dump_lines < max_scroll ? *scroll + ram_dump_lines : max_scroll;
            }
            if (i == ' ') {
                cpu_data->single_step = true;
            }
            if (i == 'f' || i == 'F') {
                *scroll = (cpu_data->regs[REG_PC] + cpu_data->regs[REG_MS]) / ram_dump_bytes_per_line;
            }
        }
    }

    return -1;
}


int init_devices_data(DevicesData *devices_data) {
    devices_data->terminal_io_device_data = malloc(sizeof(TerminalIoDeviceData));
    devices_data->disk_io_device_data = malloc(sizeof(DiskIoDevice));

    if (devices_data->terminal_io_device_data == 0) {
        free(devices_data->disk_io_device_data);
        devices_data->disk_io_device_data = NULL;
        return 1;
    }

    if (devices_data->disk_io_device_data == 0) {
        free(devices_data->terminal_io_device_data);
        devices_data->terminal_io_device_data = NULL;
        return 1;
    }

    devices_data->terminal_io_device_data->buffered_input = 0;
    devices_data->terminal_io_device_data->last_input = 0;
    devices_data->disk_io_device_data->address = 0;
    devices_data->disk_io_device_data->disk_file = fopen("disk.bin", "r+b");
    if (!devices_data->disk_io_device_data->disk_file) {
        devices_data->disk_io_device_data->disk_file = fopen("disk.bin", "w+b");
        if (!devices_data->disk_io_device_data->disk_file) {
            free(devices_data->terminal_io_device_data);
            devices_data->terminal_io_device_data = NULL;
            free(devices_data->disk_io_device_data);
            devices_data->disk_io_device_data = NULL;
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
                if (g_plain_mode) {
                    putc(c, stdout);
                    fflush(stdout);
                }
                else {
                    display_terminal_mode_putc(display_data, c);
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


void update_devices(CpuData *cpu_data, DevicesData *devices_data, DisplayData *display_data) {
    // NULL device
    { }

    // Terminal IO device
    {
        uint32_t input = get_input(cpu_data, display_data);

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
    if (cpu_push_stack_uint8(cpu_data, value)) return 1;
    if (cpu_push_stack_uint8(cpu_data, value >> 8)) return 1;

    return 0;
}

int cpu_push_stack_uint32(CpuData *cpu_data, uint32_t value) {
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
    (void)cpu_data;
    (void)devices_data;
    (void)ips;
    (void)fps;

    int display_width = 0;
    int display_height = 0;
    
    getmaxyx(stdscr, display_height, display_width);
    display_data->window_selected %= 2;

    WINDOW *display_w = display_data->display_window;

    if (display_data->window_selected == 1) {
        wattron(display_w, COLOR_PAIR(1));
    }

    if (display_data->menu_open == 0) {
        const int offset_x = (DISPLAY_WIDTH - DISPLAY_TERM_WIDTH) / 2;
        const int offset_y = (DISPLAY_HEIGHT - DISPLAY_TERM_HEIGHT) / 2;

        for (int y = 0; y < DISPLAY_TERM_HEIGHT; y++) {
            wmove(display_w, 1 + y + offset_y, 1 + offset_x);
            for (int x = 0; x < DISPLAY_TERM_WIDTH; x++) {
                waddch(display_w, display_data->data.term_mode.term_chars[x + y * DISPLAY_TERM_WIDTH]);
            }
        }
    }
    else if (display_data->menu_open == 1) {
        mvwprintw(display_w, 1, 1, "REG    HEX VALUE    DEC VALUE");
        mvwprintw(display_w, 1, 1 + 35, "REG    HEX VALUE    DEC VALUE");
        for (int r = 0; r < NUM_REGS; r++) {
            uint32_t v = cpu_data->regs[r];
            mvwprintw(display_w, 3 + r % (NUM_REGS / 2), 1 + r / (NUM_REGS / 2) * 35, "0x%02x   0x%08x   %12d", r, v, v);
        }

        mvwprintw(display_w, DISPLAY_HEIGHT, 1, "SPACE to step the simulation");

        wattron(display_w, A_DIM);
        mvwhline(display_w, 2, 1, 0, 67);
        mvwvline(display_w, 1, 6, 0, NUM_REGS / 2 + 2);
        mvwvline(display_w, 1, 19, 0, NUM_REGS / 2 + 2);
        mvwvline(display_w, 1, 35 + 6, 0, NUM_REGS / 2 + 2);
        mvwvline(display_w, 1, 35 + 19, 0, NUM_REGS / 2 + 2);
        wattroff(display_w, A_DIM);

        mvwvline(display_w, 1, 34, 0, NUM_REGS / 2 + 2);
    }
    else if (display_data->menu_open == 2) {
        for (int y = 0; y < ram_dump_lines; y++) {
            uint32_t line_addr = (y + display_data->inbuild_menus_info.ram_view_scroll) * ram_dump_bytes_per_line;
            mvwprintw(display_w, 1 + y, 1, "%08x :", line_addr);

            for (int x = 0; x < ram_dump_bytes_per_line; x++) {
                uint32_t byte_addr = line_addr + x;

                if (byte_addr == cpu_data->regs[REG_MS] + cpu_data->regs[REG_PC]) {
                    wattron(display_w, A_REVERSE);
                }

                if (byte_addr < cpu_data->ram_size) {
                    uint8_t byte = cpu_data->ram[byte_addr];
                    mvwprintw(display_w, 1 + y, 12 + x * 3, "%02x", byte);
                    if (32 <= byte && byte < 128) {
                        mvwprintw(display_w, 1 + y, 12 + ram_dump_bytes_per_line * 3 + 1 + x, "%c", byte);
                    }
                    else {
                        wattron(display_w, A_DIM);
                        mvwprintw(display_w, 1 + y, 12 + ram_dump_bytes_per_line * 3 + 1 + x, ".");
                        wattroff(display_w, A_DIM);
                    }
                }
                else {
                    mvwprintw(display_w, 1 + y, 12 + x * 3, "..");
                    mvwprintw(display_w, 1 + y, 12 + ram_dump_bytes_per_line * 3 + 1 + x, ".");
                }

                if (byte_addr == cpu_data->regs[REG_MS] + cpu_data->regs[REG_PC]) {
                    wattroff(display_w, A_REVERSE);
                }
            }
        }

        mvwhline(display_w, ram_dump_lines, 0, 0, DISPLAY_WIDTH);

        uint32_t current_pc = cpu_data->regs[REG_MS] + cpu_data->regs[REG_PC];

        if (current_pc < cpu_data->ram_size)
            mvwprintw(display_w, DISPLAY_HEIGHT - 1, 1, "INST: %-8s (%02x)", instruction_names[cpu_data->ram[current_pc]], cpu_data->ram[current_pc]);
        else
            mvwprintw(display_w, DISPLAY_HEIGHT - 1, 1, "INST: ........ (..)");
        mvwprintw(display_w, DISPLAY_HEIGHT, 1, "SPACE to step the simulation | F to jump to current PC");
    }

    box(display_w, 0, 0);

    if (display_data->window_selected == 1) {
        wattroff(display_w, COLOR_PAIR(1));
    }

    wrefresh(display_w);

    WINDOW *menu_bar_w = display_data->options_window;

    if (display_data->window_selected == 0) wattron(menu_bar_w, COLOR_PAIR(1));

    box(menu_bar_w, 0, 0);

    for (int mopt = 0; mopt < mopt_c; mopt++) {
        mvwprintw(menu_bar_w, 1, 2 + mopt * 10, "%s", menu_options[mopt]);
    }

    wattron(menu_bar_w, A_REVERSE);
    mvwprintw(menu_bar_w, 1, 2 + display_data->option_w_selected * 10, "%s", menu_options[display_data->option_w_selected]);
    wattroff(menu_bar_w, A_REVERSE);
    
    if (display_data->window_selected == 0) {
        wattroff(menu_bar_w, COLOR_PAIR(1));
    }

    //mvprintw(1, DISPLAY_TERM_WIDTH - 5, "%d", display_data->menu_open);

    wrefresh(menu_bar_w);

    refresh();
}

void free_sim_data(CpuData* cpu_data, DevicesData* devices_data, DisplayData* display_data) {
    // CPU
    
    free(cpu_data->ram);
    free(cpu_data);

    // DEVICES

    free(devices_data->terminal_io_device_data);

    fclose(devices_data->disk_io_device_data->disk_file);
    free(devices_data->disk_io_device_data);
    
    free(devices_data);

    // DISPLAY
    if (!g_plain_mode) {
        delwin(display_data->options_window);
        delwin(display_data->display_window);
    }

    free(display_data);
}

int load_rom_to_ram(const char *path, CpuData *cpu_data) {
    FILE *bin_file = fopen(path, "rb");
    if (!bin_file) {
        return 1;
    }

    uint32_t file_size = get_file_size(path);

    if (file_size > cpu_data->ram_size) {
        return 1;
    }

    fread(cpu_data->ram, 1, file_size, bin_file);

    fclose(bin_file);

    return 0;
}

struct yron2config {
    int subinsts;
    uint32_t ram_size;
};

struct yron2config *load_config_json_file() {
    char *json_string = read_file("yron2config.json");
    if (json_string == NULL) {
        return 0;
    }

    cJSON *root = cJSON_Parse(json_string);

    free(json_string);
    
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Parsing error before: %s\n", error_ptr);
        }
        return 0;
    }

    struct yron2config *config = malloc(sizeof(struct yron2config));

    config->subinsts = 1024;
    config->ram_size = 64 * 1024; // 64 KiB

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, root) {
        if (strcmp(item->string, "subinsts") == 0) {
            config->subinsts = item->valueint;
        }
        if (strcmp(item->string, "ram") == 0) {
            config->ram_size = item->valueint;
        }
    }

    return config;
}

int main(int argc, char *argv[]) {
    struct yron2config *config = load_config_json_file();

    if (config == NULL) {
        printf("Failed to load YRON2 config (yron2config.json)");
        return 1;
    }
    
    char *rom_file_path = "rom.bin";
    uint64_t timeout_insts = -1;
    {
        bool collecting_timeout_insts = false;
        for (int a = 0; a < argc; a++) {
            char *arg = argv[a];
            if (collecting_timeout_insts) {
                // Safely convert string to uint64_t using strtoull
                timeout_insts = strtoull(arg, NULL, 10);
                collecting_timeout_insts = false; // Reset the flag so it doesn't consume the next argument
            }
            else if (strcmp(arg, "-p") == 0) {
                g_plain_mode = true;
            }
            else if (strcmp(arg, "-timeout") == 0) {
                collecting_timeout_insts = true;
            }
            else {
                rom_file_path = arg;
            }
        }
    }

    CpuData *cpu_data = malloc(sizeof(CpuData));

    if (cpu_data == 0)
    {
        return 1;
    }
    
    cpu_data->ram_size = config->ram_size;
    cpu_data->sub_inst_count = config->subinsts;

    free(config);
    
    cpu_data->ram = malloc(cpu_data->ram_size);

    if (cpu_data->ram == 0)
    {
        free(cpu_data);
        return 1;
    }

    DisplayData* display_data = malloc(sizeof(DisplayData));

    if (display_data == 0)
    {
        free(cpu_data->ram);
        free(cpu_data);
        return 1;
    }

    DevicesData *devices_data = malloc(sizeof(DevicesData));

    if (devices_data == 0) {
        free(display_data);
        free(cpu_data->ram);
        free(cpu_data);
        return 1;
    }

    {
        int init_devices_data_return = init_devices_data(devices_data);
        if (init_devices_data_return) {
            free(display_data);
            free(devices_data);
            free(cpu_data->ram);
            free(cpu_data);
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
        if (load_rom_to_ram(rom_file_path, cpu_data)) {
            free(display_data);
            free(devices_data->terminal_io_device_data);
            fclose(devices_data->disk_io_device_data->disk_file);
            free(devices_data->disk_io_device_data);
            free(devices_data);
            free(cpu_data->ram);
            free(cpu_data);
        }
    }

    // MAIN LOOP

    if (!g_plain_mode) {
        initscr();
        noecho();
        cbreak();
        keypad(stdscr, true);
        nodelay(stdscr, true);
        curs_set(0);
        raw();

        if (has_colors() == false) {
            endwin();
            free(display_data);
            free(devices_data->terminal_io_device_data);
            fclose(devices_data->disk_io_device_data->disk_file);
            free(devices_data->disk_io_device_data);
            free(devices_data);
            free(cpu_data->ram);
            free(cpu_data);
            return 1;
        }
        start_color();

        init_pair(1, COLOR_YELLOW, COLOR_BLACK);

        init_display_data(display_data);
    } else {
        printf("===== PROGRAM OUTPUT - TERM 1 - START =====\n\n");
    }

    struct timespec last_ips_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_ips_time);

    uint64_t instruction_count = 0;
    uint64_t tick = 0;
    uint32_t current_ips = 0;

    int frames = 0;
    int fps = 0;

    while (true) {
        if (timeout_insts == 0) {
            break;
        }

        struct timespec loop_start, loop_end;
        clock_gettime(CLOCK_MONOTONIC, &loop_start);

        if (!g_plain_mode) tick_display(display_data, cpu_data, devices_data, current_ips, fps);
        frames++;

        if (g_plain_mode || (display_data->menu_open == 0 && display_data->window_selected == 1)) {
            for (int si = 0; si < cpu_data->sub_inst_count; si++) {
                uint32_t prev_rpc = cpu_data->regs[REG_MS] + cpu_data->regs[REG_PC];
                if (cpu_data->breakpoint_triggered) {
                    break;
                }
                tick_cpu(cpu_data, devices_data, display_data);
                instruction_count++;
                if (timeout_insts != (uint64_t)-1) {
                    timeout_insts--;
                    if (timeout_insts == 0) {
                        break;
                    }
                }
                if (cpu_data->regs[REG_MS] + cpu_data->regs[REG_PC] == prev_rpc && g_plain_mode) {
                    timeout_insts = 0;
                }
                tick++;
            }
        }
        else if (cpu_data->single_step) {
            tick_cpu(cpu_data, devices_data, display_data);
            instruction_count++;
            cpu_data->single_step = false;
            tick++;
        }
        
        update_devices(cpu_data, devices_data, display_data);

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
    printf("\n===== PROGRAM OUTPUT - TERM 1 -  END  =====\n");

    printf("Processed %lu ticks.\n", tick);

    if (!g_plain_mode) {
        cbreak();
        endwin();
    }

    free_sim_data(cpu_data, devices_data, display_data);
}
