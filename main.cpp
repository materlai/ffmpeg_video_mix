/*
   Filename : main.cpp
   Description: use  ffmpeg to decode two way(side by side) video and encoder to one file(H264 default)
   Author : mater_lai@163.com   
 */


#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<iostream>
#include<fstream>
#include<assert.h>
#include<unistd.h>

/*
  ffmpeg head file 
 */
extern "C"
{
 #include<libavcodec/avcodec.h>
 #include<libavutil/avutil.h>
 #include<libavformat/avformat.h>
 #include<libavformat/avio.h>
 #include<libavutil/imgutils.h>
 #include<libavutil/opt.h>
 #include<libswscale/swscale.h>	
 #include <libavutil/avassert.h>
 #include <libavutil/channel_layout.h>
 #include <libavutil/mathematics.h>
 #include <libavutil/timestamp.h>
 #include <libswresample/swresample.h>
	
}

/*
     CSDL head file 
 */
#include "CSDL.h"

typedef struct
{
	AVFormatContext *fmt_ctx ;
    AVStream *video_stream ;
    int vide_stream_index;
	AVCodecContext *video_dec_ctx;	
    AVCodec *dec ;
	
	char src_filename [50];
	int frame_width;
	int frame_height;
	/*decoder buffer */
   uint8_t* video_dst_data[4];
   int      video_dst_linesize[4];
   int video_dst_bufsize;
   AVFrame *frame ;
   AVPacket pkt;
   AVPacket orgin_pkt;
	//FILE * des_file_handle;	
	
}Raspi_Video_Decoder;

typedef struct
{
    AVCodec *video_codec;
    AVCodecContext *video_codec_context;
		
	int frame_width;
	int frame_height;
    int framerate;
		 
	AVFrame *frame;
    //AVPacket pkt;
    int  frame_buffer_size;	
   	
}Raspi_Media_Encoder;

typedef struct 
{
    AVStream * video_stream;
	AVCodec * video_codec ;
	AVCodecContext * video_context ;
	int stream_index;
	
}OutputStream;

typedef  struct
{
	AVFormatContext  *  media_formatcontext;
	char format_name[50];
    AVOutputFormat *output_fmt;
	
	OutputStream * video_stream;
	OutputStream * audio_stream;
    
	Raspi_Media_Encoder *video_encoder;
	Raspi_Media_Encoder * audio_encoder;
}Raspi_Muxer;


#define STREAM_FRAME_RATE  25

Raspi_Muxer * CreateMeidaFormatContext(const char * filename,enum AVCodecID vodec_id)
{
	 Raspi_Muxer * format_muxer=NULL;
	 if(!filename)  return format_muxer;
	 format_muxer=(Raspi_Muxer*)malloc(sizeof(Raspi_Muxer));
	 memset(format_muxer,0,sizeof(Raspi_Muxer));
	 if(avformat_alloc_output_context2(&format_muxer->media_formatcontext,NULL,NULL,filename))
	 {
		 fprintf(stderr,"can not decuce output format from file extension name!,try mkv now !\n");
         avformat_alloc_output_context2(&format_muxer->media_formatcontext,NULL,"mpeg",NULL);		 
	 }
	 if(!format_muxer->media_formatcontext)
	 {
		 fprintf(stderr,"failed to create format context!\n");
		 free(format_muxer);
		 format_muxer=NULL;
		 return format_muxer;
	 }
	 fprintf(stdout,"create format context finished!\n");
	 format_muxer->output_fmt=format_muxer->media_formatcontext->oformat;
	 memcpy(format_muxer->format_name,format_muxer->output_fmt->name,strlen(format_muxer->output_fmt->name));
	 //if(format_muxer->output_fmt->video_codec!=AV_CODEC_ID_NONE)
	 fprintf(stdout,"format context video stream codec id=%s\n",avcodec_get_name(format_muxer->output_fmt->video_codec));
 	 fprintf(stdout,"format context audio stream codec id=%s\n",avcodec_get_name(format_muxer->output_fmt->audio_codec));
     return format_muxer;	 
}

//add video stream to format context 
int AllocStreamAndEncoder(Raspi_Muxer * muxer,AVMediaType type)
{
	if(!muxer)   return -1;
    if(type==AVMEDIA_TYPE_VIDEO)
	{
		muxer->video_stream =(OutputStream*)malloc(sizeof(OutputStream));
		memset(muxer->video_stream,0,sizeof(OutputStream));
		muxer->video_stream->video_codec=avcodec_find_encoder(muxer->output_fmt->video_codec);
		if(!muxer->video_stream->video_codec)
		{
			fprintf(stderr,"can not find %s encoder!\n",avcodec_get_name(muxer->output_fmt->video_codec));
			free(muxer->video_stream);
			muxer->video_stream=NULL;
			return -2;			
		}
		muxer->video_stream->video_stream=avformat_new_stream(muxer->media_formatcontext,muxer->video_stream->video_codec);
		if(!muxer->video_stream->video_stream)
		{
			fprintf(stderr,"can not add video stream to format context !\n");
	        free(muxer->video_stream);
	        muxer->video_stream=NULL;
			return -3;			
		}
		muxer->video_stream->video_stream->time_base= (AVRational){ 1, STREAM_FRAME_RATE };
	    muxer->video_stream->video_stream->id=0;
		muxer->video_stream->stream_index=0;
		muxer->video_stream->video_context=muxer->video_stream->video_stream->codec;
		assert(muxer->video_stream->video_context);
		
		//step 2 : alloc encoder
		muxer->video_encoder=(Raspi_Media_Encoder*)malloc(sizeof(Raspi_Media_Encoder));
		memset(muxer->video_encoder,0,sizeof(Raspi_Media_Encoder));
		muxer->video_encoder->video_codec=muxer->video_stream->video_codec;
		muxer->video_encoder->video_codec_context=muxer->video_stream->video_context;
		if(muxer->output_fmt->flags &AVFMT_GLOBALHEADER)
			muxer->video_encoder->video_codec_context->flags |=CODEC_FLAG_GLOBAL_HEADER;
	}
	else {}  //audio stream 
	return 0;	
}


/*
static int write_frame(AVFormatContext *fmt_ctx, AVRational *time_base, AVStream *st, AVPacket *pkt)
{
   
	// av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = 0;  
    //log_packet(fmt_ctx, pkt);
    return av_interleaved_write_frame(fmt_ctx, pkt);
	}*/



int  Raspi_Alloc_destination_buffer(Raspi_Video_Decoder* decoder)
{
	int ret=0;;
	if( (ret=av_image_alloc(decoder->video_dst_data, decoder->video_dst_linesize,
                             decoder->video_dec_ctx->width, decoder->video_dec_ctx->height,
		decoder->video_dec_ctx->pix_fmt, 1))<0 ||  (decoder->frame=avcodec_alloc_frame())==NULL)
  	
	{
    	fprintf(stderr,"failed to alloc yuv width=%d,height=%d buffer for left codec context \n",decoder->video_dec_ctx->width,
		decoder->video_dec_ctx->height);
		ret = AVERROR(ENOMEM);
	    return ret;		
	}
	decoder->video_dst_bufsize=ret;
	av_init_packet(&decoder->pkt);
    decoder->pkt.data = NULL;
    decoder->pkt.size = 0;
    return ret;	
}

	
int ReadMediaVideoInfo(Raspi_Video_Decoder * decoder,const char * filename,enum AVMediaType type)
{
    if(!decoder || !filename || strlen(filename)<=0)   return -1;
	int ret=0;
    /* open input file, and allocate format context */
	memset(decoder->src_filename,0,sizeof(decoder->src_filename));
	strncpy(decoder->src_filename,filename,strlen(filename));
    if (avformat_open_input(&decoder->fmt_ctx,decoder->src_filename,NULL, NULL) < 0)
	{
        fprintf(stderr, "Could not open source file %s\n", filename);
        return -1;
    }
    /* retrieve stream information */
    if (avformat_find_stream_info(decoder->fmt_ctx, NULL) < 0)
	{
        fprintf(stderr, "Could not find stream information\n");
     	ret=-2;
	    goto error;
    }
    ret = av_find_best_stream(decoder->fmt_ctx, type, -1, -1, NULL, 0);
	if(ret<0)
	{
		fprintf(stderr,"failed to find stream index with type=%d\n",type);
	    ret=-3;
	    
	}
	else
	{
        decoder->vide_stream_index=ret;         
        decoder->video_stream = decoder->fmt_ctx->streams[ret];
		assert(decoder->video_stream);
		decoder->video_dec_ctx=decoder->video_stream->codec;
        decoder->frame_width=decoder->video_dec_ctx->width;
		decoder->frame_height=decoder->video_dec_ctx->height;
		
		decoder->dec=avcodec_find_decoder(decoder->video_dec_ctx->codec_id);
		if (!decoder->dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            ret= AVERROR(EINVAL);
	        goto error;
        }
	   if ((ret = avcodec_open2(decoder->video_dec_ctx, decoder->dec, NULL)) < 0)
	   {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
	        ret=-4;
	        goto error;
	   }
	   return 0;
	}
error:
	if(decoder->video_dec_ctx && avcodec_is_open(decoder->video_dec_ctx))  {avcodec_close(decoder->video_dec_ctx);}
	if(decoder->fmt_ctx) avformat_close_input(&decoder->fmt_ctx);
	return ret;
}


void DumpAVFrameInfo(AVFrame* frame)
{
    if(!frame)  return ;
	fprintf(stdout,"\nframe pts=%ld\n",frame->pts);
	fprintf(stdout,"frame pkt_pts=%ld\n",frame->pkt_pts);
	fprintf(stdout,"frame pkt_dts=%ld\n",frame->pkt_dts);
	fprintf(stdout,"frame pts_duration=%ld\n",frame->pkt_duration);

}

int Raspi_decoder_package(Raspi_Video_Decoder* decoder,AVPacket * package,int * got_frame)
{
	//static int decoder_frame_index=0;
	if(!decoder || !decoder->video_dec_ctx || !decoder->dec)  return -1;
	if(package->stream_index!=decoder->vide_stream_index)
	{
		fprintf(stderr,"frame package is noe video package!\n");
		return -1;		
	}  
   int ret= avcodec_decode_video2(decoder->video_dec_ctx,decoder->frame, got_frame,package);
   if (ret < 0) {
	   fprintf(stderr, "Error decoding video frame \n");// av_err2str(ret));
            //return ret;
   }
   if (*got_frame) {

	      
            /* copy decoded frame to destination buffer:
             * this is required since rawvideo expects non aligned data */
            av_image_copy(decoder->video_dst_data, decoder->video_dst_linesize,
                          (const uint8_t **)(decoder->frame->data), decoder->frame->linesize,
                          decoder->video_dec_ctx->pix_fmt, decoder->video_dec_ctx->width,
						  decoder->video_dec_ctx->height);
	        //fwrite(decoder->video_dst_data[0],1,decoder->video_dst_bufsize,decoder->des_file_handle);
	        fprintf(stdout,"RAW YUV buffer is decoded!\n");
   }
   return ret;  
	
}

int ReadPacketFromFile(Raspi_Video_Decoder* decoder)
{
	int ret=av_read_frame(decoder->fmt_ctx,&decoder->pkt);
    decoder->orgin_pkt=decoder->pkt;	
	return ret;
}
	
// read file to decode a frame unit file is read finished ! 	
int Raspi_Queue_Frame(Raspi_Video_Decoder * decoder)
{
	int got_frame=0;
    if(!decoder)  return -1;
	do
	{
	   while(decoder->pkt.size>0)
	   {
            int ret = Raspi_decoder_package(decoder,&decoder->pkt,&got_frame);
	        if (ret < 0)  { got_frame=-1;  break;}
            decoder->pkt.data += ret;
            decoder->pkt.size -= ret;
	        if(got_frame)  break;
       }
	 if(decoder->pkt.size==0 && decoder->orgin_pkt.size>0)
	 {
	    av_free_packet(&decoder->orgin_pkt);
	    decoder->orgin_pkt.size=0;
	    decoder->orgin_pkt.data=NULL;
     }
   }while( got_frame==0  && ReadPacketFromFile(decoder) >= 0);
    // if file is read finished ,then fflush media to get last frame
	if(got_frame==0)
	{
       fprintf(stdout,"read file is finished ,wait for last frame !\n");
	   decoder->pkt.size=0;
	   decoder->pkt.data=NULL;
       int ret= Raspi_decoder_package(decoder,&decoder->pkt,&got_frame);
	   if(ret<0)   got_frame=-1;
    }
    return got_frame;  		
}


void CloseFFmpegDecoder(Raspi_Video_Decoder * decoder)
{
    if(!decoder)  return ;
    if(decoder->video_dec_ctx)    avcodec_close(decoder->video_dec_ctx);
    if(decoder->fmt_ctx)    avformat_close_input(&decoder->fmt_ctx);
	if(decoder->video_dst_data[0]) av_free(decoder->video_dst_data[0]);
    if(decoder->frame)  avcodec_free_frame(&decoder->frame);
	free(decoder);	 
}

	
int Raspi_video_Encoder_Settting(Raspi_Media_Encoder * encoder, const int width,const int height,int framerate)
{
     
    if(!encoder)  return -1;
	encoder->frame_width=width;
	encoder->frame_height=height;
	encoder->framerate=framerate;

	AVCodecContext * video_codec_context=encoder->video_codec_context;
	assert(video_codec_context);
    /* put sample parameters */
    video_codec_context->bit_rate = 18000000;
	video_codec_context->codec_type=AVMEDIA_TYPE_VIDEO;
    /* resolution must be a multiple of two */
    video_codec_context->width=encoder->frame_width;
    video_codec_context->height=encoder->frame_height;
    /* frames per second */
	//encoder->video_codec_context->time_base.num=1;
	//encoder->video_codec_context->time_base.den=encoder->framerate; 
    video_codec_context->time_base=(AVRational){1,encoder->framerate};
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    video_codec_context->gop_size = framerate;;
    video_codec_context->max_b_frames = 0; //when has b frames , saw raw h264 buffer to mkv file will fail 
	video_codec_context->thread_count=2;
    video_codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
    video_codec_context->max_qdiff = 4;
    video_codec_context->qcompress=0.6;
	
    // encode quality setttings
	if(video_codec_context->codec_id==AV_CODEC_ID_H264)
	{
		fprintf(stdout,"video context codec id=%s\n",avcodec_get_name(AV_CODEC_ID_H264));	
       av_opt_set(video_codec_context->priv_data, "preset", "slow", 0);
	   encoder->video_codec_context->qmin=20;
	   encoder->video_codec_context->qmax=30;
       //encode compability setting
	   encoder->video_codec_context->profile=FF_PROFILE_H264_HIGH;
	   encoder->video_codec_context->level=40;
	}
    
  /*    
    //运动估计
   encoder->video_codec_context->pre_me = 2;

  //设置最小和最大拉格朗日乘数
  //拉格朗日乘数 是统计学用来检测瞬间平均值的一种方法
  encoder->video_codec_context->lmin = 1;
  encoder->video_codec_context->lmax = 5;

  //因为我们的量化系数q是在qmin和qmax之间浮动的，
  //qblur表示这种浮动变化的变化程度，取值范围0.0～1.0，取0表示不削减
  encoder->video_codec_context->qblur = 0.0;

  //空间复杂度的masking力度，取值范围 0.0-1.0
  encoder->video_codec_context->spatial_cplx_masking = 0.3;

  //运动场景预判功能的力度，数值越大编码时间越长
  encoder->video_codec_context->me_pre_cmp = 2;

   //采用（qmin/qmax的比值来控制码率，1表示局部采用此方法，）
  encoder->video_codec_context->rc_qsquish = 1;

  //设置 i帧、p帧与B帧之间的量化系数q比例因子，这个值越大，B帧越不清楚
  //B帧量化系数 = 前一个P帧的量化系数q * b_quant_factor + b_quant_offset
  encoder->video_codec_context->b_quant_factor = 1.25;

   //i帧、p帧与B帧的量化系数便宜量，便宜越大，B帧越不清楚
   encoder->video_codec_context->b_quant_offset = 1.25;

   //p和i的量化系数比例因子，越接近1，P帧越清楚
   //p的量化系数 = I帧的量化系数 * i_quant_factor + i_quant_offset
   encoder->video_codec_context->i_quant_factor = 0.8;
   encoder->video_codec_context->i_quant_offset = 0.0;

   //码率控制测率，宏定义，查API
  encoder->video_codec_context->rc_strategy = 2;

  //b帧的生成策略
  encoder->video_codec_context->b_frame_strategy = 0;

   //消除亮度和色度门限
   //encoder->video_codec_context->luma_elim_threshold = 0;
   //encoder->video_codec_context->chroma_elim_threshold = 0;

   //DCT变换算法的设置，有7种设置，这个算法的设置是根据不同的CPU指令集来优化的取值范围在0-7之间
   encoder->video_codec_context->dct_algo = 0;
   //这两个参数表示对过亮或过暗的场景作masking的力度，0表示不作
   encoder->video_codec_context->lumi_masking = 0.0;
   encoder->video_codec_context->dark_masking = 0.0;
*/
	
    /* open it */
    if (avcodec_open2(encoder->video_codec_context, encoder->video_codec, NULL) < 0)
	{
        fprintf(stderr, "Could not open codec\n");
	    return -3;
    }
    encoder->frame = av_frame_alloc();
    if (!encoder->frame)
	{
        fprintf(stderr, "Could not allocate video frame\n");
	    return -3;
    }
    encoder->frame->format = encoder->video_codec_context->pix_fmt;
    encoder->frame->width  = encoder->video_codec_context->width;
    encoder->frame->height = encoder->video_codec_context->height;
	encoder->frame->pts=0;
    /* the image can be allocated by any means and av_image_alloc() is
     * just the most convenient way if av_malloc() is to be used */
    int ret = av_image_alloc(encoder->frame->data, encoder->frame->linesize, encoder->video_codec_context->width, encoder->video_codec_context->height,
                         encoder->video_codec_context->pix_fmt, 32);
    if(ret<0)
	{
	  fprintf(stderr,"failed to alloc encoder frame yuv buffer!\n");
	  return -4;
    }
	encoder->frame_buffer_size=ret;
	return 0;
      	
}

	
int main(int argc,char **argv)
{
    fprintf(stdout,"ffmpeg demo program to read two way video(side by side) then decoder to yuv raw video(one as left yuv buffer,one as right yuv buffer, \
            after that use H264 encoder to  encode mix yuv buffer to  file!\n");
	int ret=0;
	int got_frame=0;
	Raspi_Video_Decoder * decoder_left=NULL;
	Raspi_Video_Decoder * decoder_right=NULL;

    Raspi_Muxer * muxer=NULL;
	FILE *des_file_handle=NULL;
    uint8_t* video_dst_data[4]={NULL};
    int    video_dst_linesize[4]={0};
	int frame_buffer_size=0;
	//uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    SDL_Interface * sdl_object=NULL;
	Raspi_Media_Encoder * encoder=NULL;
	const char * filename=NULL;
    
	if(argc<4)
	{
		fprintf(stderr,"argument is missing,try ./ffmpeg_video_mix left_video_file right_video_file  mix_file!\n");
		return -1;		
	}
    filename=argv[3];
    //step 1 : register ffmpeg codec & alloc format context 
	av_register_all();
	//step 2 : read  vide stream info & dumex it
    decoder_left= (Raspi_Video_Decoder*)malloc(sizeof(Raspi_Video_Decoder));
	memset(decoder_left,0,sizeof(Raspi_Video_Decoder));
	if(ReadMediaVideoInfo(decoder_left,argv[2],AVMEDIA_TYPE_VIDEO)<0)   goto exit;
    decoder_right=(Raspi_Video_Decoder*) malloc(sizeof(Raspi_Video_Decoder));
	memset(decoder_right,0,sizeof(Raspi_Video_Decoder));
	if(ReadMediaVideoInfo(decoder_right,argv[1],AVMEDIA_TYPE_VIDEO)<0)  goto exit;
    /* step 3 :  dump input information to stderr */
    av_dump_format(decoder_left->fmt_ctx, 0, decoder_left->src_filename, 0);
    av_dump_format(decoder_right->fmt_ctx, 0, decoder_right->src_filename, 0);
    // alloc decoder destination buffer 
	if( Raspi_Alloc_destination_buffer(decoder_left)<0 || Raspi_Alloc_destination_buffer(decoder_right)<0)
		goto exit;
    
    if(strstr(filename,".yuv")!=NULL)
	{
	 sdl_object=CreateSDLInterface(decoder_left->frame_width+decoder_right->frame_width,decoder_left->frame_height+decoder_right->frame_height);
     if(!sdl_object)
     {
	   fprintf(stdout,"failed to create sdl object !\n");
	   goto  exit;	   
     }
	 des_file_handle=fopen(filename,"wb");
	 if( (frame_buffer_size= av_image_alloc(video_dst_data, video_dst_linesize, decoder_left->frame_width+decoder_right->frame_width,decoder_left->frame_height+decoder_right->frame_height,
											decoder_left->video_dec_ctx->pix_fmt, 32))<0)
	 {
		 fprintf(stderr,"can not alloc yuv  destination buffer!\n ");
		 goto exit;		 
	 }  
	 	
	}
    else
	{
	  //step 3 : open muxer for write mkv file 
      muxer=CreateMeidaFormatContext(argv[3],AV_CODEC_ID_H264);
	  if(!muxer)  goto exit;
	  if(AllocStreamAndEncoder(muxer,AVMEDIA_TYPE_VIDEO)<0)
	  {
		fprintf(stderr,"failed to alloc video stream & video encoder !\n");
		goto end ;
	  }
	  av_dump_format(muxer->media_formatcontext, 0,filename, 1);
      //step 4 : encoder settings 
      encoder=muxer->video_encoder;
	  Raspi_video_Encoder_Settting(encoder,decoder_left->frame_width+decoder_right->frame_width,decoder_left->frame_height+decoder_right->frame_height,STREAM_FRAME_RATE);		       		
	  //step 5 : open target filename for format frame write
	  /* open the output file, if needed */
      if (!(muxer->output_fmt->flags & AVFMT_NOFILE))
	  {
    	ret = avio_open(&muxer->media_formatcontext->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open '%s\n", filename);
            goto end;
        }
		fprintf(stdout,"AVIOContext is create finished,wait for frame write!\n");
      }
	  //write format head 
	  ret = avformat_write_header(muxer->media_formatcontext, NULL);
      if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
		goto end;
      }
	memcpy(video_dst_data,encoder->frame->data,sizeof(video_dst_data));
    memcpy(video_dst_linesize,encoder->frame->linesize,sizeof(video_dst_linesize));
   }
	
    //step 5 :  decode  & mix left right image ,then encode to h264 raw or directly write yuv buffer to file  	   
   while(1)
	{
    	int ret_left=Raspi_Queue_Frame(decoder_left);
	    int ret_right=Raspi_Queue_Frame(decoder_right);
	    if(ret_left && ret_right)
	    {

			//fprintf(stdout,"start mix left & right buffer!\n");
	         // arrange yuv420p raw buffer: mix left buffer and right buffer to final yuv420p buffer & save it to file
				
			for( int scan_index=0;scan_index<decoder_left->frame_height+decoder_right->frame_height;scan_index++)
	   	    {
				
			    int des_buffer_y_index=scan_index*video_dst_linesize[0];
			    int des_buffer_u_index= scan_index*video_dst_linesize[1]/2;
			    int des_buffer_v_index=scan_index*video_dst_linesize[2]/2;
			   
			   if(scan_index< decoder_left->frame_height/2 || scan_index>=(decoder_left->frame_height+decoder_right->frame_height/2))
			   {

				    //fprintf(stdout,"scan index=%d\n",scan_index); 
				    memset(video_dst_data[0]+des_buffer_y_index,0,video_dst_linesize[0]);
					if(scan_index%2!=0)  continue;
					memset(video_dst_data[1]+des_buffer_u_index,128,video_dst_linesize[1]);
					memset(video_dst_data[2]+des_buffer_v_index,128,video_dst_linesize[2]);				
					}
			   else
			    {
			      	 
				 int index=scan_index-decoder_left->frame_height/2;
	             //copy  left Y buffer   
	             memcpy(video_dst_data[0]+des_buffer_y_index, decoder_left->video_dst_data[0]+index*decoder_left->video_dst_linesize[0],decoder_left->video_dst_linesize[0]);
	             //copy right  Y buffer
	             memcpy(video_dst_data[0]+des_buffer_y_index+decoder_left->video_dst_linesize[0],
													  decoder_right->video_dst_data[0]+index*decoder_right->video_dst_linesize[0],decoder_right->video_dst_linesize[0]);
                
	            //copy  U & V from left
	            if(scan_index%2!=0)  continue;
	            //int U_base_index=scan_index*video_dst_linesize[1]/2;
	            //int V_base_index=scan_index*video_dst_linesize[2]/2;
	            memcpy(video_dst_data[1]+des_buffer_u_index,decoder_left->video_dst_data[1]+index*decoder_left->video_dst_linesize[1]/2,decoder_left->video_dst_linesize[1]);
	            memcpy(video_dst_data[2]+des_buffer_v_index,decoder_left->video_dst_data[2]+index*decoder_left->video_dst_linesize[2]/2,decoder_left->video_dst_linesize[2]);
	            //copy U & V from right
	            memcpy(video_dst_data[1]+des_buffer_u_index+decoder_left->video_dst_linesize[1],decoder_right->video_dst_data[1]+index*decoder_right->video_dst_linesize[1]/2,
		             decoder_right->video_dst_linesize[1]);
	            memcpy(video_dst_data[2]+des_buffer_v_index+decoder_left->video_dst_linesize[2],decoder_right->video_dst_data[2]+index*decoder_right->video_dst_linesize[2]/2,
		             decoder_right->video_dst_linesize[2]);
			    }
			 }
			
	        // if(strstr(argv[3],".yuv")!=NULL && des_file_handle)
			if(strstr(argv[3],".yuv")!=NULL)
			 {
				 /*
				 unsigned char * sdl_buffer_data[4];
				 int sdl_buffer_linesize[4];
				 sdl_buffer_data[0]=sdl_object->bmp->pixels[0];
				 sdl_buffer_data[1]=sdl_object->bmp->pixels[2];
				 sdl_buffer_data[2]=sdl_object->bmp->pixels[1];
				 sdl_buffer_linesize[0]=sdl_object->bmp->pitches[0];
				 sdl_buffer_linesize[1]=sdl_object->bmp->pitches[2];
				 sdl_buffer_linesize[2]=sdl_object->bmp->pitches[1];
                
                 SDL_LockYUVOverlay(sdl_object->bmp);
                
				 memcpy(sdl_buffer_data[0],video_dst_data[0],video_dst_linesize[0]*encoder->frame->height);
                 memcpy(sdl_buffer_data[1],video_dst_data[1],video_dst_linesize[1]*encoder->frame->height/2);
				 memcpy(sdl_buffer_data[2],video_dst_data[2],video_dst_linesize[2]*encoder->frame->height/2);

				 SDL_UnlockYUVOverlay(sdl_object->bmp); 
				 */
				 
				 SDL_Show(sdl_object,video_dst_data);
				 if(des_file_handle)  fwrite(video_dst_data[0],1,frame_buffer_size,des_file_handle);
				 usleep(40*1000);
			 }
		     else
		     {
	          // h264 encoder  &  save  to file
              //fwrite(video_dst_data[0],1,video_dst_buffer_size,des_file_handle);
	          AVPacket pkt;
	          av_init_packet(&pkt);
	          pkt.data=NULL;
	          pkt.size=0;
	          int got_output=0;
			  //encoder->frame->pts++;
	          ret = avcodec_encode_video2(encoder->video_codec_context, &pkt,encoder->frame, &got_output);
	          if(ret<0)  break;
	          if(got_output)
	          {
				pkt.stream_index=muxer->video_stream->video_stream->index;
				av_interleaved_write_frame(muxer->media_formatcontext, &pkt);
	            av_free_packet(&pkt);
              }
			  //fprintf(stdout,"AVStream time base den= %d,num=%d\n",muxer->video_stream->video_stream->time_base.den,muxer->video_stream->video_stream->time_base.num);
			  encoder->frame->pts+= av_rescale_q(1, encoder->video_codec_context->time_base, muxer->video_stream->video_stream->time_base);
			  
            }
          } 
		else break;
   }
   // got last encoder buffer
   if(strstr(argv[3],".yuv")==NULL)
   {
     do
     {
	       int got_output=0;
	       AVPacket pkt;
	       av_init_packet(&pkt);
	       pkt.data=NULL;
	       pkt.size=0;
    	   int ret = avcodec_encode_video2(encoder->video_codec_context, &pkt, NULL, &got_output);
	       if(ret<0 || ! got_output)  break;
	       //fwrite(pkt.data,1,pkt.size,des_file_handle);
           //write_frame(muxer->media_formatcontext,&encoder->video_codec_context->time_base,muxer->video_stream->video_stream,&pkt);
		   pkt.stream_index=muxer->video_stream->video_stream->index;
		   av_interleaved_write_frame(muxer->media_formatcontext, &pkt);
				 
	       av_free_packet(&pkt);
     }while(1);
    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(muxer->media_formatcontext);
   // fwrite(endcode,1,sizeof(endcode),des_file_handle);
  }
  else
  {
    if(des_file_handle)
      fflush(des_file_handle);
 }
   
end:
   if(muxer)
   {
	   if(muxer->video_encoder)
	   {
		   if(muxer->video_encoder->video_codec_context && avcodec_is_open( muxer->video_encoder->video_codec_context))
		       avcodec_close(muxer->video_encoder->video_codec_context);
		   if(muxer->video_encoder->frame)
		   {
			   av_freep(muxer->video_encoder->frame->data[0]);
			    avcodec_free_frame(&muxer->video_encoder->frame);
		   }		    		   		   
	   }                         
	   if(muxer->media_formatcontext->pb)
		   avio_close(muxer->media_formatcontext->pb);	   
   	 if(muxer->media_formatcontext)
	   avformat_free_context(muxer->media_formatcontext);
   }
exit:
    if(decoder_left)     CloseFFmpegDecoder(decoder_left);
    if(decoder_right) 	 CloseFFmpegDecoder(decoder_right);
    if(des_file_handle)     
	   fclose(des_file_handle);
	return ret;
}

