#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

// Global termios struct to store original settings
static struct termios g_orig_termios;

void restore_terminal(void) {
    // Restore original terminal attributes
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
}

void setup_terminal(void) {
    struct termios newt;

    // Save original terminal settings
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    
    // Register automatic cleanup upon exit
    atexit(restore_terminal);

    newt = g_orig_termios;

    // Turn off canonical mode (ICANON) and echo (ECHO)
    newt.c_lflag &= ~(ICANON | ECHO);

    // Apply settings immediately and flush any unread input buffer
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &newt);
}

uint32_t get_input_nb() {
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
}
