import torch
import triton.language as tl
import triton


@triton.jit
def _empty(X):
    pid = tl.program_id(0)


def empty(x):
    # The SPMD launch grid denotes the number of kernel instances that should execute in parallel.
    # It is analogous to CUDA launch grids. It can be either Tuple[int], or Callable(metaparameters) -> Tuple[int]
    def grid(meta): return (triton.cdiv(N, meta['BLOCK']), )
    # NOTE:
    #  - torch.tensor objects are implicitly converted to pointers to their first element.
    #  - `triton.jit`'ed functions can be subscripted with a launch grid to obtain a callable GPU kernel
    #  - don't forget to pass meta-parameters as keywords arguments
    _empty[grid](x, BLOCK=1024)
    # We return a handle to z but, since `torch.cuda.synchronize()` hasn't been called, the kernel is still
    # running asynchronously.



def test_op():
    torch.manual_seed(0)
    size = 7
    x = torch.rand(size, device='cuda')

    za = empty(x)
    print(za)


if __name__ == "__main__":
    # execute only if run as a script
    test_op()
