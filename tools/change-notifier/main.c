#include "filewatcher.h"

#include <stdio.h>
#include <stdlib.h>

void onFileChanged(char *filepath);

int main(int argc, char * argv[]) {
    Watcher watcher = createFileWatcher(argv + 1, argc - 1);

    if (watcher.count < 1) {
        fprintf(stderr, "Usage: %s filenames...\n", argv[0]);
        exit(-1);
    }

    if (openWatcher(&watcher)) {
        listenFileChanges(&watcher, onFileChanged);
    }
    deleteFileWatcher(&watcher);

    return 0;
}

void onFileChanged(char *filepath) {
    printf("%s\n", filepath);
}
