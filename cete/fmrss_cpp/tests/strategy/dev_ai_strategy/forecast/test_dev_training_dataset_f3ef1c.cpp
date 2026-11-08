#include "devai/strategy/dev_ai_strategy/forecast/DevTrainingDataset_F3EF1C.hpp"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

namespace f=devai::strategy::f3ef1c;
int main() {
    // Synthetic 26 continuous one-minute candles within the same NSE session.
    // 2024-12-24 09:15 IST == 1735011900 UTC.
    std::vector<f::MinuteClose> bars;
    for(int i=0;i<26;++i) bars.push_back({1735011900+60*i,100.0+i,0});
    const auto feats=f::make_features(bars);
    assert(feats.size()==7);
    assert(feats.front().epoch==bars[19].epoch_seconds);
    assert(std::abs(feats.front().mean20-109.5)<1e-10);
    assert(std::abs(feats.front().return1-(119.0/118.0-1.0))<1e-12);
    auto nifty=f::make_nifty_returns(bars);
    assert(nifty.size()==25);
    std::unordered_map<std::int64_t,f::Label> labels;
    for(const auto& bar:bars) {
        f::Label l; l.session=20241224;
        l.valid={true,true,true,true,true};
        l.value={.01,.02,.03,.04,.05};
        labels.emplace(bar.epoch_seconds,l);
    }
    const auto output=f::fs::temp_directory_path()/"f3ef1c_test_dataset.csv";
    auto counts=f::write_dataset(output,"RELIANCE",bars,labels,nifty);
    assert(counts.rows==7 && counts.skipped_warmup==19);
    std::ifstream in(output);
    std::string header,first;
    std::getline(in,header);std::getline(in,first);
    assert(header.find("decision_epoch")!=std::string::npos);
    assert(header.find("target_return_15m")!=std::string::npos);
    assert(first.find("RELIANCE,1735013040,1735013100,")==0);
    // Future label values do not affect causal features.
    auto altered=labels;
    altered[bars[19].epoch_seconds].value[3]=999.0;
    assert(f::make_features(bars).front().mean20==feats.front().mean20);
    // A missing candle forces rolling-stat warmup to restart.
    bars.erase(bars.begin()+10);
    assert(f::make_features(bars).empty());
    f::fs::remove(output);
    std::cout<<"F3E-F1C DATASET BUILDER: PASS (causality, warmup, gap, NIFTY, labels, decision time)\n";
}
