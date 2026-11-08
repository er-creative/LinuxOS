#pragma once
#include "devai/approval/DevModelApprovalVerifier.hpp"
#include "devai/strategy/dev_ai_strategy/DevManagerRegistry.hpp"
#include "devai/runtime/DevApprovedRuntimeGateway.hpp"
#include "devai/execution/DevSecureExecutionPath.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace cete_5c3h_test {
namespace fs = std::filesystem;
using devai::approval::DevModelApprovalVerifier;
inline void write(const fs::path& path, const std::string& data) {
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary);
    f << data;
    if (!f) throw std::runtime_error("fixture write failed");
}
inline std::array<std::string,5> ids(int start) {
    std::array<std::string,5> a{};
    for(int i=0;i<5;++i) {
        auto n=std::to_string(start+i);
        a[i]="DevAgent_"+std::string(3-n.size(),'0')+n;
    }
    return a;
}
struct Evidence {
    std::string model, oos, universe;
};
struct Fixture {
    static fs::path prepareRoot(fs::path path) {
        // Reset BEFORE registry construction: registry may create or expect
        // its persistent directory during its constructor.
        fs::remove_all(path);
        fs::create_directories(path / "registry");
        return path;
    }
    fs::path root;
    devai::DevManagerRegistry registry;
    cete::supervision::DevinderjeetSupervisor& central;
    std::map<std::string,Evidence> expected;
    bool trusted{true}, enabled{true}, authorized{true};
    DevModelApprovalVerifier verifier;
    devai::runtime::DevApprovedRuntimeGateway gateway;
    devai::execution::DevSecureExecutionPath execution;

    Fixture(fs::path path, cete::supervision::DevinderjeetSupervisor& s)
      : root(prepareRoot(std::move(path))), registry(root/"registry"), central(s),
        verifier(root/"approvals",root/"models",root/"oos",
          [this](const devai::approval::ApprovalEvidence& e) {
            const auto it=expected.find(e.manager_id);
            return trusted && it!=expected.end() && e.decision=="APPROVED" &&
                   e.model_id=="v1" && e.approval_id=="oos1" &&
                   e.model_sha256==it->second.model &&
                   e.oos_sha256==it->second.oos &&
                   e.universe_sha256==it->second.universe;
          }),
        gateway(registry,central,verifier,
          [this](const std::string& id) { return enabled && expected.count(id)!=0; },
          [this](const std::string& action,const std::string& id) {
              return authorized && action=="submit_order" && expected.count(id)!=0;
          }),
        execution(gateway) {}
    void add(const std::string& expected_id,
             const std::vector<std::string>& symbols) {
        const auto manager=registry.create_manager(symbols);
        if(manager.manager_id!=expected_id)
            throw std::runtime_error("unexpected manager ID in fixture");
        const auto mp=root/"models"/manager.manager_id/"v1.bin";
        const auto op=root/"oos"/manager.manager_id/"oos1.report";
        const auto ap=root/"approvals"/(manager.manager_id+".approval");
        write(mp,"fixture frozen model "+manager.manager_id);
        write(op,"fixture OOS evidence "+manager.manager_id);
        Evidence e{DevModelApprovalVerifier::sha256File(mp),
                   DevModelApprovalVerifier::sha256File(op),
                   DevModelApprovalVerifier::sha256Universe(manager.symbols)};
        expected[manager.manager_id]=e;
        write(ap,"manager_id="+manager.manager_id+"\nmodel_id=v1\nmodel_sha256="+e.model+
              "\noos_sha256="+e.oos+"\nuniverse_sha256="+e.universe+
              "\napproval_id=oos1\ndecision=APPROVED\n");
        registry.set_active(manager.manager_id,true);
        if(!gateway.eligible(manager.manager_id,symbols.front()))
            throw std::runtime_error("fixture approval failed");
    }
    ~Fixture() {
        // All fixture artifacts are isolated from production state.
        std::error_code ec;
        fs::remove_all(root,ec);
    }
};
} // namespace cete_5c3h_test
