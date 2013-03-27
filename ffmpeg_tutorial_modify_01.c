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
//gcc -o ffmpeg_tutorial_modify_01 ffmpeg_tutorial_modify_01.c  -lavformat -lavcodec -lswscale -lz
//
// to build (assuming libavformat and libavcodec are correctly installed
// your system).
//
// Run using:  ./ffmpeg_tutorial_modify_01 test.avi
//
// to write the first five frames from "test.avi" to disk in PPM
// format.

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include <stdio.h>

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) 
{
	FILE *pFile;
	char szFilename[32];
	int  y;

	// Open file
	sprintf(szFilename, "frame%d.ppm", iFrame);
	pFile=fopen(szFilename, "wb");
	if(NULL==pFile)
	{
		return;
	}
	// Write header
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);

	// Write pixel data
	for(y=0; y<height; y++)
	{
		fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
	}
	// Close file
	fclose(pFile);
}

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
	AVFrame         *pFrameRGB;
	AVPacket        packet;
	int             frameFinished;
	int             numBytes;
	uint8_t         *buffer;

	if(argc < 2) {
		printf("Please provide a movie file\n");
		return -1;
	}
	
	// Register all formats and codecs
	av_register_all();

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

	// Allocate an AVFrame structure
	pFrameRGB=avcodec_alloc_frame();
	if(pFrameRGB==NULL)
	{
		return -1;
	}
	// Determine required buffer size and allocate buffer
	numBytes=avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width,
			      pCodecCtx->height);
	buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24,
		 pCodecCtx->width, pCodecCtx->height);

	printf("width = %d height = %d\n",pCodecCtx->width, pCodecCtx->height);


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
				#if 0
				// Convert the image from its native format to RGB
				img_convert((AVPicture *)pFrameRGB, PIX_FMT_RGB24, 
				    (AVPicture*)pFrame, pCodecCtx->pix_fmt, pCodecCtx->width, 
				    pCodecCtx->height);
				
				#else
				#include <libswscale/swscale.h>
				// other codes
				static struct SwsContext *img_convert_ctx;
				// other codes
				img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
				 pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
				 PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
				// other codes
				// Convert the image from its native format to RGB
				sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize,
				 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
				#endif
				// Save the frame to disk
				if(++i<=5)
				{
					SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height,i);
				}
			}
		}

	// Free the packet that was allocated by av_read_frame
	av_free_packet(&packet);
	}

	// Free the RGB image
	av_free(buffer);
	av_free(pFrameRGB);

	// Free the YUV frame
	av_free(pFrame);

	// Close the codec
	avcodec_close(pCodecCtx);

	// Close the video file
	avformat_close_input(&pFormatCtx);

	return 0;
	
}
