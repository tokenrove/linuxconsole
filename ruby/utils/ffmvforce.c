/*
 * Tests the force feedback driver
 * Opens a window. When the user clicks in the window, a force effect
 * is generated according to the position of the mouse.
 * This program needs the SDL library (http://www.libsdl.org)
 * Copyright 2001 Johann Deneux <deneux@ifrance.com>
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
 * You can contact the author by email at this address:
 * Johann Deneux <deneux@ifrance.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <linux/types.h>
#include <linux/input.h>
#include <SDL.h>

#define BIT(x) (1<<(x))
#define STR_LEN	64
#define	WIN_W	400
#define WIN_H	400
#define max(a,b)	((a)>(b)?(a):(b))

/* File descriptor of the force feedback /dev entry */
static int ff_fd;

static void generate_force(int x, int y)
{
	static int first = 1;
	double nx, ny;
	double angle;
	struct ff_effect effect;

	nx = 2*(x-WIN_W/2.0)/WIN_W;
	ny = 2*(y-WIN_H/2.0)/WIN_H;
	angle = atan2(nx, -ny);
printf("mouse: %d %d n: %4.2f %4.2f angle: %4.2f\n", x, y, nx, ny, angle);
	effect.type = FF_CONSTANT;
        effect.id = 0;
        effect.u.constant.level = 0x7fff * max(fabs(nx), fabs(ny));
        effect.u.constant.direction = 0x8000 * (angle + M_PI)/M_PI;
printf("level: %04x direction: %04x\n", (unsigned int)effect.u.constant.level, (unsigned int)effect.u.constant.direction);
        effect.u.constant.shape.attack_length = 0;
        effect.u.constant.shape.attack_level = 0;
        effect.u.constant.shape.fade_length = 0;
        effect.u.constant.shape.fade_level = 0;
        effect.trigger.button = 0;
        effect.trigger.interval = 0;
        effect.replay.length = 0xffff;
        effect.replay.delay = 0;

/* BUG: This will fill the memory of the device very quickly */
        if (ioctl(ff_fd, EVIOCSFF, &effect) == -1) {
                perror("Upload effect");
                exit(1);
        }

	/* If first time, start to play the effect */
	if (first) {
		struct input_event play;
		first = 0;
		play.type = EV_FF;
		play.code = FF_PLAY;
		play.value = 1;

		if (write(ff_fd, (const void*) &play, sizeof(play)) == -1) {
			perror("Play effect");
			exit(1);
		}
	}
}

static void stop_effect()
{
	struct input_event stop;
	stop.type = EV_FF;
	stop.code = FF_STOP;

	if (write(ff_fd, (const void*) &stop, sizeof(stop)) == -1) {
		perror("Stop effect");
	}
}

int main(int argc, char** argv)
{
	SDL_Surface* screen;
	char dev_name[STR_LEN];
	int i;

	/* Parse parameters */
	strcpy(dev_name, "/dev/input/event0");
	for (i=1; i<argc; ++i) {
		if (strncmp(argv[i], "--help", STR_LEN) == 0) {
			printf("Usage: %s /dev/input/eventXX\n", argv[0]);
			printf("Generates constant force effects depending on the position of the mouse\n");
			exit(1);
		}
		else {
			strncpy(dev_name, argv[i], STR_LEN);
		}
	}

	/* Initialize SDL */
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}
	on_exit(SDL_Quit, NULL);
	screen = SDL_SetVideoMode(WIN_W, WIN_H, 0, SDL_SWSURFACE);
	if (screen == NULL) {
		fprintf(stderr, "Could not set video mode: %s\n", SDL_GetError());
		exit(1);
	}
		
	/* Open force feedback device */
	ff_fd = open(dev_name, O_RDWR);
	if (ff_fd == -1) {
                perror("Open device file");
		exit(1);
	}
	on_exit(stop_effect, NULL);

	/* Main loop */
	for (;;) {
		SDL_Event event;
		SDL_WaitEvent(&event);

		switch (event.type) {
		case SDL_QUIT:
			exit(0);
			break;

		case SDL_MOUSEMOTION:
			if (event.motion.state)
				generate_force(event.motion.x, event.motion.y);
			break;
		}
	}

	return 0;
}