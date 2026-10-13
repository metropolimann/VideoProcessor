#include "fire_crop.h"
#include "video_utils.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <memory>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

namespace fs = std::filesystem;

// --- RAII Deleters ---
struct AVFormatContextDeleter {
    void operator()(AVFormatContext* ctx) const { if (ctx) avformat_close_input(&ctx); }
};
struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const { if (pkt) av_packet_free(&pkt); }
};
struct AVFrameDeleter {
    void operator()(AVFrame* frame) const { if (frame) av_frame_free(&frame); }
};
struct SwsContextDeleter {
    void operator()(SwsContext* ctx) const { if (ctx) sws_freeContext(ctx); }
};

using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVPacketPtr        = std::unique_ptr<AVPacket, AVPacketDeleter>;
using AVFramePtr         = std::unique_ptr<AVFrame, AVFrameDeleter>;
using SwsContextPtr      = std::unique_ptr<SwsContext, SwsContextDeleter>;

// --- Helper: Save a cropped AVFrame (RGB24) as JPEG ---
static bool saveCroppedFrameAsJPEG(AVFrame* rgbFrame, int cropW, int cropH, 
                                   const std::string& outputDir, int frameNumber) {
    // 1. Convert RGB24 cropped frame to YUVJ420P for JPEG encoding
    AVFramePtr yuvFrame(av_frame_alloc());
    yuvFrame->format = AV_PIX_FMT_YUVJ420P;
    yuvFrame->width = cropW;
    yuvFrame->height = cropH;
    if (av_frame_get_buffer(yuvFrame.get(), 32) < 0) return false;

    SwsContextPtr swsCtx(sws_getContext(
        cropW, cropH, AV_PIX_FMT_RGB24,
        cropW, cropH, AV_PIX_FMT_YUVJ420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    ));
    if (!swsCtx) return false;

    sws_scale(swsCtx.get(), rgbFrame->data, rgbFrame->linesize, 0, cropH,
              yuvFrame->data, yuvFrame->linesize);

    // 2. Encode to JPEG
    const AVCodec* jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!jpegCodec) return false;

    AVCodecContext* jpegCtx = avcodec_alloc_context3(jpegCodec);
    jpegCtx->width = cropW;
    jpegCtx->height = cropH;
    jpegCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    jpegCtx->time_base = AVRational{1, 25};
    
    if (avcodec_open2(jpegCtx, jpegCodec, nullptr) < 0) {
        avcodec_free_context(&jpegCtx);
        return false;
    }

    AVPacketPtr jpegPacket(av_packet_alloc());
    if (avcodec_send_frame(jpegCtx, yuvFrame.get()) < 0 || 
        avcodec_receive_packet(jpegCtx, jpegPacket.get()) < 0) {
        avcodec_free_context(&jpegCtx);
        return false;
    }

    // 3. Write to disk
    std::ostringstream filename;
    filename << "fire_frame_" << std::setw(6) << std::setfill('0') << frameNumber << ".jpg";
    std::string filepath = (fs::path(outputDir) / filename.str()).string();
    
    std::ofstream outFile(filepath, std::ios::binary);
    if (outFile.is_open()) {
        outFile.write(reinterpret_cast<const char*>(jpegPacket->data), jpegPacket->size);
        outFile.close();
        avcodec_free_context(&jpegCtx);
        return true;
    }
    
    avcodec_free_context(&jpegCtx);
    return false;
}

// --- Main Extraction Logic ---
bool extractFireCrop(const std::string& inputPath,
                     const std::string& outputDir,
                     const std::string& startTime,
                     const std::string& endTime,
                     int targetFPS)
{
    if (targetFPS <= 0) targetFPS = 1;

    // Pre-checks
    if (!fs::exists(inputPath)) {
        std::cerr << "Error: Input file does not exist: " << inputPath << std::endl;
        return false;
    }
    if (!fs::is_regular_file(inputPath)) {
        std::cerr << "Error: Input path is not a regular file: " << inputPath << std::endl;
        return false;
    }
    
    try {
        if (!fs::exists(outputDir)) fs::create_directories(outputDir);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Failed to create output directory: " << e.what() << std::endl;
        return false;
    }

    // Check output directory is writable
    std::string testFile = (fs::path(outputDir) / ".write_test.tmp").string();
    {
        std::ofstream testOut(testFile);
        if (!testOut.is_open()) {
            std::cerr << "Error: Output directory is not writable: " << outputDir << std::endl;
            return false;
        }
        testOut << "test";
    }
    fs::remove(testFile);

    // Open input
    AVFormatContext* rawInput = nullptr;
    if (avformat_open_input(&rawInput, inputPath.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Could not open input file: " << inputPath << std::endl;
        return false;
    }
    AVFormatContextPtr inputFmt(rawInput);
    if (avformat_find_stream_info(inputFmt.get(), nullptr) < 0) {
        std::cerr << "Failed to retrieve input stream information." << std::endl;
        return false;
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
        return false;
    }

    // Setup decoder
    const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!codec) {
        std::cerr << "Failed to find decoder." << std::endl;
        return false;
    }
    
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        std::cerr << "Failed to allocate codec context." << std::endl;
        return false;
    }
    
    if (avcodec_parameters_to_context(codecCtx, videoStream->codecpar) < 0) {
        std::cerr << "Failed to copy codec parameters." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }
    
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        std::cerr << "Failed to open codec." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }

    // Parse time
    int64_t startSec = 0, endSec = INT64_MAX;
    if (!startTime.empty()) {
        try { startSec = parseTimeToSeconds(startTime); }
        catch (const std::exception& e) { 
            std::cerr << "Start time error: " << e.what() << std::endl; 
            avcodec_free_context(&codecCtx);
            return false; 
        }
    }
    if (!endTime.empty()) {
        try { endSec = parseTimeToSeconds(endTime); }
        catch (const std::exception& e) { 
            std::cerr << "End time error: " << e.what() << std::endl; 
            avcodec_free_context(&codecCtx);
            return false; 
        }
    }

    // Validate time range
    if (!endTime.empty() && startSec >= endSec) {
        std::cerr << "Error: Start time must be less than end time." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }

    double originalFPS = av_q2d(videoStream->r_frame_rate);
    if (originalFPS <= 0) originalFPS = av_q2d(videoStream->avg_frame_rate);
    if (originalFPS <= 0) originalFPS = 30.0;
    
    int frameInterval = (targetFPS > 0) ? std::max(1, static_cast<int>(std::round(originalFPS / targetFPS))) : 1;

    // Get video duration
    double durationSec = 0;
    if (inputFmt->duration > 0) {
        durationSec = inputFmt->duration / (double)AV_TIME_BASE;
    }

    // Validate start/end against duration
    if (durationSec > 0 && startSec >= durationSec) {
        std::cerr << "Error: Start time (" << startSec << "s) is beyond video duration (" 
                  << std::fixed << std::setprecision(2) << durationSec << "s)." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }

    if (durationSec > 0 && !endTime.empty() && endSec > durationSec) {
        std::cerr << "Warning: End time (" << endSec << "s) exceeds video duration (" 
                  << std::fixed << std::setprecision(2) << durationSec << "s). Clamping to duration." << std::endl;
        endSec = static_cast<int64_t>(std::ceil(durationSec));
    }

    std::cout << "Fire/Brightness Crop Extraction:" << std::endl;
    std::cout << "  - Original FPS: " << std::fixed << std::setprecision(2) << originalFPS << std::endl;
    std::cout << "  - Target FPS: " << targetFPS << std::endl;
    std::cout << "  - Frame interval: every " << frameInterval << " frames" << std::endl;
    std::cout << "  - Output directory: " << outputDir << std::endl;
    if (startSec > 0) std::cout << "  - Start time: " << startSec << "s" << std::endl;
    if (endSec != INT64_MAX) std::cout << "  - End time: " << endSec << "s" << std::endl;
    std::cout << "  - Crop size: 50% width x 50% height (25% total area)" << std::endl;
    std::cout << std::endl;

    // Seek to start
    if (startSec > 0) {
        int64_t seekTarget = av_rescale_q(startSec * AV_TIME_BASE, AVRational{1, AV_TIME_BASE}, AV_TIME_BASE_Q);
        av_seek_frame(inputFmt.get(), -1, seekTarget, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(codecCtx);
    }

    // Scaler: Source format -> RGB24 (for easy pixel manipulation and brightness calc)
    SwsContextPtr rgbSwsCtx(sws_getContext(
        codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
        codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    ));
    if (!rgbSwsCtx) {
        std::cerr << "Failed to create RGB scaler context." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }

    AVFramePtr frame(av_frame_alloc());
    if (!frame) {
        std::cerr << "Failed to allocate input frame." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }
    
    AVFramePtr rgbFrame(av_frame_alloc());
    rgbFrame->format = AV_PIX_FMT_RGB24;
    rgbFrame->width = codecCtx->width;
    rgbFrame->height = codecCtx->height;
    if (av_frame_get_buffer(rgbFrame.get(), 32) < 0) {
        std::cerr << "Failed to allocate RGB buffer." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }

    AVPacketPtr packet(av_packet_alloc());
    if (!packet) {
        std::cerr << "Failed to allocate packet." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }

    int frameCount = 0, extractedCount = 0, failedCount = 0;
    double currentPtsSec = 0;

    std::cout << "Extracting fire/brightness crops..." << std::endl;

    while (av_read_frame(inputFmt.get(), packet.get()) >= 0) {
        if (packet->stream_index != videoStreamIdx) {
            av_packet_unref(packet.get());
            continue;
        }

        if (avcodec_send_packet(codecCtx, packet.get()) == 0) {
            while (avcodec_receive_frame(codecCtx, frame.get()) == 0) {
                if (frame->pts != AV_NOPTS_VALUE)
                    currentPtsSec = frame->pts * av_q2d(videoStream->time_base);
                else
                    currentPtsSec = static_cast<double>(frameCount) / originalFPS;

                if (currentPtsSec < startSec) { 
                    av_frame_unref(frame.get()); 
                    continue; 
                }
                
                if (currentPtsSec >= endSec && endSec != INT64_MAX) { 
                    av_frame_unref(frame.get()); 
                    goto finish_extraction; 
                }

                if (frameCount % frameInterval == 0) {
                    // 1. Convert to RGB24
                    sws_scale(rgbSwsCtx.get(), frame->data, frame->linesize, 0, codecCtx->height,
                              rgbFrame->data, rgbFrame->linesize);

                    // 2. Find brightest pixel (using luminance formula)
                    uint8_t maxLum = 0;
                    int maxX = 0, maxY = 0;
                    int w = rgbFrame->width;
                    int h = rgbFrame->height;

                    for (int y = 0; y < h; ++y) {
                        uint8_t* row = rgbFrame->data[0] + y * rgbFrame->linesize[0];
                        for (int x = 0; x < w; ++x) {
                            uint8_t* p = row + x * 3;
                            // Fast integer luminance: Y = 0.299R + 0.587G + 0.114B
                            uint8_t lum = (77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8;
                            if (lum > maxLum) {
                                maxLum = lum;
                                maxX = x;
                                maxY = y;
                            }
                        }
                    }

                    // 3. Calculate 50% crop dimensions
                    int cropW = std::max(1, w / 2);
                    int cropH = std::max(1, h / 2);

                    // 4. Center crop on brightest pixel, clamped to boundaries
                    int cropX = maxX - cropW / 2;
                    int cropY = maxY - cropH / 2;

                    if (cropX < 0) cropX = 0;
                    if (cropY < 0) cropY = 0;
                    if (cropX + cropW > w) cropX = w - cropW;
                    if (cropY + cropH > h) cropY = h - cropH;

                    // 5. Copy cropped region to a new frame
                    AVFramePtr cropRgbFrame(av_frame_alloc());
                    cropRgbFrame->format = AV_PIX_FMT_RGB24;
                    cropRgbFrame->width = cropW;
                    cropRgbFrame->height = cropH;
                    if (av_frame_get_buffer(cropRgbFrame.get(), 32) < 0) {
                        failedCount++;
                        av_frame_unref(frame.get());
                        continue;
                    }

                    for (int y = 0; y < cropH; ++y) {
                        uint8_t* dstRow = cropRgbFrame->data[0] + y * cropRgbFrame->linesize[0];
                        uint8_t* srcRow = rgbFrame->data[0] + (cropY + y) * rgbFrame->linesize[0] + cropX * 3;
                        memcpy(dstRow, srcRow, cropW * 3);
                    }

                    // 6. Save
                    if (saveCroppedFrameAsJPEG(cropRgbFrame.get(), cropW, cropH, outputDir, extractedCount)) {
                        extractedCount++;
                        if (extractedCount % 10 == 0)
                            std::cout << "  Extracted " << extractedCount << " frames (Brightest at: " 
                                      << maxX << "," << maxY << ")...\r" << std::flush;
                    } else {
                        failedCount++;
                    }
                }
                frameCount++;
                av_frame_unref(frame.get());
            }
        }
        av_packet_unref(packet.get());
    }

finish_extraction:
    // Flush decoder
    avcodec_send_packet(codecCtx, nullptr);
    while (avcodec_receive_frame(codecCtx, frame.get()) == 0) {
        frameCount++;
        av_frame_unref(frame.get());
    }

    std::cout << "\nFire/brightness crop extraction complete." << std::endl;
    std::cout << "  - Total frames processed: " << frameCount << std::endl;
    std::cout << "  - Frames extracted: " << extractedCount << std::endl;
    if (failedCount > 0) {
        std::cerr << "  - Failed to save: " << failedCount << std::endl;
    }
    
    avcodec_free_context(&codecCtx);
    return (extractedCount > 0 || frameCount == 0);
}