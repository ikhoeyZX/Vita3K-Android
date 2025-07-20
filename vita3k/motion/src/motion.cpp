// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <motion/event_handler.h>
#include <motion/functions.h>
#include <motion/state.h>

#include <ctrl/state.h>
#include <util/log.h>

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_sensor.h>
#include <numbers>

#ifdef ANDROID
#include <SDL3/SDL_system.h>
#include <jni.h>

static bool is_device_landscape = false;

static void init_device_orientation(){
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());
    jclass clazz(env->GetObjectClass(activity));

    jmethodID method_id = env->GetMethodID(clazz, "isDefaultOrientationLandscape", "()Z");

    is_device_landscape = env->CallBooleanMethod(activity, method_id);

    // clean up the local references.
    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(clazz);
}

#else 
constexpr bool is_device_landscape = true;
#endif

static void init_device_sensors(MotionState& state){
    int i, num_sensors;
    SDL_SensorID *sensors = SDL_GetSensors(&num_sensors);
    if (sensors) {
        for (i = 0; i < num_sensors; ++i) {
            LOG_INFO("Sensor name: {}", SDL_GetSensorNameForID(sensors[i]).c_str());
            LOG_INFO("Sensor type: {}", SDL_GetSensorTypeForID(sensors[i]).c_str());

            bool sensor_used = true;

            SDL_Sensor* sensor = SDL_OpenSensor(sensors[i]);
            SDL_SensorType type = SDL_GetSensorType(sensor);

            switch (type){
                case SDL_SENSOR_ACCEL:
                    state.device_accel = sensor;
                    break;
                
                case SDL_SENSOR_GYRO:
                    state.device_gyro = sensor;
                    break;
        
                default:
                    sensor_used = false;
                    break;
            }
        }
        if(!sensor_used){
            SDL_CloseSensor(sensor);
        }
    }
    
    state.has_device_motion_support = (state.device_accel && state.device_gyro);

#ifdef ANDROID
    init_device_orientation();
#endif
}

void MotionState::init(){
    init_device_sensors(*this);

    if(has_device_motion_support)
        LOG_INFO("Device has builtin accelerometer and gyroscope.");

    // close them as having them opened uses battery
    if(device_accel){
        SDL_CloseSensor(device_accel);
        device_accel = nullptr;
    }
    if(device_gyro){
        SDL_CloseSensor(device_gyro);
        device_gyro = nullptr;
    }
}

SceFVector3 get_acceleration(const MotionState &state) {
    Util::Vec3f accelerometer = state.motion_data.GetAcceleration();
    return {
        accelerometer.x,
        accelerometer.y,
        accelerometer.z,
    };
}

Util::Vec3f gyroscope = state.motion_data.GetGyroscope() * 2.f * std::numbers::pi_v<float>;
    Util::Vec3f gyroscope = state.motion_data.GetGyroscope() * static_cast<float>(2.f * M_PI);
    return {
        gyroscope.x,
        gyroscope.y,
        gyroscope.z,
    };
}

Util::Quaternion<SceFloat> get_orientation(const MotionState &state) {
    auto quat = state.motion_data.GetOrientation();
    return {
        { -quat.xyz[1], -quat.w, quat.xyz[0] },
        -quat.xyz[2],
    };
}

SceBool get_gyro_bias_correction(const MotionState &state) {
    return state.motion_data.IsGyroBiasEnabled();
}

void set_gyro_bias_correction(MotionState &state, SceBool setValue) {
    state.motion_data.EnableGyroBias(setValue);
}

SceBool get_tilt_correction(MotionState &state) {
    return state.motion_data.IsTiltCorrectionEnabled();
}

void set_tilt_correction(MotionState &state, SceBool setValue) {
    state.motion_data.EnableTiltCorrection(setValue);
}

SceBool get_deadband(MotionState &state) {
    return state.motion_data.IsDeadbandEnabled();
}

void set_deadband(MotionState &state, SceBool setValue) {
    state.motion_data.EnableDeadband(setValue);
}

SceFloat get_angle_threshold(const MotionState &state) {
    return state.motion_data.GetAngleThreshold();
}

void set_angle_threshold(MotionState &state, SceFloat setValue) {
    state.motion_data.SetAngleThreshold(setValue);
}

SceFVector3 get_basic_orientation(const MotionState &state) {
    return state.motion_data.GetBasicOrientation();
}

void handle_motion_event(EmuEnvState &emuenv, const SDL_GamepadSensorEvent &sensor) {
    if (!emuenv.motion.is_sampling)
        return;

    if (!emuenv.ctrl.has_motion_support)
        return;

    if (sensor.sensor == SDL_SENSOR_ACCEL) {
        Util::Vec3f accel{
            sensor.data[0],
            sensor.data[1],
            sensor.data[2],
        };
        accel /= -SDL_STANDARD_GRAVITY;
        std::swap(accel.y, accel.z);
        accel.y *= -1;
        emuenv.motion.motion_data.SetAcceleration(accel);
        emuenv.motion.motion_data.UpdateOrientation(sensor.sensor_timestamp - emuenv.motion.last_accel_timestamp);
        emuenv.motion.motion_data.UpdateBasicOrientation();
        emuenv.motion.last_accel_timestamp = sensor.sensor_timestamp;
        emuenv.motion.last_counter++;
    } else if (sensor.sensor == SDL_SENSOR_GYRO) {
        Util::Vec3f gyro{
            sensor.data[0],
            sensor.data[1],
            sensor.data[2],
        };
        gyro /= 2.f * std::numbers::pi_v<float>;
        std::swap(gyro.y, gyro.z);
        gyro.y *= -1;
        emuenv.motion.motion_data.SetGyroscope(gyro);
        emuenv.motion.motion_data.UpdateRotation(sensor.sensor_timestamp - emuenv.motion.last_gyro_timestamp);
        emuenv.motion.last_gyro_timestamp = sensor.sensor_timestamp;
        emuenv.motion.last_counter++;
    }
}

void refresh_motion(MotionState &state, CtrlState &ctrl_state) {
    if (!state.is_sampling)
        return;

    if (!ctrl_state.has_motion_support)
        return;

    state.last_counter++;
}
