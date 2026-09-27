# -*- coding: utf-8 -*-
"""PKCE unit tests (RFC 7636 Appendix B vector included)."""

import sys
import unittest

sys.path.insert(0, r"../office-extension/extension/python")

from v271 import pkce  # noqa: E402


class PkceTests(unittest.TestCase):

    def test_verifier_length_and_alphabet(self):
        for length in (43, 64, 128):
            verifier = pkce.generate_verifier(length)
            self.assertEqual(len(verifier), length)
            allowed = set("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~")
            self.assertTrue(set(verifier) <= allowed)

    def test_verifier_length_bounds(self):
        with self.assertRaises(ValueError):
            pkce.generate_verifier(42)
        with self.assertRaises(ValueError):
            pkce.generate_verifier(129)

    def test_s256_deterministic(self):
        verifier = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"
        expected = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"
        self.assertEqual(pkce.derive_challenge(verifier, "S256"), expected)

    def test_plain_method(self):
        verifier = pkce.generate_verifier()
        self.assertEqual(pkce.derive_challenge(verifier, "plain"), verifier)

    def test_unsupported_method(self):
        with self.assertRaises(ValueError):
            pkce.derive_challenge(pkce.generate_verifier(), "S512")

    def test_state_roundtrip(self):
        state = pkce.generate_state()
        self.assertTrue(len(state) >= 20)
        self.assertNotEqual(state, pkce.generate_state())


if __name__ == "__main__":
    unittest.main()