// CETE / DevAI - read-only Kite historical downloader (ITC smoke test)
// Build: g++ -std=c++17 -O2 tools/kite_historical_downloader.cpp -o build/kite_historical_downloader -lcurl
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

static size_t receive(char* ptr, size_t size, size_t count, void* user) {
    const size_t bytes = size * count;
    static_cast<std::string*>(user)->append(ptr, bytes);
    return bytes;
}
struct HttpResult { long code = 0; std::string body; };
static HttpResult request(const std::string& url, const std::string& key, const std::string& token) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl_easy_init failed");
    HttpResult result;
    curl_slist* headers = nullptr;
    const std::string auth = "Authorization: token " + key + ":" + token;
    headers = curl_slist_append(headers, "X-Kite-Version: 3");
    headers = curl_slist_append(headers, auth.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 45L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    const CURLcode code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (code != CURLE_OK) throw std::runtime_error(std::string("Network error: ") + curl_easy_strerror(code));
    return result;
}
static std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}
static std::vector<std::string> csvFields(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') {
            if (quoted && i + 1 < line.size() && line[i+1] == '"') { field += '"'; ++i; }
            else quoted = !quoted;
        } else if (c == ',' && !quoted) { fields.push_back(trim(field)); field.clear(); }
        else field += c;
    }
    if (quoted) throw std::runtime_error("Malformed CSV quotation");
    fields.push_back(trim(field));
    return fields;
}
static std::string readToken() {
    const char* home = std::getenv("HOME");
    if (!home) throw std::runtime_error("HOME environment variable not found");
    std::ifstream in(fs::path(home) / ".config/cete/kite_access_token");
    std::string token;
    if (!std::getline(in, token) || trim(token).empty())
        throw std::runtime_error("No access token in ~/.config/cete/kite_access_token");
    return trim(token);
}
static std::string apiError(const HttpResult& result) {
    try {
        const auto j = json::parse(result.body);
        return j.value("error_type", std::string("API error")) + ": " + j.value("message", std::string("Unknown"));
    } catch (...) { return "HTTP " + std::to_string(result.code) + " (non-JSON response)"; }
}
static std::string findITCToken(const std::string& csv) {
    std::istringstream input(csv);
    std::string line;
    if (!std::getline(input, line)) throw std::runtime_error("Empty instruments CSV");
    const auto headings = csvFields(line);
    std::map<std::string, size_t> col;
    for (size_t i=0; i<headings.size(); ++i) col[headings[i]] = i;
    for (const auto& required : {"instrument_token", "tradingsymbol", "exchange", "instrument_type"})
        if (!col.count(required)) throw std::runtime_error(std::string("Missing instruments CSV column: ") + required);
    std::set<std::string> matches;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        const auto row = csvFields(line);
        if (row.size() < headings.size()) continue;
        if (row[col["tradingsymbol"]] == "ITC" && row[col["exchange"]] == "NSE" &&
            row[col["instrument_type"]] == "EQ") matches.insert(row[col["instrument_token"]]);
    }
    if (matches.size() != 1) throw std::runtime_error("Expected exactly one NSE ITC EQ instrument; found " + std::to_string(matches.size()));
    return *matches.begin();
}
static std::string number(const json& value) {
    if (!value.is_number()) throw std::runtime_error("OHLCV value is not numeric");
    std::ostringstream s;
    s << std::setprecision(15) << value.get<double>();
    return s.str();
}
static std::vector<std::string> formatCandles(const json& candles, const std::string& interval) {
    if (!candles.is_array()) throw std::runtime_error("Invalid candles payload");
    std::map<std::string, std::string> lines;
    for (const auto& c : candles) {
        if (!c.is_array() || c.size() < 6 || !c[0].is_string()) throw std::runtime_error("Malformed candle");
        const std::string stamp = c[0].get<std::string>();
        if (stamp.size() < 16 || stamp.substr(10,1) != "T") throw std::runtime_error("Unexpected candle timestamp: " + stamp);
        if (stamp.find("+0530") == std::string::npos && stamp.find("+05:30") == std::string::npos)
            throw std::runtime_error("Expected Asia/Kolkata offset in timestamp: " + stamp);
        const std::string date = stamp.substr(0,10), time = stamp.substr(11,5);
        if (date != "2025-01-06" || time < "09:15" || time > "10:15")
            throw std::runtime_error("Unexpected candle outside requested date/window: " + stamp);
        const double o=c[1].get<double>(), h=c[2].get<double>(), l=c[3].get<double>(), cl=c[4].get<double>(), v=c[5].get<double>();
        if (!(o>0 && l>0 && h>=o && h>=cl && l<=o && l<=cl && v>=0))
            throw std::runtime_error("Invalid OHLCV: " + stamp);
        std::string line = "ITC," + date + "," + time;
        for (size_t i=1;i<=5;++i) line += "," + number(c[i]);
        if (!lines.emplace(date + " " + time, line).second) throw std::runtime_error("Duplicate API candle: " + stamp);
    }
    std::vector<std::string> out;
    for (const auto& [_, line] : lines) out.push_back(line);
    std::cout << "  " << interval << ": " << out.size() << " candles returned\n";
    return out;
}
int main() {
    try {
        const char* keyEnv = std::getenv("KITE_API_KEY");
        // Public app identifier; override via KITE_API_KEY if you regenerate/change applications.
        const std::string key = keyEnv && *keyEnv ? keyEnv : "g6n26amm0iri0mzk";
        const std::string token = readToken();
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) throw std::runtime_error("curl initialization failed");
        std::cout << "CETE / Kite historical downloader (ITC, 2025-01-06)\n";
        std::cout << "Read-only: existing market data files will NOT be modified.\n";
        const auto instruments = request("https://api.kite.trade/instruments/NSE", key, token);
        if (instruments.code != 200) throw std::runtime_error("Instrument lookup failed: " + apiError(instruments));
        const std::string instrumentToken = findITCToken(instruments.body);
        std::cout << "NSE ITC instrument token: " << instrumentToken << "\n";
        const fs::path outputDir = "/home/devinderjeet/fmrss_cpp/data/validation/phase319_3a/kite_downloads";
        fs::create_directories(outputDir / "minute");
        fs::create_directories(outputDir / "5minutes");
        fs::create_directories(outputDir / "15minutes");
        const std::vector<std::pair<std::string,std::string>> intervals = {
            {"minute", "ITC_1min.txt"}, {"5minute", "ITC_5mins.txt"}, {"15minute", "ITC_15mins.txt"}
        };
        for (const auto& [interval, filename] : intervals) {
            const std::string url = "https://api.kite.trade/instruments/historical/" + instrumentToken +
                "/" + interval + "?from=2025-01-06%2009%3A15%3A00&to=2025-01-06%2010%3A15%3A00&continuous=0&oi=0";
            const auto response = request(url, key, token);
            if (response.code != 200) throw std::runtime_error(interval + " download failed: " + apiError(response));
            const auto doc = json::parse(response.body);
            if (doc.value("status", std::string("")) != "success")
                throw std::runtime_error(interval + " response: " + doc.value("message", std::string("not successful")));
            const auto lines = formatCandles(doc.at("data").at("candles"), interval);
            if (lines.empty()) throw std::runtime_error(interval + ": no candles returned; existing files unchanged");
            const fs::path folder = (interval == "minute") ? "minute" :
                (interval == "5minute") ? "5minutes" : "15minutes";
            const fs::path path = outputDir / folder / filename;
            std::ofstream file(path, std::ios::trunc);
            if (!file) throw std::runtime_error("Cannot write: " + path.string());
            for (const auto& line : lines) file << line << '\n';
            file.close();
            if (!file) throw std::runtime_error("Failed writing: " + path.string());
            std::cout << "  Saved: " << path << '\n';
            if (interval == "minute") {
                std::set<std::string> available;
                for (const auto& line : lines) {
                    const auto cols = csvFields(line);
                    available.insert(cols.at(2));
                }
                std::cout << "  Previously missing ITC minutes (09:16-09:38):\n";
                int recovered=0;
                for (int m=16; m<=38; ++m) {
                    std::ostringstream t; t << "09:" << std::setfill('0') << std::setw(2) << m;
                    if (available.count(t.str())) ++recovered;
                    else std::cout << "    Still unavailable: " << t.str() << '\n';
                }
                std::cout << "  Available in API response: " << recovered << "/23\n";
            }
        }
        curl_global_cleanup();
        std::cout << "DONE. Review staging files; original CETE data remains unchanged.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
