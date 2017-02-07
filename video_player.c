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
#include <stdlib.h>
#include <stdio.h>

/* SDL component*/
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;
SDL_Rect rect;
SDL_Event event;

#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000

typedef struct pcm_buffer{
	uint8_t *data;
	int length;
	int posWrite;
	int posRead;
	SDL_mutex *mutex;
}pcm_buffer;

/* Audio Packet queue */
typedef struct audio_pkt_queue {
	AVPacketList *first_pkt, *last_pkt;
	int num_pkt;
	int size;
	SDL_mutex *mutex;
	SDL_cond *cond;
}audio_pkt_queue;

audio_pkt_queue audio_queue;

int init_audio_pkt_queue(audio_pkt_queue* queue)
{
	memset(queue, 0x00, sizeof(queue));
	queue->mutex = SDL_CreateMutex();
	queue->cond = SDL_CreateCond();
}


int put_audio_pkt_to_queue(audio_pkt_queue* queue, AVPacket *pkt)
{
	AVPacketList *pkt_list;
	pkt_list = (AVPacketList *)av_malloc(sizeof(AVPacketList));
	if (NULL == pkt_list) {
		printf("malloc AVPacketList failed\n");
		return -1;
	}
	pkt_list->pkt = *pkt;
	pkt_list->next = NULL;
	
	SDL_LockMutex(queue->mutex);
	if (!queue->first_pkt) {
		queue->first_pkt = pkt_list;
	} else {
		if (queue->last_pkt) {
			queue->last_pkt->next = pkt_list;
		}
		queue->last_pkt = pkt_list;
	}

	queue->num_pkt++;
	queue->size += pkt->size;
	printf("put audio pkt, num %d, size %d\n", queue->num_pkt, queue->size);
	SDL_CondSignal(queue->cond);
	SDL_UnlockMutex(queue->mutex);
	return 0;
}

int get_audio_pkt_from_queue(audio_pkt_queue* queue, AVPacket *pkt)
{
	AVPacketList *pktList;
	SDL_LockMutex(queue->mutex);
	while (1) {
		if (!queue->first_pkt) {
			printf("audio pkt queue is empty, please wait!\n");
			SDL_CondWait(queue->cond, queue->mutex);
		} else {
			pktList = queue->first_pkt;
			queue->first_pkt = pktList->next;
			*pkt = pktList->pkt;
			av_free(pktList);
			queue->num_pkt--;
			queue->size -= pkt->size;
			printf("get packet success\n");
			break;
		}
	}

	SDL_UnlockMutex(queue->mutex);
	return 0;
}

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

int audio_decoder_one_frame(AVCodecContext *pAudioCodecContext, Uint8* out_buffer, int buf_size)
{
	AVFrame *pAudioFrame;
	AVPacket packet;
	
	int got_frame = 0;

	pAudioFrame = av_frame_alloc();
	int decoder_pcm_size  = 0;
	while (!decoder_pcm_size) {
		get_audio_pkt_from_queue(&audio_queue, &packet);
		
		/* decoder audio packet */
		avcodec_decode_audio4(pAudioCodecContext, pAudioFrame, &got_frame, &packet);
		if (got_frame) {
		    decoder_pcm_size = av_samples_get_buffer_size(NULL, pAudioCodecContext->channels,
		                               pAudioFrame->nb_samples,
		                               pAudioCodecContext->sample_fmt, 1);
			printf("decoder audio sucess, decoder_pcm_size %d\n", decoder_pcm_size);
			if (decoder_pcm_size > buf_size) {
				printf("[audio_decoder_one_frame], decoder pcm size is too large\n");
				continue;
			}
			memcpy(out_buffer, pAudioFrame->data[0], decoder_pcm_size);
			av_free(pAudioFrame);
			return decoder_pcm_size;
		} else {
			printf("decoder audio failed, we need more packets\n");
		}
	}
}


void audio_callback(void *opaque, Uint8 *stream, int len)
{
	printf("enter audio callback\n");
	AVCodecContext *pAudioCodecContext = opaque;
	static Uint8 pcm_buffer_decoder[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2] = {0};
	static int valid_pcm_bytes = 0;
	while (len > 0) {
		if (valid_pcm_bytes <= 0) {
			/* we should decoder something */
			valid_pcm_bytes = audio_decoder_one_frame(pAudioCodecContext, pcm_buffer_decoder, (AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2);
		}
        if (valid_pcm_bytes > 0) {
			int copy_size = len < valid_pcm_bytes ? len : valid_pcm_bytes;
			printf("[audio_callback], desired len %d, valid len %d, return len %d\n", len, valid_pcm_bytes, copy_size);
			memcpy(stream, pcm_buffer_decoder, copy_size);
			len -= copy_size;
			valid_pcm_bytes -= copy_size;
        }
	}
	return;
}

int init_SDL_play_audio(void *userdata, int format, int channels, int sample)
{
	SDL_AudioSpec wanted_spec, spec;
	wanted_spec.channels = 2;
    wanted_spec.freq = sample;
	wanted_spec.format = AUDIO_S32SYS;
	wanted_spec.silence = 0;
    wanted_spec.samples = 512;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = userdata;

	if(SDL_OpenAudio(&wanted_spec, &spec) < 0) {
		printf("open audio failed, wanted:channels %d, freq %dHz, format %d\n", 2, sample, AUDIO_S32SYS);
		printf("return:channels %d, freq %dHz, format %d\n", spec.channels, spec.freq, spec.format);
		return -1;
	} else {
		printf("open audio success, channels %d, freq %dHz, format %d\n", 2, sample, AUDIO_S32SYS);
		printf("return:channels %d, freq %dHz, format %d\n", spec.channels, spec.freq, spec.format);
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
	init_audio_pkt_queue(&audio_queue);

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

	
	init_SDL_play_audio(pAudioCodecContext, pAudioCodecContext->sample_fmt, pAudioCodecContext->channels, pAudioCodecContext->sample_rate);
	
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

	AVFrame *pVideoFrame = NULL;
	
	/* malloc space pFrame */
	pVideoFrame = av_frame_alloc();
	
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
			put_audio_pkt_to_queue(&audio_queue, &packet);
			SDL_PauseAudio(0);
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
