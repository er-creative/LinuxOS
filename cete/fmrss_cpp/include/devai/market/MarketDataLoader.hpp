#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "devai/market/Candle.hpp"

namespace devai::market
{

class MarketDataLoader
{
public:
    struct LoadResult
    {
        std::vector<Candle> candles;

        std::size_t total_rows{0};
        std::size_t loaded_rows{0};
        std::size_t skipped_rows{0};
        std::size_t invalid_rows{0};
        std::size_t duplicate_rows{0};
    };

    explicit MarketDataLoader(
        std::filesystem::path data_folder
    );

    [[nodiscard]]
    LoadResult loadFile(
        const std::string& filename
    ) const;

    [[nodiscard]]
    LoadResult loadPath(
        const std::filesystem::path& file_path
    ) const;

    [[nodiscard]]
    const std::filesystem::path&
    dataFolder() const noexcept;

private:
    std::filesystem::path data_folder_;

    [[nodiscard]]
    static std::vector<std::string>
    splitLine(const std::string& line);

    [[nodiscard]]
    static std::string
    trim(std::string value);

    [[nodiscard]]
    static bool
    looksLikeHeader(
        const std::vector<std::string>& fields
    );

    [[nodiscard]]
    static std::int64_t
    parseTimestamp(
        const std::string& date,
        const std::string& time
    );

    [[nodiscard]]
    static Candle
    parseCandle(
        const std::vector<std::string>& fields
    );
};

} // namespace devai::market