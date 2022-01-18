#!/bin/bash

# Configure the image types to generate
BUILD_NAND_UBIFS=1
BUILD_UBIFS_128_2048=1
BUILD_UBIFS_256_4096=0
BUILD_UBIFS_512_4096=0
BUILD_UBIFS_1024_4096=0
BUILD_UBIFS_2048_8192=0
BUILD_SQUASHFS=0
BUILD_JFFS2=0
BUILD_NFSROOT=0

# UBIFS logical eraseblock limit - increase this number if mkfs.ubifs complains
#
# For 93385 using 128MB NAND and smaller flash partitions, 2047 is too large
# causing the attach/mount operation to fail.  Override this # to 250 for those
# cases.
max_leb_cnt=2047

# Volume size (in erase blocks)
min_eb_per_vol=18

LINUXDIR=linux
RG_CM_DIR=$IMAGESDIR/rg_cm_images
: ${APPSDIR:=../rg_apps/targets/`cat ../rg_apps/.last_profile`/fs.install}

set -e

function calculate_volume_size()
{
	contentsize=$2
	requiredsize=$(($min_eb_per_vol * $1))
	echo "Required Size: $requiredsize kBytes Content Size: $contentsize kBytes"
	if [ $contentsize -lt $requiredsize ]
	then
		#pad to leave room to write
		correctedvolsize=$(( ($requiredsize * 5) / 4))
	else
		correctedvolsize=$(( ($contentsize * 5) / 4))
	fi
	echo "Enlarging volume to $correctedvolsize kBytes"
}

function make_ubi_rg_img()
{
	pebk=$1
	page=$2

	peb=$(($1 * 1024))

	echo -e "\nCreating UBI image for RG partition"

	if [ $page -lt 64 ]; then
		leb=$(($peb - 64 * 2))
		minmsg="minimum write size $page (NOR)"
	else
		leb=$(($peb - $page * 2))
		minmsg="minimum write size $page (NAND)"
	fi

	jeb=$(($leb * 3))
	out=$IMAGESDIR/ubifs-${pebk}k-${page}-${TARGET}-RG.img

	echo "Writing UBIFS rootfs image for ${pebk}kB erase, ${minmsg}..."

	bin/mkfs.ubifs -v -U -D $ROOTDIR/misc/devtable.txt -r $ROMFSDIR -o tmp/ubifs0.img \
		-m $page -e $leb -c ${max_leb_cnt} -j ${jeb}

	vol_size=$(du -bk tmp/ubifs0.img | cut -f1)
	calculate_volume_size $pebk $vol_size

	cat > tmp/ubinize.cfg <<-EOF
	[rootfs-volume]
	mode=ubi
	image=tmp/ubifs0.img
	vol_id=0
	vol_size=${correctedvolsize}KiB
	vol_type=dynamic
	vol_name=rootfs
EOF

	# APPSDIR is normally set in the environment by the caller of this script
	echo "PLAT: ${PLAT} APPSDIR: ${APPSDIR}"
	RG_APPS_PATH=${APPSDIR}

	if [ -d $RG_APPS_PATH ]
	then
	echo "Writing UBIFS apps image for ${pebk}kB erase, ${minmsg}..."

	bin/mkfs.ubifs -v -U -r $RG_APPS_PATH -o tmp/ubifs1.img \
		-m $page -e $leb -c ${max_leb_cnt} -j ${jeb}

	vol_size=$(du -bk tmp/ubifs1.img | cut -f1)
	calculate_volume_size $pebk $vol_size

	cat >> tmp/ubinize.cfg <<-EOF
	[apps-volume]
	mode=ubi
	image=tmp/ubifs1.img
	vol_id=1
	vol_size=${correctedvolsize}KiB
	vol_type=dynamic
	vol_name=apps
	vol_flags=autoresize
EOF
	else
		echo -e "\nWARNING: Could not find UBIFS apps image staging directory: $RG_APPS_PATH\n"
		echo -e "RG apps volume will be empty\n"
	fi

	bin/ubinize -o tmp/ubi.img -m $page -p $peb tmp/ubinize.cfg

	mv tmp/ubi.img $out
	echo "    -> $out"
}

function make_ubi_cm_img()
{
	pebk=$1
	page=$2

	peb=$(($1 * 1024))

	if ls $RG_CM_DIR/cm* >& /dev/null; then
	    echo -e "\nCreating UBI rootfs image for CM partition"
	else
	    echo -e "\nNo files in $RG_CM_DIR so CM images will not be created by RG build.  CM images should now be created as part of CM build."
	    return
	fi

	if [ $page -lt 64 ]; then
		leb=$(($peb - 64 * 2))
		minmsg="minimum write size $page (NOR)"
	else
		leb=$(($peb - $page * 2))
		minmsg="minimum write size $page (NAND)"
	fi

	jeb=$(($leb * 3))

	out=$IMAGESDIR/ubifs-${pebk}k-${page}-${TARGET}-CM.img
	rm tmp/ubinize.cfg

	echo "Writing UBIFS CM images for ${pebk}kB erase, ${minmsg}..."

	rm -rf img_tmp
	mkdir -p img_tmp

	[ -f $RG_CM_DIR/cmrun1.bin ] && cp $RG_CM_DIR/cmrun1.bin img_tmp
	[ -f $RG_CM_DIR/cmboot.bin ] && cp $RG_CM_DIR/cmboot.bin img_tmp

	bin/mkfs.ubifs -v -U -r img_tmp -o tmp/ubifs1.img \
		-m $page -e $leb -c ${max_leb_cnt} -x zlib -j $jeb -f 3 -l 2

	vol_size=$(du -bk tmp/ubifs1.img | cut -f1)
	# calculate_volume_size $pebk $vol_size
	correctedvolsize=$(( ($vol_size + 128) ))

	cat >> tmp/ubinize.cfg <<-EOF
	[boot-volume]
	mode=ubi
	image=tmp/ubifs1.img
	vol_id=0
	vol_size=${correctedvolsize}KiB
	vol_type=dynamic
	vol_name=images
	vol_flags=autoresize
EOF

	bin/ubinize -o tmp/ubi.img -m $page -p $peb tmp/ubinize.cfg

	mv tmp/ubi.img $out
	echo "    -> $out"
	rm -rf img_tmp
}

function make_jffs2_img()
{
	pebk=$1
	out=$IMAGESDIR/jffs2-${pebk}k-${TARGET}.img

	echo "Writing JFFS2 image for ${pebk}kB eraseblock size (NOR)..."
	bin/mkfs.jffs2 -U -D $ROOTDIR/misc/devtable.txt -r $ROMFSDIR \
		-o tmp/jffs2.img -e ${pebk}KiB $JFFS2_ENDIAN
	bin/sumtool -i tmp/jffs2.img -o $out -e ${pebk}KiB $JFFS2_ENDIAN
	echo "  -> $out"
}

function make_squashfs_img()
{
    echo "Writing SQUASHFS image..."
    rm -f $(IMAGESDIR)/squashfs-${TARGET}.img
    bin/mksquashfs $ROMFSDIR $(IMAGESDIR)/squashfs-${TARGET}.img \
        -processors 1 -root-owned -p "/dev/console c 0600 0 0 5 1"
    chmod 0644 $(IMAGESDIR)/squashfs-${TARGET}.img
    echo "    -> $(IMAGESDIR)/squashfs-${TARGET}.img"
}

function make_nfsroot_img()
{
    echo -e "\nWriting NFS rootfs tarball..."
    rm -f tmp/nfsroot.tar $(IMAGESDIR)/nfsroot-${TARGET}.tar.bz2
    rm -rf tmp/romfs

    mkdir -p $ROOTDIR/tmp/romfs/boot
    cp $LINUXDIR/.config $ROOTDIR/tmp/romfs/boot/config
    cp $LINUXDIR/Module.symvers $ROOTDIR/tmp/romfs/boot/
    cp $LINUXDIR/System.map $ROOTDIR/tmp/romfs/boot/
    cp $LINUXDIR/vmlinux $ROOTDIR/tmp/romfs/boot/
    cp $ROOTDIR/misc/devconsole.tar $ROOTDIR/tmp/nfsroot.tar
    chmod u+w $ROOTDIR/tmp/nfsroot.tar

    tar --owner 0 --group 0 -rf $ROOTDIR/tmp/nfsroot.tar -C $ROOTDIR romfs/
    tar --owner 0 --group 0 -rf $ROOTDIR/tmp/nfsroot.tar -C $ROOTDIR/tmp romfs/boot/
    bzip2 < $ROOTDIR/tmp/nfsroot.tar > $(IMAGESDIR)/nfsroot-${TARGET}.tar.bz2
    echo -e "    -> $(IMAGESDIR)/nfsroot-${TARGET}.tar.bz2\n"
}

#
# MAIN
#

TARGET=`cat .target`
PLAT=`cat .target`

rm -rf tmp
mkdir -p tmp

if [[ "$TARGET" = *_be* ]]; then
	JFFS2_ENDIAN=-b
else
	JFFS2_ENDIAN=-l
fi

if [ $BUILD_SQUASHFS -eq 1 ]; then
    make_squashfs_img
fi

if [ $BUILD_NAND_UBIFS -eq 1 ]; then

    if [ $BUILD_UBIFS_128_2048 -eq 1 ]; then
	# 128k erase / 2048B page - NAND
	make_ubi_rg_img 128 2048
	make_ubi_cm_img 128 2048
    fi

    if [ $BUILD_UBIFS_256_4096 -eq 1 ]; then
	# 256k erase / 4096B page - NAND
	make_ubi_rg_img 256 4096
	make_ubi_cm_img 256 4096
    fi

    if [ $BUILD_UBIFS_512_4096 -eq 1 ]; then
	# 512k erase / 4096B page - large NAND
	make_ubi_rg_img 512 4096
	make_ubi_cm_img 512 4096
    fi

    if [ $BUILD_UBIFS_1024_4096 -eq 1 ]; then
	# 1MB erase / 4096B page - large NAND
	make_ubi_rg_img 1024 4096
	make_ubi_cm_img 1024 4096
    fi

    if [ $BUILD_UBIFS_2048_8192 -eq 1 ]; then
	# 2MB erase / 8192B page - large NAND
	make_ubi_rg_img 2048 8192
	make_ubi_cm_img 2048 8192
    fi

else
    echo -e "Skipping NAND UBIFS images - set BUILD_NAND_UBIFS=1 if they are needed\n"
fi

# JFFS2 NOR images for 64k, 128k, 256k erase sizes
if [ $BUILD_JFFS2 -eq 1 ]; then
    make_jffs2_img 64
    make_jffs2_img 128
    make_jffs2_img 256
fi

if [ $BUILD_NFSROOT -eq 1 ]; then
    make_nfsroot_img
fi

exit 0
