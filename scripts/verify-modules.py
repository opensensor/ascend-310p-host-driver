#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Validate the bare-metal modules without installing or loading anything."""

import argparse
import hashlib
import json
import shutil
import tarfile
import tempfile
import struct
import subprocess
from pathlib import Path


def command(*args):
    return subprocess.check_output(args, text=True).strip()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kernel_release")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    kernel = Path("/lib/modules") / args.kernel_release / "build"
    manifest = command(
        "bash",
        "-c",
        'Driver_Install_Mode=normal; BUILD_VASCEND=n; source "$1"; '
        'for i in "${!BUILT_MODULE_NAME[@]}"; do '
        'printf "%s/%s.ko\\n" "${BUILT_MODULE_LOCATION[$i]}" "${BUILT_MODULE_NAME[$i]}"; done',
        "manifest",
        str(root / "dkms.conf"),
    ).splitlines()
    built = {
        str(Path(line).with_suffix(".ko"))
        for line in (root / "modules.order").read_text().splitlines()
    }
    require(
        built == set(manifest), f"Module inventory mismatch: {built ^ set(manifest)}"
    )
    require(len(built) == 31, f"Expected 31 bare-metal modules, found {len(built)}")

    exports = {}
    for source in (kernel / "Module.symvers", root / "Module.symvers"):
        for line in source.read_text().splitlines():
            crc, name, provider, kind, namespace = line.split("\t")
            require(name not in exports, f"Duplicate export: {name}")
            exports[name] = (int(crc, 16), provider, kind, namespace)

    modules = []
    for relative in sorted(built):
        module = root / relative
        data = module.read_bytes()
        require(data[:6] == b"\x7fELF\x02\x01", f"Not ELF64 little-endian: {relative}")
        require(struct.unpack_from("<H", data, 18)[0] == 62, f"Not x86-64: {relative}")
        vermagic = command("modinfo", "-F", "vermagic", str(module))
        require(vermagic.split()[0] == args.kernel_release, (relative, vermagic))
        namespaces = set(
            command("modinfo", "-F", "import_ns", str(module)).splitlines()
        )
        imports = {}
        for line in command("modprobe", "--show-modversions", str(module)).splitlines():
            crc, name = line.split()
            imports[name] = int(crc, 16)
        for line in command("nm", "--undefined-only", str(module)).splitlines():
            kind, name = line.split()
            if kind != "U":  # An undefined weak reference may legally resolve to NULL.
                continue
            require(name in exports, f"{relative}: unresolved symbol {name}")
            require(name in imports, f"{relative}: no version for {name}")
        for name, crc in imports.items():
            require(name in exports, f"{relative}: unexported versioned symbol {name}")
            expected_crc, provider, kind, namespace = exports[name]
            require(crc == expected_crc, f"{relative}: CRC mismatch for {name}")
            require(
                not namespace.startswith("module:"),
                f"{relative}: private KVM symbol {name}",
            )
            require(
                not namespace or namespace in namespaces,
                f"{relative}: missing namespace {namespace}",
            )
        modules.append(
            {
                "path": relative,
                "sha256": hashlib.sha256(data).hexdigest(),
                "vermagic": vermagic,
                "versioned_symbols": len(imports),
                "depends": command("modinfo", "-F", "depends", str(module)),
            }
        )
    # depmod reads a temporary module tree; -n prevents database writes.
    with tempfile.TemporaryDirectory(prefix="ascend-module-check-") as temporary:
        module_dir = Path(temporary) / "lib/modules" / args.kernel_release
        update_dir = module_dir / "updates/ascend"
        update_dir.mkdir(parents=True)
        for relative in sorted(built):
            shutil.copyfile(root / relative, update_dir / Path(relative).name)
        (module_dir / "modules.order").write_text(
            "".join(f"updates/ascend/{Path(p).name}\n" for p in sorted(built))
        )
        (module_dir / "modules.builtin").touch()
        (module_dir / "modules.builtin.modinfo").touch()
        result = subprocess.run(
            [
                "depmod",
                "-n",
                "-e",
                "-E",
                str(kernel / "Module.symvers"),
                "-b",
                temporary,
                args.kernel_release,
            ],
            capture_output=True,
            text=True,
            check=True,
        )
        require(not result.stderr, f"depmod diagnostics: {result.stderr}")

    output = root / "build-artifacts" / args.kernel_release / "validation.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            {
                "kernel_release": args.kernel_release,
                "profile": "bare-metal",
                "excluded_module": "drv_vascend",
                "module_count": len(modules),
                "modules": modules,
            },
            indent=2,
        )
        + "\n"
    )
    bundle = output.parent / f"ascend-310p-bare-metal-{args.kernel_release}.tar.gz"
    with tarfile.open(bundle, "w:gz") as archive:
        for relative in sorted(built):
            archive.add(root / relative, arcname=relative)
        archive.add(output, arcname="validation.json")
        for document in ("README.md", "PORTING.md"):
            archive.add(root / document, arcname=document)
    print(f"Module bundle: {bundle}")
    print(
        f"PASS: {len(modules)} x86-64 modules; kernel versions, symbols, CRCs and namespaces match."
    )
    print(f"Validation report: {output}")


if __name__ == "__main__":
    main()
