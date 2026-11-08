#include "devai/approval/DevModelApprovalVerifier.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;
static void put(const fs::path& p,const std::string& s) {
    fs::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary);out<<s;
    if(!out)throw std::runtime_error("Cannot write test file");
}
int main() {
    using devai::approval::DevModelApprovalVerifier;
    const auto root=fs::temp_directory_path()/"cete_5c3f_approval_test";
    fs::remove_all(root);
    try {
        const std::string id="DevManager_001",model="frozen_v1",approval="oos_001";
        const std::vector<std::string> symbols{"RELIANCE","HDFCBANK"};
        const auto mp=root/"models"/id/(model+".bin");
        const auto op=root/"oos"/id/(approval+".report");
        const auto ap=root/"approvals"/(id+".approval");
        put(mp,"frozen-model-bytes");put(op,"OOS result evidence");
        const auto model_hash=DevModelApprovalVerifier::sha256File(mp);
        const auto oos_hash=DevModelApprovalVerifier::sha256File(op);
        const auto universe_hash=DevModelApprovalVerifier::sha256Universe(symbols);
        put(ap,"manager_id="+id+"\nmodel_id="+model+"\nmodel_sha256="+model_hash+
            "\noos_sha256="+oos_hash+"\nuniverse_sha256="+universe_hash+
            "\napproval_id="+approval+"\ndecision=APPROVED\n");
        assert(DevModelApprovalVerifier::sha256File(mp)==model_hash);
        assert(!DevModelApprovalVerifier(root/"approvals",root/"models",root/"oos").approved(id,symbols));
        bool trusted=false;
        DevModelApprovalVerifier verifier(root/"approvals",root/"models",root/"oos",
            [&](const devai::approval::ApprovalEvidence& ev) {
                return trusted && ev.manager_id==id && ev.model_sha256==model_hash &&
                       ev.oos_sha256==oos_hash && ev.universe_sha256==universe_hash &&
                       ev.approval_id==approval;
            });
        assert(!verifier.approved(id,symbols));
        trusted=true;
        assert(verifier.approved(id,symbols));
        assert(!verifier.approved(id,{"HDFCBANK","RELIANCE"}));
        assert(!verifier.approved("DevManager_002",symbols));
        put(mp,"tampered-model");assert(!verifier.approved(id,symbols));
        put(mp,"frozen-model-bytes");assert(verifier.approved(id,symbols));
        put(op,"tampered OOS");assert(!verifier.approved(id,symbols));
        put(op,"OOS result evidence");assert(verifier.approved(id,symbols));
        fs::remove(ap);assert(!verifier.approved(id,symbols));
        fs::remove_all(root);
        std::cout<<"CETE 5C-3F model approval verifier PASS\n";
    } catch(...) {fs::remove_all(root);throw;}
}
