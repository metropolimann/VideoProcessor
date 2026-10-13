#ifndef VIDEO_VARIANCE_H
#define VIDEO_VARIANCE_H

#include <string>
#include <vector>

/**
 * @brief Structure to hold the result of a single variance sample.
 */
struct VarianceResult {
    int sampleNumber;          // The sample number (1 to n)
    double differencePercent;  // 100% = frames are identical, 0% = completely different
};

/**
 * @brief Analyzes video variance by comparing frames at specified intervals.
 * 
 * This function divides the given time period into 'n' equal samples. For each sample,
 * it compares frame A with frame B (where B is 'm' frames after A). It calculates
 * the percentage of pixels that are different between the two frames.
 * 
 * @param inputPath   Path to the input video file.
 * @param startTime   Start time in H:M:S format (e.g., "00:01:00"). Empty string for beginning.
 * @param endTime     End time in H:M:S format (e.g., "00:02:00"). Empty string for end of video.
 * @param n           Number of samples to check across the duration.
 * @param m           Frame distance between frame A and frame B (0 = consecutive frames).
 * @return            A vector of VarianceResult structs containing the sample number and difference percentage.
 *                    Returns an empty vector if an error occurs.
 */
std::vector<VarianceResult> analyzeVideoVariance(const std::string& inputPath,
                                                  const std::string& startTime,
                                                  const std::string& endTime,
                                                  int n, int m);

#endif // VIDEO_VARIANCE_H