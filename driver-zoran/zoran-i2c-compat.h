/*
 * Some compat functions for using 2.5 I2C functions with 2.4
 */

#ifndef __ZORAN_I2C_COMPAT_H__
#define __ZORAN_I2C_COMPAT_H__

#include <linux/i2c.h>

#ifndef I2C_DRIVERID_ADV7175
#define I2C_DRIVERID_ADV7175 48 /* same as in 2.5.x */
#endif
#ifndef I2C_DRIVERID_SAA7114
#define I2C_DRIVERID_SAA7114 I2C_DRIVERID_EXP0
#endif
#ifndef I2C_DRIVERID_ADV7170
#define I2C_DRIVERID_ADV7170 I2C_DRIVERID_EXP1
#endif
#ifndef I2C_DRIVERID_VPX3220
#define I2C_DRIVERID_VPX3220 I2C_DRIVERID_VPX32XX
#endif
#ifndef I2C_HW_B_ZR36067
#define I2C_HW_B_ZR36067 0x20
#endif
#ifndef PCI_VENDOR_ID_IOMEGA
#define PCI_VENDOR_ID_IOMEGA 0x13ca
#endif
#ifndef PCI_DEVICE_ID_IOMEGA_BUZ
#define PCI_DEVICE_ID_IOMEGA_BUZ 0x4231
#endif
#ifndef PCI_DEVICE_ID_MIRO_DC10PLUS
#define PCI_DEVICE_ID_MIRO_DC10PLUS 0x7efe
#endif
#ifndef PCI_DEVICE_ID_MIRO_DC30PLUS
#define PCI_DEVICE_ID_MIRO_DC30PLUS 0xd801
#endif
#ifndef PCI_VENDOR_ID_ELECTRONICDESIGNGMBH
#define PCI_VENDOR_ID_ELECTRONICDESIGNGMBH 0x12f8
#endif
#ifndef PCI_DEVICE_ID_LML_33R10
#define PCI_DEVICE_ID_LML_33R10 0x8a02
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)

static inline void
msleep (int msec)
{
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(msec);
}

static inline void *
i2c_get_adapdata (struct i2c_adapter *adap)
{
	return adap->data;
}

static inline void
i2c_set_adapdata (struct i2c_adapter *adap,
                  void               *data)
{
	adap->data = data;
}

static inline void *
i2c_get_clientdata (struct i2c_client *client)
{
	return client->data;
}

static inline void
i2c_set_clientdata (struct i2c_client *client,
                    void              *data)
{
	client->data = data;
}

#define I2C_NAME(s) (s)->name
#define I2C_DEVNAME(n) .name = n

#define iminor(inode) minor(inode->i_rdev)

static inline int
try_module_get (struct module *mod)
{
	__MOD_INC_USE_COUNT (mod);
	return 1;
}

static inline void
module_put (struct module *mod)
{
	__MOD_DEC_USE_COUNT (mod);
}

#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0) */

#define I2C_NAME(s) (s)->name

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0) */

#endif /* __ZORAN_I2C_COMPAT_H__ */
