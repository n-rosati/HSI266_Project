#ifndef CONSTANTS_H
#define CONSTANTS_H

#define PIN_SERVO   8
#define PIN_A       17
#define PIN_B       10
#define PIN_C       12
#define PIN_D       14
#define PIN_BTN     13
#define PIN_RB_SENS 15

#define THREAD_SLEEP_MS 50

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
void programLoop();
void freeAll(int count, ...);
void setDisplayState(LJ_HANDLE ljHandle, int state);
void animate(LJ_HANDLE ljHandle, int numLoops);
bool doesUserWantToExit();

#endif //CONSTANTS_H
