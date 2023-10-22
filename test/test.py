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
        command = [self.program] + args
        res = subprocess.run(command, encoding="utf-8", check=check,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = res.stdout
        if self.debug:
            print(res)
        return output, res.returncode


    def test_version(self):
        """test version information output"""
        output, _ = self.run_test(['--version'])
        self.assertIn('GNU General Public License', output)
        self.assertRegex(output, r'jpeginfo v\d+\.\d+\.\d')

    def test_noarguments(self):
        """test running withouth arguments"""
        output, res = self.run_test([], check=False)
        self.assertEqual(1, res)
        self.assertIn('file arguments missing', output)

    def test_basic(self):
        """test basic output"""
        output, _ = self.run_test(['jpeginfo_test1.jpg'])
        self.assertRegex(output,
                         r'jpeginfo_test1.jpg\s+2100\s+x\s+1500\s+24bit\s+'
                         r'P\s+Exif,IPTC,XMP,ICC,Adobe,UNKNOWN\s+320159')

    def test_check_mode(self):
        """test checking image integrity"""
        output, res = self.run_test(['-c','jpeginfo_test2.jpg'], check=False)
        self.assertEqual(0, res)
        output, res = self.run_test(['-c','jpeginfo_test2_broken.jpg'], check=False)
        self.assertIn('WARNING Premature end of JPEG file', output)
        self.assertNotEqual(0, res)

    def test_progressive(self):
        """test progressive mode detection"""
        output, _ = self.run_test(['jpeginfo_test1.jpg'])
        self.assertRegex(output, r'bit\s+P\s+')

    def test_nonprogressive(self):
        """test non-progressive mode detection"""
        output, _ = self.run_test(['jpeginfo_test2.jpg'])
        self.assertRegex(output, r'bit\s+N\s')

    def test_grayscale(self):
        """test grayscale image detection"""
        output, _ = self.run_test(['jpeginfo_test3.jpg'])
        self.assertRegex(output, r'\s8bit\s')

    def test_md5(self):
        """test image MD5 checksum"""
        output, _ = self.run_test(['--md5', 'jpeginfo_test1.jpg'])
        self.assertIn('536c217b027d44cc2e4a0ad8e6e531fe', output)

    def test_sha256(self):
        """test image SHA2-256 checksum"""
        output, _ = self.run_test(['--sha256', 'jpeginfo_test1.jpg'])
        self.assertIn('9a36209da080e187a2f749ec4ed0db3e73bcebc689ca060d929bdc7d1384edac', output)

    def test_sha512(self):
        """test image SHA2-512 checksum"""
        output, _ = self.run_test(['--sha512', 'jpeginfo_test1.jpg'])
        self.assertIn('4d5dc047caf3cbd84eec91dce3c14e938d8d3f730b152eefb72c8c3ebfa65e91'
                      '46bd304b15e009df6f74e7397435a10f375c5453a60e32c9aca738051c36e211', output)

    def test_comments(self):
        """test image comments"""
        output, _ = self.run_test(['-C', 'jpeginfo_test2.jpg'])
        self.assertIn('This is a test comment.', output)


if __name__ == '__main__':
    unittest.main()

# eof :-)
