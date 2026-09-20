#include "helpers.c"

void restore_terminal(void);
void setup_terminal(void);
uint32_t get_input_nb();
uint32_t get_file_size(const char* file_path);
void process_sleep(unsigned int micro_seconds);
int write_byte(FILE *disk, uint32_t offset, uint8_t data);
int read_byte(FILE *disk, uint32_t offset, uint8_t *out_data);
