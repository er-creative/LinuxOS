// CETE — Read-only Kite historical closing-gap probe
#define main cete_downloader_main_unused
#include "kite_multistock_historical_downloader.cpp"
#undef main

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0]
                      << " SYMBOL YYYY-MM-DD\n";
            return 2;
        }

        std::string symbol = argv[1];
        std::string date = argv[2];
        parseDay(date);

        if (symbol == "NIFTY%2050")
            symbol = "NIFTY 50";

        std::string key = env("KITE_API_KEY");
        if (key.empty()) {
            std::cout << "Enter Kite API key: ";
            std::getline(std::cin, key);
            key = trim(key);
        }

        if (key.empty())
            throw std::runtime_error("Missing API key");

        auto token = readToken();
        auto tokens = instrumentTokens(key, token);
        auto instrument = tokens.find(symbol);

        if (instrument == tokens.end())
            throw std::runtime_error("Instrument not found: " + symbol);

        CURL* curl = curl_easy_init();
        if (!curl)
            throw std::runtime_error("curl initialization failed");

        std::string url;

        try {
            url = "https://api.kite.trade/instruments/historical/"
                + instrument->second
                + "/minute?from="
                + urlEncode(curl, date + " 15:15:00")
                + "&to="
                + urlEncode(curl, date + " 15:29:59")
                + "&continuous=0&oi=0";
        } catch (...) {
            curl_easy_cleanup(curl);
            throw;
        }

        curl_easy_cleanup(curl);

        Http response = requestRetry(url, key, token);
        json data = json::parse(response.body);

        if (data.value("status", "") != "success")
            throw std::runtime_error("Kite response unsuccessful");

        std::set<std::string> kiteTimes;

        for (const auto& candle : data.at("data").at("candles")) {
            std::string stamp = candle.at(0).get<std::string>();

            if (stamp.size() >= 16 &&
                stamp.substr(0, 10) == date)
                kiteTimes.insert(stamp.substr(11, 5));
        }

        std::string fileSymbol =
            symbol == "NIFTY 50" ? "NIFTY%2050" : symbol;

        fs::path file =
            fs::path("data/validation/phase319_3a/kite_downloads/minute")
            / (fileSymbol + "_1min.txt");

        std::ifstream input(file);
        if (!input)
            throw std::runtime_error("Cannot open " + file.string());

        std::set<std::string> localTimes;
        std::string line;

        while (std::getline(input, line)) {
            auto fields = csv(line);

            if (fields.size() == 8 &&
                fields[0] == fileSymbol &&
                fields[1] == date)
                localTimes.insert(fields[2]);
        }

        std::cout << "\nSymbol: " << symbol
                  << "\nDate: " << date
                  << "\nKite returned: " << kiteTimes.size()
                  << " candles\n\n";

        std::cout << "TIME   LOCAL  KITE\n";

        for (int minute = 915; minute < 930; ++minute) {
            std::ostringstream ss;
            ss << std::setfill('0')
               << std::setw(2) << minute / 60
               << ":"
               << std::setw(2) << minute % 60;

            std::string time = ss.str();

            std::cout << time << "  "
                      << (localTimes.count(time) ? "YES" : "NO ")
                      << "    "
                      << (kiteTimes.count(time) ? "YES" : "NO")
                      << '\n';
        }

        std::cout << "\nNo files modified.\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
