/* Form definition file generated with fdesign. */
#include <config.h>
#include "forms.h"
#include <stdlib.h>
#include "xlav.h"


FD_xlav *create_form_xlav(void)
{
  FL_OBJECT *obj;
  FD_xlav *fdui = (FD_xlav *) fl_calloc(1, sizeof(*fdui));

  fdui->xlav = fl_bgn_form(FL_NO_BOX, 760, 170);
  obj = fl_add_box(FL_UP_BOX,0,0,760,170,"");
  fdui->timeslider = obj = fl_add_slider(FL_HOR_NICE_SLIDER,10,20,740,20,"");
    fl_set_object_boxtype(obj,FL_FRAME_BOX);
    fl_set_object_callback(obj,timeslider_cb,0);
    fl_set_slider_value(obj, 0);
    fl_set_slider_size(obj, 0.04);
  fdui->ss = obj = fl_add_button(FL_NORMAL_BUTTON,20,50,40,40,"@|<");
    fl_set_object_lcolor(obj,FL_DODGERBLUE);
    fl_set_object_callback(obj,button_cb,1);
  fdui->se = obj = fl_add_button(FL_NORMAL_BUTTON,280,50,40,40,"@>|");
    fl_set_object_lcolor(obj,FL_DODGERBLUE);
    fl_set_object_callback(obj,button_cb,2);
  fdui->stepr = obj = fl_add_button(FL_TOUCH_BUTTON,340,50,40,40,"@<|");
    fl_set_object_lcolor(obj,FL_DODGERBLUE);
    fl_set_object_callback(obj,button_cb,3);
  fdui->stepf = obj = fl_add_button(FL_TOUCH_BUTTON,380,50,40,40,"@|>");
    fl_set_object_lcolor(obj,FL_DODGERBLUE);
    fl_set_object_callback(obj,button_cb,4);
  fdui->fr = obj = fl_add_button(FL_NORMAL_BUTTON,70,50,40,40,"@<<");
    fl_set_object_lcolor(obj,FL_DODGERBLUE);
    fl_set_object_callback(obj,rb_cb,1);
  fdui->rew = obj = fl_add_button(FL_NORMAL_BUTTON,110,50,40,40,"@<");
    fl_set_object_lcolor(obj,FL_DODGERBLUE);
    fl_set_object_callback(obj,rb_cb,2);
  fdui->stop = obj = fl_add_button(FL_NORMAL_BUTTON,150,50,40,40,"@circle");
    fl_set_object_lcolor(obj,FL_DODGERBLUE);
    fl_set_object_callback(obj,rb_cb,3);
  fdui->play = obj = fl_add_button(FL_NORMAL_BUTTON,190,50,40,40,"@>");
    fl_set_object_lcolor(obj,FL_DODGERBLUE);
    fl_set_object_callback(obj,rb_cb,4);
  fdui->ff = obj = fl_add_button(FL_NORMAL_BUTTON,230,50,40,40,"@>>");
    fl_set_object_lcolor(obj,FL_DODGERBLUE);
    fl_set_object_callback(obj,rb_cb,5);
  fdui->Timer = obj = fl_add_frame(FL_ENGRAVED_FRAME,530,50,140,40,"0:00:00:00");
    fl_set_object_lsize(obj,FL_HUGE_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_RIGHT);
    fl_set_object_lstyle(obj,FL_BOLD_STYLE);
  fdui->Exit = obj = fl_add_button(FL_NORMAL_BUTTON,680,50,70,40,"Exit");
    fl_set_object_lsize(obj,FL_MEDIUM_SIZE);
    fl_set_object_lstyle(obj,FL_BOLD_STYLE);
    fl_set_object_callback(obj,Exit_cb,1);
  fdui->EditFrame = obj = fl_add_labelframe(FL_ENGRAVED_FRAME,10,100,740,50,"Editing");
  fdui->TSelStart = obj = fl_add_text(FL_NORMAL_TEXT,20,110,40,30,"Select.\nStart:");
    fl_set_object_lalign(obj,FL_ALIGN_RIGHT|FL_ALIGN_INSIDE);
  fdui->TSelEnd = obj = fl_add_text(FL_NORMAL_TEXT,150,110,40,30,"Select.\nEnd:");
    fl_set_object_lalign(obj,FL_ALIGN_RIGHT|FL_ALIGN_INSIDE);
  fdui->FSelStart = obj = fl_add_frame(FL_ENGRAVED_FRAME,60,110,90,30,"-");
    fl_set_object_lsize(obj,FL_MEDIUM_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_RIGHT);
    fl_set_object_lstyle(obj,FL_BOLD_STYLE);
  fdui->FSelEnd = obj = fl_add_frame(FL_ENGRAVED_FRAME,190,110,90,30,"-");
    fl_set_object_lsize(obj,FL_MEDIUM_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_RIGHT);
    fl_set_object_lstyle(obj,FL_BOLD_STYLE);
  fdui->BSSelStart = obj = fl_add_button(FL_NORMAL_BUTTON,290,110,30,30,"@|<-");
    fl_set_object_lcolor(obj,FL_DODGERBLUE);
    fl_set_object_callback(obj,selection_cb,1);
  fdui->BSSelEnd = obj = fl_add_button(FL_NORMAL_BUTTON,320,110,30,30,"@->|");
    fl_set_object_lcolor(obj,FL_DODGERBLUE);
    fl_set_object_callback(obj,selection_cb,2);
  fdui->BClearSel = obj = fl_add_button(FL_NORMAL_BUTTON,350,110,30,30,"@1+");
    fl_set_object_lcolor(obj,FL_RED);
    fl_set_object_callback(obj,selection_cb,3);
  fdui->BECut = obj = fl_add_button(FL_NORMAL_BUTTON,450,110,40,30,"Cut");
    fl_set_object_callback(obj,selection_cb,4);
  fdui->BECopy = obj = fl_add_button(FL_NORMAL_BUTTON,490,110,40,30,"Copy");
    fl_set_object_callback(obj,selection_cb,5);
  fdui->BEPaste = obj = fl_add_button(FL_NORMAL_BUTTON,530,110,40,30,"Paste");
    fl_set_object_callback(obj,selection_cb,6);
  fdui->BSaveAll = obj = fl_add_button(FL_NORMAL_BUTTON,580,110,80,30,"Save all ...");
    fl_set_object_callback(obj,selection_cb,7);
  fdui->BSaveSel = obj = fl_add_button(FL_NORMAL_BUTTON,660,110,80,30,"Save select. ...");
    fl_set_object_callback(obj,selection_cb,8);
  fdui->StatDisp = obj = fl_add_frame(FL_ENGRAVED_FRAME,430,50,90,40,"Play");
    fl_set_object_lsize(obj,FL_HUGE_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_LEFT);
    fl_set_object_lstyle(obj,FL_BOLD_STYLE);
  fdui->BGotoSelStart = obj = fl_add_button(FL_NORMAL_BUTTON,380,110,30,30,"@|<");
    fl_set_object_lcolor(obj,FL_DODGERBLUE);
    fl_set_object_callback(obj,selection_cb,11);
  fdui->BGotoSelEnd = obj = fl_add_button(FL_NORMAL_BUTTON,410,110,30,30,"@>|");
    fl_set_object_lcolor(obj,FL_DODGERBLUE);
    fl_set_object_callback(obj,selection_cb,12);
  fl_end_form();

  fdui->xlav->fdui = fdui;

  return fdui;
}
/*---------------------------------------*/

