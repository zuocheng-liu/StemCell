#include <iostream>
#include "timer_controller.h"
using namespace std;
using namespace StemCell;


int main() {
    srand((unsigned)time(NULL));
    try {
        TimerController tc;
        tc.init();
        for (int i = 0; i < 1000; ++i) {
            auto seed = rand() % 8000;
            tc.delayProcess(seed, [=]() { cout << "seed:" << seed << endl; });
        }
        
        
        sleep(10);
        tc.stop();
        sleep(1);
        cout << "tc stoped!" << endl;
    } catch (exception& e) {
        cout << "error:" << e.what();
    }
    return 0;
}
