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
from subprocess import PIPE, Popen, check_output
import os, sys
from time import sleep

statefs_bin = None
tests_path = None
default_loader = None

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
        global statefs_bin, tests_path, default_loader
        self.server_path = statefs_bin
        self.provider_path = os.path.join(tests_path, "libtest.so")
        self.rootdir = "/tmp/statefs-test"
        self.default_loader = default_loader

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
        timeout = 50
        while timeout:
            sleep(0.1)
            output = check_output("mount | grep {} | wc -l".format(self.mntdir), shell=True)
            if int(output.strip()) > 0:
                sleep(0.1)
                break
            timeout -= 1

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
    def default_loader_registration(self):
        rc = subprocess.call(self.__cmd("register", self.default_loader))
        self.ensure_eq(rc, 0, "loader registration")
        self.ensure_eq(set(os.listdir(self.cfgdir)),
                       set(('loader-default.conf',)),
                       "config file should appear in the config dir")

    @test
    def provider_registration(self):
        rc = subprocess.call(self.__cmd("register", self.provider_path))
        self.ensure_eq(rc, 0, "provider registration")
        self.ensure_eq(set(os.listdir(self.cfgdir)),
                       set(('provider-test.conf','loader-default.conf')),
                       "config file should appear in the config dir")
        self.providers_dir = os.path.join(self.mntdir, "providers")
        self.namespaces_dir = os.path.join(self.mntdir, "namespaces")
        timeout = 50
        while timeout:
            providers = os.listdir(self.providers_dir)
            if len(providers) != 0:
                break
            timeout -= 1
            sleep(0.1)
        self.ensure_eq(set(providers), set(('test',)),
                       "test provider should be loaded")

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
    tests_path = os.path.dirname(sys.argv[0])
    if len(sys.argv) == 3:
        statefs_bin = sys.argv[1]
        default_loader = sys.argv[2]
    elif len(sys.argv) > 1:
        raise Exception("Unknown arguments: {}".format(args))
    else:
        statefs_bin = os.path.join(tests_path, '../src/statefs')
        default_loader = os.path.join(tests_path, '../src/libloader-default.so')

    default_main(StateFS)

