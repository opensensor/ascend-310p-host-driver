#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
kernel_release=${1:-$(uname -r)}
profile=${2:-bare-metal}
case "$profile" in
    bare-metal) build_vascend=n ;;
    virtualization) build_vascend=y ;;
    *) printf 'Unknown profile: %s (use bare-metal or virtualization)\n' "$profile" >&2; exit 2 ;;
esac
cd "$repo_dir"
mkdir -p build-logs
log_file="build-logs/build-${kernel_release}-${profile}.log"

# No installer, DKMS registration, depmod writes, or module loading.
make clean KERNEL_UNAME="$kernel_release" BUILD_VASCEND=y > "$log_file" 2>&1
make all KERNEL_UNAME="$kernel_release" CC=x86_64-linux-gnu-gcc \
    TARGET_PRODUCT=mini Driver_Install_Mode=normal BUILD_VASCEND="$build_vascend" \
    >> "$log_file" 2>&1 || {
    tail -n 80 "$log_file" >&2
    exit 1
}
python3 -m unittest discover -s tests -v
python3 scripts/verify-modules.py "$kernel_release" --profile "$profile"
printf 'Build log: %s/%s\n' "$repo_dir" "$log_file"
