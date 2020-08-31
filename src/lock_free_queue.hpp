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
        node->data = const_cast<T*>(data);
        for (;;) {
            Node* tail_node = tail;
            Node* blank_node = nullptr;
            if (tail_node->next.compare_exchange_weak(blank_node, node, std::memory_order_seq_cst)) {
                tail = node;
                ++count;
                return; 
            } else { 
                continue;
            }
        }
    }

    T* pop() {
        std::atomic<Node*>& front_node = root.next;
        for (;;) {
            Node* head_node = front_node; // Node* head_node = front_node.load();
            if (!head_node) { return nullptr; }
            Node* second_node = head_node->next; // Node* second_node = front_node->next.load();
            if(!front_node.compare_exchange_weak(head_node, second_node, std::memory_order_seq_cst)) {
                continue;
            } else {
                --count;
                T* data = head_node->data;
                recycle(head_node);
                return data;
            }
        }
    };

    T* front() { return root.next; }
    bool empty() { return (bool)count; }
    int64_t size() { return count; }
    int64_t stat_size() {
        int64_t count = 0;
        Node* node = &root;
        while (node->next) {
            node = node->next;
            ++count;
        }
        return count;
    }
private:
    Node *alloc() { return (new Node()); }
    void recycle(Node *node) { delete node; }

    std::atomic<int64_t> count;
    Node root;  // help to simplifiy the logic
    std::atomic<Node*> tail;

};

} // end namespace StemCell

#endif
