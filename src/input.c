#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

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
