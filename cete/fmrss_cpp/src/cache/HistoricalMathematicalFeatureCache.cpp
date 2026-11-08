#include "devai/cache/HistoricalMathematicalFeatureCache.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace devai::cache {
namespace {
using State = HistoricalMathematicalFeatureCache::State;
using Horizon = decltype(State{}.one_minute);
// Raw byte serialization is allowed only for trivial numeric horizon states.
// If this assertion fails, a field-wise codec must be implemented before caching.
static_assert(std::is_trivially_copyable_v<Horizon>,
              "MathematicalHorizonState is non-trivial: implement field-wise serialization");
constexpr std::uint64_t MAGIC = 0x4345544535423031ULL; // CETE5B01
constexpr std::uint64_t FORMAT = 2; // checksum-protected cache format
constexpr std::uint64_t FEATURE_REVISION = 1; // BUMP when mathematical formulas change
constexpr std::uint64_t MAX_CACHE_BYTES = 256ULL * 1024 * 1024;
constexpr std::uint64_t FNV_OFFSET = 14695981039346656037ULL;
constexpr std::uint64_t FNV_PRIME = 1099511628211ULL;
void mix(std::uint64_t& h, const char* data, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) { h ^= static_cast<unsigned char>(data[i]); h *= FNV_PRIME; }
}
std::uint64_t checksumPayload(const std::filesystem::path& file, std::uint64_t length) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open cache for checksum");
    std::array<char, 65536> buffer{};
    std::uint64_t h = FNV_OFFSET;
    while (length > 0) {
        const auto want = static_cast<std::streamsize>(
            std::min<std::uint64_t>(length, buffer.size()));
        in.read(buffer.data(), want);
        if (in.gcount() != want) throw std::runtime_error("Truncated cache checksum input");
        mix(h, buffer.data(), static_cast<std::size_t>(want));
        length -= static_cast<std::uint64_t>(want);
    }
    return h;
}
template <class T> void write(std::ostream& os, const T& v) {
    static_assert(std::is_trivially_copyable_v<T>);
    os.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
template <class T> bool read(std::istream& is, T& v) {
    static_assert(std::is_trivially_copyable_v<T>);
    return static_cast<bool>(is.read(reinterpret_cast<char*>(&v), sizeof(v)));
}
void writeString(std::ostream& os, const std::string& s) {
    const std::uint64_t n = s.size(); write(os, n); os.write(s.data(), n);
}
bool readString(std::istream& is, std::string& s) {
    std::uint64_t n{}; if (!read(is,n) || n > 1024) return false;
    s.resize(static_cast<std::size_t>(n));
    return static_cast<bool>(is.read(s.data(), static_cast<std::streamsize>(n)));
}
void writeState(std::ostream& os, const State& s) {
    writeString(os,s.stock_symbol); writeString(os,s.benchmark_symbol);
    write(os,s.decision_time);
    write(os,s.one_minute); write(os,s.five_minute); write(os,s.fifteen_minute);
    write(os,s.one_minute_ready); write(os,s.five_minute_ready);
    write(os,s.fifteen_minute_ready); write(os,s.numerical_integrity); write(os,s.fully_ready);
}
bool readState(std::istream& is, State& s) {
    return readString(is,s.stock_symbol) && readString(is,s.benchmark_symbol) &&
           read(is,s.decision_time) && read(is,s.one_minute) && read(is,s.five_minute) &&
           read(is,s.fifteen_minute) && read(is,s.one_minute_ready) &&
           read(is,s.five_minute_ready) && read(is,s.fifteen_minute_ready) &&
           read(is,s.numerical_integrity) && read(is,s.fully_ready);
}
}
HistoricalMathematicalFeatureCache::HistoricalMathematicalFeatureCache(std::filesystem::path root)
    : root_(std::move(root)) {}
std::filesystem::path HistoricalMathematicalFeatureCache::pathFor(
    const std::string& stock, const std::string& benchmark, std::int64_t end_time) const {
    // Avoid untrusted path traversal; symbol names become hex-encoded filenames.
    auto encode=[](const std::string& s) {
        constexpr char hex[]="0123456789abcdef"; std::string out;
        for (unsigned char c : s) {out += hex[c >> 4]; out += hex[c & 15];} return out;
    };
    return root_ / (encode(stock)+"_"+encode(benchmark)+"_"+std::to_string(end_time)+".bin");
}
std::uint64_t HistoricalMathematicalFeatureCache::fingerprint(
    const std::vector<std::filesystem::path>& sources) {
    std::uint64_t h=FNV_OFFSET;
    std::array<char,65536> buf{};
    for (const auto& path : sources) {
        const auto p=path.lexically_normal().string(); mix(h,p.data(),p.size());
        std::ifstream in(path,std::ios::binary);
        if (!in) throw std::runtime_error("Cannot fingerprint source: "+p);
        while (in) {in.read(buf.data(),buf.size()); auto n=in.gcount(); if(n>0) mix(h,buf.data(),static_cast<std::size_t>(n));}
        if (!in.eof()) throw std::runtime_error("Source read error: "+p);
    }
    return h;
}
bool HistoricalMathematicalFeatureCache::load(
    const std::string& stock, const std::string& benchmark, std::int64_t end_time,
    std::uint64_t source_fingerprint, const std::vector<std::int64_t>& requested,
    States& out) const {
    std::ifstream in(pathFor(stock,benchmark,end_time),std::ios::binary);
    if (!in) return false;
    std::uint64_t magic{},version{},revision{},fingerprint{},horizon_size{},count{},state_count{};
    std::int64_t stored_end{};
    if (!read(in,magic)||!read(in,version)||!read(in,revision)||!read(in,fingerprint)||
        !read(in,horizon_size)||!read(in,stored_end)||!read(in,count)) return false;
    if(magic!=MAGIC||version!=FORMAT||revision!=FEATURE_REVISION||
       fingerprint!=source_fingerprint||horizon_size!=sizeof(Horizon)||
       stored_end!=end_time||count!=requested.size()||count>1000000) return false;
    for(auto expected:requested) {std::int64_t t{};if(!read(in,t)||t!=expected)return false;}
    if(!read(in,state_count)||state_count>count)return false;
    States candidate;
    for(std::uint64_t i=0;i<state_count;++i) {
        State s{}; if(!readState(in,s)||s.stock_symbol!=stock||s.benchmark_symbol!=benchmark||
           s.decision_time<=0||s.decision_time>end_time||
           !std::binary_search(requested.begin(),requested.end(),s.decision_time)||
           !candidate.emplace(s.decision_time,std::move(s)).second)return false;
    }
    char extra{};if(in.read(&extra,1))return false;
    if(!in.eof())return false;
    out=std::move(candidate);return true;
}
void HistoricalMathematicalFeatureCache::save(
    const std::string& stock, const std::string& benchmark, std::int64_t end_time,
    std::uint64_t source_fingerprint, const std::vector<std::int64_t>& requested,
    const States& states) const {
    std::filesystem::create_directories(root_);
    const auto final=pathFor(stock,benchmark,end_time);
    const auto temp=final.string()+".tmp";
    try {
        std::ofstream os(temp,std::ios::binary|std::ios::trunc);
        if(!os)throw std::runtime_error("Cannot create cache temporary file");
        write(os,MAGIC);write(os,FORMAT);write(os,FEATURE_REVISION);
        write(os,source_fingerprint);write(os,static_cast<std::uint64_t>(sizeof(Horizon)));
        write(os,end_time);write(os,static_cast<std::uint64_t>(requested.size()));
        for(auto t:requested)write(os,t);
        write(os,static_cast<std::uint64_t>(states.size()));
        for(const auto& [t,s]:states) {
            if(t!=s.decision_time||s.stock_symbol!=stock||s.benchmark_symbol!=benchmark)
                throw std::runtime_error("Invalid state identity for cache");
            writeState(os,s);
        }
        os.flush();if(!os)throw std::runtime_error("Cache write failed");os.close();
        std::filesystem::rename(temp,final);
    } catch (...) {std::error_code ec;std::filesystem::remove(temp,ec);throw;}
}
}
