#include "devai/strategy/dev_ai_strategy/forecast/DevForecastImproved_F3EF2C.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <functional>
#include <map>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
using namespace devai::forecast::f3ef2c;
namespace fs=std::filesystem;

static Model load_model(const fs::path& path){
    std::ifstream in(path); if(!in) throw std::runtime_error("Cannot open model: "+path.string());
    std::string magic; std::getline(in,magic);
    if(magic!="F3EF2C_RIDGE_V1") throw std::runtime_error("Unexpected model format: "+magic);
    Model m;
    for(double& v:m.scale.mean) if(!(in>>v)) throw std::runtime_error("Invalid model mean");
    for(double& v:m.scale.sd) if(!(in>>v)||!std::isfinite(v)||v<=0) throw std::runtime_error("Invalid model standard deviation");
    for(int h=0;h<5;++h){int horizon=0;
        if(!(in>>horizon>>m.lambda[h]>>m.base[h])||horizon!=horizons[h]) throw std::runtime_error("Invalid model horizon");
        for(double& v:m.w[h]) if(!(in>>v)||!std::isfinite(v)) throw std::runtime_error("Invalid model coefficient");
    }
    return m;
}
static double predicted_net(double buy,double predicted_return,int qty){
    const double sell=buy*(1+predicted_return);
    if(sell<=0) return -1e100;
    return (sell-buy)*qty-zerodha_nse(buy,sell,qty).total;
}
static double break_even_return(double buy,int qty){
    // Solve net P&L == 0 with a monotone binary search, including sell-side fees.
    double lo=0,hi=0.01;
    while(predicted_net(buy,hi,qty)<0 && hi<10) hi*=2;
    if(predicted_net(buy,hi,qty)<0) throw std::runtime_error("Cannot bracket break-even");
    for(int k=0;k<65;++k){double mid=(lo+hi)/2; if(predicted_net(buy,mid,qty)>=0)hi=mid;else lo=mid;}
    return hi;
}
struct Aggregate{uint64_t total=0,eligible=0,negative=0,below=0,zero_qty=0,invalid=0;
    double pred_sum=0,pred_min=1e100,pred_max=-1e100,breakeven_sum=0,eligible_net_sum=0,eligible_realized_win=0;
    std::vector<double> predictions;
    void record(double pred,double be,bool eligible,double net){++total;pred_sum+=pred;pred_min=std::min(pred_min,pred);pred_max=std::max(pred_max,pred);breakeven_sum+=be;predictions.push_back(pred);if(eligible){++this->eligible;eligible_net_sum+=net;eligible_realized_win+=(net>0);}}
};
static double percentile(std::vector<double>& v,double p){if(v.empty())return 0;std::sort(v.begin(),v.end());size_t i=static_cast<size_t>(p*(v.size()-1));return v[i];}
int main(int argc,char**argv){try{
    if(argc!=7){std::cerr<<"Usage: "<<argv[0]<<" <F1C_dir> <shares.txt> <F2C_model> <split_f3ef2.txt> <output_dir> <capital_INR>\n";return 2;}
    fs::path data=argv[1],model_path=argv[3],split_path=argv[4],out=argv[5];
    const double capital=std::stod(argv[6]);if(!std::isfinite(capital)||capital<=0)throw std::runtime_error("Invalid capital");
    Model model=load_model(model_path);
    std::ifstream split_file(split_path);if(!split_file)throw std::runtime_error("Cannot open split file");
    int test_from=0;std::string line;
    while(std::getline(split_file,line)){
        const std::string prefix="test sessions from ";
        if(line.rfind(prefix,0)==0)test_from=std::stoi(line.substr(prefix.size()));
    }
    if(test_from<20000101||test_from>20991231)throw std::runtime_error("Missing test boundary in split file");
    std::ifstream shares(argv[2]);if(!shares)throw std::runtime_error("Cannot open shares file");
    std::vector<std::string> symbols;std::string sym;
    while(std::getline(shares,sym)){if(!sym.empty()&&sym.back()=='\r')sym.pop_back();
        if(sym.empty()||sym[0]=='#'||sym.find("NIFTY")!=std::string::npos)continue;
        if(std::find(symbols.begin(),symbols.end(),sym)==symbols.end())symbols.push_back(sym);
    }
    if(symbols.empty())throw std::runtime_error("No tradable symbols");
    fs::create_directories(out);
    std::ofstream details(out/"all_forecast_diagnostics_f3ef2c1.csv");
    details<<"symbol,decision_epoch,session_id,horizon_minutes,close,quantity,predicted_return,realized_return,break_even_return,forecast_gross_inr,forecast_charges_inr,forecast_net_inr,realized_gross_inr,realized_charges_inr,realized_net_inr,brokerage_inr,stt_inr,exchange_inr,sebi_inr,gst_inr,stamp_inr,status\n"<<std::setprecision(14);
    std::map<std::pair<std::string,int>,Aggregate> groups;
    for(const auto& symbol:symbols){
        std::ifstream in(data/(symbol+"_training_f3ef1c.csv"));if(!in)throw std::runtime_error("Missing F1C CSV for "+symbol);
        std::string head;if(!std::getline(in,head))throw std::runtime_error("Empty CSV for "+symbol);
        auto cols=header(head);uint64_t rows=0;
        while(std::getline(in,line)){
            if(line.empty())continue;
            auto r=parse(line,cols);if(r.session<test_from)continue;
            if(r.symbol!=symbol)throw std::runtime_error("CSV symbol mismatch");
            if(!finite(r))continue;
            auto preds=model.predict(r);
            for(int h=0;h<5;++h){
                auto& a=groups[{symbol,horizons[h]}];
                if(!r.valid[h]||!std::isfinite(r.y[h])){++a.invalid;continue;}
                const double pred=preds[h];if(!std::isfinite(pred))throw std::runtime_error("Non-finite prediction");
                const int qty=static_cast<int>(std::floor(capital*.75/r.close));
                const double sell=r.close*(1+r.y[h]);
                std::string status;double be=0,pg=0,pc=0,pn=0,rg=0,rc=0,rn=0;
                Cost charges{};
                if(qty<=0){status="INSUFFICIENT_CAPITAL";++a.zero_qty;}
                else if(sell<=0){status="INVALID_REALIZED_PRICE";++a.invalid;continue;}
                else{
                    be=break_even_return(r.close,qty);
                    const double psell=r.close*(1+pred);
                    if(psell>0){pg=(psell-r.close)*qty;pc=zerodha_nse(r.close,psell,qty).total;pn=pg-pc;}
                    rg=(sell-r.close)*qty;charges=zerodha_nse(r.close,sell,qty);rc=charges.total;rn=rg-rc;
                    if(pred<=0){status="NEGATIVE_OR_ZERO_FORECAST";++a.negative;}
                    else if(pn<=0){status="BELOW_BREAK_EVEN";++a.below;}
                    else status="ELIGIBLE";
                }
                const bool eligible=status=="ELIGIBLE";
                a.record(pred,be,eligible,rn);
                details<<symbol<<','<<r.epoch<<','<<r.session<<','<<horizons[h]<<','<<r.close<<','<<qty<<','<<pred<<','<<r.y[h]<<','<<be<<','<<pg<<','<<pc<<','<<pn<<','<<rg<<','<<rc<<','<<rn<<','<<charges.brokerage<<','<<charges.stt<<','<<charges.exchange<<','<<charges.sebi<<','<<charges.gst<<','<<charges.stamp<<','<<status<<'\n';
                ++rows;
            }
        }
        std::cout<<"PASS "<<symbol<<" diagnostic_rows="<<rows<<'\n';
    }
    std::ofstream summary(out/"forecast_diagnostic_summary_f3ef2c1.csv");
    summary<<"symbol,horizon_minutes,valid_rows,invalid_target_rows,eligible,negative_or_zero_forecast,below_break_even,insufficient_capital,mean_predicted_return,min_predicted_return,p05_predicted_return,median_predicted_return,p95_predicted_return,max_predicted_return,mean_break_even_return,eligible_mean_realized_net_inr,eligible_realized_win_fraction\n"<<std::setprecision(14);
    for(auto& [key,a]:groups){const double n=a.total?double(a.total):1, en=a.eligible?double(a.eligible):1;
        double p05=percentile(a.predictions,.05),median=percentile(a.predictions,.5),p95=percentile(a.predictions,.95);
        summary<<key.first<<','<<key.second<<','<<a.total<<','<<a.invalid<<','<<a.eligible<<','<<a.negative<<','<<a.below<<','<<a.zero_qty<<','<<a.pred_sum/n<<','<<(a.total?a.pred_min:0)<<','<<p05<<','<<median<<','<<p95<<','<<(a.total?a.pred_max:0)<<','<<a.breakeven_sum/n<<','<<(a.eligible?a.eligible_net_sum/en:0)<<','<<(a.eligible?a.eligible_realized_win/en:0)<<'\n';
    }
    std::cout<<"F3E-F2C.1 COMPLETE symbols="<<symbols.size()<<" test_from="<<test_from<<" output="<<out<<'\n';
    std::cout<<"NOTE: Every row is a hypothetical independent opportunity; no portfolio backtest.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"F3E-F2C.1 ERROR: "<<e.what()<<'\n';return 1;}}
