/*
 *  linux/drivers/video/offb.c -- Open Firmware based frame buffer device
 *
 *	Copyright (C) 1997 Geert Uytterhoeven
 *
 *  This driver is partly based on the PowerMac console driver:
 *
 *	Copyright (C) 1996 Paul Mackerras
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/malloc.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/ioport.h>
#ifdef CONFIG_FB_COMPAT_XPMAC
#include <asm/vc_ioctl.h>
#endif
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/bootx.h>

// #include <video/macmodes.h>

/* Supported palette hacks */
enum {
	cmap_unknown,
	cmap_m64,	/* ATI Mach64 */
	cmap_r128,	/* ATI Rage128 */
	cmap_M3A,	/* ATI Rage Mobility M3 Head A */
	cmap_M3B	/* ATI Rage Mobility M3 Head B */
};

struct offb_par {
	volatile unsigned char *cmap_adr;
	volatile unsigned char *cmap_data;
	int cmap_type;
};

#ifdef __powerpc__
#define mach_eieio()	eieio()
#else
#define mach_eieio()	do {} while (0)
#endif

    /*
     *  Interface used by the world
     */
int offb_init(void);

static int offb_check_var(struct fb_var_screeninfo *var, struct fb_info *info); 
static int offb_set_par(struct fb_info *info);
static int offb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                          u_int transp, struct fb_info *info);
static void offb_blank(int blank, struct fb_info *info);

    /*
     *  Interface to the low level console driver
     */
extern boot_infos_t *boot_infos;

static void offb_init_nodriver(struct device_node *);
static void offb_init_fb(const char *name, const char *full_name, int width,
		      int height, int depth, int pitch, unsigned long address,
		      struct device_node *dp);

static struct fb_ops offb_ops = {
	owner:		THIS_MODULE,
	fb_check_var:	offb_check_var,
	fb_set_par:	offb_set_par,
	fb_setcolreg:	offb_setcolreg,
	fb_blank:	offb_blank,
};

    /*
     *  Set the User Defined Part of the Display
     */

static int offb_set_var(struct fb_var_screeninfo *var, 
			struct fb_info *info)
{
    unsigned int oldbpp = 0;
    int err;
    int activate = var->activate;

    if (var->xres > info->var.xres || var->yres > info->var.yres ||
	var->xres_virtual > info->var.xres_virtual ||
	var->yres_virtual > info->var.yres_virtual ||
	var->bits_per_pixel > info->var.bits_per_pixel ||
	var->nonstd ||
	(var->vmode & FB_VMODE_MASK) != FB_VMODE_NONINTERLACED)
	return -EINVAL;
    memcpy(var, &info->var, sizeof(struct fb_var_screeninfo));

    if ((activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
	oldbpp = var.bits_per_pixel;
	info->var = *var;
    }
    if ((oldbpp != var->bits_per_pixel) || (info->cmap.len == 0)) {
	if ((err = fb_alloc_cmap(&info->cmap, 0, 0)))
	    return err;
    }
    return 0;
}

    /*
     *  Initialisation
     */

int __init offb_init(void)
{
    struct device_node *dp;
    unsigned int dpy;
    struct device_node *displays = find_type_devices("display");
    struct device_node *macos_display = NULL;

    /* If we're booted from BootX... */
    if (prom_num_displays == 0 && boot_infos != 0) {
	unsigned long addr = (unsigned long) boot_infos->dispDeviceBase;
	/* find the device node corresponding to the macos display */
	for (dp = displays; dp != NULL; dp = dp->next) {
	    int i;
	    /*
	     * Grrr...  It looks like the MacOS ATI driver
	     * munges the assigned-addresses property (but
	     * the AAPL,address value is OK).
	     */
	    if (strncmp(dp->name, "ATY,", 4) == 0 && dp->n_addrs == 1) {
		unsigned int *ap = (unsigned int *)
		    get_property(dp, "AAPL,address", NULL);
		if (ap != NULL) {
		    dp->addrs[0].address = *ap;
		    dp->addrs[0].size = 0x01000000;
		}
	    }

	    /*
	     * The LTPro on the Lombard powerbook has no addresses
	     * on the display nodes, they are on their parent.
	     */
	    if (dp->n_addrs == 0 && device_is_compatible(dp, "ATY,264LTPro")) {
		int na;
		unsigned int *ap = (unsigned int *)
		    get_property(dp, "AAPL,address", &na);
		if (ap != 0)
		    for (na /= sizeof(unsigned int); na > 0; --na, ++ap)
			if (*ap <= addr && addr < *ap + 0x1000000)
			    goto foundit;
	    }

	    /*
	     * See if the display address is in one of the address
	     * ranges for this display.
	     */
	    for (i = 0; i < dp->n_addrs; ++i) {
		if (dp->addrs[i].address <= addr
		    && addr < dp->addrs[i].address + dp->addrs[i].size)
		    break;
	    }
	    if (i < dp->n_addrs) {
	    foundit:
		printk(KERN_INFO "MacOS display is %s\n", dp->full_name);
		macos_display = dp;
		break;
	    }
	}

	/* initialize it */
	offb_init_fb(macos_display? macos_display->name: "MacOS display",
		     macos_display? macos_display->full_name: "MacOS display",
		     boot_infos->dispDeviceRect[2],
		     boot_infos->dispDeviceRect[3],
		     boot_infos->dispDeviceDepth,
		     boot_infos->dispDeviceRowBytes, addr, NULL);
    }

    for (dpy = 0; dpy < prom_num_displays; dpy++) {
	if ((dp = find_path_device(prom_display_paths[dpy])))
	    offb_init_nodriver(dp);
    }
    return 0;
}


static void __init offb_init_nodriver(struct device_node *dp)
{
    int *pp, i;
    unsigned int len;
    int width = 640, height = 480, depth = 8, pitch;
    unsigned *up, address;

    if ((pp = (int *)get_property(dp, "depth", &len)) != NULL
	&& len == sizeof(int))
	depth = *pp;
    if ((pp = (int *)get_property(dp, "width", &len)) != NULL
	&& len == sizeof(int))
	width = *pp;
    if ((pp = (int *)get_property(dp, "height", &len)) != NULL
	&& len == sizeof(int))
	height = *pp;
    if ((pp = (int *)get_property(dp, "linebytes", &len)) != NULL
	&& len == sizeof(int)) {
	pitch = *pp;
	if (pitch == 1)
	    pitch = 0x1000;
    } else
	pitch = width;
    if ((up = (unsigned *)get_property(dp, "address", &len)) != NULL
	&& len == sizeof(unsigned))
	address = (u_long)*up;
    else {
	for (i = 0; i < dp->n_addrs; ++i)
	    if (dp->addrs[i].size >= pitch*height*depth/8)
		break;
	if (i >= dp->n_addrs) {
	    printk(KERN_ERR "no framebuffer address found for %s\n", dp->full_name);
	    return;
	}

	address = (u_long)dp->addrs[i].address;

	/* kludge for valkyrie */
	if (strcmp(dp->name, "valkyrie") == 0) 
	    address += 0x1000;
    }
    offb_init_fb(dp->name, dp->full_name, width, height, depth,
		 pitch, address, dp);
    
}

static void __init offb_init_fb(const char *name, const char *full_name,
				    int width, int height, int depth,
				    int pitch, unsigned long address,
				    struct device_node *dp)
{
    unsigned long res_start = address;
    unsigned long res_size = pitch*height*depth/8;
    struct fb_fix_screeninfo *fix;
    struct fb_var_screeninfo *var;
    struct fb_info *info;
    struct offb_par *par;	
    int i;	

    if (!request_mem_region(res_start, res_size, "offb"))
	return;

    printk(KERN_INFO "Using unsupported %dx%d %s at %lx, depth=%d, pitch=%d\n",
	   width, height, name, address, depth, pitch);
    if (depth != 8 && depth != 16 && depth != 32) {
	printk(KERN_ERR "%s: can't use depth = %d\n", full_name, depth);
	release_mem_region(res_start, res_size);
	return;
    }
	
    par = kmalloc(sizeof(struct offb_par), GFP_ATOMIC);
    if (par == NULL) {
	release_mem_region(res_start, res_size);
	return;	
    }  	
    memset(par, 0, sizeof(*par));

    info = kmalloc(sizeof(struct fb_info), GFP_ATOMIC);
    if (info == 0) {
	kfree(par);
	release_mem_region(res_start, res_size);
	return;
    }
    memset(info, 0, sizeof(*info));

    fix = &info->fix;
    var = &info->var;

    strcpy(fix->id, "OFfb ");
    strncat(fix->id, name, sizeof(fix->id));
    fix->id[sizeof(fix->id)-1] = '\0';

    var->xres = var->xres_virtual = width;
    var->yres = var->yres_virtual = height;
    fix->line_length = pitch;

    fix->smem_start = address;
    fix->smem_len = pitch * height;
    fix->type = FB_TYPE_PACKED_PIXELS;
    fix->type_aux = 0;

    par->cmap_type = cmap_unknown;
    if (depth == 8)
    {
    	/* XXX kludge for ati */
	if (dp && !strncmp(name, "ATY,Rage128", 11)) {
		unsigned long regbase = dp->addrs[2].address;
		par->cmap_adr = ioremap(regbase, 0x1FFF);
		par->cmap_type = cmap_r128;
	} else if (dp && !strncmp(name, "ATY,RageM3pA", 12)) {
		unsigned long regbase = dp->parent->addrs[2].address;
		par->cmap_adr = ioremap(regbase, 0x1FFF);
		par->cmap_type = cmap_M3A;
	} else if (dp && !strncmp(name, "ATY,RageM3pB", 12)) {
		unsigned long regbase = dp->parent->addrs[2].address;
		par->cmap_adr = ioremap(regbase, 0x1FFF);
		par->cmap_type = cmap_M3B;
	} else if (!strncmp(name, "ATY,", 4)) {
		unsigned long base = address & 0xff000000UL;
		par->cmap_adr = ioremap(base + 0x7ff000, 0x1000) + 0xcc0;
		par->cmap_data = info->cmap_adr + 1;
		par->cmap_type = cmap_m64;
	}
        fix->visual = info->cmap_adr ? FB_VISUAL_PSEUDOCOLOR
				     : FB_VISUAL_STATIC_PSEUDOCOLOR;
    }
    else
	fix->visual = /*info->cmap_adr ? FB_VISUAL_DIRECTCOLOR
				     : */FB_VISUAL_TRUECOLOR;

    var->xoffset = var->yoffset = 0;
    var->bits_per_pixel = depth;
    switch (depth) {
	case 8:
	    var->bits_per_pixel = 8;
	    var->red.offset = 0;
	    var->red.length = 8;
	    var->green.offset = 0;
	    var->green.length = 8;
	    var->blue.offset = 0;
	    var->blue.length = 8;
	    var->transp.offset = 0;
	    var->transp.length = 0;
	    break;
	case 16:	/* RGB 555 */
	    var->bits_per_pixel = 16;
	    var->red.offset = 10;
	    var->red.length = 5;
	    var->green.offset = 5;
	    var->green.length = 5;
	    var->blue.offset = 0;
	    var->blue.length = 5;
	    var->transp.offset = 0;
	    var->transp.length = 0;
	    break;
	case 32:	/* RGB 888 */
	    var->bits_per_pixel = 32;
	    var->red.offset = 16;
	    var->red.length = 8;
	    var->green.offset = 8;
	    var->green.length = 8;
	    var->blue.offset = 0;
	    var->blue.length = 8;
	    var->transp.offset = 24;
	    var->transp.length = 8;
	    break;
    }
    var->red.msb_right = var->green.msb_right = var->blue.msb_right = var->transp.msb_right = 0;
    var->grayscale = 0;
    var->nonstd = 0;
    var->activate = 0;
    var->height = var->width = -1;
    var->pixclock = 10000;
    var->left_margin = var->right_margin = 16;
    var->upper_margin = var->lower_margin = 16;
    var->hsync_len = var->vsync_len = 8;
    var->sync = 0;
    var->vmode = FB_VMODE_NONINTERLACED;

    info->var = *var;
    info->cmap.start = 0;
    info->cmap.len = 0;
    info->cmap.red = NULL;
    info->cmap.green = NULL;
    info->cmap.blue = NULL;
    info->cmap.transp = NULL;
    info->screen_base = ioremap(address, fix->smem_len);
    fix->ypanstep = 0;
    fix->ywrapstep = 0;
  //  disp->can_soft_blank = info->cmap_adr ? 1 : 0;
  //  disp->inverse = 0;
    switch (depth) {
#ifdef FBCON_HAS_CFB8
        case 8:
            disp->dispsw = &fbcon_cfb8;
            break;
#endif
#ifdef FBCON_HAS_CFB16
        case 16:
            disp->dispsw = &fbcon_cfb16;
            disp->dispsw_data = info->fbcon_cmap.cfb16;
            for (i = 0; i < 16; i++)
            	if (fix->visual == FB_VISUAL_TRUECOLOR)
		    info->fbcon_cmap.cfb16[i] =
			    (((default_blu[i] >> 3) & 0x1f) << 10) |
			    (((default_grn[i] >> 3) & 0x1f) << 5) |
			    ((default_red[i] >> 3) & 0x1f);
		else
		    info->fbcon_cmap.cfb16[i] =
			    (i << 10) | (i << 5) | i;
            break;
#endif
#ifdef FBCON_HAS_CFB32
        case 32:
            disp->dispsw = &fbcon_cfb32;
            disp->dispsw_data = info->fbcon_cmap.cfb32;
            for (i = 0; i < 16; i++)
            	if (fix->visual == FB_VISUAL_TRUECOLOR)
		    info->fbcon_cmap.cfb32[i] =
			(default_blu[i] << 16) |
			(default_grn[i] << 8) |
			default_red[i];
		else
		    info->fbcon_cmap.cfb32[i] =
			    (i << 16) | (i << 8) | i;
            break;
#endif
        default:
            disp->dispsw = &fbcon_dummy;
    }

//    disp->scrollmode = SCROLL_YREDRAW;

    strcpy(info->modename, "OFfb ");
    strncat(info->modename, full_name, sizeof(info->modename));
    info->node = -1;
    info->fbops = &offb_ops;
    info->flags = FBINFO_FLAG_DEFAULT;

    offb_set_var(var, &info);

    if (register_framebuffer(&info) < 0) {
	kfree(info);
	kfree(par);
	release_mem_region(res_start, res_size);
	return;
    }

    printk(KERN_INFO "fb%d: Open Firmware frame buffer device on %s\n",
	   GET_FB_IDX(info->node), full_name);

#ifdef CONFIG_FB_COMPAT_XPMAC
    if (!console_fb_info) {
	display_info.height = var->yres;
	display_info.width = var->xres;
	display_info.depth = depth;
	display_info.pitch = fix->line_length;
	display_info.mode = 0;
	strncpy(display_info.name, name, sizeof(display_info.name));
	display_info.fb_address = address;
	display_info.cmap_adr_address = 0;
	display_info.cmap_data_address = 0;
	display_info.disp_reg_address = 0;
	/* XXX kludge for ati */
	if (par->cmap_type == cmap_m64) {
	    unsigned long base = address & 0xff000000UL;
	    display_info.disp_reg_address = base + 0x7ffc00;
	    display_info.cmap_adr_address = base + 0x7ffcc0;
	    display_info.cmap_data_address = base + 0x7ffcc1;
	}
	console_fb_info = info;
    }
#endif /* CONFIG_FB_COMPAT_XPMAC) */
}

    /*
     *  Blank the display.
     */

static void offb_blank(int blank, struct fb_info *info)
{
    int i, j;

    if (!par->cmap_adr)
	return;

    if (blank)
	for (i = 0; i < 256; i++) {
	    switch(par->cmap_type) {
	    case cmap_m64:
	        *par->cmap_adr = i;
	  	mach_eieio();
	  	for (j = 0; j < 3; j++) {
		    *par->cmap_data = 0;
		    mach_eieio();
	    	}
	    	break;
	    case cmap_M3A:
	        /* Clear PALETTE_ACCESS_CNTL in DAC_CNTL */
	    	out_le32((unsigned *)(par->cmap_adr + 0x58),
	    		in_le32((unsigned *)(par->cmap_adr + 0x58)) & ~0x20);
	    case cmap_r128:
	    	/* Set palette index & data */
    	        out_8(par->cmap_adr + 0xb0, i);
	    	out_le32((unsigned *)(par->cmap_adr + 0xb4), 0);
	    	break;
	    case cmap_M3B:
	        /* Set PALETTE_ACCESS_CNTL in DAC_CNTL */
	    	out_le32((unsigned *)(par->cmap_adr + 0x58),
	    		in_le32((unsigned *)(par->cmap_adr + 0x58)) | 0x20);
	    	/* Set palette index & data */
	    	out_8(par->cmap_adr + 0xb0, i);
	    	out_le32((unsigned *)(par->cmap_adr + 0xb4), 0);
	    	break;
	    }
	}
//    else
//	do_install_cmap(currcon, info);
}

    /*
     *  Set a single color register. The values supplied are already
     *  rounded down to the hardware's capabilities (according to the
     *  entries in the var structure). Return != 0 for invalid regno.
     */

static int offb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			 u_int transp, struct fb_info *info)
{
    struct fb_info_offb *info2 = (struct fb_info_offb *)info;
	
    if (!info2->cmap_adr || regno > 255)
	return 1;

    red >>= 8;
    green >>= 8;
    blue >>= 8;

    switch(par->cmap_type) {
    case cmap_m64:
        *par->cmap_adr = regno;
	mach_eieio();
	*par->cmap_data = red;
	mach_eieio();
	*par->cmap_data = green;
	mach_eieio();
	*par->cmap_data = blue;
	mach_eieio();
	break;
    case cmap_M3A:
	/* Clear PALETTE_ACCESS_CNTL in DAC_CNTL */
	out_le32((unsigned *)(par->cmap_adr + 0x58),
		in_le32((unsigned *)(par->cmap_adr + 0x58)) & ~0x20);
    case cmap_r128:
	/* Set palette index & data */
	out_8(par->cmap_adr + 0xb0, regno);
	out_le32((unsigned *)(par->cmap_adr + 0xb4),
		(red << 16 | green << 8 | blue));
	break;
    case cmap_M3B:
        /* Set PALETTE_ACCESS_CNTL in DAC_CNTL */
    	out_le32((unsigned *)(par->cmap_adr + 0x58),
    		in_le32((unsigned *)(par->cmap_adr + 0x58)) | 0x20);
    	/* Set palette index & data */
    	out_8(par->cmap_adr + 0xb0, regno);
  	out_le32((unsigned *)(par->cmap_adr + 0xb4),
    		(red << 16 | green << 8 | blue));
    	break;
    }

    if (regno < 16)
	switch (info->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB16
	    case 16:
		info->pseudo_palette[regno] = (regno << 10) | (regno << 5) | regno;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	    case 32:
	    {
		int i = (regno << 8) | regno;
		info->pseudo_palette[regno] = (i << 16) | i;
		break;
	    }
#endif
       }
    return 0;
}
