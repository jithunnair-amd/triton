#include "triton/codegen/pass.h"
#include "triton/codegen/analysis/align.h"
#include "triton/codegen/analysis/allocation.h"
#include "triton/codegen/analysis/axes.h"
#include "triton/codegen/analysis/liveness.h"
#include "triton/codegen/analysis/swizzle.h"
#include "triton/codegen/selection/generator.h"
#include "triton/codegen/transform/coalesce.h"
#include "triton/codegen/transform/cts.h"
#include "triton/codegen/transform/dce.h"
#include "triton/codegen/transform/disassociate.h"
#include "triton/codegen/transform/membar.h"
#include "triton/codegen/transform/peephole.h"
#include "triton/codegen/transform/pipeline.h"
#include "triton/codegen/transform/prefetch.h"
#include "triton/driver/device.h"
#include "triton/driver/kernel.h"
#include "triton/driver/module.h"
#include "triton/ir/function.h"
#include "triton/ir/module.h"
#include "triton/ir/print.h"
#include "llvm/IR/Module.h"

namespace triton {
namespace codegen {

// TODO:
// There should be a proper pass manager there!
void add_passes_to_emit_bin(ir::module &ir, driver::device *dev, int num_warps,
                            driver::module *&mod, driver::kernel *&ker, size_t &shared_mem) {
  // generate llvm code
  llvm::LLVMContext ctx;
  std::string name = ir.get_function_list()[0]->get_name();
  std::unique_ptr<llvm::Module> llvm(new llvm::Module(name, ctx));
  // optimizations
  std::unique_ptr<codegen::target> target = dev->make_target();
  bool cts_use_async = target->as_nvidia()->sm() >= 80;
  // create passes
  codegen::analysis::align align;
  codegen::analysis::axes axes;
  codegen::transform::cts cts(cts_use_async);
  codegen::transform::pipeline pipeline(cts_use_async);
  codegen::transform::disassociate disassociate;
  codegen::analysis::layouts layouts(&axes, &align, num_warps, target.get());
  codegen::analysis::liveness liveness(&layouts);
  codegen::analysis::swizzle swizzle(&layouts, target.get());
  codegen::analysis::allocation allocation(&liveness);
  codegen::transform::membar barriers(&liveness, &layouts, &allocation, target.get());
  codegen::transform::dce dce;
  codegen::transform::peephole peephole(target.get(), &layouts);
//  codegen::transform::reassociate reassociate;
  codegen::transform::coalesce coalesce(&align, &layouts);
  codegen::transform::prefetch prefetch_s(target.get());
  codegen::generator isel(&axes, &layouts, &align, &allocation, &swizzle, target.get(), num_warps);
  // run passes
  dce.run(ir);
  peephole.run(ir);
  dce.run(ir);
  pipeline.run(ir);
  dce.run(ir);
  //ir::print(ir, std::cout);
  disassociate.run(ir);
  dce.run(ir);
  align.run(ir);
  axes.run(ir);
  layouts.run(ir);
  peephole.run(ir);
  dce.run(ir);
  if (target->is_gpu())
    cts.run(ir);
  align.run(ir);
  axes.run(ir);
  layouts.run(ir);
  coalesce.run(ir);
  dce.run(ir);
  align.run(ir);
  dce.run(ir);
  if (target->is_gpu()) {
//    reassociate.run(ir);
    cts.run(ir);
  }
  dce.run(ir);
  align.run(ir);
  axes.run(ir);
  layouts.run(ir);
  peephole.run(ir);
  dce.run(ir);
  align.run(ir);
  axes.run(ir);
  layouts.run(ir);
  swizzle.run(ir);
  liveness.run(ir);
  allocation.run(ir);
  prefetch_s.run(ir);
  barriers.run(ir);
  // ir::print(ir, std::cout);
  isel.visit(ir, *llvm);
  mod = driver::module::create(dev, std::move(llvm));
  ker = driver::kernel::create(&*mod, name.c_str());
  shared_mem = allocation.allocated_size();
}

} // namespace codegen
} // namespace triton
