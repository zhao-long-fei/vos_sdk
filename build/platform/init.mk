#export LD_LIBRARY_PATH=/home/aispeech/workspace/aimake/build_env/Rockchip3308_32bit/host/lib
TOOLCHAIN_HOME = /home/qiudong/cross/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-

#TOOLCHAIN_HOME = /home/cuke/work/aimake/aimake_wsl/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
CC  = ${TOOLCHAIN_HOME}gcc
LD  = ${TOOLCHAIN_HOME}ld
CPP = ${TOOLCHAIN_HOME}cpp
CXX = ${TOOLCHAIN_HOME}g++
AR  = ${TOOLCHAIN_HOME}ar
AS  = ${TOOLCHAIN_HOME}as
NM  = ${TOOLCHAIN_HOME}nm
STRIP = ${TOOLCHAIN_HOME}strip

CFLAGS   := #-fPIC -pipe
CXXFLAGS := $(CFLAGS)
LDFLAGS :=


#
# explict rules
#
%.o : %.c
	$(CC) $(LOCAL_CFLAGS) $(CFLAGS) -c $< -o $@

%.o : %.cc
	$(CXX) $(LOCAL_CXXFLAGS) $(CXXFLAGS) -c $< -o $@

%.o : %.cpp
	$(CXX) $(LOCAL_CXXFLAGS) $(CXXFLAGS) -c $< -o $@


#
# building targets
#
EXECUTABLE = $(LOCAL_MODULE)
SHARED_LIBRARY  = lib$(LOCAL_MODULE).so
STATIC_LIBRARY  = lib$(LOCAL_MODULE).a
PACKAGE  = $(shell basename .t/$(LOCAL_MODULE))-$(shell cat /etc/issue | head -n 1 | awk '{os=tolower($$1);for(i=2;i<=NF;i++){if(match($$i,/[0-9\.]+/)){os=os"_"$$i;break;}};printf(os);}')-$(shell uname -m)-$(VERSION)-$(TIMESTAMP).tar.gz
