#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

using namespace std;

struct Event {
    int id;
};

class Monitor {
    mutex m_;
    condition_variable cv_;
    Event* _event = nullptr;

public:
    void send(Event* ev) {
        unique_lock<mutex> lock(m_);
        while (_event != nullptr) {          
            cv_.wait(lock);
        }
        _event = ev;
        cv_.notify_one();
    }

    Event* receive() {
        unique_lock<mutex> lock(m_);
        while (_event == nullptr) {         
            cv_.wait(lock);
        }
        Event* ev = _event;
        _event = nullptr;
        cv_.notify_one();
        return ev;
    }
};


void Test1() {
    Monitor monitor;

    thread consumer([&]() {
        while (true) {
            Event* ev = monitor.receive();
            std::cout << "Comsumer: Received event " << ev->id << "\n";
            delete ev;
        }
        });

    thread producer([&]() {
        int counter = 0;
        while (true) {
            this_thread::sleep_for(chrono::seconds(1));

            Event* ev = new Event{ ++counter };
            std::cout << "Producer: Sent event " << ev->id << "\n";

            monitor.send(ev);
        }
        });

    producer.join();
    consumer.detach();
}