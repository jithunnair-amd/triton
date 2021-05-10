# pytest --verbose python/test
# pytest python/test/test_blocksparse.py 

# export TRITON_LIBHIP=/opt/rocm/lib/libamdhip64.so
pytest --verbose python/test/test_blocksparse.py::test_matmul[sdd-False-False-16-float16]