#!/usr/bin/env bash

if [ ! -n "$1" ]; then
	echo "Usage: `basename $0` all rg  atlas bolt  [clean|cleanall] [A0|B0]"
	exit 1
fi 

SAVEIFS=$IFS
IFS=$(echo -en "\n\b")

index=1          	# Initialize count.
Chip=""
ChipSet=""
rg=""
atlas=""
bolt=""
clean=""
cleanall=""
configFlag=""
cFileName=""
firstRG="Y"
PWD=`pwd`
imgDir=$PWD
buildCfgFile=`pwd`"/buildConfigB0.cfg"

for arg in $*
do 
	let "index+=1"
	if [ "$arg" == "-c" ]; then
		configFlag="Y"
		continue
	fi
	if [ "$configFlag" == "Y" ]; then
		cFileName=$arg
		configFlag="N"
		continue
	fi
	case "$arg" in
		A0) 
			echo "Will build images for A0 chipset.";
			ChipSet="A0"
		;;
		B0) 
			echo "Will build images for B0 chipset.";
			ChipSet="B0"
			buildCfgFile=`pwd`"/buildConfigB0.cfg"
		;;
		3390)
			echo "Will build images for 3390 chip.";
			Chip="3390"
		;;
		rg) 
			echo "Will build rg.";
			rg="Y"
		;;
		
		all) 
			echo "Will build all components.";
			rg="Y";
			bolt="Y";
			
		;;
		clean) clean="Y"
		;;
		cleanall) 
			cleanall="Y";
			clean="Y"
		;;
		*) echo "Invalid input $arg";
			exit 1;
		;;
	esac 
done		#Unquoted $* sees arguments as separate words.

PKG_ROOT=`pwd`
BrcmUtilsDir=""

if [ "$ChipSet" == "" ]; then
	echo "Will build default images for A0 chipset.";
	ChipSet="A0"
fi
if [ "$Chip" == "" ]; then
	echo "Will build default images for 3390 chip.";
	Chip="3390"
fi
lowerChipSet=`echo $ChipSet | awk '{print tolower($0)}'`


#Checking for build config file. This file feeds build options to this script.
if [ -a "$buildCfgFile" ]; then
	echo "Will use $buildCfgFile as build configuration."
else
	echo "ERROR: Unable to locate build config file $buildCfgFile."
	exit 1
fi

BUILD_LOG="./builds/${Chip}/${ChipSet}/build_logs/"
BUILD_DATA="./builds/${Chip}/${ChipSet}/build_data/"

mkdir -p $BUILD_LOG
mkdir -p $BUILD_DATA

echo "Verifying toolcahins.."
toolchain_rg=`pwd`"/../../../stbgcc-4.8-1.5/bin"
if [ ! -d "$toolchain_rg" ]; then
	echo "ERROR: Unable to locate toochain_rg $toolchain_rg."
	echo "       Make sure tollchain is properly installed at location defined in ${toolchain_rg}."
	exit 1
else
	echo "Found: toochain_rg $toolchain_rg."
fi



build_bolt(){ # This functions is for building bolt images
	bolt_BuildOptions=( `cat ${buildCfgFile} | grep ^bolt` )
	if [ ${#bolt_BuildOptions[@]} -ne 0 ]; then
		echo "******Building bolt images******"
	fi
	toolchain_bolt=( `cat ${buildCfgFile} | grep ^toolchain_stb | cut -f2 -d"="` )
	unset i
	for i in "${bolt_BuildOptions[@]}"
	do
		imgDir=$PWD
		project=`echo $i |awk -F"|" '{print $1}'`
		product=`echo $i |awk -F"|" '{print $2}'`
		opt=`echo $i |awk -F"|" '{print $3}'`
		expEnv=`echo $i |awk -F"|" '{print $4}'`

		#Getting environment variables from buildConfig file
		OIFS="$IFS"
		IFS=";"
		read -a envAry<<<"${expEnv}"
		IFS="$OIFS"

		bashFile=${BUILD_DATA}${project}_${product}.bash
		logFile=${BUILD_LOG}${project}_${product}.log
		bolt_BUILD_DIR=${PKG_ROOT}/stb/bolt/
		if [ ! -d "$bolt_BUILD_DIR" ]; then
			echo "ERROR: Directory $bolt_BUILD_DIR does not exist.";
			echo "       Make sure you are in top level directory of the package.";
			echo "       Also make sure you have installed common package.";
			exit 1
		fi
		if [ "$clean" == "Y" ]; then
			bashFile=${BUILD_DATA}${project}_${product}_clean.bash
			logFile=${BUILD_LOG}${project}_${product}_clean.log
			CMD="cd ${bolt_BUILD_DIR}\npwd\nmake clean"
		else 
			CMD="export PATH=${toolchain_bolt}:${PATH}:.:\ncd ${bolt_BUILD_DIR}\npwd\n$opt\ncp ${bolt_BUILD_DIR}objs/${Chip}${lowerChipSet}/bolt-v*.bin ${imgDir}/images/${Chip}/${ChipSet}/stb/"
		fi

		echo "" > $bashFile
		#including user defined environment variables from the buildConfig file
		for envStr in "${envAry[@]}"
		do
			echo "${envStr}" >> $bashFile
#			echo "${envStr}"
		done

		echo -e "${CMD}" >> $bashFile
		echo $bashFile
		chmod 755 $bashFile
		$bashFile > $logFile 2>&1 &
	done
	echo "********************************"
}

build_rg(){ # This functions is for building rg linux images
	rg_BuildOptions=( `cat ${buildCfgFile} | grep ^rg` )
	if [ ${#rg_BuildOptions[@]} -ne 0 ]; then
		echo "*****Building rg images.*****"
	fi
	toolchain_rg=( `cat ${buildCfgFile} | grep ^toolchain_rg | cut -f2 -d"="` )
	unset i
	for i in "${rg_BuildOptions[@]}"
	do
		imgDir=$PWD
		project=`echo $i |awk -F"|" '{print $1}'`
		product=`echo $i |awk -F"|" '{print $2}'`
		opt=`echo $i |awk -F"|" '{print $3}'`
		expEnv=`echo $i |awk -F"|" '{print $4}'`

		#Getting environment variables from buildConfig file
		OIFS="$IFS"
		IFS=";"
		read -a envAry<<<"${expEnv}"
		IFS="$OIFS"

		bashFile=${BUILD_DATA}${project}_${product}.bash
		logFile=${BUILD_LOG}${project}_${product}.log

		if [ -f ${PKG_ROOT}/rg/Makefile ] ; then
			rg_BUILD_DIR=${PKG_ROOT}/rg/
		else
			rg_BUILD_DIR=${PKG_ROOT}/rg/rootfs
		fi

		rg_DIR=${PKG_ROOT}/rg/rg_apps/
		if [ ! -d "$rg_BUILD_DIR" ]; then
			echo "ERROR: Directory $rg_BUILD_DIR does not exist.";
			echo "       Make sure you are in top level directory of the package.";
			echo "       Also make sure you have installed common package.";
			exit 1
		fi

		if [ "$clean" == "Y" ]; then
			bashFile=${BUILD_DATA}${project}_${product}_clean.bash
			logFile=${BUILD_LOG}${project}_${product}_clean.log
			CMD="cd ${rg_BUILD_DIR}\npwd\nmake distclean"
		else 
			CMD="cd ${rg_BUILD_DIR}\npwd\nexport PATH=${toolchain_rg}:${PATH}:.:\n"
		fi

		echo -e ${CMD} > $bashFile
		chmod 755 $bashFile
		#including user defined environment variables from the buildConfig file
		for envStr in "${envAry[@]}"
		do
			echo "${envStr}" >> $bashFile
#			echo "${envStr}"
		done

		#Including make command
		echo "${opt}" >> $bashFile

		unset ctr
		if [ "$Chip" != "3390" ]; then
			for ctr in "${compDirAry[@]}"
			do
				if [[ "$ctr" =~ ^"${Chip}/${ChipSet}/${product}"\/* ]]; then
					if [[ "${ctr}" == *cert ]]; then
						echo "cp ${imgDir}/rg/images/rgrun.bin.flashfs.cert ${imgDir}/images/${ctr}/rgrun.bin" >> $bashFile
					else
						echo "cp ${imgDir}/rg/images/rgrun.bin.flashfs ${imgDir}/images/${ctr}/rgrun.bin" >> $bashFile
					fi
				fi
			done
		fi
		echo -e "cp ${imgDir}/rg/images/vmlinuz-* ${imgDir}/images/${Chip}/${ChipSet}/stb/\n" >> $bashFile
		chmod 755 $bashFile
		echo $bashFile
		if [ "$firstRG" == "Y" ]; then
			echo "$bashFile > $logFile 2>&1" > $BUILD_DATA/rg_atlas.bash
		else 
			echo "$bashFile > $logFile 2>&1" >> $BUILD_DATA/rg_atlas.bash
		fi
		chmod 755 $BUILD_DATA/rg_atlas.bash
		firstRG="N"
	done
	echo "*****************************"
}

copy_rg_ubifs() {
	rg_BuildOptions=( `cat ${buildCfgFile} | grep ^rg` )
	toolchain_rg=( `cat ${buildCfgFile} | grep ^toolchain_stb | cut -f2 -d"="` )
	bashFile=""
	logFile=""
	unset i
	allBashFile=${BUILD_DATA}${project}_ubifs.bash
	allLogFile=${BUILD_LOG}${project}_ubifs.log
	touch $allBashFile
	touch $allLogFile
	for i in "${rg_BuildOptions[@]}"
	do
		new="Y"
		imgDir=$PWD
		project=`echo $i |awk -F"|" '{print $1}'`
		product=`echo $i |awk -F"|" '{print $2}'`
		appProduct=`echo ${product#*bcm*} | awk '{print toupper($0)}'`
		unset ctr
		bashFile=${BUILD_DATA}${project}_${product}_ubifs.bash
		logFile=${BUILD_LOG}${project}_${product}_ubifs.log
		echo "========================================"
		echo "bashFile->${bashFile}"
		echo "logFile->${logFile}"
		echo "========================================"
		for ctr in "${compDirAry[@]}"
		do
			prj=`echo $ctr |awk -F"/" '{print $3}'`
			prd=`echo $ctr |awk -F"/" '{print $4}'`
			outDir=`echo $ctr |awk -F"/" '{print $5}'`
#			echo "product-${product}"
#			echo "prj-${prj}"
#			echo "outDir=${outDir}"
			if [ "${product}" == "${prj}" ]; then
				if [ "$new" == "Y" ]; then
					echo -e "echo \"tar xzf ${BUILD_DATA}/${project}_${product}.tgz\"\ntar xzf ${BUILD_DATA}/${project}_${product}.tgz\n" > $bashFile
#					echo -e "export APPSDIR=../rg_apps/targets/${appProduct}/fs.install\necho APPSDIR=\$APPSDIR\n" >> $bashFile
					new="N"
#					if [[ "$product" =~ vcm* ]]; then
#						echo -e "export APPSDIR=../rg_apps/targets/9${Chip}VCM/fs.install\necho APPSDIR=\$APPSDIR\n" >> $bashFile
#					else
#						echo -e "export APPSDIR=../rg_apps/targets/9${Chip}GW/fs.install\necho APPSDIR=\$APPSDIR\n" >> $bashFile
#					fi
				fi
			#	echo "cp ${imgDir}/rg/images/rgrun.bin.flashfs ${imgDir}/images/${ctr}/rgrun.bin" >> $bashFile
				echo -e "rm -rf ${imgDir}/rg/images/rg_cm_images/\nrm -rf ${imgDir}/rg/images/ubifs-*\nmkdir -p ${imgDir}/rg/images/rg_cm_images/\ncd ${PKG_ROOT}\ncp ${imgDir}/images/${Chip}/${ChipSet}/${prj}/${prd}/${outDir}/cm*.bin ${imgDir}/rg/images/rg_cm_images/\ncd ${imgDir}/rg\npwd\necho \"ls  ${imgDir}/rg/images/rg_cm_images\"\nls ${imgDir}/rg/images/rg_cm_images\necho "TARGET FILE VALUE"\ncat ${imgDir}/rg/.target\npwd\n./rootfs/bin/build_rootfs_images.sh\n" >> $bashFile
				if [ "$prd" == "rg_cm_pc15_components_cert" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/pc15_cert/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_pc15_components_prod" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/pc15_prod/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_pc20_components_cert" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/pc20_cert/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_pc20_components_prod" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/pc20_prod/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_d30_components_cert" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/d30_cert/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_d31_components_cert" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/d31_cert/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_d30_components_prod" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/d30_prod/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_d31_components_prod" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/d31_prod/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_pc15_components_cert_euro" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/pc15_cert_euro/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_pc15_components_prod_euro" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/pc15_prod_euro/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_pc20_components_cert_euro" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/pc20_cert_euro/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_pc20_components_prod_euro" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/pc20_prod_euro/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_d30_components_cert_euro" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/d30_cert_euro/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_d31_components_cert_euro" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/d31_cert_euro/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_d30_components_prod_euro" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/d30_prod_euro/${outDir}/" >> $bashFile
				elif [ "$prd" == "rg_cm_d31_components_prod_euro" ]; then
					echo "cp ${imgDir}/rg/images/ubifs-* ${imgDir}/images/${Chip}/${ChipSet}/${prj}/d31_prod_euro/${outDir}/" >> $bashFile
				fi
			fi
		done
		echo "make distclean" >> $bashFile
		chmod 755 $bashFile
		echo "$bashFile" >> $allBashFile
	done
	chmod 755 $allBashFile
	echo "$allBashFile > $allLogFile"
	$allBashFile > $allLogFile 2>&1 &
}

build_atlas(){ # This functions is for building atlas linux images
	atlas_BuildOptions=( `cat ${buildCfgFile} | grep ^atlas` )
	if [ ${#atlas_BuildOptions[@]} -ne 0 ]; then
		echo "Building atlas images."
	fi
	toolchain_atlas=( `cat ${buildCfgFile} | grep ^toolchain_stb | cut -f2 -d"="` )

	unset i
	for i in "${atlas_BuildOptions[@]}"
	do
		imgDir=$PWD
		project=`echo $i |awk -F"|" '{print $1}'`
		product=`echo $i |awk -F"|" '{print $2}'`
		opt=`echo $i |awk -F"|" '{print $3}'`
		expEnv=`echo $i |awk -F"|" '{print $4}'`

		#Getting environment variables from buildConfig file
		OIFS="$IFS"
		IFS=";"
		read -a envAry<<<"${expEnv}"
		IFS="$OIFS"

		bashFile=${BUILD_DATA}${project}_${product}.bash
		logFile=${BUILD_LOG}${project}_${product}.log
		atlas_BUILD_DIR=${PKG_ROOT}/stb/trellis/BSEAV/app/atlas/build/
#		if [ ! -d "$atlas_BUILD_DIR" ]; then
#			echo "ERROR: Directory $atlas_BUILD_DIR does not exist.";
#			echo "       Make sure you are in top level directory of the package.";
#			echo "       Also make sure you have installed common package.";
##			exit 1
#		fi
		if [ "$clean" == "Y" ]; then
			bashFile=${BUILD_DATA}${project}_${product}_clean.bash
			logFile=${BUILD_LOG}${project}_${product}_clean.log
			CMD="cd ${atlas_BUILD_DIR}\nmake clean"
		else 
#			CMD="cd ${atlas_BUILD_DIR}\n$opt\n"
			CMD="cd ${atlas_BUILD_DIR}\n$opt\ncp ${imgDir}/stb/trellis/obj.97145/BSEAV/bin/refsw-*.tgz ${imgDir}/images/${Chip}/${ChipSet}/stb/\n"
		fi
#		env="export NEXUS_PLATFORM=97145\nexport BCHP_VER=${ChipSet}\nexport PLATFORM=97145\nexport B_REFSW_ARCH=arm-linux\nexport LINUX=${PKG_ROOT}/rg/linux\nexport PATH=${toolchain_atlas}:${PATH}:.:\nexport NEXUS_SECURITY_SUPPORT=n\n"
		env="export NEXUS_PLATFORM=97145\nexport BCHP_VER=${ChipSet}\nexport PLATFORM=97145\nexport B_REFSW_ARCH=arm-linux\nexport LINUX=${PKG_ROOT}/rg/linux\nexport PATH=${toolchain_atlas}:${PATH}:.:\n"

		#including user defined environment variables from the buildConfig file
		for envStr in "${envAry[@]}"
		do
			echo "${envStr}" >> $bashFile
#			echo "${envStr}"
		done

		echo -e "${env}${CMD}" > $bashFile
		chmod 755 $bashFile
		if [ "$rg" == "Y" ]; then
			echo "$bashFile > $logFile 2>&1" >> $BUILD_DATA/rg_atlas.bash
		else
			echo "$bashFile > $logFile 2>&1" > $BUILD_DATA/rg_atlas.bash
		fi
		chmod 755 $BUILD_DATA/rg_atlas.bash
		echo $bashFile
#		$BUILD_DATA/rg_atlas.bash > $logFile 2>&1 &
#		$bashFile
	done
}


if [ "$bolt" == "Y" ]; then
	if [ "$clean" != "Y" ]; then
		mkdir -p ${imgDir}/images/${Chip}/${ChipSet}/stb
	fi
	build_bolt
fi
if [ "$rg" == "Y" ]; then
	if [ "$clean" != "Y" ]; then
		mkdir -p ${imgDir}/images/${Chip}/${ChipSet}/stb
	fi
	build_rg
fi
if [ "$atlas" == "Y" ]; then
	if [ "$clean" != "Y" ]; then
		mkdir -p ${imgDir}/images/${Chip}/${ChipSet}/stb
	fi
	build_atlas
fi
if [ "$atlas" == "Y" ] || [ "$rg" == "Y" ]; then
	echo $BUILD_DATA/rg_atlas.bash
	if [ "$clean" != "Y" ]; then
		mkdir -p ${imgDir}/images/${Chip}/${ChipSet}/stb
	fi
	$BUILD_DATA/rg_atlas.bash 2>&1 &
fi

echo "****Following jobs assigned *****"
jobs -l
echo "*********************************"
echo "NOTE: Builds are in progress."
echo "You can review the build logs at ${Chip}/${ChipSet}/builds/build_logs from another session."
wait


#Validation of bolt build
if [ "$bolt" == "Y" ] && [ "${clean}" != "Y" ]; then
	bolt_error=""
	if [ "$Chip" == "3390" ]; then
		bolt_error=( `grep -e "^  LD      objs/${Chip}${lowerChipSet}/fsbl.elf" ${BUILD_LOG}/bolt_${Chip}.log` )
	else
		bolt_error=( `grep -e "^  LD      objs/7145${lowerChipSet}/fsbl.elf" ${BUILD_LOG}/bolt_${Chip}.log` )
	fi
	if [ ${#bolt_error[@]} -eq 0 ]; then
		echo "Bolt build FAILED. Please review builds log ${Chip}/${ChipSet}/builds/build_logs/bolt_${Chip}.log"
	else
		echo "Bolt build successfully."
	fi
fi
#Validation of rg build
if [ "$rg" == "Y" ] && [ "${clean}" != "Y" ]; then
	rg_error=( `grep -e "^=== Making kernel_img ..... finish ===" ${BUILD_LOG}/rg_*.log` )
	if [ ${#rg_error[@]} -eq 0 ]; then
		echo "RG build FAILED. Please review builds log ${Chip}/${ChipSet}/builds/build_logs/rg_${Chip}.log"
	else
		echo "RG build successfully."
	fi
fi
if [ "${rg}" == "Y" ] && [ "${clean}" != "Y" ]; then
	echo "Making ubifs images."
	copy_rg_ubifs
fi
echo "**** Following jobs assigned ****"
jobs -l
echo "*********************************"
wait
echo "Done with ubifs images."
if [ "$trellis" == "Y" ]; then
        mkdir -p ${imgDir}/images/${Chip}/${ChipSet}/stb
        build_trellis
fi
echo "****Following jobs assigned *****"
jobs -l
echo "*********************************"
wait

if [ "$cleanall" == "Y" ]; then
	echo "Removing up images and builds directory"
	rm -rf images builds
fi

echo "Post build cleanup."
echo "rm -f ${BUILD_DATA}/rg_bcm*tgz"
rm -f ${BUILD_DATA}/rg_bcm*tgz

echo "Completed all builds."
echo "You can find images under images directory"
echo "You can find build logs under ${Chip}/${ChipSet}/builds/build_logs directory"
