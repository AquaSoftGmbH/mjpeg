#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "forms.h"
#include "xlav.h"

#define PLAY_PROG "lavplay"
#define LAVPLAY_VSTR "lavplay" LAVPLAY_VERSION  /* Expected version info */

int verbose = 1;
static FD_xlav *fd_xlav;

static int inp_pipe;
static int out_pipe;

static int pid;

static int norm, cur_pos, total_frames, cur_speed=1, old_speed=999999;
static int slider_pause = 0;

static int ff_stat=0, fr_stat=0;
static int ff_speed[4] = { 1, 3, 10, 30 };

static int selection_start = -1;
static int selection_end = -1;

#define MAXINP 4096

static char inpbuff[MAXINP];
static int inplen = 0;

static char timecode[64];

static void calc_timecode(int pos, int do_frames)
{
   int h, m, s, f;

   /* Calculate hours:min:sec:frames */

   if(norm=='n')
   {
      f = cur_pos%30;
      s = cur_pos/30;
   }
   else
   {
      f = cur_pos%25;
      s = cur_pos/25;
   }
   m = s/60;
   s = s%60;
   h = m/60;
   m = m%60;
   if (!do_frames) f=0;
   sprintf(timecode,"%2d:%2.2d:%2.2d:%2.2d",h,m,s,f);
}

void dispatch_input()
{
	char normc;
   /* A line starting with '-' should be ignored */

   if(inpbuff[0]=='-') return;

   /* A line starting with '@' contains psoition information */

   if(inpbuff[0]=='@')
   {
      sscanf(inpbuff+1,"%c%d/%d/%d",&normc,&cur_pos,&total_frames,&cur_speed);
	  norm = normc;
      calc_timecode(cur_pos,cur_speed==0);
      fl_set_object_label(fd_xlav->Timer,timecode);
      if(total_frames<1) total_frames=1;
      if(slider_pause)
         slider_pause--;
      else
         fl_set_slider_value(fd_xlav->timeslider,(double)cur_pos/(double)total_frames);
      if(cur_speed != old_speed)
      {
         char label[32];

         if(cur_speed == 1) {
            fl_set_object_label(fd_xlav->StatDisp,"Play >");
         } else if(cur_speed == 0) {
            fl_set_object_label(fd_xlav->StatDisp,"Pause");
         } else if(cur_speed == -1) {
            fl_set_object_label(fd_xlav->StatDisp,"Play <");
         } else if(cur_speed < -1) {
            sprintf(label,"<<%2dx",-cur_speed);
            fl_set_object_label(fd_xlav->StatDisp,label);
         } else if(cur_speed > 1) {
            sprintf(label,">>%2dx",cur_speed);
            fl_set_object_label(fd_xlav->StatDisp,label);
         }
         old_speed = cur_speed;
      }
      return;
   }

   /* Default: some print output from lavplay */

   printf("+++ %s",inpbuff);
   if(inpbuff[inplen-1]!='\n') printf("<<<\n");
}

void get_input(int fd, void *data)
{
   char input[4096];
   int i, n;

   n = read(fd,input,4096);
   if(n==0) exit(0);

   for(i=0;i<n;i++)
   {
      if(inplen<MAXINP-1) inpbuff[inplen++] = input[i];
      if(input[i]=='\n')
      {
         inpbuff[inplen] = 0;
         dispatch_input();
         inplen = 0;
      }
   }
}

void timeslider_cb(FL_OBJECT *ob, long data)
{
   double val;
   char out[256];

   val = fl_get_slider_value(fd_xlav->timeslider);
   sprintf(out,"s%ld\n",(long)(val*total_frames));
   write(out_pipe,out,strlen(out));
   slider_pause = 8;
}

void button_cb(FL_OBJECT *ob, long data)
{
   switch(data)
   {
      case 1: write(out_pipe,"s0\n",3); break;
      case 2: write(out_pipe,"s10000000\n",10); break;
      case 3: write(out_pipe,"-\n",2); break;
      case 4: write(out_pipe,"+\n",2); break;
   }
}

void rb_cb(FL_OBJECT *ob, long data)
{
   char out[32];

   if (data!=1) fr_stat = 0;
   if (data!=5) ff_stat = 0;

   switch(data)
   {
      case 1:
         fr_stat++;
         if(fr_stat>3) fr_stat=1;
         sprintf(out,"p-%d\n",ff_speed[fr_stat]);
         write(out_pipe,out,strlen(out));
         break;
      case 2: write(out_pipe,"p-1\n",4); break;
      case 3: write(out_pipe,"p0\n",3); break;
      case 4: write(out_pipe,"p1\n",3); break;
      case 5:
         ff_stat++;
         if(ff_stat>3) ff_stat=1;
         sprintf(out,"p%d\n",ff_speed[ff_stat]);
         write(out_pipe,out,strlen(out));
         break;
   }
}

#if 0 /* No Audio mute at the moment */
void Audio_cb(FL_OBJECT *ob, long data)
{
   if(fl_get_button(fd_xlav->Audio))
      write(out_pipe,"a1\n",3);
   else
      write(out_pipe,"a0\n",3);
}
#endif

void do_real_exit(int ID, void *data)
{
   int status;

   /* Kill all our children and exit */

   kill(0,9);
   waitpid(pid,&status,0);
   exit(0);
}

void Exit_cb(FL_OBJECT *ob, long data)
{
   /* Try to exit gracefully, wait 1 second, do real exit */

   write(out_pipe,"q\n\n\n",4);
   printf("Exit: Waiting 1 second for childs\n");
   fl_add_timeout(1000, do_real_exit, 0);
}

void signal_cb(int signum, void *data)
{
   printf("Got signal %d\n",signum);
   Exit_cb(0,0);
}

static int check_selection()
{
   if(selection_start>=0 && selection_end>=selection_start) return 0;
   fl_show_alert("Selection invalid!!!","","",1);
   return -1;
}


void selection_cb(FL_OBJECT *ob, long data)
{
   char str[256];
   const char *name;

   switch(data)
   {
      case 1:
         selection_start = cur_pos;
         calc_timecode(cur_pos,1);
         fl_set_object_label(fd_xlav->FSelStart,timecode);
         break;
      case 2:
         selection_end   = cur_pos;
         calc_timecode(cur_pos,1);
         fl_set_object_label(fd_xlav->FSelEnd  ,timecode);
         break;
      case 3:
         selection_start = -1;
         selection_end   = -1;
         fl_set_object_label(fd_xlav->FSelStart,"");
         fl_set_object_label(fd_xlav->FSelEnd  ,"");
         break;
      case 4:
      case 5:
         if(check_selection()) return;
         sprintf(str,"e%c %d %d\n",(data==4)?'u':'o',selection_start,selection_end);
         write(out_pipe,str,strlen(str));
         if(data==4)
         {
            selection_start = -1;
            selection_end   = -1;
            fl_set_object_label(fd_xlav->FSelStart,"");
            fl_set_object_label(fd_xlav->FSelEnd  ,"");
         }
         break;
      case 6:
         selection_start = -1;
         selection_end   = -1;
         fl_set_object_label(fd_xlav->FSelStart,"");
         fl_set_object_label(fd_xlav->FSelEnd  ,"");
         write(out_pipe,"ep\n",3);
         break;
      case 7:
         name = fl_show_fselector("Enter name of Edit List File:","","*","");
         if(name==0) return;
         sprintf(str,"wa %s\n",name);
         write(out_pipe,str,strlen(str));
         break;
      case 8:
         if(check_selection()) return;
         name = fl_show_fselector("Enter name of Edit List File:","","*","");
         if(name==0) return;
         sprintf(str,"ws %d %d %s\n",selection_start,selection_end,name);
         write(out_pipe,str,strlen(str));
         break;
      case 11:
         if(selection_start >= 0)
         {
            sprintf(str,"s%d\n",selection_start);
            write(out_pipe,str,strlen(str));
         }
         else
            printf("Selection Start is not set!\n");
         break;
      case 12:
         if(selection_end >= 0)
         {
            sprintf(str,"s%d\n",selection_end);
            write(out_pipe,str,strlen(str));
         }
         else
            printf("Selection End is not set!\n");
         break;
      default:
         printf("selection %ld\n",data);
   }
}

void create_child(char **args)
{
   int ipipe[2], opipe[2];
   int n, vlen;
   char version[32];

   if(pipe(ipipe)!=0 || pipe(opipe)!=0) { perror("Starting "PLAY_PROG); exit(1); }

   pid = fork();

   if(pid<0) { perror("Starting "PLAY_PROG); exit(1); }

   if (pid)
   {
      /* We are the parent */

      inp_pipe = ipipe[0];
      close(ipipe[1]);
      out_pipe = opipe[1];
      close(opipe[0]);
   }
   else
   {
      /* We are the child */

      close(ipipe[0]);
      close(opipe[1]);

      close(0);
      n = dup(opipe[0]);
      if(n!=0) exit(1);

      close(opipe[0]);
      close(1);
      n = dup(ipipe[1]);
      if(n!=1) exit(1);
      close(ipipe[1]);
      close(2);
      n = dup(1);
      if(n!=2) exit(1);

      execvp(PLAY_PROG,args);

      /* if exec returns, an error occured */
      exit(1);
   }

   /* Check if child sends right version number */

   vlen = strlen(LAVPLAY_VSTR);
   n = read(inp_pipe,version,vlen+1); /* vlen+1: for trailing \n */
   version[vlen] = 0;
   if(n!=vlen+1 || strncmp(version,LAVPLAY_VSTR,vlen)!=0)
   {
      fprintf(stderr,"%s did not send correct version info\n",PLAY_PROG);
      fprintf(stderr,"Got: \"%s\" Expected: \"%s\"\n",version,VERSION);
      do_real_exit(0,0);
   }
}

int main(int argc, char *argv[])
{
   int i;
   char **argvn;

   /* copy our argument list */

   argvn = (char**) malloc(sizeof(char*)*(argc+3));
   if(argvn==0) { fprintf(stderr,"malloc failed\n"); exit(1); }
   argvn[0] = PLAY_PROG;
   argvn[1] = "-q";
   argvn[2] = "-g";
   for(i=1;i<argc;i++) argvn[i+2] = argv[i];
   argvn[argc+2] = 0;

   create_child(argvn);

   fl_initialize(&argc, argv, 0, 0, 0);
   fl_set_border_width(-2);
   fd_xlav = create_form_xlav();

   fl_add_io_callback(inp_pipe,FL_READ,get_input,0);
   fl_add_signal_callback(SIGHUP, signal_cb, 0);
   fl_add_signal_callback(SIGINT, signal_cb, 0);

   /* show the first form */
   fl_show_form(fd_xlav->xlav,FL_PLACE_CENTERFREE,FL_FULLBORDER,"xlav");
   fl_do_forms();
   return 0;
}
