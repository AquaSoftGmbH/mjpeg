#define DEBUGLEVEL 0
/* 
    bt856 - BT856A Digital Video Encoder (Rockwell Part)

    Copyright (C) 1999 Mike Bernson <mike@mlb.org>
    Copyright (C) 1998 Dave Perks <dperks@ibm.net>

    Modifications for LML33/DC10plus unified driver
    Copyright (C) 2000 Serguei Miridonov <mirsev@cicese.mx>
    
    This code was modify/ported from the saa7111 driver written
    by Dave Perks.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/signal.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <linux/sched.h>
#include <asm/segment.h>
#include <linux/types.h>
#include <linux/wrapper.h>

#include <linux/videodev.h>
#include <linux/version.h>
#include <asm/uaccess.h>

#if LINUX_VERSION_CODE >= 0x020300
#include <linux/slab.h>
#include <linux/i2c-old.h>
#else
#include <linux/malloc.h>
#include <linux/i2c.h>
#endif
#include <linux/video_encoder.h>

#if (DEBUGLEVEL > 0)
#define DEBUG(x)  x   /* Debug driver */
#else
#define DEBUG(x)
#endif

/* ----------------------------------------------------------------------- */

#define REG_OFFSET  0xCE

struct bt856
{
   struct i2c_bus   *bus;
   int      addr;
   unsigned char   reg[32];

   int      norm;
   int      enable;
   int      bright;
   int      contrast;
   int      hue;
   int      sat;
};

#define   I2C_BT856        0x88

#define I2C_DELAY   10

/* ----------------------------------------------------------------------- */

static int bt856_write(struct bt856 *dev, unsigned char subaddr, unsigned char data)
{
   int ack;

   LOCK_I2C_BUS(dev->bus);

   i2c_start(dev->bus);
   i2c_sendbyte(dev->bus, dev->addr, I2C_DELAY);
   i2c_sendbyte(dev->bus, subaddr, I2C_DELAY);
   ack = i2c_sendbyte(dev->bus, data, I2C_DELAY);
   dev->reg[subaddr-REG_OFFSET] = data;
   i2c_stop(dev->bus);
   UNLOCK_I2C_BUS(dev->bus);
   return ack;
}

static int bt856_setbit(struct bt856 *dev, int subaddr, int bit, int data)
{
	return bt856_write(dev, subaddr, 
		(dev->reg[subaddr-REG_OFFSET] & ~(1 << bit)) | (data ? (1 << bit) : 0));

}

#if (DEBUGLEVEL > 0)
static void bt856_dump(struct bt856 *encoder)
{
        int i;
        printk(KERN_INFO "%s-bt856: register dump:", encoder->bus->name);
        for (i = 0xd6; i <= 0xde; i += 2)
                printk(" %02x", encoder->reg[i-REG_OFFSET]);
        printk("\n");
}
#endif

/* ----------------------------------------------------------------------- */

static int bt856_attach(struct i2c_device *device)
{
   struct bt856 * encoder;

   /* This chip is not on the buz card but at the same address saa7185 */
   //if (memcmp(device->bus->name, "buz", 3) == 0 || memcmp(device->bus->name, "zr36057", 6) == 0)
   //	return 1;

   device->data = encoder = kmalloc(sizeof(struct bt856), GFP_KERNEL);

   if (encoder == NULL) {
      return -ENOMEM;
   }

   MOD_INC_USE_COUNT;


   memset(encoder, 0, sizeof(struct bt856));
   strcpy(device->name, "bt856");
   encoder->bus = device->bus;
   encoder->addr = device->addr;
   encoder->norm = VIDEO_MODE_NTSC;
   encoder->enable = 1;

   DEBUG(printk(KERN_INFO "%s-bt856: attach\n", encoder->bus->name));

   bt856_write(encoder, 0xdc, 0x18);
   bt856_write(encoder, 0xda, 0);
   bt856_write(encoder, 0xde, 0);

   bt856_setbit(encoder, 0xdc, 3, 1);
   //bt856_setbit(encoder, 0xdc, 6, 0);
   bt856_setbit(encoder, 0xdc, 4, 1);

   switch(encoder->norm) {

   case VIDEO_MODE_NTSC:
	bt856_setbit(encoder, 0xdc, 2, 0);
	break;

   case VIDEO_MODE_PAL:
	bt856_setbit(encoder, 0xdc, 2, 1);
	break;	
   }

   bt856_setbit(encoder, 0xdc, 1, 1);
   bt856_setbit(encoder, 0xde, 4, 0);
   bt856_setbit(encoder, 0xde, 3, 1);
   DEBUG(bt856_dump(encoder));
   return 0;
}


static int bt856_detach(struct i2c_device *device)
{
   kfree(device->data);
   MOD_DEC_USE_COUNT;
   return 0;
}

static int bt856_command(struct i2c_device *device, unsigned int cmd, void * arg)
{
   struct bt856 * encoder = device->data;

   switch (cmd) {

   case 0:  // This is just for testing!!!
           
           DEBUG(printk(KERN_INFO "%s-bt856: init\n", encoder->bus->name));
           bt856_write(encoder, 0xdc, 0x18);
           bt856_write(encoder, 0xda, 0);
           bt856_write(encoder, 0xde, 0);

           bt856_setbit(encoder, 0xdc, 3, 1);
           //bt856_setbit(encoder, 0xdc, 6, 0);
           bt856_setbit(encoder, 0xdc, 4, 1);

           switch(encoder->norm) {

           case VIDEO_MODE_NTSC:
	        bt856_setbit(encoder, 0xdc, 2, 0);
	        break;

           case VIDEO_MODE_PAL:
	        bt856_setbit(encoder, 0xdc, 2, 1);
	        break;	
           }

           bt856_setbit(encoder, 0xdc, 1, 1);
           bt856_setbit(encoder, 0xde, 4, 0);
           bt856_setbit(encoder, 0xde, 3, 1);
           DEBUG(bt856_dump(encoder));
        break;
   
   case ENCODER_GET_CAPABILITIES:
      {
         struct video_encoder_capability *cap = arg;

	 DEBUG(printk(KERN_INFO "%s-bt856: get capabilities\n", 
		encoder->bus->name));

         cap->flags
            = VIDEO_ENCODER_PAL
            | VIDEO_ENCODER_NTSC
            | VIDEO_ENCODER_CCIR;
         cap->inputs = 2;
         cap->outputs = 1;
      }
      break;

   case ENCODER_SET_NORM:
      {
         int * iarg = arg;

	 DEBUG(printk(KERN_INFO "%s-bt856: set norm %d\n", 
		encoder->bus->name, *iarg));
	
         switch (*iarg) {

         case VIDEO_MODE_NTSC:
            bt856_setbit(encoder, 0xdc, 2,0);
            break;

         case VIDEO_MODE_PAL:
            bt856_setbit(encoder, 0xdc, 2, 1);
	    bt856_setbit(encoder, 0xda, 0, 0);
	    //bt856_setbit(encoder, 0xda, 0, 1);
            break;

         default:
            return -EINVAL;

         }
         encoder->norm = *iarg;
         DEBUG(bt856_dump(encoder));
      }
      break;

   case ENCODER_SET_INPUT:
      {
         int * iarg = arg;

	 DEBUG(printk(KERN_INFO "%s-bt856: set input %d\n", 
		encoder->bus->name, *iarg));
	
         /*     We only have video bus.
		*iarg = 0: input is from bt819
                *iarg = 1: input is from ZR36060 */

         switch (*iarg) {

         case 0:
                bt856_setbit(encoder, 0xde, 4, 0);
                bt856_setbit(encoder, 0xde, 3, 1);
                bt856_setbit(encoder, 0xdc, 3, 1);
                bt856_setbit(encoder, 0xdc, 6, 0);
		break;
         case 1:
                bt856_setbit(encoder, 0xde, 4, 0);
                bt856_setbit(encoder, 0xde, 3, 1);
                bt856_setbit(encoder, 0xdc, 3, 1);
                bt856_setbit(encoder, 0xdc, 6, 1);
		break;
         case 2:            // Color bar
                bt856_setbit(encoder, 0xdc, 3, 0);
                bt856_setbit(encoder, 0xde, 4, 1);
                break;
         default:
            return -EINVAL;

         }
         DEBUG(bt856_dump(encoder));
      }
      break;

   case ENCODER_SET_OUTPUT:
      {
         int * iarg = arg;

	 DEBUG(printk(KERN_INFO "%s-bt856: set output %d\n", 
		encoder->bus->name, *iarg));

         /* not much choice of outputs */
         if (*iarg != 0) {
            return -EINVAL;
         }
      }
      break;

  case ENCODER_ENABLE_OUTPUT:
      {
         int * iarg = arg;

         encoder->enable = !!*iarg;

	 DEBUG(printk(KERN_INFO "%s-bt856: enable output %d\n", 
		encoder->bus->name, encoder->enable));
      }
      break;

   default:
      return -EINVAL;
   }

   return 0;
}

/* ----------------------------------------------------------------------- */

struct i2c_driver i2c_driver_bt856 = {
   name:       "bt856",      /* name */
   id:         I2C_DRIVERID_VIDEOENCODER,   /* ID */
   addr_l:     I2C_BT856,
   addr_h:     I2C_BT856+1,
 
   attach:     bt856_attach,
   detach:     bt856_detach,
   command:    bt856_command
};

EXPORT_NO_SYMBOLS;

#ifdef MODULE
int init_module(void)
#else
int bt856_init(void)
#endif
{
   return i2c_register_driver(&i2c_driver_bt856);
}



#ifdef MODULE

void cleanup_module(void)
{
   i2c_unregister_driver(&i2c_driver_bt856);
}

#endif
