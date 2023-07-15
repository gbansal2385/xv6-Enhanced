#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(2, "Usage: strace mask command [args]\n");
        exit(1);
    }

    int mask = atoi(argv[1]);
    if (mask <= 0)
    {
        fprintf(2, "strace: mask must be a postive integer\n");
        exit(1);
    }

    trace(mask);

    char *cmd[MAXARG];
    for (int i = 2; i < argc; i++)
    {
        cmd[i - 2] = argv[i];
    }

    exec(cmd[0], cmd);
    fprintf(2, "exec %s failed\n", cmd[0]);
    exit(0);
}