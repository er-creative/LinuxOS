#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace devai::universe
{

class SymbolUniverse
{
public:
    explicit SymbolUniverse(
        std::filesystem::path symbols_file
    );

    void load();

    [[nodiscard]]
    const std::vector<std::string>&
    tradableSymbols() const noexcept;

    [[nodiscard]]
    const std::string&
    benchmarkSymbol() const noexcept;

    [[nodiscard]]
    std::size_t
    size() const noexcept;

private:
    std::filesystem::path symbols_file_;

    std::vector<std::string> tradable_symbols_;

    std::string benchmark_symbol_;

    static std::string trim(std::string value);
};

} // namespace devai::universe