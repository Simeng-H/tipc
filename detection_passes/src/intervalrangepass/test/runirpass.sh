tipc -do $1.tip
llvm-dis $1.tip.bc
opt -enable-new-pm=0 -load ~/local_dev/Compilers/tipc-passes/build/src/intervalrangepass/irpass.dylib -mem2reg -irpass < $1.tip.bc >/dev/null 2>$1.irpass
