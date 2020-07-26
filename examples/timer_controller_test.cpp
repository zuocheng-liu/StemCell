#include <iostream>
#include "timer_controller.h"
using namespace std;
using namespace StemCell;


int main() {
    try {
    TimerController tc;
    tc.init();
    tc.delayProcess(1000 * 2, []() {
            cout << "sleep 2 sec" << endl;
            });
    tc.delayProcess(1000 * 3, []() {
            cout << "sleep 3 sec" << endl;
            });
    sleep(10);
    cout << "sleep 10 sec, complate!" << endl;
    return 0;
    } catch (exception& e) {
        cout << "error:" << e.what();
    }
}
