#include "devai/strategy/dev_ai_strategy/DevForecastValidationD8E.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>
namespace devai::strategy {
namespace {
struct Acc {
    DevD8EMetrics out{};
    double ae=0,se=0,base=0,pe=0,sx=0,sy=0,sxx=0,syy=0,sxy=0;
    double gross=0,net=0;
    std::size_t signals=0;
    void add(const DevD8EEvaluationRow& r,double cost,double threshold){
        if(r.symbol.empty()||r.decision_key<=0||r.target_key<=r.decision_key||
           !(r.current_price>0)||!(r.actual_price>0)||
           !std::isfinite(r.current_price)||!std::isfinite(r.actual_price)||
           !std::isfinite(r.predicted_price)||!std::isfinite(r.predicted_return))
            throw std::invalid_argument("D8E invalid row");
        const double expected=r.current_price*(1+r.predicted_return);
        if(std::abs(expected-r.predicted_price)>1e-7*std::max(1.0,std::abs(expected)))
            throw std::invalid_argument("D8E inconsistent dual forecast");
        const double actual=r.actual_price/r.current_price-1.0;
        const double e=r.predicted_return-actual;
        ++out.count;
        ae+=std::abs(e);se+=e*e;base+=actual*actual;
        pe+=(r.predicted_price-r.actual_price)*(r.predicted_price-r.actual_price);
        sx+=r.predicted_return;sy+=actual;sxx+=r.predicted_return*r.predicted_return;
        syy+=actual*actual;sxy+=r.predicted_return*actual;
        if(r.predicted_return!=0){++out.nonzero_predictions;
            if((r.predicted_return>0)==(actual>0))++out.direction_correct;}
        if(r.predicted_return>threshold){++signals;gross+=actual;net+=actual-cost;}
    }
    DevD8EMetrics finish(){
        auto& x=out;if(!x.count)return x;
        const double n=static_cast<double>(x.count);
        x.return_mae=ae/n;x.return_rmse=std::sqrt(se/n);
        x.baseline_rmse=std::sqrt(base/n);x.price_rmse=std::sqrt(pe/n);
        x.directional_accuracy=x.nonzero_predictions?
            static_cast<double>(x.direction_correct)/x.nonzero_predictions:0;
        const double vx=n*sxx-sx*sx,vy=n*syy-sy*sy;
        x.correlation=(vx>0&&vy>0)?(n*sxy-sx*sy)/std::sqrt(vx*vy):0;
        x.signal_fraction=static_cast<double>(signals)/n;
        x.gross_mean_return=signals?gross/signals:0;
        x.net_mean_return=signals?net/signals:0;
        return x;
    }
};
}
DevD8EReport devD8EEvaluate(const std::vector<DevD8EEvaluationRow>& rows,
                             double cost_bps,double threshold_bps){
    if(!std::isfinite(cost_bps)||cost_bps<0||!std::isfinite(threshold_bps)||threshold_bps<0)
        throw std::invalid_argument("D8E invalid costs/threshold");
    Acc all;std::map<std::string,Acc> by;
    for(const auto& r:rows){all.add(r,cost_bps/10000.0,threshold_bps/10000.0);
        by[r.symbol].add(r,cost_bps/10000.0,threshold_bps/10000.0);}
    DevD8EReport result;result.aggregate=all.finish();
    for(auto& [name,a]:by)result.per_stock.emplace(name,a.finish());
    return result;
}
} // namespace devai::strategy
