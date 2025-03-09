#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include "C:/Program Files (x86)/LabJack/Drivers/LabJackUD.h"

DWORD WINAPI handleRollingBallSensor(LPVOID lpParam);
void freeAll(int count, ...);

typedef struct SensorHandlerVals {
    LJ_HANDLE lj_handle;
    bool *rbSensorState;
    bool *sig_terminate;
} SensorHandlerVals;

int main(int argc, char **argv) {
    LJ_HANDLE lj_handle = 0;
    OpenLabJack(LJ_dtU3, LJ_ctUSB, "1", 1, &lj_handle);
    ePut(lj_handle, LJ_ioPIN_CONFIGURATION_RESET, 0, 0, 0);

    SensorHandlerVals vals;
    vals.lj_handle = lj_handle;
    vals.rbSensorState = calloc(1, sizeof(bool));
    vals.sig_terminate = calloc(1, sizeof(bool));

    HANDLE thread = CreateThread(NULL, 0, handleRollingBallSensor, (LPVOID) (&vals), 0, NULL);

    for (int i = 0; i < 100; ++i) {
        printf("%d", *vals.rbSensorState);
        Sleep(100);
    }

    *vals.sig_terminate = true;

    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    freeAll(2, vals.sig_terminate, vals.rbSensorState);
    Close();
    return 0;
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

/**
 * Sets the rbSensorState when the rolling ball sensor is triggered
 * @param lpParam Parameter. Should be a SensorHandlerVals struct pointer
 * @return
 */
DWORD WINAPI handleRollingBallSensor(LPVOID lpParam) {
    SensorHandlerVals *vals = lpParam;

    double rbSensor = 0;

    while (!*vals->sig_terminate) {
        AddRequest(vals->lj_handle, LJ_ioGET_DIGITAL_BIT, 15, 0, 0, 0);
        Go();
        GetResult(vals->lj_handle, LJ_ioGET_DIGITAL_BIT, 15, &rbSensor);

        if (rbSensor) {
            *vals->rbSensorState = true;
            Sleep(3000);
            *vals->rbSensorState = false;
        }

        Sleep(50);
    }

    return 0;
}