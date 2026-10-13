#include "video_utils.h"
#include "registry.h"
#include "fire_crop.h" 
#include <iostream>
#include <filesystem>
namespace fs = std::filesystem;

void printUsage(const char* prog) {
    std::cout << "Usage:\n"
              << "  " << prog << " <input.mp4> <output_dir> --extract-frames <fps> [--start H:M:S] [--end H:M:S]\n"
              << "  " << prog << " <input.mp4> <output_dir> [--no-audio] <start1> <end1> [start2 end2 ...]\n"
              << "\nModes:\n"
              << "  1. Frame extraction: Extract frames at specified FPS\n"
              << "     " << prog << " video.mp4 ./frames --extract-frames 5\n"
              << "     " << prog << " video.mp4 ./frames --extract-frames 10 --start 00:01:00 --end 00:02:00\n"
              << "\n2. Clip trimming: Extract video segments (original behavior)\n"
              << "     " << prog << " video.mp4 ./clips 00:01:15 00:02:45\n"
              << "     " << prog << " video.mp4 ./clips --no-audio 00:01:15 00:02:45 00:05:00 00:06:30\n"
              << "\nRegistry:\n"
              << "  A JSON file (clip_registry.json) is created in the same directory as this\n"
              << "  source file. It tracks used IDs per video to prevent duplicate names across runs.\n";
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string inputFile  = argv[1];
    std::string outputDir  = argv[2];
    bool removeAudio       = false;
    int argIdx = 3;
// Check for --extract-fire-crop mode
if (argIdx < argc && std::string(argv[argIdx]) == "--extract-fire-crop") {
    int targetFPS = 1; // Default to 1 FPS for fire detection
    
    // Optional: allow passing a custom FPS
    if (argIdx + 1 < argc && std::string(argv[argIdx + 1]) != "--start" && std::string(argv[argIdx + 1]) != "--end") {
        try { targetFPS = std::stoi(argv[argIdx + 1]); argIdx++; } 
        catch (...) { /* ignore and use default */ }
    }
    
    argIdx++; // Move past the flag (and optional FPS)

    std::string startTime = "", endTime = "";
    while (argIdx < argc) {
        std::string arg = argv[argIdx];
        if (arg == "--start" && argIdx + 1 < argc) { startTime = argv[argIdx + 1]; argIdx += 2; } 
        else if (arg == "--end" && argIdx + 1 < argc) { endTime = argv[argIdx + 1]; argIdx += 2; } 
        else { std::cerr << "Unknown option: " << arg << std::endl; return 1; }
    }

    try { if (!fs::exists(outputDir)) fs::create_directories(outputDir); } 
    catch (const fs::filesystem_error& e) { std::cerr << "Dir error: " << e.what() << std::endl; return 1; }

    if (extractFireCrop(inputFile, outputDir, startTime, endTime, targetFPS)) {
        std::cout << "\nAll fire crops extracted successfully to: " << outputDir << std::endl;
        return 0;
    } else {
        std::cerr << "\nFire crop extraction failed." << std::endl;
        return 1;
    }
}
    // Check for --extract-frames mode
    if (argIdx < argc && std::string(argv[argIdx]) == "--extract-frames") {
        if (argIdx + 1 >= argc) {
            std::cerr << "Error: --extract-frames requires an FPS value." << std::endl;
            printUsage(argv[0]);
            return 1;
        }
        
        int targetFPS = 0;
        try {
            targetFPS = std::stoi(argv[argIdx + 1]);
        } catch (const std::invalid_argument&) {
            std::cerr << "Error: Invalid FPS value '" << argv[argIdx + 1] << "'. Must be an integer." << std::endl;
            return 1;
        } catch (const std::out_of_range&) {
            std::cerr << "Error: FPS value '" << argv[argIdx + 1] << "' is out of range." << std::endl;
            return 1;
        }

        if (targetFPS <= 0) {
            std::cerr << "Error: FPS must be positive." << std::endl;
            return 1;
        }
        argIdx += 2;

        std::string startTime = "", endTime = "";
        while (argIdx < argc) {
            std::string arg = argv[argIdx];
            if (arg == "--start" && argIdx + 1 < argc) {
                startTime = argv[argIdx + 1];
                argIdx += 2;
            } else if (arg == "--end" && argIdx + 1 < argc) {
                endTime = argv[argIdx + 1];
                argIdx += 2;
            } else {
                std::cerr << "Unknown option: " << arg << std::endl;
                return 1;
            }
        }

        try {
            if (!fs::exists(outputDir)) fs::create_directories(outputDir);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Failed to create output directory: " << e.what() << std::endl;
            return 1;
        }

        if (extractFramesAtFPS(inputFile, outputDir, targetFPS, startTime, endTime)) {
            std::cout << "\nAll frames extracted successfully to: " << outputDir << std::endl;
            return 0;
        } else {
            std::cerr << "\nFrame extraction failed." << std::endl;
            return 1;
        }
    }

    // Original clip trimming mode (with optional --no-audio)
    if (argIdx < argc && std::string(argv[argIdx]) == "--no-audio") {
        removeAudio = true;
        ++argIdx;
    }

    if ((argc - argIdx) % 2 != 0) {
        std::cerr << "Error: Time arguments must come in start/end pairs." << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    std::vector<TrimSegment> segments;
    for (int i = argIdx; i < argc; i += 2) {
        segments.push_back({argv[i], argv[i + 1]});
    }

    // Note: trimVideoBatch does not use the registry in this version.
    // If you want unique naming via registry, modify trimVideoBatch accordingly.
    if (trimVideoBatch(inputFile, outputDir, segments, removeAudio)) {
        std::cout << "\nAll clips generated successfully in: " << outputDir << std::endl;
        return 0;
    } else {
        std::cerr << "\nSome clips failed." << std::endl;
        return 1;
    }
}