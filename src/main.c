#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <time.h>
#include "C:/Program Files (x86)/LabJack/Drivers/LabJackUD.h"
#include "main.h"

#include <corecrt_share.h>

int main() {
    LJ_HANDLE ljHandle = 0;
    LJ_ERROR ljErr = 0;
    ljErr = OpenLabJack(LJ_dtU3, LJ_ctUSB, "1", 1, &ljHandle);

    if (ljErr == LJE_DEVICE_ALREADY_OPEN) {
        printf("LabJack in use! Exiting\n");
        return 0;
    }

    if (ljHandle == 0) {
        printf("Invalid LabJack handle received. Exiting\n");
        return 0;
    }

    ePut(ljHandle, LJ_ioPIN_CONFIGURATION_RESET, 0, 0, 0);

    // Check if the output file exists and ask the user what they want to do
    char mode[2] = { 'w' , '\0'};
    if (outputFileExists()) {
        printf("Output file already exists. Enter 1 to append or 2 to overwrite: ");

        char input[2];
        scanf_s("%s", input, _countof(input));
            if (strcmp(input, "1") == 0) {
                mode[0] = 'a';
            }
    }

    // Spawn threads for input handling (button, tilt sensor, console)
    HANDLE threadHandles[3];
    bool sigTerminateThreads = false;
    TiltSensorHandlerVals sensHandlerVals = { &ljHandle, calloc(1, sizeof(bool)), &sigTerminateThreads };
    threadHandles[0] = CreateThread(NULL, 0, handleRollingBallSensor, &sensHandlerVals, 0, NULL);
    ModeSwitchButtonHandlerVals btnHandlerVals = { &ljHandle, calloc(1, sizeof(bool)), &sigTerminateThreads };
    threadHandles[1] = CreateThread(NULL, 0, handleModeSwitch, &btnHandlerVals, 0, NULL);
    ConsoleInputHandlerVals consoleInputHandlerVals = { &sigTerminateThreads };
    threadHandles[2] = CreateThread(NULL, 0, handleConsoleInput, &consoleInputHandlerVals, 0, NULL);

    // PWM timer setup
    AddRequest(ljHandle, LJ_ioPUT_CONFIG, LJ_chNUMBER_TIMERS_ENABLED, 1, 0, 0);
    AddRequest(ljHandle, LJ_ioPUT_CONFIG, LJ_chTIMER_COUNTER_PIN_OFFSET, PIN_SERVO, 0, 0);
    AddRequest(ljHandle, LJ_ioPUT_CONFIG, LJ_chTIMER_CLOCK_BASE, LJ_tc1MHZ_DIV, 0, 0);
    AddRequest(ljHandle, LJ_ioPUT_CONFIG, LJ_chTIMER_CLOCK_DIVISOR, 78, 0, 0);
    AddRequest(ljHandle, LJ_ioPUT_TIMER_MODE, 0, LJ_tmPWM8, 0, 0);
    AddRequest(ljHandle, LJ_ioPUT_TIMER_VALUE, 0, SERVO_LEFT_POS_VALUE, 0, 0);
    Go();

    // Set the initial display state to blank
    setDisplayState(ljHandle, 15);

    // Main program logic loop
    FILE* fp = _fsopen(OUTPUT_FILE_NAME, mode, _SH_DENYWR);
    if (mode[0] == 'w') fprintf_s(fp, "year,month,day,hour,minute,second,mode,value\n");
    srand(time(NULL));
    programLoop(ljHandle, fp, &sigTerminateThreads, btnHandlerVals.mode, sensHandlerVals.rbSensorState);
    fclose(fp);

    // End the program gracefully
    sigTerminateThreads = true;

    setDisplayState(ljHandle, 15);
    AddRequest(ljHandle, LJ_ioPUT_TIMER_VALUE, 0, SERVO_MIDDLE_POS_VALUE, 0, 0);
    Go();

    WaitForMultipleObjects(ARRAYSIZE(threadHandles), threadHandles, TRUE, INFINITE);
    CloseHandle(threadHandles[0]);
    CloseHandle(threadHandles[1]);
    CloseHandle(threadHandles[2]);

    freeAll(2, sensHandlerVals.rbSensorState, btnHandlerVals.mode);
    Close();
    return 0;
}

void programLoop(const LJ_HANDLE ljHandle, FILE* fp, const bool *sigTerminate, const bool *mode, const bool *isTilted) {
    bool wasTilted = false;
    int value;

    while (!*sigTerminate) {
        if (*isTilted == true && *isTilted ^ wasTilted) {
            if (*mode == DIE_MODE) {
                value = rand() % 6 + 1;
            } else if (*mode == COIN_MODE) {
                value = rand() % 2 + 1;
            }

            animate(ljHandle, rand() % (ANIMATE_MAX - ANIMATE_MIN + 1) + ANIMATE_MIN);
            setDisplayState(ljHandle, value);
            writeValueToFile(fp, *mode, value);
            Sleep(DISPLAY_VALUE_SLEEP_MS);
            setDisplayState(ljHandle, 15);
        }

        wasTilted = *isTilted;
        Sleep(50);
    }
}

/**
 * Writes the current date, time, rolling mode, and value to a file
 * @param fp File pointer to write to
 * @param mode Current rolling mode. Should be DIE_MODE or COIN_MODE
 * @param value Value rolled
 */
void writeValueToFile(const FILE* fp, const bool mode, const int value) {
    time_t curTime;
    time(&curTime);
    struct tm localtime;
    localtime_s(&localtime, &curTime);

    fprintf_s(fp, "%d,%d,%d,%d,%d,%d,%s,%d\n", localtime.tm_year + 1900,
                                                     localtime.tm_mon + 1,
                                                     localtime.tm_mday,
                                                     localtime.tm_hour,
                                                     localtime.tm_min,
                                                     localtime.tm_sec,
                                                     mode == DIE_MODE ? "die" : "coin",
                                                     value);
    fflush(fp);
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
    TiltSensorHandlerVals *vals = lpParam;

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
    ModeSwitchButtonHandlerVals *vals = lpParam;

    double btn_pdEIO5 = 0;
    double btnPrevState = 0;

    while (!*vals->sigTerminate) {
        AddRequest(*vals->ljHandle, LJ_ioGET_DIGITAL_BIT, PIN_BTN, 0, 0, 0);
        Go();
        GetResult(*vals->ljHandle, LJ_ioGET_DIGITAL_BIT, PIN_BTN, &btn_pdEIO5);

        if (btn_pdEIO5 == 1 && ((int) btn_pdEIO5 ^ (int) btnPrevState)) {
            *vals->mode = !*vals->mode;

            // Move the motor to point at the new mode
            if (*vals->mode) {
                AddRequest(*vals->ljHandle, LJ_ioPUT_TIMER_VALUE, 0, SERVO_RIGHT_POS_VALUE, 0, 0);
            } else {
                AddRequest(*vals->ljHandle, LJ_ioPUT_TIMER_VALUE, 0, SERVO_LEFT_POS_VALUE, 0, 0);
            }
            Go();

            // Timeout on mode switching to ignore bouncing and let the servo finish prior move
            Sleep(750);
        }
        btnPrevState = btn_pdEIO5;

        Sleep(THREAD_SLEEP_MS);
    }

    return 0;
}

/**
 * Monitors the terminal for user input
 * @param lpParam Parameter. Should be a ConsoleInputHandler struct pointer
 * @return
 */
DWORD WINAPI handleConsoleInput(LPVOID lpParam) {
    ConsoleInputHandlerVals *vals = lpParam;

    char input[5];
    while (!*vals->sigTerminate) {
        printf("Enter 'exit' to end the program\n");
        scanf_s("%s", input, _countof(input));
        if (strcmp(input, "exit") == 0) {
            *vals->sigTerminate = true;
        }
    }

    return 0;
}

/**
 * Sets outputs on pins A B C D (defined in main.h) to the associated state number as per the datasheet of the 74LS47
 * @param ljHandle
 * @param state State number to set
 */
void setDisplayState(LJ_HANDLE ljHandle, int state) {
    switch (state) {
        case 0:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 0, 0, 0);
            break;
        case 1:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 0, 0, 0);
            break;
        case 2:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 0, 0, 0);
            break;
        case 3:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 0, 0, 0);
            break;
        case 4:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 0, 0, 0);
            break;
        case 5:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 0, 0, 0);
            break;
        case 6:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 0, 0, 0);
            break;
        case 7:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 0, 0, 0);
            break;
        case 8:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 1, 0, 0);
            break;
        case 9:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 1, 0, 0);
            break;
        case 10:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 1, 0, 0);
            break;
        case 11:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 1, 0, 0);
            break;
        case 12:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 1, 0, 0);
            break;
        case 13:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 1, 0, 0);
            break;
        case 14:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 0, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 1, 0, 0);
            break;
        default:
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_A, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_B, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_C, 1, 0, 0);
            AddRequest(ljHandle, LJ_ioPUT_DIGITAL_BIT, PIN_D, 1, 0, 0);
            break;
    }
    Go();
}

/**
 * Plays an animation a number of times on the display
 * @param ljHandle
 * @param numLoops Number of times to loop the animation
 */
void animate(LJ_HANDLE ljHandle, int numLoops) {
    for (int i = 0; i < numLoops; ++i) {
        setDisplayState(ljHandle, 11);
        Sleep(200);
        setDisplayState(ljHandle, 10);
        Sleep(200);
    }

    // Blank the display when done
    setDisplayState(ljHandle, 15);
}

bool doesUserWantToExit() {
    char input[5];
    scanf_s("%s", input, _countof(input));
    return strcmp(input, "exit") == 0 ? true : false;
}

bool outputFileExists() {
    FILE* fp;
    const errno_t err = fopen_s(&fp, OUTPUT_FILE_NAME, "r");

    if (err) return false;
    return true;
}