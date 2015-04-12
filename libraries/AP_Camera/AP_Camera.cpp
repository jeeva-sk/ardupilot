// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-

#include <stdio.h>
#include <AP_Camera.h>
#include <AP_Relay.h>
#include <AP_Math.h>
#include <RC_Channel.h>
#include <AP_HAL.h>

// ------------------------------
#define CAM_DEBUG DISABLED

const AP_Param::GroupInfo AP_Camera::var_info[] PROGMEM = {
    // @Param: TRIGG_TYPE
    // @DisplayName: Camera shutter (trigger) type
    // @Description: how to trigger the camera to take a picture
    // @Values: 0:Servo,1:Relay
    // @User: Standard
    AP_GROUPINFO("TRIGG_TYPE",  0, AP_Camera, _trigger_type, AP_CAMERA_TRIGGER_DEFAULT_TRIGGER_TYPE),

    // @Param: DURATION
    // @DisplayName: Duration that shutter is held open
    // @Description: How long the shutter will be held open in 10ths of a second (i.e. enter 10 for 1second, 50 for 5seconds)
    // @Units: seconds
    // @Range: 0 50
    // @User: Standard
    AP_GROUPINFO("DURATION",    1, AP_Camera, _trigger_duration, AP_CAMERA_TRIGGER_DEFAULT_DURATION),

    // @Param: SERVO_ON
    // @DisplayName: Servo ON PWM value
    // @Description: PWM value to move servo to when shutter is activated
    // @Units: pwm
    // @Range: 1000 2000
    // @User: Standard
    AP_GROUPINFO("SERVO_ON",    2, AP_Camera, _servo_on_pwm, AP_CAMERA_SERVO_ON_PWM),

    // @Param: SERVO_OFF
    // @DisplayName: Servo OFF PWM value
    // @Description: PWM value to move servo to when shutter is deactivated
    // @Units: pwm
    // @Range: 1000 2000
    // @User: Standard
    AP_GROUPINFO("SERVO_OFF",   3, AP_Camera, _servo_off_pwm, AP_CAMERA_SERVO_OFF_PWM),

    // @Param: TRIGG_DIST
    // @DisplayName: Camera trigger distance
    // @Description: Distance in meters between camera triggers. If this value is non-zero then the camera will trigger whenever the GPS position changes by this number of meters regardless of what mode the APM is in. Note that this parameter can also be set in an auto mission using the DO_SET_CAM_TRIGG_DIST command, allowing you to enable/disable the triggering of the camera during the flight.
    // @User: Standard
    // @Units: meters
    // @Range: 0 1000
    AP_GROUPINFO("TRIGG_DIST",  4, AP_Camera, _trigg_dist, 0),

    AP_GROUPEND
};


/// Servo operated camera
void
AP_Camera::servo_pic()
{
	RC_Channel_aux::set_radio(RC_Channel_aux::k_cam_trigger, _servo_on_pwm);

	// leave a message that it should be active for this many loops (assumes 50hz loops)
	_trigger_counter = constrain_int16(_trigger_duration*5,0,255);
}

/// basic relay activation
void
AP_Camera::relay_pic()
{
    _apm_relay->on(0);

    // leave a message that it should be active for this many loops (assumes 50hz loops)
    _trigger_counter = constrain_int16(_trigger_duration*5,0,255);
}

/// single entry point to take pictures
///  set send_mavlink_msg to true to send DO_DIGICAM_CONTROL message to all components
void
AP_Camera::trigger_pic(bool send_mavlink_msg)
{
    _image_index++;
    switch (_trigger_type)
    {
    case AP_CAMERA_TRIGGER_TYPE_SERVO:
        servo_pic();                    // Servo operated camera
        break;
    case AP_CAMERA_TRIGGER_TYPE_RELAY:
        relay_pic();                    // basic relay activation
        break;
    }

    if (send_mavlink_msg) {
        // create command long mavlink message
        mavlink_command_long_t cmd_msg;
        memset(&cmd_msg, 0, sizeof(cmd_msg));
        cmd_msg.command = MAV_CMD_DO_DIGICAM_CONTROL;

        // create message
        mavlink_message_t msg;
        mavlink_msg_command_long_encode(0, 0, &msg, &cmd_msg);

        // forward to all components
        GCS_MAVLINK::send_to_components(&msg);
    }
}

/// de-activate the trigger after some delay, but without using a delay() function
/// should be called at 50hz
void
AP_Camera::trigger_pic_cleanup()
{
    if (_trigger_counter) {
        _trigger_counter--;
    } else {
        switch (_trigger_type) {
            case AP_CAMERA_TRIGGER_TYPE_SERVO:
                RC_Channel_aux::set_radio(RC_Channel_aux::k_cam_trigger, _servo_off_pwm);
                break;
            case AP_CAMERA_TRIGGER_TYPE_RELAY:
                _apm_relay->off(0);
                break;
        }
    }
}

/// decode MavLink that configures camera
void
AP_Camera::configure_msg(mavlink_message_t* msg)
{
    // this message is not used directly but instead forwarded to components
    GCS_MAVLINK::send_to_components(msg);
}

/// decode MavLink that controls camera
void
AP_Camera::control_msg(mavlink_message_t* msg)
{
    __mavlink_digicam_control_t packet;
    mavlink_msg_digicam_control_decode(msg, &packet);

    // This values may or not be used by APM (the shot is)
    // They are bypassed as "echo" to a external specialized board
    /*
     *  packet.command_id
     *  packet.extra_param
     *  packet.extra_value
     *  packet.focus_lock
     *  packet.session
     *  packet.shot
     *  packet.zoom_pos
     *  packet.zoom_step
     */
    if (packet.shot)
    {
        trigger_pic(false);
    }

    // forward to all components
    GCS_MAVLINK::send_to_components(msg);
}

// Mission command processing
void AP_Camera::configure_cmd(const AP_Mission::Mission_Command& cmd)
{
    mavlink_mission_item_t mav_cmd = {};

    // convert command to mavlink message
    if (AP_Mission::mission_cmd_to_mavlink(cmd, mav_cmd)) {
        // convert mission item to mavlink message
        // convert mission item to mavlink message
        mavlink_message_t msg;
        mavlink_msg_mission_item_encode(1, 0, &msg, &mav_cmd);
        // send to all components
        GCS_MAVLINK::send_to_components(&msg);
    }
}

void AP_Camera::control_cmd(const AP_Mission::Mission_Command& cmd)
{
    mavlink_mission_item_t mav_cmd = {};

    // convert command to mavlink mission item
    if (AP_Mission::mission_cmd_to_mavlink(cmd, mav_cmd)) {
        // convert mission item to mavlink message
        mavlink_message_t msg;

        ::printf("cmd:%f\n",(float)mav_cmd.command);
        ::printf("param1:%f\n",(float)mav_cmd.param1);
        ::printf("param2:%f\n",(float)mav_cmd.param2);
        ::printf("param3:%f\n",(float)mav_cmd.param3);
        ::printf("param4:%f\n",(float)mav_cmd.param4);
        ::printf("param5:%f\n",(float)mav_cmd.x);
        ::printf("param6:%f\n",(float)mav_cmd.y);
        ::printf("param7:%f\n",(float)mav_cmd.z);

        mavlink_msg_mission_item_encode(1, 0, &msg, &mav_cmd);
        // send to all components
        GCS_MAVLINK::send_to_components(&msg);
    }
}

/*
  Send camera feedback to the GCS
 */
void AP_Camera::send_feedback(mavlink_channel_t chan, AP_GPS &gps, const AP_AHRS &ahrs, const Location &current_loc)
{
    float altitude, altitude_rel;
    if (current_loc.flags.relative_alt) {
        altitude = current_loc.alt+ahrs.get_home().alt;
        altitude_rel = current_loc.alt;
    } else {
        altitude = current_loc.alt;
        altitude_rel = current_loc.alt - ahrs.get_home().alt;
    }

    mavlink_msg_camera_feedback_send(chan, 
        gps.time_epoch_usec(),
        0, 0, _image_index,
        current_loc.lat, current_loc.lng,
        altitude/100.0f, altitude_rel/100.0f,
        ahrs.roll_sensor/100.0f, ahrs.pitch_sensor/100.0f, ahrs.yaw_sensor/100.0f,
        0.0,0);
}


/*  update location, for triggering by GPS distance moved
    This function returns true if a picture should be taken
    The caller is responsible for taking the picture based on the return value of this function.
    The caller is also responsible for logging the details about the photo
*/
bool AP_Camera::update_location(const struct Location &loc)
{
    if (_trigg_dist == 0.0f) {
        return false;
    }
    if (_last_location.lat == 0 && _last_location.lng == 0) {
        _last_location = loc;
        return false;
    }
    if (_last_location.lat == loc.lat && _last_location.lng == loc.lng) {
        // we haven't moved - this can happen as update_location() may
        // be called without a new GPS fix
        return false;
    }
    if (get_distance(loc, _last_location) < _trigg_dist) {
        return false;
    }
    _last_location = loc;
    return true;
}
