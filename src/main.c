#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <time.h>
#include "LabJackUD.h"
#include "main.h"

#include <corecrt_share.h>

int main() {
    LJ_HANDLE ljHandle = 0;
    LJ_ERROR ljErr = 0;
    ljErr = OpenLabJack(LJ_dtU3, LJ_ctUSB, "1", 1, &ljHandle);

    if (ljErr == LJE_DEVICE_ALREADY_OPEN) {
        printf("LabJack in use! Exiting\n");
        fflush(stdout);
        return 1;
    }

    if (ljHandle == 0) {
        printf("Invalid LabJack handle received. Exiting\n");
        fflush(stdout);
        return 1;
    }

    ePut(ljHandle, LJ_ioPIN_CONFIGURATION_RESET, 0, 0, 0);

    // Check if the output file exists and ask the user what they want to do
    char mode[2] = { 'w' , '\0'};
    if (outputFileExists()) {
        printf("Output file already exists. Enter 1 to append or 2 to overwrite: ");
        fflush(stdout);

        char input[2];
        scanf_s("%s", input, _countof(input));
            if (strcmp(input, "1") == 0) {
                mode[0] = 'a';
            }
    }

    // Initial upload of HTML and JS files
    system("cmd.exe /C winscp.com /script=\"scripts\\init_upload.txt\" >NUL 2>&1");

    // Spawn threads for input and output handling (button, tilt sensor, console input, file uploading)
    HANDLE threadHandles[4];
    bool sigTerminateThreads = false;
    TiltSensorHandlerVals sensHandlerVals = { &ljHandle, calloc(1, sizeof(bool)), &sigTerminateThreads };
    threadHandles[0] = CreateThread(NULL, 0, handleRollingBallSensor, &sensHandlerVals, 0, NULL);
    ModeSwitchButtonHandlerVals btnHandlerVals = { &ljHandle, calloc(1, sizeof(bool)), calloc(1, sizeof(bool)), &sigTerminateThreads };
    threadHandles[1] = CreateThread(NULL, 0, handleModeSwitch, &btnHandlerVals, 0, NULL);
    ConsoleInputHandlerVals consoleInputHandlerVals = { &sigTerminateThreads };
    threadHandles[2] = CreateThread(NULL, 0, handleConsoleInput, &consoleInputHandlerVals, 0, NULL);
    FileUploadHandlerVals fileUploadHandlerVals = { &sigTerminateThreads };
    threadHandles[3] = CreateThread(NULL, 0, handleFileUpload, &fileUploadHandlerVals, 0, NULL);

    // PWM timer setup
    setupPWM(ljHandle, PIN_SERVO, LJ_tmPWM8, LJ_tc1MHZ_DIV, 78, SERVO_LEFT_POS_VALUE);

    // Set the initial display state to blank
    setDisplayState(ljHandle, 15);

    // Try to open the output file
    FILE* fp = _fsopen(OUTPUT_FILE_NAME, mode, _SH_DENYWR);
    if (fp == NULL) {
        printf("Could not open output file `%s`. Exiting.\n", OUTPUT_FILE_NAME);
        fflush(stdout);
        return 2;
    }
    if (mode[0] == 'w') fprintf_s(fp, "year,month,day,hour,minute,second,mode,value");

    // Main program loop
    programLoop(ljHandle, fp, &sigTerminateThreads, btnHandlerVals.mode, sensHandlerVals.rbSensorState, btnHandlerVals.canChangeMode);

    // End the program and threads gracefully
    sigTerminateThreads = true;

    setDisplayState(ljHandle, 15);
    AddRequest(ljHandle, LJ_ioPUT_TIMER_VALUE, 0, SERVO_MIDDLE_POS_VALUE, 0, 0);
    Go();

    WaitForMultipleObjects(ARRAYSIZE(threadHandles), threadHandles, TRUE, INFINITE);
    CloseHandle(threadHandles[0]);
    CloseHandle(threadHandles[1]);
    CloseHandle(threadHandles[2]);
    CloseHandle(threadHandles[3]);

    free(sensHandlerVals.rbSensorState);
    free(btnHandlerVals.mode);
    free(btnHandlerVals.canChangeMode);
    fclose(fp);
    Close();
    return 0;
}

void programLoop(const LJ_HANDLE ljHandle, FILE* fp, const bool *sigTerminate, const bool *mode, const bool *isTilted, bool *canChangeMode) {
    bool wasTilted = false;
    bool capturedMode;
    int value;
    srand(time(NULL));

    while (!*sigTerminate) {
        if (*isTilted == true && *isTilted ^ wasTilted) {
            *canChangeMode = false; //lock out the mode change until the display is clear again
            if (*mode == DIE_MODE) {
                capturedMode = DIE_MODE;
                value = rand() % 6 + 1;
            } else {
                capturedMode = COIN_MODE;
                value = rand() % 2 + 1;
            }

            animate(ljHandle, rand() % (ANIMATE_MAX - ANIMATE_MIN + 1) + ANIMATE_MIN);
            setDisplayState(ljHandle, value);
            writeValueToFile(fp, capturedMode, value);
            Sleep(DISPLAY_VALUE_SLEEP_MS);
            setDisplayState(ljHandle, 15);
            *canChangeMode = true;
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
void writeValueToFile(FILE *fp, const bool mode, const int value) {
    time_t curTime;
    time(&curTime);
    struct tm localtime;
    localtime_s(&localtime, &curTime);

    // YYYY,MM,dd,HH,mm,ss,mode,value
    fprintf_s(fp, "\n%d,%d,%d,%d,%d,%d,%s,%d", localtime.tm_year + 1900,
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

        if (*vals->canChangeMode && btn_pdEIO5 == 1 && ((int) btn_pdEIO5 ^ (int) btnPrevState)) {
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
        fflush(stdout);
        scanf_s("%s", input, _countof(input));
        if (strcmp(input, "exit") == 0) {
            *vals->sigTerminate = true;
        }
    }

    return 0;
}

DWORD WINAPI handleFileUpload(LPVOID lpParam) {
    FileUploadHandlerVals *vals = lpParam;
    int counter = 0;

    while (!*vals->sigTerminate) {
        Sleep(THREAD_SLEEP_MS);
        if (counter % ((FILE_UPLOAD_INTERVAL_SECONDS * 1000) / THREAD_SLEEP_MS) == 0) {
            system("cmd.exe /C winscp.com /script=\"scripts\\data_upload.txt\" >NUL 2>&1");
        }
        counter++;
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

    fclose(fp);
    return true;
}

void setupPWM(long ljHandle, long pinOffset, long timerMode, long timerClockBase, long timerClockDiv, int initialValue) {
    // PWM timer setup
    AddRequest(ljHandle, LJ_ioPUT_CONFIG, LJ_chNUMBER_TIMERS_ENABLED, 1, 0, 0);
    AddRequest(ljHandle, LJ_ioPUT_CONFIG, LJ_chTIMER_COUNTER_PIN_OFFSET, pinOffset, 0, 0);
    AddRequest(ljHandle, LJ_ioPUT_CONFIG, LJ_chTIMER_CLOCK_BASE, timerClockBase, 0, 0);
    AddRequest(ljHandle, LJ_ioPUT_CONFIG, LJ_chTIMER_CLOCK_DIVISOR, timerClockDiv, 0, 0);
    AddRequest(ljHandle, LJ_ioPUT_TIMER_MODE, 0, timerMode, 0, 0);
    AddRequest(ljHandle, LJ_ioPUT_TIMER_VALUE, 0, initialValue, 0, 0);
    Go();
}