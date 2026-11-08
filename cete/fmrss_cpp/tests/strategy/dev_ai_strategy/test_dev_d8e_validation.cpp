#include "devai/strategy/dev_ai_strategy/DevForecastValidationD8E.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace devai::strategy;
int main() {
    std::vector<DevD8EEvaluationRow> rows{{"RELIANCE",100,105,100,0.01,101,101.5},{"SBIN",101,106,200,-0.01,198,197}};
    auto r=devD8EEvaluate(rows,20,20);
    assert(r.aggregate.count==2 && r.per_stock.size()==2);
    assert(r.aggregate.return_rmse>0 && r.aggregate.price_rmse>0);
    assert(r.aggregate.signal_fraction==0.5);
    rows[0].predicted_price=999;
    bool threw=false;
    try {(void)devD8EEvaluate(rows,20,20);} catch(const std::invalid_argument&){threw=true;}
    assert(threw);
    std::cout << "PASS 5C-3I-D8E forecast metrics and dual-price integrity\n";
}
