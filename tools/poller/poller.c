#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#define LOG(...) fprintf(stderr, __VA_ARGS__)

int main(int argc, char * argv[])
{
    const int count = argc - 1;
    const int mode = O_RDONLY | O_DIRECT;
    int i;
    int *fds;
    struct pollfd *pfds;
    int rc = -1;
    char buf[1024];

    if (count < 1) {
        LOG("Usage: %s filenames...\n", argv[0]);
        exit(-1);
    }

    fds = malloc(sizeof(fds[0]) * count);
    pfds = malloc(sizeof(pfds[0]) * count);

    for (i = 0; i < count; ++i) {
        char const *name = argv[i + 1];
        fds[i] = open(name, mode);
        LOG("Subscribe to %s: %d\n", name, fds[i]);
        if (fds[i] < 0) {
            LOG("Can't open %s\n", name);
            goto out;
        }
    }

    while (true) {
        for (i = 0; i < count; ++i) {
            memset(&pfds[i], 0, sizeof(pfds[0]));
            pfds[i].fd = fds[i];
            pfds[i].events = POLLIN | POLLPRI;
        }

        LOG("Polling...\n");
        rc = poll(pfds, count, -1);
        if (rc < 0) {
            LOG("poll returned %d: %s\n", rc, strerror(errno));
            goto out;
        }

        if (!rc) {
            LOG("No events... timeout?");
            continue;
        }

        for (i = 0; i < count; ++i) {
            struct pollfd *pfd = &pfds[i];
            if (pfd->revents == 0)
                continue;

            int fd = fds[i];
            char const *fname = argv[i + 1];

            LOG("file %s: %d\n", fname, fd);
            LOG("rc %i, ev %x, rev %x\n", rc, pfd->events, pfd->revents);
            if (pfd->revents & (POLLERR | POLLHUP | POLLNVAL)) {
                LOG("Poll error(E H N)=(%d %d %d)\n"
                       , pfd->revents & (POLLERR)
                       , pfd->revents & (POLLHUP)
                       , pfd->revents & (POLLNVAL)
                    );
            }
            memset(buf, 0, sizeof(buf));
            int rc = read(fd, buf, sizeof(buf) - 1);
            LOG("read %i\n", rc);
            if (rc < 0) {
                LOG("Error %d (%s) reading %s\n", rc, strerror(errno)
                       , fname);
                goto out;
            }
            lseek(fd, 0, SEEK_SET);
            if (rc) {
                buf[rc] = 0;
                printf("%s=%s\n", argv[i + 1], buf);
            }
        }
    }
    rc = 0;
out:
    free(pfds);
    free(fds);
    return rc;
}
