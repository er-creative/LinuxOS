#include "devai/runtime/DevApprovedRuntimeGateway.hpp"
#include "devai/activation/DevManagerActivationController.hpp"
#include "devai/strategy/dev_ai_strategy/DevAISupervisor.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs = std::filesystem;
static void write(const fs::path& path, const std::string& data) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary); out << data;
    if (!out) throw std::runtime_error("test write failed");
}
int main() {
    using devai::approval::DevModelApprovalVerifier;
    const fs::path root = fs::temp_directory_path()/"cete_5c3g_approval_runtime_test";
    fs::remove_all(root);
    try {
        devai::DevManagerRegistry registry(root/"registry");
        const auto manager=registry.create_manager({"RELIANCE","HDFCBANK"});
        registry.set_active(manager.manager_id,false);
        cete::supervision::DevinderjeetSupervisor central;
        central.registerManager({manager.manager_id,
            {"DevAgent_001","DevAgent_002","DevAgent_003","DevAgent_004","DevAgent_005"},1000.0});
        bool auth=true,enabled=true,trust=false;
        devai::strategy::DevAISupervisor supervisor(registry,central,
            [&](const std::string&,const std::string&){return auth;});
        const auto mp=root/"models"/manager.manager_id/"v1.bin";
        const auto op=root/"oos"/manager.manager_id/"oos1.report";
        const auto ap=root/"approvals"/(manager.manager_id+".approval");
        write(mp,"frozen model"); write(op,"OOS evidence");
        const auto mh=DevModelApprovalVerifier::sha256File(mp);
        const auto oh=DevModelApprovalVerifier::sha256File(op);
        const auto uh=DevModelApprovalVerifier::sha256Universe(manager.symbols);
        write(ap,"manager_id="+manager.manager_id+"\nmodel_id=v1\nmodel_sha256="+mh+
            "\noos_sha256="+oh+"\nuniverse_sha256="+uh+
            "\napproval_id=oos1\ndecision=APPROVED\n");
        DevModelApprovalVerifier verifier(root/"approvals",root/"models",root/"oos",
            [&](const devai::approval::ApprovalEvidence& ev){
                return trust && ev.manager_id==manager.manager_id && ev.model_sha256==mh &&
                    ev.oos_sha256==oh && ev.universe_sha256==uh && ev.approval_id=="oos1";
            });
        devai::activation::DevManagerActivationController activation(supervisor,registry,
            [&](const std::string& id,const std::vector<std::string>& shares){return verifier.approved(id,shares);},
            [&](const std::string& id){return enabled && id==manager.manager_id;});
        devai::runtime::DevApprovedRuntimeGateway gateway(registry,central,verifier,
            [&](const std::string& id){return enabled && id==manager.manager_id;},
            [&](const std::string&,const std::string& id){return auth && id==manager.manager_id;});
        assert(!gateway.eligible(manager.manager_id,"RELIANCE"));
        try {activation.activate(manager.manager_id);assert(false);} catch(const std::runtime_error&) {}
        assert(!registry.load_manager(manager.manager_id).active);
        trust=true;
        activation.activate(manager.manager_id);
        assert(gateway.eligible(manager.manager_id,"RELIANCE"));
        assert(!gateway.eligible(manager.manager_id,"NIFTY%2050"));
        assert(!gateway.eligible("DevManager_002","RELIANCE"));
        write(mp,"tampered model");assert(!gateway.eligible(manager.manager_id,"RELIANCE"));
        write(mp,"frozen model");assert(gateway.eligible(manager.manager_id,"RELIANCE"));
        write(op,"tampered OOS");assert(!gateway.eligible(manager.manager_id,"RELIANCE"));
        write(op,"OOS evidence");assert(gateway.eligible(manager.manager_id,"RELIANCE"));
        registry.assign_shares(manager.manager_id,{"RELIANCE","HDFCBANK","SBIN"});
        assert(!gateway.eligible(manager.manager_id,"RELIANCE"));
        registry.assign_shares(manager.manager_id,manager.symbols);
        enabled=false;assert(!gateway.eligible(manager.manager_id,"RELIANCE"));
        enabled=true;auth=false;assert(!gateway.eligible(manager.manager_id,"RELIANCE"));
        auth=true;central.setHalted(true);assert(!gateway.eligible(manager.manager_id,"RELIANCE"));
        central.setHalted(false);trust=false;assert(!gateway.eligible(manager.manager_id,"RELIANCE"));
        trust=true;activation.deactivate(manager.manager_id);
        assert(!gateway.eligible(manager.manager_id,"RELIANCE"));
        devai::runtime::DevApprovedRuntimeGateway no_callbacks(registry,central,verifier);
        assert(!no_callbacks.eligible(manager.manager_id,"RELIANCE"));
        fs::remove_all(root);
        std::cout << "CETE 5C-3G approval/runtime gateway PASS\n";
    } catch(...) {fs::remove_all(root);throw;}
}
