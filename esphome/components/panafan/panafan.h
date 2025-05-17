/*
 * Copyright 2025 Hoang Minh
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <deque>
#include <map>

#include "esphome/core/component.h"
#include "esphome/components/fan/fan.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"

namespace esphome
{
  namespace panafan
  {

    static const char *const TAG = "panafan.fan";

    enum FanSpeed
    {
      PANAFAN_OFF = 0,
      PANAFAN_LOW = 1,
      PANAFAN_MEDIUM = 2,
      PANAFAN_HIGH = 3,
      PANAFAN_INVALID = 100
    };

    enum FanOscilatting
    {
      PANAFAN_OSC_OFF = 0,
      PANAFAN_OSC_ON = 1,
      PANAFAN_OSC_INVALID = 100,
    };

#define PANAFAN_TIMER_OFF 0
#define PANAFAN_TIMER_1H 1
#define PANAFAN_TIMER_3H 2
#define PANAFAN_TIMER_6H 3
#define PANAFAN_TIMER_INVALID 100

    struct FanState
    {
      FanSpeed speed;
      FanOscilatting oscillating;
      uint8_t timer;
    };

#define STR_FANTIMER_NONE_STR "---"
#define STR_FANTIMER_1_0H_STR "1.0h"
#define STR_FANTIMER_3_0H_STR "3.0h"
#define STR_FANTIMER_6_0H_STR "6.0h"

    // Array using the defines
    static const char *STR_FANTIMER_STR[] = {
        STR_FANTIMER_NONE_STR,
        STR_FANTIMER_1_0H_STR,
        STR_FANTIMER_3_0H_STR,
        STR_FANTIMER_6_0H_STR,
    };

#define PANAFAN_CMD_ONOFF 0
#define PANAFAN_CMD_SPEED 1
#define PANAFAN_CMD_OSCIL 2
#define PANAFAN_CMD_TIMER 3

    // Pulse parameters in RC
    const uint16_t PANAFAN_BIT_MARK = 900;
    const uint16_t PANAFAN_ONE_SPACE = 2615;
    const uint16_t PANAFAN_ZERO_SPACE = 850;
    const uint16_t PANAFAN_HEADER_MARK = 3650;
    const uint16_t PANAFAN_HEADER_SPACE = 3440;
    const uint16_t PANAFAN_FRAME_END = 10000;

    static std::array<std::vector<uint8_t>, 4> IR_COMMANDS = {{
        {0x3B, 0x48, 0x7C}, // OFF/ON
        {0xFB, 0x47, 0x80}, // SPEED
        {0x7B, 0x47, 0x88}, // OSCIL
        {0xBB, 0x47, 0x84}  // TIMER
    }};

    enum Model
    {
      PANAFAN_F409M
    };

    class PanaFan;

    class PanaFanTimer : public text_sensor::TextSensor, public Component
    {
    public:
      void setup() override;
      void dump_config() override;
      void set_parent_fan(PanaFan *fan) { this->fan_ = fan; }
      void set_fan_timer(uint8_t timer);

    private:
      PanaFan *fan_{nullptr};
    };

    class PanaFanSetTimer : public button::Button, public Component
    {
    public:
      void setup() override;
      void dump_config() override;
      void press_action() override;
      void set_parent_fan(PanaFan *fan) { this->fan_ = fan; }

    private:
      PanaFan *fan_{nullptr};
    };

    class PanaFan : public Component,
                    public fan::Fan,
                    public i2c::I2CDevice,
                    public remote_base::RemoteReceiverListener,
                    public remote_base::RemoteTransmittable
    {
    public:
      PanaFan() {}
      void setup() override;
      void dump_config() override;
      void loop() override;
      void set_interval_ms(int interval_ms) { this->interval_ms_ = interval_ms; }
      void set_fan_timer(PanaFanTimer *fan_timer) { this->fan_timer_ = fan_timer; }
      void set_fan_settimer(PanaFanSetTimer *fan_settimer) { this->fan_settimer_ = fan_settimer; }
      void set_model(Model model) { this->model_ = model; }
      fan::FanTraits get_traits() override { return this->traits_; }
      std::deque<uint8_t> command_queue{};
      void process_command();
      bool processing{false};

    protected:
      bool on_receive(remote_base::RemoteReceiveData data) override;
      void control(const fan::FanCall &call) override;

    private:
      fan::FanTraits traits_;
      
      uint32_t last_run_{0};
      FanState fan_state_;

      int interval_ms_{0};

      PanaFanTimer *fan_timer_{nullptr};
      PanaFanSetTimer *fan_settimer_{nullptr};

      Model model_{};

      void write_gpio(uint16_t value);
      void update_state();
      bool decode_data(remote_base::RemoteReceiveData data, std::vector<uint8_t> &state_bytes);
      void transmit_command(uint8_t cmd);
    };

  } // namespace panafan
} // namespace esphome
