#ifndef VIDEO_VARIANCE_H
#define VIDEO_VARIANCE_H

#include <string>
#include <vector>

struct VarianceSample {
    int sampleNumber;
    double frameAPts;      // PTS of frame A in seconds
    double frameBPts;      // PTS of frame B in seconds
    double differencePct;  // 0% = completely different, 100% = identical
};

/**
 * @brief Analyzes video variance by comparing frames at regular intervals
 * 
 * @param inputPath   Path to the input video
 * @param startTime   Start time in H:M:S format
 * @param endTime     End time in H:M:S format
 * @param n           Number of samples to analyze
 * @param m           Frame distance between frames A and B (0 = consecutive frames)
 * @return std::vector<VarianceSample> Results for each sample
 */
std::vector<VarianceSample> analyzeVideoVariance(
    const std::string& inputPath,
    const std::string& startTime,
    const std::string& endTime,
    int n,
    int m
);

#endif // VIDEO_VARIANCE_H