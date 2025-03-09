#include "inputs.h"

extern boolean *sig_terminate;
extern boolean *rollTrigd;

DWORD WINAPI handleRollingBallSensor(LPVOID lpParam) {
    while (!*sig_terminate) {
        *rollTrigd = !*rollTrigd;
        Sleep(1000);
    }

    return 0;
}