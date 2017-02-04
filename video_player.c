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


int main(int argc, char** argv)
{

	/* regiser all muxers ,demuxers, encoders, decoders */
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

    /* open input*/
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

	/* dump media info*/
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
	
	/*find the codec*/
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

	
	i = 0;
	while (av_read_frame(ic, &packet) >= 0)
	{
		if (packet.stream_index == video_stream_index)
		{
			avcodec_decode_video2(pVideoCodecContext, pFrame, &frame_finished, &packet);
			if (frame_finished)
			{
				AVPicture picture;
				
				rect.x = 0;
				rect.y = 0;
				rect.w = pVideoCodecContext->width;
				rect.h = pVideoCodecContext->height;

                SDL_UpdateYUVTexture(texture, NULL, pFrame->data[0], pFrame->linesize[0],
                                                  pFrame->data[1], pFrame->linesize[1],
                                                  pFrame->data[2], pFrame->linesize[2]);
                SDL_RenderClear(renderer);
				SDL_RenderCopy(renderer, texture, NULL, &rect);
				SDL_RenderPresent(renderer);

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


