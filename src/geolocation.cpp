#include <iostream>
#include <string>
#include <curl/curl.h>

// callback for libcurl to write response into a std::string
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total = size * nmemb;
    output->append((char*)contents, total);
    return total;
}

std::string get_geolocation(const std::string& query) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize libcurl\n";
        return "";
    }

    std::string url = "http://ip-api.com/json/" + query;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: "
                  << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        return "(Unkown, Local Router)";
    }

    curl_easy_cleanup(curl);
    auto find_value = [&](const std::string& key) -> std::string {
        size_t pos = response.find("\"" + key + "\":");
        if (pos == std::string::npos) return "";
        pos = response.find("\"", pos + key.size() + 3);
        if (pos == std::string::npos) return "";
        size_t end = response.find("\"", pos + 1);
        if (end == std::string::npos) return "";
        return response.substr(pos + 1, end - pos - 1);
    };

    std::string country = find_value("country");
    std::string regionName = find_value("regionName");
    std::string city = find_value("city");
    std::string isp = find_value("isp");

    // std::cout << "Query: " << query << std::endl;
    // std::cout << "Location: " << city << ", " << regionName << ", " << country << std::endl;
    // std::cout << "ISP: " << isp << std::endl;
    if (country.empty() && regionName.empty() && city.empty() && isp.empty()) {
        return "(Unknown, Local Router)";
    }
    return "(" + city + ", " + regionName + ", " + country + ", " + isp + ")";
}

