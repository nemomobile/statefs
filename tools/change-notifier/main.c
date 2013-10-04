#include <stdio.h>
#include <stdlib.h>
#include "filewatcher.h"

void onFileChanged(char *filepath);


int main(int argc, char * argv[]) {
    int count = argc - 1;
    WATCHER watcher;

    if (count < 1) {
        printf("Usage: %s filenames...\n", argv[0]);
        exit(-1);
    }

    watcher = createFileWatcher(argv + 1, count);
    listenFileChanges(watcher, onFileChanged);

    deleteFileWatcher(watcher);
    return 0;
}

void onFileChanged(char *filepath) {
    printf("%s\n", filepath);
}
