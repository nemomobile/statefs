#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#define WATCHER void*

WATCHER createFileWatcher(char* filePaths[], int count);
void deleteFileWatcher(WATCHER watcher);
void listenFileChanges(WATCHER watcher, void (*listener)(char *));

#endif
