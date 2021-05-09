#include "ffmpeg.h"
#include "Log.H"

#include <memory>

extern "C" {
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
}

using namespace ps;

namespace {
  auto logger = Log("MEDIA");
}

#define PTR(type, deleter)     \
  struct type ## Deleter       \
  {                            \
    void operator()(type* t)   \
    {                          \
      if (t) {                 \
        deleter(&t);           \
      }                        \
    }                          \
  };                           \
  using type ## Ptr = std::unique_ptr<type, type ## Deleter>;

PTR(AVFormatContext, avformat_close_input) // AVFormatContextPtr
PTR(AVCodecContext, avcodec_free_context)  // AVFCodecContextPtr


using FormatPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;

/// Blantanly stolen from ffmpeg’s examples/demuxing_decoding.c "open_codec_context".
AudioData ps::readAudioFile(const std::filesystem::path& path)
{
  AVFormatContext* formats = nullptr;
  if (avformat_open_input(&formats, path.c_str(), nullptr, nullptr) < 0) {
    logger.throw_("Unable to open {}", path);
  }
  AVFormatContextPtr formatsP(formats);

  int streamIndex = av_find_best_stream(formats, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
  if (streamIndex < 0) {
    logger.throw_("Could not find any audio stream in {}", path);
  }

  /* find decoder for the stream */
  AVStream* st = formats->streams[streamIndex];
  const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
  if (codec == nullptr) {
    logger.throw_("Audio codec unknown to ffmpeg (id: {})",
        st->codecpar->channel_layout);
  }

  /* Allocate a codec context for the decoder */
  AVCodecContext* context = avcodec_alloc_context3(codec);
  if (context == nullptr) {
    logger.throw_("Failed to allocate audio decoder context (out of memory ?)");
  }
  AVCodecContextPtr contextP(context);

  /* Copy codec parameters from input stream to output codec context */
  if (avcodec_parameters_to_context(context, st->codecpar) < 0) {
    logger.throw_("Failed to copy audio parameters (out of memory ?)");
  }

  /* Init the decoders */
  AVDictionary* opts = nullptr; // TODO: leaked ?
  if (avcodec_open2(context, codec, &opts) < 0) {
    logger.throw_("Failed to initialize audio codec for {}", path);
  }

  return {};
}
