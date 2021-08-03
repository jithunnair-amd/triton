set -x
rm -rf core
rm -rf ptx.hip
rm -rf python/build/
rm -rf python/test/__pycache__/
rm -rf python/triton.egg-info/
rm -rf python/triton/_C/libtriton.so
rm -rf python/triton/__pycache__/
rm -rf python/triton/ops/__pycache__/
rm -rf python/triton/ops/blocksparse/__pycache__/
rm -rf *.asm
rm -rf *.gcn
rm -rf *.ir
rm -rf *.o
rm -rf amdgcn
rm -rf *.hsaco
rm -rf *.out
