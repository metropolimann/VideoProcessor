#include "video_variance.h"
#include "video_utils.h"
#include <iostream>
#include <iomanip>
#include <memory>
#include <cmath>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

// RAII deleters
struct AVFormatContextDeleter {
    void operator()(AVFormatContext* ctx) const {
        if (ctx) avformat_close_input(&ctx);
    }
};
struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const {
        if (pkt) av_packet_free(&pkt);
    }
};
struct AVFrameDeleter {
    void operator()(AVFrame* frame) const {
        if (frame) av_frame_free(&frame);
    }
};
struct SwsContextDeleter {
    void operator()(SwsContext* ctx) const {
        if (ctx) sws_freeContext(ctx);
    }
};

using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVPacketPtr        = std::unique_ptr<AVPacket, AVPacketDeleter>;
using AVFramePtr         = std::unique_ptr<AVFrame, AVFrameDeleter>;
using SwsContextPtr      = std::unique_ptr<SwsContext, SwsContextDeleter>;

/**
 * @brief Calculate difference percentage between two frames
 * @return 0.0 = completely different, 100.0 = identical
 */
static double calculateFrameDifference(AVFrame* frameA, AVFrame* frameB, SwsContext* rgbSwsCtx) {
    if (!frameA || !frameB) return 0.0;
    
    int width = frameA->width;
    int height = frameA->height;
    
    // Convert both frames to RGB24 for comparison
    AVFramePtr rgbA(av_frame_alloc());
    AVFramePtr rgbB(av_frame_alloc());
    rgbA->format = AV_PIX_FMT_RGB24;
    rgbA->width = width;
    rgbA->height = height;
    rgbB->format = AV_PIX_FMT_RGB24;
    rgbB->width = width;
    rgbB->height = height;
    
    if (av_frame_get_buffer(rgbA.get(), 32) < 0 || 
        av_frame_get_buffer(rgbB.get(), 32) < 0) {
        return 0.0;
    }
    
    sws_scale(rgbSwsCtx, frameA->data, frameA->linesize, 0, height,
              rgbA->data, rgbA->linesize);
    sws_scale(rgbSwsCtx, frameB->data, frameB->linesize, 0, height,
              rgbB->data, rgbB->linesize);
    
    // Count identical pixels
    int64_t identicalPixels = 0;
    int64_t totalPixels = width * height;
    
    for (int y = 0; y < height; ++y) {
        uint8_t* rowA = rgbA->data[0] + y * rgbA->linesize[0];
        uint8_t* rowB = rgbB->data[0] + y * rgbB->linesize[0];
        
        for (int x = 0; x < width; ++x) {
            uint8_t* pixelA = rowA + x * 3;
            uint8_t* pixelB = rowB + x * 3;
            
            // Check if all RGB components are identical
            if (pixelA[0] == pixelB[0] && 
                pixelA[1] == pixelB[1] && 
                pixelA[2] == pixelB[2]) {
                identicalPixels++;
            }
        }
    }
    
    // Calculate percentage (100% = identical, 0% = completely different)
    return (static_cast<double>(identicalPixels) / totalPixels) * 100.0;
}

/**
 * @brief Extract frame at or after the specified PTS
 */
static bool extractFrameAtPts(AVFormatContext* inputFmt, AVCodecContext* codecCtx,
                              int videoStreamIdx, double targetPts,
                              AVFramePtr& outFrame, double& actualPts) {
    AVStream* videoStream = inputFmt->streams[videoStreamIdx];
    AVPacketPtr packet(av_packet_alloc());
    
    // Seek to approximately the right position
    int64_t seekTarget = av_rescale_q(static_cast<int64_t>(targetPts * AV_TIME_BASE),
                                      AVRational{1, AV_TIME_BASE},
                                      AV_TIME_BASE_Q);
    av_seek_frame(inputFmt, -1, seekTarget, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(codecCtx);
    
    while (av_read_frame(inputFmt, packet.get()) >= 0) {
        if (packet->stream_index != videoStreamIdx) {
            av_packet_unref(packet.get());
            continue;
        }
        
        if (avcodec_send_packet(codecCtx, packet.get()) < 0) {
            av_packet_unref(packet.get());
            continue;
        }
        
        while (avcodec_receive_frame(codecCtx, outFrame.get()) >= 0) {
            double framePts = AV_NOPTS_VALUE;
            if (outFrame->pts != AV_NOPTS_VALUE) {
                framePts = outFrame->pts * av_q2d(videoStream->time_base);
            }
            
            if (framePts >= targetPts) {
                actualPts = framePts;
                av_packet_unref(packet.get());
                return true;
            }
            av_frame_unref(outFrame.get());
        }
        av_packet_unref(packet.get());
    }
    
    return false;
}

std::vector<VarianceSample> analyzeVideoVariance(
    const std::string& inputPath,
    const std::string& startTime,
    const std::string& endTime,
    int n,
    int m)
{
    std::vector<VarianceSample> results;
    
    // Validate inputs
    if (n <= 0) {
        std::cerr << "Error: Number of samples (n) must be positive." << std::endl;
        return results;
    }
    if (m < 0) {
        std::cerr << "Error: Frame distance (m) must be non-negative." << std::endl;
        return results;
    }
    
    int64_t startSec = 0, endSec = 0;
    try {
        startSec = parseTimeToSeconds(startTime);
        endSec = parseTimeToSeconds(endTime);
    } catch (const std::exception& e) {
        std::cerr << "Time parse error: " << e.what() << std::endl;
        return results;
    }
    
    if (startSec >= endSec) {
        std::cerr << "Error: Start time must be less than end time." << std::endl;
        return results;
    }
    
    double durationSec = static_cast<double>(endSec - startSec);
    double sampleInterval = durationSec / n;
    
    std::cout << "Video Variance Analysis:" << std::endl;
    std::cout << "  - Period: " << startTime << " to " << endTime 
              << " (" << durationSec << "s)" << std::endl;
    std::cout << "  - Number of samples: " << n << std::endl;
    std::cout << "  - Frame distance (m): " << m << std::endl;
    std::cout << "  - Sample interval: " << std::fixed << std::setprecision(3) 
              << sampleInterval << "s" << std::endl;
    std::cout << std::endl;
    
    // Open input file
    AVFormatContext* rawInput = nullptr;
    if (avformat_open_input(&rawInput, inputPath.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Could not open input file: " << inputPath << std::endl;
        return results;
    }
    AVFormatContextPtr inputFmt(rawInput);
    
    if (avformat_find_stream_info(inputFmt.get(), nullptr) < 0) {
        std::cerr << "Failed to retrieve input stream information." << std::endl;
        return results;
    }
    
    // Find video stream
    int videoStreamIdx = -1;
    AVStream* videoStream = nullptr;
    for (unsigned int i = 0; i < inputFmt->nb_streams; ++i) {
        if (inputFmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIdx = i;
            videoStream = inputFmt->streams[i];
            break;
        }
    }
    if (videoStreamIdx == -1) {
        std::cerr << "No video stream found." << std::endl;
        return results;
    }
    
    // Get frame rate
    double fps = av_q2d(videoStream->r_frame_rate);
    if (fps <= 0) fps = av_q2d(videoStream->avg_frame_rate);
    if (fps <= 0) fps = 30.0;
    
    // Setup decoder
    const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!codec) {
        std::cerr << "Failed to find decoder." << std::endl;
        return results;
    }
    
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        std::cerr << "Failed to allocate codec context." << std::endl;
        return results;
    }
    
    if (avcodec_parameters_to_context(codecCtx, videoStream->codecpar) < 0) {
        std::cerr << "Failed to copy codec parameters." << std::endl;
        avcodec_free_context(&codecCtx);
        return results;
    }
    
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        std::cerr << "Failed to open codec." << std::endl;
        avcodec_free_context(&codecCtx);
        return results;
    }
    
    // Create scaler for frame comparison
    SwsContextPtr rgbSwsCtx(sws_getContext(
        codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
        codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    ));
    if (!rgbSwsCtx) {
        std::cerr << "Failed to create scaler context." << std::endl;
        avcodec_free_context(&codecCtx);
        return results;
    }
    
    AVFramePtr frameA(av_frame_alloc());
    AVFramePtr frameB(av_frame_alloc());
    
    std::cout << "Analyzing samples..." << std::endl;
    
    // Analyze each sample
    for (int i = 1; i <= n; ++i) {
        // Calculate PTS for frame A and frame B
        double sampleStartPts = startSec + (i - 1) * sampleInterval;
        double frameAPts = sampleStartPts;
        double frameBPts = sampleStartPts + (m * (1.0 / fps));
        
        // Ensure frame B doesn't exceed end time
        if (frameBPts > endSec) {
            frameBPts = endSec - 0.001; // Small offset to stay within bounds
        }
        
        // Extract frame A
        double actualPtsA = 0;
        if (!extractFrameAtPts(inputFmt.get(), codecCtx, videoStreamIdx, 
                               frameAPts, frameA, actualPtsA)) {
            std::cerr << "  Sample " << i << ": Failed to extract frame A" << std::endl;
            results.push_back({i, frameAPts, frameBPts, -1.0});
            continue;
        }
        
        // Extract frame B
        double actualPtsB = 0;
        if (!extractFrameAtPts(inputFmt.get(), codecCtx, videoStreamIdx, 
                               frameBPts, frameB, actualPtsB)) {
            std::cerr << "  Sample " << i << ": Failed to extract frame B" << std::endl;
            results.push_back({i, actualPtsA, frameBPts, -1.0});
            continue;
        }
        
        // Calculate difference
        double diffPct = calculateFrameDifference(frameA.get(), frameB.get(), rgbSwsCtx.get());
        
        results.push_back({i, actualPtsA, actualPtsB, diffPct});
        
        // Progress indicator
        if (i % 10 == 0 || i == n) {
            std::cout << "  Processed " << i << "/" << n << " samples...\r" << std::flush;
        }
        
        // Clear frames for next iteration
        av_frame_unref(frameA.get());
        av_frame_unref(frameB.get());
    }
    
    std::cout << "\nAnalysis complete!" << std::endl;
    std::cout << std::endl;
    
    // Print results
    std::cout << "Results (Difference % between frames):" << std::endl;
    std::cout << "  Sample |  Frame A (s) |  Frame B (s) | Difference %" << std::endl;
    std::cout << "  -------|--------------|--------------|-------------" << std::endl;
    
    double minDiff = 101.0;
    int minDiffSample = -1;
    
    for (const auto& sample : results) {
        if (sample.differencePct >= 0) {
            std::cout << "  " << std::setw(6) << sample.sampleNumber << " | "
                      << std::setw(12) << std::fixed << std::setprecision(3) << sample.frameAPts << " | "
                      << std::setw(12) << sample.frameBPts << " | "
                      << std::setw(11) << std::setprecision(2) << sample.differencePct << std::endl;
            
            // Track minimum difference (most change)
            if (sample.differencePct < minDiff) {
                minDiff = sample.differencePct;
                minDiffSample = sample.sampleNumber;
            }
        } else {
            std::cout << "  " << std::setw(6) << sample.sampleNumber << " | "
                      << std::setw(12) << std::fixed << std::setprecision(3) << sample.frameAPts << " | "
                      << std::setw(12) << sample.frameBPts << " | "
                      << "     ERROR" << std::endl;
        }
    }
    
    std::cout << std::endl;
    if (minDiffSample > 0) {
        std::cout << "Most variant period: Sample #" << minDiffSample 
                  << " (" << minDiff << "% similarity = " 
                  << (100.0 - minDiff) << "% change)" << std::endl;
    }
    
    avcodec_free_context(&codecCtx);
    return results;
}