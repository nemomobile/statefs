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
