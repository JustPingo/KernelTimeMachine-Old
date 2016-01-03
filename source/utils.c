#include <3ds.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdio.h>

// KernelTimeMachine
// Safe CIA manager
// Licensed under GNU General Public License (GPL)
// Check out https://www.gnu.org/licenses/gpl.html

u8* fbTopLeft;
u8* fbTopRight;
u8* fbBottom;

void clearScreen() {
	memset(fbTopLeft, 0, 240 * 400 * 3);
	memset(fbTopRight, 0, 240 * 400 * 3);
	memset(fbBottom, 0, 240 * 320 * 3);
}

int error(char* msg, u8 errorCode) {
	consoleClear();
	clearScreen();
	printf(msg);
	printf("\n\nPress (START) to exit.");
	u32 kDown;
	while (aptMainLoop()) {
		gspWaitForVBlank();
		gfxFlushBuffers();
		gfxSwapBuffers();
		hidScanInput();
		kDown = hidKeysDown();
		if (kDown & KEY_START)
			return errorCode;
	}
	return 0;
}
