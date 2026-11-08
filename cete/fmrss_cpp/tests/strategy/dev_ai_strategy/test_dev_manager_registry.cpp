#include "devai/strategy/dev_ai_strategy/DevManagerRegistry.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <vector>
int main(){
  auto dir=std::filesystem::temp_directory_path()/"cete_registry_test";
  std::filesystem::remove_all(dir);
  {
    devai::DevManagerRegistry registry(dir);
    const auto a=registry.create_manager({"RELIANCE","SBIN","RELIANCE"});
    assert(a.manager_id=="DevManager_001"&&a.symbols.size()==2);
    const auto b=registry.create_manager({"NTPC","HCLTECH"});
    assert(b.manager_id=="DevManager_002");
    registry.assign_shares(a.manager_id,{"ITC","AXISBANK","NTPC"});
    registry.set_active(b.manager_id,false);
    assert(registry.load_manager(a.manager_id).symbols.size()==3);
    assert(!registry.load_manager(b.manager_id).active);
    assert(registry.list_managers().size()==2);
  }
  std::filesystem::remove_all(dir);
  std::cout<<"DevManagerRegistry PASS\n";
}
