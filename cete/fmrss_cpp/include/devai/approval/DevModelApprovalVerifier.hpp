#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace devai::approval {
// Attestation MUST be backed by a separately trusted OOS approval authority.
// Do not implement this callback as `return true` in production.
struct ApprovalEvidence {
    std::string manager_id;
    std::string model_id;
    std::string model_sha256;
    std::string oos_sha256;
    std::string universe_sha256;
    std::string approval_id;
    std::string decision;
};
using TrustedAttestation = std::function<bool(const ApprovalEvidence&)>;

class DevModelApprovalVerifier {
public:
    DevModelApprovalVerifier(std::filesystem::path approval_root,
                             std::filesystem::path model_root,
                             std::filesystem::path oos_root,
                             TrustedAttestation trusted = {});
    // Returns false for missing/untrusted/mismatched evidence; never writes files.
    [[nodiscard]] bool approved(const std::string& manager_id,
                                const std::vector<std::string>& symbols) const;
    [[nodiscard]] static std::string sha256File(const std::filesystem::path& file);
    [[nodiscard]] static std::string sha256Universe(const std::vector<std::string>& symbols);
private:
    std::filesystem::path approvals_, models_, oos_;
    TrustedAttestation trusted_;
};
} // namespace devai::approval
