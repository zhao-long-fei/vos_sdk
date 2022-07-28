# !/bin/sh
show_usage()
{
echo "==============================================================="
echo "	sh	pack.sh"
echo "==============================================================="
}

show_usage

read -p $' Please make sure your project has built success!!!\x0a Please make sure current directory is NOT your working copy!!!\x0a  Since this script will REMOVE some local files!\x0a  =========== Are you sure to continue? [y/n] ' KEEP_GOING
case $KEEP_GOING  in
        [yY]*)
                echo "!!!!!!!!!!!!! Go !!!!!!!!!!!!"
                ;;
        [nN]*)
                exit
                ;;
        *)
                echo "Just enter y or n, please."
                exit
                ;;
esac

echo "Step 1:  Check git status ..."
git_status=`git status -s | awk -F " " '{print $1}'`
for f in $git_status
do	
	if [ "${f}" != "??" ]; then
		echo "Please make sure your modification has commit !"
		echo "Packing abort !"
		exit 1
	fi
done

echo "Step 2:  ar ..."
PROJ_ROOT=`pwd`
libs="player ghost player_manager"
OUT_MDW_DIR="${PROJ_ROOT}/component/aispeech"
MDW_DIR="${PROJ_ROOT}/component/aispeech"
AR_BIN="/home/aispeech/workspace/aimake/build_env/rk3126_32_s/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-ar"
for ais_mw in $libs
do
	o_f=`find ${OUT_MDW_DIR}/${ais_mw} -type f -name "*.o"`	
	if [ "${o_f}" != "" ];then
		${AR_BIN} -rcs lib${ais_mw}.a ${o_f}
	fi
	if [ ! -f "lib${ais_mw}.a" ];then
		echo "AR ${ais_mw} fail"
	else
		echo "AR ${ais_mw} success"
		mv lib${ais_mw}.a ${MDW_DIR}/${ais_mw}
	fi
	src_c_f=`find ${MDW_DIR}/${ais_mw} -type f -name "*.c"`
	rm -rf ${src_c_f}
	src_cpp_f=`find ${MDW_DIR}/${ais_mw} -type f -name "*.cpp"`
	rm -rf ${src_cpp_f}	
done

echo "AR all files done"
