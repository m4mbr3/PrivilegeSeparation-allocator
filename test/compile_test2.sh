#!/bin/bash
FILECPP="test_lib2.c"
FILEBC="test_lib2.bc"
FILEBC1="test_lib2.bc1"
FILEBC2="test_lib2.bc2"
FILEBC3="test_lib2.bc3"
FILEBC4="test_lib2.bc4"
FILEOBJ="test_lib2.o"
FILEEXE="test2"
echo "$LLVM_ROOT/bin/clang -emit-llvm -c $FILECPP -o $FILEBC -I/root/privsep_malloc/include"
$LLVM_ROOT/bin/clang -g -emit-llvm -c $FILECPP -o $FILEBC -I/root/privsep_malloc/include 
$LLVM_ROOT/bin/llvm-link  $FILEBC -o $FILEBC1 
$LLVM_ROOT/bin/opt $FILEBC1 -o $FILEBC2 -TaggingPropagation 
$LLVM_ROOT/bin/opt $FILEBC2 -o $FILEBC3 -SysCallInsertion 
$LLVM_ROOT/bin/opt $FILEBC3 -o $FILEBC4 -PrivilegeSeparationOnModule 
$LLVM_ROOT/bin/llc -filetype=obj $FILEBC4 -o $FILEOBJ 
echo "$LLVM_ROOT/bin/clang -g $FILEOBJ -o $FILEEXE -L/root/privsep_malloc/lib -lprivsep_malloc"
$LLVM_ROOT/bin/clang -g $FILEOBJ -o $FILEEXE -L/root/privsep_malloc/lib -lprivsep_malloc
$LLVM_ROOT/bin/clang -g -Xlinker "-Tps_link_script.ld"  $FILEOBJ -o $FILEEXE  -L/root/privsep_malloc/lib -lprivsep_malloc
