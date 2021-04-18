#!/bin/bash

# Stops on first error, echo on
set -e
set -x

# Allow variable core usage
# default uses all cpu cores
#
if [ -f /usr/bin/nproc ]; then
    num_cpus=$(nproc)
elif [ -f /usr/sbin/sysctl ]; then
    num_cpus=$(sysctl -n hw.logicalcpu)
else
    num_cpus=2
fi
: "${CORES:=$num_cpus}"

# If no build target, exit
#: "${FLAVOR:=ALL}"

for i in "$@"
do
case $i in
    --jobs=*)
      CORES="${i#*=}"
      shift
      ;;
    -j*)
      CORES="${i#*j}"
      shift
      ;;
    -Wno-error)
      WERROR=0
      shift
      ;;
    -b*)
      FLAVOR="${i#*b}"
      shift
      ;;
esac
done

# Add GCC_ARM to PATH
if [[ -n ${GCC_ARM} ]] ; then
  export PATH=${GCC_ARM}:$PATH
fi

: ${SRCDIR:=$(dirname $(pwd)/"$0")/..}

: ${BUILD_TYPE:=Debug}
: ${COMMON_OPTIONS:="-DCMAKE_BUILD_TYPE=$BUILD_TYPE -Wno-dev "}
if (( $WERROR )); then COMMON_OPTIONS+=" -DWARNINGS_AS_ERRORS=YES "; fi

: ${EXTRA_OPTIONS:="$EXTRA_OPTIONS"}

COMMON_OPTIONS+=${EXTRA_OPTIONS}

: ${FIRMARE_TARGET:="firmware-size"}

# wipe build directory clean
rm -rf build && mkdir -p build && cd build

target_names=$(echo "$FLAVOR" | tr '[:upper:]' '[:lower:]' | tr ';' '\n')

if [[ " T16 COLORLCD ALL " =~ \ ${FLAVOR}\  ]] ; then
  # OpenTX on T16 boards
   rm -rf ./* || true
  cmake "${COMMON_OPTIONS}" -DPCB=X10 -DPCBREV=T16 -DHELI=YES -DLUA=YES -DGVARS=YES -DAFHDS3=NO "${SRCDIR}"
  make -j"${CORES}" ${FIRMARE_TARGET}
  make -j"${CORES}" libsimulator
  make -j"${CORES}" tests-radio
fi

if [[ " T18 COLORLCD ALL " =~ \ ${FLAVOR}\  ]] ; then
  # OpenTX on T16 boards
   rm -rf ./* || true
  cmake "${COMMON_OPTIONS}" -DPCB=X10 -DPCBREV=T18 -DHELI=YES -DLUA=YES -DGVARS=YES -DAFHDS3=NO "${SRCDIR}"
  make -j"${CORES}" ${FIRMARE_TARGET}
  make -j"${CORES}" libsimulator
  make -j"${CORES}" tests-radio
fi

if [[ " TX16S COLORLCD ALL " =~ \ ${FLAVOR}\  ]] ; then
  # OpenTX on TX16S boards
   rm -rf ./* || true
  cmake "${COMMON_OPTIONS}" -DPCB=X10 -DPCBREV=TX16S -DHELI=YES -DLUA=YES -DGVARS=YES -DTRANSLATIONS=CN "${SRCDIR}"
  make -j"${CORES}" ${FIRMARE_TARGET}
  make -j"${CORES}" libsimulator
  make -j"${CORES}" tests-radio
fi

if [[ " X12S COLORLCD ALL " =~ \ ${FLAVOR}\  ]] ; then
  # OpenTX on X12S
   rm -rf ./* || true
  cmake "${COMMON_OPTIONS}" -DPCB=X12S -DHELI=YES -DLUA=YES -DGVARS=YES -DMULTIMODULE=NO "${SRCDIR}"
  make -j"${CORES}" ${FIRMARE_TARGET}
  make -j"${CORES}" libsimulator
  make -j"${CORES}" tests-radio
fi

if [[ " NV14 COLORLCD ALL " =~ \ ${FLAVOR}\  ]] ; then
  rm -rf ./* || true
  cmake "${COMMON_OPTIONS}" -DPCB=NV14 "${SRCDIR}"
  make -j"${CORES}" ${FIRMARE_TARGET}
  make -j"${CORES}" libsimulator
  #TODO: make -j"${CORES}" tests-radio
fi

if [[ " COMPANION ALL " =~ \ ${FLAVOR}\  ]] ; then
  # Companion
   rm -rf ./* || true
  cmake "${COMMON_OPTIONS}" "${SRCDIR}"
  make -j"${CORES}" companion24 simulator24
  make -j"${CORES}" tests-companion
fi

if [[ " YAML ALL " =~ \ ${FLAVOR}\  ]] ; then
  # OpenTX on TX16S boards
  rm -rf ./* || true
  cmake "${COMMON_OPTIONS}" -DPCB=X10 -DPCBREV=TX16S -DHELI=YES -DLUA=YES -DGVARS=YES -DYAML_STORAGE=ON "${SRCDIR}"
  make -j"${CORES}" yaml_data
  make -j"${CORES}" ${FIRMARE_TARGET}
  make -j"${CORES}" tests-radio
fi
