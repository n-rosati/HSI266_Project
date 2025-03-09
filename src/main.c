#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include "C:/Program Files (x86)/LabJack/Drivers/LabJackUD.h"

DWORD WINAPI handleRollingBallSensor(LPVOID lpParam);
DWORD WINAPI handleModeSwitch(LPVOID lpParam);
void freeAll(int count, ...);

typedef struct SensorHandlerVals {
    LJ_HANDLE lj_handle;
    bool *rbSensorState;
    bool *sig_terminate;
} SensorHandlerVals;

typedef struct ButtonHandlerVals {
    LJ_HANDLE lj_handle;
    bool *mode;
    bool *sig_terminate;
} ButtonHandlerVals;

int main(int argc, char **argv) {
    LJ_HANDLE lj_handle = 0;
    OpenLabJack(LJ_dtU3, LJ_ctUSB, "1", 1, &lj_handle);
    ePut(lj_handle, LJ_ioPIN_CONFIGURATION_RESET, 0, 0, 0);

    HANDLE handles[2];

    SensorHandlerVals sensVals;
    sensVals.lj_handle = lj_handle;
    sensVals.rbSensorState = calloc(1, sizeof(bool));
    sensVals.sig_terminate = calloc(1, sizeof(bool));
    handles[0] = CreateThread(NULL, 0, handleRollingBallSensor, (LPVOID) (&sensVals), 0, NULL);

    ButtonHandlerVals btnVals;
    btnVals.lj_handle = lj_handle;
    btnVals.mode = calloc(1, sizeof(bool));
    btnVals.sig_terminate = calloc(1, sizeof(bool));
    handles[1] = CreateThread(NULL, 0, handleModeSwitch, (LPVOID) (&btnVals), 0, NULL);

    for (int i = 0; i < 100; ++i) {
        printf("%d", *btnVals.mode);
        Sleep(100);
    }

    *sensVals.sig_terminate = true;
    *btnVals.sig_terminate = true;

    WaitForMultipleObjects(2, handles, TRUE, INFINITE);
    CloseHandle(handles[0]);
    CloseHandle(handles[1]);
    freeAll(4, sensVals.sig_terminate, sensVals.rbSensorState, btnVals.mode, btnVals.sig_terminate);
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

            // 3s timeout on rolling to prevent accidental activations
            Sleep(3000);

            *vals->rbSensorState = false;
        }

        Sleep(50);
    }

    return 0;
}

/**
 * Toggles the device mode when the push button is pressed
 * @param lpParam Parameter. Should be a ButtonHandlerVals struct pointer
 * @return
 */
DWORD WINAPI handleModeSwitch(LPVOID lpParam) {
    ButtonHandlerVals *vals = lpParam;

    double btn_pdEIO5 = 0;

    while (!*vals->sig_terminate) {
        AddRequest(vals->lj_handle, LJ_ioGET_DIGITAL_BIT, 13, 0, 0, 0);
        Go();
        GetResult(vals->lj_handle, LJ_ioGET_DIGITAL_BIT, 13, &btn_pdEIO5);

        if (btn_pdEIO5) {
            *vals->mode = !*vals->mode;

            // 500ms timeout on mode switching to prevent bouncing
            Sleep(500);
        }

        Sleep(50);
    }

    return 0;
}