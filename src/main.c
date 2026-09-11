#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


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

int tick_cpu(CpuData* cpu_data) {
    uint32_t pc = cpu_data->regs[REG_PC];
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
    
    // 64 KiB of RAM, for now
    cpu_data->ram_size = 64 * 1024;
    
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

    free(cpu_data->ram);
    printf("Freed RAM\n");
    free(cpu_data);
    printf("Freed CpuData struct\n");
}
