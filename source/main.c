#include <3ds.h>
#include <stdio.h>
#include "libzip/zip.h"
#include "sha1.h"

// KernelTimeMachine
// Safe CIA manager
// Licensed under GNU General Public License (GPL)
// Check out https://www.gnu.org/licenses/gpl.html

Handle am;
PrintConsole topConsole;

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
}

bool checkTTP(char region, bool isNew, char* path) { // Verifies the integrity of a TTP file and if it corresponds to the console.

	// Just figured out there's a cleaner syntax to perform sdmc reads. Note to future.
	union {
		u8 c[4];
		u32 l;
	} longChar; // easy way to convert u8[4] to u32

	Handle file;
	FS_Archive archive = {ARCH_SDMC, {PATH_EMPTY, 0, 0}};
	FSUSER_OpenArchive(0, &archive);
	FSUSER_OpenFile(&file, archive, fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0);

	u32 bytesRead = 0;
	char* buf = malloc(0x3);
	FSFILE_Read(file, &bytesRead, 0x0, buf, 0x3);
	if (strcmp(buf, "ttp") != 0) { free(buf); return false; }
	free(buf); // not sure if freeing all the time is needed, maybe allocating 0x14 since the beginning should be enough

	buf = malloc(0x1);
	FSFILE_Read(file, &bytesRead, 0x3, buf, 0x1);
	if (*buf != region && buf != 0xFF) { free(buf); return false; }
	//free(buf);

	//buf = malloc(0x1);
	FSFILE_Read(file, &bytesRead, 0x4, buf, 0x1);
	if (*buf != (char)isNew && buf != 0xFF) { free(buf); return false; }
	free(buf);

	// SHA-1 check
	buf = malloc(0x14);
	FSFILE_Read(file, &bytesRead, 0x5, buf, 0x14);
	SHA1Context context;
	SHA1Reset(&context);

	u8* size = malloc(0x4);
	FSFILE_Read(file, &bytesRead, 0x19, size, 0x4);
	longChar.c = *size; // this might be buggy because of little endian shit

	u32 blockAmount = longChar.l / 0x160000;
	u32 i;
	char* block = malloc(0x160000);
	for (i = 0; i < blockAmount; i++) {
		FSFILE_Read(file, &bytesRead, 0x1D+0x160000*i, block, 0x160000);
		SHA1Input(&context, block, 0x160000);
	}

	if (longChar.l % 0x160000 != 0) {
		FSFILE_Read(file, &bytesRead, 0x1D+0x160000*blockAmount, block, longChar.l-0x160000*blockAmount);
		SHA1Input(&context, block, bytesRead);
	}

	free(block);

	if (!SHA1Result(&context)) { free(buf); return false; }

	union {
		char c[0x14];
		unsigned i[0x5];
	} shaBytes;

	shaBytes.c = *buf;
	free(buf);

	for (i = 0; i < 5; i++) {
		if (context.Message_Digest[i] != shaBytes.i[i]) 
			return false;
	}

	return true;

}

bool installTTP(char* path) {

	FILE *ciaFile = fopen(path, "r");


}

u8 downgradeMenu() {
	Result res;
	u32 kDown;
	u8 timer = 255;
	bool canContinue = false;
	while (aptMainLoop() && !canContinue) {
		consoleClear();
		clearScreen();

		printf("FIRMWARE DOWNGRADE\n\n");
		printf("WARNING!\n");
		printf("YOU ARE ENTERING A DANGEROUS PROCESS!\n");
		printf("Please read those instructions\nbefore trying anything.\n\n");

		printf("DO NOT turn off you console during\nthe process: it WILL brick.\n");
		printf("DO NOT get to HomebrewMenu during\nthe process: it WILL brick.\n");
		printf("DO NOT remove the SD card during\nthe process: it WILL brick.\n");
		printf("DO NOT throw off your 3DS during\nthe process: it WILL break.\n");
		printf("PLUG your charger in, and have\nsome energy in your battery.\n\n");

		printf("Press (B) to get back.\n")
		if (timer != 0) { printf("Please read the instructions to continue."); timer--; }
		else printf("Press (A) to proceed.");

		gspWaitForVBlank();
		gfxFlushBuffers();
		gfxSwapBuffers();

		hidScanInput();
		kDown = hidKeysDown();
		if (kDown & KEY_B)
			return 1;

		if (kDown & KEY_A && timer == 0)
			canContinue = true;
	}

	if (!canContinue) return 0;

	u32 firmware = osGetFirmVersion(); // according to some tests, this does not work
	u32 major = GET_VERSION_MAJOR(firmware); // doesn't matter really, will be deleted if it sucks
	u32 minor = GET_VERSION_MINOR(firmware));
	u32 rev = GET_VERSION_REVISION(firmware));

	u8 region;
	u8 model;
	char* regionName;
	char* modelName;
	CFGU_SecureInfoGetRegion(&region);
	CFGU_GetSystemModel(&model);

	switch (region) {
		case 0: regionName = "JPN"; break;
		case 1: regionName = "NTSC"; break;
		case 2: 
		case 3: regionName = "PAL"; break; // I implemented the australian case but really no one cares
		case 4: regionName = "CHN"; break;
		case 5: regionName = "KOR"; break;
		case 6: regionName = "TWN"; break;
		default: regionName = "UNKNOWN";
	}

	switch (model) {
		case 0: modelName = "O3DS"; break;
		case 1: modelName = "O3DS XL"; break;
		case 2: modelName = "N3DS"; break;
		case 3: modelName = "2DS"; break;
		case 4: modelName = "N3DS XL"; break;
		default: modelName = "UNKNOWN";
	}

	bool isNew = (model == 2 || model == 4);

	canContinue = false;

packChoice: // yes i know gotoes are the devil
	Handle packagesDir;
	FS_archive fsarchive;
	res = FSUSER_OpenDirectory(0, &packagesDir, &fsarchive, "/downgrade");

	FS_DirectoryEntry* entries[16] = malloc(16 * sizeof(FS_DirectoryEntry));
	u32 actualAmount;
	res = FSDIR_Read(packagesDir, &actualAmount, 16, entries);

	if (actualAmount == 0) {
		while (aptMainLoop() && !canContinue) {
			consoleClear();
			clearScreen();

			printf("No file found.\n\nPress (B) to exit.");

			hidScanInput();
			kDown = hidKeysDown();
			if (kDown & KEY_B) {
				free(entries);
				FSDIR_Close(packagesDir);
				return 1;
			}

			gspWaitForVBlank();
			gfxFlushBuffers();
			gfxSwapBuffers();
		}
	}

	u8 currentPack = 0;
	bool showNot = false;
	while (aptMainLoop() && !canContinue) {
		consoleClear();
		clearScreen();

		printf("Please choose the downgrade pack you want to install.\n\n");

		printf(" < %s >\n", (*entries)[currentPack].shortName);

		hidScanInput();
		kDown = hidKeysDown();
		if (kDown & KEY_LEFT) {
			showNot = false;
			if (currentPack == 0) currentPack = actualAmount-1;
			else currentPack--;
		} else if (kDown & KEY_RIGHT) {
			showNot = false;
			if (currentPack == actualAmount-1) currentPack = 0;
			else currentPack++;
		} else if (kDown & KEY_X) {
			if ((*entries)[currentPack].shortExt != ".ttp") showNot = true;
			else canContinue = true;
		}

		printf("%s\nUse (LEFT) and (RIGHT) to choose.\nPress (X) to confirm.", (showNot ? "Not a Time Traveller Package!\n" : ""));

		gspWaitForVBlank();
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	FS_DirectoryEntry chosenPack = (*entries)[currentPack];
	FSDIR_Close(packagesDir);
	free(entries);

	canContinue = false;
	timer = 100;
	while (aptMainLoop() && !canContinue) {
		consoleClear();
		clearScreen();		

		printf("Detected model: %s ", modelName);
		printf("(%s family)\n", isNew ? "New3DS" : "Old3DS");
		printf("Detected region: %s\n", regionName);
		printf("Detected firmware: %i.%i.%i\n", major, minor, rev);
		printf("Downgrade pack: %s\n\n", chosenPack.shortName);

		hidScanInput();
		kDown = hidKeysDown();
		if (kDown & KEY_B)
			return 1;

		if (kDown & KEY_LEFT)
			goto packChoice;

		if (regionName == "UNKNOWN") {
			printf("Woops! Something weird happened with the region.\nREGIONID: %i\n", region);
			printf("Please contact the community for more info.\n\n");
		} else {
			if ((region == 5 || region == 6) && isNew) {
				printf("Sorry, we didn't know there were N3DS in your region.\nREGIONID: %i\n", region);
				printf("Please contact the community for more info.\n\n");
			} else {
				if (modelName == "UNKNOWN") {
					printf("Woops! Something weird happened with the model.\nMODELID: %i\n", model);
					printf("Please contact the community for more info.\n\n");
				} else {
					printf("Is that correct?");
					printf("In case of error, please exit and contact the community\nfor more information. DO NOT try if there is\nat least one info wrong.\n\n");
					if (timer == 0) {
						printf("Press (A) to confirm.\n");
						if (kDown & KEY_A)
							canContinue = true;
					} else timer--;
				}
			}
		}

		printf("Press (LEFT) to choose another downgrade pack.\n");
		printf("Press (B) to exit.");

		gspWaitForVBlank();
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	if (!canContinue) return 0;

	char* completePath = malloc(strlen(chosenPack.shortName) + 11);
	strcpy(completePath, "/downgrade/");
	strcat(completePath, &(chosenPack.shortName));
	if (!checkTTP(region, isNew, completePath)) {
		free(completePath);
		while (aptMainLoop()) {
			consoleClear();
			clearScreen();

			printf("Your downgrade pack (or KTM itself) seems corrupted or innapropriate.\n");
			printf("Press (B) to exit.");

			hidScanInput();
			kDown = hidKeysDown();
			if (kDown & KEY_B)
				return 1;

			gspWaitForVBlank();
			gfxFlushBuffers();
			gfxSwapBuffers();
		}
		return 0;
	}

	u8 isBatteryCharging;
	u8 batteryLevel;

	canContinue = false;

	while (aptMainLoop() && !canContinue) {
		consoleClear();
		clearScreen();

		hidScanInput();
		kDown = hidKeysDown();

		PTMU_GetBatteryChargeState(NULL, &isBatteryCharging);
		PTMU_GetBatteryLevel(NULL, &batteryLevel);

		printf("Please leave your console charging during the process.\n\n");

		if (kDown & KEY_B)
			return 1;

		if (isBatteryCharging == 0) {
			printf("Your console is not charging. Please leave it charging.\n\n");
		} else {
			if (batteryLevel >= 3) {
				printf("Your console is now ready for the downgrade process.\n\n");
				printf("AFTER THAT POINT YOU MUST NOT TURN OFF THE CONSOLE OR REMOVE\nTHE SD CARD OR IT WILL BRICK!\n\n");
				printf("Press (A) to proceed.\n");
				if (kDown & KEY_A)
					conContinue = true;
			} else {
				printf("To be extra sure, please leave it charging a bit.\n");
				printf("Current charging level: %i of 3 needed\n\n", batteryLevel);
			}
		}

		printf("Press (B) to exit.");

		gspWaitForVBlank();
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	if (!canContinue) return 0;

	// POINT OF NO RETURN

	

}

u8 legitInstallMenu() {
	/*u32 kDown;
	while (aptMainLoop()) {
		consoleClear();
		clearScreen();

		printf("LEGIT CIA INSTALLATION\n\n");
		printf("This allows you to install a certain type\nof CIA files to your sysNAND.\n\nThis won't do the work of a signpatch.\n\nBE EXTREMELY CAREFUL ABOUT WHAT YOU INSTALL");
		printf("Press (A) to proceed.\n");
		printf("Press (B) to get back.");

		gspWaitForVBlank();
		gfxFlushBuffers();
		gfxSwapBuffers();

		hidScanInput();
		kDown = hidKeysDown();
		if (kDown & KEY_B)
			return 1;

		if (kDown & KEY_A)
			break;
	}*/

	return 1;
}

u8 mainMenu() {
	u32 kDown;
	while (aptMainLoop()) {
		consoleClear();
		clearScreen();

		printf("KernelTimeMachine\nFix your mistakes\n");
		printf("----------------\n\n");
		printf("(A) Install CIA [WIP]\n")
		printf("(Y) Downgrade Firmware\n");
		printf("(L+Y) Downgrade MSET [WIP]\n");
		printf("(R+Y) Downgrade Browser [WIP]\n");
		printf("(START) Exit");

		gspWaitForVBlank();
		gfxFlushBuffers();
		gfxSwapBuffers();

		hidScanInput();
		kDown = hidKeysDown();
		if (kDown & KEY_START)
			return 0;

		if (kDown & KEY_Y)
			if (kDown & KEY_L)
				return 4;
			else if (kDown & KEY_R)
				return 5;
			else
				return 3;
	}
	return 0;
}

int main() {
	gfxInitDefault();
	consoleInit(GFX_TOP, &topConsole);
	consoleSelect(&topConsole);

	ptmInit();
	cfguInit();
	fsInit();

	fbTopLeft = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	fbTopRight = gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, NULL, NULL);
	fbBottom = gfxGetFramebuffer(GFX_BOTTOM, 0, NULL, NULL);

	u8 next = mainMenu();
	while (aptMainLoop()) {
		switch (next) {
			case 0: break;
			case 1: next = mainMenu(); break;
			//case 2: next = legitInstallMenu(); break;
			case 3: next = downgradeMenu(); break;
			//case 4: next = downgradeMSETMenu(); break;
			//case 5: next = downgradeBrowserMenu(); break;
			default: next = mainMenu();
		}
		if (next == 0) break;
	}

	srv // remove handle
	gfxExit();
	return 0;
}

void KernelTimeMachine(Handle amHandle) {

	am = (amHandle == NULL ? (amInit() == 0 ? *amGetSessionHandle() : NULL) : amHandle);
	if (am == NULL) return;
	main();

}