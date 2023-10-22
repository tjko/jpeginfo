#!/usr/bin/env python3
#
# test.py -- Unit tests for jpeginfo
#
# Copyright (C) 2023 Timo Kokkonen <tjko@iki.fi>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

"""jpeginfo unit tester"""

import unittest
import subprocess

class JpeginfoTests(unittest.TestCase):
    """jpeginfo test cases"""

    program = '../jpeginfo'
    debug = False

    def run_test(self, args, check=True):
        """execute jpeginfo for a test"""
        command = [self.program]
        if isinstance(args, list):
            command.extend(args)
        else:
            if len(args) > 0:
                command.append(args)
        res = subprocess.run(command, encoding="utf-8", check=check,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = res.stdout
        if self.debug:
            print(res)
        return output, res.returncode


    def test_version(self):
        """test version information output"""
        output, _ = self.run_test('--version')
        self.assertIn('GNU General Public License', output)
        self.assertRegex(output, r'jpeginfo v\d+\.\d+\.\d')

    def test_noarguments(self):
        """test running withouth arguments"""
        output, res = self.run_test('', check=False)
        self.assertEqual(1, res)
        self.assertIn('file arguments missing', output)

    def test_basic(self):
        """test basic output"""
        output, _ = self.run_test('jpeginfo_test1.jpg')
        self.assertRegex(output,
                         r'jpeginfo_test1.jpg\s+2100\s+x\s+1500\s+24bit\s+'
                         r'P\s+Exif,IPTC,XMP,ICC,Adobe,UNKNOWN\s+320159')

if __name__ == '__main__':
    unittest.main()


# eof :-)
