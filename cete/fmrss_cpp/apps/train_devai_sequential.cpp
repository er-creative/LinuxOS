#include "devai/strategy/dev_ai_strategy/DevSequentialRLTrainer.hpp"
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
int main(int argc, char** argv) {
 if (argc!=5 && argc!=6 && argc!=7) {
  std::cerr<<"Usage: "<<argv[0]<<" <train.cache> <validation.cache> <minute_folder> <checkpoint> [epochs] [learning_rate]\n";
  return 2;
 }
 try {
  devai::strategy::DevSequentialRLConfig c{};
  c.cache=argv[1];c.validation_cache=argv[2];c.minute_folder=argv[3];c.checkpoint=argv[4];
  if(argc>=6)c.epochs=std::stoul(argv[5]);
  if(argc>=7)c.learning_rate=std::stod(argv[6]);
  // Cash-equity LONG-only architecture: SHORT is deliberately not exposed.
  const auto r=devai::strategy::DevSequentialRLTrainer(c).run();
  std::cout<<"CETE chronological shared RL training completed\n"
           <<"Train decisions: "<<r.train_decisions<<" Manager updates: "<<r.manager_updates
           <<" Agent updates: "<<r.agent_updates<<" Replay size: "<<r.replay_transitions
           <<"\nMean TD loss: "<<r.mean_td_loss<<" Validation TD loss: "<<r.validation_td_loss
           <<"\nLONG entries: "<<r.long_entries<<" SHORT entries: "<<r.short_entries
           <<" Forced exits: "<<r.forced_exits
           <<"\nLast training equity: "<<r.last_training_equity
           <<"\nCheckpoint: "<<c.checkpoint<<'\n';
  return 0;
 }catch(const std::exception& e){std::cerr<<"Training FAILED: "<<e.what()<<'\n';return 1;}
}
