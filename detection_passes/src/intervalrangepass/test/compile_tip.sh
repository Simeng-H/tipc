# iterate over all tip files in the current directory and print
for f in /Users/simeng/local_dev/Compilers/tipc-passes/src/intervalrangepass/test/interval*.tip
do
    tipc -do $f
    llvm-dis $f.bc
done
