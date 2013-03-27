// tutorial01.c

// Code based on a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1

// modified by zhaotq(zhaotq@hotmail.com) 
// and test on gcc (Ubuntu/Linaro 4.6.3-1ubuntu5) 4.6.3

// A small sample program that shows how to use libavformat and libavcodec to
// read video from a file.
//
// Use
//
//gcc -o ffmpeg_tutorial_modify_03 ffmpeg_tutorial_modify_03.c  -lavformat -lavcodec -lswscale -lz -lm `sdl-config --cflags --libs`
//
// to build (assuming libavformat and libavcodec are correctly installed
// your system).
//
// Run using:  ./ffmpeg_tutorial_modify_03 test.avi
//
// to write the first five frames from "test.avi" to disk in PPM
// format.

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"


#include <SDL.h>
#include <SDL_thread.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#include <stdio.h>

#define SDL_AUDIO_BUFFER_SIZE 1024

typedef struct PacketQueue {
  AVPacketList *first_pkt, *last_pkt;
  int nb_packets;
  int size;
  SDL_mutex *mutex;
  SDL_cond *cond;
} PacketQueue;

PacketQueue audioq;

int quit = 0;

void packet_queue_init(PacketQueue *q) {
  memset(q, 0, sizeof(PacketQueue));
  q->mutex = SDL_CreateMutex();
  q->cond = SDL_CreateCond();
}
int packet_queue_put(PacketQueue *q, AVPacket *pkt) {

  AVPacketList *pkt1;
  if(av_dup_packet(pkt) < 0) {
    return -1;
  }
  pkt1 = av_malloc(sizeof(AVPacketList));
  if (!pkt1)
    return -1;
  pkt1->pkt = *pkt;
  pkt1->next = NULL;
  
  
  SDL_LockMutex(q->mutex);
  
  if (!q->last_pkt)
    q->first_pkt = pkt1;
  else
    q->last_pkt->next = pkt1;
  q->last_pkt = pkt1;
  q->nb_packets++;
  q->size += pkt1->pkt.size;
  SDL_CondSignal(q->cond);
  
  SDL_UnlockMutex(q->mutex);
  return 0;
}
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
  AVPacketList *pkt1;
  int ret;
  
  SDL_LockMutex(q->mutex);
  
  for(;;) {
    
    if(quit) {
      ret = -1;
      break;
    }

    pkt1 = q->first_pkt;
    if (pkt1) {
      q->first_pkt = pkt1->next;
      if (!q->first_pkt)
	q->last_pkt = NULL;
      q->nb_packets--;
      q->size -= pkt1->pkt.size;
      *pkt = pkt1->pkt;
      av_free(pkt1);
      ret = 1;
      break;
    } else if (!block) {
      ret = 0;
      break;
    } else {
      SDL_CondWait(q->cond, q->mutex);
    }
  }
  SDL_UnlockMutex(q->mutex);
  return ret;
}

int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf, int buf_size) {

  static AVPacket pkt;
  static uint8_t *audio_pkt_data = NULL;
  static int audio_pkt_size = 0;

  int len1, data_size;

  for(;;) {
    while(audio_pkt_size > 0) {
      data_size = buf_size;
	  
      len1 = avcodec_decode_audio3(aCodecCtx, (int16_t *)audio_buf, &data_size, 
				  &pkt);
	  
      if(len1 < 0) {
	/* if error, skip frame */
	audio_pkt_size = 0;
	break;
      }
      audio_pkt_data += len1;
      audio_pkt_size -= len1;
      if(data_size <= 0) {
	/* No data yet, get more frames */
	continue;
      }
      /* We have data, return it and come back for more later */
      return data_size;
    }
    if(pkt.data)
      av_free_packet(&pkt);

    if(quit) {
      return -1;
    }

    if(packet_queue_get(&audioq, &pkt, 1) < 0) {
      return -1;
    }
    audio_pkt_data = pkt.data;
    audio_pkt_size = pkt.size;
  }
}

void audio_callback(void *userdata, Uint8 *stream, int len) {

  AVCodecContext *aCodecCtx = (AVCodecContext *)userdata;
  int len1, audio_size;

  static uint8_t audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
  static unsigned int audio_buf_size = 0;
  static unsigned int audio_buf_index = 0;

  while(len > 0) {
    if(audio_buf_index >= audio_buf_size) {
      /* We have already sent all our data; get more */
      audio_size = audio_decode_frame(aCodecCtx, audio_buf, sizeof(audio_buf));
      if(audio_size < 0) {
	/* If error, output silence */
	audio_buf_size = 1024; // arbitrary?
	memset(audio_buf, 0, audio_buf_size);
      } else {
	audio_buf_size = audio_size;
      }
      audio_buf_index = 0;
    }
    len1 = audio_buf_size - audio_buf_index;
    if(len1 > len)
      len1 = len;
    memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
    len -= len1;
    stream += len1;
    audio_buf_index += len1;
  }
}


/* Called from the main */
int main(int argc, char **argv)
{
	AVFormatContext *pFormatCtx = NULL;
	int err;
	int i;
	int videoStream, audioStream;;
	AVCodecContext *pCodecCtx;
	AVCodec         *pCodec;
	AVFrame         *pFrame; 

	AVPacket        packet;
	int             frameFinished;
	float           aspect_ratio;

	AVCodecContext  *aCodecCtx;
	AVCodec         *aCodec;

	SDL_Overlay     *bmp;
	SDL_Surface     *screen;
	SDL_Rect        rect;
	SDL_Event       event;
	SDL_AudioSpec   wanted_spec, spec;

	
	if(argc < 2) {
		printf("Please provide a movie file\n");
		return -1;
	}
	
	// Register all formats and codecs
	av_register_all();

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
		exit(1);
	}

	pFormatCtx = avformat_alloc_context();
	
	// Open video file
	err = avformat_open_input(&pFormatCtx, argv[1],NULL,NULL);
	if(err<0)
	{
		printf("error ret =  %d\n",err);
	}

	// Retrieve stream information
	err = avformat_find_stream_info(pFormatCtx, NULL);
	if(err<0)
	{
		printf("error ret =  %d\n",err);
	}

	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, argv[1], 0);

	// Find the first video stream
	videoStream = AVMEDIA_TYPE_UNKNOWN;
	audioStream=AVMEDIA_TYPE_UNKNOWN;
	for(i=0; i<pFormatCtx->nb_streams; i++) {
		if(AVMEDIA_TYPE_VIDEO==pFormatCtx->streams[i]->codec->codec_type &&
			videoStream < 0) {
			videoStream=i;
		}
		if(AVMEDIA_TYPE_AUDIO==pFormatCtx->streams[i]->codec->codec_type &&
			audioStream < 0) {
			audioStream=i;
		}
	}
	
	if(videoStream==AVMEDIA_TYPE_UNKNOWN)
	{
		return -1; // Didn't find a video stream
	}

	if(audioStream==AVMEDIA_TYPE_UNKNOWN)
	{
		return -1; // Didn't find a video stream
	}


	aCodecCtx=pFormatCtx->streams[audioStream]->codec;

	// Set audio settings from codec info
	wanted_spec.freq = aCodecCtx->sample_rate;
	wanted_spec.format = AUDIO_S16SYS;
	wanted_spec.channels = aCodecCtx->channels;
	wanted_spec.silence = 0;
	wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
	wanted_spec.callback = audio_callback;
	wanted_spec.userdata = aCodecCtx;

	if(SDL_OpenAudio(&wanted_spec, &spec) < 0) {
		fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
		return -1;
	}

	aCodec = avcodec_find_decoder(aCodecCtx->codec_id);
	if(!aCodec) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1;
	}

	avcodec_open2(aCodecCtx, aCodec,NULL);

	// audio_st = pFormatCtx->streams[index]
	packet_queue_init(&audioq);
	SDL_PauseAudio(0);


	// Get a pointer to the codec context for the video stream
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;

	// Find the decoder for the video stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}
	// Open codec
	if(avcodec_open2(pCodecCtx, pCodec,NULL)<0)
		return -1; // Could not open codec

	// Allocate video frame
	pFrame=avcodec_alloc_frame();


	// Make a screen to put our video
#ifndef __DARWIN__
	screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
#else
	screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 24, 0);
#endif
	if(!screen) {
		fprintf(stderr, "SDL: could not set video mode - exiting\n");
		exit(1);
	}

	// Allocate a place to put our YUV image on that screen
	bmp = SDL_CreateYUVOverlay(pCodecCtx->width,
			 pCodecCtx->height,
			 SDL_YV12_OVERLAY,
			 screen);

	// Read frames and save first five frames to disk
	i=0;
	while(av_read_frame(pFormatCtx, &packet)>=0)
	{
		// Is this a packet from the video stream?
		if(packet.stream_index==videoStream) 
		{
			// Decode video frame
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, 
									&packet);

			// Did we get a video frame?
			if(frameFinished) 
				{

					SDL_LockYUVOverlay(bmp);

					AVPicture pict;
					pict.data[0] = bmp->pixels[0];
					pict.data[1] = bmp->pixels[2];
					pict.data[2] = bmp->pixels[1];

					pict.linesize[0] = bmp->pitches[0];
					pict.linesize[1] = bmp->pitches[2];
					pict.linesize[2] = bmp->pitches[1];

					// Convert the image into YUV format that SDL uses
					#include <libswscale/swscale.h>
					// other codes
					static struct SwsContext *img_convert_ctx;
					// other codes
					img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
													 pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
													 PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
					// other codes
					// Convert the image from its native format to RGB
					sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize,
					 0, pCodecCtx->height, pict.data, pict.linesize);

					SDL_UnlockYUVOverlay(bmp);

					rect.x = 0;
					rect.y = 0;
					rect.w = pCodecCtx->width;
					rect.h = pCodecCtx->height;
					SDL_DisplayYUVOverlay(bmp, &rect);
					av_free_packet(&packet);
			      }
			} 
			else if(packet.stream_index==audioStream) {
				packet_queue_put(&audioq, &packet);
			} else {
				av_free_packet(&packet);
			}

		SDL_PollEvent(&event);
		switch(event.type) {
			case SDL_QUIT:
				quit = 1;
				SDL_Quit();
				exit(0);
				break;
			default:
				break;
		}		
	}

	// Free the YUV frame
	av_free(pFrame);

	// Close the codec
	avcodec_close(pCodecCtx);

	// Close the video file
	avformat_close_input(&pFormatCtx);

	return 0;
	
}
