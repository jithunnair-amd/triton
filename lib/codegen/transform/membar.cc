#include <vector>
#include <set>
#include <algorithm>
#include "triton/codegen/analysis/layout.h"
#include "triton/codegen/analysis/allocation.h"
#include "triton/codegen/transform/membar.h"
#include "triton/ir/module.h"
#include "triton/ir/function.h"
#include "triton/ir/basic_block.h"
#include "triton/ir/instructions.h"
#include "triton/ir/utils.h"

namespace triton {

namespace codegen{
namespace transform{



int membar::group_of(ir::value* v, std::vector<ir::value*> &async_write) {
  if(ir::phi_node* phi = dynamic_cast<ir::phi_node*>(v)){
    analysis::shared_layout* layout = layouts_->get(v)->to_shared();
    analysis::double_buffer_info_t* info = layout->get_double_buffer();
    if(info)
      return group_of(info->first, async_write);
    std::vector<int> groups(phi->get_num_operands());
    std::transform(phi->op_begin(), phi->op_end(), groups.begin(), [&](ir::value* v){ return group_of(v, async_write);});
    return *std::max_element(groups.begin(), groups.end());
  }
  else{
    auto it = std::find(async_write.begin(), async_write.end(), v);
    return std::distance(async_write.begin(), it);
  }
}


membar::val_set_t membar::intersect_with(const val_set_t& as, const val_set_t& bs) {
  val_set_t ret;
  for(ir::value* a: as){
    if(!a->get_type()->is_block_ty())
      continue;
    analysis::shared_layout* a_layout = layouts_->get(a)->to_shared();
    if(!a_layout)
      continue;
    int a_start = alloc_->offset(a_layout);
    int a_end = a_start + a_layout->get_size();
    for(ir::value* b: bs){
      if(!b->get_type()->is_block_ty())
        continue;
      analysis::shared_layout* b_layout = layouts_->get(b)->to_shared();
      if(!b_layout)
        continue;
      int b_start = alloc_->offset(b_layout);
      int b_end = b_start + b_layout->get_size();
      if(a_start < b_end || b_start < a_end)
        ret.insert(b);
    }
  }
  return ret;
}

void membar::transfer(ir::basic_block *block,
                      val_vec_t& async_write,
                      val_set_t& sync_write,
                      val_set_t& sync_read,
                      std::set<ir::value*>& safe_war,
                      bool& inserted, ir::builder& builder) {
  ir::basic_block::inst_list_t instructions = block->get_inst_list();
  for(ir::instruction *i: instructions){
    if(dynamic_cast<ir::phi_node*>(i))
      continue;
    if(std::find(async_write.begin(), async_write.end(), i) == async_write.end() &&
       dynamic_cast<ir::masked_load_async_inst*>(i)){
      async_write.push_back(i);
    }
    if(dynamic_cast<ir::copy_to_shared_inst*>(i))
      sync_write.insert(i);
    ir::barrier_inst* barrier = dynamic_cast<ir::barrier_inst*>(i);
    ir::async_wait_inst* async_wait = dynamic_cast<ir::async_wait_inst*>(i);
    // Get shared memory reads
    std::set<ir::value*> read;
    std::copy_if(i->op_begin(), i->op_end(), std::inserter(read, read.begin()),
                 [&](ir::value* i){ return i->get_type()->is_block_ty() && layouts_->get(i)->to_shared();});
    // RAW (async)
    val_set_t tmp;
    std::copy(async_write.begin(), async_write.end(), std::inserter(tmp, tmp.begin()));
    if(intersect_with(read, tmp).size()){
      std::vector<int> groups(read.size());
      std::transform(read.begin(), read.end(), groups.begin(), [&](ir::value* v){ return group_of(v, async_write);});
      int N = *std::max_element(groups.begin(), groups.end());
      if(N < async_write.size()){
        builder.set_insert_point(i);
        async_wait = (ir::async_wait_inst*)builder.create_async_wait(async_write.size() - 1 - N);
        barrier = (ir::barrier_inst*)builder.create_barrier();
        inserted = true;
      }
    }
    // RAW, WAR
    bool is_i_double_buffered = i->get_type()->is_block_ty() &&
                                layouts_->get(i)->to_shared() &&
                                layouts_->get(i)->to_shared()->get_double_buffer();
    // WAR barrier is not required when data is double-buffered
    // TODO: how about other patterns, like WWAR?
    if(!intersect_with(read, sync_write).empty() || 
       (!intersect_with({i}, sync_read).empty() && !is_i_double_buffered) ||
       // force WAR barrier on A100
       (!intersect_with({i}, sync_read).empty() && tgt_->as_nvidia()->sm() >= 80)){
      builder.set_insert_point(i);
      barrier = (ir::barrier_inst*)builder.create_barrier();
      inserted = true;
    }
    // update state of asynchronous copies
    if(async_wait){
      int N = async_write.size() - async_wait->get_N();
      async_write.erase(async_write.begin(), async_write.begin() + N);
    }
    // all the copy_to_shared and read from shared are synchronized after barrier
    if(barrier){
      sync_write.clear();
      sync_read.clear();
    }
    sync_read.insert(read.begin(), read.end());

  }
}

void membar::run(ir::module &mod) {
  ir::builder &builder = mod.get_builder();
  // extract phi-node associates with double-buffered
  // shared-memory copies. These can be read from and written to
  // without needing synchronization
  std::set<ir::value*> safe_war;
  for(const auto& x: layouts_->get_all()){
    analysis::shared_layout* layout = x.second->to_shared();
    if(!layout || !layout->get_double_buffer())
      continue;
    for(ir::value *v: layout->get_values())
      if(v != layout->get_double_buffer()->phi){
        safe_war.insert(v);
      }
  }

  for(ir::function *fn: mod.get_function_list()){
    // TODO: (dyan) we need DominatorTree here.
    std::vector<ir::basic_block*> rpo = ir::cfg::reverse_post_order(fn);
    std::map<ir::basic_block*, val_vec_t> async_writes;
    std::map<ir::basic_block*, val_set_t> sync_writes;
    std::map<ir::basic_block*, val_set_t> sync_reads;
    std::list<ir::value *> pipelined;
    bool inserted;
    do{
      inserted = false;
      // find barrier location
      for(ir::basic_block *block: rpo){
        // join inputs
        val_vec_t async_write;
        val_set_t sync_write;
        val_set_t sync_read;
        val_set_t tmp;
        for(ir::basic_block* pred: block->get_predecessors()){
          for(ir::value* v: async_writes[pred])
            if(tmp.insert(v).second)
              async_write.push_back(v);
          sync_write.insert(sync_writes[pred].begin(), sync_writes[pred].end());
          sync_read.insert(sync_reads[pred].begin(), sync_reads[pred].end());
        }
        transfer(block, async_write, sync_write, sync_read, safe_war, inserted, builder);
        async_writes[block] = async_write;
        sync_writes[block] = sync_write;
        sync_reads[block] = sync_read;
      }
    }while(inserted);
  }
}

}
}
}