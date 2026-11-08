#include "devai/strategy/dev_ai_strategy/DevManagerRL_D9F.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
namespace devai::strategy::d9f {
namespace {
int bucket(double x) {return x < -2?-3:x < -.7?-2:x < 0?-1:x==0?0:x < .7?1:x < 2?2:3;}
int ageBucket(int x){return x<5?0:x<10?1:x<20?2:x<40?3:4;}
int timeBucket(int x){return x<=10?0:x<=30?1:x<=90?2:3;}
}
SharedManagerQ::SharedManagerQ(Config c):cfg_(c),rng_(c.seed?c.seed:17){
 if(!std::isfinite(c.alpha)||c.alpha<=0||c.alpha>1||!std::isfinite(c.gamma)||c.gamma<0||c.gamma>1||!std::isfinite(c.epsilon)||c.epsilon<0||c.epsilon>1)
  throw std::invalid_argument("D9F invalid Q parameters");
}
double SharedManagerQ::random01(){rng_^=rng_<<13;rng_^=rng_>>7;rng_^=rng_<<17;return (rng_>>11)*(1.0/9007199254740992.0);}
std::string SharedManagerQ::stateKey(const State& s)const{
 if(!std::isfinite(s.candidate_score)||!std::isfinite(s.held_score)||!std::isfinite(s.advantage))throw std::invalid_argument("D9F invalid state features");
 std::ostringstream o;
 o<<(s.forecast_mode?1:0)<<':'<<(s.invested?1:0)<<':'<<bucket(s.candidate_score)<<':'
  <<bucket(s.held_score)<<':'<<bucket(s.advantage)<<':'<<ageBucket(s.position_age)<<':'
  <<timeBucket(s.minutes_to_close)<<':'<<std::min(3,std::max(0,s.entries_remaining))<<':'
  <<(s.candidate_available?1:0);
 return o.str();
}
double SharedManagerQ::qValue(const State&s,Action a)const{
 auto it=q_.find(stateKey(s));return it==q_.end()?0.0:it->second.at(static_cast<std::size_t>(a));
}
Action SharedManagerQ::choose(const State&s,const std::vector<Action>&legal,bool explore){
 if(legal.empty())throw std::invalid_argument("D9F empty legal actions");
 if(explore&&random01()<cfg_.epsilon)return legal[static_cast<std::size_t>(random01()*legal.size())%legal.size()];
 Action best=legal.front();double value=qValue(s,best);
 for(Action a:legal){double v=qValue(s,a);if(v>value){value=v;best=a;}}
 return best;
}
void SharedManagerQ::learn(const State&s,Action a,double reward,const State&next,const std::vector<Action>&legal,bool terminal){
 if(!std::isfinite(reward))throw std::invalid_argument("D9F non-finite reward");
 double future=0.0;
 if(!terminal){
  if(legal.empty())throw std::invalid_argument("D9F empty bootstrap mask");
  future=qValue(next,legal.front());for(Action n:legal)future=std::max(future,qValue(next,n));
 }
 auto& value=q_[stateKey(s)].at(static_cast<std::size_t>(a));
 value+=cfg_.alpha*(reward+cfg_.gamma*future-value);
}
}
