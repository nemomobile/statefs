#include "filecontents.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

FileContents readFileContents(char *filepath) {
    FileContents contents = {
        0, 0
    };
    FILE* file = fopen(filepath, "r");

    if (file) {
        fseek(file, 0, SEEK_END);
        contents.length = ftell(file);
        fseek(file, 0, SEEK_SET);
        contents.data = malloc(contents.length);
        if (contents.data) {
            fread(contents.data, 1, contents.length, file);
        }
        fclose(file);
    }
    return contents;
}

void freeFileContents(FileContents contents) {
    free(contents.data);
}

bool isFileContentsEqual(FileContents c1, FileContents c2) {
    if (c1.length != c2.length) {
        return false;
    }
    return !memcmp(c1.data, c2.data, c1.length);
}
