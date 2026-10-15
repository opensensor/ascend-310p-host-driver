# SPDX-License-Identifier: GPL-2.0-only
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("profile_dma", ROOT / "scripts/profile-dma.py")
PROFILE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PROFILE)


def event(**overrides):
    values = dict(device=0, channel=3, nodes=1, direction=1, wait_mode=2, status=0,
                  bytes=320, start_ns=100, submit_ns=120, wait_ns=125,
                  complete_ns=150, end_ns=170, sleep_ns=20, sleeps=1, queries=2)
    values.update(overrides)
    return "worker-7 [001] ... 1.0: ascend_dma_sync: " + " ".join(
        f"{key}={value}" for key, value in values.items())


class ProfileTests(unittest.TestCase):
    def test_bounded_query_polling_and_original_timeout(self):
        source = (ROOT / "pcie_host/devdrv_dma.c").read_text()
        start = source.index("STATIC bool devdrv_dma_chan_spin_query(")
        end = source.index("\nSTATIC int devdrv_dma_chan_sync_wait(", start)
        header = (ROOT / "pcie_host/devdrv_dma_profile.h").read_text()
        header = "\n".join(line for line in header.splitlines() if not line.startswith("#include"))
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            (path / "spin_profile_header.inc").write_text(header)
            (path / "spin_query_functions.inc").write_text(source[start:end])
            subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", f"-I{path}",
                            str(ROOT / "tests/dma_query_spin_test.c"), "-o", str(path / "test")], check=True)
            subprocess.run([str(path / "test")], check=True)

    def test_spin_capture_fields_and_budget_grouping(self):
        result = PROFILE.summarize("\n".join([
            event(),
            event(spin_budget_us=5, spin_queries=4, spin_completed=1, spin_ns=10),
            event(spin_budget_us=10, spin_queries=4, spin_completed=1, spin_ns=10),
            event(spin_budget_us=5, spin_queries=4, spin_completed=1, spin_ns=1000),
        ]))
        self.assertEqual(len(result["groups"]), 3)
        self.assertEqual(result["malformed_or_incomplete"], 1)
        self.assertEqual(result["groups"][1]["metrics"]["spin_completed"]["p50"], 1)

    def test_actual_submission_path_and_failure_samples(self):
        source = (ROOT / "pcie_host/devdrv_dma.c").read_text()
        start = source.index("int devdrv_dma_chan_copy(u32 ")
        end = source.index("\nvoid devdrv_dma_copy_type_info_init", start)
        header = (ROOT / "pcie_host/devdrv_dma_profile.h").read_text()
        header = "\n".join(line for line in header.splitlines() if not line.startswith("#include"))
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            (path / "dma_profile_header.inc").write_text(header)
            (path / "dma_profile_path.inc").write_text(source[start:end])
            subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", f"-I{path}",
                            str(ROOT / "tests/dma_profile_path_test.c"), "-o", str(path / "test")], check=True)
            subprocess.run([str(path / "test")], check=True)

    def test_metrics_and_completion_before_wait(self):
        result = PROFILE.summarize(event() + "\n" + event(complete_ns=122))
        metrics = result["groups"][0]["metrics"]
        self.assertEqual(metrics["total_ns"]["p50"], 70)
        self.assertEqual(metrics["prepare_ns"]["p50"], 20)
        self.assertEqual(metrics["submit_to_observed_complete_ns"]["max"], 30)
        self.assertEqual(metrics["observed_complete_to_return_ns"]["max"], 48)
        self.assertEqual(result["malformed_or_incomplete"], 0)

    def test_failures_do_not_pollute_latency(self):
        result = PROFILE.summarize("\n".join([
            event(), event(status=-28, submit_ns=0, wait_ns=0, complete_ns=0),
            event(status=-110, complete_ns=0), event(complete_ns=0),
            event(sleep_ns=1000), event(end_ns=99),
        ]))
        self.assertEqual(result["failure_status_counts"], {"-28": 1, "-110": 1})
        self.assertEqual(result["malformed_or_incomplete"], 3)
        self.assertEqual(result["groups"][0]["samples"], 1)

    def test_grouping_modes_devices_sizes_and_lossless_parse(self):
        result = PROFILE.summarize("\n".join([
            event(), event(device=1), event(bytes=5120), event(wait_mode=1),
            event(channel=4), event() + " bytes=900", event().replace(" queries=2", ""),
            "unrelated kernel event",
        ]))
        self.assertEqual(len(result["groups"]), 5)
        self.assertEqual(result["malformed_or_incomplete"], 2)
        self.assertEqual(PROFILE.summarize("")["groups"], [])

    def test_profile_helpers_no_clock_calls_when_disabled(self):
        # Compile the actual helper body with a deterministic clock. Exercise
        # reset between requests, completion publication and cumulative sleeps.
        header = (ROOT / "pcie_host/devdrv_dma_profile.h").read_text()
        header = "\n".join(line for line in header.splitlines() if not line.startswith("#include"))
        source = '''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
typedef uint64_t u64;
typedef uint32_t u32;
static u64 now;
static unsigned calls;
static u64 ktime_get_ns(void) { calls++; return now; }
''' + header + '''
int main(void) {
    struct devdrv_dma_profile p;
    u64 start;
    devdrv_dma_profile_complete(NULL);
    start = devdrv_dma_profile_sleep_begin(NULL);
    devdrv_dma_profile_sleep_end(NULL, start);
    assert(calls == 0);
    memset(&p, 0xff, sizeof(p));
    now = 10;
    devdrv_dma_profile_begin(&p);
    assert(p.start_ns == 10 && p.complete_ns == 0 && p.sleeps == 0);
    now = 20; start = devdrv_dma_profile_sleep_begin(&p);
    now = 50; devdrv_dma_profile_sleep_end(&p, start);
    now = 60; start = devdrv_dma_profile_sleep_begin(&p);
    now = 70; devdrv_dma_profile_sleep_end(&p, start);
    now = 80; devdrv_dma_profile_complete(&p);
    assert(p.sleeps == 2 && p.sleep_ns == 40 && p.complete_ns == 80);
    now = 90; devdrv_dma_profile_begin(&p);
    assert(p.sleeps == 0 && p.sleep_ns == 0 && p.complete_ns == 0);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            (path / "test.c").write_text(source)
            subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                            str(path / "test.c"), "-o", str(path / "test")], check=True)
            subprocess.run([str(path / "test")], check=True)


if __name__ == "__main__":
    unittest.main()
