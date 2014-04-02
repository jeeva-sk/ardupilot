/// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-

/*
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

/*
  backend driver for airspeed from a I2C MS4525D0 sensor
 */

#include <AP_Common.h>
#include <AP_HAL.h>
#include <AP_Math.h>
#include <AP_Airspeed_I2C.h>

extern const AP_HAL::HAL& hal;

<<<<<<< HEAD
#define MS4525D0 0
#define HSCDRRN  1
=======
#define MS4525DO 0
#define HSCDRRN001ND2A5 1
>>>>>>> 0b6284125c6c2a4304fdb2c66d1156f2327c5171
#define I2C_ADDRESS_MS4525DO	0x28
#define I2C_ADDRESS_HSCDRRN     0x28

// probe and initialise the sensor
bool AP_Airspeed_I2C::init(void)
{
    // get pointer to i2c bus semaphore
    AP_HAL::Semaphore* i2c_sem = hal.i2c->get_semaphore();

    // take i2c bus sempahore
    if (!i2c_sem->take(200))
        return false;

    _measure();
    hal.scheduler->delay(10);
    _collect();
    i2c_sem->give();
    if (_last_sample_time_ms != 0) {
        hal.scheduler->register_timer_process(AP_HAL_MEMBERPROC(&AP_Airspeed_I2C::_timer));
        return true;
    }
    return false;
}

// start a measurement
void AP_Airspeed_I2C::_measure(void)
{
    _measurement_started_ms = 0;
    if (_sensor_type == MS4525D0) {
        if (hal.i2c->writeRegisters(I2C_ADDRESS_MS4525DO, 0, 0, NULL) == 0) {
            _measurement_started_ms = hal.scheduler->millis();
        }
    } else {
        if (hal.i2c->writeRegisters(I2C_ADDRESS_HSCDRRN, 0, 0, NULL) == 0) {
            _measurement_started_ms = hal.scheduler->millis();
        }
    }
}

// read the values from the sensor
void AP_Airspeed_I2C::_collect(void)
{
    uint8_t data[4];

    _measurement_started_ms = 0;
    if (_sensor_type == MS4525D0) {
        if (hal.i2c->read(I2C_ADDRESS_MS4525DO, 4, data) != 0) {
            return;
        }
        
        uint8_t status = (data[0] & 0xC0) >> 6;
        
        switch (status) {
            case 0: break;
            case 1: return; // Reserved
            case 2: return; // Stale data
            case 3: return; // Fault
        }

        int16_t dp_raw, dT_raw;
        dp_raw = (data[0] << 8) + data[1];
        dp_raw = 0x3FFF & dp_raw;
        dT_raw = (data[2] << 8) + data[3];
        dT_raw = (0xFFE0 & dT_raw) >> 5;

        const float P_min = -1.0f;
        const float P_max = 1.0f;
        const float PSI_to_Pa = 6894.757f;
        /*
          this equation is an inversion of the equation in the
          pressure transfer function figure on page 4 of the datasheet

          We negate the result so that positive differential pressures
          are generated when the bottom port is used as the static
          port on the pitot and top port is used as the dynamic port
         */
        float diff_press_PSI = -((dp_raw - 0.1f*16383) * (P_max-P_min)/(0.8f*16383) + P_min);

        _pressure = diff_press_PSI * PSI_to_Pa;
        _temperature = ((200.0f * dT_raw) / 2047) - 50;
    } else if (_sensor_type == HSCDRRN) {
        if (hal.i2c->read(I2C_ADDRESS_HSCDRRN, 4, data) != 0) {
            return;
        }
        
        uint8_t status = (data[0] & 0xC0) >> 6;
        
        switch (status) {
            case 0: break;
            case 1: return; // Programming mode
            case 2: return; // Stale data
            case 3: return; // Fault
        }

        int16_t dp_raw, dT_raw;
        dp_raw = (data[0] << 8) + data[1];
        dp_raw = 0x3FFF & dp_raw;
        dT_raw = (data[2] << 8) + data[3];
        dT_raw = (0xFFE0 & dT_raw) >> 5;

        const float P_min = -1.0f;
        const float P_max = 1.0f;
        const float InAq_to_Pa = 248.84f;
        
        // Taken from Honeywell Technical Note 008201-3-EN
        // https://sensing.honeywell.com/i2c%20comms%20digital%20output%20pressure%20sensors_tn_008201-3-en_final_30may12.pdf
        float diff_press_PSI = -(dp_raw - 1638.0f)*(P_max - P_min)/(14745.0f-1638.0f);

        _pressure = diff_press_PSI * InAq_to_Pa;
        _temperature = ((200.0f * dT_raw) / 2047) - 50;
    }
    _last_sample_time_ms = hal.scheduler->millis();
}

// 1kHz timer
void AP_Airspeed_I2C::_timer(void)
{
    AP_HAL::Semaphore* i2c_sem = hal.i2c->get_semaphore();

    if (!i2c_sem->take_nonblocking())
        return;

    if (_measurement_started_ms == 0) {
        _measure();
        i2c_sem->give();
        return;
    }
    if ((hal.scheduler->millis() - _measurement_started_ms) > 10) {
        _collect();
        // start a new measurement
        _measure();
    }
    i2c_sem->give();
}

// return the current differential_pressure in Pascal
bool AP_Airspeed_I2C::get_differential_pressure(float &pressure)
{
    if ((hal.scheduler->millis() - _last_sample_time_ms) > 100) {
        return false;
    }
    pressure = _pressure;
    return true;
}

// return the current temperature in degrees C, if available
bool AP_Airspeed_I2C::get_temperature(float &temperature)
{
    if ((hal.scheduler->millis() - _last_sample_time_ms) > 100) {
        return false;
    }
    temperature = _temperature;
    return true;
}


