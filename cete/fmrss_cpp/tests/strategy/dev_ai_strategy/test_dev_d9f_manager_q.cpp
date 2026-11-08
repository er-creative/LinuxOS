#include "devai/strategy/dev_ai_strategy/DevManagerRL_D9F.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace devai::strategy::d9f;
int main(){
 SharedManagerQ m({1.0,0.0,0.0,17});
 State flat;flat.entries_remaining=3;flat.candidate_available=true;flat.candidate_score=1.2;flat.minutes_to_close=200;
 State invested=flat;invested.invested=true;invested.position_age=6;
 assert(m.choose(flat,{Action::WAIT,Action::OPEN},false)==Action::WAIT);
 m.learn(flat,Action::OPEN,.01,invested,{},true);
 assert(std::abs(m.qValue(flat,Action::OPEN)-.01)<1e-12);
 assert(m.choose(flat,{Action::WAIT,Action::OPEN},false)==Action::OPEN);
 assert(m.choose(flat,{Action::WAIT},false)==Action::WAIT);
 m.learn(flat,Action::OPEN,-.02,invested,{},true);
 assert(m.choose(flat,{Action::WAIT,Action::OPEN},false)==Action::WAIT);
 assert(m.stateKey(flat)!=m.stateKey(invested));
 State older=invested;older.position_age=35;
 assert(m.stateKey(older)!=m.stateKey(invested));
 State exhausted=flat;exhausted.entries_remaining=0;
 assert(m.stateKey(exhausted)!=m.stateKey(flat));
 std::cout<<"D9F_MANAGER_Q_TESTS PASS mask=yes terminal=yes state_context=yes\n";
}
