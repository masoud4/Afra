#include "Afra.h"
#include "X11/X.h"
#include "X11/Xlib.h"
#include "X11/Xutil.h"
#include "libavformat/avio.h"
#include "libavutil/error.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define FAILERROR(code)                                                        \
  do {                                                                         \
    OnFfmpegError(code);                                                       \
    exit(1);                                                                   \
  } while (0)

struct Afra {
  int height, width;
  Display *display;
  Window window;
  AVCodec *codec;
  AVCodecContext *context;
  AVFrame *frame;
  AVPacket *pkt;
  int channel;
  FILE *output;
  int fps;
} Afra;

static void OnFfmpegError(const int code) {
  printf("[EE] %s \n", ffmpegError[abs(code)]);
}
static void encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt,
                   FILE *outfile) {
  int ret;
  /* send the frame to the encoder */
  if (frame)
    ret = avcodec_send_frame(enc_ctx, frame);
  if (ret < 0) {
    FAILERROR(ret);
  }

  while (ret >= 0) {
    ret = avcodec_receive_packet(enc_ctx, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      return;
    else if (ret < 0) {
      FAILERROR(ret);
    }
    fwrite(pkt->data, 1, pkt->size, outfile);
    av_packet_unref(pkt);
  }
}

int main(int argc, char **argv) {
  const char *filename, *codec_name;
  uint8_t endcode[] = {0, 0, 1, 0xb7};
  Display *display = XOpenDisplay(NULL);
  Afra.display = display;
  int screen = XDefaultScreen(display);
  Afra.window = DefaultRootWindow(display);
  int index = 0;
  unsigned long pixel;
  Afra.context = NULL;
  Afra.height = DisplayHeight(display, screen);
  Afra.width = DisplayWidth(display, screen);
  Afra.channel = 4;
  Afra.fps = 120;
  if (Afra.width <= 0 || Afra.height <= 0) {
    fprintf(stderr, "Could not get Witdh and Height. \n");
    exit(0);
  }

  if (argc <= 2) {
    fprintf(stderr, "Usage: %s <output file> <codec name>\n", argv[0]);
    exit(0);
  }

  filename = argv[1];
  codec_name = argv[2];

  /* find the mpeg1video encoder */
  Afra.codec = avcodec_find_encoder_by_name(codec_name);
  if (!Afra.codec) {
    fprintf(stderr, "Codec '%s' not found\n", codec_name);
    exit(1);
  }
  Afra.context = avcodec_alloc_context3(Afra.codec);
  if (!Afra.context) {
    fprintf(stderr, "Could not allocate video codec context\n");
    exit(1);
  }

  Afra.pkt = av_packet_alloc();
  if (!Afra.pkt)
    exit(1);

  /* put sample parameters */
  Afra.context->bit_rate = 4000000;
  /* resolution must be a multiple of two */
  Afra.context->width = Afra.width;
  Afra.context->height = Afra.height;
  /* frames per second */
  Afra.context->time_base = (AVRational){1, 15};
  Afra.context->framerate = (AVRational){Afra.fps, 1};

  /* emit one intra frame every ten frames
   * check frame pict_type before passing frame
   * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
   * then gop_size is ignored and the output of encoder
   * will always be I frame irrespective to gop_size
   */
  Afra.context->gop_size = 10;
  Afra.context->max_b_frames = 1;
  Afra.context->pix_fmt = AV_PIX_FMT_YUV420P;

  if (Afra.codec->id == AV_CODEC_ID_H264)
    av_opt_set(Afra.context->priv_data, "preset", "slow", 0);

  /* open it */

  int ret = avcodec_open2(Afra.context, Afra.codec, NULL);
  if (ret < 0) {
    fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
    exit(1);
  }

  Afra.output = fopen(filename, "wb");
  if (!Afra.output) {
    fprintf(stderr, "Could not open %s\n", filename);
    exit(1);
  }

  Afra.frame = av_frame_alloc();
  if (!Afra.frame) {
    fprintf(stderr, "Could not allocate video frame\n");
    exit(1);
  }
  Afra.frame->format = Afra.context->pix_fmt;
  Afra.frame->width = Afra.context->width;
  Afra.frame->height = Afra.context->height;

  uint8_t *rgb_buffer =
      malloc(Afra.context->width * Afra.context->height * 3 * sizeof(uint8_t));

  ret = av_frame_get_buffer(Afra.frame, 0);
  if (ret < 0) {
    fprintf(stderr, "Could not allocate the video frame data\n");
    exit(1);
  }

  fflush(stdout);
  while (True) {
    /* make sure the frame data is writable */
    ret = av_frame_make_writable(Afra.frame);
    if (ret < 0) {
      FAILERROR(12);
    }
    XImage *image = XGetImage(Afra.display, Afra.window, 0, 0, Afra.width,
                              Afra.height, AllPlanes, ZPixmap);
    for (int j = 0; j < Afra.height; j++) {
      for (int i = 0; i < Afra.width; i++) {
        // Extract RGB components from the pixel
        unsigned long pixel = XGetPixel(image, i, j);
        uint8_t red = (pixel & image->red_mask) >> 16;
        uint8_t green = (pixel & image->green_mask) >> 8;
        uint8_t blue = (pixel & image->blue_mask);

        // Convert RGB to YCbCr color space
        int Y = 0.299 * red + 0.587 * green + 0.114 * blue;
        int Cb = -0.1687 * red - 0.3313 * green + 0.5 * blue + 128;
        int Cr = 0.5 * red - 0.4187 * green - 0.0813 * blue + 128;

        // Write YCbCr values to destination image buffer
        uint8_t *Y_row = Afra.frame->data[0] + j * Afra.frame->linesize[0];
        uint8_t *Cb_row = Afra.frame->data[1] + j / 2 * Afra.frame->linesize[1];
        uint8_t *Cr_row = Afra.frame->data[2] + j / 2 * Afra.frame->linesize[2];

        Y_row[i] = Y;
        if (i % 2 == 0 && j % 2 == 0) {
          Cb_row[i / 2] = Cb;
          Cr_row[i / 2] = Cr;
        }
      }
    }
    Afra.frame->pts = index;
    index += 1;

    /* encode the image */
    encode(Afra.context, Afra.frame, Afra.pkt, Afra.output);
    XDestroyImage(image);
  }

  /* flush the encoder */
  // encode(Afra.context, NULL, Afra.pkt, Afra.output);

  /* add sequence end code to have a real MPEG file */
  if (Afra.codec->id == AV_CODEC_ID_MPEG1VIDEO ||
      Afra.codec->id == AV_CODEC_ID_MPEG2VIDEO)
    fwrite(endcode, 1, sizeof(endcode), Afra.output);
  fclose(Afra.output);
  avcodec_free_context(&Afra.context);
  av_frame_free(&Afra.frame);
  av_packet_free(&Afra.pkt);
  return 0;
}
