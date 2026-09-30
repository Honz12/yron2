#include <stdio.h>
#include <stdint.h>

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
