export TRITON_LIBHIP=/opt/rocm/lib/libamdhip64.so

# pytest python/test
# pytest python/test/test_blocksparse.py 

pytest --verbose python/test/test_conv.py
# pytest --verbose python/test/test_blocksparse.py::test_matmul[sdd-False-False-16-float16]
# pytest --verbose python/test/test_blocksparse.py::test_attention_fwd_bwd