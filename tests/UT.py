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

# Some code was taken from Python 2.6 unittest.py. Copyright is following.

# Copyright (c) 1999-2003 Steve Purcell
# This module is free software, and you may redistribute it and/or modify
# it under the same terms as Python itself, so long as this copyright message
# and disclaimer are retained in their original form.

# IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
# SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF
# THIS CODE, EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

# THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE.  THE CODE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS,
# AND THERE IS NO OBLIGATION WHATSOEVER TO PROVIDE MAINTENANCE,
# SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

import sys
import traceback
import re

# just to track this is the UT module
__unittest = 1

class StackParser:

    __header_re = re.compile(r'^Traceback \(mo.+')
    __fname_re = re.compile(r'\s*File "(.+)", line ([0-9]+), in (.+)')
    __exc_line = re.compile(r'^(Exception|Failure): (.+)')

    def __init__(self):
        self.last_fname = "?"
        self.err = "?"
        self.locations = {}
        self.info = []
        self.__parse = self.__parse_start

    def __parse_remain(self, x):
        self.info.append(x)

    def __parse_start(self, x):
        if self.__header_re.match(x):
            self.__parse = self.__parse_stack

    def __parse_stack(self, x):
        m = self.__exc_line.match(x)
        if m:
            self.err = m.group(2)
            self.__parse = self.__parse_remain
            return

        m = self.__fname_re.match(x)
        if m:
            k = ':'.join([m.group(i) for i in range(1,4)])
            self.last_fname = k
        else:
            loc = self.last_fname
            if not loc in self.locations:
                self.locations[loc] = []
            self.locations[loc].append('"{0}"'.format(str(x).strip()))

    def parse(self, stack):
        [self.__parse(x) for x in stack.split("\n")]
        traces = {loc : '\n'.join(lines) \
                      for loc, lines in self.locations.items() }
        return self.err, traces

class Test(object):
    def __init__(self):
        self.__id = Test.__new_id()

    def prepare(self, fn):
        self.result = None
        self.short_doc = fn.__doc__.split('\n')[0].strip() if fn.__doc__ else ''
        self.__fn = fn
        self.__mk_special_method(fn, 'setup')
        return self

    __next_test_id = 0

    def __mk_special_method(self, fn, name):
        method = fn.unit_test_args.get(name, None)
        if method:
            method = method if type(method) == str else method.__name__
            setattr(self, name, getattr(fn.__self__, method))
        else:
            setattr(self, name, lambda : False)

    @staticmethod
    def __new_id():
        res = Test.__next_test_id
        Test.__next_test_id += 1
        return res

    def __str__(self):
        return self.name

    def __repr__(self):
        return "Test({})".format(self.name)

    @property
    def number(self):
        return self.__id

    @property
    def name(self):
        return self.__fn.__name__

    def __enter__(self):
        self.setup()

    def __exit__(self, err_type, err, tb):
        self.__traceback = (err_type, err, tb)
        try:
            self.__fn.__self__._teardown()
        except Exception as e:
            self.__traceback = sys.exc_info()
            raise e
            

    @property
    def traceback(self):
        return self.__traceback

    def __call__(self, *args, **kwargs):
        self.__fn(*args, **kwargs)

def test(*args, **kwargs):
    def modify(fn):
        fn.unit_test = Test()
        fn.unit_test_args = kwargs
        return fn
        
    if len(kwargs):
        return modify

    return modify(args[0])

def is_test(fn):
    return hasattr(fn, 'unit_test')

class Suite(object):
    def __init__(self):
        self.__report = None
        test_names = [name for name in dir(self.__class__) \
                          if is_test(getattr(self.__class__, name))]
        methods = [(name, getattr(self, name)) for name in test_names]
        self.__tests = { name : fn.unit_test.prepare(fn) for name, fn in methods }

        self.log = lambda *args, **kwargs: False

        self.suite_teardown = []
        self.teardown = []

    @property
    def name(self):
        return type(self).__name__

    @property
    def tests(self) :
        return self.__tests.values()

    def __enter__(self):
        if hasattr(self, 'suite_setup'):
            [getattr(self, fn.__name__)() for fn in self.suite_setup]

    def __exit__(self, *args):
        if hasattr(self, 'suite_teardown'):
            self.__teardown(self.suite_teardown)

    def __teardown(self, fns):
        while len(fns):
            fn = fns.pop()
            fn()

    def _teardown(self):
        self.__teardown(self.teardown)

    def __assertion(self, msg, *args, **kwargs):
        self.log.warning(msg.format(*args, **kwargs))
        raise Failure(msg, *args, **kwargs)
        
    def ensure(self, condition, msg, *args, **kwargs):
        if not condition:
            self.__assertion('failed:' + msg, *args, **kwargs)

    def ensure_eq(self, x, y, msg, *args, **kwargs):
        if x != y:
            fmt = 'failed: ({} == {}): {}'
            self.__assertion(fmt, x, y, msg.format(*args, **kwargs))

    def ensure_ne(self, x, y, msg, *args, **kwargs):
        if x == y:
            fmt = 'failed: ({} != {}): {}'
            self.__assertion(fmt, x, y, msg.format(*args, **kwargs))


class Format(object):
    def __init__(self, msg, *args, **kwargs):
        self.msg = msg
        self.args = args
        self.kwargs = kwargs

    def __repr__(self):
        return self.msg.format(*self.args, **self.kwargs)

class Error(object):
    def __init__(self, test, err):
        self.__test_info = test
        self.__name = test.name
        self.__err = err

    @property
    def ok(self):
        return False

    def __repr__(self):
        return ':'.join([self.__name, 'ERROR', repr(self.__err)])

    __str__ = __repr__

    @property
    def test(self):
        return self.__test_info

class Success(object):
    def __init__(self, test):
        self.__test_info = test
        self.__name = test.name

    def __repr__(self):
        return ':'.join([self.__name, 'OK'])

    __str__ = __repr__

    @property
    def test(self):
        return self.__test_info

    @property
    def ok(self):
        return True

class Failure(Exception):

    def __init__(self, msg, *args, **kwargs):
        self.__fmt = Format(msg, *args, **kwargs)

    def init(self, test):
        self.__test_info = test
        self.__name = test.name

    @property
    def test(self):
        return self.__test_info

    @property
    def ok(self):
        return False

    def __repr__(self):
        return ':'.join([self.__name, 'FAIL', repr(self.__fmt)])

    __str__ = __repr__

    @property
    def source(self):
        return self.__name

class Runner(object):
    def __init__(self, suite):
        self.__suite = suite

    def __run(self, test):
        try:
            with test:
                test()
            if test.result is None:
                test.result = Success(test)
        except Failure as e:
            e.init(test)
            test.result = e
        except Exception as e:
            test.result = Error(test, e)

    def run(self, report):
        self.__suite.log = report.log
        report.suite = self.__suite
        with report:
            tests = sorted(self.__suite.tests,
                           key = lambda v: v.number)
            with self.__suite:
                for test in tests:
                    section = report.section(test)
                    with section:
                        self.__run(test)

class Report(object):

    def __init__(self, stream, log):
        self.__suite = None
        self.__results = []
        self.stream = stream
        self.log = log

    class Section(object):
        def __init__(self, report, test):
            self.report = report
            self.__test = test

        def __enter__(self):
            self.report.log.info("Test {:s}".format(self.__test))

        def __exit__(self, *args):
            self.report.log.info("}")
            stamp = '.' if self.__test.result.ok else 'F'
            self.report.stream.write(stamp)
        
    def section(self, test):
        return Report.Section(self, test)

    def __enter__(self):
        self.log.info("Suite %s {", self.__suite.name)
        self.stream.write("{} ".format(self.__suite.name))

    def __exit__(self, *args):
        self.stream.write("\n")
        self.log.info("}")
        self.__results = self.__suite.tests
        self.__suite = None

    @property
    def suite(self):
        return self.__suite

    @suite.setter
    def suite(self, v):
        self.__suite = v

    @property
    def results(self):
        return [x.result for x in self.__results]

    @property
    def failed_count(self):
        return len([x for x in self.results if not x.ok])

    def _is_relevant_tb_level(self, tb):
        return '__unittest' in tb.tb_frame.f_globals

    def _count_relevant_tb_levels(self, tb):
        length = 0
        while tb and not self._is_relevant_tb_level(tb):
            length += 1
            tb = tb.tb_next
        return length

    def _exc_info_to_string(self, err):
        """Converts a sys.exc_info()-style tuple of values into a string."""
        exctype, value, tb = err
        if exctype is Failure :
            # Skip test runner traceback levels
            while tb and self._is_relevant_tb_level(tb):
                tb = tb.tb_next
            # Skip assert*() traceback levels
            length = self._count_relevant_tb_levels(tb)
        else:
            length = None
        return ''.join(traceback.format_exception(exctype, value, tb, length))

    def stack(self, traceback):
        parser = StackParser()
        return parser.parse(self._exc_info_to_string(traceback))

    def format_results(self):
        self.log.always("REPORT:\n")
        failed = [self.__format_result(x) for x in self.results if not x.ok ]
        self.log.always("Failed: {}\n\n".format(len(failed)))

    def __format_result(self, res):
        def log(data):
            if type(data) != str:
                [self.log.warning(':'.join(x)) for x in data.items()]
                #self.stream.writelines(data)
            else:
                self.log.warning(data)
                #self.stream.write("{:s}\n".format(data))
        err, traces = self.stack(res.test.traceback)
        log(err)
        log(traces)

def default_main(*classes):
    import logging
    log = logging.getLogger("UT")
    log.addHandler(logging.StreamHandler())
    log.setLevel(logging.CRITICAL)
    log.always = log.warning
    rc = 0
    for cls in classes:
        report = Report(sys.stderr, log)
        Runner(cls()).run(report)
        log.setLevel(logging.WARNING)
        report.format_results()
        rc += report.failed_count
    exit(rc)

class Tests(Suite):
    @test
    def test1(self):
        '''First test info'''
        return False

    @test
    def test2(self):
        '''Second test info'''
        self.ensure(False, "e {}", "s")
        return True
    
if __name__ == '__main__':
    default_main(Tests)
