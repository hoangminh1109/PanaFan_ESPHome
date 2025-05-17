#include "panafan.h"
#include "esphome/core/log.h"

#include <vector>
#include <array>

namespace esphome
{
  namespace panafan
  {

    void PanaFan::setup()
    {
      last_run_ = millis();

      auto restore = this->restore_state_();
      if (restore.has_value())
      {
        restore->apply(*this);
      }

      // Construct traits
      fan::FanTraits fan_traits(true, true, false, 3);
      this->traits_ = fan_traits;

      this->fan_state_.speed = PANAFAN_INVALID;
      this->fan_state_.oscillating = PANAFAN_OSC_INVALID;
      this->fan_state_.timer = PANAFAN_TIMER_INVALID;

      this->fan_timer_->publish_state(STR_FANTIMER_NONE_STR);
    }

    void PanaFan::dump_config()
    {
      LOG_FAN(TAG, "PanaFan Wall Fan", this);
    }

    bool PanaFan::decode_data(remote_base::RemoteReceiveData data, std::vector<uint8_t> &state_bytes)
    {
      auto raw_data = data.get_raw_data();

      if (!data.expect_item(PANAFAN_HEADER_MARK, PANAFAN_HEADER_SPACE))
      {
        ESP_LOGV(TAG, "Invalid data - expected header");
        return false;
      }

      state_bytes.clear();
      while (data.get_index() + 2 < raw_data.size())
      {
        uint8_t byte = 0;
        for (uint8_t a_bit = 0; a_bit < 8; a_bit++)
        {
          if (data.expect_item(PANAFAN_BIT_MARK, PANAFAN_FRAME_END))
          {
            // expect new header if there are remain data
            if (!data.expect_item(PANAFAN_HEADER_MARK, PANAFAN_HEADER_SPACE))
            {
              ESP_LOGV(TAG, "Invalid data - expected header at index = %d", data.get_index());
              return false;
            }
          }

          // bit 1
          if (data.expect_item(PANAFAN_BIT_MARK, PANAFAN_ONE_SPACE))
          {
            byte |= 1 << a_bit;
          }
          // bit 0
          else if (data.expect_item(PANAFAN_BIT_MARK, PANAFAN_ZERO_SPACE))
          {
            // 0 already initialized, hence do nothingg here
          }
          else
          {
            ESP_LOGV(TAG, "Invalid bit %d of byte %d, index = %d", a_bit, state_bytes.size(), data.get_index());
            return false;
          }
        }
        state_bytes.push_back(byte);
      }

      return true;
    }

    void PanaFan::transmit_command(uint8_t cmd)
    {
      std::vector<uint8_t> command = IR_COMMANDS[cmd];

      auto transmit = this->transmitter_->transmit();
      auto *data = transmit.get_data();

      // First frame
      data->mark(PANAFAN_HEADER_MARK);
      data->space(PANAFAN_HEADER_SPACE);
      for (uint8_t i_byte = 0; i_byte < command.size(); i_byte++)
      {
        for (uint8_t i_bit = 0; i_bit < 8; i_bit++)
        {
          data->mark(PANAFAN_BIT_MARK);
          bool bit = command[i_byte] & (1 << i_bit);
          data->space(bit ? PANAFAN_ONE_SPACE : PANAFAN_ZERO_SPACE);
        }
      }
      data->mark(PANAFAN_BIT_MARK);
      data->space(PANAFAN_FRAME_END);

      // transmit
      transmit.perform();
    }

    bool PanaFan::on_receive(remote_base::RemoteReceiveData data)
    {
      // std::vector<uint8_t> state_bytes;
      // if (!decode_data(data, state_bytes))
      // {
      //     ESP_LOGV(TAG, "Decode ir data failed");
      //     return false;
      // }

      return true;
    }

    void PanaFan::update_state()
    {
      uint32_t now = millis();
      if (now - this->last_run_ >= this->interval_ms_)
      {
        this->last_run_ = now;

        // interval task
        uint8_t data[2];
        bool success = this->read_bytes_raw(data, 2);
        if (!success)
        {
          ESP_LOGW(TAG, "I2C read error!");
          return;
        }

        uint16_t led_status = (~data[0] & 0xFC); // bit0 & bit1 is 2 buttons
        led_status |= ((~data[1] & 0x80) << 8);  // bit15 is for swing

        bool led_state[7] = {
            (led_status & (1 << 2)) == (1 << 2),   // HIGH
            (led_status & (1 << 3)) == (1 << 3),   // MEDIUM
            (led_status & (1 << 4)) == (1 << 4),   // LOW
            (led_status & (1 << 5)) == (1 << 5),   // 6H
            (led_status & (1 << 6)) == (1 << 6),   // 3H
            (led_status & (1 << 7)) == (1 << 7),   // 1H
            (led_status & (1 << 15)) == (1 << 15), // SWING
        };

        FanSpeed speed = PANAFAN_OFF;
        if (led_state[0])
          speed = PANAFAN_HIGH;
        if (led_state[1])
          speed = PANAFAN_MEDIUM;
        if (led_state[2])
          speed = PANAFAN_LOW;

        FanOscilatting osc = PANAFAN_OSC_OFF;
        if (led_state[6])
          osc = PANAFAN_OSC_ON;

        uint8_t timer = PANAFAN_TIMER_OFF;
        if (led_state[3])
          timer = (PANAFAN_TIMER_6H);
        if (led_state[4])
          timer = (PANAFAN_TIMER_3H);
        if (led_state[5])
          timer = (PANAFAN_TIMER_1H);

        bool state_change = false;

        // fan speed
        if (this->fan_state_.speed != speed)
        {
          this->fan_state_.speed = speed;
          if (this->fan_state_.speed == PANAFAN_OFF)
          {
            this->state = false;
          }
          else
          {
            this->state = true;
            this->speed = static_cast<int>(this->fan_state_.speed);
          }
          state_change = true;
        }

        // fan oscillating
        if (this->fan_state_.oscillating != osc)
        {
          this->fan_state_.oscillating = osc;
          this->oscillating = static_cast<bool>(this->fan_state_.oscillating);
          state_change = true;
        }

        // fan timer
        if (this->fan_state_.timer != timer)
        {
          this->fan_state_.timer = timer;
          this->fan_timer_->set_fan_timer(this->fan_state_.timer);
        }

        if (state_change)
        {
          this->publish_state();
        }
      }
    }

    void PanaFan::loop()
    {
      // skip if it is processing a command
      if (this->processing)
      {
        return;
      }

      this->update_state();
    }

    void PanaFan::write_gpio(uint16_t value)
    {
      uint8_t data[2];
      data[0] = ~value;
      data[1] = ~value >> 8;
      if (this->write(data, 2) != i2c::ERROR_OK)
      {
        ESP_LOGW(TAG, "I2C write error!");
      }
    }

    void PanaFan::process_command()
    {
      if (this->command_queue.size() == 0)
      {
        this->set_timeout("resume_operation",
                          50,
                          [this]()
                          {
                            this->update_state();
                            this->processing = false;
                          });
        return;
      }

      uint8_t cmd = this->command_queue.front();
      this->transmit_command(cmd);
      this->command_queue.pop_front();
      if (this->command_queue.size() > 0)
      {
        this->set_timeout("next_cmd",
                          300,
                          [this]()
                          {
                            this->process_command();
                          });
      }
      else
      {
        this->process_command();        
      }
    }

    void PanaFan::control(const fan::FanCall &call)
    {
      // skip if it is processing a command
      if (this->processing)
      {
        return;
      }

      if (call.get_state().has_value())
      {
        bool newstate = *call.get_state();
        if (!newstate) // off
        {
          if (this->state)
          {
            this->command_queue.push_back(PANAFAN_CMD_ONOFF);
          }
        }
        else // on
        {
          // if the fan is currently off, we need an additional ON pressed to on.
          if (!this->state)
          {
            this->command_queue.push_back(PANAFAN_CMD_ONOFF);
          }

          if (call.get_speed().has_value())
          {
            uint8_t currspeed = this->speed;
            uint8_t newspeed = *call.get_speed();

            newspeed = newspeed < currspeed ? newspeed + 3 : newspeed;
            this->command_queue.insert(this->command_queue.end(), newspeed - currspeed, PANAFAN_CMD_SPEED);
          }
        }
      }

      if (call.get_oscillating().has_value())
      {
        if (this->oscillating != *call.get_oscillating())
        {
          this->command_queue.push_back(PANAFAN_CMD_OSCIL);
        }
      }

      if (this->command_queue.size() > 0)
      {
        // start processing
        this->processing = true;
        this->process_command();
      }
    }

    void PanaFanTimer::dump_config()
    {
      ESP_LOGCONFIG(TAG, "PanaFanTimer:");
      LOG_TEXT_SENSOR("  Fan Timer: ", "fan_timer", this);
    }

    void PanaFanTimer::setup()
    {
      this->set_icon("mdi:timer-outline");
    }

    void PanaFanTimer::set_fan_timer(uint8_t timer)
    {
      this->publish_state(STR_FANTIMER_STR[timer]);
    }

    void PanaFanSetTimer::dump_config()
    {
      ESP_LOGCONFIG(TAG, "PanaFanSetTimer:");
      LOG_BUTTON("  Fan Set Timer: ", "fan_settimer", this);
    }

    void PanaFanSetTimer::setup()
    {
    }

    void PanaFanSetTimer::press_action()
    {
      if (this->fan_->state)
      {
        this->fan_->command_queue.push_back(PANAFAN_CMD_TIMER);
        // start processing
        this->fan_->processing = true;
        this->fan_->process_command();
      }
    }

  } // namespace panafan
} // namespace esphome
