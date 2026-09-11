#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
kernel_release=${1:-$(uname -r)}
cd "$repo_dir"
mkdir -p build-logs
log_file="build-logs/build-${kernel_release}.log"

# No installer, DKMS registration, depmod writes, or module loading.
make clean KERNEL_UNAME="$kernel_release" > "$log_file" 2>&1
make all KERNEL_UNAME="$kernel_release" CC=x86_64-linux-gnu-gcc \
    TARGET_PRODUCT=mini Driver_Install_Mode=normal BUILD_VASCEND=n \
    >> "$log_file" 2>&1 || {
    tail -n 80 "$log_file" >&2
    exit 1
}
python3 -m unittest discover -s tests -v
python3 scripts/verify-modules.py "$kernel_release"
printf 'Build log: %s/%s\n' "$repo_dir" "$log_file"
