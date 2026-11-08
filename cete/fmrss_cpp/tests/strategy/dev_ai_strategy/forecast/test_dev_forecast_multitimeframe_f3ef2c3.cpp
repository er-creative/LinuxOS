#include "devai/strategy/dev_ai_strategy/forecast/DevForecastMultiTimeframe_F3EF2C3.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace devai::forecast::f3ef2c3;
int main(){
 std::string cols="symbol,candle_start_epoch,decision_epoch,session_id,close,mean20,std20,z20,return_1m_past,nifty_return_1m_past,valid_1m,target_return_1m,valid_3m,target_return_3m,valid_5m,target_return_5m,valid_10m,target_return_10m,valid_15m,target_return_15m,ret_5m_past,ret_15m_past,mean5_4,std5_4,z5_4,mean15_2,std15_2,z15_2,volatility20,mean60,std60,z60,nifty_ret_5m_past,nifty_ret_15m_past,relative_ret_5m,relative_ret_15m,age5_minutes,age15_minutes";
 std::string row="RELIANCE,1,2,20260101,100,99,1,1,0.001,0.0005,1,0.002,1,0.003,1,0.004,1,0.005,0,,0.002,0.003,99,1,1,99,1,1,0.0003,99,1,1,0.001,0.001,0.001,0.002,2,12";
 auto r=parse(row,header(cols));assert(r.x.size()==24&&r.x[12]==.002&&r.x[22]==2&&r.x[23]==12);assert(r.valid[0]&&!r.valid[4]);assert(finite(r));
 auto fees=zerodha_nse(100,101,50);assert(fees.total>0&&fees.brokerage>0);
 Stats s;s.add(r.x);s.add(r.x);Scale scale;scale.fit(s);auto x=scale.apply(r.x);for(double v:x)assert(std::abs(v)<1e-8);
 Normal n;Vec z{};n.add(z,0.001);n.add(z,0.002);auto w=solve(n,0.1);assert(std::abs(predict(w,z)-.0015)<1e-9);
 Model m;m.scale=scale;for(int h=0;h<5;++h){m.lambda[h]=.1;m.base[h]=.002;m.w[h][0]=.003;}auto path=std::filesystem::temp_directory_path()/"test_f3ef2c3_model.tmp";m.save(path.string());auto loaded=Model::load(path.string());assert(std::abs(loaded.predict(r)[0]-.003)<1e-12);std::filesystem::remove(path);
 std::cout<<"F3E-F2C.3 PASS (24 causal features, labels, ridge, model roundtrip, brokerage)\n";
}
