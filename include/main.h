#ifndef CONSTANTS_H
#define CONSTANTS_H

#define PIN_SERVO   8
#define PIN_A       17
#define PIN_B       10
#define PIN_C       12
#define PIN_D       14
#define PIN_BTN     13
#define PIN_RB_SENS 15

#define SERVO_LEFT_POS_VALUE 59500
#define SERVO_MIDDLE_POS_VALUE 60900
#define SERVO_RIGHT_POS_VALUE 62300

#define DIE_MODE false
#define COIN_MODE true

#define ANIMATE_MIN 5
#define ANIMATE_MAX 10

#define THREAD_SLEEP_MS 50
#define DISPLAY_VALUE_SLEEP_MS 5000

#define OUTPUT_FILE_NAME "output.txt"

typedef struct TiltSensorHandlerVals {
    LJ_HANDLE *ljHandle;
    bool *rbSensorState;
    bool *sigTerminate;
} TiltSensorHandlerVals;

typedef struct ModeSwitchButtonHandlerVals {
    LJ_HANDLE *ljHandle;
    bool *mode;
    bool *sigTerminate;
} ModeSwitchButtonHandlerVals;

typedef struct ConsoleInputHandlerVals {
    bool *sigTerminate;
} ConsoleInputHandlerVals;

DWORD WINAPI handleRollingBallSensor(LPVOID lpParam);
DWORD WINAPI handleModeSwitch(LPVOID lpParam);
DWORD WINAPI handleConsoleInput(LPVOID lpParam);
void programLoop(const LJ_HANDLE ljHandle, FILE* fp, const bool *sigTerminate, const bool *mode, const bool *isTilted);
void writeValueToFile(const FILE* fp, bool mode, const int value);
void freeAll(int count, ...);
void setDisplayState(LJ_HANDLE ljHandle, int state);
void animate(LJ_HANDLE ljHandle, int numLoops);
bool doesUserWantToExit();

#endif //CONSTANTS_H
