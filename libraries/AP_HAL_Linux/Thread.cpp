/// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-
/*
 * Copyright (C) 2016  Intel Corporation. All rights reserved.
 *
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "Thread.h"

#include <sys/types.h>
#include <unistd.h>

#include <AP_HAL/AP_HAL.h>
#include <AP_Math/AP_Math.h>

#include "Scheduler.h"

extern const AP_HAL::HAL &hal;

namespace Linux {


void *Thread::run_trampoline(void *arg)
{
    Thread *thread = static_cast<Thread *>(arg);
    thread->run();

    return nullptr;
}

bool Thread::run()
{
    if (!_task) {
        return false;
    }

    _task();

    return true;
}

bool Thread::start(const char *name, int policy, int prio)
{
    if (_started) {
        return false;
    }

    struct sched_param param = { .sched_priority = prio };
    pthread_attr_t attr;
    int r;

    pthread_attr_init(&attr);

    /*
      we need to run as root to get realtime scheduling. Allow it to
      run as non-root for debugging purposes, plus to allow the Replay
      tool to run
     */
    if (geteuid() == 0) {
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
        pthread_attr_setschedparam(&attr, &param);
    }

    r = pthread_create(&_ctx, &attr, &Thread::run_trampoline, this);
    if (r != 0) {
        AP_HAL::panic("Failed to create thread '%s': %s\n",
                      name, strerror(r));
    }
    pthread_attr_destroy(&attr);

    if (name) {
        pthread_setname_np(_ctx, name);
    }

    _started = true;

    return true;
}

bool Thread::is_current_thread()
{
    return pthread_equal(pthread_self(), _ctx);
}

bool PeriodicThread::set_rate(uint32_t rate_hz)
{
    if (_started || rate_hz == 0) {
        return false;
    }

    _period_usec = hz_to_usec(rate_hz);

    return true;
}

bool PeriodicThread::run()
{
    uint64_t next_run_usec = AP_HAL::micros64() + _period_usec;

    while (true) {
        uint64_t dt = next_run_usec - AP_HAL::micros64();
        if (dt > _period_usec) {
            // we've lost sync - restart
            next_run_usec = AP_HAL::micros64();
        } else {
            Scheduler::from(hal.scheduler)->microsleep(dt);
        }
        next_run_usec += _period_usec;

        _task();
    }

    return true;
}

}
