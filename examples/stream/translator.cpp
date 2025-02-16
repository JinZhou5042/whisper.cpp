#include "translator.h"
#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;


static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t totalSize = size * nmemb;
    out->append((char*)contents, totalSize);
    return totalSize;
}

bool translate_text(const std::string& target, const std::string& english_text, std::string& out) {
    if (target.empty() || english_text.empty()) {
        return false;
    }

    const std::string apiUrl = "https://translation.googleapis.com/language/translate/v2";
    const std::string apiKey = "AIzaSyDdJegq9f-lUV31xq9yTgm6D90X8Ht6u4g";

    json requestBody = {
        {"q", english_text},
        {"target", target},
        {"format", "text"}
    };
    std::string jString = requestBody.dump();

    std::string response;
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string fullUrl = apiUrl + "?key=" + apiKey;

    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jString.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, jString.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_cleanup(curl);

    try {
        json jsonResponse = json::parse(response);

        if (jsonResponse.contains("error")) {
            std::cerr << "Error: " << jsonResponse["error"]["message"] << std::endl;
            return false;
        }

        out = jsonResponse["data"]["translations"][0]["translatedText"];
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return false;
    }

    return true;
}
