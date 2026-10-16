#include "video_variance.h"
#include "video_utils.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <filesystem>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace fs = std::filesystem;

// Calculate percentage difference between two frames
static double calculateFrameDifference(AVFrame* frameA, AVFrame* frameB, int width, int height) {
    int totalPixels = width * height;
    int differentPixels = 0;

    for (int y = 0; y < height; ++y) {
        uint8_t* rowA = frameA->data[0] + y * frameA->linesize[0];
        uint8_t* rowB = frameB->data[0] + y * frameB->linesize[0];
        for (int x = 0; x < width; ++x) {
            int idx = x * 3;
            if (rowA[idx] != rowB[idx] ||
                rowA[idx + 1] != rowB[idx + 1] ||
                rowA[idx + 2] != rowB[idx + 2]) {
                differentPixels++;
            }
        }
    }

    return 100.0 * (1.0 - static_cast<double>(differentPixels) / totalPixels);
}

std::vector<VarianceResult> analyzeVideoVariance(const std::string& inputPath,
                                                  const std::string& startTime,
                                                  const std::string& endTime,
                                                  int n, int m) {
    std::vector<VarianceResult> results;

    if (n <= 0) {
        std::cerr << "Error: Number of samples (n) must be positive." << std::endl;
        return results;
    }
    if (m < 0) {
        std::cerr << "Error: Frame distance (m) must be non-negative." << std::endl;
        return results;
    }
    if (!fs::exists(inputPath)) {
        std::cerr << "Error: Input file does not exist: " << inputPath << std::endl;
        return results;
    }

    AVFormatContext* rawInput = nullptr;
    if (avformat_open_input(&rawInput, inputPath.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Could not open input file." << std::endl;
        return results;
    }
    std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)> inputFmt(
        rawInput, [](AVFormatContext* ctx) { avformat_close_input(&ctx); });

    if (avformat_find_stream_info(inputFmt.get(), nullptr) < 0) {
        std::cerr << "Failed to retrieve stream info." << std::endl;
        return results;
    }

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

    const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!codec) {
        std::cerr << "Failed to find decoder." << std::endl;
        return results;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
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

    int64_t startSec = 0, endSec = INT64_MAX;
    if (!startTime.empty()) startSec = parseTimeToSeconds(startTime);
    if (!endTime.empty()) endSec = parseTimeToSeconds(endTime);

    if (startSec >= endSec) {
        std::cerr << "Error: Start time must be less than end time." << std::endl;
        avcodec_free_context(&codecCtx);
        return results;
    }

    double durationSec = startSec == 0 && endSec == INT64_MAX ? 
        inputFmt->duration / (double)AV_TIME_BASE : (endSec - startSec);

    if (durationSec <= 0) {
        std::cerr << "Error: Invalid duration." << std::endl;
        avcodec_free_context(&codecCtx);
        return results;
    }

    double sampleIntervalSec = durationSec / n;

    std::cout << "Variance Analysis:" << std::endl;
    std::cout << " - Duration: " << std::fixed << std::setprecision(2) 
              << durationSec << "s" << std::endl;
    std::cout << " - Samples (n): " << n << std::endl;
    std::cout << " - Frame distance (m): " << m << std::endl;
    std::cout << " - Sample interval: " << sampleIntervalSec << "s" << std::endl;

    // CORRECTED: Guard against zero frame rate
    double fps = av_q2d(videoStream->r_frame_rate);
    if (fps <= 0) fps = av_q2d(videoStream->avg_frame_rate);
    if (fps <= 0) fps = 30.0; // Fallback to 30 FPS
    double frameDuration = 1.0 / fps;

    SwsContext* swsCtx = sws_getContext(
        codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
        codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    AVFrame* rawFrame = av_frame_alloc();
    AVFrame* rgbFrameA = av_frame_alloc();
    AVFrame* rgbFrameB = av_frame_alloc();
    rgbFrameA->format = AV_PIX_FMT_RGB24;
    rgbFrameA->width = codecCtx->width;
    rgbFrameA->height = codecCtx->height;
    av_frame_get_buffer(rgbFrameA, 32);
    rgbFrameB->format = AV_PIX_FMT_RGB24;
    rgbFrameB->width = codecCtx->width;
    rgbFrameB->height = codecCtx->height;
    av_frame_get_buffer(rgbFrameB, 32);

    AVPacket* packet = av_packet_alloc();
    
    for (int sample = 1; sample <= n; ++sample) {
        double targetTimeA = startSec + (sample - 1) * sampleIntervalSec;
        // Uses the guarded frameDuration
        double targetTimeB = targetTimeA + (m > 0 ? m * frameDuration : frameDuration);

        int64_t seekTarget = static_cast<int64_t>(targetTimeA * AV_TIME_BASE);
        av_seek_frame(inputFmt.get(), -1, seekTarget, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(codecCtx);

        bool gotA = false;
        bool gotB = false;
        double currentPtsSec = targetTimeA;

        while (av_read_frame(inputFmt.get(), packet) >= 0) {
            if (packet->stream_index != videoStreamIdx) {
                av_packet_unref(packet);
                continue;
            }

            int ret = avcodec_send_packet(codecCtx, packet);
            av_packet_unref(packet);
            if (ret < 0) continue;

            while (avcodec_receive_frame(codecCtx, rawFrame) == 0) {
                double ptsSec = currentPtsSec;
                if (rawFrame->pts != AV_NOPTS_VALUE) {
                    ptsSec = rawFrame->pts * av_q2d(videoStream->time_base);
                } else {
                    ptsSec = currentPtsSec + frameDuration;
                }
                currentPtsSec = ptsSec;

                if (!gotA && ptsSec >= targetTimeA) {
                    sws_scale(swsCtx, rawFrame->data, rawFrame->linesize, 0,
                              codecCtx->height, rgbFrameA->data, rgbFrameA->linesize);
                    gotA = true;
                }

                if (gotA && !gotB && ptsSec >= targetTimeB) {
                    sws_scale(swsCtx, rawFrame->data, rawFrame->linesize, 0,
                              codecCtx->height, rgbFrameB->data, rgbFrameB->linesize);
                    gotB = true;
                    break;
                }
            }
            if (gotB) break;
        }

        if (gotA && gotB) {
            double diff = calculateFrameDifference(rgbFrameA, rgbFrameB, codecCtx->width, codecCtx->height);
            results.push_back({sample, diff});
        } else {
            std::cerr << "Warning: Could not extract frame pair for sample " << sample << std::endl;
        }
    }

    av_frame_free(&rawFrame);
    av_frame_free(&rgbFrameA);
    av_frame_free(&rgbFrameB);
    av_packet_free(&packet);
    sws_freeContext(swsCtx);
    avcodec_free_context(&codecCtx);

    return results;
}