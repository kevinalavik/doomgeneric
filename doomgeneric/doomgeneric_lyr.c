// doomgeneric for lyrOS

#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"

#include <errno.h>
#include <fcntl.h>
#include <lyr/fb.h>
#include <lyr/input.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static int fb = -1;
static lyr_fb_info_t fb_info;
static struct timespec start_time = { 0 };

static struct termios old_termios;
static int termios_valid = 0;

static lyr_kbd_t kbd;
static int kbd_valid = 0;

#define KEYQUEUE_SIZE 64

struct key_event {
	int pressed;
	unsigned char key;
};

static struct key_event keyqueue[KEYQUEUE_SIZE];
static unsigned keyqueue_rd = 0;
static unsigned keyqueue_wr = 0;

static void queue_key(int pressed, unsigned char key)
{
	unsigned next = (keyqueue_wr + 1) % KEYQUEUE_SIZE;

	if (next == keyqueue_rd)
		return;

	keyqueue[keyqueue_wr].pressed = pressed;
	keyqueue[keyqueue_wr].key = key;
	keyqueue_wr = next;
}

static int dequeue_key(int *pressed, unsigned char *key)
{
	if (keyqueue_rd == keyqueue_wr)
		return 0;

	*pressed = keyqueue[keyqueue_rd].pressed;
	*key = keyqueue[keyqueue_rd].key;
	keyqueue_rd = (keyqueue_rd + 1) % KEYQUEUE_SIZE;

	return 1;
}

static void restore_terminal(void)
{
	if (termios_valid) {
		tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
		termios_valid = 0;
	}

	if (kbd_valid) {
		lyr_kbd_close(&kbd);
		kbd_valid = 0;
	}

	if (fb >= 0) {
		close(fb);
		fb = -1;
	}
}

static void setup_terminal(void)
{
	struct termios tio;

	if (tcgetattr(STDIN_FILENO, &old_termios) == 0) {
		tio = old_termios;
		tio.c_lflag &= ~(ICANON | ECHO);
		tio.c_iflag &= ~(IXON | ICRNL);
		tio.c_cc[VMIN] = 0;
		tio.c_cc[VTIME] = 0;

		if (tcsetattr(STDIN_FILENO, TCSANOW, &tio) == 0)
			termios_valid = 1;

		int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
		if (flags >= 0)
			fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
	}

	memset(&kbd, 0, sizeof(kbd));

	if (lyr_kbd_open(&kbd) == 0)
		kbd_valid = 1;
	if (kbd_valid)
		lyr_kbd_flush(&kbd);
}

static unsigned char map_lyr_key_event(const lyr_key_event_t *ev)
{
	uint16_t keycode = ev->keycode;
	int shift = lyr_key_shift(*ev) != 0;
	int caps = lyr_key_caps(*ev) != 0;

	switch (keycode) {
	case LYR_KEY_ESC:
		return KEY_ESCAPE;

	case LYR_KEY_ENTER:
		return KEY_ENTER;
	case LYR_KEY_KPENTER:
		return KEYP_ENTER;

	case LYR_KEY_TAB:
		return KEY_TAB;

	case LYR_KEY_BACKSPACE:
		return KEY_BACKSPACE;

	case LYR_KEY_SPACE:
		return ' ';

	case LYR_KEY_UP:
		return KEY_UPARROW;
	case LYR_KEY_DOWN:
		return KEY_DOWNARROW;
	case LYR_KEY_LEFT:
		return KEY_LEFTARROW;
	case LYR_KEY_RIGHT:
		return KEY_RIGHTARROW;

	case LYR_KEY_F1:
		return KEY_F1;
	case LYR_KEY_F2:
		return KEY_F2;
	case LYR_KEY_F3:
		return KEY_F3;
	case LYR_KEY_F4:
		return KEY_F4;
	case LYR_KEY_F5:
		return KEY_F5;
	case LYR_KEY_F6:
		return KEY_F6;
	case LYR_KEY_F7:
		return KEY_F7;
	case LYR_KEY_F8:
		return KEY_F8;
	case LYR_KEY_F9:
		return KEY_F9;
	case LYR_KEY_F10:
		return KEY_F10;
	case LYR_KEY_F11:
		return KEY_F11;
	case LYR_KEY_F12:
		return KEY_F12;

	case LYR_KEY_LEFTSHIFT:
	case LYR_KEY_RIGHTSHIFT:
		return KEY_RSHIFT;

	case LYR_KEY_LEFTCTRL:
	case LYR_KEY_RIGHTCTRL:
		return KEY_RCTRL;

	case LYR_KEY_LEFTALT:
	case LYR_KEY_RIGHTALT:
		return KEY_RALT;

	case LYR_KEY_CAPSLOCK:
		return KEY_CAPSLOCK;
	case LYR_KEY_NUMLOCK:
		return KEY_NUMLOCK;
	case LYR_KEY_SCROLLLOCK:
		return KEY_SCRLCK;

	case LYR_KEY_HOME:
		return KEY_HOME;
	case LYR_KEY_END:
		return KEY_END;
	case LYR_KEY_PAGEUP:
		return KEY_PGUP;
	case LYR_KEY_PAGEDOWN:
		return KEY_PGDN;
	case LYR_KEY_INSERT:
		return KEY_INS;
	case LYR_KEY_DELETE:
		return KEY_DEL;

	case LYR_KEY_KP0:
		return KEYP_0;
	case LYR_KEY_KP1:
		return KEYP_1;
	case LYR_KEY_KP2:
		return KEYP_2;
	case LYR_KEY_KP3:
		return KEYP_3;
	case LYR_KEY_KP4:
		return KEYP_4;
	case LYR_KEY_KP5:
		return KEYP_5;
	case LYR_KEY_KP6:
		return KEYP_6;
	case LYR_KEY_KP7:
		return KEYP_7;
	case LYR_KEY_KP8:
		return KEYP_8;
	case LYR_KEY_KP9:
		return KEYP_9;
	case LYR_KEY_KPSLASH:
		return KEYP_DIVIDE;
	case LYR_KEY_KPPLUS:
		return KEYP_PLUS;
	case LYR_KEY_KPMINUS:
		return KEYP_MINUS;
	case LYR_KEY_KPASTERISK:
		return KEYP_MULTIPLY;
	case LYR_KEY_KPDOT:
		return KEYP_PERIOD;

	case LYR_KEY_MINUS:
		return shift ? '_' : KEY_MINUS;
	case LYR_KEY_EQUAL:
		return shift ? '+' : KEY_EQUALS;

	case LYR_KEY_1:
		return shift ? '!' : '1';
	case LYR_KEY_2:
		return shift ? '@' : '2';
	case LYR_KEY_3:
		return shift ? '#' : '3';
	case LYR_KEY_4:
		return shift ? '$' : '4';
	case LYR_KEY_5:
		return shift ? '%' : '5';
	case LYR_KEY_6:
		return shift ? '^' : '6';
	case LYR_KEY_7:
		return shift ? '&' : '7';
	case LYR_KEY_8:
		return shift ? '*' : '8';
	case LYR_KEY_9:
		return shift ? '(' : '9';
	case LYR_KEY_0:
		return shift ? ')' : '0';

	case LYR_KEY_Q:
		return (shift ^ caps) ? 'Q' : 'q';
	case LYR_KEY_W:
		return (shift ^ caps) ? 'W' : 'w';
	case LYR_KEY_E:
		return (shift ^ caps) ? 'E' : 'e';
	case LYR_KEY_R:
		return (shift ^ caps) ? 'R' : 'r';
	case LYR_KEY_T:
		return (shift ^ caps) ? 'T' : 't';
	case LYR_KEY_Y:
		return (shift ^ caps) ? 'Y' : 'y';
	case LYR_KEY_U:
		return (shift ^ caps) ? 'U' : 'u';
	case LYR_KEY_I:
		return (shift ^ caps) ? 'I' : 'i';
	case LYR_KEY_O:
		return (shift ^ caps) ? 'O' : 'o';
	case LYR_KEY_P:
		return (shift ^ caps) ? 'P' : 'p';
	case LYR_KEY_A:
		return (shift ^ caps) ? 'A' : 'a';
	case LYR_KEY_S:
		return (shift ^ caps) ? 'S' : 's';
	case LYR_KEY_D:
		return (shift ^ caps) ? 'D' : 'd';
	case LYR_KEY_F:
		return (shift ^ caps) ? 'F' : 'f';
	case LYR_KEY_G:
		return (shift ^ caps) ? 'G' : 'g';
	case LYR_KEY_H:
		return (shift ^ caps) ? 'H' : 'h';
	case LYR_KEY_J:
		return (shift ^ caps) ? 'J' : 'j';
	case LYR_KEY_K:
		return (shift ^ caps) ? 'K' : 'k';
	case LYR_KEY_L:
		return (shift ^ caps) ? 'L' : 'l';
	case LYR_KEY_Z:
		return (shift ^ caps) ? 'Z' : 'z';
	case LYR_KEY_X:
		return (shift ^ caps) ? 'X' : 'x';
	case LYR_KEY_C:
		return (shift ^ caps) ? 'C' : 'c';
	case LYR_KEY_V:
		return (shift ^ caps) ? 'V' : 'v';
	case LYR_KEY_B:
		return (shift ^ caps) ? 'B' : 'b';
	case LYR_KEY_N:
		return (shift ^ caps) ? 'N' : 'n';
	case LYR_KEY_M:
		return (shift ^ caps) ? 'M' : 'm';

	case LYR_KEY_LEFTBRACE:
		return shift ? '{' : '[';
	case LYR_KEY_RIGHTBRACE:
		return shift ? '}' : ']';
	case LYR_KEY_BACKSLASH:
		return shift ? '|' : '\\';
	case LYR_KEY_SEMICOLON:
		return shift ? ':' : ';';
	case LYR_KEY_APOSTROPHE:
		return shift ? '"' : '\'';
	case LYR_KEY_GRAVE:
		return shift ? '~' : '`';
	case LYR_KEY_COMMA:
		return shift ? '<' : ',';
	case LYR_KEY_DOT:
		return shift ? '>' : '.';
	case LYR_KEY_SLASH:
		return shift ? '?' : '/';

	default:
		return 0;
	}
}

void DG_Init(void)
{
	if (fb >= 0)
		return;

	fb = open(LYR_FB_DEVICE, O_WRONLY);
	if (fb < 0) {
		perror("doomgeneric: open framebuffer");
		exit(1);
	}

	memset(&fb_info, 0, sizeof(fb_info));

	if (ioctl(fb, LYR_FBIOGET_INFO, &fb_info) < 0) {
		perror("doomgeneric: ioctl(LYR_FBIOGET_INFO)");
		exit(1);
	}

	if (fb_info.bpp != 32) {
		fprintf(stderr, "doomgeneric: unsupported framebuffer bpp=%u\n",
				fb_info.bpp);
		exit(1);
	}

	if (fb_info.width == 0 || fb_info.height == 0 || fb_info.pitch == 0) {
		fprintf(stderr, "doomgeneric: invalid framebuffer geometry\n");
		exit(1);
	}

	if (clock_gettime(CLOCK_REALTIME, &start_time) < 0) {
		start_time.tv_sec = 0;
		start_time.tv_nsec = 0;
	}

	setup_terminal();
	atexit(restore_terminal);
}

void DG_DrawFrame(void)
{
	if (fb < 0 || DG_ScreenBuffer == NULL)
		return;

	lseek(fb, 0, SEEK_SET);
	write(fb, DG_ScreenBuffer,
		  DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(*DG_ScreenBuffer));
}

void DG_SleepMs(uint32_t ms)
{
	usleep(ms * 1000);
}

uint32_t DG_GetTicksMs(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_REALTIME, &now) < 0)
		return 0;

	int64_t sec = (int64_t)now.tv_sec - (int64_t)start_time.tv_sec;
	int64_t nsec = (int64_t)now.tv_nsec - (int64_t)start_time.tv_nsec;

	if (nsec < 0) {
		sec--;
		nsec += 1000000000LL;
	}

	if (sec < 0)
		return 0;

	return (uint32_t)(sec * 1000 + nsec / 1000000);
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
	if (dequeue_key(pressed, doomKey))
		return 1;

	if (!kbd_valid)
		return 0;

	for (;;) {
		lyr_key_event_t ev;
		int r = lyr_kbd_poll(&kbd, 0);

		if (r <= 0)
			break;

		r = lyr_kbd_read(&kbd, &ev);
		if (r < 0)
			break;

		unsigned char key = map_lyr_key_event(&ev);
		if (key == 0)
			continue;

		queue_key(ev.down ? 1 : 0, key);
	}

	return dequeue_key(pressed, doomKey);
}

void DG_SetWindowTitle(const char *title)
{
	(void)title;
}

int main(int argc, char **argv)
{
	doomgeneric_Create(argc, argv);

	for (;;)
		doomgeneric_Tick();

	return 0;
}