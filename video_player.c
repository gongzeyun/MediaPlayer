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

/* SDL component*/
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;
SDL_Rect rect;
SDL_Event event;
int g_start_read = 0;

uint8_t temp_buffer[192000];
/* Audio Packet queue */
typedef struct pcm_buffer{
	uint8_t *data;
	int length;
	int posWrite;
	int posRead;
	SDL_mutex *mutex;
}pcm_buffer;


int init_pcm_buffer(pcm_buffer* ppbuffer, int length)
{
	ppbuffer->length = length;
	ppbuffer->data = malloc(length);
	if (NULL == ppbuffer->data) {
		printf("malloc pcm buffer failed\n");
		return -1;
	}
	memset(ppbuffer->data, 0x00, length);
	ppbuffer->mutex = SDL_CreateMutex();
	ppbuffer->posRead = 0;
	ppbuffer->posWrite = 0;

	return 0;
}

int destroy_pcm_buffer(pcm_buffer* ppbuffer)
{
    if (ppbuffer->data) {
		free(ppbuffer->data);
		ppbuffer->data = NULL;
    }

	return 0;
}
int write_pcm_buffer(pcm_buffer* ppbuffer, Uint8* pcm_write, int length)
{
    //printf("write to pcm buffer, length %d\n", length);
	int aviliablespace = 0;
	static int first_write = 1;
	SDL_LockMutex(ppbuffer->mutex);
	if (ppbuffer->posWrite <= ppbuffer->posRead) {
		aviliablespace = ppbuffer->posRead - ppbuffer->posWrite;
		if (0 == ppbuffer->posWrite && 0 == ppbuffer->posRead && first_write) {
			first_write = 0;
		    aviliablespace = ppbuffer->length;
	    }
		//printf("[write_pcm_buffer], aviliablespace %d, pos_write %d, pos_read %d, length %d\n", 
		//		aviliablespace, ppbuffer->posWrite, ppbuffer->posRead, length);
		
		if (aviliablespace > length) {
			memcpy(ppbuffer->data + ppbuffer->posWrite, pcm_write, length);
			ppbuffer->posWrite += length;
			SDL_UnlockMutex(ppbuffer->mutex);
			return 0;
		} else {
		 	SDL_UnlockMutex(ppbuffer->mutex);
			return -1;
		}
	} else {
		aviliablespace = ppbuffer->length - (ppbuffer->posWrite - ppbuffer->posRead);
		//printf("[write_pcm_buffer], aviliablespace %d, pos_write %d, pos_read %d, length %d\n", 
		//		aviliablespace, ppbuffer->posWrite, ppbuffer->posRead, length);
		if (aviliablespace > length) {
			if (ppbuffer->length - ppbuffer->posWrite > length) {
				memcpy(ppbuffer->data + ppbuffer->posWrite, pcm_write, length);
				ppbuffer->posWrite += length;
			} else {
				memcpy(ppbuffer->data + ppbuffer->posWrite, pcm_write, 
							ppbuffer->length - ppbuffer->posWrite);
				memcpy(ppbuffer->data, pcm_write + ppbuffer->length - ppbuffer->posWrite, 
					        length - (ppbuffer->length - ppbuffer->posWrite));
				ppbuffer->posWrite = length - (ppbuffer->length - ppbuffer->posWrite);
			}
			SDL_UnlockMutex(ppbuffer->mutex);
			return 0;
		} else {
			SDL_UnlockMutex(ppbuffer->mutex);
			return -1;
		}
	}
	SDL_UnlockMutex(ppbuffer->mutex);
	return 0;
	
}

int read_pcm_buffer(pcm_buffer* ppbuffer, Uint8* pcm_read, int length)
{
	//printf("read from pcm buffer, length %d\n", length);
	int aviliablebytes = 0;
	SDL_LockMutex(ppbuffer->mutex);
	if (ppbuffer->posRead <= ppbuffer->posWrite) {
		aviliablebytes = ppbuffer->posWrite - ppbuffer->posRead;
		//printf("[read_pcm_buffer], aviliablebytes %d, pos_write %d, pos_read %d, length %d\n", 
		//		aviliablebytes, ppbuffer->posWrite, ppbuffer->posRead, length);
		if (aviliablebytes > length) {
			memcpy(pcm_read, ppbuffer->data + ppbuffer->posRead, length);
			ppbuffer->posRead += length;
			SDL_UnlockMutex(ppbuffer->mutex);
			return 0;
		} else {
			SDL_UnlockMutex(ppbuffer->mutex);
		    return -1;
		}
	} else {
		aviliablebytes = ppbuffer->length - (ppbuffer->posRead - ppbuffer->posWrite);
		//printf("[read_pcm_buffer], aviliablebytes %d, pos_write %d, pos_read %d, length %d\n", 
		//		aviliablebytes, ppbuffer->posWrite, ppbuffer->posRead, length);
		if (aviliablebytes > length) {
			if (ppbuffer->length - ppbuffer->posRead > length) {
				memcpy(pcm_read, ppbuffer->data + ppbuffer->posRead, length);
				ppbuffer->posRead += length;
			} else {
				memcpy(pcm_read, ppbuffer->data + ppbuffer->posRead, 
						ppbuffer->length - ppbuffer->posRead);
				memcpy(pcm_read + (ppbuffer->length - ppbuffer->posRead), 
						ppbuffer->data, length - (ppbuffer->length - ppbuffer->posRead));
				ppbuffer->posRead = length - (ppbuffer->length - ppbuffer->posRead);
			}
			SDL_UnlockMutex(ppbuffer->mutex);
			return 0;
		}
		SDL_UnlockMutex(ppbuffer->mutex);
		return -1;
	}
	SDL_UnlockMutex(ppbuffer->mutex);
	return 0;
}
int init_SDL_system()
{
	 /* Init SDL */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		printf("SDL init failed\n");
		return -1;
	}

	return 0;
}

int init_SDL_play_video(char *title, int posx, int posy, int width, int height, int flag)
{
	window = SDL_CreateWindow(title, posx, posy, width, height, flag);
							   //SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (window) {
        renderer = SDL_CreateRenderer(window, -1, 0);
		if (renderer) {
			texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);
			if (!texture) {
				printf("create SDL texture failed\n");
				return -1;
			}
		} else {
		    printf("create SDL render failed\n");
			return -1;
		}
    } else {
        printf("create SDL Window failed\n");
		return -1;
    }

	return 0;
}

void audio_callback(void *opaque, Uint8 *stream, int len)
{
	//printf("enter audio callback\n");
	pcm_buffer *pbuffer = opaque;
	//if(read_pcm_buffer(pbuffer, stream, len)) {
	//	memset(stream, 0x00, len);
	//	printf("get pcm data failed, len %d\n", len);
	//
	//}
	if (g_start_read == 1) {
		//memcpy(stream, (uint8_t *)pbuffer->data + pbuffer->posRead, len);
		SDL_MixAudio(stream, pbuffer->data + pbuffer->posRead, len, SDL_MIX_MAXVOLUME);
		printf("get pcm data success, len %d, pos %d\n", len, pbuffer->posRead);
		pbuffer->posRead += len;
		if (pbuffer->posRead > pbuffer->posWrite) {
			pbuffer->posRead = 0;
		}
	} else {
	    printf("get pcm data failed, len %d\n", len);
		memset(stream, 0x00, len);
	}
	return;
}

int init_SDL_play_audio(void *userdata, int format, int channels, int sample)
{
    AVCodecContext *pAudioCodecContext = userdata;
	SDL_AudioSpec wanted_spec, spec;
	wanted_spec.channels = 2;
    wanted_spec.freq = sample;
	wanted_spec.format = AUDIO_S16SYS;
	wanted_spec.silence = 0;
    wanted_spec.samples = 2048;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = userdata;

	if(SDL_OpenAudio(&wanted_spec, NULL) < 0) {
		printf("open audio failed, wanted:channels %d, freq %dHz, format %d\n", channels, sample, format);
		printf("return:channels %d, freq %dHz, format %d\n", spec.channels, spec.freq, spec.format);
		return -1;
	} else {
		printf("open audio success, channels %d, freq %dHz, format %d\n", wanted_spec.channels, sample, format);
		//printf("return:channels %d, freq %dHz, format %d\n", spec.channels, spec.freq, spec.format);
		return 0;
	}
	
	
}

int display_video_frame(AVFrame *pFrame)
{
	rect.x = 0;
	rect.y = 0;
	rect.w = pFrame->width;
	rect.h = pFrame->height;
	SDL_UpdateYUVTexture(texture, NULL, pFrame->data[0], pFrame->linesize[0],
										pFrame->data[1], pFrame->linesize[1],
										pFrame->data[2], pFrame->linesize[2]);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, &rect);
	SDL_RenderPresent(renderer);
}


int main(int argc, char** argv)
{
	pcm_buffer pcm_buf; 
	init_pcm_buffer(&pcm_buf, 2 * 1024 * 1024);
	

    if (-1 == init_SDL_system()) {
    	printf("init sdl system fail\n");
		goto fail;
    }
	
	/* regiser all muxers ,demuxers, encoders, decoders */
	av_register_all();

	AVFormatContext *ic = NULL;
	
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


	/* get video&audio stream index */
	int video_stream_index = -1;
	int audio_stream_index = -1;
	
	int i;
	for (i = 0; i < ic->nb_streams; i++)
	{
		if (AVMEDIA_TYPE_VIDEO == ic->streams[i]->codec->codec_type)
		{
			video_stream_index = i;
			printf("video stream index is %d\n", video_stream_index);
		}
		if (AVMEDIA_TYPE_AUDIO == ic->streams[i]->codec->codec_type) {
			audio_stream_index = i;
			printf("audio stream index is %d\n", audio_stream_index);
		}
	}

	if (-1 == video_stream_index || -1 == audio_stream_index)
	{
		goto fail;
	}
	
	AVCodecContext *pVideoCodecContextOrigin = NULL;
	AVCodecContext *pAudioCodecContextOrigin = NULL;
	AVCodecContext *pAudioCodecContext = NULL;
	AVCodecContext *pVideoCodecContext = NULL;

	pVideoCodecContextOrigin = ic->streams[video_stream_index]->codec;
	pAudioCodecContextOrigin = ic->streams[audio_stream_index]->codec;
		
	/*find video codec*/
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


    /*find audio codec */
	AVCodec *pAudioCodec = NULL;
	printf("audio codec id %d\n", pAudioCodecContextOrigin->codec_id);
	pAudioCodec = avcodec_find_decoder(pAudioCodecContextOrigin->codec_id);
	if (NULL == pAudioCodec)
	{
		printf("can not find decoder, id %d\n", pVideoCodecContextOrigin->codec_id);
		goto fail;
	}
	
	
	pAudioCodecContext = avcodec_alloc_context3(pAudioCodec);

	/* copy codec context */
	if (avcodec_copy_context(pVideoCodecContext, pVideoCodecContextOrigin) < 0)
	{
		printf("copy codec context failed\n");
		goto fail;
	}

	/* copy codec context */
	if (avcodec_copy_context(pAudioCodecContext, pAudioCodecContextOrigin) < 0)
	{
		printf("copy codec context failed\n");
		goto fail;
	}

    init_SDL_play_video(ic->filename, 0, 0, 
						pVideoCodecContext->width, pVideoCodecContext->height,
						SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

	
	init_SDL_play_audio(&pcm_buf, pAudioCodecContext->sample_fmt, pAudioCodecContext->channels, pAudioCodecContext->sample_rate);
	
	/* open video decoder */
	if (avcodec_open2(pVideoCodecContext, pVideoCodec, NULL) < 0)
	{
		printf("open video codec failed\n");
		goto fail;
	}
	
	/* open audio decoder */
	if (avcodec_open2(pAudioCodecContext, pAudioCodec, NULL) < 0)
	{
		printf("open audio codec failed\n");
		goto fail;
	}

	//int64_t in_channel_layout = av_get_default_channel_layout(pAudioCodecContext->channels);
	struct SwrContext *au_convert_ctx;	
	printf("111\n");
	swr_free(&au_convert_ctx);
	au_convert_ctx = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 48000 ,  
		AV_CH_LAYOUT_STEREO,AV_SAMPLE_FMT_S16 , 48000,0, NULL);  
	printf("222\n");
	swr_init(au_convert_ctx); 
	printf("333\n");

	AVFrame *pVideoFrame = NULL;
	AVFrame *pAudioFrame = NULL;
	
	/* malloc space pFrame */
	pVideoFrame = av_frame_alloc();
	pAudioFrame = av_frame_alloc();
	
	if (NULL == pVideoFrame)
	{
		printf("malloc space pFrame failed\n");
		goto fail;
	}
	
	int frame_finished = -1;
	AVPacket packet;
	while (av_read_frame(ic, &packet) >= 0)
	{
		if (packet.stream_index == video_stream_index)
		{
		    /*decoder video packet */
			avcodec_decode_video2(pVideoCodecContext, pVideoFrame, &frame_finished, &packet);
			if (frame_finished)
			{
				display_video_frame(pVideoFrame);
			}
		} else if (packet.stream_index == audio_stream_index) {
		    /* decoder audio packet */
			avcodec_decode_audio4(pAudioCodecContext, pAudioFrame, &frame_finished, &packet);
			int data_size = 0;
			if (frame_finished) {
				swr_convert(au_convert_ctx,&temp_buffer, 192000,(const uint8_t **)pAudioFrame->data , pAudioFrame->nb_samples); 
			    data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(pAudioFrame),
                                           pAudioFrame->nb_samples,
                                           AV_SAMPLE_FMT_S16, 1);
				SDL_PauseAudio(0);
				printf("decoder audio sucess, data_size %d\n", data_size);
				if (pcm_buf.posWrite + data_size < pcm_buf.length) {
					memcpy(pcm_buf.posWrite + pcm_buf.data, temp_buffer, data_size);
					pcm_buf.posWrite += data_size;
				} else {
					g_start_read = 1;
				}
				//while (write_pcm_buffer(&pcm_buf,pAudioFrame->data[0], data_size)) {
				//	SDL_Delay(1);
				//}
				
			} else {
				printf("decoder audio failed\n");
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

	
	av_free(pVideoFrame);
    av_free(pAudioFrame);
	avcodec_close(pVideoCodecContext);
	avcodec_close(pVideoCodecContextOrigin);
	avcodec_close(pAudioCodecContext);
	avcodec_close(pAudioCodecContextOrigin);

	avformat_close_input(&ic);
	
	return 0;

	fail:
		printf("error, programe exit\n");
		return -1;
		
	
}
/*printf("Bitrate:\t %3d\n", pFormatCtx->bit_rate);  
    printf("Decoder Name:\t %s\n", pCodecCtx->codec->long_name);  
    printf("Channels:\t %d\n", pCodecCtx->channels);  
    printf("Sample per Second\t %d \n", pCodecCtx->sample_rate);  
  
    uint32_t ret,len = 0;  
    int got_picture;  
    int index = 0;  
    //FIX:Some Codec's Context Information is missing  
    int64_t in_channel_layout=av_get_default_channel_layout(pCodecCtx->channels);  
    //Swr  
    struct SwrContext *au_convert_ctx;  
    au_convert_ctx = swr_alloc();  
    au_convert_ctx=swr_alloc_set_opts(au_convert_ctx,out_channel_layout, out_sample_fmt, out_sample_rate,  
        in_channel_layout,pCodecCtx->sample_fmt , pCodecCtx->sample_rate,0, NULL);  
    swr_init(au_convert_ctx);  
  
    //Play  
    SDL_PauseAudio(0);  
  
    while(av_read_frame(pFormatCtx, packet)>=0){  
        if(packet->stream_index==audioStream){  
  
            ret = avcodec_decode_audio4( pCodecCtx, pFrame,&got_picture, packet);  
            if ( ret < 0 ) {  
                printf("Error in decoding audio frame.\n");  
                return -1;  
            }  
            if ( got_picture > 0 ){  
                swr_convert(au_convert_ctx,&out_buffer, MAX_AUDIO_FRAME_SIZE,(const uint8_t **)pFrame->data , pFrame->nb_samples);  
  
                printf("index:%5d\t pts:%lld\t packet size:%d\n",index,packet->pts,packet->size);  
  
#if OUTPUT_PCM  
                //Write PCM  
                fwrite(out_buffer, 1, out_buffer_size, pFile);  
#endif  
                  
                index++;  
            }  
//SDL------------------  
#if USE_SDL  
            //Set audio buffer (PCM data)  
            audio_chunk = (Uint8 *) out_buffer;   
            //Audio buffer length  
            audio_len =out_buffer_size;  
  
            audio_pos = audio_chunk;  */

