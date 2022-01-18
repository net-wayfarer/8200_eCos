#!/bin/bash

# UBIFS logical eraseblock limit - increase this number if mkfs.ubifs complains
#
# for 93385 using 128MB NAND and smaller flash partitions, 2047 is too large
# causing the attach/mount operation to fail.  Override this # to 250 for those
# cases.
max_leb_cnt=2047

max_led_cnt_small_eb=250

# change to '1' to build NAND ubifs images (they might be large files,
# particularly for bigger eraseblock sizes)
build_nand_ubifs=1

# Volume size (in erase blocks)
min_eb_per_vol=18

LINUXDIR=linux
RG_CM_DIR=images/rg_cm_images

cat ./rg_apps/.last_profile

: ${APPSDIR:=./rg_apps/targets/`cat ./rg_apps/.last_profile`/fs.install}

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

function make_ubi_stb_img()
{
	pebk=$1
	page=$2

	peb=$(($1 * 1024))

	echo -e "\nCreating UBI image for BA/STB partition"

	if [ $pebk -eq 128 ]; then
		max_leb_cnt=$max_led_cnt_small_eb
	fi

	if [ $page -lt 64 ]; then
		leb=$(($peb - 64 * 2))
		minmsg="minimum write size $page (NOR)"
	else
		leb=$(($peb - $page * 2))
		minmsg="minimum write size $page (NAND)"
	fi

	out="images/ubifs-${pebk}k-${page}-${TARGET}-STB.img"

	echo "Writing UBIFS rootfs image for ${pebk}kB erase, ${minmsg}..."

	bin/mkfs.ubifs -U -D rootfs/misc/devtable.txt -r rootfs/romfs -o tmp/ubifs0.img \
		-m $page -e $leb -c ${max_leb_cnt}

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

	if [ -d "$RG_CM_DIR" ]
	then
		echo "Writing UBIFS apps image for ${pebk}kB erase, ${minmsg}..."

		bin/mkfs.ubifs -U -r "$RG_CM_DIR" -o tmp/ubifs1.img \
			-m $page -e $leb -c ${max_leb_cnt}

		vol_size=$(du -bk tmp/ubifs1.img | cut -f1)
		calculate_volume_size $pebk $vol_size

		cat >> tmp/ubinize.cfg <<-EOF
		[boot-volume]
		mode=ubi
		image=tmp/ubifs1.img
		vol_id=1
		vol_size=${correctedvolsize}KiB
		vol_type=dynamic
		vol_name=boot
		vol_flags=autoresize
EOF
	fi

	bin/ubinize -o tmp/ubi.img -m $page -p $peb tmp/ubinize.cfg

	mv tmp/ubi.img $out
	echo "    -> $out"
}

function make_ubi_rg_img()
{
	pebk=$1
	page=$2

	peb=$(($1 * 1024))

	echo -e "\nCreating UBI image for RG partition"

	if [ $pebk -eq 128 ]; then
		max_leb_cnt=$max_led_cnt_small_eb
	fi

	if [ $page -lt 64 ]; then
		leb=$(($peb - 64 * 2))
		minmsg="minimum write size $page (NOR)"
	else
		leb=$(($peb - $page * 2))
		minmsg="minimum write size $page (NAND)"
	fi

	out="images/ubifs-${pebk}k-${page}-${TARGET}-RG.img"

	echo "Writing UBIFS rootfs image for ${pebk}kB erase, ${minmsg}..."

	bin/mkfs.ubifs -U -D rootfs/misc/devtable.txt -r rootfs/romfs -o tmp/ubifs0.img \
		-m $page -e $leb -c ${max_leb_cnt}

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

	bin/mkfs.ubifs -U -r $RG_APPS_PATH -o tmp/ubifs1.img \
		-m $page -e $leb -c ${max_leb_cnt}

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

	echo -e "\nCreating UBI rootfs image for CM partition"

	if [ $pebk -eq 128 ]; then
		max_leb_cnt=$max_led_cnt_small_eb
	fi

	if [ $page -lt 64 ]; then
		leb=$(($peb - 64 * 2))
		minmsg="minimum write size $page (NOR)"
	else
		leb=$(($peb - $page * 2))
		minmsg="minimum write size $page (NAND)"
	fi

	out="images/ubifs-${pebk}k-${page}-${TARGET}-CM.img"

	rm tmp/ubinize.cfg

	echo "Writing UBIFS CM images for ${pebk}kB erase, ${minmsg}..."

	rm -rf img_tmp
	mkdir -p img_tmp

	[ -f $RG_CM_DIR/cmrun0.bin ] && cp $RG_CM_DIR/cmrun0.bin img_tmp
	[ -f $RG_CM_DIR/cmrun1.bin ] && cp $RG_CM_DIR/cmrun1.bin img_tmp
	[ -f $RG_CM_DIR/cmboot.bin ] && cp $RG_CM_DIR/cmboot.bin img_tmp

	bin/mkfs.ubifs -U -r img_tmp -o tmp/ubifs1.img \
		-m $page -e $leb -c ${max_leb_cnt}

	vol_size=$(du -bk tmp/ubifs1.img | cut -f1)
	calculate_volume_size $pebk $vol_size

	cat >> tmp/ubinize.cfg <<-EOF
	[boot-volume]
	mode=ubi
	image=tmp/ubifs1.img
	vol_id=1
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
	out=images/jffs2-${pebk}k-${TARGET}.img

	echo "Writing JFFS2 image for ${pebk}kB eraseblock size (NOR)..."
	bin/mkfs.jffs2 -U -D rootfs/misc/devtable.txt -r rootfs/romfs \
		-o tmp/jffs2.img -e ${pebk}KiB $JFFS2_ENDIAN
	bin/sumtool -i tmp/jffs2.img -o $out -e ${pebk}KiB $JFFS2_ENDIAN
	echo "  -> $out"
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

#echo "Writing SQUASHFS image..."
#rm -f images/squashfs-${TARGET}.img
#bin/mksquashfs romfs images/squashfs-${TARGET}.img \
#	-processors 1 -root-owned -p "/dev/console c 0600 0 0 5 1"
#chmod 0644 images/squashfs-${TARGET}.img
#echo "    -> images/squashfs-${TARGET}.img"

# 64k erase / 1B unit size - NOR
#make_ubi_img 64 1

# 128k erase / 1B unit size - NOR
#make_ubi_img 128 1

# 256k erase / 1B unit size - NOR
#make_ubi_img 256 1

if [ "$build_nand_ubifs" = "1" ]; then

	# 16k erase / 512B page - small NAND
	#make_ubi_img 16 512

	# 128k erase / 2048B page - NAND
	#make_ubi_img 128 2048
#	make_ubi_stb_img 128 2048
	make_ubi_rg_img 128 2048
	make_ubi_cm_img 128 2048

	# 256k erase / 4096B page - NAND
	#make_ubi_img 256 4096
#	make_ubi_rg_img 256 4096
#	make_ubi_cm_img 256 4096

	# 512k erase / 4096B page - large NAND
	#make_ubi_img 512 4096

	# 1MB erase / 4096B page - large NAND
#	make_ubi_stb_img 1024 4096
#	make_ubi_stb_img 2048 8192
#	make_ubi_rg_img 1024 4096
#	make_ubi_rg_img 2048 8192
#	make_ubi_cm_img 1024 4096
#	make_ubi_cm_img 2048 8192

	# 1MB erase / 8192B page - large NAND
	#make_ubi_img 1024 8192

	# 2MB erase / 4096B page - large NAND
	#make_ubi_img 2048 4096

	# 2MB erase / 8192B page - large NAND
	#make_ubi_img 2048 8192

else
	echo "Skipping NAND UBIFS images - set build_nand_ubifs=1 if they are needed"
	echo ""
fi

# jffs2 NOR images for 64k, 128k, 256k erase sizes
#make_jffs2_img 64
#make_jffs2_img 128
#make_jffs2_img 256

echo -e "\nWriting NFS rootfs tarball..."
rm -f tmp/nfsroot.tar images/nfsroot-${TARGET}.tar.bz2
rm -rf tmp/romfs

mkdir -p tmp/romfs/boot
cp $LINUXDIR/.config tmp/romfs/boot/config
cp $LINUXDIR/Module.symvers tmp/romfs/boot/
cp $LINUXDIR/System.map tmp/romfs/boot/
cp $LINUXDIR/vmlinux tmp/romfs/boot/

cp rootfs/misc/devconsole.tar tmp/nfsroot.tar
chmod u+w tmp/nfsroot.tar
tar --owner 0 --group 0 -rf tmp/nfsroot.tar romfs/
tar --owner 0 --group 0 -rf tmp/nfsroot.tar -C tmp romfs/boot/
bzip2 < tmp/nfsroot.tar > images/nfsroot-${TARGET}.tar.bz2
echo "    -> images/nfsroot-${TARGET}.tar.bz2"
echo ""

exit 0
