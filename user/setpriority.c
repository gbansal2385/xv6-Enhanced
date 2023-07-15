#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(2, "Usage: setpriority priority pid\n");
        exit(1);
    }

    int priority = atoi(argv[1]);
    if (priority < 0 || priority > 100)
    {
        fprintf(2, "setpriority: priority must be in the range 0 to 100\n");
        exit(1);
    }

    int pid = atoi(argv[2]);
    if (pid < 0)
    {
        fprintf(2, "setpriority: pid must be a postive integer\n");
        exit(1);
    }

    set_priority(priority, pid);
    exit(0);
}