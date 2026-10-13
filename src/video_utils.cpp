#include "video_utils.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

namespace fs = std::filesystem;

// RAII deleters
struct AVFormatContextDeleter {
    void operator()(AVFormatContext* ctx) const {
        if (ctx) avformat_close_input(&ctx);
    }
};
struct AVFormatContextOutputDeleter {
    void operator()(AVFormatContext* ctx) const {
        if (ctx) {
            if (!(ctx->oformat->flags & AVFMT_NOFILE) && ctx->pb) {
                avio_closep(&ctx->pb);
            }
            avformat_free_context(ctx);
        }
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

using AVFormatContextPtr    = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVFormatContextOutPtr = std::unique_ptr<AVFormatContext, AVFormatContextOutputDeleter>;
using AVPacketPtr           = std::unique_ptr<AVPacket, AVPacketDeleter>;
using AVFramePtr            = std::unique_ptr<AVFrame, AVFrameDeleter>;
using SwsContextPtr         = std::unique_ptr<SwsContext, SwsContextDeleter>;

// ---------- Time parsing ----------
int64_t parseTimeToSeconds(const std::string& timeStr) {
    int hours = 0, minutes = 0, seconds = 0;
    char delim1 = '\0', delim2 = '\0';
    std::stringstream ss(timeStr);
    if (ss >> hours >> delim1 >> minutes >> delim2 >> seconds) {
        if (delim1 == ':' && delim2 == ':' &&
            hours >= 0 && minutes >= 0 && minutes < 60 &&
            seconds >= 0 && seconds < 60) {
            return static_cast<int64_t>(hours) * 3600 + minutes * 60 + seconds;
        }
    }
    throw std::invalid_argument("Invalid time format. Expected H:M:S or HH:MM:SS");
}

// ---------- Helper: save a single frame as JPEG (YUVJ420P) ----------
static bool saveFrameAsJPEG(AVFrame* yuvFrame, const std::string& outputDir, int frameNumber) {
    const AVCodec* jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!jpegCodec) {
        std::cerr << "Failed to find JPEG encoder." << std::endl;
        return false;
    }
    AVCodecContext* jpegCtx = avcodec_alloc_context3(jpegCodec);
    if (!jpegCtx) {
        std::cerr << "Failed to allocate JPEG codec context." << std::endl;
        return false;
    }
    jpegCtx->width = yuvFrame->width;
    jpegCtx->height = yuvFrame->height;
    jpegCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;   // full-range YUV for JPEG
    jpegCtx->time_base = AVRational{1, 25};

    if (avcodec_open2(jpegCtx, jpegCodec, nullptr) < 0) {
        std::cerr << "Failed to open JPEG codec." << std::endl;
        avcodec_free_context(&jpegCtx);
        return false;
    }

    AVPacketPtr jpegPacket(av_packet_alloc());
    if (!jpegPacket) {
        std::cerr << "Failed to allocate JPEG packet." << std::endl;
        avcodec_free_context(&jpegCtx);
        return false;
    }

    int ret = avcodec_send_frame(jpegCtx, yuvFrame);
    if (ret < 0) {
        std::cerr << "Error sending frame to JPEG encoder." << std::endl;
        avcodec_free_context(&jpegCtx);
        return false;
    }

    ret = avcodec_receive_packet(jpegCtx, jpegPacket.get());
    if (ret == 0) {
        std::ostringstream filename;
        filename << "frame_" << std::setw(6) << std::setfill('0') << frameNumber << ".jpg";
        std::string filepath = (fs::path(outputDir) / filename.str()).string();
        std::ofstream outFile(filepath, std::ios::binary);
        if (outFile.is_open()) {
            outFile.write(reinterpret_cast<const char*>(jpegPacket->data), jpegPacket->size);
            outFile.close();
            avcodec_free_context(&jpegCtx);
            return true;
        }
    }
    avcodec_free_context(&jpegCtx);
    return false;
}

// ---------- Frame extraction ----------
bool extractFramesAtFPS(const std::string& inputPath,
                        const std::string& outputDir,
                        int targetFPS,
                        const std::string& startTime,
                        const std::string& endTime)
{
    // 1. targetFPS > 0 check (Prevents division-by-zero UB)
    if (targetFPS <= 0) {
        std::cerr << "Error: targetFPS must be greater than 0." << std::endl;
        return false;
    }

    // 2. Input is a regular file pre-check
    if (!fs::exists(inputPath)) {
        std::cerr << "Error: Input file does not exist: " << inputPath << std::endl;
        return false;
    }
    if (!fs::is_regular_file(inputPath)) {
        std::cerr << "Error: Input path is not a regular file: " << inputPath << std::endl;
        return false;
    }

    // Create output directory if needed
    try {
        if (!fs::exists(outputDir)) {
            fs::create_directories(outputDir);
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Failed to create output directory: " << e.what() << std::endl;
        return false;
    }

    // 3. Output dir is writable pre-check
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

    int videoStreamIdx = -1;
    AVCodecParameters* videoCodecPar = nullptr;
    AVStream* videoStream = nullptr;
    for (unsigned int i = 0; i < inputFmt->nb_streams; ++i) {
        if (inputFmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIdx = i;
            videoCodecPar = inputFmt->streams[i]->codecpar;
            videoStream = inputFmt->streams[i];
            break;
        }
    }
    if (videoStreamIdx == -1) {
        std::cerr << "No video stream found." << std::endl;
        return false;
    }

    const AVCodec* codec = avcodec_find_decoder(videoCodecPar->codec_id);
    if (!codec) {
        std::cerr << "Failed to find decoder." << std::endl;
        return false;
    }
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        std::cerr << "Failed to allocate codec context." << std::endl;
        return false;
    }
    if (avcodec_parameters_to_context(codecCtx, videoCodecPar) < 0) {
        std::cerr << "Failed to copy codec parameters." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        std::cerr << "Failed to open codec." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }

    int64_t startSec = 0, endSec = INT64_MAX;
    if (!startTime.empty()) {
        try { startSec = parseTimeToSeconds(startTime); }
        catch (const std::exception& e) {
            std::cerr << "Start time parse error: " << e.what() << std::endl;
            avcodec_free_context(&codecCtx);
            return false;
        }
    }
    if (!endTime.empty()) {
        try { endSec = parseTimeToSeconds(endTime); }
        catch (const std::exception& e) {
            std::cerr << "End time parse error: " << e.what() << std::endl;
            avcodec_free_context(&codecCtx);
            return false;
        }
    }
    // --- NEW: Duration Validation ---
    double durationSec = 0;
    if (inputFmt->duration > 0) {
        durationSec = inputFmt->duration / (double)AV_TIME_BASE;
    }

    if (durationSec > 0 && startSec >= durationSec) {
        std::cerr << "Error: Start time (" << startSec << "s) is beyond video duration (" 
                  << std::fixed << std::setprecision(2) << durationSec << "s)." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }

    if (durationSec > 0 && !endTime.empty() && endSec > durationSec) {
        std::cerr << "Error: End time (" << endSec << "s) exceeds video duration (" 
                  << std::fixed << std::setprecision(2) << durationSec << "s)." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }

    // 4. startSec < endSec check
    if (!endTime.empty() && startSec >= endSec) {
        std::cerr << "Error: Start time must be less than end time." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }

    double originalFPS = av_q2d(videoStream->r_frame_rate);
    if (originalFPS <= 0) originalFPS = av_q2d(videoStream->avg_frame_rate);
    if (originalFPS <= 0) originalFPS = 30.0;

    // 5. targetFPS <= sourceFPS check (Warning + Cap)
    if (targetFPS > originalFPS) {
        std::cerr << "Warning: targetFPS (" << targetFPS << ") is greater than source FPS (" 
                  << std::fixed << std::setprecision(2) << originalFPS << "). Capping to source FPS." << std::endl;
        targetFPS = static_cast<int>(std::round(originalFPS));
        if (targetFPS <= 0) targetFPS = 1;
    }

    int frameInterval = std::max(1, static_cast<int>(std::round(originalFPS / targetFPS)));


    std::cout << "Video info:\n"
              << "  - Original FPS: " << std::fixed << std::setprecision(2) << originalFPS << "\n"
              << "  - Target FPS: " << targetFPS << "\n"
              << "  - Frame interval: every " << frameInterval << " frames\n"
              << "  - Output directory: " << outputDir << "\n";
    if (startSec > 0) std::cout << "  - Start time: " << startSec << "s\n";
    if (endSec != INT64_MAX) std::cout << "  - End time: " << endSec << "s\n";
    std::cout << std::endl;

    if (startSec > 0) {
        int64_t seekTarget = av_rescale_q(startSec * AV_TIME_BASE,
                                          AVRational{1, AV_TIME_BASE},
                                          AV_TIME_BASE_Q);
        av_seek_frame(inputFmt.get(), -1, seekTarget, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(codecCtx);
    }

    // Scaler: output full-range YUVJ420P (JPEG compatible)
    SwsContextPtr swsCtx(sws_getContext(
        codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
        codecCtx->width, codecCtx->height, AV_PIX_FMT_YUVJ420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    ));
    if (!swsCtx) {
        std::cerr << "Failed to create scaler context." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }

    // Allocate frame for YUVJ output
    AVFramePtr yuvFrame(av_frame_alloc());
    yuvFrame->format = AV_PIX_FMT_YUVJ420P;
    yuvFrame->width = codecCtx->width;
    yuvFrame->height = codecCtx->height;
    if (av_frame_get_buffer(yuvFrame.get(), 32) < 0) {
        std::cerr << "Failed to allocate YUVJ buffer." << std::endl;
        avcodec_free_context(&codecCtx);
        return false;
    }

    AVFramePtr frame(av_frame_alloc());
    if (!frame) {
        std::cerr << "Failed to allocate input frame." << std::endl;
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
    std::cout << "Extracting frames..." << std::endl;

    while (av_read_frame(inputFmt.get(), packet.get()) >= 0) {
        if (packet->stream_index != videoStreamIdx) {
            av_packet_unref(packet.get());
            continue;
        }
        if (avcodec_send_packet(codecCtx, packet.get()) < 0) {
            av_packet_unref(packet.get());
            continue;
        }
        while (avcodec_receive_frame(codecCtx, frame.get()) >= 0) {
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
                av_packet_unref(packet.get());
                avcodec_send_packet(codecCtx, nullptr);
                std::cout << "\nExtraction complete. Extracted " << extractedCount << " frames." << std::endl;
                if (failedCount > 0) std::cerr << "Warning: " << failedCount << " frames failed to save." << std::endl;
                avcodec_free_context(&codecCtx);
                return true;
            }

            if (frameCount % frameInterval == 0) {
                // Convert to YUVJ420P
                const uint8_t* src[4] = { frame->data[0], frame->data[1], frame->data[2], nullptr };
                int srcStride[4] = { frame->linesize[0], frame->linesize[1], frame->linesize[2], 0 };
                sws_scale(swsCtx.get(), src, srcStride, 0, codecCtx->height,
                          yuvFrame->data, yuvFrame->linesize);
                
                // 8. Failure counter
                if (saveFrameAsJPEG(yuvFrame.get(), outputDir, extractedCount)) {
                    extractedCount++;
                    if (extractedCount % 10 == 0)
                        std::cout << "  Extracted " << extractedCount << " frames...\r" << std::flush;
                } else {
                    failedCount++;
                }
            }
            frameCount++;
            av_frame_unref(frame.get());
        }
        av_packet_unref(packet.get());
    }

    // Flush decoder
    avcodec_send_packet(codecCtx, nullptr);
    while (avcodec_receive_frame(codecCtx, frame.get()) >= 0) {
        if (frameCount % frameInterval == 0) {
            const uint8_t* src[4] = { frame->data[0], frame->data[1], frame->data[2], nullptr };
            int srcStride[4] = { frame->linesize[0], frame->linesize[1], frame->linesize[2], 0 };
            sws_scale(swsCtx.get(), src, srcStride, 0, codecCtx->height,
                      yuvFrame->data, yuvFrame->linesize);
            if (saveFrameAsJPEG(yuvFrame.get(), outputDir, extractedCount)) {
                extractedCount++;
            } else {
                failedCount++;
            }
        }
        frameCount++;
        av_frame_unref(frame.get());
    }

    std::cout << "\nExtraction complete. Extracted " << extractedCount << " frames." << std::endl;
    if (failedCount > 0) {
        std::cerr << "Warning: " << failedCount << " frames failed to save." << std::endl;
    }
    avcodec_free_context(&codecCtx);
    return true;
}


// ---------- Trimming (core) ----------
struct StreamMapping {
    std::vector<int> inToOut;
    std::vector<int> outToIn;
};

bool trimVideoSegment(const std::string& inputPath,
                      const std::string& outputPath,
                      int64_t startSec,
                      int64_t endSec,
                      bool removeAudio)
{
    if (startSec >= endSec) {
        std::cerr << "Error: Start time must be less than end time." << std::endl;
        return false;
    }

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

    AVFormatContext* rawOutput = nullptr;
    avformat_alloc_output_context2(&rawOutput, nullptr, nullptr, outputPath.c_str());
    if (!rawOutput) {
        std::cerr << "Could not create output context." << std::endl;
        return false;
    }
    AVFormatContextOutPtr outputFmt(rawOutput);

    StreamMapping mapping;
    mapping.inToOut.resize(inputFmt->nb_streams, -1);

    for (unsigned int i = 0; i < inputFmt->nb_streams; ++i) {
        AVStream* inStream = inputFmt->streams[i];
        AVCodecParameters* inCodecPar = inStream->codecpar;

        if (inCodecPar->codec_type != AVMEDIA_TYPE_VIDEO &&
            inCodecPar->codec_type != AVMEDIA_TYPE_AUDIO &&
            inCodecPar->codec_type != AVMEDIA_TYPE_SUBTITLE)
            continue;

        if (removeAudio && inCodecPar->codec_type == AVMEDIA_TYPE_AUDIO)
            continue;

        AVStream* outStream = avformat_new_stream(outputFmt.get(), nullptr);
        if (!outStream) {
            std::cerr << "Failed allocating output stream." << std::endl;
            return false;
        }

        if (avcodec_parameters_copy(outStream->codecpar, inCodecPar) < 0) {
            std::cerr << "Failed to copy codec parameters." << std::endl;
            return false;
        }
        outStream->codecpar->codec_tag = 0;
        outStream->disposition = inStream->disposition;
        av_dict_copy(&outStream->metadata, inStream->metadata, 0);
        outStream->time_base = inStream->time_base;

        mapping.inToOut[i] = static_cast<int>(outputFmt->nb_streams - 1);
        mapping.outToIn.push_back(static_cast<int>(i));
    }

    if (outputFmt->nb_streams == 0) {
        std::cerr << "No valid streams found for output." << std::endl;
        return false;
    }

    if (!(outputFmt->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outputFmt->pb, outputPath.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Could not open output file: " << outputPath << std::endl;
            return false;
        }
    }

    av_dict_copy(&outputFmt->metadata, inputFmt->metadata, 0);
    if (avformat_write_header(outputFmt.get(), nullptr) < 0) {
        std::cerr << "Error writing output file header." << std::endl;
        return false;
    }

    int64_t seekTarget = av_rescale_q(startSec * AV_TIME_BASE,
                                      AVRational{1, AV_TIME_BASE},
                                      AV_TIME_BASE_Q);
    av_seek_frame(inputFmt.get(), -1, seekTarget, AVSEEK_FLAG_BACKWARD);

    AVPacketPtr packet(av_packet_alloc());
    if (!packet) {
        std::cerr << "Could not allocate packet." << std::endl;
        return false;
    }

    std::vector<int64_t> ptsStartOffsets(inputFmt->nb_streams, AV_NOPTS_VALUE);
    std::vector<int64_t> dtsStartOffsets(inputFmt->nb_streams, AV_NOPTS_VALUE);
    std::vector<bool> streamStarted(inputFmt->nb_streams, false);
    int64_t actualStartSec = -1;
    bool endBoundaryReached = false;

    while (av_read_frame(inputFmt.get(), packet.get()) >= 0) {
        unsigned int inIdx = packet->stream_index;
        if (inIdx >= mapping.inToOut.size() || mapping.inToOut[inIdx] < 0) {
            av_packet_unref(packet.get());
            continue;
        }

        int outIdx = mapping.inToOut[inIdx];
        AVStream* inStream = inputFmt->streams[inIdx];
        AVStream* outStream = outputFmt->streams[outIdx];

        if (packet->pts == AV_NOPTS_VALUE) {
            av_packet_unref(packet.get());
            continue;
        }

        double packetPtsSec = packet->pts * av_q2d(inStream->time_base);

        if (!endBoundaryReached) {
            if (packetPtsSec > static_cast<double>(endSec)) {
                if (inStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                    endBoundaryReached = true;
                else {
                    av_packet_unref(packet.get());
                    continue;
                }
            }
        } else {
            if (inStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                if (packet->flags & AV_PKT_FLAG_KEY) {
                    av_packet_unref(packet.get());
                    break;
                }
            } else {
                av_packet_unref(packet.get());
                continue;
            }
        }

        if (packetPtsSec < static_cast<double>(startSec)) {
            av_packet_unref(packet.get());
            continue;
        }

        if (!streamStarted[inIdx]) {
            if (inStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && !(packet->flags & AV_PKT_FLAG_KEY)) {
                av_packet_unref(packet.get());
                continue;
            }
            ptsStartOffsets[inIdx] = packet->pts;
            dtsStartOffsets[inIdx] = (packet->dts != AV_NOPTS_VALUE) ? packet->dts : packet->pts;
            streamStarted[inIdx] = true;
            if (actualStartSec < 0) actualStartSec = static_cast<int64_t>(packetPtsSec);
        }

        packet->pts -= ptsStartOffsets[inIdx];
        if (packet->dts != AV_NOPTS_VALUE) packet->dts -= dtsStartOffsets[inIdx];
        if (packet->dts != AV_NOPTS_VALUE && packet->dts < 0) packet->dts = 0;
        if (packet->pts < 0) packet->pts = 0;

        av_packet_rescale_ts(packet.get(), inStream->time_base, outStream->time_base);
        packet->pos = -1;
        packet->stream_index = outIdx;

        if (av_interleaved_write_frame(outputFmt.get(), packet.get()) < 0) {
            std::cerr << "Error writing packet." << std::endl;
            break;
        }
        av_packet_unref(packet.get());
    }

    av_interleaved_write_frame(outputFmt.get(), nullptr);
    av_write_trailer(outputFmt.get());

    if (actualStartSec >= 0 && actualStartSec != startSec) {
        std::cout << "  [Note] Start cut adjusted to nearest keyframe. Actual start: "
                  << actualStartSec << "s (requested " << startSec << "s)." << std::endl;
    }
    return true;
}

// ---------- Batch trimming ----------
bool trimVideoBatch(const std::string& inputPath,
                    const std::string& outputDir,
                    const std::vector<TrimSegment>& segments,
                    bool removeAudio)
{
    if (segments.empty()) {
        std::cerr << "No segments provided." << std::endl;
        return false;
    }

    try {
        if (!fs::exists(outputDir)) fs::create_directories(outputDir);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Failed to create output directory: " << e.what() << std::endl;
        return false;
    }

    bool allOk = true;
    int total = static_cast<int>(segments.size());
    for (int i = 0; i < total; ++i) {
        std::ostringstream oss;
        oss << "clip_" << std::setw(3) << std::setfill('0') << (i+1) << ".mp4";
        std::string filename = oss.str();
        std::string outPath = (fs::path(outputDir) / filename).string();

        std::cout << "[" << (i+1) << "/" << total << "] Extracting "
                  << segments[i].start << " -> " << segments[i].end
                  << "  =>  " << filename << (removeAudio ? " [no audio]" : "") << std::endl;

        int64_t startSec = 0, endSec = 0;
        try {
            startSec = parseTimeToSeconds(segments[i].start);
            endSec   = parseTimeToSeconds(segments[i].end);
        } catch (const std::exception& e) {
            std::cerr << "  Time parse error: " << e.what() << std::endl;
            allOk = false;
            continue;
        }

        if (!trimVideoSegment(inputPath, outPath, startSec, endSec, removeAudio)) {
            std::cerr << "  FAILED." << std::endl;
            allOk = false;
        } else {
            std::cout << "  Done." << std::endl;
        }
    }
    return allOk;
}