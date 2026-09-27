# -*- coding: utf-8 -*-
"""Chunker unit tests."""

import sys
import unittest

sys.path.insert(0, r"../office-extension/extension/python")

from v271.chunker import chunk_documents, chunk_text  # noqa: E402


class ChunkerTests(unittest.TestCase):

    def test_short_text_single_chunk(self):
        chunks = chunk_text("hello world", "p1", max_chars=2000)
        self.assertEqual(chunks, [("p1", "hello world")])

    def test_long_text_split(self):
        text = " ".join(["word%04d" % i for i in range(600)])
        chunks = chunk_text(text, "p1", max_chars=500)
        self.assertGreater(len(chunks), 1)
        for provenance, chunk in chunks:
            self.assertEqual(provenance, "p1")
            self.assertLessEqual(len(chunk), 500)

    def test_join_preserves_content(self):
        text = " ".join(["word%04d" % i for i in range(300)])
        chunks = chunk_text(text, "p1", max_chars=1000)
        joined = " ".join(chunk for _, chunk in chunks)
        self.assertIn("word0000", joined)
        self.assertIn("word0299", joined)

    def test_empty_input(self):
        self.assertEqual(chunk_text("", "p"), [])
        self.assertEqual(chunk_text("   ", "p"), [])
        self.assertEqual(chunk_documents([("p", ""), ("p", None)]), [])

    def test_provenance_preserved_in_documents(self):
        chunks = chunk_documents([("slide:1", "alpha beta gamma"),
                                  ("slide:2", "delta")], max_chars=2000)
        provenances = {p for p, _ in chunks}
        self.assertEqual(provenances, {"slide:1", "slide:2"})

    def test_min_size_guard(self):
        with self.assertRaises(ValueError):
            chunk_text("x" * 100, "p", max_chars=50)

    def test_unicode(self):
        chunks = chunk_text("příliš žluťoučký kůň úpěl ďábelské ódy", "p")
        self.assertEqual(len(chunks), 1)
        self.assertIn("žluťoučký", chunks[0][1])


if __name__ == "__main__":
    unittest.main()