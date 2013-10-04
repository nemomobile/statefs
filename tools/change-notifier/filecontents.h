#ifndef FILECONTENTS_H
#define FILECONTENTS_H

#include <stdbool.h>

typedef struct {
    char* data;
    int length;
} FileContents;

FileContents readFileContents(char *filepath);
void freeFileContents(FileContents contents);
bool isFileContentsEqual(FileContents c1, FileContents c2);

#endif
