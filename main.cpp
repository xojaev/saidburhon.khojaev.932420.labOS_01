#include "monitor.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

static const int EVENTS_COUNT = 5;

void* producer(void* argument)
{
    auto* monitor = static_cast<Monitor*>(argument);

    for (int i = 1; i <= EVENTS_COUNT; ++i) {
        sleep(1);   /* задержка 1 с между событиями */

        Payload* payload = new Payload{i * 10};
        Event*   e       = new Event{i, payload};
        monitor->provide(e);
    }

    monitor->stop();   /* <-- обязательно: разбудить потребителя */
    return nullptr;
}

void* consumer(void* argument)
{
    auto* monitor = static_cast<Monitor*>(argument);

    /* честный цикл, без магической пятёрки */
    while (monitor->consume()) {
        /* вся обработка внутри consume() */
    }
    return nullptr;
}

int main()
{
    Monitor monitor;

    pthread_t consumer_thread;
    pthread_t producer_thread;

    int error = pthread_create(&consumer_thread, nullptr, consumer, &monitor);
    if (error != 0) {
        std::fprintf(stderr, "pthread_create (consumer): %s\n", std::strerror(error));
        return 1;
    }

    error = pthread_create(&producer_thread, nullptr, producer, &monitor);
    if (error != 0) {
        std::fprintf(stderr, "pthread_create (producer): %s\n", std::strerror(error));
        monitor.stop();
        pthread_join(consumer_thread, nullptr);
        return 1;
    }

    pthread_join(producer_thread, nullptr);
    pthread_join(consumer_thread, nullptr);
    return 0;
}