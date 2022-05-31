#!/bin/bash

echo "to compile and run you need to set up MPI, read this .sh to see usage"

mpicc -g -I/usr/lib/x86_64-linux-gnu/openmpi/include/openmpi -o $2 $2.c

mpiexec -n $1 --use-hwthread-cpus $2
