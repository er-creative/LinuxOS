#include "devai/approval/DevModelApprovalVerifier.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerRegistry.hpp"
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace devai::approval {
namespace {
using u32 = std::uint32_t;
constexpr std::array<u32,64> K = {
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
inline u32 r(u32 x,unsigned n) {return (x>>n)|(x<<(32-n));}
class SHA256 {
    std::array<u32,8> h_{{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                           0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
    std::array<unsigned char,64> buf_{};
    std::size_t len_=0;
    std::uint64_t bytes_=0;
    void block(const unsigned char* p) {
        u32 w[64]{};
        for(int i=0;i<16;++i) w[i]=(u32(p[4*i])<<24)|(u32(p[4*i+1])<<16)|(u32(p[4*i+2])<<8)|p[4*i+3];
        for(int i=16;i<64;++i) {
            u32 s0=r(w[i-15],7)^r(w[i-15],18)^(w[i-15]>>3);
            u32 s1=r(w[i-2],17)^r(w[i-2],19)^(w[i-2]>>10);
            w[i]=w[i-16]+s0+w[i-7]+s1;
        }
        u32 a=h_[0],b=h_[1],c=h_[2],d=h_[3],e=h_[4],f=h_[5],g=h_[6],h=h_[7];
        for(int i=0;i<64;++i) {
            u32 s1=r(e,6)^r(e,11)^r(e,25),ch=(e&f)^(~e&g);
            u32 t1=h+s1+ch+K[i]+w[i];
            u32 s0=r(a,2)^r(a,13)^r(a,22),maj=(a&b)^(a&c)^(b&c);
            u32 t2=s0+maj;
            h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h_[0]+=a;h_[1]+=b;h_[2]+=c;h_[3]+=d;
        h_[4]+=e;h_[5]+=f;h_[6]+=g;h_[7]+=h;
    }
public:
    void update(const unsigned char* data,std::size_t n) {
        bytes_+=n;
        for(std::size_t i=0;i<n;++i) {
            buf_[len_++]=data[i];
            if(len_==64) {block(buf_.data());len_=0;}
        }
    }
    std::string finish() {
        const auto bits=bytes_*8;
        const unsigned char pad=0x80;
        update(&pad,1);
        const unsigned char zero=0;
        while(len_!=56) update(&zero,1);
        unsigned char length[8]{};
        for(int i=0;i<8;++i) length[7-i]=static_cast<unsigned char>(bits>>(8*i));
        update(length,8);
        std::ostringstream out;
        out<<std::hex<<std::setfill('0');
        for(auto v:h_) out<<std::setw(8)<<v;
        return out.str();
    }
};
std::string digest(const std::string& s) {
    SHA256 sha;sha.update(reinterpret_cast<const unsigned char*>(s.data()),s.size());return sha.finish();
}
bool safeId(const std::string& s) {
    if(s.size()<14 || s.rfind("DevManager_",0)!=0) return false;
    for(std::size_t i=11;i<s.size();++i) if(s[i]<'0'||s[i]>'9') return false;
    return true;
}
bool safeName(const std::string& s) {
    if(s.empty() || s.size()>128) return false;
    for(char c:s) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                        (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.')) return false;
    return s!="." && s!="..";
}
bool hexDigest(const std::string& s) {
    if(s.size()!=64)return false;
    for(char c:s) if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
std::map<std::string,std::string> readRecord(const std::filesystem::path& file) {
    std::ifstream in(file);if(!in)throw std::runtime_error("Approval record missing");
    std::map<std::string,std::string> record;
    std::string line;
    while(std::getline(in,line)) {
        if(!line.empty() && line.back()=='\r')line.pop_back();
        if(line.empty())continue;
        auto pos=line.find('=');
        if(pos==std::string::npos || pos==0 || !record.emplace(line.substr(0,pos),line.substr(pos+1)).second)
            throw std::runtime_error("Malformed or duplicate approval field");
    }
    constexpr std::array<const char*,7> fields{{"manager_id","model_id","model_sha256",
       "oos_sha256","universe_sha256","approval_id","decision"}};
    if(record.size()!=fields.size())throw std::runtime_error("Unexpected approval fields");
    for(auto f:fields)if(!record.count(f))throw std::runtime_error("Missing approval field");
    return record;
}
} // namespace
DevModelApprovalVerifier::DevModelApprovalVerifier(std::filesystem::path approvals,
    std::filesystem::path models,std::filesystem::path oos,TrustedAttestation trusted)
    : approvals_(std::move(approvals)),models_(std::move(models)),
      oos_(std::move(oos)),trusted_(std::move(trusted)) {}
std::string DevModelApprovalVerifier::sha256File(const std::filesystem::path& file) {
    if(!std::filesystem::is_regular_file(file) || std::filesystem::is_symlink(file))
        throw std::runtime_error("Not a regular artifact file");
    std::ifstream in(file,std::ios::binary);
    if(!in)throw std::runtime_error("Cannot read artifact");
    SHA256 sha;std::array<char,8192> buf{};
    while(in) {
        in.read(buf.data(),static_cast<std::streamsize>(buf.size()));
        auto n=in.gcount();
        if(n>0)sha.update(reinterpret_cast<const unsigned char*>(buf.data()),static_cast<std::size_t>(n));
    }
    if(in.bad())throw std::runtime_error("Artifact read error");
    return sha.finish();
}
std::string DevModelApprovalVerifier::sha256Universe(const std::vector<std::string>& symbols) {
    const auto checked=devai::DevManagerRegistry::validate_symbols(symbols);
    if(checked!=symbols)throw std::invalid_argument("Duplicate symbols in universe");
    std::string normalized;
    for(const auto& s:checked)normalized+=s+"\n";
    return digest(normalized);
}
bool DevModelApprovalVerifier::approved(const std::string& manager_id,
    const std::vector<std::string>& symbols) const {
    try {
        if(!trusted_ || !safeId(manager_id))return false;
        const auto rec=readRecord(approvals_/(manager_id+".approval"));
        ApprovalEvidence ev{rec.at("manager_id"),rec.at("model_id"),
            rec.at("model_sha256"),rec.at("oos_sha256"),rec.at("universe_sha256"),
            rec.at("approval_id"),rec.at("decision")};
        if(ev.manager_id!=manager_id || ev.decision!="APPROVED" ||
           !safeName(ev.model_id) || !safeName(ev.approval_id) ||
           !hexDigest(ev.model_sha256) || !hexDigest(ev.oos_sha256) ||
           !hexDigest(ev.universe_sha256))return false;
        if(ev.universe_sha256!=sha256Universe(symbols))return false;
        if(ev.model_sha256!=sha256File(models_/manager_id/(ev.model_id+".bin")))return false;
        if(ev.oos_sha256!=sha256File(oos_/manager_id/(ev.approval_id+".report")))return false;
        // The trusted authority MUST independently authenticate the decision,
        // model digest, OOS evidence, universe and manager binding.
        return trusted_(ev);
    } catch(...) {return false;}
}
} // namespace devai::approval
