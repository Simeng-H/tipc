# get first argument: path to the .tip file
TIP_FILE=$1

# if no .tip file is given, use the default
if [ -z "$TIP_FILE" ]
then
    TIP_FILE=/Users/simeng/local_dev/Compilers/tip_programs/free.tip
fi

# get dir of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# compile the .tip file
$SCRIPT_DIR/../build/src/tipc -do $TIP_FILE

# disassemble the .bc file
llvm-dis $TIP_FILE.bc
