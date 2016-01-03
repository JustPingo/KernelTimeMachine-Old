#include <3ds.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdio.h>
#include "ttp.h"

// KernelTimeMachine
// Safe CIA manager
// Licensed under GNU General Public License (GPL)
// Check out https://www.gnu.org/licenses/gpl.html

PrintConsole topConsole;
PrintConsole botConsole;

u8 downgradeMenu() {
	Result res;
	u32 kDown;
	u16 timer = 255;
	bool shouldNotChange = true;
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
		printf("Press (B) to get back.\n");

		if (timer > 1) {
			printf("Please read the instructions to continue.\n\n");
		} else {
			printf("Press (A) to proceed.");
		}

		gspWaitForVBlank();
		gfxFlushBuffers();
		gfxSwapBuffers();
		while (aptMainLoop() && !canContinue && shouldNotChange) {

			if (timer > 1) timer--;
			else { if (timer == 1) shouldNotChange = false; }

			hidScanInput();
			kDown = hidKeysDown();
			if (kDown & KEY_B)
				return 1;
			if ((kDown & KEY_A) && timer == 0)
				canContinue = true;
		}
		shouldNotChange = true;
		timer = 0;
	}

	if (!canContinue) return 0;

	u32 firmware = osGetFirmVersion(); // according to some tests, this does not work
	u32 major = GET_VERSION_MAJOR(firmware); // doesn't matter really, will be deleted if it sucks
	u32 minor = GET_VERSION_MINOR(firmware);
	u32 rev = GET_VERSION_REVISION(firmware);

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

	Handle packagesDir;
	//FS_Archive fsarchive;
	FS_DirectoryEntry* entries;
	u32 actualAmount = 0;
	u8 currentPack;
	bool showNot;
	FS_DirectoryEntry chosenPack;

	packChoice: ; // despite common belief, gotoes are great when you're not doing the fuck with the memory
	FS_Archive fsarchive = {ARCHIVE_SDMC, {PATH_EMPTY, 0, 0}};
	FSUSER_OpenArchive(&fsarchive);
	res = FSUSER_OpenDirectory(&packagesDir, fsarchive, fsMakePath(PATH_ASCII, "/downgrade"));
	FSUSER_CloseArchive(&fsarchive);

	//I have no idea if this works correctly, apparently you can't set it on the same line, pointers when referencing are redundant
	entries = malloc(16 * sizeof(FS_DirectoryEntry));

	res = FSDIR_Read(packagesDir, &actualAmount, 16, entries);
	if (actualAmount == 0) {
		consoleClear();
		clearScreen();

		printf("No file found.\n\nPress (B) to exit.\n\n%x", res);
		while (aptMainLoop() && !canContinue) {
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

	currentPack = 0;
	showNot = false;
	shouldNotChange = true;
	while (aptMainLoop() && !canContinue) {
		consoleClear();
		clearScreen();

		printf("Please choose the downgrade pack\nyou want to install.\n\n");

		printf(" < %s >\n", entries[currentPack].shortName);

		printf("%s\nUse (LEFT) and (RIGHT) to choose.\nPress (X) to confirm.", (showNot ? "Not a Time Traveller Package!\n" : ""));

		//printf("\n\n%i\n%i\n%i\n%i", entries[currentPack].shortExt[0], entries[currentPack].shortExt[1] ,entries[currentPack].shortExt[2], entries[currentPack].shortExt[3]);

		while (aptMainLoop() && !canContinue && shouldNotChange) {
			hidScanInput();
			kDown = hidKeysDown();
			if (kDown & KEY_B) {
				free(entries);
				return 1;
			}

			if (kDown & KEY_LEFT) {
				showNot = false;
				shouldNotChange = false;
				if (currentPack == 0) currentPack = actualAmount-1;
				else currentPack--;
			} else if (kDown & KEY_RIGHT) {
				showNot = false;
				shouldNotChange = false;
				if (currentPack == actualAmount-1) currentPack = 0;
				else currentPack++;
			} else if (kDown & KEY_X) {
				shouldNotChange = false;
				if (entries[currentPack].shortExt[0] != 84 || entries[currentPack].shortExt[1] != 84 || entries[currentPack].shortExt[2] != 80 || entries[currentPack].shortExt[3] != 0)
					showNot = true; // shortExt != "TTP"
				else canContinue = true;
			}


			gspWaitForVBlank();
			gfxFlushBuffers();
			gfxSwapBuffers();
		}
		shouldNotChange = true;
	}

	chosenPack = entries[currentPack];
	FSDIR_Close(packagesDir);
	free(entries);

	canContinue = false;
	timer = 0;

	consoleClear();
	clearScreen();

	printf("Detected model: %s ", modelName);
	printf("(%s family)\n", isNew ? "New3DS" : "Old3DS");
	printf("Detected region: %s\n", regionName);
	//printf("Detected firmware: %i.%i.%i\n", major, minor, rev);
	printf("Downgrade pack: %s\n\n", chosenPack.shortName);

	bool canGoAhead = false;
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
				printf("Is that correct?\n");
				printf("In case of error, please exit and contact the community\nfor more information. DO NOT try if there is\nat least one info wrong.\n\n");
				if (timer == 0) {
					printf("Press (A) to confirm.\n");
					canGoAhead = true;
				} else timer--;
			}
		}
	}

	printf("Press (LEFT) to choose another downgrade pack.\n");
	printf("Press (B) to exit.");

	while (aptMainLoop() && !canContinue) {
		hidScanInput();
		kDown = hidKeysDown();
		if (kDown & KEY_B)
			return 1;

		if (kDown & KEY_A && canGoAhead)
			canContinue = true;

		if (kDown & KEY_LEFT)
			goto packChoice;

		gspWaitForVBlank();
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	if (!canContinue) return 0;

	char* completePath = malloc(strlen(chosenPack.shortName) + 11);
	strcpy(completePath, "/downgrade/");
	strcat(completePath, &(chosenPack.shortName));
	if (/*!checkTTP(region, isNew, completePath)*/true) {
		u32 value = checkTTP(region, isNew, completePath);
		free(completePath);

		consoleClear();
		clearScreen();

		printf("Your downgrade pack (or KTM itself) seems corrupted or inappropriate.\n");
		printf("Press (B) to exit.\n\n%x", value);
		while (aptMainLoop()) {
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

		PTMU_GetBatteryChargeState(&isBatteryCharging);
		PTMU_GetBatteryLevel(&batteryLevel);

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
					canContinue = true;
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

	u32 *threadStack = memalign(32, 4 * 1024);

	threadPath = completePath;
	threadMediatype = MEDIATYPE_NAND;
	isDone = false;

	consoleClear();
	printf("DOWNGRADE PROCESS HAS STARTED. FOR YOUR OWN SAFETY:\n\n");
	printf("DO NOT panic, this won't take long.\n");
	printf("DO NOT turn off you console during\nthe process: it WILL brick.\n");
	printf("DO NOT get to HomebrewMenu during\nthe process: it WILL brick.\n");
	printf("DO NOT remove the SD card during\nthe process: it WILL brick.\n");
	printf("DO NOT remove the battery during\nthe process: it WILL brick.\n");
	printf("AVOID to unplug the charger.\n\n");
	printf("Please wait...");

	consoleInit(GFX_BOTTOM, &botConsole);
	consoleSelect(&botConsole);

	res = svcCreateThread(&threadInstallHandle, installTTPthread, 0, &threadStack[1024], 0x3f, 0);

	while (!isDone) {
		gspWaitForVBlank();
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	consoleClear();
	consoleSelect(&topConsole);

	printf("Downgrade complete.\nPress (START) to reboot the console.");

	while (aptMainLoop()) {

		gspWaitForVBlank();
		gfxFlushBuffers();
		gfxSwapBuffers();

		hidScanInput();
		kDown = hidKeysDown();
		if (kDown & KEY_START) {
			aptOpenSession();
			APT_HardwareResetAsync();
			aptCloseSession();
			return 0;
		}
	}

	return 0;
}

u8 mainMenu() {
	u32 kDown = 0;
	bool shouldNotChange = true;
	gspWaitForVBlank();
	consoleClear();
	//clearScreen();
	printf("KernelTimeMachine\nFix your mistakes\n");
	printf("-----------------\n\n");
	printf("(A) Install CIA [WIP]\n");
	printf("(Y) Downgrade Firmware\n");
	printf("(L+Y) Downgrade MSET [WIP]\n");
	printf("(R+Y) Downgrade Browser [WIP]\n");
	printf("(START) Exit");
	//gfxFlushBuffers();
	//gfxSwapBuffers();
	while (aptMainLoop()) {
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
