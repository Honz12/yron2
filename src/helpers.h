#pragma once

#include <stdio.h>
#include <stdint.h>

uint32_t get_file_size(const char* file_path);
void process_sleep(unsigned int micro_seconds);
int write_byte(FILE *disk, uint32_t offset, uint8_t data);
int read_byte(FILE *disk, uint32_t offset, uint8_t *out_data);
