#include <linux/ctype.h>

struct procfs_params_zr36067 {
	char *name;
	short reg;
	u32 mask;
	short bit;
};

static struct procfs_params_zr36067 zr67[] = {
	{"HSPol", 0x000, 1, 30},
	{"HStart", 0x000, 0x3ff, 10},
	{"HEnd", 0x000, 0x3ff, 0},

	{"VSPol", 0x004, 1, 30},
	{"VStart", 0x004, 0x3ff, 10},
	{"VEnd", 0x004, 0x3ff, 0},

	{"ExtFl", 0x008, 1, 26},
	{"TopField", 0x008, 1, 25},
	{"VCLKPol", 0x008, 1, 24},
	{"DupFld", 0x008, 1, 20},
	{"LittleEndian", 0x008, 1, 0},

	{"HsyncStart", 0x10c, 0xffff, 16},
	{"LineTot", 0x10c, 0xffff, 0},

	{"NAX", 0x110, 0xffff, 16},
	{"PAX", 0x110, 0xffff, 0},

	{"NAY", 0x114, 0xffff, 16},
	{"PAY", 0x114, 0xffff, 0},
/*    {"",,,}, */

	{NULL, 0, 0, 0},
};

static void setparam(struct zoran *zr, char *name, char *sval)
{
	int i, reg0, reg, val;
	i = 0;
	while (zr67[i].name != NULL) {
		if (!strncmp(name, zr67[i].name, strlen(zr67[i].name))) {
			reg = reg0 = btread(zr67[i].reg);
			reg &= ~(zr67[i].mask << zr67[i].bit);
			if (!isdigit(sval[0]))
				break;
			val = simple_strtoul(sval, NULL, 0);
			if ((val & ~zr67[i].mask))
				break;
			reg |= (val & zr67[i].mask) << zr67[i].bit;
			printk(KERN_INFO "%s: setparam: setting ZR36067 register 0x%03x: 0x%08x=>0x%08x %s=%d\n",
			       zr->name, zr67[i].reg, reg0, reg, zr67[i].name, val);
			btwrite(reg, zr67[i].reg);
			break;
		}
		i++;
	}
}

struct procfs_io {
        char *buffer;
        char *end;
        int  neof;
        int  count;
        int  count_current;
};

static int print_procfs (struct procfs_io *io, const char *fmt, ...)
{
        va_list args;
        int i;

        if (io->buffer >= io->end) {
            io->neof++;
            return 0;
        }
        if (io->count > io->count_current++)
            return 0;
        va_start(args, fmt);
        i = vsprintf(io->buffer, fmt, args);
        io->buffer += i;
        va_end(args);
        return i;
}

static void zoran_procfs_output(struct procfs_io *io,  void *data)
{
	int i;
	struct zoran *zr;
	zr = (struct zoran *) data;

	print_procfs(io, "ZR36067 registers:");
	for (i = 0; i < 0x130; i += 4) {
		if (!(i % 16)) {
			print_procfs(io, "\n%03X", i);
		};
		print_procfs(io, " %08X ", btread(i));
	};
	print_procfs(io, "\n");
}

static int zoran_read_proc(char *buffer, char **start, off_t offset, int size, int *eof, void *data)
{
#ifdef CONFIG_PROC_FS
        struct procfs_io io;
        int    nbytes;

        io.buffer = buffer;
        io.end    = buffer + size - 128; // Just to make it a little bit safer
        io.count  = offset;
        io.count_current = 0;
        io.neof   = 0;
        zoran_procfs_output(&io, data);
        *start    = (char *)(io.count_current - io.count);
        nbytes = (int)(io.buffer - buffer);
        *eof = !io.neof;
        return nbytes;
#endif
	return 0;
}

static int zoran_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
#ifdef CONFIG_PROC_FS
	char *string, *sp;
	char *line, *ldelim, *varname, *svar, *tdelim;
	struct zoran *zr;

	zr = (struct zoran *) data;

	string = sp = vmalloc(count + 1);
	if (!string) {
		printk(KERN_ERR "%s: write_proc: can not allocate memory\n", zr->name);
		return -ENOMEM;
	}
	memcpy(string, buffer, count);
	string[count] = 0;
	DEBUG2(printk(KERN_INFO "%s: write_proc: name=%s count=%lu data=%x\n", zr->name, file->f_dentry->d_name.name, count, (int) data));
	ldelim = " \t\n";
	tdelim = "=";
	line = strpbrk(sp, ldelim);
	while (line) {
		*line = 0;
		svar = strpbrk(sp, tdelim);
		if (svar) {
			*svar = 0;
			varname = sp;
			svar++;
			setparam(zr, varname, svar);
		}
		sp = line + 1;
		line = strpbrk(sp, ldelim);
	}
	vfree(string);
#endif
	return count;
}

static int zoran_proc_init(int i)
{
#ifdef CONFIG_PROC_FS
	char name[8];
	sprintf(name, "zoran%d", i);
	if ((zoran[i].zoran_proc = create_proc_entry(name, 0, 0))) {
		zoran[i].zoran_proc->read_proc = zoran_read_proc;
		zoran[i].zoran_proc->write_proc = zoran_write_proc;
		zoran[i].zoran_proc->data = &zoran[i];
                zoran[i].zoran_proc->owner = THIS_MODULE;
		printk(KERN_INFO "%s: procfs entry /proc/%s allocated. data=%x\n", zoran[i].name, name, (int) zoran[i].zoran_proc->data);
	} else {
		printk(KERN_ERR "%s: Unable to initialise /proc/%s\n", zoran[i].name, name);
		return 1;
	}
#endif
	return 0;
}

static void zoran_proc_cleanup(int i)
{
#ifdef CONFIG_PROC_FS
	char name[8];
	sprintf(name, "zoran%d", i);
	if (zoran[i].zoran_proc) {
		remove_proc_entry(name, 0);
	}
	zoran[i].zoran_proc = NULL;
#endif
}
