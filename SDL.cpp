/*
  Filename:SDL.cpp
  Description: sdl to show yuv422 buffer and updateto SDL2 
  Author: mater_lai@163.com 
*/

 
#include "CSDL.h"

SDL_Interface* CreateSDLInterface(const int width,const int height)
{

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
	    std::cerr<<"Could not initialize SDL for"<<SDL_GetError()<<std::endl;
	    return NULL;
	}
       SDL_Interface * sdl_object=new SDL_Interface;
       ::memset(sdl_object,0,sizeof(SDL_Interface));
       sdl_object->screen = SDL_SetVideoMode(width, height, 0, 0);
       if(!sdl_object->screen)
       {
		  std::cerr<<"create video mode error ! "<<std::endl;
		  return NULL ;
       }
	 sdl_object->bmp = SDL_CreateYUVOverlay(width,
							 height,
							 SDL_YV12_OVERLAY,
							 sdl_object->screen);
	 //fprintf(stdout," linesize with Y =%d U=%d,V=%d\n",sdl_object->bmp->pitches[0],sdl_object->bmp->pitches[2],sdl_object->bmp->pitches[1]);
	 sdl_object->rect.x = 0;
	 sdl_object->rect.y = 0;
	 sdl_object->rect.w = width;
	 sdl_object->rect.h = height;
     return sdl_object; 
	 
}


void SDL_Show(SDL_Interface * sdl_object,unsigned char * pix_buffer[4])
{
	    SDL_LockYUVOverlay(sdl_object->bmp);		
		memcpy(sdl_object->bmp->pixels[0],pix_buffer[0],sdl_object->bmp->pitches[0]*sdl_object->rect.h);
		memcpy(sdl_object->bmp->pixels[2],pix_buffer[1],sdl_object->bmp->pitches[2]*sdl_object->rect.h/2);
		memcpy(sdl_object->bmp->pixels[1],pix_buffer[2],sdl_object->bmp->pitches[1]*sdl_object->rect.h/2);
		SDL_UnlockYUVOverlay(sdl_object->bmp);
		SDL_DisplayYUVOverlay(sdl_object->bmp, &sdl_object->rect);
}

