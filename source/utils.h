#pragma once
// KernelTimeMachine
// Safe CIA manager
// Licensed under GNU General Public License (GPL)
// Check out https://www.gnu.org/licenses/gpl.html

void clearScreens();
int error(char *msg, u8 errorCode);

extern u8* fbTopLeft;
extern u8* fbTopRight;
extern u8* fbBottom;
