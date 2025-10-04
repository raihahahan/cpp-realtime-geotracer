#include <ctime>
#include <iostream>
#include <vector>
#include <iomanip>
#include <numeric>

double timespec_diff_ms(const struct timespec &a, const struct timespec &b) {
    // returns (b - a) in ms
    double s = (double)(b.tv_sec - a.tv_sec);
    double ns = (double)(b.tv_nsec - a.tv_nsec);
    return s * 1000.0 + ns / 1e6;
}

void print_rtt_summary(const std::vector<double> &rtts, const std::string& location) {
    std::vector<double> valid;
    for (double v : rtts) if (v >= 0) valid.push_back(v);

    if (valid.empty()) {
        std::cout << "  *  *  *";
        return;
    }

    double mn = *std::min_element(valid.begin(), valid.end());
    double mx = *std::max_element(valid.begin(), valid.end());
    double sum = std::accumulate(valid.begin(), valid.end(), 0.0);
    double avg = sum / valid.size();

    // print with 1 decimal ms precision
    std::cout.setf(std::ios::fixed);
    std::cout << "  " << location;
    std::cout << "  " << std::setw(3) << std::setprecision(1) << mn << " ms"
         << "  " << std::setw(3) << std::setprecision(1) << avg << " ms"
         << "  " << std::setw(3) << std::setprecision(1) << mx << " ms";
    std::cout.unsetf(std::ios::fixed);
}