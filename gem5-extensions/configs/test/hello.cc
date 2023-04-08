#include <stdio.h>

int main(int argc, char *argv[])
{
    if (argc > 1) {
        printf("Howdy %s\n", argv[1]);
    } else {
        printf("Howdy fellas\n");
    }

    return 0;
}
