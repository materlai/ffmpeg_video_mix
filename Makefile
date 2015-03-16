

CC := gcc 
CXX := g++


Target_Object := ffmpeg_video_mix 

INCLUDE_PATH :=-I/usr/include/ffmpeg
LIBS_PATH :=-L/usr/lib
LIBS := -lavcodec -lavformat -lavutil -lswscale  -lSDL

All:main.o  SDL.o
	$(CXX) $(LIBS_PATH) $(LIBS)  main.o SDL.o  -o $(Target_Object)
main.o:main.cpp
	$(CXX) -c $(INCLUDE_PATH)  main.cpp -o main.o
SDL.o:CSDL.h SDL.cpp
	$(CXX) -c $(INCLUDE_PATH)  SDL.cpp -o SDL.o

clean:
	rm -rf ./main.o   $(Target_Object)
