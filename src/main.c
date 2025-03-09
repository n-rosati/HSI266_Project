#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include "C:/Program Files (x86)/LabJack/Drivers/LabJackUD.h"

#define PIN_MOTOR   8
#define PIN_A       17
#define PIN_B       18
#define PIN_C       12
#define PIN_D       14
#define PIN_BTN     13
#define PIN_RB_SENS 15

DWORD WINAPI handleRollingBallSensor(LPVOID lpParam);
DWORD WINAPI handleModeSwitch(LPVOID lpParam);
void freeAll(int count, ...);

typedef struct SensorHandlerVals {
    LJ_HANDLE *ljHandle;
    bool *rbSensorState;
    bool *sigTerminate;
} SensorHandlerVals;

typedef struct ButtonHandlerVals {
    LJ_HANDLE *ljHandle;
    bool *mode;
    bool *sigTerminate;
} ButtonHandlerVals;

int main(int argc, char **argv) {
    LJ_HANDLE lj_handle = 0;
    OpenLabJack(LJ_dtU3, LJ_ctUSB, "1", 1, &lj_handle);
    ePut(lj_handle, LJ_ioPIN_CONFIGURATION_RESET, 0, 0, 0);

    HANDLE threadHandles[2];
    bool sigTerminateThreads;

    SensorHandlerVals sensVals = { &lj_handle, calloc(1, sizeof(bool)), &sigTerminateThreads };
    threadHandles[0] = CreateThread(NULL, 0, handleRollingBallSensor, (LPVOID) (&sensVals), 0, NULL);

    ButtonHandlerVals btnVals = { &lj_handle, calloc(1, sizeof(bool)), &sigTerminateThreads };
    threadHandles[1] = CreateThread(NULL, 0, handleModeSwitch, (LPVOID) (&btnVals), 0, NULL);

    for (int i = 0; i < 100; ++i) {
        printf("%d", *btnVals.mode);
        Sleep(100);
    }

    sigTerminateThreads = true;

    WaitForMultipleObjects(2, threadHandles, TRUE, INFINITE);
    CloseHandle(threadHandles[0]);
    CloseHandle(threadHandles[1]);
    freeAll(2, sensVals.rbSensorState, btnVals.mode);
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

    while (!*vals->sigTerminate) {
        AddRequest(*vals->ljHandle, LJ_ioGET_DIGITAL_BIT, PIN_RB_SENS, 0, 0, 0);
        Go();
        GetResult(*vals->ljHandle, LJ_ioGET_DIGITAL_BIT, PIN_RB_SENS, &rbSensor);

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

    while (!*vals->sigTerminate) {
        AddRequest(*vals->ljHandle, LJ_ioGET_DIGITAL_BIT, PIN_BTN, 0, 0, 0);
        Go();
        GetResult(*vals->ljHandle, LJ_ioGET_DIGITAL_BIT, PIN_BTN, &btn_pdEIO5);

        if (btn_pdEIO5) {
            *vals->mode = !*vals->mode;

            // 500ms timeout on mode switching to prevent bouncing and let the servo finish a previous move
            Sleep(500);
        }

        Sleep(50);
    }

    return 0;
}