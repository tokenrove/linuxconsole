/*
 *  amimouse.c  Version 0.1
 *
 *  Copyright (c) 2000 Vojtech Pavlik
 *
 *  Based on the work of:
 *	Michael Rausch		James Banks
 *	Matther Dillon		David Giller
 *	Nathan Laredo		Linus Torvalds
 *	Johan Myreen		Jes Sorensen
 *	Russel King
 *
 *  Sponsored by SuSE
 */

/*
 * Amiga mouse driver for Linux/m68k
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@suse.cz>, or by paper mail:
 * Vojtech Pavlik, Ucitelska 1576, Prague 8, 182 00 Czech Republic
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>

#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/amigahw.h>
#include <asm/amigaints.h>

static int amimouse_used = 0;
static int amimouse_lastx, amimouse_lasty;

static void amimouse_interrupt(int irq, void *dev_id, struct pt_regs *regs);

static int amimouse_open(struct input_dev *dev)
{
	unsigned short joy0dat;

        if (amimouse_used++)
                return 0;

	joy0dat = custom.joy0dat;

	amimouse_lastx = joy0dat & 0xff;
	amimouse_lasty = joy0dat >> 8;

	if (request_irq(IRQ_AMIGA_VERTB, amimouse_interrupt, 0, "amimouse", NULL)) {
                amimouse_used--;
                printk(KERN_ERR "amimouse.c: Can't allocate irq %d\n", amimouse_irq);
                return -EBUSY;
        }

        return 0;
}

static void amimouse_close(struct input_dev *dev)
{
        if (!--amimouse_used)
		free_irq(IRQ_AMIGA_VERTB, amimouse_interrupt);
}

static struct input_dev amimouse_dev = {
	evbit:	{BIT(EV_KEY) | BIT(EV_REL)},
	keybit:	{BIT(BTN_LEFT) | BIT(BTN_MIDDLE) | BIT(BTN_RIGHT)},
	relbit:	{BIT(REL_X) | BIT(REL_Y)},
	open:	amimouse_open,
	close:	amimouse_close
};

static void amimouse_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
	unsigned short joy0dat, potgor;
	int nx, ny, dx, dy;

	joy0dat = custom.joy0dat;

	nx = joy0dat & 0xff;
	ny = joy0dat >> 8;

	dx = nx - amimouse_lastx;
	dy = ny - amimouse_lasty;

	if (dx < -127) dx = (256 + nx) - lastx;
	if (dx >  127) dx = (nx - 256) - lastx;
	if (dy < -127) dy = (256 + ny) - lasty;
	if (dy >  127) dy = (ny - 256) - lasty;

	amimouse_lastx = nx;
	amimouse_lasty = ny;

	potgor = custom.potgor;

	input_report_rel(&amimouse_dev, REL_X, dx);
	input_report_rel(&amimouse_dev, REL_Y, dy);
	
	input_report_btn(&amimouse_dev, BTN_LEFT,   ciaa.pra & 0x40);
	input_report_btn(&amimouse_dev, BTN_MIDDLE, potgor & 0x0100);
	input_report_btn(&amimouse_dev, BTN_RIGHT,  potgor & 0x0400);
}

static int __init amimouse_init(void)
{
	if (!MACH_IS_AMIGA || !AMIGAHW_PRESENT(AMI_MOUSE))
		return -ENODEV;

	input_register_device(&amimouse_dev);

        printk(KERN_INFO "input%d: Amiga mouse at joy0dat\n", amimouse_dev.number);
}

static void __exit amimouse_exit(void)
{
        input_unregister_device(&amimouse_dev);
}

module_init(amimouse_init);
module_exit(amimouse_exit);
