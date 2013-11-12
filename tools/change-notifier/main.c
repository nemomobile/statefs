/*
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Janne Hakonen <janne.hakonen@oss.tieto.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

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
