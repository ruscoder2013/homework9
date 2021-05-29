#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <queue>
#include <algorithm>
#include <thread>
#include <condition_variable>

#ifndef THREADSAFE_QUEUE
#define THREADSAFE_QUEUE

template<class T>
class threadsafe_queue {
public:
    threadsafe_queue() {}
    void push(T value) {
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(value);
        data_cond.notify_one();
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return false;
        value = data_queue.front();
        data_queue.pop();
        return true;
    }
    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> res(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        return res;
    }

    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this]() {
            return !data_queue.empty();
        });
        value=data_queue.front();
        data_queue.pop();
    }
    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this]() {
            return !data_queue.empty();
        });
        std::shared_ptr<T> res(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        return res;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
private:
    mutable std::mutex mut;
    std::queue<T> data_queue;
    std::condition_variable data_cond;
};

#endif