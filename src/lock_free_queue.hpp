#ifndef LOCK_FREE_QUEUE_HPP
#define LOCK_FREE_QUEUE_HPP

#include <atomic>

namespace StemCell {

template<typename T>
class LockFreeQueue {
public:
    struct Node {
        Node() : data(nullptr), next(nullptr) {}
        void reset() { data = nullptr; next = nullptr;}
        
        T *data;
        std::atomic<Node*> next;
    };
    
    LockFreeQueue() : count(0), tail(&root) {}
    ~LockFreeQueue() {}

    void push(const T* data) {
        Node* node = alloc();
        node->data = data;
        for (;;) {
            (tail->next).compare_exchange_weak(nullptr, node, std::memory_order_seq_cst) ? break : continue;
        }
        ++count;
    };

    T* pop() {
        std::atomic<Node*>& front_node = root.next;
        for (;;) {
            Node* head_node = front_node; // Node* head_node = front_node.load();
            if (!head_node) { return nullptr; }
            Node* second_node = front_node->next; // Node* second_node = front_node->next.load();
            if(!front_node.compare_exchange_weak(head_node, second, std::memory_order_seq_cst)) {
                continue;
            } else {
                --count;
                recycle(origin_head);
                return head_node;
            }
        }
    };

    T* front() { return root.next; }
    bool empty() { return (bool)count; }
    int64_t size() { return count; }

private:
    Node *alloc() { return (new Node()); }
    void recycle(Node *node) { delete node; }

    std::atomic<int64_t> count;
    Node root;  // help to simplifiy the logic
    std::atomic<Node*> tail;

};

} // end namespace StemCell

#endif
