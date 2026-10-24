"""Unit tests for the OXT vendor sync platform filter (scripts/build_oxt.py).

The committed vendor/ tree holds pure-Python packages only; platform wheels
(cryptography's .pyd/.so bindings, console .exe wrappers) are added per host
by officelabs-build/vendor_native.sh. These tests pin the safety net that
keeps FOREIGN-platform binaries out of plugin/lib/ and the OXT bundle.
"""

import os

import pytest

from scripts import build_oxt


@pytest.fixture(autouse=True)
def _fake_platform(monkeypatch):
    """Run every case as if on a Linux host (.so native, .pyd foreign).

    The helper is monkeypatched so the tests are deterministic on any
    developer machine; the real platform table is exercised implicitly
    through _vendor_all_native_exts().
    """
    monkeypatch.setattr(build_oxt, "_vendor_platform_exts", lambda: frozenset({".so"}))
    yield


def _write(root: str, rel: str, content: str = "x") -> None:
    path = os.path.join(root, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(content)


def test_platform_exts_known():
    assert build_oxt._vendor_all_native_exts() >= {".pyd", ".exe", ".dll", ".so", ".dylib"}
    assert build_oxt._vendor_platform_exts() == frozenset({".so"})


def test_dir_native_state_pure_python(tmp_path):
    _write(tmp_path, "isodate/__init__.py")
    _write(tmp_path, "isodate/duration.py")
    assert build_oxt._vendor_dir_native_state(str(tmp_path)) == (False, False)


def test_dir_native_state_native_wheel(tmp_path):
    _write(tmp_path, "cryptography/__init__.py")
    _write(tmp_path, "cryptography/hazmat/bindings/_rust.so")
    assert build_oxt._vendor_dir_native_state(str(tmp_path)) == (True, False)


def test_dir_native_state_foreign_wheel(tmp_path):
    _write(tmp_path, "cryptography/__init__.py")
    _write(tmp_path, "cryptography/hazmat/bindings/_rust.pyd")
    assert build_oxt._vendor_dir_native_state(str(tmp_path)) == (False, True)


def test_dir_native_state_mixed(tmp_path):
    _write(tmp_path, "pkg/_rust.so")
    _write(tmp_path, "pkg/_legacy.pyd")
    assert build_oxt._vendor_dir_native_state(str(tmp_path)) == (True, True)


def test_sync_skips_foreign_dir_and_file(tmp_path, capsys):
    vendor = tmp_path / "vendor"
    lib = tmp_path / "lib"
    # Pure-Python package: must be copied.
    _write(vendor, "isodate/__init__.py")
    # Foreign platform wheel (Windows .pyd): must be skipped entirely.
    _write(vendor, "cryptography/__init__.py")
    _write(vendor, "cryptography/hazmat/bindings/_rust.pyd")
    # Standalone foreign binary (top-level .pyd, e.g. a cffi backend): skipped.
    _write(vendor, "cffi_backend.cp313-win_amd64.pyd")
    # Foreign console-script dir (vendor/bin/*.exe): skipped.
    _write(vendor, "bin/json_repair.exe")

    count = build_oxt.sync_vendor_into_lib(str(vendor), str(lib))

    assert count == 1  # only isodate
    assert (lib / "isodate" / "__init__.py").exists()
    assert not (lib / "cryptography").exists()
    assert not (lib / "cffi_backend.cp313-win_amd64.pyd").exists()
    assert not (lib / "bin").exists()
    out = capsys.readouterr().out
    assert "cryptography" in out and "cffi_backend" in out and "bin" in out


def test_sync_keeps_native_wheel(tmp_path):
    vendor = tmp_path / "vendor"
    lib = tmp_path / "lib"
    _write(vendor, "cryptography/__init__.py")
    _write(vendor, "cryptography/hazmat/bindings/_rust.so")
    _write(vendor, "websockets/__init__.py")  # pure python

    count = build_oxt.sync_vendor_into_lib(str(vendor), str(lib), prune_websockets=False)

    assert count == 2
    assert (lib / "cryptography" / "hazmat" / "bindings" / "_rust.so").exists()
    assert (lib / "websockets" / "__init__.py").exists()


def test_sync_missing_vendor_dir(tmp_path):
    assert build_oxt.sync_vendor_into_lib(str(tmp_path / "nope"), str(tmp_path / "lib")) == 0