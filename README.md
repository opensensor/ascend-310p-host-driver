# Ascend 310P host driver: Linux 7 port

OpenSensor source fork of the host kernel tree shipped in Huawei's
`Ascend-hdk-310p-npu-driver_26.0.rc1_linux-x86-64.run`.

**Both profiles build on Ubuntu's x86-64 `7.0.0-31-generic` kernel:**
31 bare-metal modules by default, or all 32 modules with mediated vNPU
virtualization enabled. The virtualization port requires driver changes only;
no kernel patch or KVM-private export is used. This is a compile-validated port;
it has not been installed, loaded, or tested with an Ascend card.

## Build and validate

Requirements: matching kernel headers (including `Module.symvers`), GNU Make,
GCC 15, binutils, kmod (`modinfo`, `modprobe`), Bash, and Python 3.
The tested compiler is Ubuntu's `x86_64-linux-gnu-gcc` 15.2.0-16ubuntu1.

```bash
./scripts/build-host.sh
# Or explicitly select the installed target headers:
./scripts/build-host.sh 7.0.0-31-generic bare-metal
# Include drv_vascend for mediated vNPU virtualization:
./scripts/build-host.sh 7.0.0-31-generic virtualization
```

The script cleans previous build products, builds the selected profile,
runs the CPU-only regression tests, and verifies every module's architecture,
kernel release, undefined symbols, symbol-version CRCs, and namespace imports.
It does not install software, register DKMS, or load modules. Build logs go to
`build-logs/`; the JSON validation manifest and a compressed module bundle go to
`build-artifacts/<kernel-release>/<profile>/`. The verifier also uses matching
installed VFIO/KVM dependency modules for its isolated depmod check.

For incremental development:

```bash
make all CC=x86_64-linux-gnu-gcc
python3 -m unittest discover -s tests -v
python3 scripts/verify-modules.py "$(uname -r)"
```

The defaults are `TARGET_PRODUCT=mini`, `Driver_Install_Mode=normal`, and
`BUILD_VASCEND=n`. The `Makefile` and `dkms.conf` symlinks select the packaged
310P product configuration. DKMS's normal-mode module list matches the
31-module build; DKMS installation itself has not been tested.

For incremental virtualization builds use `make all BUILD_VASCEND=y
CC=x86_64-linux-gnu-gcc`, then run the verifier with `--profile virtualization`.
The vendor guest-mode configuration remains in the source but is unvalidated.

## Changes

- Replace obsolete Kbuild `EXTRA_CFLAGS` settings with `ccflags-y`, preserving
  include paths, product defines, compiler options, and original line endings.
- Make unresolved module symbols fatal and read stack-protector configuration
  from the selected target kernel.
- Adapt removed timer names, hrtimer initialization, namespace declarations,
  unmapped-address lookup, and page-table helpers.
- Replace `follow_pfn` with paired `follow_pfnmap_start`/`follow_pfnmap_end`
  calls while preserving its lookup-only contract.
- Propagate const qualification for file paths and the PCI bus; initialize the
  vendor's existing private file snapshot with a byte copy because `struct file`
  now contains a const path member.
- Preserve the exported CRC-table accessor using a local immutable CRC-16/IBM
  table after the kernel made its table private.
- Use `GPIOF_IN` and supply missing module descriptions.
- Replace virtualization's removed PFN translation APIs with balanced VFIO
  pins, and its private visibility call with header-defined KVM slot lookup
  under SRCU. Update IOMMU domain allocation and error cleanup for Linux 7.
- Give each modern VFIO device its own guest DMA pin cache. Serialize cache
  lifetime with callbacks, unwind failed opens, and invalidate every intersecting
  cached RAM slot before returning from a VFIO unmap callback.

No userspace ioctl definitions, HAL libraries, device firmware, or device OS
files were changed. The original userspace ABI is an implementation constraint;
successful compilation does not establish HAL interoperability.

## Validation and limits

The clean Linux `7.0.0-31-generic` build passes strict modpost with zero
unresolved-symbol or namespace errors in both profiles. All 32 modules in the
virtualization profile have matching vermagic,
and all strong undefined symbols and recorded CRCs resolve against either the
target kernel's exports or this build's exports. An isolated `depmod -n -e -E`
check also found no missing symbols or version disagreements.

Seven CPU regression tests cover PFN lookup and lock pairing, CRC known vectors,
DKMS module arrays, VFIO full/partial/error pin ownership, KVM slot visibility,
IOMMU allocation and group-reference cleanup, failed guest opens, device-specific
pin ownership, and repeated/partial/multi-slot/full-space invalidations. The
lifecycle tests compile the actual driver functions with mocked kernel operations.
They do not exercise real kernel concurrency or hardware.

Invalidation conservatively retires an entire intersecting cached RAM slot.
Live remapping, ballooning, and memory hotplug are not validated; reopening the
vNPU is required to rebuild retired slots. Per-device caches also pin guest RAM
separately for each vNPU, which can increase locked-memory accounting. Neither
VFIO type1 nor IOMMUFD has been tested with an actual guest.

Existing vendor compiler warnings remain (missing prototypes, fallthrough, a
debug callback cast, and a debug log format mismatch). Module BTF generation is
skipped because a matching `vmlinux` and pahole are unavailable. The modules are
unsigned. No compatibility claim is made for other kernel releases or hardware.

Before deployment, hardware validation must cover PCI enumeration, firmware
handshake, HAL open/close and allocation, DMA, memory teardown, interrupts,
reset, and unload. The vendor's private `struct file` snapshot and its manual
page-table walker retain existing lifetime/concurrency assumptions that a
compile-only test cannot verify. See [PORTING.md](PORTING.md).

## Source provenance

The pristine import is commit `cdeb44897e1fb0656491cef9db71368e09e29414`.
It contains 671 regular C files, 898 regular headers, and 64 internal symlinks
from the audited kernel tree. The original extraction remains untouched at:

```text
/home/matteius/re-framework/artifacts/ascend-310p-driver-20260911/
```

Package SHA-256:
`884777a75f23a4ee7b63412fc5c12094d45545ed4ce5cb3d58c4a538dbb52fdc`.

The package was obtained from the
[official Huawei download](https://ascend-repo.obs.cn-east-2.myhuaweicloud.com/Ascend%20HDK/Ascend%20HDK%2026.0.RC1/Ascend-hdk-310p-npu-driver_26.0.rc1_linux-x86-64.run).
Individual vendor file notices are retained. Most C sources carry GPLv2 notices;
some build files carry CANN license text. This fork does not assign a blanket
license to vendor material. Newly added port helpers and tests use
`GPL-2.0-only`. The source repository is
[opensensor/ascend-310p-host-driver](https://github.com/opensensor/ascend-310p-host-driver).
