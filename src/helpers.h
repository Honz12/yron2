#include "helpers.c"

void restore_terminal(void);
void setup_terminal(void);
uint32_t get_input_nb();
uint32_t get_file_size(const char* file_path);
void process_sleep(unsigned int micro_seconds);
