#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "ntp.h"

bool setsysInternetTimeSyncIsOn() {
    Result rs = setsysInitialize();
    if (R_FAILED(rs)) {
        printf("setsysInitialize failed, %x\n", rs);
        return false;
    }

    bool internetTimeSyncIsOn;
    rs = setsysIsUserSystemClockAutomaticCorrectionEnabled(&internetTimeSyncIsOn);
    setsysExit();
    if (R_FAILED(rs)) {
        printf("Unable to detect if Internet time sync is enabled, %x\n", rs);
        return false;
    }

    return internetTimeSyncIsOn;
}

Result enableSetsysInternetTimeSync() {
    Result rs = setsysInitialize();
    if (R_FAILED(rs)) {
        printf("setsysInitialize failed, %x\n", rs);
        return rs;
    }

    rs = setsysSetUserSystemClockAutomaticCorrectionEnabled(true);
    setsysExit();
    if (R_FAILED(rs)) {
        printf("Unable to enable Internet time sync: %x\n", rs);
    }

    return rs;
}

/*

Type   | SYNC | User | Local | Network
=======|======|======|=======|========
User   |      |      |       |
-------+------+------+-------+--------
Menu   |      |  *   |   X   |
-------+------+------+-------+--------
System |      |      |       |   X
-------+------+------+-------+--------
User   |  ON  |      |       |
-------+------+------+-------+--------
Menu   |  ON  |      |       |
-------+------+------+-------+--------
System |  ON  |  *   |   *   |   X

*/
TimeServiceType __nx_time_service_type = TimeServiceType_System;
bool setNetworkSystemClock(time_t time) {
    Result rs = timeSetCurrentTime(TimeType_NetworkSystemClock, (uint64_t)time);
    if (R_FAILED(rs)) {
        printf("timeSetCurrentTime failed with %x\n", rs);
        return false;
    }
    printf("Successfully set NetworkSystemClock.\n");
    return true;
}

int consoleExitWithMsg(char* msg) {
    printf("%s\n\nPress + to quit...", msg);

    while (appletMainLoop()) {
        hidScanInput();
        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        if (kDown & KEY_PLUS) {
            consoleExit(NULL);
            return 0;  // return to hbmenu
        }

        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}

bool toggleHBMenuPath(char* curPath) {
    const char* HB_MENU_NRO_PATH = "sdmc:/hbmenu.nro";
    const char* HB_MENU_BAK_PATH = "sdmc:/hbmenu.nro.bak";
    const char* DEFAULT_RESTORE_PATH = "sdmc:/switch/switch-time.nro";

    printf("\n\n");

    Result rs;
    if (strcmp(curPath, HB_MENU_NRO_PATH) == 0) {
        // restore hbmenu
        rs = access(HB_MENU_BAK_PATH, F_OK);
        if (R_FAILED(rs)) {
            printf("could not find %s to restore. failed: 0x%x", HB_MENU_BAK_PATH, rs);
            consoleExitWithMsg("");
            return false;
        }

        rs = rename(curPath, DEFAULT_RESTORE_PATH);
        if (R_FAILED(rs)) {
            printf("fsFsRenameFile(%s, %s) failed: 0x%x", curPath, DEFAULT_RESTORE_PATH, rs);
            consoleExitWithMsg("");
            return false;
        }
        rs = rename(HB_MENU_BAK_PATH, HB_MENU_NRO_PATH);
        if (R_FAILED(rs)) {
            printf("fsFsRenameFile(%s, %s) failed: 0x%x", HB_MENU_BAK_PATH, HB_MENU_NRO_PATH, rs);
            consoleExitWithMsg("");
            return false;
        }
    } else {
        // replace hbmenu
        rs = rename(HB_MENU_NRO_PATH, HB_MENU_BAK_PATH);
        if (R_FAILED(rs)) {
            printf("fsFsRenameFile(%s, %s) failed: 0x%x", HB_MENU_NRO_PATH, HB_MENU_BAK_PATH, rs);
            consoleExitWithMsg("");
            return false;
        }
        rs = rename(curPath, HB_MENU_NRO_PATH);
        if (R_FAILED(rs)) {
            printf("fsFsRenameFile(%s, %s) failed: 0x%x", curPath, HB_MENU_NRO_PATH, rs);
            rename(HB_MENU_BAK_PATH, HB_MENU_NRO_PATH);  // hbmenu already moved, try to move it back
            consoleExitWithMsg("");
            return false;
        }
    }

    printf("Quick launch toggled\n\n");

    return true;
}

int main(int argc, char* argv[]) {
    consoleInit(NULL);
    printf("SwitchTime v0.1.1\n\n");

    if (!setsysInternetTimeSyncIsOn()) {
        // printf("Trying setsysSetUserSystemClockAutomaticCorrectionEnabled...\n");
        // if (R_FAILED(enableSetsysInternetTimeSync())) {
        //     return consoleExitWithMsg("Internet time sync is not enabled. Please enable it in System Settings.");
        // }
        // doesn't work without rebooting? not worth it
        return consoleExitWithMsg("Internet time sync is not enabled. Please enable it in System Settings.");
    }

    // Main loop
    while (appletMainLoop()) {
        printf(
            "\n\n"
            "Press:\n\n"
            "UP/DOWN to change hour | LEFT/RIGHT to change day\n"
            "A to confirm time      | Y to reset to current time (Cloudflare time server)\n"
            "                       | + to quit\n\n\n");

        int dayChange = 0, hourChange = 0;
        while (appletMainLoop()) {
            hidScanInput();
            u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

            if (kDown & KEY_PLUS) {
                consoleExit(NULL);
                return 0;  // return to hbmenu
            }
            if (kDown & KEY_L) {
                if (!toggleHBMenuPath(argv[0])) {
                    return 0;
                }
            }

            time_t currentTime;
            Result rs = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&currentTime);
            if (R_FAILED(rs)) {
                printf("timeGetCurrentTime failed with %x", rs);
                return consoleExitWithMsg("");
            }

            struct tm* p_tm_timeToSet = localtime(&currentTime);
            p_tm_timeToSet->tm_mday += dayChange;
            p_tm_timeToSet->tm_hour += hourChange;
            time_t timeToSet = mktime(p_tm_timeToSet);

            if (kDown & KEY_A) {
                printf("\n\n\n");
                setNetworkSystemClock(timeToSet);
                break;
            }

            if (kDown & KEY_Y) {
                printf("\n\n\n");
                rs = ntpGetTime(&timeToSet);
                if (R_SUCCEEDED(rs)) {
                    setNetworkSystemClock(timeToSet);
                }
                break;
            }

            if (kDown & KEY_LEFT) {
                dayChange--;
            } else if (kDown & KEY_RIGHT) {
                dayChange++;
            } else if (kDown & KEY_DOWN) {
                hourChange--;
            } else if (kDown & KEY_UP) {
                hourChange++;
            }

            char timeToSetStr[25];
            strftime(timeToSetStr, sizeof timeToSetStr, "%c", p_tm_timeToSet);
            printf("\rTime to set: %s", timeToSetStr);
            consoleUpdate(NULL);
        }
    }
}