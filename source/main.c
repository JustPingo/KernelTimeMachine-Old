#include <3ds.h>
#include <stdio.h>
//#include "libzip/zip.h"
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

bool checkTTP(char region, bool isNew, char* path) { // Verifies the integrity of a TTP file and if it corresponds to the console. (needs sha1.c)

	// Just figured out there's a cleaner syntax to perform sdmc reads. Note to future.
	union {
		u8 c[4];
		u32 l;
	} longChar; // easy way to convert u8[4] to u32

	Handle file;
	FS_Archive archive = {0, {PATH_EMPTY, 0, 0}};
	FSUSER_OpenArchive(&archive);
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
	longChar.c = size; // this might be buggy because of little endian shit
	free(size);

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

	FSFILE_Close(file);

	if (!SHA1Result(&context)) { free(buf); return false; }

	union {
		char c[0x14];
		unsigned i[0x5];
	} shaBytes;

	shaBytes.c = buf;
	free(buf);

	for (i = 0; i < 5; i++) {
		if (context.Message_Digest[i] != shaBytes.i[i]) 
			return false;
	}

	return true;

}

bool installCIA(char* path, u8 mediatype, u64* installedTitleIDs, char* name) {

	Result res;

	printf("Installing %s...\n", name);
	Handle ciaHandle;
	AM_TitleEntry ciaInfo;
	FILE* file = fopen(path, "rb");
	if (file == NULL) return false;

	res = AM_GetCiaFileInfo(u8 mediatype, &ciaInfo, cia);

	if (res != 0) return false;

	u32 i;
	for (i = 0; i < sizeof(installedTitleIDs) / 8; i++) {
		if (installedTitleIDs[i] == ciaInfo.titleID) {
			if (titleID >> 32 & 0xFFFF) AM_DeleteTitle(mediaType, ciaInfo.titleID);
			else AM_DeleteAppTitle(mediatype, ciaInfo.titleID);
			break;
		}
	}

	res = AM_StartCiaInstall(mediatype, &ciaHandle);
	if (res != 0) return false;

	fseek(file, 0, SEEK_END);
	off_t size = ftell(file);
	fseek(file, 0, SEEK_SET);
	u32 blockAmount = size / 0x160000; // Finds how many blocks of 4MB you have in the file
	char* block = malloc(0x160000);
	for (i = 0; i < blockAmount; i++) {
		fread(block, 1, 0x160000, file);
		fwrite(block, 1, 0x160000, ciaHandle);
	}

	if (size % 0x160000 != 0) {
		fread(block, 1, size-0x160000*blockAmount, file);
		fwrite(block, 1, size-0x160000*blockAmount, ciaHandle);
	}

	free(block);

	res = AM_FinishCiaInstall(mediatype, &ciaHandle);
	if (res != 0) return false;

	if (ciaInfo.titleID == 0x0004013800000002LL || ciaInfo.titleID == 0x0004013820000002LL) { // If you're installing NATIVE_FIRM
		AM_InstallNativeFirm();
	}

	return true;
	
}

bool installTTP(char* path, u8 mediatype) { // Install a TTP file. (needs libzip and installCIA)

	FS_Archive archive = {ARCH_SDMC, {PATH_EMPTY, 0, 0}};
	FSUSER_DeleteFile(archive, "/tmp/ttp.tmp");
	FILE* ttp = fopen(path, "rb");
	FILE* tmp = fopen("/tmp/ttp.tmp", "wb"); 

	u32 titlesAmount;
	AM_GetTitleCount(mediatype, &titlesAmount);
	u64* titleIDs = malloc(sizeof(u64) * titlesAmount);
	AM_GetTitleIdList(mediatype, titlesAmount, titleIDs);

	u32 size;
	fseek(ttp, 0x19, SEEK_SET);
	fread(&size, 0x4, 1, ttp);
	fseek(ttp, 0x1D, SEEK_SET);

	u32 blockAmount = size / 0x160000; // Finds how many blocks of 4MB you have in the file
	u32 i;
	char* block = malloc(0x160000);
	for (i = 0; i < blockAmount; i++) {
		fread(block, 1, 0x160000, ttp);
		fwrite(block, 1, 0x160000, tmp);
	}

	if (size % 0x160000 != 0) {
		fread(block, 1, size-0x160000*blockAmount, ttp);
		fwrite(block, 1, size-0x160000*blockAmount, tmp);
	}

	free(block);

	fclose(ttp);
	fclose(tmp);

	u32 zipError = 0;
	zip* zipFile = zip_open("/tmp/ttp.tmp", 0, &zipError);

	FSUSER_DeleteDirectoryRecursively(archive, "/tmp/cias");
	FSUSER_CreateDirectory(archive, "/tmp/cias", 0);

	u16 i;
	zip_stat_t entry;
	zip_file_t entryFile;
	char* ciaPath;
	FILE* cia;
	for (i = 0; i < zip_get_num_entries(zipFile, 0); i++) {
		if (zip_stat_index(zipFile, i, 0, &entry) == 0) {
			ciaPath = malloc(10 + strlen(entry.name));
			strcpy(ciaPath, "/tmp/cias/");
			strcat(ciaPath, entry.name);
			entryFile = zip_fopen_index(zipFile, i, 0);
			cia = fopen(ciaPath);

			blockAmount = entry.size / 0x160000;
			u32 i;
			block = malloc(0x160000);
			for (i = 0; i < blockAmount; i++) {
				zip_fread(entryFile, 0x160000, block);
				fwrite(block, 1, 0x160000, cia);
			}

			if (size % 0x160000 != 0) {
				zip_fread(entryFile, size-0x160000*blockAmount, block);
				fwrite(block, 1, size-0x160000*blockAmount, cia);
			}

			free(block);
			zip_fclose(entryFile);
			fclose(cia);

			if (!installCIA(ciaPath, mediatype, entry.name))
				if (!installCIA(ciaPath, mediatype, entry.name)) // Tries to install the CIA 3 times then give up. If it has to give up, that probably means brick.
					installCIA(ciaPath, mediatype, entry.name);

			free(ciaPath);
		}
	}

	zip_close(zipFile);
	FSUSER_DeleteDirectoryRecursively(archive, "/tmp/cias");
	FSUSER_DeleteFile(archive, "/tmp/ttp.tmp");

	free(titleIDs);

	return true;

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

		printf("Press (B) to get back.\n");
		if (timer != 0) {
			printf("Please read the instructions to continue.");
			timer--; 
		} else { 
			printf("Press (A) to proceed.");
		}

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
	FS_archive fsarchive;
	FS_DirectoryEntry* entries;
	u32 actualAmount;
	u8 currentPack;
	bool showNot;
	FS_DirectoryEntry chosenPack;

packChoice: // despite common belief, gotoes are great when you're not doing the fuck with the memory
	res = FSUSER_OpenDirectory(0, &packagesDir, &fsarchive, "/downgrade");

	//I have no idea if this works correctly, apparently you can't set it on the same line, pointers when referencing are redundant
	entries = malloc(16 * sizeof(FS_DirectoryEntry));
	
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

	currentPack = 0;
	showNot = false;
	while (aptMainLoop() && !canContinue) {
		consoleClear();
		clearScreen();

		printf("Please choose the downgrade pack you want to install.\n\n");

		printf(" < %s >\n", entries[currentPack].shortName);

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
			if (entries[currentPack].shortExt != ".ttp") showNot = true;
			else canContinue = true;
		}

		printf("%s\nUse (LEFT) and (RIGHT) to choose.\nPress (X) to confirm.", (showNot ? "Not a Time Traveller Package!\n" : ""));

		gspWaitForVBlank();
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	chosenPack = entries[currentPack];
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

			printf("Your downgrade pack (or KTM itself) seems corrupted or inappropriate.\n");
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
}


u8 mainMenu() {
	u32 kDown;
	while (aptMainLoop()) {
		consoleClear();
		//clearScreen();

		printf("KernelTimeMachine\nFix your mistakes\n");
		printf("----------------\n\n");
		printf("(A) Install CIA [WIP]\n");
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

	gfxExit();
	return 0;
}

void KernelTimeMachine(Handle amHandle) {

	am = (amHandle == NULL ? (amInit() == 0 ? *amGetSessionHandle() : NULL) : amHandle);
	if (am == NULL) return;
	main();

}
