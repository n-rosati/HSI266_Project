#ifndef MAIN_H
#define MAIN_H

#define PIN_SERVO   8
#define PIN_A       17
#define PIN_B       10
#define PIN_C       12
#define PIN_D       14
#define PIN_BTN     13
#define PIN_RB_SENS 15

#define SERVO_LEFT_POS_VALUE    60500
#define SERVO_MIDDLE_POS_VALUE  58000
#define SERVO_RIGHT_POS_VALUE   61750

#define DIE_MODE    false
#define COIN_MODE   true

#define ANIMATE_MIN 5
#define ANIMATE_MAX 10

#define THREAD_SLEEP_MS                 50
#define DISPLAY_VALUE_SLEEP_MS          5000
#define FILE_UPLOAD_INTERVAL_SECONDS    15

#define OUTPUT_FILE_NAME "data.csv"

typedef struct TiltSensorHandlerVals {
    LJ_HANDLE *ljHandle;
    bool *rbSensorState;
    bool *sigTerminate;
} TiltSensorHandlerVals;

typedef struct ModeSwitchButtonHandlerVals {
    LJ_HANDLE *ljHandle;
    bool *mode;
    bool *canChangeMode;
    bool *sigTerminate;
} ModeSwitchButtonHandlerVals;

typedef struct ConsoleInputHandlerVals {
    bool *sigTerminate;
} ConsoleInputHandlerVals;

typedef struct FileUploadHandlerVals {
    bool *sigTerminate;
} FileUploadHandlerVals;

DWORD WINAPI handleRollingBallSensor(LPVOID lpParam);
DWORD WINAPI handleModeSwitch(LPVOID lpParam);
DWORD WINAPI handleConsoleInput(LPVOID lpParam);
DWORD WINAPI handleFileUpload(LPVOID lpParam);
void programLoop(LJ_HANDLE ljHandle, FILE* fp, const bool *sigTerminate, const bool *mode, const bool *isTilted, bool *canChangeMode);
void writeValueToFile(FILE *fp, bool mode, int value);
void setDisplayState(LJ_HANDLE ljHandle, int state);
void animate(LJ_HANDLE ljHandle, int numLoops);
bool doesUserWantToExit();
bool outputFileExists();
void setupPWM(long ljHandle, long pinOffset, long timerMode, long timerClockBase, long timerClockDiv, int initialValue);

#endif //MAIN_H
