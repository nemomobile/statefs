#!/usr/bin/python

from UT import test, Suite, default_main

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

class OtherTests(Suite):

    def init_a(self):
        self.a = 1

    suite_setup = [init_a]
    def before_test(self):
        self.a = 13
        self.teardown.append(self.after_first_test)

    def after_first_test(self):
        self.a = 31

    @test(setup = before_test)
    def test1(self):
        '''Other first test info'''
        self.ensure_eq(self.a, 13, "initialized in setup")

    @test
    def test2(self):
        '''Other first test info'''
        self.ensure_eq(self.a, 31, "after first")


if __name__ == '__main__':
    default_main(Tests, OtherTests)
