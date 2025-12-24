#ifndef TETRIS_SSH_PTY_SHIM_H
#define TETRIS_SSH_PTY_SHIM_H

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>

int ptsname_r(int fd, char *buf, size_t size)
{
    if (buf == NULL) {
        return EINVAL;
    }

    unsigned int slave_pty_number;
    int result = ioctl(fd, TIOCGPTN, &slave_pty_number);
    if (result != 0) {
        return result;
    }

    int written_bytes = snprintf(buf, size, "/dev/pts/%d", slave_pty_number);

    if ((size_t)written_bytes >= size) {
        return ERANGE;
    }

    return 0;
}

int unlockpt(int fd)
{
    int zero = 0;
    return ioctl(fd, TIOCSPTLCK, &zero);
}

#endif // TETRIS_SSH_PTY_SHIM_H

