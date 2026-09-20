#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
    // Global variable to store original Windows console mode
    static DWORD g_orig_console_mode;
#else
    #include <unistd.h>
    #include <termios.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    // Global variable to store original Linux terminal settings
    static struct termios g_orig_termios;
#endif

void restore_terminal(void) {
#ifdef _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hStdin, g_orig_console_mode);
#else
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
#endif
}

void setup_terminal(void) {
#ifdef _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    
    // Save original console mode
    GetConsoleMode(hStdin, &g_orig_console_mode);
    
    // Register automatic cleanup upon exit
    atexit(restore_terminal);

    // Disable canonical input (line buffering) and echo
    DWORD new_mode = g_orig_console_mode;
    new_mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    SetConsoleMode(hStdin, new_mode);
#else
    struct termios newt;
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    atexit(restore_terminal);

    newt = g_orig_termios;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &newt);
#endif
}

uint32_t get_input_nb() {
#ifdef _WIN32
    // Check if a key has been pressed without blocking
    if (_kbhit()) {
        return (uint32_t)_getch();
    }
    return 0;
#else
    struct termios oldt, newt;
    char ch = 0;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    int read_bytes = read(STDIN_FILENO, &ch, 1);
    if (read_bytes <= 0) {
        ch = 0;
    }

    fcntl(STDIN_FILENO, F_SETFL, oldf);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
#endif
}

uint32_t get_file_size(const char* file_path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(file_path, GetFileExInfoStandard, &fad)) {
        printf("Failed to get file attributes.\n");
        return 0; 
    }
    // Combines low and high DWORDs to handle files larger than 4GB safely
    LARGE_INTEGER size;
    size.LowPart = fad.nFileSizeLow;
    size.HighPart = fad.nFileSizeHigh;
    return (uint32_t)size.QuadPart;
#else
    struct stat st;
    if (stat(file_path, &st) != 0) {
        printf("Failed to stat file.\n");
        return 0;
    }
    return st.st_size;
#endif
}

void process_sleep(unsigned int micro_seconds) {
#ifdef _WIN32
    // Negative values specify relative time in 100-nanosecond intervals.
    // 1 microsecond = 10 units of 100-nanoseconds.
    LARGE_INTEGER due_time;
    due_time.QuadPart = -(LONGLONG)(micro_seconds * 10);

    HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
    if (timer != NULL) {
        SetWaitableTimer(timer, &due_time, 0, NULL, NULL, 0);
        WaitForSingleObject(timer, INFINITE);
        CloseHandle(timer);
    }
#else
    usleep(micro_seconds);
#endif
}

int write_byte(FILE *disk, uint32_t offset, uint8_t data) {
    // Move file pointer to the target offset relative to start (SEEK_SET)
    if (fseek(disk, offset, SEEK_SET) != 0) {
        return -1; // Seek error
    }
    
    // Write 1 byte from the address of 'data'
    if (fwrite(&data, sizeof(uint8_t), 1, disk) != 1) {
        return -1; // Write error
    }
    
    // Flush to ensure data is written out of the stream buffer
    fflush(disk);
    return 0; // Success
}

int read_byte(FILE *disk, uint32_t offset, uint8_t *out_data) {
    // Move file pointer to the target offset
    if (fseek(disk, offset, SEEK_SET) != 0) {
        return -1; // Seek error
    }
    
    // Read 1 byte into out_data
    if (fread(out_data, sizeof(uint8_t), 1, disk) != 1) {
        return -1; // Read error (e.g., attempt to read past End-Of-File)
    }
    
    return 0; // Success
}
