#include <iostream>
#include <thread>
#include <vector>
#include "lock_free_queue.hpp"
using namespace std;
using namespace StemCell;
struct Data {
    int64_t num;
};
int main() {
    LockFreeQueue<Data> test_queue;
    std::vector< std::thread > workers;
    size_t thread_num = 10;
    for(size_t i = 0;i < thread_num ;++i) {
        workers.emplace_back([=,&test_queue]{
              for (int64_t j = 0; j < 1e6+i; ++j) {
                  Data *data = new Data();
                  data->num = j * 100 + i;
                  // cout << "num:" << data->num << endl;
                  test_queue.push(data);
              }
        });
    }
    for(std::thread &worker: workers) {
        worker.join();
    }
    cout << "test_queue size:" << test_queue.size() << endl;
    cout << "test_queue stat_size:" << test_queue.stat_size() << endl;
    
    vector<int64_t> thread_stat(thread_num);
    for (;;) {
        Data* p = test_queue.pop();
        if (!p) break;
        int64_t num = p->num;
        // cout << "num:" << num << endl;
        int64_t thread_no = num % thread_num;
        thread_stat[thread_no] += 1;
        delete p;
    }
    for (int i : thread_stat) {
        cout << "add node number:\t" << i << endl;
    }
    return 0;
}
