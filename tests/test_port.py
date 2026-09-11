# SPDX-License-Identifier: GPL-2.0-only
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class PortTests(unittest.TestCase):
    def compile_and_run(self, name, *includes):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / name
            subprocess.run(
                [
                    "gcc",
                    "-std=c11",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    *(f"-I{ROOT / include}" for include in includes),
                    str(ROOT / "tests" / f"{name}.c"),
                    "-o",
                    str(binary),
                ],
                check=True,
            )
            subprocess.run([str(binary)], check=True)

    def test_pfn_lookup_failure_and_lock_pairing(self):
        self.compile_and_run("pfn_lookup_test", "tests/stubs", "kernel_adapt/include")

    def test_crc16_known_vectors(self):
        self.compile_and_run("crc16_test", "kernel_adapt/base")

    def test_dkms_profiles_have_aligned_arrays(self):
        for profile, virtual, expected in (
            ("normal", "n", 31),
            ("normal", "y", 32),
            ("vnpu_guest", "n", 14),
        ):
            with self.subTest(profile=profile, virtual=virtual):
                subprocess.run(
                    [
                        "bash",
                        "-c",
                        'Driver_Install_Mode=$2; BUILD_VASCEND=$3; source "$1"; '
                        'test "${#BUILT_MODULE_NAME[@]}" = "$4" && '
                        'test "${#BUILT_MODULE_LOCATION[@]}" = "$4" && '
                        'test "${#DEST_MODULE_LOCATION[@]}" = "$4"',
                        "dkms-test",
                        str(ROOT / "dkms.conf"),
                        profile,
                        virtual,
                        str(expected),
                    ],
                    check=True,
                )


if __name__ == "__main__":
    unittest.main()
