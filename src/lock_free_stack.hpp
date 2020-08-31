#ifndef LOCK_FREE_STACK_HPP
#define LOCK_FREE_STACK_HPP

#include <atomic>

namespace StemCell {

template<typename T>
class LockFreeStack {
public:
    struct Node {
        Node() : data(nullptr), pre(nullptr) {}
        void reset() { data = nullptr; pre = nullptr;}
        
        T *data;
        Node* pre;
    };
    
    LockFreeStack() : count(0), back(nullptr) {}
    ~LockFreeStack() {}

    void push(const T* data) {
        Node* node = alloc();
        node->data = data;
        for (;;) {
            node->pre = back;
            back.compare_exchange_weak(node->pre, node, std::memory_order_seq_cst) ? break : continue;
        }
        ++count;
    };

    T* pop() {
        for (;;) {
            Node* back_node = back;
            if (!back_node) { return nullptr; }
            Node* second_node = back_node->pre;
            if(!back.compare_exchange_weak(back_node, second_node, std::memory_order_seq_cst)) {
                continue;
            } else {
                --count;
                T* data = back_node->data;
                recycle(back_node);
                return data;
            }
        }
    };

    T* back() { return back; }
    bool empty() { return (bool)count; }
    int64_t size() { return count; }

private:
    Node *alloc() { return (new Node()); }
    void recycle(Node *node) { delete node; }

    std::atomic<int64_t> count;
    std::atomic<Node*> back;
};
} // end namespace StemCell
#endif
