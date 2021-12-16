/*
 * how_to_get_input_without_enter.cpp
 *
 */


#include <stdio.h>
#include <termios.h>


int hit_key() {
    termios oldattr;
    tcgetattr(0, &oldattr);

    termios newattr = oldattr;
    newattr.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &newattr);

    int code = getchar();

    tcsetattr(0, TCSANOW, &oldattr);

    return code;
}


int main() {
    printf("\nPlease hit a key (Ctrl-C to quit).\n\n");
    while (true) {
        int code = hit_key();
        printf("\nYour input is %c, its ASCII code is %d.\n", code, code);
    }
}

