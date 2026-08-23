#include <stdio.h>

int main(int argc, char** argv)
{
    puts("Hello TabOS!");
    for (int index = 0; index < argc; ++index) {
        printf("argv: %s\n", argv[index]);
    }
    return 0;
}
