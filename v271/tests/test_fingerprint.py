# -*- coding: utf-8 -*-
"""Fingerprint unit tests."""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, r"../office-extension/extension/python")

from v271 import fingerprint  # noqa: E402


class FingerprintTests(unittest.TestCase):

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def _write(self, name, content):
        path = os.path.join(self.tmp.name, name)
        with open(path, "wb") as handle:
            handle.write(content)
        return path

    def test_file_hash_stable(self):
        path = self._write("a.txt", b"hello v271")
        first = fingerprint.fingerprint_file(path)
        second = fingerprint.fingerprint_file(path)
        self.assertEqual(first, second)
        self.assertTrue(first.startswith("sha256:"))

    def test_missing_file(self):
        self.assertIsNone(fingerprint.fingerprint_file(
            os.path.join(self.tmp.name, "nope.txt")))

    def test_size_guard(self):
        path = self._write("big.bin", b"x" * 1000)
        self.assertIsNone(fingerprint.fingerprint_file(path, max_bytes=100))

    def test_text_fingerprint(self):
        first = fingerprint.fingerprint_text("hello")
        second = fingerprint.fingerprint_text("hello")
        self.assertEqual(first, second)
        self.assertNotEqual(first, fingerprint.fingerprint_text("hello!"))


if __name__ == "__main__":
    unittest.main()