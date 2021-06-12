#include <string>
#include <mutex>
#include <regex>
#include <functional>
#include <algorithm>
#include <sstream>
#include <memory>
#include "triton/codegen/analysis/axes.h"
#include "triton/codegen/analysis/allocation.h"
#include "triton/codegen/analysis/liveness.h"
#include "triton/codegen/analysis/align.h"
#include "triton/codegen/analysis/swizzle.h"
#include "triton/codegen/transform/coalesce.h"
#include "triton/codegen/transform/dce.h"
#include "triton/codegen/transform/peephole.h"
#include "triton/codegen/transform/membar.h"
#include "triton/codegen/transform/reassociate.h"
#include "triton/codegen/transform/cts.h"
#include "triton/codegen/transform/disassociate.h"
#include "triton/codegen/selection/generator.h"
#include "triton/codegen/transform/pipeline.h"
#include "triton/runtime/function.h"
#include "triton/lang/cpp.h"
#include "triton/lang/parser.h"
#include "triton/lang/code_gen.h"
#include "triton/driver/device_hip.h"
#include "triton/driver/stream_hip.h"
#include "triton/driver/kernel_hip.h"
#include "triton/driver/module_hip.h"
#include "triton/driver/error_hip.h"
#include "triton/ir/module.h"
#include "triton/ir/function.h"
#include "triton/ir/print.h"
#include "triton/runtime/error.h"
#include "triton/tools/bench_hip.hpp"
#include "triton/tools/sha1.hpp"
#include "triton/tools/sys/getenv.hpp"
#include "triton/tools/sys/mkdir.hpp"
#include "llvm/IR/Module.h"
#include <mutex>
#include <fstream>


namespace triton{
namespace runtime {

/* --------------------------------- */
/* --------------------------------- */
/* --------------------------------- */

std::shared_ptr<ir::module> kernel::src_to_ir(const std::string& _src, const options_t& opt) {
  std::string src =
R"(
#define bool _Bool
#define true 1
#define false 0

#define __readonly      __attribute__((readonly))
#define __writeonly     __attribute__((writeonly))
#define __noalias       __attribute__((noalias))
#define __aligned(A)    __attribute__((aligned(A)))
#define __multipleof(A) __attribute__((multipleof(A)))
#define __retune        __attribute__((retune))

#define F32_INFINITY bitcast<float>(0x7F800000)
#define F16_INFINITY bitcast<half>((int16)0x7C00)

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

#define PASTER(a, b, _) a ## _ ## b
#define EVALUATOR(a, b, _)  PASTER(a, b, _)
#define atomic_add(TYPE, TM, TN) EVALUATOR(atomic_add, EVALUATOR(TYPE, EVALUATOR(TM, TN, x), _), _)
#define DECLARATION(TYPE, TM, TN) extern void atomic_add(TYPE, TM, TN)(TYPE*[TM, TN], TYPE[TM, TN], bool[TM, TN])

DECLARATION(float, 64, 64);
DECLARATION(float, 64, 128);
DECLARATION(float, 128, 64);
DECLARATION(float, 128, 128);
extern void atomic_add_half_1x1(half*, half, bool);

DECLARATION(half , 64, 64);
DECLARATION(half , 64, 128);
DECLARATION(half , 128, 64);
DECLARATION(half , 128, 128);
extern void atomic_add_float_1x1(float*, float, bool);

extern int atomic_cas(int*, int, int);
extern int atomic_xchg(int*, int);
extern int get_program_id(int);
extern void __debug_barrier();
extern int get_num_programs(int);
extern int select(bool, int, int);
extern char __constant__ * calloc(int);

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;
typedef char int8;
typedef short int16;
typedef int int32;
typedef long int64;
)";
  src += _src;
  // pre-process
  TokenSequence tokens;
  Preprocessor cpp(&src, true);
  for(auto it: opt.defines)
    cpp.AddMacro(it.first, &it.second);
  cpp.Process(tokens);
  // src -> ast
  Parser parser(tokens);
  parser.Parse();
  // ast -> triton-ir
  auto ret = std::make_shared<ir::module>("");
  Generator gen(&parser);
  gen.Gen(&*ret);
  return ret;
}

std::tuple<std::shared_ptr<driver::module>,
           std::shared_ptr<driver::kernel>,
           size_t> kernel::ir_to_bin(ir::module &ir, driver::device* dev, const options_t& opt) {
  std::cout << "function_hip::kernel::driver" << std::endl;
  // generate llvm code
  llvm::LLVMContext ctx;
  std::string name = ir.get_function_list()[0]->get_name();
  std::unique_ptr<llvm::Module> llvm(new llvm::Module(name, ctx));
  // optimizations
  std::unique_ptr<codegen::target> target = dev->make_target();
  // bool cts_use_async = target->as_nvidia()->sm() >= 80;
  bool cts_use_async = false;
  // create passes
  codegen::analysis::align align;
  codegen::analysis::axes axes;
  codegen::transform::cts cts(cts_use_async);
  codegen::transform::pipeline pipeline(cts_use_async);
  codegen::transform::disassociate disassociate;
  codegen::analysis::layouts layouts(&axes, &align, opt.num_warps, target.get());
  codegen::analysis::liveness liveness(&layouts);
  codegen::analysis::swizzle swizzle(&layouts, target.get());
  codegen::analysis::allocation allocation(&liveness);
  codegen::transform::membar barriers(&liveness, &layouts, &allocation);
  codegen::transform::dce dce;
  codegen::transform::peephole peephole(target.get(), &layouts);
  codegen::transform::reassociate reassociate;
  codegen::transform::coalesce coalesce(&align, &layouts);
  codegen::generator isel(&axes, &layouts, &align, &allocation, &swizzle, target.get(), opt.num_warps);
  // run passes
  dce.run(ir);
  peephole.run(ir);
  dce.run(ir);
  pipeline.run(ir);
  dce.run(ir);
  disassociate.run(ir);
  dce.run(ir);
  align.run(ir);
  axes.run(ir);
  layouts.run(ir);
  peephole.run(ir);
  dce.run(ir);
  std::ofstream ir_out("conv_after_passes.ir");
  ir::print(ir, ir_out);
  // if(target->is_gpu())
  // {
  //   std::cout << "function_hip::kernel::driver: gpu check 1" << std::endl;
  //   cts.run(ir);
  // }
  align.run(ir);
  axes.run(ir);
  layouts.run(ir);
  coalesce.run(ir);
  dce.run(ir);
  align.run(ir);
  dce.run(ir);
  // if(target->is_gpu()){
  //   std::cout << "function_hip::kernel::driver: gpu check 2" << std::endl;
  //   reassociate.run(ir);
  //   cts.run(ir);
  // }
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
  barriers.run(ir);
  std::ofstream ir_out_2("conv_before_visit.ir");
  ir::print(ir, ir_out_2);
  isel.visit(ir, *llvm);
  std::ofstream ir_out_3("conv_after_visit.ir");
  ir::print(ir, ir_out_3);
  // llvm src call
  std::shared_ptr<driver::module> mod(driver::module::create(dev, std::move(llvm)));
  std::shared_ptr<driver::kernel> ker(driver::kernel::create(&*mod, name.c_str()));
  size_t shared_mem = allocation.allocated_size();
  return std::make_tuple(mod, ker, shared_mem);
}

kernel::kernel(const std::string& src, const options_t& opt, driver::device *dev, const std::map<int, ir::attribute> &attrs):
  opt(opt), dev_(dev) {
  // std::cout << "function_hip::kernel::kernel" << std::endl;
  // compile to Triton IR
  // std::cout << "function_hip::kernel::kernel: compile to Triton IR" << std::endl;
  ir_ = src_to_ir(src, opt);
  // add attributes
  // std::cout << "function_hip::kernel::kernel: add attributes" << std::endl;
  for(const auto&x: attrs)
    ir_->get_function_list()[0]->add_attr(x.first, x.second);
  
  // compile to binary
  // std::cout << "function_hip::kernel::kernel: compile to binary" << std::endl;
  std::tie(mod_, ker_, shared_mem_) = ir_to_bin(*ir_, dev, opt);
}

void kernel::operator()(const std::string& args, driver::stream *stream, const std::vector<size_t>& _grid) const{
  // set grid
  if(_grid.size() > 3)
    throw std::runtime_error("grid size must be no greater than 3");
  std::array<size_t, 3> grid;
  for(size_t i = 0; i < 3; i++)
    grid[i] = (i < _grid.size()) ? _grid[i] : 1;
  // enqueue
  stream->enqueue(&*ker_, grid, {(size_t)opt.num_warps * 32, 1, 1}, (void*)args.data(), args.size(), shared_mem_);
}

std::string kernel::get_asm(const std::string& mode) {
  std::cout << "function_hip::kernel::get_asm" << std::endl;
  std::vector<std::string> modes = {"llir", "ptx"};
  if(std::find(modes.begin(), modes.end(), mode) == modes.end()){
    std::string err = "Unrecognized mode. Supported values are: ";
    for(std::string m: modes){
      if(m != modes[0])
        err += ", ";
      err += m;
    }
    throw std::runtime_error(err);
  }
  if(mode == "llir")
      return ((driver::cu_module*)mod_.get())->llir();
  if(mode == "ptx")
      return ((driver::cu_module*)mod_.get())->ptx();
  assert(false);
  return "";
}
/* --------------------------------- */
/* --------------------------------- */
/* --------------------------------- */




function::function(const std::string& src, const options_t &opt, driver::device *device,
                   const std::vector<config> &tune_confs, const std::vector<std::string>& tune_key)
  : src_(src), device_(device) {
  // kernel options
  size_t num_opts = std::max(tune_confs.size(), (size_t)1);
  opts_ = std::vector<options_t>(num_opts, opt);
  for(size_t i = 0; i < tune_confs.size(); i++){
    opts_[i].defines.insert(tune_confs[i].defines.begin(), tune_confs[i].defines.end());
    opts_[i].num_warps = tune_confs[i].num_warps;
  }
  std::shared_ptr<ir::module> ir = kernel::src_to_ir(src, opts_[0]);
  std::vector<ir::argument*> args = ir->get_function_list()[0]->args();
  // signature
  auto convert = [](ir::type *ty) {
    if(ty->is_integer_ty(1))  return INT1_T;
    if(ty->is_integer_ty(8))  return INT8_T;
    if(ty->is_integer_ty(16)) return INT16_T;
    if(ty->is_integer_ty(32)) return INT32_T;
    if(ty->is_integer_ty(64)) return INT64_T;
    if(ty->is_half_ty())      return HALF_T;
    if(ty->is_float_ty())     return FLOAT_T;
    if(ty->is_double_ty())    return DOUBLE_T;
    if(ty->is_pointer_ty())   return BUFFER_T;
    throw std::runtime_error("unknown type");
  };
  for(ir::argument* arg: args)
    sig_.push_back(convert(arg->get_type()));
  // find indices of autotune keys
  for(const std::string& name: tune_key){
    auto pred = [&](ir::argument* arg) { return arg->get_name() == name; };
//    std::cout << "----" << std::endl;
//    for(ir::argument* arg: args)
//        std::cout << arg->get_name() << std::endl;
    auto it = std::find_if(args.begin(), args.end(), pred);
    if(it == args.end())
      throw std::runtime_error(name + " is not a valid argument name");
    key_idxs_.push_back(std::distance(args.begin(), it));
  }
  // find indices of pointer
  for(size_t i = 0; i < args.size(); i++)
    if(args[i]->get_type()->is_pointer_ty() ||
       args[i]->get_type()->is_integer_ty())
      align_idxs_.push_back(i);
  // argument size and offset
  size_t curr = 0;
  for(arg_type ty: sig_){
    arg_size_.push_back(size_of(ty));
    arg_off_.push_back(curr);
    curr += arg_size_.back();
  }
}

uint64_t pow2_divisor(uint64_t N){
  if(N % 16 == 0) return 16;
  if(N % 8 == 0) return 8;
  if(N % 4 == 0) return 4;
  if(N % 2 == 0) return 2;
  return 1;
}

kernel* function::autotune(const std::string &args, const grid_fn_ty& grid_fn, driver::stream* stream) {
  std::cout << "function_hip::autotune" << std::endl;
  
  // align key
  // std::cout << "function_hip::autotune: align key" << std::endl;
  std::vector<uint64_t> rt_key(align_idxs_.size(), 0);
  for(size_t i = 0; i < align_idxs_.size(); i++){
    int idx = align_idxs_[i];
    uint64_t tmp = 0;
    std::memcpy((void*)&tmp, (void*)((char*)args.data() + arg_off_[idx]), arg_size_[idx]);
    rt_key[i] = pow2_divisor(tmp);
  }
  // auto-tuning key
  //  std::cout << "function_hip::autotune: auto-tuning key" << std::endl;
  std::vector<uint64_t> at_key(key_idxs_.size(), 0);
  for(size_t i = 0; i < at_key.size(); i++){
    int idx = key_idxs_[i];
    std::memcpy((void*)&at_key[i], (void*)((char*)args.data() + arg_off_[idx]), arg_size_[idx]);
  }
  // cache key
  //  std::cout << "function_hip::autotune: cache key" << std::endl;
  std::vector<uint64_t> cache_key;
  cache_key.reserve(rt_key.size() + at_key.size());
  cache_key.insert(cache_key.end(), rt_key.begin(), rt_key.end());
  cache_key.insert(cache_key.end(), at_key.begin(), at_key.end());
  auto it = cache_.find(cache_key);
  if(it != cache_.end())
    return it->second;
  // compile kernels
  //  std::cout << "function_hip::autotune: compile kernels" << std::endl;
  if(kernels_.find(rt_key) == kernels_.end()){
    // std::cout << "function_hip::autotune: compile kernels: loop 1" << std::endl;
    std::map<int, ir::attribute> attrs;
    for(size_t i = 0; i < align_idxs_.size(); i++){
      bool is_ptr = sig_[align_idxs_[i]] == BUFFER_T;
      attrs.insert({align_idxs_[i] + 1, ir::attribute(is_ptr ? ir::aligned : ir::multiple_of, rt_key[i])});
    }

    // std::cout << "function_hip::autotune: compile kernels: loop 2" << std::endl;
    for(const options_t& opt: opts_)
      kernels_[rt_key].emplace_back(new kernel(src_, opt, device_, attrs));
  }
  // run auto-tuner
  // std::cout << "function_hip::autotune: run auto-tuner" << std::endl;
  double best_ts = INFINITY;
  auto& kernels = kernels_.at(rt_key);
  kernel* ret = nullptr;
  if(kernels.size() == 1)
    ret = &*kernels.back();
  else{
    for(auto &current : kernels_.at(rt_key)){
      auto grid = grid_fn(current->opt);
      while(grid.size() < 3)
        grid.push_back(1);
      double ts = tools::bench([&]() { (*current)(args, stream, grid); },
                                       stream, 5, 20);
      ret = (ts < best_ts) ? &*current : ret;
      best_ts = std::min(ts, best_ts);
    }
    stream->synchronize();
  }
  it = cache_.insert({cache_key, ret}).first;
  return it->second;
}

void function::operator()(const std::string& args, const grid_fn_ty& grid_fn, driver::stream *stream) {
  runtime::kernel* fn = autotune(args, grid_fn, stream);
  (*fn)(args, stream, grid_fn(fn->opt));
}


}
}
