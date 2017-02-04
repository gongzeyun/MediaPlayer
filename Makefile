#Author:gongzeyun
#Date:2015.9.9
#make file for video_player

OBJECTS 	= 	video_player.o
CC		=	gcc
LIBS		=	avutil -lavformat -lavcodec -lz -lavutil -lm -lpthread -lswresample -lrt -lswscale -lSDL2
MODULE		= 	video_player
LD		=	ld

all:$(MODULE).out

$(MODULE).out:$(OBJECTS) 
	$(CC) -o $(MODULE).out $(OBJECTS) -l$(LIBS)

:$(OBJECTS):
	$(CC) video_player.c