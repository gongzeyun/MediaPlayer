/*
	Author:    	gongzeyun
	Date:		2015. 9. 09
	Function:	this is a very simple player	
*/

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#define SDL2

int main(int argc, char** argv)
{

	/* First, let's regiser all muxers ,demuxers, coders, decoders */
	av_register_all();

	AVFormatContext *ic = NULL;
	AVCodecContext *pVideoCodecContextOrigin = NULL;
	AVCodecContext *pVideoCodecContext = NULL;
	
	/* alloc space for AVFormatContext */
	ic = avformat_alloc_context();

	printf("video file name is %s\n", argv[1]);
	if (NULL == ic)
	{
		printf("malloc space for AVFormatContext failed...\n");
		goto fail;
	}

	
	/* OK, it's the time to open file */
	int err = avformat_open_input(&ic, argv[1], NULL, NULL);
	if (0 > err)
	{
		printf("open file %s failed\n", argv[1]);
		goto fail;
	}

	/* find stream info */
	if(avformat_find_stream_info(ic, NULL) < 0)
	{
		printf("find stream info error\n");
		goto fail;
	}

	/* output the media file info */
	av_dump_format(ic, 0, argv[1], 0);


	/* get video stream index */
	int video_stream_index = -1;
	int i;
	for (i = 0; i < ic->nb_streams; i++)
	{
		if (AVMEDIA_TYPE_VIDEO == ic->streams[i]->codec->codec_type)
		{
			video_stream_index = i;
			printf("video stream index is %d\n", video_stream_index);
			break;
		}
	}

	if (-1 == video_stream_index)
	{
		goto fail;
	}

	pVideoCodecContextOrigin = ic->streams[video_stream_index]->codec;
	
	/* Now, let's find the codec*/
	AVCodec *pVideoCodec = NULL;
	printf("video codec id %d\n", pVideoCodecContextOrigin->codec_id);
	pVideoCodec = avcodec_find_decoder(pVideoCodecContextOrigin->codec_id);
	if (NULL == pVideoCodec)
	{
		printf("can not find decoder, id %d\n", pVideoCodecContextOrigin->codec_id);
		goto fail;
	}




	/* we can not use pVideoCodecContext directly, so we must get a copy  */
	pVideoCodecContext = avcodec_alloc_context3(pVideoCodec);


	/* copy codec context */
	if (avcodec_copy_context(pVideoCodecContext, pVideoCodecContextOrigin) < 0)
	{
		printf("copy codec context failed\n");
		goto fail;
	}

#ifndef SDL2
	/* Init SDL */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		printf("SDL init failed\n");
		goto fail;
	}
	SDL_Surface *screen = NULL;
	screen = SDL_SetVideoMode(pVideoCodecContext->width, pVideoCodecContext->height, 
								0, 0);
	if (NULL == screen)
	{
		printf("Create SDL surface failed\n");
		goto fail;
	}

	SDL_Overlay *bmp = NULL;
	bmp = SDL_CreateYUVOverlay(pVideoCodecContext->width, pVideoCodecContext->height, 
								SDL_YV12_OVERLAY, screen);
	if (NULL == bmp)
	{
		printf("SDL Create YUV Overlay failed\n");
		goto fail;
	}
#else
    /* Init SDL */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		printf("SDL init failed\n");
		goto fail;
	}
	SDL_Window *window = NULL;
	SDL_Renderer *renderer = NULL;
	SDL_Texture *texture = NULL;
	window = SDL_CreateWindow(ic->filename, 0, 0, 
								pVideoCodecContext->width, pVideoCodecContext->height,
								SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (window) {
        renderer = SDL_CreateRenderer(window, -1, 0);
		if (renderer) {
			texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, pVideoCodecContext->width, pVideoCodecContext->height);
			if (!texture) {
				printf("create SDL texture failed\n");
				goto fail;
			}
		} else {
		    printf("create SDL render failed\n");
			goto fail;
		}
    } else {
        printf("create SDL Window failed\n");
		goto fail;
    }
#endif

	SDL_Rect rect;
	SDL_Event event;
	
	/* open decoder */
	if (avcodec_open2(pVideoCodecContext, pVideoCodec, NULL) < 0)
	{
		printf("open codec failed\n");
		goto fail;
	}


	AVFrame *pFrame = NULL;
	
#if 0
	AVFrame *pFrameRGB = NULL;
#endif
	/* malloc space pFrame */
	pFrame = av_frame_alloc();
	if (NULL == pFrame)
	{
		printf("malloc space pFrame failed\n");
		goto fail;
	}
	
	struct SwsContext *sws_context = NULL;
	int frame_finished = -1;
	AVPacket packet;

	
#ifndef SDL2	
	sws_context = sws_getContext(pVideoCodecContext->width, 
								 pVideoCodecContext->height, 
								 pVideoCodecContext->pix_fmt, 
								 pVideoCodecContext->width, 
								 pVideoCodecContext->height,
								 AV_PIX_FMT_YUV420P, 
								 SWS_BILINEAR,
								 NULL, 
								 NULL, 
								 NULL);
#endif
	i = 0;
	while (av_read_frame(ic, &packet) >= 0)
	{
		if (packet.stream_index == video_stream_index)
		{
			avcodec_decode_video2(pVideoCodecContext, pFrame, &frame_finished, &packet);
			if (frame_finished)
			{
#ifndef SDL2
				/* display the image */
				SDL_LockYUVOverlay(bmp);
#endif
				AVPicture picture;

#ifndef SDL2
				picture.data[0] = bmp->pixels[0];
				picture.data[1] = bmp->pixels[2];
				picture.data[2] = bmp->pixels[1];

				picture.linesize[0] = bmp->pitches[0];
				picture.linesize[1] = bmp->pitches[2];
				picture.linesize[2] = bmp->pitches[1];				
#endif

#ifndef SDL2				
				/* convert image from its native fmt to YUV420 */
				sws_scale(sws_context, (const uint8_t * const*)pFrame->data, pFrame->linesize, 0, 
							pVideoCodecContext->height, picture.data, picture.linesize);
				
				printf("Y line_size %d, U line_size %d, V line_size %d\n", 
#endif							picture.linesize[0], picture.linesize[1], picture.linesize[2]);
#ifndef SDL2				
				SDL_UnlockYUVOverlay(bmp);
#endif
				rect.x = 0;
				rect.y = 0;
				rect.w = pVideoCodecContext->width;
				rect.h = pVideoCodecContext->height;
#ifndef SDL2
				SDL_DisplayYUVOverlay(bmp, &rect);
#else
                SDL_UpdateYUVTexture(texture, NULL, pFrame->data[0], pFrame->linesize[0],
                                                  pFrame->data[1], pFrame->linesize[1],
                                                  pFrame->data[2], pFrame->linesize[2]);
                SDL_RenderClear(renderer);
				SDL_RenderCopy(renderer, texture, NULL, &rect);
				SDL_RenderPresent(renderer);
#endif
			}
		}

		av_free_packet(&packet);
		SDL_PollEvent(&event);
		switch (event.type)
		{
			case SDL_QUIT:
				{
					SDL_Quit();
					break;
				}
			case SDL_KEYDOWN:
				{
					switch (event.key.keysym.sym)
					{
						case SDLK_q:
							exit(0);
							break;
						default:
							break;
					}
				}
			default:
				{
					break;
				}
		}
	}

	
	av_free(pFrame);

	avcodec_close(pVideoCodecContext);
	avcodec_close(pVideoCodecContextOrigin);

	avformat_close_input(&ic);
	
	return 0;

	fail:
		printf("error, programe exit\n");
		return -1;
		
	
}


