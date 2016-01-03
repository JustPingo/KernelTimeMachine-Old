#include <3ds.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdio.h>

// KernelTimeMachine
// Safe CIA manager
// Licensed under GNU General Public License (GPL)
// Check out https://www.gnu.org/licenses/gpl.html

bool installCIA(char* path, u8 mediatype, u64* installedTitleIDs, char* name) {

	Result res;
	FS_Archive archive = {ARCHIVE_SDMC, {PATH_EMPTY, 0, 0}};
	FSUSER_OpenArchive(&archive);
	printf("Installing %s...\n", name);
	Handle ciaHandle;
	Handle ciaFileHandle;
	AM_TitleEntry ciaInfo;

	FSUSER_OpenFile(&ciaFileHandle, archive, fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0);
	res = AM_GetCiaFileInfo(mediatype, &ciaInfo, ciaFileHandle);
	FSFILE_Close(ciaFileHandle);
	FSUSER_CloseArchive(&archive);
	if (res != 0) return false;

	FILE* file = fopen(path, "rb");
	if (file == NULL) return false;


	u32 i;
	for (i = 0; i < sizeof(installedTitleIDs) / 8; i++) {
		if (installedTitleIDs[i] == ciaInfo.titleID) {
			if (ciaInfo.titleID >> 32 & 0xFFFF) AM_DeleteTitle(mediatype, ciaInfo.titleID);
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
	u32* bytesWritten = 0;
	for (i = 0; i < blockAmount; i++) {
		fread(block, 1, 0x160000, file);
		FSFILE_Write(ciaHandle, bytesWritten, 0x160000*i, block, 0x160000, 0);
	}

	if (size % 0x160000 != 0) {
		fread(block, 1, size-0x160000*blockAmount, file);
		FSFILE_Write(ciaHandle, bytesWritten, 0x160000*blockAmount, block, size-0x160000*blockAmount, 0);
	}

	free(block);

	res = AM_FinishCiaInstall(mediatype, &ciaHandle);
	if (res != 0) return false;

	if (ciaInfo.titleID == 0x0004013800000002LL || ciaInfo.titleID == 0x0004013820000002LL) { // If you're installing NATIVE_FIRM
		AM_InstallFirm(ciaInfo.titleID);
	}

	return true;

}
