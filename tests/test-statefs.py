#!/usr/bin/python
# -*- coding: utf-8 -*-

# Copyright (C) 2013 Jolla Ltd.
# Contact: Denis Zalevskiy <denis.zalevskiy@jollamobile.com>

# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301 USA

# http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html


from UT import test, Suite, default_main

import subprocess
from subprocess import PIPE, Popen
import os
from time import sleep

def items(*args):
    return args

def shell(cwd, cmd, *args):
    p = Popen(items(cmd, *args), stdout=PIPE, stderr=PIPE, cwd = cwd)
    out, err = p.communicate()
    return out, err, p.returncode


def mkdir(name):
    os.path.exists(name) or os.makedirs(name)

class StateFS(Suite):

    def init_paths(self):
        self.server_path = "../src/statefs"
        self.provider_path = "./libtest.so"
        self.rootdir = "/tmp/statefs-test"

    def create_tree(self):
        mkdir(self.rootdir)
        self.cfgdir = os.path.join(self.rootdir, "config")
        os.path.exists(self.cfgdir) and os.system("rm -rf " + self.cfgdir)
        self.mntdir = os.path.join(self.rootdir, "state")
        mkdir(self.cfgdir)
        mkdir(self.mntdir)
        cmd = [self.server_path, "--statefs-config-dir", self.cfgdir]
        self.__cmd = lambda *params: cmd + [x for x in params]

    def run_fuse_server(self):
        self.server = Popen(self.__cmd("-f", self.mntdir),
                            stdout=PIPE, stderr=PIPE)
        self.suite_teardown.append(self.terminate_server)
        sleep(1)

    def terminate_server(self):
        self.server.terminate()
        self.server.wait()

    suite_setup = [init_paths, create_tree, run_fuse_server]

    @test
    def initial_structure(self):
        state_dirs = os.listdir(self.mntdir)
        self.ensure_eq(set(state_dirs),
                       set(('namespaces', 'providers')),
                       "basic structure")

    @test
    def registration(self):
        rc = subprocess.call(self.__cmd("register", self.provider_path))
        self.ensure_eq(rc, 0, "provider registration")
        self.ensure_eq(set(os.listdir(self.cfgdir)),
                       set(('test.scm',)),
                       "config file should appear in the config dir")
        self.providers_dir = os.path.join(self.mntdir, "providers")
        self.namespaces_dir = os.path.join(self.mntdir, "namespaces")
        self.ensure_eq(set(os.listdir(self.providers_dir)),
                       set(('test',)),
                       "statefs should notify test provider")

    @test
    def provider_introspection(self):
        test_dir = os.path.join(self.providers_dir, "test")
        self.ensure_eq(set(os.listdir(test_dir)),
                       set(('ns1',)),
                       "namespace ns1 is provided")
        self.ensure_eq(set(os.listdir(self.namespaces_dir)),
                       set(('ns1',)),
                       "namespace ns1 properties are exposed")

        def test_tree(root_path):
            ns1_dir = os.path.join(root_path, "ns1")
            files = ('a', 'b', 'c')
            self.ensure_eq(set(os.listdir(ns1_dir)), set(files),
                           "wrong files in namespace ns1 from {}", root_path)
            self.file_paths = {f : os.path.join(ns1_dir, f) for f in files}

        test_tree(test_dir)
        test_tree(self.namespaces_dir)

    @test
    def properties(self):
        content = { 'a' : '1', 'b' : '20', 'c' : '300' }
        [self.ensure_eq(open(fname, 'r').readline().strip(), content[name],
                        "expected file {} content", name) \
             for name, fname in self.file_paths.items()]

if __name__ == '__main__':
    default_main(StateFS)

