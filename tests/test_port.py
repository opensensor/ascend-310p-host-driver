# SPDX-License-Identifier: GPL-2.0-only
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class PortTests(unittest.TestCase):
    def compile_and_run(self, name, *includes, extracted_functions=()):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / name
            if extracted_functions:
                fragments = []
                for filename, function in extracted_functions:
                    source = (ROOT / filename).read_text()
                    start = source.index(function + "(")
                    start = source.rfind("\n", 0, start) + 1
                    body = source.index("{", start)
                    depth = 1
                    end = body + 1
                    while depth:
                        depth += (source[end] == "{") - (source[end] == "}")
                        end += 1
                    fragments.append(source[start:end])
                (Path(temporary) / f"{name}.inc").write_text(
                    "\n\n".join(fragments) + "\n"
                )
            subprocess.run(
                [
                    "gcc",
                    "-std=c11",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    f"-I{temporary}",
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

    def test_vfio_pin_ownership_and_invalidation_boundaries(self):
        self.compile_and_run("guest_memory_test", "tests/stubs", "vascend_drv")

    def test_guest_visibility_and_srcu_pairing(self):
        self.compile_and_run("guest_slots_test", "tests/stubs", "vascend_drv")

    def test_iommu_allocation_errors_and_group_references(self):
        self.compile_and_run(
            "iommu_lifecycle_test",
            "tests/stubs",
            extracted_functions=(
                ("vascend_drv/dma_pool_pt.c", "hw_vdavinci_iommu_attach_group"),
                ("vascend_drv/dma_pool_pt.c", "hw_vdavinci_iommu_detach_group"),
            ),
        )

    def test_vfio_domain_lifecycle_and_multislot_unmap(self):
        functions = (
            ("domain_manage.c", "vm_dom_info_find"),
            ("domain_manage.c", "vm_dom_info_new"),
            ("domain_manage.c", "vm_dom_info_get"),
            ("domain_manage.c", "vm_dom_info_release"),
            ("dma_pool.c", "hw_vdavinci_unpin_pages"),
            ("kvmdt.c", "hw_vdavinci_release_vm_domain"),
            ("dma_pool.c", "hw_vdavinci_unplug_ram"),
            ("vfio_ops.c", "hw_vdavinci_vfio_open"),
            ("vfio_ops.c", "hw_vdavinci_vfio_dma_unmap"),
        )
        self.compile_and_run(
            "virtualization_lifecycle_test",
            "tests/stubs",
            "vascend_drv",
            extracted_functions=tuple(
                (f"vascend_drv/{filename}", function)
                for filename, function in functions
            ),
        )

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
