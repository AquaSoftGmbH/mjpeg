/*
    saa7110 - Philips SAA7110(A) video decoder driver

    Copyright (C) 1998 Pauline Middelink <middelin@polyware.nl>
    
    Copyright (C) 1999 Wolfgang Scherr <scherr@net4you.net>
    Copyright (C) 2000 Serguei Miridonov <mirsev@cicese.mx>
       - some corrections for Pinnacle Systems Inc. DC10plus card.

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

#include <linux/version.h>

#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#if LINUX_VERSION_CODE < 0x20400
#include <linux/i2c.h>
#else
#include <linux/i2c-old.h>
#endif

#include <linux/videodev.h>
#include <linux/video_decoder.h>

#define DEBUG(x...)			/* remove when no long debugging */

#define SAA7110_MAX_INPUT	9	/* 6 CVBS, 3 SVHS */
#define SAA7110_MAX_OUTPUT	0	/* its a decoder only */

#define	I2C_SAA7110		0x9C	/* or 0x9E */

#define	I2C_DELAY		10	/* 10 us or 100khz */

#if LINUX_VERSION_CODE < 0x20212
typedef struct wait_queue *wait_queue_head_t;
#define     init_waitqueue_head(x)   do { *(x) = NULL; } while (0)
#endif

struct saa7110 {
	struct	i2c_bus	*bus;
	int		addr;
	unsigned char	reg[54];

	int		norm;
	int		input;
	int		enable;
	int		bright;
	int		contrast;
	int		hue;
	int		sat;
	
	wait_queue_head_t wq;
};

/* ----------------------------------------------------------------------- */
/* I2C support functions						   */
/* ----------------------------------------------------------------------- */
static
int saa7110_write(struct saa7110 *decoder, unsigned char subaddr, unsigned char data)
{
	int ack;

	LOCK_I2C_BUS(decoder->bus);
	i2c_start(decoder->bus);
	i2c_sendbyte(decoder->bus, decoder->addr, I2C_DELAY);
	i2c_sendbyte(decoder->bus, subaddr, I2C_DELAY);
	ack = i2c_sendbyte(decoder->bus, data, I2C_DELAY);
	i2c_stop(decoder->bus);
	decoder->reg[subaddr] = data;
	UNLOCK_I2C_BUS(decoder->bus);
	return ack;
}

static
int saa7110_write_block(struct saa7110* decoder, unsigned const char *data, unsigned int len)
{
	unsigned subaddr = *data;

	LOCK_I2C_BUS(decoder->bus);
        i2c_start(decoder->bus);
        i2c_sendbyte(decoder->bus,decoder->addr,I2C_DELAY);
	while (len-- > 0) {
                if (i2c_sendbyte(decoder->bus,*data,0)) {
                        i2c_stop(decoder->bus);
                        return -EAGAIN;
                }
		decoder->reg[subaddr++] = *data++;
        }
	i2c_stop(decoder->bus);
	UNLOCK_I2C_BUS(decoder->bus);

	return 0;
}

static
int saa7110_read(struct saa7110* decoder)
{
	int data;

	LOCK_I2C_BUS(decoder->bus);
	i2c_start(decoder->bus);
	//i2c_sendbyte(decoder->bus, decoder->addr, I2C_DELAY);
	//i2c_start(decoder->bus);
	i2c_sendbyte(decoder->bus, decoder->addr | 1, I2C_DELAY);
	data = i2c_readbyte(decoder->bus, 1);
	i2c_stop(decoder->bus);
	UNLOCK_I2C_BUS(decoder->bus);
	return data;
}

/* ----------------------------------------------------------------------- */
/* SAA7110 functions							   */
/* ----------------------------------------------------------------------- */

#define FRESP_06H_COMPST 0x03       //0x13
#define FRESP_06H_SVIDEO 0x83       //0xC0


static
int saa7110_selmux(struct i2c_device *device, int chan)
{
static	const unsigned char modes[9][8] = {
/* mode 0 */	{ FRESP_06H_COMPST, 0xD9, 0x17, 0x40, 0x03, 0x44, 0x75, 0x16 },
/* mode 1 */	{ FRESP_06H_COMPST, 0xD8, 0x17, 0x40, 0x03, 0x44, 0x75, 0x16 },
/* mode 2 */	{ FRESP_06H_COMPST, 0xBA, 0x07, 0x91, 0x03, 0x60, 0xB5, 0x05 },
/* mode 3 */	{ FRESP_06H_COMPST, 0xB8, 0x07, 0x91, 0x03, 0x60, 0xB5, 0x05 },
/* mode 4 */	{ FRESP_06H_COMPST, 0x7C, 0x07, 0xD2, 0x83, 0x60, 0xB5, 0x03 },
/* mode 5 */	{ FRESP_06H_COMPST, 0x78, 0x07, 0xD2, 0x83, 0x60, 0xB5, 0x03 },
/* mode 6 */	{ FRESP_06H_SVIDEO, 0x59, 0x17, 0x42, 0xA3, 0x44, 0x75, 0x12 },
/* mode 7 */	{ FRESP_06H_SVIDEO, 0x9A, 0x17, 0xB1, 0x13, 0x60, 0xB5, 0x14 },
/* mode 8 */	{ FRESP_06H_SVIDEO, 0x3C, 0x27, 0xC1, 0x23, 0x44, 0x75, 0x21 } };
	struct saa7110* decoder = device->data;
	const unsigned char* ptr = modes[chan];

	saa7110_write(decoder,0x06,ptr[0]);	/* Luminance control	*/
	saa7110_write(decoder,0x20,ptr[1]);	/* Analog Control #1	*/
	saa7110_write(decoder,0x21,ptr[2]);	/* Analog Control #2	*/
	saa7110_write(decoder,0x22,ptr[3]);	/* Mixer Control #1	*/
	saa7110_write(decoder,0x2C,ptr[4]);	/* Mixer Control #2	*/
	saa7110_write(decoder,0x30,ptr[5]);	/* ADCs gain control	*/
	saa7110_write(decoder,0x31,ptr[6]);	/* Mixer Control #3	*/
	saa7110_write(decoder,0x21,ptr[7]);	/* Analog Control #2	*/
	decoder->input = chan;

	return 0;
}

static	const unsigned char initseq[] = {
  	   0, 0x4C, 0x3C, 0x0D, 0xEF, 0xBD, 0xF2, 0x03, 0x00,
  	      0xF8, 0xF8, 0x60, 0x60, 0x00, 0x86, 0x18, 0x90,
  	      0x00, 0x59, 0x40, 0x46, 0x42, 0x1A, 0xFF, 0xDA,
  	      0xF2, 0x8B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  	      0xD9, 0x16, 0x40, 0x41, 0x80, 0x41, 0x80, 0x4F,
  	      0xFE, 0x01, 0xCF, 0x0F, 0x03, 0x01, 0x03, 0x0C,
  	      0x44, 0x71, 0x02, 0x8C, 0x02};

static
int determine_norm(struct i2c_device* device)
{
	struct	saa7110* decoder = device->data;
	int	status;

	/* mode changed, start automatic detection */
	saa7110_write_block(decoder, initseq, sizeof(initseq));
	saa7110_selmux(device, decoder->input);
	sleep_on_timeout(&decoder->wq, HZ/4);
	status = saa7110_read(decoder);
	if (status & 0x40) {
	    DEBUG(printk(KERN_INFO "%s: status=0x%02x (no signal)\n",device->name, status));
	    return decoder->norm; // no change
	}
	if ((status & 3) == 0) {
		saa7110_write(decoder,0x06,0x83);
		if (status & 0x20) {
			DEBUG(printk(KERN_INFO "%s: status=0x%02x (NTSC/no color)\n",device->name, status));
			//saa7110_write(decoder,0x2E,0x81);
			return VIDEO_MODE_NTSC;
		}
		DEBUG(printk(KERN_INFO "%s: status=0x%02x (PAL/no color)\n",device->name, status));
		//saa7110_write(decoder,0x2E,0x9A);
		return VIDEO_MODE_PAL;
	}

	//saa7110_write(decoder,0x06,0x03);
	if (status & 0x20) {	/* 60Hz */
		DEBUG(printk(KERN_INFO "%s: status=0x%02x (NTSC)\n",device->name, status));
		saa7110_write(decoder,0x0D,0x86);
		saa7110_write(decoder,0x0F,0x50);
		saa7110_write(decoder,0x11,0x2C);
		//saa7110_write(decoder,0x2E,0x81);
		return VIDEO_MODE_NTSC;
	}

	/* 50Hz -> PAL/SECAM */
	saa7110_write(decoder,0x0D,0x86);
	saa7110_write(decoder,0x0F,0x10);
	saa7110_write(decoder,0x11,0x59);
	//saa7110_write(decoder,0x2E,0x9A);

	sleep_on_timeout(&decoder->wq, HZ/4);

	status = saa7110_read(decoder);
	if ((status & 0x03) == 0x01) {
		DEBUG(printk(KERN_INFO "%s: status=0x%02x (SECAM)\n",device->name, status));
		saa7110_write(decoder,0x0D,0x87);
		return VIDEO_MODE_SECAM;
	}
	DEBUG(printk(KERN_INFO "%s: status=0x%02x (PAL)\n",device->name, status));
	return VIDEO_MODE_PAL;
}

static
int saa7110_attach(struct i2c_device *device)
{
	struct	saa7110*	decoder;
	int			rv;

	device->data = decoder = kmalloc(sizeof(struct saa7110), GFP_KERNEL);
	if (device->data == 0)
		return -ENOMEM;

	MOD_INC_USE_COUNT;

	/* clear our private data */
	memset(decoder, 0, sizeof(struct saa7110));
	strcpy(device->name, "saa7110");
	decoder->bus = device->bus;
	decoder->addr = device->addr;
	decoder->norm = VIDEO_MODE_PAL;
	decoder->input = 0;
	decoder->enable = 1;
	decoder->bright = 32768;
	decoder->contrast = 32768;
	decoder->hue = 32768;
	decoder->sat = 32768;
        
        init_waitqueue_head(&decoder->wq);

	rv = saa7110_write_block(decoder, initseq, sizeof(initseq));
	if (rv < 0)
		printk(KERN_ERR "%s_attach: init status %d\n", device->name, rv);
	else {
		int ver, status;
		saa7110_write(decoder,0x21,0x10);
		saa7110_write(decoder,0x0e,0x18);
		saa7110_write(decoder,0x0D,0x04);
		ver = saa7110_read(decoder);
		saa7110_write(decoder,0x0D,0x06);
		//mdelay(150);
		status = saa7110_read(decoder);
		printk(KERN_INFO "%s_attach: SAA7110A version %x at 0x%02x, status=0x%02x\n", device->name, ver, device->addr, status);
		saa7110_write(decoder, 0x0D, 0x86);
		saa7110_write(decoder, 0x0F, 0x10);
		saa7110_write(decoder, 0x11, 0x59);
		//saa7110_write(decoder, 0x2E, 0x9A);
	}

        //saa7110_selmux(device,0);
        //determine_norm(device);
	/* setup and implicit mode 0 select has been performed */
	return 0;
}

static
int saa7110_detach(struct i2c_device *device)
{
	struct saa7110* decoder = device->data;

	DEBUG(printk(KERN_INFO "%s_detach\n",device->name));

	/* stop further output */
	saa7110_write(decoder,0x0E,0x00);

	kfree(device->data);

	MOD_DEC_USE_COUNT;
	return 0;
}

static
int saa7110_command(struct i2c_device *device, unsigned int cmd, void *arg)
{
	struct saa7110* decoder = device->data;
	int	v;

	switch (cmd) {
	 case DECODER_GET_CAPABILITIES:
		{
			struct video_decoder_capability *dc = arg;
			dc->flags = VIDEO_DECODER_PAL
				  | VIDEO_DECODER_NTSC
				  | VIDEO_DECODER_SECAM
				  | VIDEO_DECODER_AUTO;
			dc->inputs = SAA7110_MAX_INPUT;
			dc->outputs = SAA7110_MAX_OUTPUT;
		}
		break;

	 case DECODER_GET_STATUS:
		{
			struct saa7110* decoder = device->data;
			int status;
			int res = 0;

			status = i2c_read(device->bus,device->addr|1);
			DEBUG(printk(KERN_INFO "%s: status=0x%02x norm=%d\n",device->name, status, decoder->norm));
			if (!(status & 0x40))
				res |= DECODER_STATUS_GOOD;
			if (status & 0x03)
				res |= DECODER_STATUS_COLOR;

			switch (decoder->norm) {
			 case VIDEO_MODE_NTSC:
				res |= DECODER_STATUS_NTSC;
				break;
			 case VIDEO_MODE_PAL:
				res |= DECODER_STATUS_PAL;
				break;
			 case VIDEO_MODE_SECAM:
				res |= DECODER_STATUS_SECAM;
				break;
			}
			*(int*)arg = res;
		}
		break;

	 case DECODER_SET_NORM:
		v = *(int*)arg;
		if (decoder->norm != v) {
			decoder->norm = v;
			//saa7110_write(decoder, 0x06, 0x03);
			switch (v) {
			 case VIDEO_MODE_NTSC:
				saa7110_write(decoder, 0x0D, 0x86);
				saa7110_write(decoder, 0x0F, 0x50);
				saa7110_write(decoder, 0x11, 0x2C);
				//saa7110_write(decoder, 0x2E, 0x81);
	    	    	    	DEBUG(printk(KERN_INFO "%s: switched to NTSC\n",device->name));
				break;
			 case VIDEO_MODE_PAL:
				saa7110_write(decoder, 0x0D, 0x86);
				saa7110_write(decoder, 0x0F, 0x10);
				saa7110_write(decoder, 0x11, 0x59);
				//saa7110_write(decoder, 0x2E, 0x9A);
	    	    	    	DEBUG(printk(KERN_INFO "%s: switched to PAL\n",device->name));
				break;
			 case VIDEO_MODE_SECAM:
				saa7110_write(decoder, 0x0D, 0x87);
				saa7110_write(decoder, 0x0F, 0x10);
				saa7110_write(decoder, 0x11, 0x59);
				//saa7110_write(decoder, 0x2E, 0x9A);
	    	    	    	DEBUG(printk(KERN_INFO "%s: switched to SECAM\n",device->name));
				break;
			 case VIDEO_MODE_AUTO:
	    	    	    	DEBUG(printk(KERN_INFO "%s: TV standard detection...\n",device->name));
				decoder->norm = determine_norm(device);
				*(int*)arg = decoder->norm;
				break;
			 default:
				return -EPERM;
			}
		}
		break;

	 case DECODER_SET_INPUT:
		v = *(int*)arg;
		if (v<0 || v>SAA7110_MAX_INPUT) {
	    	    	DEBUG(printk(KERN_INFO "%s: input=%d not available\n",device->name, v));
			return -EINVAL;
		}
		if (decoder->input != v) {
			saa7110_selmux(device, v);
	    	    	DEBUG(printk(KERN_INFO "%s: switched to input=%d\n",device->name, v));
		}
		break;

	 case DECODER_SET_OUTPUT:
		v = *(int*)arg;
		/* not much choice of outputs */
		if (v != 0)
			return -EINVAL;
		break;

	 case DECODER_ENABLE_OUTPUT:
		v = *(int*)arg;
		if (decoder->enable != v) {
			decoder->enable = v;
	                saa7110_write(decoder,0x0E, v ? 0x18 : 0x80);
			DEBUG(printk(KERN_INFO "%s: YUV %s\n",device->name,v ? "on" : "off"));
		}
		break;

	 case DECODER_SET_PICTURE:
		{
			struct video_picture *pic = arg;

			if (decoder->bright != pic->brightness) {
				/* We want 0 to 255 we get 0-65535 */
				decoder->bright = pic->brightness;
				saa7110_write(decoder, 0x19, decoder->bright >> 8);
			}
			if (decoder->contrast != pic->contrast) {
				/* We want 0 to 127 we get 0-65535 */
				decoder->contrast = pic->contrast;
				saa7110_write(decoder, 0x13, decoder->contrast >> 9);
			}
			if (decoder->sat != pic->colour) {
				/* We want 0 to 127 we get 0-65535 */
				decoder->sat = pic->colour;
				saa7110_write(decoder, 0x12, decoder->sat >> 9);
			}
			if (decoder->hue != pic->hue) {
				/* We want -128 to 127 we get 0-65535 */
				decoder->hue = pic->hue;
				saa7110_write(decoder, 0x07, (decoder->hue>>8)-128);
			}
		}
		break;

	 case DECODER_DUMP:
		for (v=0; v<0x34; v+=16) {
			int j;
			DEBUG(printk(KERN_INFO "%s: %03x\n",device->name,v));
			for (j=0; j<16; j++) {
				DEBUG(printk(KERN_INFO " %02x",decoder->reg[v+j]));
			}
			DEBUG(printk(KERN_INFO "\n"));
		}
		break;

	 default:
		DEBUG(printk(KERN_INFO "unknown saa7110_command??(%d)\n",cmd));
		return -EINVAL;
	}
	return 0;
}

/* ----------------------------------------------------------------------- */

struct i2c_driver i2c_driver_saa7110 =
{
	"saa7110",			/* name */

	I2C_DRIVERID_VIDEODECODER,		/* in i2c.h */
	I2C_SAA7110, I2C_SAA7110+3,	/* Addr range */

	saa7110_attach,
	saa7110_detach,
	saa7110_command
};

EXPORT_NO_SYMBOLS;

#ifdef MODULE
int init_module(void)
#else
int saa7110_init(void)
#endif
{
	return i2c_register_driver(&i2c_driver_saa7110);
}

#ifdef MODULE
void cleanup_module(void)
{
	i2c_unregister_driver(&i2c_driver_saa7110);
}
#endif
