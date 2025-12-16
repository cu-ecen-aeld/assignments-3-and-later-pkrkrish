#!/bin/bash
# Script to build Linux kernel + BusyBox + initramfs for ARM64
# Author: Siddhant Jajoo (updated)

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_REPO=https://git.busybox.net/busybox.git
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath "$(dirname "$0")")
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

# Toolchain directory (adjust if installed elsewhere)
TOOLCHAIN_DIR="/home/rpk/arm-toolchain/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu"

# Add toolchain bin directory to PATH so gcc, ld, ar, etc. are found
if [ -d "$TOOLCHAIN_DIR/bin" ]; then
  export PATH="$TOOLCHAIN_DIR/bin:$PATH"
fi

# Sanity check: fail early if critical tools are missing
for tool in gcc ld ar as objcopy; do
  if ! command -v ${CROSS_COMPILE}${tool} >/dev/null 2>&1; then
    echo "Error: ${CROSS_COMPILE}${tool} not found in PATH. Check TOOLCHAIN_DIR."
    exit 1
  fi
done

# OUTDIR override
if [ $# -ge 1 ]; then
  OUTDIR=$(realpath "$1")
  echo "Using passed directory ${OUTDIR} for output"
else
  echo "Using default directory ${OUTDIR} for output"
fi

mkdir -p "${OUTDIR}"
cd "${OUTDIR}"

# -------- Kernel: clone and build Image if missing --------
if [ ! -d "${OUTDIR}/linux-stable" ]; then
  echo "Cloning Linux stable ${KERNEL_VERSION}"
  git clone "${KERNEL_REPO}" --depth 1 --single-branch --branch "${KERNEL_VERSION}" linux-stable
fi

if [ ! -e "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image" ]; then
  cd "${OUTDIR}/linux-stable"
  echo "Building kernel ${KERNEL_VERSION} for ${ARCH}"
  make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" mrproper
  make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" defconfig
  make -j"$(nproc)" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" Image
  make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" dtbs
fi

echo "Adding the Image in outdir"
cp "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image" "${OUTDIR}/Image"

# -------- Rootfs staging --------
echo "Creating the staging directory for the root filesystem"
if [ -d "${OUTDIR}/rootfs" ]; then
  echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
  sudo rm -rf "${OUTDIR}/rootfs"
fi

mkdir -p "${OUTDIR}/rootfs"
cd "${OUTDIR}/rootfs"
mkdir -p bin dev etc home/conf lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin var/log

# -------- BusyBox: clone, build, install --------
cd "${OUTDIR}"
if [ ! -d "${OUTDIR}/busybox" ]; then
  echo "Cloning BusyBox ${BUSYBOX_VERSION}"
  git clone "${BUSYBOX_REPO}" --branch "${BUSYBOX_VERSION}" --depth 1 busybox
fi

cd busybox
make distclean
make defconfig
make -j"$(nproc)" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}"
make CONFIG_PREFIX="${OUTDIR}/rootfs" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" install

# -------- Library dependencies for BusyBox --------
echo "Library dependencies"
BUSYBOX_BIN="${OUTDIR}/rootfs/bin/busybox"
PROGRAM_INTERP="$(${CROSS_COMPILE}readelf -a "${BUSYBOX_BIN}" | awk -F': ' '/program interpreter/ {print $2}' | tr -d '[]')"
SHARED_LIBS="$(${CROSS_COMPILE}readelf -a "${BUSYBOX_BIN}" | awk -F': ' '/Shared library/ {print $2}' | tr -d '[]')"

SYSROOT="$(${CROSS_COMPILE}gcc -print-sysroot || true)"

copy_one_lib() {
  local name="$1"
  [ -z "$name" ] && return 0
  found=$(find "$SYSROOT" "$TOOLCHAIN_DIR" -name "$name" 2>/dev/null | head -n1 || true)
  if [ -n "$found" ]; then
    case "$found" in
      */lib64/*) sudo cp -a "$found" "${OUTDIR}/rootfs/lib64/" ;;
      *)         sudo cp -a "$found" "${OUTDIR}/rootfs/lib/" ;;
    esac
  else
    echo "WARNING: Could not locate library: $name"
  fi
}

interp_base="$(basename "$PROGRAM_INTERP" 2>/dev/null || echo '')"
copy_one_lib "$interp_base"

while IFS= read -r lib; do
  copy_one_lib "$lib"
done <<< "$SHARED_LIBS"

# -------- Device nodes --------
cd "${OUTDIR}/rootfs"
if [ ! -e "dev/null" ]; then
  sudo mknod -m 666 dev/null c 1 3
fi
if [ ! -e "dev/console" ]; then
  sudo mknod -m 600 dev/console c 5 1
fi

# -------- Finder app: build and copy --------
echo "Building writer utility and copying finder files"
cd "${FINDER_APP_DIR}"
rm -f writer *.o || true
${CROSS_COMPILE}gcc -c writer.c -o writer.o
${CROSS_COMPILE}gcc writer.o -o writer

cp -a writer finder.sh finder-test.sh autorun-qemu.sh "${OUTDIR}/rootfs/home/"
cp -a conf/username.txt conf/assignment.txt "${OUTDIR}/rootfs/home/conf/"

# -------- Create minimal /init script --------
cat << 'EOF' | sudo tee "${OUTDIR}/rootfs/init" > /dev/null
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
echo "Init script running..."
exec setsid /bin/sh
EOF
sudo chmod +x "${OUTDIR}/rootfs/init"

# -------- Ownership and initramfs --------
sudo chown -R root:root "${OUTDIR}/rootfs"

cd "${OUTDIR}/rootfs"
find . -print0 | cpio --null -ov --format=newc --owner root:root | gzip -9 > "${OUTDIR}/initramfs.cpio.gz"

echo "Done."
echo "Kernel Image: ${OUTDIR}/Image"
echo "Initramfs:    ${OUTDIR}/initramfs.cpio.gz"
echo "Boot with:"
echo "qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -smp 1 -m 256M \\"
echo "  -kernel ${OUTDIR}/Image -initrd ${OUTDIR}/initramfs.cpio.gz \\"
echo "  -append \"rdinit=/init console=ttyAMA0\""
