#ifndef TRITON_INCLUDE_IR_CODEGEN_SWIZZLE_H
#define TRITON_INCLUDE_IR_CODEGEN_SWIZZLE_H

#include <map>
#include <iostream>

namespace triton{

namespace ir{
  class module;
}

namespace codegen{
class target;

namespace analysis{

class layouts;
class data_layout;

class swizzle {
public:
  // constructor
  swizzle(layouts *l, target* tgt): layouts_(l), tgt_(tgt){ }
  // accessors
  int get_per_phase(data_layout* layout) {
    std::cout << "swizzle::get_per_phase" << std::endl;
    return per_phase_.at(layout);
  }
  int get_max_phase(data_layout *layout)
  {
    std::cout << "swizzle::get_max_phase" << std::endl;
    return max_phase_.at(layout);
  }
  int get_vec(data_layout *layout)
  {
    std::cout << "swizzle::get_vec" << std::endl;
    return vec_.at(layout);
  }
  // run
  void run(ir::module &mod);
private:
  layouts* layouts_;
  target* tgt_;
  std::map<data_layout*, int> per_phase_;
  std::map<data_layout*, int> max_phase_;
  std::map<data_layout*, int> vec_;
};

}
}
}


#endif
