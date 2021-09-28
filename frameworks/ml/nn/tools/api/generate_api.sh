#!/bin/bash -u

if [[ -z "${ANDROID_BUILD_TOP:-}" ]] ; then
  echo >&2 "*** ERROR: $(basename $0) requires envar ANDROID_BUILD_TOP to be set"
  exit 1
fi

DRYRUN=""
MODE="update"
while [[ $# -ne 0 ]] ; do
  case "${1}" in
    --dryrun)
      DRYRUN="echo"
      shift
      ;;
    --mode=*)
      MODE=${1#"--mode="}
      shift
      ;;
    *)
      echo >&2 "*** USAGE: $(basename $0) [--dryrun] [--mode={update|ndk_hook}]"
      exit 1
      ;;
  esac
done

TOOL=$(dirname $0)/generate_api.py
SPECFILE=$(dirname $0)/types.spec
HALDIR=${ANDROID_BUILD_TOP}/hardware/interfaces/neuralnetworks
NDKDIR=${ANDROID_BUILD_TOP}/frameworks/ml/nn/runtime/include

RET=0
function doit {
  typeset -r kind="$1" in="$2" out="$3"
  echo "=== $kind"
  ${DRYRUN} ${TOOL} --kind ${kind} --specification ${SPECFILE} --template ${in} --out ${out}
  if [[ $? -ne 0 ]] ; then RET=1 ; fi
}

case "${MODE}" in
  update)
    doit ndk $(dirname $0)/NeuralNetworks.t ${NDKDIR}/NeuralNetworks.h
    doit hal_1.0 ${HALDIR}/1.0/types.t ${HALDIR}/1.0/types.hal
    doit hal_1.1 ${HALDIR}/1.1/types.t ${HALDIR}/1.1/types.hal
    doit hal_1.2 ${HALDIR}/1.2/types.t ${HALDIR}/1.2/types.hal
    doit hal_1.3 ${HALDIR}/1.3/types.t ${HALDIR}/1.3/types.hal
    ;;
  ndk_hook)
    TEMPDIR=$(mktemp -d)
    doit ndk $(dirname $0)/NeuralNetworks.t ${TEMPDIR}/NeuralNetworks.h
    if [[ ${RET} -eq 0 ]] ; then
      ${DRYRUN} cmp -s ${NDKDIR}/NeuralNetworks.h ${TEMPDIR}/NeuralNetworks.h || {
        RET=1
        echo >&2 "Error: NeuralNetworks.h is out of sync with NeuralNetworks.t or types.spec. Please run generate_api.sh before uploading."
      }
    fi
    ;;
  *)
    echo >&2 "*** Unknown mode: ${MODE}"
    exit 1
    ;;
esac

if [[ ${RET} -ne 0 ]] ; then
  echo >&2 "*** FAILED"
fi
exit ${RET}
