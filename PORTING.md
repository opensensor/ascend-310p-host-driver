# Linux 7 port notes

## Scope and source baseline

Target: x86-64, Ubuntu `7.0.0-31-generic`, GCC 15.2.0, Ascend 310P 26.0.rc1
host kernel sources, normal host operation with optional mediated vNPU support.

The default profile keeps the 31 non-`drv_vascend` modules from the vendor's
normal-mode DKMS manifest. The opt-in virtualization profile builds all 32. This includes `drv_vascend_stub`, the host virtualization
interface stub already shipped by the vendor, plus the host virtual-management
and VPC infrastructure referenced by the physical-device stack. A stub replacing
the excluded `drv_vascend` implementation was not introduced.

The vLLM Ascend repository is a separate project and was left unchanged.

## Kernel API evidence

The installed headers and `/lib/modules/7.0.0-31-generic/build/Module.symvers`
are the authority for the build and exported symbol versions.

| Area | Port treatment |
| --- | --- |
| Kbuild | Modern `scripts/Makefile.lib` uses `ccflags-y`; all legacy module-local flags migrated mechanically. |
| Timers | A small forced-include header supplies the removed legacy names for Linux 6.15 and newer. It includes only `linux/version.h`, avoiding early inclusion of printk before a source file defines `pr_fmt`. |
| Hrtimers | Linux 7 requires the callback in `hrtimer_setup`; passing `NULL` and assigning `.function` later leaves the callback unregistered. Host call sites now pass their callback directly. |
| PFN lookup | `follow_pfnmap_start` acquires page-table locks; the wrapper copies the PFN before `follow_pfnmap_end` and releases nothing on a failed start. The caller must still hold the mmap lock and protect the mapping lifetime. |
| Unmapped area | Use `mm_get_unmapped_area`; the mm argument was removed in Linux 6.19. Calling the generic file-dispatching `get_unmapped_area` here could recurse into the same file operation. |
| PMD entries | On Linux 7, use `pmd_is_migration_entry` and `pmd_leaf` in the vendor walker. The three separately packaged copies receive the same change. |
| File constness | Return a const path pointer. Preserve the vendor's private file snapshot initialization without modifying the live VFS file's path. |
| CRC table | Keep the adapter's exported pointer-returning API and use a const, generated table for reflected polynomial `0xa001`. Known CRC-16/ARC and MODBUS vectors test the table. |

Primary references:

- [Linux timer API documentation](https://www.kernel.org/doc/html/v6.15/driver-api/basics.html).
- [Linux 6.13 PFN lookup declarations](https://github.com/torvalds/linux/blob/v6.13/include/linux/mm.h).
- [Linux 7 PFN lookup implementation and locking contract](https://github.com/torvalds/linux/blob/v7.0/mm/memory.c).
- [VFS change making `f_path` const](https://kernel.googlesource.com/pub/scm/linux/kernel/git/viro/vfs/+/2f7d98f10b8f64525b2c74cae7d70ae5278eb654).
- [Linux 6.19 unmapped-area API](https://github.com/torvalds/linux/blob/v6.19/include/linux/sched/mm.h).
- [Linux symbol namespaces](https://docs.kernel.org/6.14/core-api/symbol-namespaces.html).

## Driver-only virtualization port

`BUILD_VASCEND=y` now builds `drv_vascend` on the unmodified target kernel.
The previous guard against its KVM-private import has been removed because the
active dependency has been replaced. The module's only undefined KVM functions
are `kvm_get_kvm` and `kvm_put_kvm`, both permitted target-kernel exports.

| Previous dependency | Active Linux 7 implementation |
| --- | --- |
| `gfn_to_pfn` / `kvm_release_pfn_clean` | VFIO pin/unpin through the owning emulated `vfio_device`; copy the PFN before releasing the temporary pin. |
| `kvm_is_visible_gfn` | Header-defined `__gfn_to_memslot(kvm_memslots(kvm), gfn)` and `kvm_is_visible_memslot`, with SRCU held across lookup and inspection. |
| `iommu_domain_alloc(&pci_bus_type)` | `iommu_paging_domain_alloc(dev)`, with `ERR_PTR` handling and balanced IOMMU group references on success and failure. |

The visibility helper preserves valid user-slot semantics, including read-only
visibility. Visibility does not authorize DMA: the DMA pool skips read-only and
invisible slots, and VFIO checks mappings and permissions when pages are pinned.
Header-defined KVM access still depends on the target kernel's internal layouts;
this is not a stable cross-kernel ABI.

The vendor's modern path already uses `vfio_dma_rw` for guest CPU reads/writes
and provides the emulated-device IOMMUFD callbacks. Its `kvm_read_guest` and
`kvm_write_guest` calls are in older-kernel branches. No kernel-private namespace
import, export shim, kernel patch, HAL change, or firmware change is needed to
compile the new profile.

### Pin ownership and invalidation

A successful pin helper returns exactly the requested count. Positive partial
results are unpinned and converted to an error, negative errors retain no pins,
and address/count validation prevents wrapping the requested IOVA. The
`gfn_to_mfn` callback retains its vendor lookup-only ABI: its returned PFN has no
pin. Actual DMA uses the independently pinned pool. Temporary lookup pins also
hold the cache mutex so an unmap callback waits for them to drain.

Modern VM DMA domains are keyed by both KVM and VFIO device. A second vNPU in the
same VM must obtain its own pins: IOMMUFD access pins cannot be borrowed from
another device and released through the wrong owner. The legacy pre-6.0 path
retains its old shared domain key. Multiple modern vNPUs can therefore increase
locked-memory accounting compared with a shared cache.

The cache mutex now spans device creation through removal. VFIO unmap callbacks
may run before guest open or after close, when no DMA domain exists. Domain
publication and detachment use that mutex; failed opens release the domain.
Last-reference teardown removes the domain from the lookup list before unpinning,
preventing a concurrent open from finding an expired reference. The KVM reference
is retained until DMA cleanup finishes.

Unmap iterates every cached RAM slot intersecting the invalidation, including
partial, multi-slot and full-address-space ranges. DMA mappings are removed
before their pins are released. Range checks handle sub-page offsets and avoid
end-address overflow. A partial invalidation retires the whole intersecting
cached slot; the driver does not rebuild that slot automatically. Reopen the
vNPU to rebuild it. Guest ballooning, live memory remapping, memory hotplug,
migration, DMA quiescence, and reset behavior require hardware validation.

Primary references for these contracts:

- [Linux 7 VFIO pin/unpin and CPU DMA implementation](https://github.com/torvalds/linux/blob/v7.0/drivers/vfio/vfio_main.c).
- [Linux 7 VFIO callback lifetime](https://github.com/torvalds/linux/blob/v7.0/include/linux/vfio.h).
- [Linux 7 KVM slot lookup and visibility helpers](https://github.com/torvalds/linux/blob/v7.0/include/linux/kvm_host.h).
- [Linux 7 IOMMU domain allocation](https://github.com/torvalds/linux/blob/v7.0/drivers/iommu/iommu.c).
- [Linux 7 IOMMUFD emulated-device access](https://github.com/torvalds/linux/blob/v7.0/drivers/vfio/iommufd.c).

## Reproduction and evidence

`scripts/build-host.sh [kernel-release] [bare-metal|virtualization]` performs a
clean build, runs seven CPU regression tests, and calls `scripts/verify-modules.py`.
The verifier requires the selected DKMS manifest to match `modules.order`
exactly, then checks all 31 or 32 actual ELF files, vermagic, strong symbol
references, every modversion CRC, and namespace imports. It separately checks
that dynamically acquired `vfio_dma_rw` is exported without a namespace
restriction. An isolated `depmod -n -e -E` check includes the installed kernel
provider modules and their dependencies; only temporary copies are used. Its
JSON report contains module hashes and dependencies, and it packages the
validated driver modules without installing them.

Four added virtualization tests cover:

- Pin ownership on success, partial results and errors; IOVA overflow and
  invalidation boundaries.
- Visible/invisible/invalid/internal/read-only KVM slots and SRCU pairing.
- Actual IOMMU attach/detach functions under allocation, group, attachment and
  MSI-cookie failures, including repeated cleanup.
- Actual domain lookup/open/release and VFIO unmap functions with mocked kernel
  operations: allocation and open failures, device-specific pin owners, legacy
  shared references, callbacks without an open domain, multi-slot invalidation,
  unmap-before-unpin ordering, and repeated teardown.

The source-function harness tests control flow and resource accounting. Its
mock mutexes and reference counts do not prove real concurrent execution safe.

Initial failure logs and subsequent build logs remain locally in `build-logs/`.
They cover the pristine build, Kbuild flag migration, API adaptation, and the
first successful strict modpost. Clean-build logs are
`build-logs/build-7.0.0-31-generic-<profile>.log`; module metadata is in
`build-artifacts/7.0.0-31-generic/<profile>/validation.json`.
The original bare-metal artifacts remain in the parent directory as an earlier
checkpoint.

Neither unit tests nor symbol checks establish that a module can initialize a
card correctly. Runtime validation is still required for the PFN lifetime
assumptions, the existing manual page-table walk, the private file snapshot,
HAL interaction, DMA, resets, and device teardown. No card, host driver, HAL,
firmware, or system module configuration was installed or modified in this work.
