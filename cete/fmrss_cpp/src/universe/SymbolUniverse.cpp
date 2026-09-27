#include "devai/universe/SymbolUniverse.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

namespace devai::universe
{

SymbolUniverse::SymbolUniverse(
    std::filesystem::path symbols_file
)
    : symbols_file_(std::move(symbols_file))
{
    if (symbols_file_.empty())
    {
        throw std::invalid_argument(
            "SymbolUniverse: symbols file is empty."
        );
    }
}


std::string
SymbolUniverse::trim(std::string value)
{
    const auto not_space =
        [](unsigned char character)
        {
            return !std::isspace(character);
        };

    value.erase(
        value.begin(),
        std::find_if(
            value.begin(),
            value.end(),
            not_space
        )
    );

    value.erase(
        std::find_if(
            value.rbegin(),
            value.rend(),
            not_space
        ).base(),
        value.end()
    );

    return value;
}


void SymbolUniverse::load()
{
    tradable_symbols_.clear();
    benchmark_symbol_.clear();

    std::ifstream input(symbols_file_);

    if (!input.is_open())
    {
        throw std::runtime_error(
            "Unable to open symbols file: " +
            symbols_file_.string()
        );
    }

    std::unordered_set<std::string> seen;

    std::string line;

    while (std::getline(input, line))
    {
        std::string symbol = trim(line);

        if (symbol.empty())
        {
            continue;
        }

        if (!seen.insert(symbol).second)
        {
            throw std::runtime_error(
                "Duplicate symbol in shares.txt: " +
                symbol
            );
        }

        if (symbol == "NIFTY%2050")
        {
            benchmark_symbol_ = symbol;
            continue;
        }

        tradable_symbols_.push_back(
            std::move(symbol)
        );
    }

    if (tradable_symbols_.empty())
    {
        throw std::runtime_error(
            "No tradable symbols found."
        );
    }

    if (benchmark_symbol_.empty())
    {
        throw std::runtime_error(
            "NIFTY%2050 benchmark not found."
        );
    }
}


const std::vector<std::string>&
SymbolUniverse::tradableSymbols() const noexcept
{
    return tradable_symbols_;
}


const std::string&
SymbolUniverse::benchmarkSymbol() const noexcept
{
    return benchmark_symbol_;
}


std::size_t
SymbolUniverse::size() const noexcept
{
    return tradable_symbols_.size();
}

} // namespace devai::universe