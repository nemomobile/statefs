#include "filewatcher.h"
#include <stdlib.h>
#include <sys/inotify.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include "filecontents.h"

#define INOTIFY_EVENT_SIZE (sizeof(struct inotify_event) + FILENAME_MAX + 1)
#define INOTIFY_READ_BUFFER_LENGTH (10 * INOTIFY_EVENT_SIZE)

typedef struct {
    int watchDescriptor;
    char* filepath;
    FileContents contents;
} WatchedFile;

typedef struct {
    WatchedFile *files;
    int count;
    int inotifyFd;
} FilesWatcher;

WatchedFile _createWatchedFilename(FilesWatcher* this, char *filepath);
WatchedFile* _getWatcherFile(FilesWatcher* this, int watchDescriptor);


WATCHER createFileWatcher(char* filePaths[], int count) {
    int i;
    FilesWatcher* this = (FilesWatcher*) malloc(sizeof(FilesWatcher));
    this->inotifyFd = inotify_init1(IN_CLOEXEC);
    this->files = malloc(sizeof(WatchedFile) * count);
    this->count = count;

    for (i = 0; i < count; i++) {
        char *filepath = filePaths[i];
        this->files[i] = _createWatchedFilename(this, filepath);
    }

    return this;
}

WatchedFile _createWatchedFilename(FilesWatcher* this, char *filepath) {
    WatchedFile wf;
    wf.watchDescriptor = inotify_add_watch(this->inotifyFd, filepath, IN_CLOSE_WRITE);
    if (wf.watchDescriptor < 0) {
        fprintf(stderr, "Can't open %s\n", filepath);
    }
    wf.filepath = filepath;
    wf.contents = readFileContents(filepath);
    return wf;
}

void deleteFileWatcher(WATCHER watcher) {
    FilesWatcher* this = (FilesWatcher*)watcher;
    int i;

    for (i = 0; i < this->count; i++) {
        inotify_rm_watch(this->inotifyFd, this->files[i].watchDescriptor);
    }

    free(this->files);
    free(this);
}

void listenFileChanges(WATCHER watcher, void (*listener)(char *)) {
    FilesWatcher* this = (FilesWatcher*)watcher;
    struct inotify_event* event;
    int readCount;
    char buf[INOTIFY_READ_BUFFER_LENGTH];
    char *p;
    WatchedFile *wf;
    FileContents newContents;

    while (true) {
        readCount = read(this->inotifyFd, buf, INOTIFY_READ_BUFFER_LENGTH);
        for (p = buf; p < buf + readCount;) {
            event = (struct inotify_event *) p;
            wf = _getWatcherFile(this, event->wd);

            if (wf) {
                newContents = readFileContents(wf->filepath);

                if (!isFileContentsEqual(wf->contents, newContents)) {
                    freeFileContents(wf->contents);
                    wf->contents = newContents;
                    listener(wf->filepath);
                }
            }

            p += sizeof(struct inotify_event) + event->len;
        }
    }
}

WatchedFile* _getWatcherFile(FilesWatcher* this, int watchDescriptor) {
    int i;
    for (i = 0; i < this->count; i++) {
        if (this->files[i].watchDescriptor == watchDescriptor) {
            return &this->files[i];
        }
    }
    return 0;
}


