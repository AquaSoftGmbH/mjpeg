/** Header file generated with fdesign on Sun Mar  5 18:11:00 2000.**/

#ifndef FD_xlav_h_
#define FD_xlav_h_

/** Callbacks, globals and object handlers **/
extern void timeslider_cb(FL_OBJECT *, long);
extern void button_cb(FL_OBJECT *, long);
extern void rb_cb(FL_OBJECT *, long);
extern void Exit_cb(FL_OBJECT *, long);
extern void selection_cb(FL_OBJECT *, long);


/**** Forms and Objects ****/
typedef struct {
	FL_FORM *xlav;
	void *vdata;
	char *cdata;
	long  ldata;
	FL_OBJECT *timeslider;
	FL_OBJECT *ss;
	FL_OBJECT *se;
	FL_OBJECT *stepr;
	FL_OBJECT *stepf;
	FL_OBJECT *fr;
	FL_OBJECT *rew;
	FL_OBJECT *stop;
	FL_OBJECT *play;
	FL_OBJECT *ff;
	FL_OBJECT *Timer;
	FL_OBJECT *Exit;
	FL_OBJECT *EditFrame;
	FL_OBJECT *TSelStart;
	FL_OBJECT *TSelEnd;
	FL_OBJECT *FSelStart;
	FL_OBJECT *FSelEnd;
	FL_OBJECT *BSSelStart;
	FL_OBJECT *BSSelEnd;
	FL_OBJECT *BClearSel;
	FL_OBJECT *BECut;
	FL_OBJECT *BECopy;
	FL_OBJECT *BEPaste;
	FL_OBJECT *BSaveAll;
	FL_OBJECT *BSaveSel;
	FL_OBJECT *StatDisp;
	FL_OBJECT *BGotoSelStart;
	FL_OBJECT *BGotoSelEnd;
} FD_xlav;

extern FD_xlav * create_form_xlav(void);
extern	void dispatch_input(void);
extern  void get_input(int fd, void *data);
extern  void do_real_exit(int ID, void *data);
extern	void signal_cb(int signum, void *data);
extern	void create_child(char **args);


#endif /* FD_xlav_h_ */
