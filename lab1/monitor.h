#ifndef MONITOR_H
#define MONITOR_H

#include <pthread.h>

struct Payload {
    int value;
};

struct Event {
    int number;
    Payload* data;
};

class Monitor {
    pthread_mutex_t mutex;
    pthread_cond_t  condition;
    Event* event;
    bool   ready;
    bool   stopped;

public:
    Monitor();
    ~Monitor();

    Monitor(const Monitor&) = delete;
    Monitor& operator=(const Monitor&) = delete;

    void provide(Event* new_event);   
    bool consume();                   
    void stop();                      
};

#endif