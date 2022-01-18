#!/bin/bash


usage() { echo "Usage: `basename $0` <kernel|userspace>" 1>&2; exit 1; }

if [ $# != 1 ]; then
    usage
    exit 1
fi

image_type=$1
if [ $image_type != "kernel" ] && [ $image_type != "userspace" ]; then
    usage
    exit 2
fi

# UBIFS logical eraseblock limit - increase this number if mkfs.ubifs complains
max_leb_cnt=2047
max_led_cnt_small_eb=250

# change to '1' to build NAND ubifs images (they might be large files,
# particularly for bigger eraseblock sizes)
build_nand_ubifs=1

LINUXDIR=linux
: ${APPSDIR:=rg_apps/targets/`cat rg_apps/.last_profile`/fs.install}

# Volume size (in erase blocks)
min_eb_per_vol=18

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

	out=$IMAGESDIR/ubifs-${pebk}k-${page}-${TARGET}-RG.img

	echo "Writing UBIFS rootfs image for ${pebk}kB erase, ${minmsg}..."

	bin/mkfs.ubifs -U -F -D $ROOTDIR/misc/devtable.txt -r rootfs/romfs -o tmp/ubifs0.img \
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

	    bin/mkfs.ubifs -U -F -r $RG_APPS_PATH -o tmp/ubifs1.img \
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

	echo "bin/ubinize -o tmp/ubi.img -m $page -p $peb tmp/ubinize.cfg"

	bin/ubinize -o tmp/ubi.img -m $page -p $peb tmp/ubinize.cfg || ls -l tmp; cat tmp/ubinize.cfg

	mv tmp/ubi.img $out
	echo "    -> $out"
}

function make_ubi_img()
{
	pebk=$1
	page=$2

	peb=$(($1 * 1024))

	if [ $page -lt 64 ]; then
		leb=$(($peb - 64 * 2))
		minmsg="minimum write size $page (NOR)"
	else
		leb=$(($peb - $page * 2))
		minmsg="minimum write size $page (NAND)"
	fi

	out=$IMAGESDIR/ubifs-${pebk}k-${page}-${TARGET}.img

	echo "Writing UBIFS image for ${pebk}kB erase, ${minmsg}..."

	bin/mkfs.ubifs -F -U -D $ROOTDIR/misc/devtable.txt -r rootfs/romfs -o tmp/ubifs.img \
		-m $page -e $leb -c ${max_leb_cnt}

	vol_size=$(du -bm tmp/ubifs.img | cut -f1)

	cat > tmp/ubinize.cfg <<-EOF
	[ubifs]
	mode=ubi
	image=tmp/ubifs.img
	vol_id=0
	vol_size=${vol_size}MiB
	vol_type=dynamic
	vol_name=rootfs
	vol_flags=autoresize
	EOF

	echo "bin/ubinize -o tmp/ubi.img -m $page -p $peb tmp/ubinize.cfg"

	bin/ubinize -o tmp/ubi.img -m $page -p $peb tmp/ubinize.cfg || cat tmp/ubinize.cfg

	mv tmp/ubi.img $out
	echo "  -> $out"
}

function make_jffs2_img()
{
	pebk=$1
	out=$IMAGESDIR/jffs2-${pebk}k-${TARGET}.img

	echo "Writing JFFS2 image for ${pebk}kB eraseblock size (NOR)..."
	bin/mkfs.jffs2 -U -D $ROOTDIR/misc/devtable.txt -r rootfs/romfs \
		-o tmp/jffs2.img -e ${pebk}KiB $JFFS2_ENDIAN
	bin/sumtool -i tmp/jffs2.img -o $out -e ${pebk}KiB $JFFS2_ENDIAN
	echo "  -> $out"
}

#
# MAIN
#

TARGET=$(cat .target)

rm -rf tmp
mkdir -p tmp

JFFS2_ENDIAN=-b
# JFFS2_ENDIAN=-l

# 64k erase / 1B unit size - NOR
# make_ubi_img 64 1

# 128k erase / 1B unit size - NOR
# make_ubi_img 128 1

# 256k erase / 1B unit size - NOR
# make_ubi_img 256 1

if [ "$build_nand_ubifs" = "1" ]; then

	# 16k erase / 512B page - small NAND
	# make_ubi_img 16 512

	# 128k erase / 2048B page - NAND
        if [ $image_type == "userspace" ]; then
	    make_ubi_rg_img 128 2048
	fi

	# 256k erase / 4096B page - NAND
	# make_ubi_img 256 4096

	# 512k erase / 4096B page - large NAND
	# make_ubi_img 512 4096

	# 1MB erase / 4096B page - large NAND
	# make_ubi_img 1024 4096

	# 1MB erase / 8192B page - large NAND
	# make_ubi_img 1024 8192

	# 2MB erase / 4096B page - large NAND
	# make_ubi_img 2048 4096

	# 2MB erase / 8192B page - large NAND
	# make_ubi_img 2048 8192

else
	echo "Skipping NAND UBIFS images - set build_nand_ubifs=1 if they are needed"
	echo ""
fi

# jffs2 NOR images for 64k, 128k, 256k erase sizes
# make_jffs2_img 64
# make_jffs2_img 128
# make_jffs2_img 256

# echo "Writing NFS rootfs tarball..."
# rm -f tmp/nfsroot.tar images/nfsroot-${TARGET}.tar.bz2
# rm -rf tmp/romfs

# mkdir -p tmp/romfs/boot
# cp $LINUXDIR/.config tmp/romfs/boot/config
# cp $LINUXDIR/Module.symvers tmp/romfs/boot/
# cp $LINUXDIR/System.map tmp/romfs/boot/
# cp $LINUXDIR/vmlinux tmp/romfs/boot/

# cp $ROOTDIR/misc/devconsole.tar tmp/nfsroot.tar
# chmod u+w tmp/nfsroot.tar
# tar --owner 0 --group 0 -rf tmp/nfsroot.tar romfs/
# tar --owner 0 --group 0 -rf tmp/nfsroot.tar -C tmp romfs/boot/
# bzip2 < tmp/nfsroot.tar > images/nfsroot-${TARGET}.tar.bz2
# echo "  -> images/nfsroot-${TARGET}.tar.bz2"

if [[ ${TARGET} != *"3384"* ]]; then
    echo This is a 3384/33843 script but the target ${TARGET} is not a 3384. Exiting!
    exit -1
fi

if [[ ${TARGET} == *"33843"* ]]; then
    imagebasenames="bcm33843"
    signature=0x3843
else
    imagebasenames="bcm3384"
    signature=0x3384
fi
echo signature=${signature}

linuxbaseaddr=0x00000000

if [ $image_type == "kernel" ]; then
    echo "creating kernel image(s)"
    for imagebasename in $imagebasenames;
    do
	boardtype=`echo "$imagebasename" | sed 's/.*[43]//'`
	aeolus/ProgramStore/ProgramStore \
	    -f $IMAGESDIR/concat.bin \
	    -o $IMAGESDIR/${TARGET}_kernel \
	    -c 4 -s $signature -a $linuxbaseaddr -v 003.000
    done
fi


if [ $image_type == "userspace" ]; then
    echo "creating USERSPACE image"
    dd if=/dev/urandom of=$IMAGESDIR/filler bs=1k count=100
    aeolus/ProgramStore/ProgramStore \
	-f $IMAGESDIR/filler \
	-f2 $IMAGESDIR/ubifs-128k-2048-${TARGET}-RG.img \
	-o $IMAGESDIR/${TARGET}_apps_nand_ubifs_bs128k_ps2k \
	-p 131072 -c 4 -s $signature -a 0 -v 003.000
    rm $IMAGESDIR/filler

    touch $IMAGESDIR/${TARGET}_rootfs_ubifs_bs128k_ps2k
    echo "rootfs and appsfs are now both in a multivolume apps partition" > $IMAGESDIR/info_about_rootfs.txt
fi

exit 0
