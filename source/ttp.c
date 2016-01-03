#include <3ds.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdio.h>
#include "archives.h"
#include "sha1.h"

// KernelTimeMachine
// Safe CIA manager
// Licensed under GNU General Public License (GPL)
// Check out https://www.gnu.org/licenses/gpl.html

u32 checkTTP(char region, bool isNew, char* path) { // Verifies the integrity of a TTP file and if it corresponds to the console. (needs sha1.c)

	// Just figured out there's a cleaner syntax to perform sdmc reads. Note to future.
	union {
		u8 c;
		u32 l;
	} longChar; // easy way to convert u8[4] to u32

	Handle file;
	FS_Archive archive = {ARCHIVE_SDMC, {PATH_EMPTY, 0, 0}};
	FSUSER_OpenArchive(&archive);
	FSUSER_OpenFile(&file, archive, fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0);
	FSUSER_CloseArchive(&archive);

	u32 bytesRead = 0;
	char* buf = malloc(0x3);
	Result res = FSFILE_Read(file, &bytesRead, 0x0, buf, 0x3); // 0xD8E007F7 exception (Invalid handle)
	if (buf[0] != 84 || buf[1] != 84 || buf[2] != 80) { free(buf); return res; }
	free(buf); // not sure if freeing all the time is needed, maybe allocating 0x14 since the beginning should be enough

	buf = malloc(0x1);
	FSFILE_Read(file, &bytesRead, 0x3, buf, 0x1);
	if (*buf != region && buf != 0xFF) { free(buf); return 2; }
	//free(buf);

	//buf = malloc(0x1);
	FSFILE_Read(file, &bytesRead, 0x4, buf, 0x1);
	if (*buf != (char)isNew && buf != 0xFF) { free(buf); return 3; }
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
	//FSUSER_CloseArchive(&archive);

	if (!SHA1Result(&context)) { free(buf); return 4; }

	union {
		char c;
		unsigned i[0x5];
	} shaBytes;

	shaBytes.c = buf;
	free(buf);

	for (i = 0; i < 5; i++) {
		if (context.Message_Digest[i] != shaBytes.i[i])
			return 5;
	}

	return 6;

}

bool installTTP(char* path, u8 mediatype) { // Install a TTP file. (needs libzip and installCIA)

	Result res;
	FS_Archive archive = {ARCHIVE_SDMC, {PATH_EMPTY, 0, 0}};
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
		if (!installCIA(ciaPath, mediatype, titleIDs, entries[i].shortName))
			if (!installCIA(ciaPath, mediatype, titleIDs, entries[i].shortName)) // Tries to install the CIA 3 times then give up. If it has to give up, that probably means brick.
				installCIA(ciaPath, mediatype, titleIDs, entries[i].shortName);

		free(ciaPath);
	}

	FSUSER_DeleteDirectoryRecursively(archive, fsMakePath(PATH_ASCII, "/tmp/cias"));

	free(titleIDs);

	return true;

}

volatile char* threadPath;
volatile u8 threadMediatype;
volatile bool isDone;
Handle threadInstallHandle;

void installTTPthread() {
	installTTP(threadPath, threadMediatype);
	isDone = true;
}
