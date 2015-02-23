#!/bin/bash
# Script para compilar y empaquetar kernel.
# Creado por elitemovil.
# Modificado y traducido por r0i.
# visita www.darksideteam.com.

# Colores
amarillo='\033[01;33m'
verde='\033[01;32m'
rojo='\033[01;31m'
blink_rojo='\033[05;31m'
blanco='\033[0m'
restore='\033[0m'

clear
echo
echo
if [ -f release/release-ext4/*.zip ]
then
rm -f release/release-ext4/*.zip
fi
if [ -f release/release-ext4/*.md5 ]
then
rm -f release/release-ext4/*.md5
fi
if [ -f release/release-ext4/boot.img ]
then
rm -f release/release-ext4/boot.img
fi
if [ -f release/release-f2fs/*.zip ]
then
rm -f release/release-f2fs/*.zip
fi
if [ -f release/release-f2fs/*.md5 ]
then
rm -f release/release-f2fs/*.md5
fi
if [ -f release/release-f2fs/boot.img ]
then
rm -f release/release-f2fs/boot.img
fi
if [ -f out/*.zip ]
then
rm -f out/*.zip
fi
if [ -f out/zImage ]
then
rm -f out/zImage
fi
if [ -f out/*.gz ]
then
rm -f out/*.gz
fi
if [ -f out/*.img ]
then
rm -f out/*.img
fi
if [ -f temp_boot/*.img ]
then
rm -f temp_boot/*.img
fi
echo
echo
echo -e "${rojo}"
echo 'Script de compilacion para Kernel'
echo
echo '     Creado por @elitemovil      '
echo
echo ' Modificado y traducido por @r0i '
echo
echo '   Visita www.darksideteam.com   '
echo
echo -e "${verde}"
echo
echo "Opcion 1 = Compilar kernel"
echo "Opcion 2 = Limpiar compilacion anterior"
echo "Opcion 3 = Salir"
echo
echo
echo -e "${blanco}" 
PS3='Elige la opcion: '
options=("Opcion 1" "Opcion 2" "Opcion 3")
select opt in "${options[@]}"
do
    case $opt in
        "Opcion 2")
	    echo
	    echo
            echo "Elimininando compilacion"
	    echo
	    echo
	make mrproper
echo
echo
echo "Compilacion eliminada"
clear
echo "Opcion 1 = Compilar kernel"
echo "Opcion 2 = Limpiar compilacion anterior"
echo "Opcion 3 = Salir"
echo
echo
            ;;
        "Opcion 1")
            echo "Has elegido compilar el kernel"
echo
echo
clear
echo -e "${verde}"
echo
echo "La compilacion a comenzada a las:"
date
echo -e "${blanco}"
echo
echo
if [ -f release/release-ext4/*.zip ]
then
rm -f release/release-ext4/*.zip
fi
if [ -f release/release-ext4/*.md5 ]
then
rm -f release/release-ext4/*.md5
fi
if [ -f release/release-ext4/boot.img ]
then
rm -f release/release-ext4/boot.img
fi
if [ -f release/release-f2fs/*.zip ]
then
rm -f release/release-f2fs/*.zip
fi
if [ -f release/release-f2fs/*.md5 ]
then
rm -f release/release-f2fs/*.md5
fi
if [ -f release/release-f2fs/boot.img ]
then
rm -f release/release-f2fs/boot.img
fi
if [ -f out/*.zip ]
then
rm -f out/*.zip
fi
if [ -f out/*.gz ]
then
rm -f out/*.gz
fi
if [ -f out/*.img ]
then
rm -f out/*.img
fi
if [ -f out/zImage ]
then
rm -f out/zImage
fi
if [ -f temp_boot/*.img ]
then
rm -f temp_boot/*.img
fi
DATE_START=$(date +"%s")
export ARCH=arm
export SUBARCH=arm
export KBUILD_BUILD_USER="r0i"
export KBUILD_BUILD_HOST="darksideteam.com"
export CROSS_COMPILE="/home/r0i/Android/kernel/toolchains/SaberNaro4.9/bin/arm-eabi-"
export KERNELDIR="/home/r0i/Android/kernel/bacon/"
echo "CROSS_COMPILE="$CROSS_COMPILE
echo "ARCH="$ARCH
echo "SUBARCH="$SUBARCH
echo "KBUILD_BUILD_USER="$KBUILD_BUILD_USER
echo "KBUILD_BUILD_HOST="$KBUILD_BUILD_HOST
make darknel_bacon_defconfig
export NKERNEL="-DarkNel"
export BUILDNO="_OPO_CM12_rev.3.v.0.0.6"
export BUILDNOF2FS="_OPO_CM12_F2FS_rev.3.v.0.0.6"
export ZIPKERNEL="DarkNel_Kernel"
export LOCALVERSION=$NKERNEL$BUILDNO
make -j4
echo
echo
echo -e "${verde}"
echo "Creando version EXT4"
echo
echo -e "${blanco}"
echo
echo "Creando modules..."
echo
echo -e "${verde}"
# make modules and move to out dir
make modules -j4
for i in $(find "$KERNELDIR" -name '*.ko'); do
        cp -av "$i" "$KERNELDIR"release/release-ext4/system/lib/modules;
done;
echo -e "${blanco}"
echo
echo "Creando dt.img..."
echo
echo -e "${verde}"
"$KERNELDIR"opo_tools/dtbToolCM -2 -o out/dt.img -s 2048 -p "$KERNELDIR"scripts/dtc/ "$KERNELDIR"arch/arm/boot/
echo
echo
echo -e "${blanco}"
echo
echo "Comprimiendo ramdisk..."
echo
echo -e "${verde}"
"$KERNELDIR"opo_tools/mkbootfs "$KERNELDIR"ramdisk/ramdisk-ext4 | gzip > ramdisk.gz 2>/dev/null
cp ramdisk.gz "$KERNELDIR"out/
echo
echo -e "${blanco}"
echo
echo "Creando boot.img..."
echo
echo -e "${verde}"
echo
cp arch/arm/boot/zImage "$KERNELDIR"out/
"$KERNELDIR"opo_tools/mkbootimg --kernel "$KERNELDIR"out/zImage --ramdisk "$KERNELDIR"out/initramfs.img --cmdline "console=ttyHSL0,115200,n8 androidboot.hardware=bacon user_debug=31 msm_rtb.filter=0x3F ehci-hcd.park=3 androidboot.selinux=permissive" --base 0x00000000 --pagesize 2048 --ramdisk_offset 0x02000000 --tags_offset 0x01e00000 --dt "$KERNELDIR"out/dt.img -o "$KERNELDIR"temp_boot/boot.img
echo
echo
echo -e "${blanco}"
echo
echo "Copiando kernel al directorio de salida..."
cp "$KERNELDIR"temp_boot/boot.img "$KERNELDIR"release/release-ext4/boot.img
echo
echo
echo
cd release
echo "Creando el zip..."
cd "$KERNELDIR"release/release-ext4
zip -r $ZIPKERNEL$BUILDNO.zip *
cd ..
echo
echo
echo
echo "Firmando el zip..."
mv "$KERNELDIR"release/release-ext4/*.zip "$KERNELDIR"opo_tools/sign
cd "$KERNELDIR"opo_tools/sign
java -jar signapk.jar testkey.x509.pem testkey.pk8 $ZIPKERNEL$BUILDNO.zip $ZIPKERNEL$BUILDNO-signed.zip
rm -f $ZIPKERNEL$BUILDNO.zip
cd ..
cd ..
mv "$KERNELDIR"opo_tools/sign/*.zip release/release-ext4
cd "$KERNELDIR"release/release-ext4
md5sum *.zip > $ZIPKERNEL$BUILDNO-signed.md5
cd ..
echo -e "${verde}"
echo
echo "Version EXT4 lista"
echo
echo "Creando version F2FS"
echo
if [ -f temp_boot/*.img ]
then
rm -f temp_boot/*.img
fi
if [ -f out/*.img ]
then
rm -f out/*.img
fi
echo
echo
echo -e "${blanco}"
echo
echo "Creando modules..."
echo
echo -e "${verde}"
cp "$KERNELDIR"release/release-ext4/system/lib/modules/*.ko "$KERNELDIR"release/release-f2fs/system/lib/modules
echo
echo -e "${blanco}"
echo
echo "Comprimiendo ramdisk..."
echo
echo -e "${verde}"
"$KERNELDIR"opo_tools/mkbootfs "$KERNELDIR"ramdisk/ramdisk-f2fs | gzip > ramdisk.gz 2>/dev/null
cp ramdisk.gz "$KERNELDIR"out/
echo
echo -e "${blanco}"
echo
echo "Creando boot.img..."
echo
echo -e "${verde}"
echo
"$KERNELDIR"opo_tools/mkbootimg --kernel "$KERNELDIR"out/zImage --ramdisk "$KERNELDIR"out/initramfs.img --cmdline "console=ttyHSL0,115200,n8 androidboot.hardware=bacon user_debug=31 msm_rtb.filter=0x3F ehci-hcd.park=3 androidboot.selinux=permissive" --base 0x00000000 --pagesize 2048 --ramdisk_offset 0x02000000 --tags_offset 0x01e00000 --dt "$KERNELDIR"out/dt.img -o "$KERNELDIR"temp_boot/boot.img
echo -e "${blanco}"
echo
echo "Copiando kernel al directorio de salida..."
cp "$KERNELDIR"temp_boot/boot.img "$KERNELDIR"release/release-f2fs/boot.img
echo
echo
echo
echo "Creando el zip..."
cd "$KERNELDIR"release/release-f2fs
zip -r $ZIPKERNEL$BUILDNOF2FS.zip *
cd ..
echo
echo
echo
echo "Firmando el zip..."
mv "$KERNELDIR"release/release-f2fs/*.zip "$KERNELDIR"opo_tools/sign
cd "$KERNELDIR"opo_tools/sign
java -jar signapk.jar testkey.x509.pem testkey.pk8 $ZIPKERNEL$BUILDNOF2FS.zip $ZIPKERNEL$BUILDNOF2FS-signed.zip
rm -f $ZIPKERNEL$BUILDNOF2FS.zip
cd ..
cd ..
mv "$KERNELDIR"opo_tools/sign/*.zip release/release-f2fs
cd "$KERNELDIR"release/release-f2fs
md5sum *.zip > $ZIPKERNEL$BUILDNOF2FS-signed.md5
cd ..
echo
echo
echo -e "${verde}"
echo
echo "Version F2FS lista"
echo
echo -e "${rojo}"
echo
echo "Compilacion acabada a las:"
date
DATE_END=$(date +"%s")
echo
DIFF=$(($DATE_END - $DATE_START))
echo "Compilacion completada en $(($DIFF / 60)) minuto(s) and $(($DIFF % 60)) segundo(s)."
echo
echo -e "${verde}"
echo
echo "Opcion 1 = Compilar kernel"
echo "Opcion 2 = Limpiar compilacion anterior"
echo "Opcion 3 = Salir"
echo
echo
            ;;
        "Opcion 3")
echo
echo
echo -e "${red}"
            echo "Adios!!!"
echo
echo
echo -e "${restore}"
            exit
            ;;
    esac
done
