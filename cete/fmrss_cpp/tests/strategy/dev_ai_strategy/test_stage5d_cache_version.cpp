#include "devai/strategy/dev_ai_strategy/DevHistoricalTrainingCache.hpp"
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace devai::strategy;
int main() {
  const auto path = std::filesystem::temp_directory_path() / "cete_stage5d_cache_version_test.bin";
  DevHistoricalTrainingDataset d{};
  d.numerical_integrity = true;
  assert(d.format_version == 4U);
  DevHistoricalTrainingCache::save(path, d);
  assert(DevHistoricalTrainingCache::load(path).format_version == 4U);
  {
    std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
    assert(io);
    io.seekp(8); // eight-byte cache magic precedes uint32 format version
    const std::uint32_t legacy_version = 3U;
    io.write(reinterpret_cast<const char*>(&legacy_version), sizeof(legacy_version));
    assert(io);
  }
  bool rejected = false;
  try { (void)DevHistoricalTrainingCache::load(path); }
  catch (const std::exception&) { rejected = true; }
  std::filesystem::remove(path);
  assert(rejected && "Version-3 historical caches must be rejected");
  std::cout << "Stage 5D cache version 4 / legacy version 3 rejection PASS\n";
}
