#define main f3ef2c4_runner_main
#include "../../../../apps/run_dev_f3ef2c4_portfolio.cpp"
#undef main
#include <cassert>
int main(){using namespace f4;assert(local_minutes(1782103500)==10*60+15);auto c=fees(100,101,50);assert(std::abs(c.brokerage-3.015)<1e-9);assert(c.total>c.brokerage);auto v=split("A,123,5");assert(v.size()==3&&v[1]=="123");std::cout<<"F3E-F2C.4 PASS (IST time, brokerage, CSV parser)\n";}
