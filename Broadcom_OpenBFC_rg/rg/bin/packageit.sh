#!/bin/bash

TARGET=$(cat .target)
TARG_SHORT=${TARGET:0:4}

APPS_IMAGE=$IMAGESDIR/apps_"$FSTYPE"_"$TARG_SHORT".bin
RG_APPS_IMAGE=$IMAGESDIR/apps_"$TARG_SHORT"_rg.tgz
RG_ROOTFS_IMAGE=$IMAGESDIR/rootfs_"$TARG_SHORT"_rg.tgz
CONF_IMAGE=$IMAGESDIR/config_jffs2.bin
KERNEL_IMAGE=$IMAGESDIR/bcm"$TARG_SHORT"_fs_kernel
IMAGE_SIG=0x"$TARG_SHORT"
RG_CM_DIR=$IMAGESDIR/rg_cm_images

echo $TARG_SHORT
echo $APPS_IMAGE

# Create only rgrun.bin.initrd.full, an initrd which also includes
# the /mnt/apps within the rootfs (i.e. no NAND flash is used)
create_full_rgrun_bin() {
  RG_DTB=../linux/arch/${ARCH}/boot/dts/rg."$TARGET".dtb
  RG_DTS_ORIG=../linux/arch/${ARCH}/boot/dts/rg."$TARGET".dts
  RG_DTS=$IMAGESDIR/rg.dts.tmp
  RG_BOOT=romfs/lib/firmware/rgboot.bin
  RGRUN_INITRD=$IMAGESDIR/rgrun.bin.initrd.full
  KERNEL_INITRD=$IMAGESDIR/vmlinuz-initrd-"$TARGET"

  echo -e "\n=== Creating full initrd rg image ==="

  # Copy RG DTS file from kernel so we modify it locally
  cp -f $RG_DTS_ORIG $RG_DTS > /dev/null

  rm -f $RGRUN_INITRD

  echo -e "\nPreparing rg.dts for initrd boot\nInitial rg.dts bootargs:"

  grep bootargs $RG_DTS | sed -e 's/^[ \t]*bootargs/    bootargs/g'
  sed -i s/'ubi.mtd=vf0 root=ubi0:rootfs rootfstype=ubifs cert '// $RG_DTS
  sed -i s/'ubifs_apps '// $RG_DTS
  echo -e "\nModified rg.dts bootargs:"
  grep bootargs $RG_DTS | sed -e 's/^[ \t]*bootargs/    bootargs/g'
  echo

  ../linux/scripts/dtc/dtc -I dts -o $RG_DTB -O dtb $RG_DTS
  dd if=$RG_BOOT bs=4096 count=1 conv=sync > $RGRUN_INITRD 2>> /dev/null
  dd if=$RG_DTB bs=28672 count=1 conv=sync >> $RGRUN_INITRD 2>> /dev/null
  cat $KERNEL_INITRD >> $RGRUN_INITRD
  echo "    ->  $RGRUN_INITRD"
}

create_rgrun_bin() {
  RG_DTB=../linux/arch/${ARCH}/boot/dts/rg."$TARGET".dtb
  RG_DTS_ORIG=../linux/arch/${ARCH}/boot/dts/rg."$TARGET".dts
  RG_DTS=$IMAGESDIR/rg.dts.tmp
  RG_BOOT=romfs/lib/firmware/rgboot.bin
  RG_RUN=$IMAGESDIR/rgrun.bin
  RGRUN_INITRD=$RG_RUN.initrd
  RGRUN_FLASHFS=$RG_RUN.flashfs
  RGRUN_CERT=$RGRUN_FLASHFS.cert

  echo -e "\n=== Creating unified kernel images for RG =="

  # Copy RG DTS file from kernel so we modify it locally
  cp -f $RG_DTS_ORIG $RG_DTS > /dev/null

  rm -f $RG_RUN
  rm -f $RGRUN_INITRD
  rm -f $RGRUN_FLASHFS
  rm -f $RGRUN_CERT

  echo -e "\nPreparing rg.dts for initrd boot\nInitial rg.dts bootargs:"

  grep bootargs $RG_DTS | sed -e 's/^[ \t]*bootargs/    bootargs/g'
  sed -i s/'ubi.mtd=vf0 root=ubi0:rootfs rootfstype=ubifs cert '// $RG_DTS
  echo -e "\nModified rg.dts bootargs:"
  grep bootargs $RG_DTS | sed -e 's/^[ \t]*bootargs/    bootargs/g'
  echo

  ../linux/scripts/dtc/dtc -I dts -o $RG_DTB -O dtb $RG_DTS
  dd if=$RG_BOOT bs=4096 count=1 conv=sync > $RG_RUN 2>> /dev/null
  dd if=$RG_DTB bs=28672 count=1 conv=sync >> $RG_RUN 2>> /dev/null
  cat $KERNEL_INITRD >> $RG_RUN
  mv -f $RG_RUN $RGRUN_INITRD
  echo "    ->  $RGRUN_INITRD"

  echo -e "\nPreparing rg.dts for non-initrd boot\nInitial rg.dts bootargs:"

  grep bootargs $RG_DTS | sed -e 's/^[ \t]*bootargs/    bootargs/g'
  sed -i s/'ubi.mtd=vf0 root=ubi0:rootfs rootfstype=ubifs '// $RG_DTS
  sed -i s/'bootargs = \"'/'bootargs = \"ubi.mtd=vf0 root=ubi0:rootfs rootfstype=ubifs '/ $RG_DTS
  echo -e "\nModified rg.dts bootargs:"
  grep bootargs $RG_DTS | sed -e 's/^[ \t]*bootargs/    bootargs/g'
  echo

  ../linux/scripts/dtc/dtc -I dts -o $RG_DTB -O dtb $RG_DTS
  dd if=$RG_BOOT bs=4096 count=1 conv=sync > $RG_RUN 2>> /dev/null
  dd if=$RG_DTB bs=28672 count=1 conv=sync >> $RG_RUN 2>> /dev/null
  cat $KERNEL_NONINITRD >> $RG_RUN
  mv -f $RG_RUN $RGRUN_FLASHFS
  echo "    ->  $RGRUN_FLASHFS"
  echo -e "\nDefault rgrun.bin is rgrun.bin.flashfs.  Creating soft link."
  ln -sf rgrun.bin.flashfs $RG_RUN
  echo "    ->  $RG_RUN"

  [ -e "$RG_CM_DIR" ] || mkdir -p "$RG_CM_DIR"
  cp "$RG_RUN" "$RG_CM_DIR"

  echo -e "\nPreparing rg.dts for non-initrd cert boot\nInitial rg.dts bootargs:"

  grep bootargs $RG_DTS | sed -e 's/^[ \t]*bootargs/    bootargs/g'
  sed -i s/'ubi.mtd=vf0 root=ubi0:rootfs rootfstype=ubifs '// $RG_DTS
  sed -i s/'bootargs = \"'/'bootargs = \"ubi.mtd=vf0 root=ubi0:rootfs rootfstype=ubifs cert '/ $RG_DTS
  echo -e "\nModified rg.dts bootargs:"
  grep bootargs $RG_DTS | sed -e 's/^[ \t]*bootargs/    bootargs/g'
  echo

  ../linux/scripts/dtc/dtc -I dts -o $RG_DTB -O dtb $RG_DTS
  dd if=$RG_BOOT bs=4096 count=1 conv=sync > $RGRUN_CERT 2>> /dev/null
  dd if=$RG_DTB bs=28672 count=1 conv=sync >> $RGRUN_CERT 2>> /dev/null
  cat $KERNEL_NONINITRD >> $RGRUN_CERT
  echo "    ->  $RGRUN_CERT"
}

if [ ! -d "$TOOLSDIR" ]; then
	echo -e "\nerror: this program must be run from the uclinux-rootfs dir"
	exit 1
fi

if [[ "$TARGET" = *_be* ]]; then
    JFFS2_ENDIAN=-b
else
    JFFS2_ENDIAN=-l
fi

# Handle dcm builds - Remove the suffix
OLD_TARGET=${TARGET}
if [[ "$TARGET" = *-dcm ]]; then
    DCM_BUILD=1
    TARGET="${OLD_TARGET%-dcm}"
    echo -e "\nDCM build: Using non-dcm target: $TARGET\n"
fi

KERNEL_INITRD=$IMAGESDIR/vmlinuz-initrd-"$OLD_TARGET"
KERNEL_NONINITRD=$IMAGESDIR/vmlinuz-"$OLD_TARGET"

if [ -z "$TFTPDIR" ] || [ ! -d "$TFTPDIR" ]; then
    TFTPDIR=/tftpboot
fi

if [ ! -z "$IMGTYPE" ]; then
    # Script was called to create the RG tarball images and rgrun.bin
    if  [ "$IMGTYPE" = "rguapps" ]; then
        if [ -e "$APPSDIR" ]; then
            echo -e "\n=== Creating RGU apps tarball image =="

            rm -f "$RG_APPS_IMAGE"
            tar czf $RG_APPS_IMAGE -C $APPSDIR .
            echo "    ->  $RG_APPS_IMAGE"
        fi

        if [ -e "$ROOTFSDIR" ]; then
            echo -e "\n=== Creating RGU rootfs tarball image =="
            rm -f "$RG_ROOTFS_IMAGE"
            tar czf $RG_ROOTFS_IMAGE -C $ROOTFSDIR .
            echo "    ->  $RG_ROOTFS_IMAGE"
        fi

        if [ "$TARGET" = "7145a0" ]; then
            # Now make the rgrun.bin (Only applies to 3385A0 w/ separate RG CPU)
            create_rgrun_bin
        else
            # Create "rg_cm_images" directory which will be used in other scripts/utilities
            mkdir -p $RG_CM_DIR > /dev/null
        fi

        if [ -d "$TFTPDIR" ]; then
          echo -e "\n=== Copying images to tftpboot directory =="
          if [ -e "$RG_APPS_IMAGE" ]; then
            cp "$RG_APPS_IMAGE" "$TFTPDIR"
            echo "Copied $RG_APPS_IMAGE to $TFTPDIR"
          fi

          if [ -e "$RG_ROOTFS_IMAGE" ]; then
            cp "$RG_ROOTFS_IMAGE" "$TFTPDIR"
            echo "Copied $RG_ROOTFS_IMAGE to $TFTPDIR"
          fi

          if [ -e "$KERNEL_INITRD" ]; then
            cp "$KERNEL_INITRD" "$TFTPDIR"
            echo "Copied $KERNEL_INITRD to $TFTPDIR"
          fi

          if [ -e "$KERNEL_NONINITRD" ]; then
            cp "$KERNEL_NONINITRD" "$TFTPDIR"
            echo "Copied $KERNEL_NONINITRD to $TFTPDIR"
          fi

          if [ -e "$RGRUN_INITRD" ]; then
            cp "$RGRUN_INITRD" "$TFTPDIR"
            echo "Copied $RGRUN_INITRD to $TFTPDIR"
          fi

          if [ -e "$RGRUN_FLASHFS" ]; then
            cp "$RGRUN_FLASHFS" "$TFTPDIR"
            echo "Copied $RGRUN_FLASHFS to $TFTPDIR"
          fi
        fi

        exit 0
    fi

    # Script was called to create only the config image
    if  [ "$IMGTYPE" = "config" ] && [ ! -z "CONFDIR" ]; then
        echo -e "\n=== Creating jffs2 config image =="
        rm -f "$CONF_IMAGE"
        bin/mkfs.jffs2 -q "$JFFS2_ENDIAN" -r "$CONFDIR" -o $IMAGESDIR/tmp_jffs2_pre.img
        bin/sumtool "$JFFS2_ENDIAN" -i $IMAGESDIR/tmp_jffs2_pre.img -o "$CONF_IMAGE"
        rm -f $IMAGESDIR/tmp_jffs2_pre.img
        echo "    ->  $CONF_IMAGE"
        exit 0
    fi

    if  [ "$IMGTYPE" = "rginitrdprep" ]; then
        echo -e "\n=== Prepare skel for RG initrd ==="
        mkdir skel/data
        tar -xvzf $RG_APPS_IMAGE -C skel/mnt/apps
        exit 0
    fi

    if  [ "$IMGTYPE" = "rginitrdrgrun" ]; then
        if [ "$TARGET" = "7145a0" ]; then
            # Now make the rgrun.bin (Only applies to 3385A0 w/ separate RG CPU)
            echo -e "\n=== Create rgrun ==="
            create_full_rgrun_bin
        fi
        exit 0
    fi

    if  [ "$IMGTYPE" = "rginitrdclean" ]; then
        echo -e "\n=== Clean skel of RG initrd ==="
        find ./skel/mnt/apps -type f ! -name 'dummy.txt' -execdir rm {} +
        rmdir skel/data
        exit 0
    else
	    echo -e "\nerror: invalid arguments for image creation ($IMGTYPE) ($CONFDIR)"
        exit 1
    fi
fi

# Otheriwse, the script was called to make kernel and rootfs image
if [ ! -e $IMAGESDIR/squashfs-$TARGET.img ]; then
	echo -e "\nerror: missing $IMAGESDIR/squashfs-$TARGET.img"
	echo "run 'make images-$TARGET' first"
	exit 1
fi

if [ ! -e linux/vmlinux ]; then
	echo -e "\nerror: missing linux/vmlinux"
	echo "run 'make -C linux vmlinux'"
	exit 1
fi

rm -f KERNEL_IMAGE APPS_IMAGE

echo -e "\n=== Creating Linux kernel and rootfs image for $ARCH =="
if [ "$ARCH" = "mips" ]; then
mips-linux-uclibc-strip -o $IMAGESDIR/vmlinux linux/vmlinux
mips-linux-uclibc-objcopy -O binary $IMAGESDIR/vmlinux $IMAGESDIR/vmlinux.bin
else
arm-linux-uclibcgnueabi-strip -o $IMAGESDIR/vmlinux linux/vmlinux
arm-linux-uclibcgnueabi-objcopy -O binary $IMAGESDIR/vmlinux $IMAGESDIR/vmlinux.bin
fi

"$TOOLSDIR"/ProgramStore2 -f $IMAGESDIR/vmlinux.bin -f2 $IMAGESDIR/squashfs-$TARGET.img -o "$KERNEL_IMAGE" -v 002.17h -a 0x84010000 -n2 -p 65536 -c 4 -s $IMAGE_SIG > /dev/null
rm -f $IMAGESDIR/vmlinux $IMAGESDIR/vmlinux.bin

echo "    ->  $KERNEL_IMAGE"

if [ ! -d "$APPSDIR" ]; then
    echo "error: apps partition directory ($APPSDIR) not found"
    exit 1
fi

if [ "$FSTYPE" = "squashfs" ]; then
    echo -e "\n=== Creating squashfs apps image =="
    bin/mksquashfs "$APPSDIR" $IMAGESDIR/squashfs-app.img -noappend > /dev/null
    dd if=/dev/zero of=filepad.tmp bs=1k count=50
    "$TOOLSDIR"/ProgramStore2 -f filepad.tmp -f2 $IMAGESDIR/squashfs-app.img -o "$APPS_IMAGE" -v 002.17h -a 0x84010000 -n2 -p 65536 -c 0 -s $IMAGE_SIG > /dev/null
    rm -f filepad.tmp $IMAGESDIR/squashfs-app.img
    echo "    ->  $APPS_IMAGE"
elif [ "$FSTYPE" = "jffs2" ]; then
    echo -e "\n=== Creating jffs2 apps image =="
    bin/mkfs.jffs2 -q "$JFFS2_ENDIAN" -r "$APPSDIR" -o $IMAGESDIR/tmp_jffs2_pre.img
    bin/sumtool "$JFFS2_ENDIAN" -i $IMAGESDIR/tmp_jffs2_pre.img -o $IMAGESDIR/tmp_jffs2.img
    dd if=/dev/zero of=filepad.tmp bs=1k count=50
    "$TOOLSDIR"/ProgramStore2 -f filepad.tmp -f2 $IMAGESDIR/tmp_jffs2.img -o "$APPS_IMAGE" -v 002.17h -n2 -p 65536 -c 0 -s $IMAGE_SIG > /dev/null
    rm -f filepad.tmp $IMAGESDIR/tmp_jffs2_pre.img $IMAGESDIR/tmp_jffs2.img
    echo "    ->  $APPS_IMAGE"
else
    echo -e "\nerror: did not recognize file system type: '$FSTYPE'"
    exit 1
fi

if [ -d "$TFTPDIR" ]; then
    echo -e "\n=== Copying images to tftpboot directory =="
    if [ -e "$KERNEL_IMAGE" ]; then
        cp "$KERNEL_IMAGE" "$TFTPDIR"
        echo -e "Copied $KERNEL_IMAGE to $TFTPDIR"
    fi

    if [ -e "$APPS_IMAGE" ]; then
        cp "$APPS_IMAGE" "$TFTPDIR"
        echo -e "Copied $APPS_IMAGE to $TFTPDIR\n"
    fi
fi
