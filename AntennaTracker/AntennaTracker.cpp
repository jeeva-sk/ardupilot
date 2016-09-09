/*
   Lead developers: Matthew Ridley and Andrew Tridgell
 
   Please contribute your ideas! See http://dev.ardupilot.org for details

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Tracker.h"
#include "version.h"

/*
  scheduler table - all regular tasks apart from the fast_loop()
  should be listed here, along with how often they should be called
  (in 20ms units) and the maximum time they are expected to take (in
  microseconds)
 */
const AP_Task<Tracker> Tracker::scheduler_tasks[] = {
    AP_Task<Tracker>::create(&Tracker::update_ahrs,            50,   1000),
    AP_Task<Tracker>::create(&Tracker::read_radio,             50,    200),
    AP_Task<Tracker>::create(&Tracker::update_tracking,        50,   1000),
    AP_Task<Tracker>::create(&Tracker::update_GPS,             10,   4000),
    AP_Task<Tracker>::create(&Tracker::update_compass,         10,   1500),
    AP_Task<Tracker>::create(&Tracker::update_barometer,       10,   1500),
    AP_Task<Tracker>::create(&Tracker::gcs_update,             50,   1700),
    AP_Task<Tracker>::create(&Tracker::gcs_data_stream_send,   50,   3000),
    AP_Task<Tracker>::create(&Tracker::compass_accumulate,     50,   1500),
    AP_Task<Tracker>::create(&Tracker::barometer_accumulate,   50,    900),
    AP_Task<Tracker>::create(&Tracker::ten_hz_logging_loop,    10,    300),
    AP_Task<Tracker>::create(&Tracker::dataflash_periodic,     50,    300),
    AP_Task<Tracker>::create(&Tracker::update_notify,          50,    100),
    AP_Task<Tracker>::create(&Tracker::check_usb_mux,          10,    300),
    AP_Task<Tracker>::create(&Tracker::gcs_retry_deferred,     50,   1000),
    AP_Task<Tracker>::create(&Tracker::one_second_loop,         1,   3900),
    AP_Task<Tracker>::create(&Tracker::compass_cal_update,     50,    100),
    AP_Task<Tracker>::create(&Tracker::accel_cal_update,       10,    100)
};

/**
  setup the sketch - called once on startup
 */
void Tracker::setup() 
{
    // load the default values of variables listed in var_info[]
    AP_Param::setup_sketch_defaults();

    init_tracker();

    // initialise the main loop scheduler
    scheduler.init(&scheduler_tasks[0], ARRAY_SIZE(scheduler_tasks));
}

/**
   loop() is called continuously 
 */
void Tracker::loop()
{
    // wait for an INS sample
    ins.wait_for_sample();

    // tell the scheduler one tick has passed
    scheduler.tick();

    scheduler.run(19900UL);
}

void Tracker::dataflash_periodic(void)
{
    DataFlash.periodic_tasks();
}

void Tracker::one_second_loop()
{
    // send a heartbeat
    gcs_send_message(MSG_HEARTBEAT);

    // make it possible to change orientation at runtime
    ahrs.set_orientation();

    // sync MAVLink system ID
    mavlink_system.sysid = g.sysid_this_mav;

    // updated armed/disarmed status LEDs
    update_armed_disarmed();

    one_second_counter++;

    if (one_second_counter >= 60) {
        if (g.compass_enabled) {
            compass.save_offsets();
        }
        one_second_counter = 0;
    }
}

void Tracker::ten_hz_logging_loop()
{
    if (should_log(MASK_LOG_IMU)) {
        DataFlash.Log_Write_IMU(ins);
    }
    if (should_log(MASK_LOG_ATTITUDE)) {
        Log_Write_Attitude();
    }
    if (should_log(MASK_LOG_RCIN)) {
        DataFlash.Log_Write_RCIN();
    }
    if (should_log(MASK_LOG_RCOUT)) {
        DataFlash.Log_Write_RCOUT();
    }
}

const AP_HAL::HAL& hal = AP_HAL::get_HAL();

Tracker::Tracker(void)
    : DataFlash{FIRMWARE_STRING}
{
    memset(&current_loc, 0, sizeof(current_loc));
    memset(&vehicle, 0, sizeof(vehicle));
}

Tracker tracker;

AP_HAL_MAIN_CALLBACKS(&tracker);
