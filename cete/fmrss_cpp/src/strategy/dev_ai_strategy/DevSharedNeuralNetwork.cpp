#include "devai/strategy/dev_ai_strategy/DevSharedNeuralNetwork.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>

namespace devai::strategy {
namespace {
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void finite(double x) { check(std::isfinite(x), "shared network: non-finite value"); }
void validate(const DevSharedParameters& p) {
    check(p.w1.size()==DEV_SHARED_HIDDEN*DEV_SHARED_INPUT &&
          p.b1.size()==DEV_SHARED_HIDDEN &&
          p.w2.size()==DEV_SHARED_OUTPUT*DEV_SHARED_HIDDEN &&
          p.b2.size()==DEV_SHARED_OUTPUT, "shared network: invalid parameter dimensions");
    for (const auto* v : {&p.w1,&p.b1,&p.w2,&p.b2})
        for (double x:*v) finite(x);
}
}
DevSharedNeuralNetwork::DevSharedNeuralNetwork(std::uint32_t seed) {
    params_.w1.resize(DEV_SHARED_HIDDEN*DEV_SHARED_INPUT);
    params_.b1.assign(DEV_SHARED_HIDDEN,0.0);
    params_.w2.resize(DEV_SHARED_OUTPUT*DEV_SHARED_HIDDEN);
    params_.b2.assign(DEV_SHARED_OUTPUT,0.0);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> d(-0.025,0.025);
    for (auto& x:params_.w1) x=d(rng);
    for (auto& x:params_.w2) x=d(rng);
}
DevSharedInput DevSharedNeuralNetwork::encodeAgent(const DevAIObservation& observation) const {
    ObservationNormalizer normalizer;
    const auto x=normalizer.normalize(observation);
    DevSharedInput result{};
    std::copy(x.begin(),x.end(),result.values.begin());
    result.values[DEV_Q_INPUT_SIZE+DEV_MANAGER_INPUT_SIZE]=1.0;
    for (double v:result.values) finite(v);
    return result;
}
DevSharedInput DevSharedNeuralNetwork::encodeManager(const DevManagerEncodedState& state) const {
    check(state.numerical_integrity,"shared network: invalid manager state");
    DevSharedInput result{};
    std::copy(state.values.begin(),state.values.end(),result.values.begin()+DEV_Q_INPUT_SIZE);
    result.values[DEV_Q_INPUT_SIZE+DEV_MANAGER_INPUT_SIZE+1]=1.0;
    for (double v:result.values) finite(v);
    return result;
}
std::array<double,DEV_SHARED_OUTPUT> DevSharedNeuralNetwork::predict(const DevSharedInput& input) const {
    std::array<double,DEV_SHARED_HIDDEN> hidden{};
    for (double x:input.values) finite(x);
    for (std::size_t h=0;h<DEV_SHARED_HIDDEN;++h) {
        double z=params_.b1[h];
        for (std::size_t i=0;i<DEV_SHARED_INPUT;++i) z+=params_.w1[h*DEV_SHARED_INPUT+i]*input.values[i];
        hidden[h]=std::max(0.0,z);
    }
    std::array<double,DEV_SHARED_OUTPUT> out{};
    for (std::size_t a=0;a<DEV_SHARED_OUTPUT;++a) {
        double z=params_.b2[a];
        for (std::size_t h=0;h<DEV_SHARED_HIDDEN;++h) z+=params_.w2[a*DEV_SHARED_HIDDEN+h]*hidden[h];
        finite(z);out[a]=z;
    }
    return out;
}
DevQValues DevSharedNeuralNetwork::predictAgent(const DevAIObservation& observation) const {
    const auto raw=predict(encodeAgent(observation));
    DevQValues q{};
    std::copy_n(raw.begin(),DEV_Q_OUTPUT_SIZE,q.values.begin());
    q.numerical_integrity=true;
    return q;
}
DevManagerQValues DevSharedNeuralNetwork::predictManager(const DevManagerEncodedState& state) const {
    const auto raw=predict(encodeManager(state));
    DevManagerQValues q{};
    std::copy(raw.begin(),raw.end(),q.values.begin());
    q.numerical_integrity=true;
    return q;
}
double DevSharedNeuralNetwork::train(const DevSharedInput& input, std::size_t action,
                                    double target, double learning_rate) {
    check(action<DEV_SHARED_OUTPUT,"shared network: action out of range");
    finite(target);finite(learning_rate);
    check(learning_rate>0.0 && learning_rate<=0.1,"shared network: invalid learning rate");
    std::array<double,DEV_SHARED_HIDDEN> z{},h{};
    for (double x:input.values) finite(x);
    for (std::size_t j=0;j<DEV_SHARED_HIDDEN;++j) {
        z[j]=params_.b1[j];
        for (std::size_t i=0;i<DEV_SHARED_INPUT;++i) z[j]+=params_.w1[j*DEV_SHARED_INPUT+i]*input.values[i];
        h[j]=std::max(0.0,z[j]);
    }
    double predicted=params_.b2[action];
    for (std::size_t j=0;j<DEV_SHARED_HIDDEN;++j) predicted+=params_.w2[action*DEV_SHARED_HIDDEN+j]*h[j];
    finite(predicted);
    const double error=std::clamp(predicted-target,-10.0,10.0);
    std::array<double,DEV_SHARED_HIDDEN> back{};
    for (std::size_t j=0;j<DEV_SHARED_HIDDEN;++j)
        back[j]=z[j]>0.0?error*params_.w2[action*DEV_SHARED_HIDDEN+j]:0.0;
    for (std::size_t j=0;j<DEV_SHARED_HIDDEN;++j)
        params_.w2[action*DEV_SHARED_HIDDEN+j]-=learning_rate*error*h[j];
    params_.b2[action]-=learning_rate*error;
    for (std::size_t j=0;j<DEV_SHARED_HIDDEN;++j) {
        for (std::size_t i=0;i<DEV_SHARED_INPUT;++i)
            params_.w1[j*DEV_SHARED_INPUT+i]-=learning_rate*back[j]*input.values[i];
        params_.b1[j]-=learning_rate*back[j];
    }
    return 0.5*error*error;
}
void DevSharedNeuralNetwork::setParameters(const DevSharedParameters& p) {validate(p);params_=p;}
void DevSharedNeuralNetwork::save(const std::filesystem::path& path) const {
    std::ofstream out(path);check(bool(out),"shared network: cannot save checkpoint");
    out.precision(17);out<<"CETE_SHARED_DEVAI_V3_FIVE_MIXED_AGENTS\n";
    for (const auto* v:{&params_.w1,&params_.b1,&params_.w2,&params_.b2}) {
        out<<v->size()<<'\n';for (double x:*v)out<<x<<'\n';
    }
    check(bool(out),"shared network: checkpoint write failed");
}
void DevSharedNeuralNetwork::load(const std::filesystem::path& path) {
    std::ifstream in(path);check(bool(in),"shared network: cannot load checkpoint");
    std::string magic;in>>magic;check(magic=="CETE_SHARED_DEVAI_V3_FIVE_MIXED_AGENTS","shared network: checkpoint version mismatch");
    DevSharedParameters p;
    for (auto* v:{&p.w1,&p.b1,&p.w2,&p.b2}) {
        std::size_t n=0;check(bool(in>>n) && n<=100000,"shared network: bad vector length");
        v->resize(n);for (double& x:*v) check(bool(in>>x),"shared network: truncated checkpoint");
    }
    validate(p);params_=std::move(p);
}
}
