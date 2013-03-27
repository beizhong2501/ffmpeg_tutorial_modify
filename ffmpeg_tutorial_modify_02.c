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
//gcc -o ffmpeg_tutorial_modify_02 ffmpeg_tutorial_modify_02.c  -lavformat -lavcodec -lswscale -lz -lm `sdl-config --cflags --libs`
//
// to build (assuming libavformat and libavcodec are correctly installed
// your system).
//
// Run using:  ./ffmpeg_tutorial_modify_02 test.avi
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



/* Called from the main */
int main(int argc, char **argv)
{
	AVFormatContext *pFormatCtx = NULL;
	int err;
	int i;
	int videoStream;
	AVCodecContext *pCodecCtx;
	AVCodec         *pCodec;
	AVFrame         *pFrame; 

	AVPacket        packet;
	int             frameFinished;
	float           aspect_ratio;

	SDL_Overlay     *bmp;
	SDL_Surface     *screen;
	SDL_Rect        rect;
	SDL_Event       event;


	
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
	printf("nb_streams = %d\n",pFormatCtx->nb_streams);
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
	videoStream=AVMEDIA_TYPE_UNKNOWN;
	for(i=0; i<pFormatCtx->nb_streams; i++)
	{
		if(AVMEDIA_TYPE_VIDEO==pFormatCtx->streams[i]->codec->codec_type)
		{
			videoStream=i;
			break;
		}
	}
	if(videoStream==AVMEDIA_TYPE_UNKNOWN)
	{
		return -1; // Didn't find a video stream
	}
	printf("videoStream = %d\n",videoStream);
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
			
			}
		}

		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);

		SDL_PollEvent(&event);
		switch(event.type) {
			case SDL_QUIT:
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
