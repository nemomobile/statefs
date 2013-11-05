#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include <stdbool.h>

typedef struct {
    char** filepaths;
    int count;
    int* fds;
    struct pollfd* pfds;
} Watcher;

Watcher createFileWatcher(char* filepaths[], int count);
bool openWatcher(Watcher* watcher);
void listenFileChanges(Watcher* watcher, void (*callback)(char*));
void deleteFileWatcher(Watcher* watcher);

#endif
