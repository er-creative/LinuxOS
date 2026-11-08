// CETE / DevAI - Zerodha Kite access token generator
// Build: g++ -std=c++17 -O2 kite_generate_access_token.cpp -o kite_generate_access_token -lcurl -lcrypto
#include <curl/curl.h>
#include <openssl/sha.h>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

static size_t write_response(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* output = static_cast<std::string*>(userdata);
    const size_t bytes = size * nmemb;
    output->append(ptr, bytes);
    return bytes;
}

static std::string prompt(const std::string& label) {
    std::string value;
    std::cout << label << std::flush;
    if (!std::getline(std::cin, value)) throw std::runtime_error("Input cancelled");
    if (value.empty()) throw std::runtime_error(label + " cannot be empty");
    if (value.find_first_of(" \t\r\n") != std::string::npos)
        throw std::runtime_error("Input contains whitespace. Paste the value without spaces.");
    return value;
}

static std::string sha256hex(const std::string& value) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(value.data()), value.size(), digest);
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (unsigned char byte : digest) stream << std::setw(2) << static_cast<unsigned>(byte);
    return stream.str();
}

int main() {
    try {
        std::cout << "CETE - Kite Access Token Generator\n"
                  << "Your inputs are VISIBLE so you can verify them.\n"
                  << "Use this only in a private terminal; do not share screenshots.\n\n";
        const std::string api_key = prompt("API key: ");
        const std::string api_secret = prompt("API secret: ");
        const std::string request_token = prompt("Fresh request token: ");

        const std::string checksum = sha256hex(api_key + request_token + api_secret);
        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("Unable to initialize curl");
        std::string response;
        long http_status = 0;
        curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "X-Kite-Version: 3");
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
        char* key_encoded = curl_easy_escape(curl, api_key.c_str(), 0);
        char* token_encoded = curl_easy_escape(curl, request_token.c_str(), 0);
        char* checksum_encoded = curl_easy_escape(curl, checksum.c_str(), 0);
        if (!key_encoded || !token_encoded || !checksum_encoded) {
            if (key_encoded) curl_free(key_encoded);
            if (token_encoded) curl_free(token_encoded);
            if (checksum_encoded) curl_free(checksum_encoded);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            throw std::runtime_error("Could not encode request");
        }
        const std::string post_fields = std::string("api_key=") + key_encoded +
            "&request_token=" + token_encoded + "&checksum=" + checksum_encoded;
        curl_free(key_encoded);
        curl_free(token_encoded);
        curl_free(checksum_encoded);
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.kite.trade/session/token");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        std::cout << "\nRequesting access token from Zerodha...\n";
        const CURLcode result = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        if (result != CURLE_OK) throw std::runtime_error(std::string("Network error: ") + curl_easy_strerror(result));
        const json reply = json::parse(response);
        if (http_status != 200 || reply.value("status", "") != "success") {
            std::cerr << "FAILED (HTTP " << http_status << "): "
                      << reply.value("error_type", "API error") << " - "
                      << reply.value("message", "Unknown error") << '\n';
            return 1;
        }
        const std::string access_token = reply.at("data").at("access_token").get<std::string>();
        if (access_token.empty()) throw std::runtime_error("Zerodha returned an empty access token");
        const char* home = std::getenv("HOME");
        if (!home || !*home) throw std::runtime_error("HOME environment variable not set");
        const std::filesystem::path directory = std::filesystem::path(home) / ".config" / "cete";
        std::filesystem::create_directories(directory);
        if (chmod(directory.c_str(), 0700) != 0) throw std::runtime_error("Could not secure token directory");
        const auto path = directory / "kite_access_token";
        const auto temp_path = directory / ("kite_access_token.tmp." + std::to_string(getpid()));
        {
            const mode_t previous_umask = umask(0077);
            std::ofstream file(temp_path, std::ios::trunc);
            umask(previous_umask);
            if (!file) throw std::runtime_error("Could not create token file");
            file << access_token << '\n';
            file.close();
            if (!file) throw std::runtime_error("Could not write token file");
        }
        if (chmod(temp_path.c_str(), 0600) != 0) throw std::runtime_error("Could not secure token file");
        std::filesystem::rename(temp_path, path);
        std::cout << "SUCCESS: Access token generated and saved.\n"
                  << "Location: " << path << "\n"
                  << "Token value is intentionally not displayed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
