# get first argument: path to the bc file
BC_FILE=$1

# if no bc file is given, use the default
if [ -z "$BC_FILE" ]
then
    BC_FILE=/Users/simeng/local_dev/Compilers/tip_programs/free.tip.bc
fi

# get dir of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# get dir of mspass dylib
MSPASS_DIR=$SCRIPT_DIR/build/src/memsafetypass/mspass.dylib 

# run the pass on the bc file
opt -enable-new-pm=0 -load $MSPASS_DIR --mspass < $BC_FILE >/dev/null
