#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include "C:/Program Files (x86)/LabJack/Drivers/LabJackUD.h"
#include "constants.h"

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
    bool sigTerminateThreads = false;

    SensorHandlerVals sensVals = { &lj_handle, calloc(1, sizeof(bool)), &sigTerminateThreads };
    threadHandles[0] = CreateThread(NULL, 0, handleRollingBallSensor, &sensVals, 0, NULL);

    ButtonHandlerVals btnVals = { &lj_handle, calloc(1, sizeof(bool)), &sigTerminateThreads };
    threadHandles[1] = CreateThread(NULL, 0, handleModeSwitch, &btnVals, 0, NULL);

    // PWM timer setup
    AddRequest(lj_handle, LJ_ioPUT_CONFIG, LJ_chNUMBER_TIMERS_ENABLED, 1, 0, 0);
    AddRequest(lj_handle, LJ_ioPUT_CONFIG, LJ_chTIMER_COUNTER_PIN_OFFSET, PIN_SERVO, 0, 0);
    AddRequest(lj_handle, LJ_ioPUT_CONFIG, LJ_chTIMER_CLOCK_BASE, LJ_tc1MHZ_DIV, 0, 0);
    AddRequest(lj_handle, LJ_ioPUT_CONFIG, LJ_chTIMER_CLOCK_DIVISOR, 78, 0, 0);
    AddRequest(lj_handle, LJ_ioPUT_TIMER_MODE, 0, LJ_tmPWM8, 0, 0);
    AddRequest(lj_handle, LJ_ioPUT_TIMER_VALUE, 0, 59500, 0, 0);
    Go();

    for (int i = 0; i < 500; ++i) {
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

        Sleep(THREAD_SLEEP_MS);
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
    double btnPrevState = 0;

    while (!*vals->sigTerminate) {
        AddRequest(*vals->ljHandle, LJ_ioGET_DIGITAL_BIT, PIN_BTN, 0, 0, 0);
        Go();
        GetResult(*vals->ljHandle, LJ_ioGET_DIGITAL_BIT, PIN_BTN, &btn_pdEIO5);

        if (btn_pdEIO5 == 1 && ((int) btn_pdEIO5 ^ (int) btnPrevState)) {
            *vals->mode = !*vals->mode;

            if (*vals->mode) {
                AddRequest(*vals->ljHandle, LJ_ioPUT_TIMER_VALUE, 0, 59500, 0, 0);
                Go();
            } else {
                AddRequest(*vals->ljHandle, LJ_ioPUT_TIMER_VALUE, 0, 62300, 0, 0);
                Go();
            }

            // 500ms timeout on mode switching to prevent bouncing and let the servo finish a previous move
            Sleep(750);
        }
        btnPrevState = btn_pdEIO5;

        Sleep(THREAD_SLEEP_MS);
    }

    return 0;
}