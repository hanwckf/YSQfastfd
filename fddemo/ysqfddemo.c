/*
  Copyright (C) 2017 Open Intelligent Machines Co.,Ltd

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  Authour : Haibing Xu <hxu@openailab.com>
*/

#include <cairo.h>
#include <gtk/gtk.h>
#include <window.h>
#include <perf.h>
#include <ysqfd.h>
#include <aaidalgr.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>

#define WINDOW_XPOS   100
#define WINDOW_YPOS   100
#define RGB_WINDOW    "RGB"

static int unlock;
static int lock_fail;
void text_osd_onwindow(cairo_t *cr, gpointer data);
void rectangle_onwindow(cairo_t *cr, gpointer data);

int algr_ysqfd_init(int imgw, int imgh);
void algr_ysqfd_exit(void);
int ysqfd_process(void *keyinfo, fcvImage *vimg, int* faceCounts);

static uint8_t *grey8rawdata = NULL;

static uint8_t *grey8mem_init(int iw, int ih)
{
	grey8rawdata = malloc(iw * ih);
	return grey8rawdata;
}
static void grey8mem_release(void)
{
	if (grey8rawdata) {
		free(grey8rawdata);
		grey8rawdata = NULL;
	}
}

void greydata_from_frame(captureCamera *frame)
{
	int w, h;

	w = frame->pixfmt->width;
	h = frame->pixfmt->height;
	convert_yuyv_to_grey8(frame->base, grey8rawdata, w, h);
}

/*
 * Only for verication of get_algconfig & set_algconfig
 *
 * change the minimum detect face size
 */
void change_face_size(int size)
{
	struct alg_config c;

	get_algconfig(AAID_ALGR_YSQ_FACEDET,&c);

	c.config.ysqfd.min_object_width = size;

	set_algconfig(AAID_ALGR_YSQ_FACEDET,&c.config);
}

void initimer(long int * timer)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	*timer = tv.tv_sec;
}

long int get_timer(long int * timer)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec - *timer);
}

void handle_unlock()
{
	unlock = 1;
	sleep (3);
}

void handle_lock()
{
	unlock = 0;
	lock_fail++;
	if (lock_fail == 5){
		lock_fail = 0;
		system("/root/alarm.sh &");
	}
}
void wait_for_humandetect()
{
	char buf[]="0";
	while (!(strcmp(buf,"0"))){
	FILE* stream = fopen("/tmp/human_status","r");
	if (!stream) printf ("error");
	fscanf(stream,"%s",buf);
	fclose(stream);
	}
}

int main(int argc, char **argv)
{
	signal(SIGUSR1, handle_unlock);
	signal(SIGUSR2, handle_lock);

	int camfd;
	int camid = 0;
	int faceCounts = 0 ;
	fcvImage *image_orig;
	captureCamera *frame;
	int vidiw, vidih;
	GError* gdk_error = NULL;
	int vaildFace = 0;
	long int timer = 0;
	initimer(&timer);		
	camfd = create_vidcapture(camid);
	if (camfd < 0) {
		assert_failure();
		return camfd;
	}
	query_vidimgsize(camid, &vidiw, &vidih);
	algr_ysqfd_init(vidiw, vidih);
	grey8mem_init(vidiw, vidih);
	named_window(RGB_WINDOW, WINDOW_NORMAL);
	resize_window(RGB_WINDOW, vidiw, vidih);
	do {
		wait_for_humandetect();
		frame = capturevid(camid);
		greydata_from_frame(frame);
		image_orig = vimage_from_frame(frame);
		ysqfd_process(grey8rawdata, image_orig,&faceCounts);
		
		imageshow(RGB_WINDOW, image_orig);
		
		//printf ("Face Counts %d \n",faceCounts);
		
		if ( faceCounts > 0 )		++vaildFace;
		
		if ( vaildFace == 5 ){
			int temp = get_timer(&timer);
			if ( temp <=3 )	{
				printf ("Face Vaild and timer satisfied ,time %d !\n",temp);
				gdk_pixbuf_save(image_orig,"/tmp/screen.jpg", "jpeg",&gdk_error,NULL);
				system("/root/face.sh &");
				//printf("Waiting for signal ");
				pause();
			}
			else
				printf ("Faces detected but timeout, time %d !\n",temp);
			vaildFace = 0;
			initimer(&timer);
		}
		//g_object_unref(image_orig);
		//ikey = waitkey(10);
		waitkey(10);
		//if (ikey == WINDOW_QUITKEY)
			//break;
	}while(1);

	destroy_vidcapture(camid);
    grey8mem_release();
	destroy_window(RGB_WINDOW);
	algr_ysqfd_exit();
	return 0;
}
