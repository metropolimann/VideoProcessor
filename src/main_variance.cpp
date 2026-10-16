#include "video_variance.h"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

void printUsage(const char* prog) {
    std::cout << "Usage:\n"
              << "  " << prog << " <input.mp4> <start_time> <end_time> <n> <m>\n"
              << "\nArguments:\n"
              << "  input.mp4    Path to input video\n"
              << "  start_time   Start time in H:M:S format (e.g., 00:01:00)\n"
              << "  end_time     End time in H:M:S format (e.g., 00:02:00)\n"
              << "  n            Number of samples to analyze\n"
              << "  m            Frame distance between frames A and B (0 = consecutive)\n"
              << "\nExample:\n"
              << "  " << prog << " video.mp4 00:00:00 00:01:00 10 5\n"
              << "  Analyzes 10 samples over 1 minute, comparing frames 5 apart\n"
              << "\nOutput:\n"
              << "  - 100% = frames are identical (no change)\n"
              << "  - 0% = frames are completely different (maximum change)\n"
              << "  - Lower % = more motion/variation in that period\n";
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        printUsage(argv[0]);
        return 1;
    }

    std::string inputFile = argv[1];
    std::string startTime = argv[2];
    std::string endTime = argv[3];
    int n = 0, m = 0;

    try {
        n = std::stoi(argv[4]);
        m = std::stoi(argv[5]);
    } catch (const std::exception& e) {
        std::cerr << "Error: Invalid numeric arguments. " << e.what() << std::endl;
        return 1;
    }

    if (!fs::exists(inputFile)) {
        std::cerr << "Error: Input file does not exist: " << inputFile << std::endl;
        return 1;
    }

    // CORRECTED: Wrapped in try-catch to prevent crash on bad time format
    std::vector<VarianceResult> results;
    try {
        results = analyzeVideoVariance(inputFile, startTime, endTime, n, m);
    } catch (const std::exception& e) {
        std::cerr << "Error during variance analysis: " << e.what() << std::endl;
        return 1;
    }

    if (results.empty()) {
        std::cerr << "Analysis failed or no results." << std::endl;
        return 1;
    }

    // Print results
    std::cout << "\nVariance Results:\n";
    std::cout << "Sample | Difference %\n";
    std::cout << "-------|-------------\n";
    for (const auto& r : results) {
        std::cout << "  " << r.sampleNumber << "    | " 
                  << std::fixed << std::setprecision(2) << r.differencePercent << "%\n";
    }

    return 0;
}