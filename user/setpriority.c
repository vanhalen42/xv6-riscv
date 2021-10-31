#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(2, "Usage: setpriority new_priority pid");
        exit(0);
    }

    setpriority(atoi(argv[1]), atoi(argv[2]));
    exit(0);
}