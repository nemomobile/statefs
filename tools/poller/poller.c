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

int main(int argc, char * argv[])
{
    const int count = argc - 1;
    int i;
    int *fds;
    struct pollfd *pfds;
    int rc = -1;
    char buf[1024];

    if (count < 1) {
        printf("Usage: %s filenames...\n", argv[0]);
        exit(-1);
    }

    fds = malloc(sizeof(fds[0]) * count);
    pfds = malloc(sizeof(pfds[0]) * count);

    for (i = 0; i < count; ++i) {
        char const *name = argv[i + 1];
        fds[i] = open(name, O_RDONLY);
        printf("Subscribe to %s: %d\n", name, fds[i]);
        if (fds[i] < 0) {
            printf("Can't open %s\n", name);
            goto out;
        }

        memset(&pfds[i], 0, sizeof(pfds[0]));
        pfds[i].fd = fds[i];
        pfds[i].events = POLLIN | POLLPRI;
    }

    while (true) {
        for (i = 0; i < count; ++i) {
            memset(&pfds[i], 0, sizeof(pfds[0]));
            pfds[i].fd = fds[i];
            pfds[i].events = POLLIN | POLLPRI;
        }

        printf("Polling...\n");
        rc = poll(pfds, count, -1);
        if (rc < 0) {
            printf("poll returned %d: %s\n", rc, strerror(errno));
            goto out;
        }

        if (!rc) {
            printf("No events... timeout?");
            continue;
        }

        for (i = 0; i < count; ++i) {
            struct pollfd *pfd = &pfds[i];
            if (pfd->revents == 0)
                continue;

            int fd = fds[i]; 
            char const *fname = argv[i + 1];

            printf("file %s: %d\n", fname, fd);
            lseek(fd, 0, SEEK_SET);
            printf("rc %i, ev %x, rev %x\n", rc, pfd->events, pfd->revents);
            if (pfd->revents & (POLLERR | POLLHUP | POLLNVAL)) {
                printf("Poll error(E H N)=(%d %d %d)\n"
                       , pfd->revents & (POLLERR)
                       , pfd->revents & (POLLHUP)
                       , pfd->revents & (POLLNVAL)
                    );
                goto out;
            }
            memset(buf, 0, sizeof(buf));
            int rc = read(fd, buf, sizeof(buf) - 1);
            printf("read %i\n", rc);
            if (rc < 0) {
                printf("Error %d (%s) reading %s\n", rc, strerror(errno)
                       , fname);
                goto out;
            }
            if (rc) {
                buf[rc] = 0;
                printf("%s\n", buf);
            }
        }
    }
    rc = 0;
out:
    free(pfds);
    free(fds);
    return rc;
}
