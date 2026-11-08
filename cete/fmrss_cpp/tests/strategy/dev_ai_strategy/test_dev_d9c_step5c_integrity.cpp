#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Key=std::int64_t;
int digits(const std::string&s,int p,int n){int v=0;for(int i=p;i<p+n;++i){if(s.at(i)<'0'||s.at(i)>'9')throw std::runtime_error("invalid date");v=10*v+s[i]-'0';}return v;}
Key ordinal(int y,unsigned m,unsigned d){y-=m<=2;int era=(y>=0?y:y-399)/400;unsigned yoe=y-era*400;unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;unsigned doe=yoe*365+yoe/4-yoe/100+doy;return static_cast<Key>(era)*146097+doe;}
Key dateKey(const std::string&s){if(s.size()!=10||s[4]!='-'||s[7]!='-')throw std::runtime_error("date must be YYYY-MM-DD");int y=digits(s,0,4),m=digits(s,5,2),d=digits(s,8,2);if(m<1||m>12||d<1||d>31)throw std::runtime_error("invalid date");return ordinal(y,m,d)*1440;}
std::vector<std::string> fields(const std::string&s){std::vector<std::string> v;std::size_t start=0;for(;;){auto p=s.find(',',start);v.push_back(s.substr(start,p==std::string::npos?p:p-start));if(p==std::string::npos)break;start=p+1;}return v;}
std::string read(const std::string&path){std::ifstream in(path);if(!in)throw std::runtime_error("cannot open source: "+path);return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};}
void require(bool ok,const std::string&message){if(!ok)throw std::runtime_error(message);}
int oldBucket(double x){return x<-.002?-2:x<-.0004?-1:x>.002?2:x>.0004?1:0;}
int proposedBucket(double x,double inner,double outer){return x< -outer?-2:x< -inner?-1:x>outer?2:x>inner?1:0;}
struct Stats{std::size_t count=0,oldZero=0,positive=0,negative=0,zero=0;std::vector<double> absForecast;std::array<std::size_t,5> oldCounts{};};
double quantile(const std::vector<double>&v,double p){if(v.empty())throw std::runtime_error("empty training sample");auto ix=static_cast<std::size_t>(p*(v.size()-1));return v[ix];}
}
int main(int argc,char**argv){
 try{
  if(argc!=8)throw std::runtime_error("Usage: test_dev_d9c_step5c_integrity CSV TRAIN_START VALIDATION_START OOS_START D8_CPP EXPORT_CPP D9B_CPP");
  const auto train=dateKey(argv[2]),validation=dateKey(argv[3]),oos=dateKey(argv[4]);require(train<validation&&validation<oos,"invalid chronological split");
  const auto d8=read(argv[5]),exporter=read(argv[6]),d9b=read(argv[7]);
  require(d8.find("target_key>=config_.oos_start_key")!=std::string::npos,"D8 future-target split guard missing");
  require(d8.find("key>=config_.oos_start_key")!=std::string::npos,"D8 input split guard missing");
  require(exporter.find("f.decision_key<train_start")!=std::string::npos,"exporter fit observation cutoff missing");
  require(exporter.find("tc.oos_start_key=train_start")!=std::string::npos,"exporter D8 fit cutoff assignment missing");
  require(exporter.find("tc.horizon_minutes=5")!=std::string::npos,"exporter 5-minute horizon missing");
  require(d9b.find("bucket(o.predicted_forward_return,0.002)")!=std::string::npos,"D9B forecast key differs from audited version; inspect before changing");
  std::cout<<"STEP5C_SOURCE_SPLIT_GUARDS PASS target_crossing_blocked=yes exporter_pre_fit_boundary=yes forecast_horizon=5\n";
  std::ifstream in(argv[1]);require(bool(in),"cannot open CSV");std::string line;
  require(bool(std::getline(in,line))&&line=="symbol,decision_key,close,predicted_forward_return,zscore_1m,zscore_5m,zscore_15m,forecast_ready,shortable,selected_top5","unexpected CSV schema");
  Stats tr,val;std::size_t oosRows=0,preRows=0;Key last=0;
  while(std::getline(in,line)){
   if(line.empty()) continue;
   auto f=fields(line);
   require(f.size()==10,"malformed CSV row");
   const Key key=std::stoll(f[1]);require(key>=last,"nonchronological CSV");last=key;
   if(key>=oos){++oosRows;continue;} // No OOS forecast values inspected.
   if(key<train){++preRows;continue;}
   if(f[7]!="1"||f[9]!="1")continue;
   const double x=std::stod(f[3]);require(std::isfinite(x),"nonfinite forecast");
   Stats& s=key<validation?tr:val;++s.count;
   if(x>0)++s.positive;else if(x<0)++s.negative;else ++s.zero;
   const int b=oldBucket(x);++s.oldCounts[static_cast<std::size_t>(b+2)];if(b==0)++s.oldZero;
   if(key<validation)s.absForecast.push_back(std::abs(x));
  }
  require(tr.count>=100&&val.count>=100,"insufficient eligible training or validation forecasts");
  std::sort(tr.absForecast.begin(),tr.absForecast.end());
  const double q25=quantile(tr.absForecast,.25),q75=quantile(tr.absForecast,.75);
  // Positive and negative forecasts are separated using magnitudes fitted ONLY on training data.
  // This is a diagnostic proposal, not a model change or claim of predictive value.
  const double inner=std::max(q25,1e-12),outer=std::max(q75,inner*1.000001);
  auto print=[&](const char*name,const Stats&s){
   std::cout<<"STEP5C_FORECAST_BUCKET_AUDIT phase="<<name<<" eligible="<<s.count
            <<" old_center_fraction="<<std::setprecision(8)<<double(s.oldZero)/s.count
            <<" old_bins=";for(auto n:s.oldCounts)std::cout<<n<<',';
   std::cout<<" positive="<<s.positive<<" negative="<<s.negative<<" zero="<<s.zero<<'\n';
  };
  print("TRAIN",tr);print("VALIDATION",val);
  std::cout<<"STEP5C_TRAIN_ONLY_CANDIDATE inner_abs_fraction="<<std::setprecision(17)<<inner
           <<" outer_abs_fraction="<<outer<<" inner_bps="<<inner*10000
           <<" outer_bps="<<outer*10000<<"\n";
  std::cout<<"STEP5C_SIGN_SEPARATION_TEST "
           <<(proposedBucket(-outer*2,inner,outer)<0&&proposedBucket(outer*2,inner,outer)>0?"PASS":"FAIL")<<'\n';
  std::cout<<"STEP5C_DATA_PROVENANCE WARNING CSV has no embedded D8_fit_end metadata; verify exporter invocation/log independently\n";
  std::cout<<"STEP5C_OOS_GUARD PASS skipped_oos_rows="<<oosRows<<" inspected_oos_forecasts=no\n";
  std::cout<<"STEP5C_AUDIT_PASS diagnostic_only=yes no_model_changes=yes no_retraining=yes\n";
 }catch(const std::exception&e){std::cerr<<"STEP5C_AUDIT_FAIL "<<e.what()<<'\n';return 1;}
}
