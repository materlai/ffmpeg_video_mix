/*
   Filename: CSDL.h
   Description: SDL head file
   Author: mater_lai@163.com
 */

#include<cstdio>
#include<cstring>
#include<cstdlib>
#include<iostream>

#include<SDL/SDL.h>

typedef struct 
{
        SDL_Overlay     *bmp;
	    SDL_Surface     *screen;
	    SDL_Rect        rect;
        //unsigned int window_buffer_size;
        //unsigned char *window_buffer;
}SDL_Interface;

SDL_Interface* CreateSDLInterface(const int width,const int height);
//void SDL_Show(SDL_Interface * sdl_object);
void SDL_Show(SDL_Interface * sdl_object,unsigned char * pix_buffer[4]);



