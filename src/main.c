#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


#define NUM_REGS 32


typedef struct {
    uint32_t regs[NUM_REGS];
    uint8_t* ram;
} CpuData;


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
    int ram_size = 64 * 1024;
    
    cpu_data->ram = malloc(ram_size);

    if (cpu_data->ram == 0)
    {
        printf("FAILED TO ALLOCATE RAM\n");
        return 1;
    }
    printf("Allocated RAM\n");

    for (int i = 0; i < NUM_REGS; i++)
    {
        cpu_data->regs[i] = 0;
    }

    free(cpu_data->ram);
    printf("Freed RAM\n");
    free(cpu_data);
    printf("Freed CpuData struct\n");
}
