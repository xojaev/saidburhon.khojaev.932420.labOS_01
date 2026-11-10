#include "monitor.h"

#include <cstdio>

Monitor::Monitor()
    : event(nullptr), ready(false), stopped(false)
{
    pthread_mutex_init(&mutex, nullptr);
    pthread_cond_init(&condition, nullptr);
}

Monitor::~Monitor()
{
    pthread_cond_destroy(&condition);
    pthread_mutex_destroy(&mutex);
}

void Monitor::provide(Event* new_event)
{
    pthread_mutex_lock(&mutex);

    /* ждём, пока потребитель не заберёт предыдущее событие */
    while (ready && !stopped)
        pthread_cond_wait(&condition, &mutex);

    if (stopped) {
        /* монитор остановлен — событие не принимаем, освобождаем память */
        delete new_event->data;
        delete new_event;
        pthread_mutex_unlock(&mutex);
        return;
    }

    event = new_event;
    ready = true;

    std::printf("Поставщик: отправил событие %d\n", event->number);
    std::fflush(stdout);

    pthread_cond_signal(&condition);   /* будим потребителя */
    pthread_mutex_unlock(&mutex);
}

bool Monitor::consume()
{
    pthread_mutex_lock(&mutex);

    /* ЦИКЛ: защита от ложных пробуждений и от сигнала до wait */
    while (!ready && !stopped)
        pthread_cond_wait(&condition, &mutex);   /* мьютекс временно отпускается */

    if (!ready) {
        /* проснулись из-за stopped */
        pthread_mutex_unlock(&mutex);
        return false;
    }

    std::printf("Потребитель: обработал событие %d, данные %d\n",
                event->number, event->data->value);
    std::fflush(stdout);

    delete event->data;
    delete event;
    event = nullptr;
    ready = false;

    pthread_cond_signal(&condition);   /* будим поставщика, если он ждёт */
    pthread_mutex_unlock(&mutex);
    return true;
}

void Monitor::stop()
{
    pthread_mutex_lock(&mutex);
    stopped = true;
    pthread_cond_broadcast(&condition);   /* будим ВСЕХ ожидающих */
    pthread_mutex_unlock(&mutex);
}