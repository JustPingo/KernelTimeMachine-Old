#include <3ds.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdio.h>
#include "archives.h"
#include "sha1.h"
#include "ttp.h"
#include "cia.h"

// KernelTimeMachine
// Safe CIA manager
// Licensed under GNU General Public License (GPL)
// Check out https://www.gnu.org/licenses/gpl.html

#define USELESS92AMOUNT (12)
u64 uselessTitlesFor92[USELESS92AMOUNT] = { // Latest update: 10.3

	0x0004001B00019002, 0x000400300000B902, 0x0004013000004002, 0x0004003000009502, 0x0004003000009E02, 0x0004001B00010802,
	0x0004009B00010402, 0x0004013000001A02, 0x0004013000001B02, 0x0004800542383841, 0x00048005484E4441, 0x0004800F484E4841

};

bool checkTTP(char region, bool isNew, char* path) { // Verifies the integrity of a TTP file and if it corresponds to the console. (needs sha1.c)

	mbedtls_sha1_context context;

	// Just figured out there's a cleaner syntax to perform sdmc reads. Note to future.

	Result res;
	Handle file;
	FS_Archive archive = {ARCHIVE_SDMC, {PATH_EMPTY, 0, 0}};
	FSUSER_OpenArchive(&archive);
	res = FSUSER_OpenFile(&file, archive, fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0);
	FSUSER_CloseArchive(&archive);

	if (res)
	{
		FSFILE_Close(file);
		return false;
	}

	u32 bytesRead = 0;
	char* buf = malloc(0x3);
	FSFILE_Read(file, &bytesRead, 0x0, buf, 0x3);
	if ((u8)buf[0] != 0x74 || (u8)buf[1] != 0x74 || (u8)buf[2] != 0x70) { free(buf); return false; } // if (buf != "ttp")
	free(buf); // not sure if freeing all the time is needed, maybe allocating 0x14 since the beginning should be enough

	buf = malloc(0x1);
	FSFILE_Read(file, &bytesRead, 0x3, buf, 0x1);
	if ((u8)buf[0] != region && (u8)buf[0] != 0xFF) { free(buf); return false; }
	//free(buf);

	//buf = malloc(0x1);
	FSFILE_Read(file, &bytesRead, 0x4, buf, 0x1);
	if ((u8)buf[0] != (u8)isNew && (u8)buf[0] != 0xFF) { free(buf); return false; }
	free(buf);

	// SHA-1 check
	buf = malloc(0x14);
	FSFILE_Read(file, &bytesRead, 0x5, buf, 0x14);
	mbedtls_sha1_init(&context);
	mbedtls_sha1_starts(&context);

	char t[4];
	FSFILE_Read(file, &bytesRead, 0x19, t, 4); // t is an array, so it's a pointer here
	u32 ulSize =
	( ((u32)t[0]) << (8*3) ) | // t[0] stores MSB of UInt32
	( ((u32)t[1]) << (8*2) ) | // ..
	( ((u32)t[2]) << (8*1) ) | // ..
	( ((u32)t[3]) << (8*0) ) ; // t[3] stores LSB of UInt32

	u32 blockAmount = ulSize / 0x160000;
	u32 i;
	u8* block = malloc(0x160000);
	for (i = 0; i < blockAmount; i++) {
		FSFILE_Read(file, &bytesRead, 0x1D+0x160000*i, block, 0x160000);
		mbedtls_sha1_update(&context, block, 0x160000);
	}

	if (ulSize % 0x160000 != 0) {
		FSFILE_Read(file, &bytesRead, 0x1D+0x160000*blockAmount, block, ulSize-0x160000*blockAmount);
		mbedtls_sha1_update(&context, block, bytesRead);
	}

	free(block);

	FSFILE_Close(file);
	//FSUSER_CloseArchive(&archive);

	u8 hash[20];
	mbedtls_sha1_finish(&context, hash);

	//shaBytes.c = buf;
	

	for (i = 0; i < 20; i++) {
		if (hash[i] != buf[i]) { free(buf); return false; }
	}

	free(buf);
	return true;

}

void removeUselessTitles(u8 mediatype, u64* installedTitles, u32 amount) {

	printf("Removing useless title...\n");
	u32 i;
	u32 y;
	for (i = 0; i < amount; i++) {
		for (y = 0; i < USELESS92AMOUNT; i++) {
			if (installedTitles[i] == uselessTitlesFor92[y]) {
				if (uselessTitlesFor92[y] >> 32 & 0xFFFF) AM_DeleteTitle(mediatype, uselessTitlesFor92[y]);
				else AM_DeleteAppTitle(mediatype, uselessTitlesFor92[y]);
				break;
			}
		}
	}

}

bool installTTP(char* path, u8 mediatype) { // Install a TTP file. (needs libzip and installCIA)

	Result res;
	FS_Archive archive = {ARCHIVE_SDMC, {PATH_EMPTY, 0, 0}};
	FSUSER_OpenArchive(&archive);
	FSUSER_DeleteDirectoryRecursively(archive, fsMakePath(PATH_ASCII, "/tmp/cias"));
	FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/tmp/cias"), 0);
	FILE* ttp = fopen(path, "rb");
	FILE* tmp = fopen("/tmp/cias/ttp.tmp", "wb");

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

	FSUSER_DeleteDirectoryRecursively(archive, fsMakePath(PATH_ASCII, "/tmp/cias"));
	FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/tmp/cias"), 0);

	Zip *zipHandle = ZipOpen("/tmp/cias/ttp.tmp");
	ZipExtract(zipHandle, NULL);
	ZipClose(zipHandle);

	Handle ciaDir;
	FS_Archive fsarchive;
	u32 actualAmount;
	FS_DirectoryEntry* entries;

	FSUSER_DeleteFile(archive, fsMakePath(PATH_ASCII, "/tmp/cias/ttp.tmp"));
	res = FSUSER_OpenDirectory(&ciaDir, fsarchive, fsMakePath(PATH_ASCII, "/tmp/cias"));
	if (res != 0) { free(titleIDs); return false; }
	entries = malloc(256 * sizeof(FS_DirectoryEntry));
	res = FSDIR_Read(ciaDir, &actualAmount, 256, entries);
	if (res != 0) { free(titleIDs); return false; }

	char* ciaPath;
	for (i = 0; i < actualAmount; i++) {
		ciaPath = malloc(14 + strlen(entries[i].shortName));
		strcpy(ciaPath, "/tmp/cias/");
		strcat(ciaPath, entries[i].shortName);
		strcat(ciaPath, ".cia");

		// Tries to install the CIA 5 times before abandon
		for (u8 i = 0; i < 5 && !installCIA(ciaPath, mediatype, titleIDs, titlesAmount, entries[i].shortName, false); i++);

		free(ciaPath);
	}

	installPendingFIRM();

	removeUselessTitles(mediatype, titleIDs, titlesAmount);

	FSUSER_DeleteDirectoryRecursively(archive, fsMakePath(PATH_ASCII, "/tmp/cias"));

	FSUSER_CloseArchive(&archive);

	free(titleIDs);

	return true;

}

volatile char* threadPath;
volatile u8 threadMediatype;
volatile bool isDone;
Handle threadInstallHandle;

void installTTPthread() {
	installTTP((char*) threadPath, threadMediatype);
	isDone = true;
}
