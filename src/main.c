#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

#include "C:/Program Files (x86)/LabJack/Drivers/LabJackUD.h"
#include "inputs.h"
#include "outputs.h"
#include "helper.h"

BOOL WINAPI consoleCtrlHandler(DWORD signal);
void cleanup();
void freeAll(int count, ...);

HANDLE thread;
boolean *rollTrigd;
boolean *sig_terminate;

int main(int argc, char **argv) {
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);

    LJ_HANDLE lj_handle = 0;
    double ljSN = 0;

    OpenLabJack(LJ_dtU3, LJ_ctUSB, "1", 1, &lj_handle);
    ePut(lj_handle, LJ_ioPIN_CONFIGURATION_RESET, 0, 0, 0);

    eGet(lj_handle, LJ_ioGET_CONFIG, LJ_chSERIAL_NUMBER, &ljSN, 0);
    printf("Serial number: %.0lf\n", ljSN);

    rollTrigd = calloc(1, sizeof(boolean));
    sig_terminate = calloc(1, sizeof(boolean));

    DWORD threadID;
    thread = CreateThread(NULL, 0, handleRollingBallSensor, NULL, 0, &threadID);

    for (int i = 0; i < 20; ++i) {
        printf("%d\t", *rollTrigd);
        Sleep(500);
    }

    *sig_terminate = true;

    cleanup();
    return 0;
}

/**
 * Exit gracefully when quitting
 */
BOOL WINAPI consoleCtrlHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        cleanup();
    }

    return TRUE;
}

/**
 * Cleans up the program before quitting
 */
void cleanup() {
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    freeAll(2, rollTrigd, sig_terminate);
    Close();
}

/**
 * Frees the given list of pointers
 * @param count Number of items being passed
 * @param ... Pointers to things to be free'd
 */
void freeAll(int count, ...) {
    va_list args;
    va_start(args, count);

    for (int i = 0; i < count; ++i) {
        void* ptr = va_arg(args, void *);
        if (ptr != NULL) free(ptr);
    }

    va_end(args);
}