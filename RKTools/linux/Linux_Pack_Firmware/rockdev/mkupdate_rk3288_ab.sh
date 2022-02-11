#!/bin/bash
pause()
{
echo "Press any key to quit:"
read -n1 -s key
exit 1
}
echo "start to make update.img..."
if [ ! -f "Image/parameter.txt" ]; then
	echo "Error:No found parameter!"
#	pause
fi
if [ ! -f "package-file-ab" ]; then
	echo "Error:No found package-file!"
#	pause
fi
./afptool -pack ./ Image/update.img ./package-file-ab || pause
./rkImageMaker -RK32 Image/MiniLoaderAll.bin Image/update.img update.img -os_type:androidos || pause
echo "Making update.img OK."
#echo "Press any key to quit:"
#read -n1 -s key
exit 0
