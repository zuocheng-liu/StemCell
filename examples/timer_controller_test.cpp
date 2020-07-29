#include <iostream>
#include "timer_controller.h"
using namespace std;
using namespace StemCell;


int main() {
    srand((unsigned)time(NULL));
    try {
        TimerController tc;
        tc.init();
        tc.cycleProcess(1000, [=]() { cout << "cycle 1 sec" << endl; });
        tc.cycleProcess(500, [=]() { cout << "cycle 0.5 sec" << endl; });
        tc.cycleProcess(100, [=]() { cout << "cycle 0.1 sec" << endl; });
        for (int i = 0; i < 80; ++i) {
            auto seed = rand() % 8000;
            tc.delayProcess(seed + 1, [=]() { cout << "delay:" << seed << endl; });
        }
        sleep(8);
        tc.stop();
        cout << "tc stoped!" << endl;
        sleep(1);
    } catch (exception& e) {
        cout << "error:" << e.what();
    }
    return 0;
}
