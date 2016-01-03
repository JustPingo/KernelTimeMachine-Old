// KernelTimeMachine
// Safe CIA manager
// Licensed under GNU General Public License (GPL)
// Check out https://www.gnu.org/licenses/gpl.html

void installTTPthread();
bool installTTP(char *path,u8 mediatype);
u32 checkTTP(char region,bool isNew,char *path);

extern Handle threadInstallHandle;
extern volatile bool isDone;
extern volatile u8 threadMediatype;
extern volatile char *threadPath;
