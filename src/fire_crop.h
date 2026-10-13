#ifndef FIRE_CROP_H
#define FIRE_CROP_H

#include <string>

/**
 * @brief Extracts cropped frames centered around the brightest pixel.
 * 
 * @param inputPath  Path to the input video.
 * @param outputDir  Directory to save the cropped JPEG frames.
 * @param startTime  Start time in H:M:S format (empty for beginning).
 * @param endTime    End time in H:M:S format (empty for end of video).
 * @param targetFPS  Target frames per second to extract (0 = every frame).
 * @return true      If extraction was successful.
 */
bool extractFireCrop(const std::string& inputPath,
                     const std::string& outputDir,
                     const std::string& startTime,
                     const std::string& endTime,
                     int targetFPS = 1);

#endif // FIRE_CROP_H