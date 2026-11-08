#include "devai/benchmark/DevMaskablePPO.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string>

namespace devai::benchmark {
namespace {
void require(bool condition, const char* msg) {
    if (!condition) throw std::runtime_error(msg);
}
void finite(double x) { require(std::isfinite(x), "PPO: nonfinite value"); }
template<class T> void writeArray(std::ostream& o, const T& a) {
    for (double x : a) o << x << '\n';
}
template<class T> void readArray(std::istream& in, T& a) {
    for (double& x : a) { require(bool(in >> x), "PPO: truncated checkpoint"); finite(x); }
}
}
DevMaskablePPO::DevMaskablePPO(PPOConfig c) : config_(c), rng_(c.seed) {
    require(c.gamma>=0 && c.gamma<=1 && c.gae_lambda>=0 && c.gae_lambda<=1,
            "PPO: invalid discount");
    require(c.clip_epsilon>0 && c.clip_epsilon<1 && c.policy_lr>0 &&
            c.value_lr>0 && c.entropy_coef>=0 && c.epochs>0, "PPO: invalid config");
    std::uniform_real_distribution<double> dist(-0.02,0.02);
    for (auto& x : trunk_) x=dist(rng_);
    for (auto& x : policy_) x=dist(rng_);
    for (auto& x : value_head_) x=dist(rng_);
}
void DevMaskablePPO::checkState(const PPOState& s) {
    for (double x:s) finite(x);
}
void DevMaskablePPO::checkMask(const PPOMask& mask) {
    require(std::any_of(mask.begin(),mask.end(),[](bool x){return x;}),
            "PPO: no legal actions");
}
DevMaskablePPO::Forward DevMaskablePPO::forward(const PPOState& s) const {
    checkState(s);
    Forward f{};
    for (std::size_t h=0;h<PPO_HIDDEN;++h) {
        double z=trunk_bias_[h];
        for (std::size_t i=0;i<PPO_INPUT;++i) z+=trunk_[h*PPO_INPUT+i]*s[i];
        f.hidden[h]=std::tanh(z);
    }
    for (std::size_t a=0;a<PPO_ACTIONS;++a) {
        double z=policy_bias_[a];
        for (std::size_t h=0;h<PPO_HIDDEN;++h)
            z+=policy_[a*PPO_HIDDEN+h]*f.hidden[h];
        finite(z); f.logits[a]=z;
    }
    f.value=value_bias_;
    for (std::size_t h=0;h<PPO_HIDDEN;++h) f.value+=value_head_[h]*f.hidden[h];
    finite(f.value);
    return f;
}
std::array<double,PPO_ACTIONS> DevMaskablePPO::probabilities(
    const std::array<double,PPO_ACTIONS>& logits, const PPOMask& mask) {
    checkMask(mask);
    double mx=-std::numeric_limits<double>::infinity();
    for (std::size_t a=0;a<PPO_ACTIONS;++a)
        if (mask[a]) { finite(logits[a]); mx=std::max(mx,logits[a]); }
    std::array<double,PPO_ACTIONS> p{};
    double total=0;
    for (std::size_t a=0;a<PPO_ACTIONS;++a)
        if (mask[a]) { p[a]=std::exp(logits[a]-mx); total+=p[a]; }
    require(total>0 && std::isfinite(total),"PPO: invalid softmax");
    for (double& x:p) x/=total;
    return p;
}
PPOChoice DevMaskablePPO::choose(const PPOState& s, const PPOMask& mask, bool deterministic) {
    auto f=forward(s);
    auto p=probabilities(f.logits,mask);
    std::size_t a=0;
    if (deterministic) {
        double best=-1;
        for (std::size_t i=0;i<PPO_ACTIONS;++i)
            if (mask[i] && p[i]>best) {best=p[i];a=i;}
    } else {
        std::discrete_distribution<std::size_t> dist(p.begin(),p.end());
        a=dist(rng_);
    }
    require(mask[a] && p[a]>0,"PPO: illegal sampled action");
    return {a,std::log(p[a]),f.value};
}
std::array<double, PPO_ACTIONS> DevMaskablePPO::policyLogits(const PPOState& s) const {
    return forward(s).logits;
}
double DevMaskablePPO::value(const PPOState& s) const {return forward(s).value;}
void DevMaskablePPO::computeAdvantages(std::vector<PPOStep>& steps, double bootstrap) const {
    finite(bootstrap);
    double next_value=bootstrap, gae=0;
    for (std::size_t j=steps.size();j>0;--j) {
        auto& step=steps[j-1];
        checkState(step.state); checkMask(step.legal);
        require(step.action<PPO_ACTIONS && step.legal[step.action],
                "PPO: illegal recorded action");
        finite(step.reward); finite(step.old_value); finite(step.old_log_probability);
        const double continuation=step.terminal?0.0:1.0;
        const double delta=step.reward+config_.gamma*next_value*continuation-step.old_value;
        gae=delta+config_.gamma*config_.gae_lambda*continuation*gae;
        step.advantage=gae;
        step.target_value=gae+step.old_value;
        finite(step.advantage);finite(step.target_value);
        next_value=step.old_value;
    }
}
void DevMaskablePPO::update(const std::vector<PPOStep>& steps) {
    require(!steps.empty(),"PPO: empty batch");
    double mean=0, variance=0;
    for (const auto& t:steps) { finite(t.advantage);mean+=t.advantage; }
    mean/=steps.size();
    for (const auto& t:steps) variance+=(t.advantage-mean)*(t.advantage-mean);
    const double scale=std::sqrt(variance/steps.size()+1e-8);
    // Stochastic minibatch size 1, fixed order. No optimizer state is required.
    for (std::size_t epoch=0;epoch<config_.epochs;++epoch) {
        for (const auto& t:steps) {
            checkState(t.state);checkMask(t.legal);
            require(t.action<PPO_ACTIONS && t.legal[t.action],
                    "PPO: illegal training action");
            finite(t.old_log_probability);finite(t.target_value);
            auto f=forward(t.state);
            auto p=probabilities(f.logits,t.legal);
            const double logp=std::log(std::max(p[t.action],1e-300));
            const double ratio=std::exp(std::clamp(logp-t.old_log_probability,-50.0,50.0));
            const double adv=(t.advantage-mean)/scale;
            // Clipped PPO surrogate: gradient vanishes outside active region.
            const bool active= !((adv>=0 && ratio>1+config_.clip_epsilon) ||
                                 (adv<0 && ratio<1-config_.clip_epsilon));
            const double coefficient=active?ratio*adv:0.0;
            std::array<double,PPO_ACTIONS> grad_logits{};
            double entropy=0;
            for (std::size_t a=0;a<PPO_ACTIONS;++a)
                if (t.legal[a]) entropy-=p[a]*std::log(std::max(p[a],1e-300));
            // dH/dlogit_i = -p_i (log p_i + H)
            for (std::size_t a=0;a<PPO_ACTIONS;++a)
                if (t.legal[a]) {
                    grad_logits[a]=coefficient*((a==t.action?1.0:0.0)-p[a])
                        -config_.entropy_coef*p[a]*(std::log(std::max(p[a],1e-300))+entropy);
                }
            const double value_error=std::clamp(f.value-t.target_value,-10.0,10.0);
            std::array<double,PPO_HIDDEN> back{};
            for (std::size_t h=0;h<PPO_HIDDEN;++h) {
                double g=-value_error*value_head_[h];
                for (std::size_t a=0;a<PPO_ACTIONS;++a)
                    g+=grad_logits[a]*policy_[a*PPO_HIDDEN+h];
                back[h]=std::clamp(g*(1-f.hidden[h]*f.hidden[h]),-10.0,10.0);
            }
            for (std::size_t a=0;a<PPO_ACTIONS;++a) {
                for (std::size_t h=0;h<PPO_HIDDEN;++h)
                    policy_[a*PPO_HIDDEN+h]+=config_.policy_lr*
                        std::clamp(grad_logits[a]*f.hidden[h],-10.0,10.0);
                policy_bias_[a]+=config_.policy_lr*std::clamp(grad_logits[a],-10.0,10.0);
            }
            for (std::size_t h=0;h<PPO_HIDDEN;++h) {
                value_head_[h]-=config_.value_lr*value_error*f.hidden[h];
            }
            value_bias_-=config_.value_lr*value_error;
            for (std::size_t h=0;h<PPO_HIDDEN;++h) {
                for (std::size_t i=0;i<PPO_INPUT;++i)
                    trunk_[h*PPO_INPUT+i]+=config_.policy_lr*
                        std::clamp(back[h]*t.state[i],-10.0,10.0);
                trunk_bias_[h]+=config_.policy_lr*back[h];
            }
        }
    }
}
void DevMaskablePPO::save(const std::filesystem::path& path) const {
    std::ofstream out(path);
    require(bool(out),"PPO: cannot write checkpoint");
    out<<std::setprecision(17)<<"CETE_MASKABLE_PPO_V1\n";
    out<<PPO_INPUT<<' '<<PPO_HIDDEN<<' '<<PPO_ACTIONS<<'\n';
    writeArray(out,trunk_);writeArray(out,trunk_bias_);
    writeArray(out,policy_);writeArray(out,policy_bias_);
    writeArray(out,value_head_);out<<value_bias_<<'\n';
    require(bool(out),"PPO: checkpoint write failed");
}
void DevMaskablePPO::load(const std::filesystem::path& path) {
    std::ifstream in(path);
    require(bool(in),"PPO: cannot read checkpoint");
    std::string magic;std::size_t input=0,hidden=0,actions=0;
    require(bool(in>>magic>>input>>hidden>>actions) &&
        magic=="CETE_MASKABLE_PPO_V1" && input==PPO_INPUT &&
        hidden==PPO_HIDDEN && actions==PPO_ACTIONS,"PPO: checkpoint version/dimension mismatch");
    // Transactional load: keep the original model on failure.
    auto trunk=trunk_;auto trunk_bias=trunk_bias_;
    auto policy=policy_;auto policy_bias=policy_bias_;
    auto value_head=value_head_;double value_bias=0;
    readArray(in,trunk);readArray(in,trunk_bias);
    readArray(in,policy);readArray(in,policy_bias);
    readArray(in,value_head);
    require(bool(in>>value_bias),"PPO: truncated value bias");finite(value_bias);
    std::string extra;require(!(in>>extra),"PPO: trailing checkpoint data");
    trunk_=trunk;trunk_bias_=trunk_bias;policy_=policy;
    policy_bias_=policy_bias;value_head_=value_head;value_bias_=value_bias;
}
}
