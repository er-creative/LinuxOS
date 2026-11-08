#include "devai/benchmark/DevCandidateModelRegistry.hpp"
#include <openssl/evp.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace fs=std::filesystem;
namespace devai::benchmark {
namespace {
std::string readText(const fs::path& path) {
    std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("cannot read "+path.string());
    return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
std::string quoted(const std::string& s) {
    std::string o="\"";for(char c:s) {if(c=='\\'||c=='"')o+='\\';if(c=='\n'||c=='\r')throw std::runtime_error("invalid manifest field");o+=c;}return o+'"';
}
void requireFile(const fs::path& p) {
    if(!fs::is_regular_file(p)||fs::file_size(p)==0)throw std::runtime_error("missing/empty checkpoint: "+p.string());
}
}
std::string DevCandidateModelRegistry::sha256(const fs::path& path) {
    requireFile(path);std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("cannot open checkpoint");
    EVP_MD_CTX* ctx=EVP_MD_CTX_new();if(!ctx)throw std::runtime_error("EVP allocation failed");
    try {
        if(EVP_DigestInit_ex(ctx,EVP_sha256(),nullptr)!=1)throw std::runtime_error("SHA256 init failed");
        std::array<char,65536> buffer{};
        while(f) {f.read(buffer.data(),buffer.size());const auto n=f.gcount();
            if(n>0&&EVP_DigestUpdate(ctx,buffer.data(),static_cast<std::size_t>(n))!=1)throw std::runtime_error("SHA256 update failed");}
        if(!f.eof())throw std::runtime_error("checkpoint read failed");
        unsigned char digest[EVP_MAX_MD_SIZE];unsigned len=0;
        if(EVP_DigestFinal_ex(ctx,digest,&len)!=1||len!=32)throw std::runtime_error("SHA256 final failed");
        EVP_MD_CTX_free(ctx);std::ostringstream o;o<<std::hex<<std::setfill('0');
        for(unsigned i=0;i<len;++i) { o<<std::setw(2)<<unsigned(digest[i]); }
        return o.str();
    } catch(...) {EVP_MD_CTX_free(ctx);throw;}
}
DevFrozenCandidate DevCandidateModelRegistry::freezeCandidate0(const fs::path& source,const fs::path& directory) {
    requireFile(source);
    if(source.filename()=="dev_shared_warmstart.model")throw std::runtime_error("contextual-bandit warm start is not Candidate 0 sequential RL");
    if(fs::exists(directory))throw std::runtime_error("freeze directory already exists: "+directory.string());
    if(!directory.parent_path().empty())fs::create_directories(directory.parent_path());
    fs::create_directory(directory);
    try {
        const fs::path frozen=directory/"candidate_0.chk";
        fs::copy_file(source,frozen,fs::copy_options::none);
        const auto before=sha256(source),after=sha256(frozen);
        if(before!=after)throw std::runtime_error("checkpoint copy integrity mismatch");
        const fs::path direction=source.string()+".direction.txt";
        std::string sidecar="NOT_PRESENT";
        if(fs::is_regular_file(direction)) {
            sidecar=readText(direction);
            std::ofstream copy(directory/"candidate_0.direction.txt",std::ios::binary);
            copy<<sidecar;if(!copy)throw std::runtime_error("sidecar copy failed");
        }
        std::ofstream manifest(directory/"candidate_0.manifest.txt",std::ios::binary);
        if(!manifest)throw std::runtime_error("manifest open failed");
        manifest<<"CETE_H1_CANDIDATE_0_V1\n"<<after<<"\n"<<fs::file_size(frozen)<<"\n";
        if(!manifest)throw std::runtime_error("manifest write failed");
        std::ofstream provenance(directory/"candidate_0.provenance.json",std::ios::binary);
        if(!provenance)throw std::runtime_error("provenance open failed");
        provenance<<"{\n  \"candidate\": \"candidate_0_sequential_td\",\n  \"source\": "
                  <<quoted(fs::absolute(source).string())<<",\n  \"sha256\": "<<quoted(after)
                  <<",\n  \"bytes\": "<<fs::file_size(frozen)
                  <<",\n  \"short_certified\": false,\n  \"checkpoint_load_verified\": false\n}\n";
        if(!provenance)throw std::runtime_error("provenance write failed");
        return {frozen,after,fs::file_size(frozen),fs::absolute(source).string(),sidecar};
    } catch(...) {fs::remove_all(directory);throw;}
}
void DevCandidateModelRegistry::verifyCandidate0(const fs::path& directory) {
    const auto manifest=readText(directory/"candidate_0.manifest.txt");
    std::istringstream in(manifest);std::string magic,hash,bytes,extra;
    if(!std::getline(in,magic)||magic!="CETE_H1_CANDIDATE_0_V1"||
       !std::getline(in,hash)||hash.size()!=64||!std::getline(in,bytes)||
       std::getline(in,extra))throw std::runtime_error("invalid Candidate 0 manifest");
    const auto checkpoint=directory/"candidate_0.chk";
    if(sha256(checkpoint)!=hash||std::to_string(fs::file_size(checkpoint))!=bytes)
        throw std::runtime_error("Candidate 0 checksum/size mismatch");
}
}
