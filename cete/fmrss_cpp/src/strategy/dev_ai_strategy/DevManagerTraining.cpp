#include "devai/strategy/dev_ai_strategy/DevManagerTraining.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace devai::strategy
{
namespace
{
constexpr double EPS = 1.0e-12;

bool finite(double x) noexcept { return std::isfinite(x); }
double squash(double x) noexcept { return std::tanh(x); }
double relu(double x) noexcept { return x > 0.0 ? x : 0.0; }

std::size_t actionIndex(DevManagerAction a)
{
    if (!isValidManagerAction(a))
        throw std::invalid_argument("DevManagerTraining: invalid Manager action.");
    return static_cast<std::size_t>(a);
}

void validateParameters(const DevManagerNetworkParameters& p)
{
    const std::size_t w1 = DEV_MANAGER_HIDDEN1_SIZE * DEV_MANAGER_INPUT_SIZE;
    const std::size_t w2 = DEV_MANAGER_HIDDEN2_SIZE * DEV_MANAGER_HIDDEN1_SIZE;
    const std::size_t w3 = DEV_MANAGER_ACTION_COUNT * DEV_MANAGER_HIDDEN2_SIZE;
    if (p.weights1.size()!=w1 || p.biases1.size()!=DEV_MANAGER_HIDDEN1_SIZE ||
        p.weights2.size()!=w2 || p.biases2.size()!=DEV_MANAGER_HIDDEN2_SIZE ||
        p.weights3.size()!=w3 || p.biases3.size()!=DEV_MANAGER_ACTION_COUNT)
        throw std::invalid_argument("DevManagerQNetwork: parameter dimensions mismatch.");
    auto check=[](const std::vector<double>& v){
        return std::all_of(v.begin(),v.end(),[](double x){return std::isfinite(x);});};
    if (!check(p.weights1)||!check(p.biases1)||!check(p.weights2)||!check(p.biases2)||!check(p.weights3)||!check(p.biases3))
        throw std::invalid_argument("DevManagerQNetwork: non-finite parameter.");
}

struct Forward
{
    std::array<double, DEV_MANAGER_HIDDEN1_SIZE> z1{}, h1{};
    std::array<double, DEV_MANAGER_HIDDEN2_SIZE> z2{}, h2{};
    std::array<double, DEV_MANAGER_ACTION_COUNT> q{};
};

Forward forward(const DevManagerNetworkParameters& p,
                const std::array<double,DEV_MANAGER_INPUT_SIZE>& x)
{
    Forward f{};
    for(std::size_t j=0;j<DEV_MANAGER_HIDDEN1_SIZE;++j){
        double z=p.biases1[j];
        for(std::size_t i=0;i<DEV_MANAGER_INPUT_SIZE;++i)
            z += p.weights1[j*DEV_MANAGER_INPUT_SIZE+i]*x[i];
        f.z1[j]=z; f.h1[j]=relu(z);
    }
    for(std::size_t k=0;k<DEV_MANAGER_HIDDEN2_SIZE;++k){
        double z=p.biases2[k];
        for(std::size_t j=0;j<DEV_MANAGER_HIDDEN1_SIZE;++j)
            z += p.weights2[k*DEV_MANAGER_HIDDEN1_SIZE+j]*f.h1[j];
        f.z2[k]=z; f.h2[k]=relu(z);
    }
    for(std::size_t a=0;a<DEV_MANAGER_ACTION_COUNT;++a){
        double q=p.biases3[a];
        for(std::size_t k=0;k<DEV_MANAGER_HIDDEN2_SIZE;++k)
            q += p.weights3[a*DEV_MANAGER_HIDDEN2_SIZE+k]*f.h2[k];
        f.q[a]=q;
    }
    return f;
}

double huberLoss(double e,double d) noexcept
{
    const double a=std::fabs(e);
    return a<=d ? 0.5*e*e : d*(a-0.5*d);
}
double huberDerivative(double prediction_minus_target,double d) noexcept
{
    if(prediction_minus_target>d) return d;
    if(prediction_minus_target<-d) return -d;
    return prediction_minus_target;
}

double clip(double x,double c) noexcept
{
    if(c<=0.0) return x;
    return std::clamp(x,-c,c);
}

void validateConfig(const DevManagerTrainingConfig& c)
{
    if(c.replay_capacity==0 || c.minimum_replay_size==0 || c.batch_size==0)
        throw std::invalid_argument("DevManagerTrainer: replay/batch sizes must be positive.");
    if(c.batch_size>c.minimum_replay_size || c.minimum_replay_size>c.replay_capacity)
        throw std::invalid_argument("DevManagerTrainer: require batch <= minimum replay <= capacity.");
    if(!finite(c.gamma)||c.gamma<0.0||c.gamma>1.0 || !finite(c.learning_rate)||c.learning_rate<=0.0 ||
       !finite(c.huber_delta)||c.huber_delta<=0.0 || !finite(c.gradient_clip) || c.target_sync_interval==0 ||
       !finite(c.epsilon_start)||!finite(c.epsilon_end)||c.epsilon_start<0.0||c.epsilon_start>1.0||
       c.epsilon_end<0.0||c.epsilon_end>1.0||c.epsilon_end>c.epsilon_start||c.epsilon_decay_selections==0)
        throw std::invalid_argument("DevManagerTrainer: invalid training configuration.");
}

bool flatPosition(double p) noexcept { return std::fabs(p) <= EPS; }
}

DevManagerEncodedState DevManagerStateEncoder::encode(const DevManagerState& state) const
{
    if(!state.numerical_integrity || state.decision_time<=0 || state.report_count>DEV_AGENT_SLOT_COUNT ||
       !state.portfolio.numerical_integrity)
        throw std::invalid_argument("DevManagerStateEncoder: invalid Manager state.");

    DevManagerEncodedState out{};
    out.decision_time=state.decision_time;
    std::size_t n=0;
    for(std::size_t slot=0;slot<DEV_AGENT_SLOT_COUNT;++slot){
        const auto& opt=state.agent_reports[slot];
        out.values[n++]=opt ? 1.0 : 0.0;
        if(opt){
            const auto& r=*opt;
            if(!r.numerical_integrity || !r.q_values.numerical_integrity || r.slot_index!=slot ||
               r.decision_time!=state.decision_time || !finite(r.candidate_score) || !finite(r.preferred_q_value))
                throw std::invalid_argument("DevManagerStateEncoder: invalid agent report.");
            out.values[n++]=squash(r.candidate_score);
            for(double q:r.q_values.values){ if(!finite(q)) throw std::invalid_argument("DevManagerStateEncoder: non-finite Q."); out.values[n++]=squash(q); }
            for(std::size_t a=0;a<4;++a) out.values[n++]=(static_cast<std::size_t>(r.preferred_action)==a)?1.0:0.0;
        } else {
            n += 9;
        }
    }
    const auto& p=state.portfolio.portfolio;
    const std::array<double,6> portfolio_values{
        p.position,p.unrealized_return,p.realized_return,p.equity_return,p.drawdown,p.holding_period};
    for(double v:portfolio_values){ if(!finite(v)) throw std::invalid_argument("DevManagerStateEncoder: non-finite portfolio state."); out.values[n++]=squash(v); }
    for(std::size_t slot=0;slot<DEV_AGENT_SLOT_COUNT;++slot)
        out.values[n++]=(state.agent_reports[slot] && !state.portfolio.current_symbol.empty() &&
                         state.agent_reports[slot]->symbol==state.portfolio.current_symbol)?1.0:0.0;
    out.values[n++]=static_cast<double>(state.report_count)/static_cast<double>(DEV_AGENT_SLOT_COUNT);
    if(n!=DEV_MANAGER_INPUT_SIZE) throw std::logic_error("DevManagerStateEncoder: internal dimension mismatch.");

    out.valid_actions.fill(false);
    out.valid_actions[0]=true;
    out.valid_actions[1]=!flatPosition(p.position) && !state.portfolio.current_symbol.empty();
    for(std::size_t slot=0;slot<DEV_AGENT_SLOT_COUNT;++slot)
        out.valid_actions[2+slot]=state.agent_reports[slot].has_value();
    out.numerical_integrity=std::all_of(out.values.begin(),out.values.end(),[](double v){return std::isfinite(v);});
    if(!out.numerical_integrity) throw std::invalid_argument("DevManagerStateEncoder: encoded state not finite.");
    return out;
}

double DevManagerQValues::value(DevManagerAction action) const { return values[actionIndex(action)]; }
DevManagerAction DevManagerQValues::greedyAction(const std::array<bool,DEV_MANAGER_ACTION_COUNT>& valid) const
{
    bool found=false; std::size_t best=0; double bestv=-std::numeric_limits<double>::infinity();
    for(std::size_t i=0;i<DEV_MANAGER_ACTION_COUNT;++i) if(valid[i] && (!found || values[i]>bestv)){found=true;best=i;bestv=values[i];}
    if(!found) throw std::invalid_argument("DevManagerQValues: no valid Manager action.");
    return static_cast<DevManagerAction>(best);
}
double DevManagerQValues::maximumValue(const std::array<bool,DEV_MANAGER_ACTION_COUNT>& valid) const { return value(greedyAction(valid)); }

DevManagerQNetwork::DevManagerQNetwork(DevManagerNetworkConfig config):config_(config)
{
    parameters_.weights1.resize(DEV_MANAGER_HIDDEN1_SIZE*DEV_MANAGER_INPUT_SIZE);
    parameters_.biases1.resize(DEV_MANAGER_HIDDEN1_SIZE);
    parameters_.weights2.resize(DEV_MANAGER_HIDDEN2_SIZE*DEV_MANAGER_HIDDEN1_SIZE);
    parameters_.biases2.resize(DEV_MANAGER_HIDDEN2_SIZE);
    parameters_.weights3.resize(DEV_MANAGER_ACTION_COUNT*DEV_MANAGER_HIDDEN2_SIZE);
    parameters_.biases3.resize(DEV_MANAGER_ACTION_COUNT);
    if(!config.zero_initialize){
        std::mt19937 rng(config.random_seed);
        auto init=[&](std::vector<double>& v,double fan){std::normal_distribution<double>d(0.0,std::sqrt(2.0/fan));for(double&x:v)x=d(rng);};
        init(parameters_.weights1,DEV_MANAGER_INPUT_SIZE);
        init(parameters_.weights2,DEV_MANAGER_HIDDEN1_SIZE);
        init(parameters_.weights3,DEV_MANAGER_HIDDEN2_SIZE);
    }
}
DevManagerQValues DevManagerQNetwork::predict(const DevManagerEncodedState& state) const
{
    if(!state.numerical_integrity) throw std::invalid_argument("DevManagerQNetwork: invalid encoded state.");
    const auto f=forward(parameters_,state.values); DevManagerQValues q{}; q.values=f.q;
    q.numerical_integrity=std::all_of(q.values.begin(),q.values.end(),[](double x){return std::isfinite(x);});
    if (!q.numerical_integrity)
    {
        throw std::runtime_error("DevManagerQNetwork: non-finite inference.");
    }

    return q;
}
std::size_t DevManagerQNetwork::parameterCount() const noexcept
{
    return DEV_MANAGER_HIDDEN1_SIZE*DEV_MANAGER_INPUT_SIZE+DEV_MANAGER_HIDDEN1_SIZE+
           DEV_MANAGER_HIDDEN2_SIZE*DEV_MANAGER_HIDDEN1_SIZE+DEV_MANAGER_HIDDEN2_SIZE+
           DEV_MANAGER_ACTION_COUNT*DEV_MANAGER_HIDDEN2_SIZE+DEV_MANAGER_ACTION_COUNT;
}
DevManagerNetworkParameters DevManagerQNetwork::parameters() const{return parameters_;}
void DevManagerQNetwork::setParameters(const DevManagerNetworkParameters& p){validateParameters(p);parameters_=p;}
void DevManagerQNetwork::copyParametersFrom(const DevManagerQNetwork& other){parameters_=other.parameters_;}

DevManagerTrainer::DevManagerTrainer(DevManagerTrainingConfig config)
    :config_(config),online_network_(DevManagerNetworkConfig{config.random_seed+1U,false}),
     target_network_(DevManagerNetworkConfig{config.random_seed+2U,false}),random_engine_(config.random_seed)
{
    validateConfig(config_); target_network_.copyParametersFrom(online_network_);
}

void DevManagerTrainer::validateTransition(const DevManagerTransition& t)
{
    if(!t.numerical_integrity || t.decision_time<=0 || t.next_decision_time<=t.decision_time ||
       t.state.decision_time!=t.decision_time || t.next_state.decision_time!=t.next_decision_time ||
       !t.state.numerical_integrity || !t.next_state.numerical_integrity || !finite(t.reward) || !isValidManagerAction(t.action))
        throw std::invalid_argument("DevManagerTrainer: invalid transition contract.");
    DevManagerStateEncoder enc; const auto s=enc.encode(t.state); (void)enc.encode(t.next_state);
    if(!s.valid_actions[actionIndex(t.action)]) throw std::invalid_argument("DevManagerTrainer: transition action invalid for state.");
}
void DevManagerTrainer::addExperience(const DevManagerTransition& t)
{
    validateTransition(t); if(replay_.size()==config_.replay_capacity) replay_.pop_front(); replay_.push_back(t);
}
bool DevManagerTrainer::canTrain() const noexcept{return replay_.size()>=config_.minimum_replay_size;}

DevManagerTrainingStepResult DevManagerTrainer::trainOneUpdate()
{
    if(!canTrain()) throw std::runtime_error("DevManagerTrainer: replay warm-up not complete.");
    std::vector<std::size_t> ids(replay_.size()); std::iota(ids.begin(),ids.end(),0);
    std::shuffle(ids.begin(),ids.end(),random_engine_); ids.resize(config_.batch_size);

    auto base=online_network_.parameters();
    DevManagerNetworkParameters g{};
    g.weights1.assign(base.weights1.size(),0.0);g.biases1.assign(base.biases1.size(),0.0);
    g.weights2.assign(base.weights2.size(),0.0);g.biases2.assign(base.biases2.size(),0.0);
    g.weights3.assign(base.weights3.size(),0.0);g.biases3.assign(base.biases3.size(),0.0);
    double sum_target=0,sum_loss=0,sum_td=0;

    for(std::size_t id:ids){
        const auto& tr=replay_[id]; const auto s=encoder_.encode(tr.state); const auto ns=encoder_.encode(tr.next_state);
        const auto targetq=target_network_.predict(ns);
        const double target=tr.reward+(tr.terminal?0.0:config_.gamma*targetq.maximumValue(ns.valid_actions));
        const auto f=forward(base,s.values); const std::size_t a=actionIndex(tr.action);
        const double pred=f.q[a], td=target-pred, d=huberDerivative(pred-target,config_.huber_delta);
        sum_target+=target;sum_loss+=huberLoss(pred-target,config_.huber_delta);sum_td+=td;
        std::array<double,DEV_MANAGER_HIDDEN2_SIZE> dh2{};
        for(std::size_t k=0;k<DEV_MANAGER_HIDDEN2_SIZE;++k){g.weights3[a*DEV_MANAGER_HIDDEN2_SIZE+k]+=d*f.h2[k];dh2[k]=d*base.weights3[a*DEV_MANAGER_HIDDEN2_SIZE+k];}
        g.biases3[a]+=d;
        std::array<double,DEV_MANAGER_HIDDEN1_SIZE> dh1{};
        for(std::size_t k=0;k<DEV_MANAGER_HIDDEN2_SIZE;++k){const double dz=dh2[k]*(f.z2[k]>0.0?1.0:0.0);g.biases2[k]+=dz;for(std::size_t j=0;j<DEV_MANAGER_HIDDEN1_SIZE;++j){g.weights2[k*DEV_MANAGER_HIDDEN1_SIZE+j]+=dz*f.h1[j];dh1[j]+=dz*base.weights2[k*DEV_MANAGER_HIDDEN1_SIZE+j];}}
        for(std::size_t j=0;j<DEV_MANAGER_HIDDEN1_SIZE;++j){const double dz=dh1[j]*(f.z1[j]>0.0?1.0:0.0);g.biases1[j]+=dz;for(std::size_t i=0;i<DEV_MANAGER_INPUT_SIZE;++i)g.weights1[j*DEV_MANAGER_INPUT_SIZE+i]+=dz*s.values[i];}
    }
    const double inv=1.0/static_cast<double>(config_.batch_size); double norm2=0,cnorm2=0;
    auto apply=[&](std::vector<double>& p,std::vector<double>& grad){for(std::size_t i=0;i<p.size();++i){const double raw=grad[i]*inv;norm2+=raw*raw;const double c=clip(raw,config_.gradient_clip);cnorm2+=c*c;p[i]-=config_.learning_rate*c;}};
    apply(base.weights1,g.weights1);apply(base.biases1,g.biases1);apply(base.weights2,g.weights2);apply(base.biases2,g.biases2);apply(base.weights3,g.weights3);apply(base.biases3,g.biases3);
    online_network_.setParameters(base); ++completed_training_updates_;
    const bool sync=(completed_training_updates_%config_.target_sync_interval)==0; if(sync)target_network_.copyParametersFrom(online_network_);
    DevManagerTrainingStepResult r{};r.completed_training_updates=completed_training_updates_;r.batch_size=config_.batch_size;r.mean_bellman_target=sum_target*inv;r.mean_loss=sum_loss*inv;r.mean_td_error=sum_td*inv;r.gradient_l2_norm=std::sqrt(norm2);r.clipped_gradient_l2_norm=std::sqrt(cnorm2);r.target_synchronized=sync;
    r.numerical_integrity=finite(r.mean_bellman_target)&&finite(r.mean_loss)&&finite(r.mean_td_error)&&finite(r.gradient_l2_norm)&&finite(r.clipped_gradient_l2_norm);
    return r;
}

std::vector<std::size_t> DevManagerTrainer::validActionIndices(const DevManagerEncodedState& s) const
{std::vector<std::size_t> v;for(std::size_t i=0;i<DEV_MANAGER_ACTION_COUNT;++i)if(s.valid_actions[i])v.push_back(i);return v;}
double DevManagerTrainer::epsilonForSelection(std::uint64_t n) const noexcept
{const double f=std::min(1.0,static_cast<double>(n)/static_cast<double>(config_.epsilon_decay_selections));return config_.epsilon_start+(config_.epsilon_end-config_.epsilon_start)*f;}

DevManagerDecision DevManagerTrainer::materializeDecision(DevManagerAction a,const DevManagerState& s,const DevManager& m)
{
    switch(a){case DevManagerAction::NO_TRADE:return m.makeNoTradeDecision(s);case DevManagerAction::KEEP_CURRENT:return m.makeKeepCurrentDecision(s);
    case DevManagerAction::SELECT_AGENT_001:return m.makeSelectAgentDecision(s,0);case DevManagerAction::SELECT_AGENT_002:return m.makeSelectAgentDecision(s,1);
    case DevManagerAction::SELECT_AGENT_003:return m.makeSelectAgentDecision(s,2);case DevManagerAction::SELECT_AGENT_004:return m.makeSelectAgentDecision(s,3);
    case DevManagerAction::SELECT_AGENT_005:return m.makeSelectAgentDecision(s,4);
} throw std::logic_error("DevManagerTrainer: unreachable action.");
}

DevManagerPolicyDecision DevManagerTrainer::selectDecision(const DevManagerState& state,const DevManager& manager)
{
    manager.validateState(state); const auto e=encoder_.encode(state); const auto q=online_network_.predict(e); const double eps=currentEpsilon();
    const auto valid=validActionIndices(e); if(valid.empty())throw std::logic_error("DevManagerTrainer: no valid actions.");
    const bool explore=unit_distribution_(random_engine_)<eps; std::size_t idx=0;
    if(explore){std::uniform_int_distribution<std::size_t>d(0,valid.size()-1);idx=valid[d(random_engine_)];}
    else idx=actionIndex(q.greedyAction(e.valid_actions));
    const auto action=static_cast<DevManagerAction>(idx); auto decision=materializeDecision(action,state,manager); manager.validateDecision(state,decision);
    ++completed_action_selections_; return {action,decision,q,eps,explore,true};
}
void DevManagerTrainer::resetEpisode() noexcept{}
void DevManagerTrainer::clearExperienceAndResetTraining(){replay_.clear();completed_training_updates_=0;completed_action_selections_=0;random_engine_.seed(config_.random_seed);unit_distribution_.reset();target_network_.copyParametersFrom(online_network_);}
const DevManagerQNetwork& DevManagerTrainer::onlineNetwork()const noexcept{return online_network_;}
const DevManagerQNetwork& DevManagerTrainer::targetNetwork()const noexcept{return target_network_;}
std::size_t DevManagerTrainer::replaySize()const noexcept{return replay_.size();}
std::uint64_t DevManagerTrainer::completedTrainingUpdates()const noexcept{return completed_training_updates_;}
std::uint64_t DevManagerTrainer::completedActionSelections()const noexcept{return completed_action_selections_;}
double DevManagerTrainer::currentEpsilon()const noexcept{return epsilonForSelection(completed_action_selections_);}
const DevManagerTrainingConfig& DevManagerTrainer::config()const noexcept{return config_;}

} // namespace devai::strategy
