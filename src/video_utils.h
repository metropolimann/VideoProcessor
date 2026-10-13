#ifndef VIDEO_UTILS_H
#define VIDEO_UTILS_H

#include <string>
#include <vector>
#include <cstdint>

// Time parsing
int64_t parseTimeToSeconds(const std::string& timeStr);

// Frame extraction
bool extractFramesAtFPS(const std::string& inputPath,
                        const std::string& outputDir,
                        int targetFPS,
                        const std::string& startTime = "",
                        const std::string& endTime = "");

// Video trimming
struct TrimSegment {
    std::string start;
    std::string end;
};
bool trimVideoSegment(const std::string& inputPath,
                      const std::string& outputPath,
                      int64_t startSec,
                      int64_t endSec,
                      bool removeAudio);
bool trimVideoBatch(const std::string& inputPath,
                    const std::string& outputDir,
                    const std::vector<TrimSegment>& segments,
                    bool removeAudio);

#endif // VIDEO_UTILS_H