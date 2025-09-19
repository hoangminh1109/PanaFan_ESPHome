# Copyright 2025 Minh Hoang
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan, i2c, text_sensor, button, remote_base
from esphome.const import (
    CONF_OUTPUT_ID,
    CONF_MODEL,
    CONF_ID,
    CONF_NAME,
    CONF_DISABLED_BY_DEFAULT
)

DEPENDENCIES = ["remote_transmitter", "i2c"]
AUTO_LOAD = ["fan", "remote_base", "i2c", "text_sensor", "button"]

panafan_ns = cg.esphome_ns.namespace('panafan')
PanaFan = panafan_ns.class_(
    "PanaFan",
    cg.Component,
    fan.Fan,
    i2c.I2CDevice,
    remote_base.RemoteReceiverListener,
    remote_base.RemoteTransmittable,
    )
PanaFanTimer = panafan_ns.class_(
    'PanaFanTimer',
    text_sensor.TextSensor,
    cg.Component
    )
PanaFanSetTimer = panafan_ns.class_(
    'PanaFanSetTimer',
    button.Button,
    cg.Component
    )

Model = panafan_ns.enum("Model")
MODELS = {
    "f409m": Model.PANAFAN_F409M,
}

CONF_FANTIMER_ID = "fantimer_id"
CONF_FANSETTIMER_ID = "fansettimer_id"
CONF_INTERVAL_MS = "interval"

CONFIG_SCHEMA = (fan.fan_schema(PanaFan).extend({
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(PanaFan),
        cv.GenerateID(CONF_FANTIMER_ID): cv.declare_id(PanaFanTimer),
        cv.GenerateID(CONF_FANSETTIMER_ID): cv.declare_id(PanaFanSetTimer),
        cv.Required(CONF_MODEL): cv.enum(MODELS),
        cv.Optional(CONF_INTERVAL_MS, default=10): cv.int_range(min=1),
        cv.Optional(remote_base.CONF_RECEIVER_ID): cv.use_id(remote_base.RemoteReceiverBase),
    })
    .extend(cv.COMPONENT_SCHEMA)
    .extend(remote_base.REMOTE_TRANSMITTABLE_SCHEMA)
    .extend(i2c.i2c_device_schema(0x20))
    )


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await fan.register_fan(var, config)
    await i2c.register_i2c_device(var, config)
    await remote_base.register_transmittable(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))

    # Fan timer text_sensor
    fantimer_default_config = { CONF_ID: config[CONF_FANTIMER_ID],
                                CONF_NAME: "Timer",
                                CONF_DISABLED_BY_DEFAULT: False}
    fantimer = cg.new_Pvariable(config[CONF_FANTIMER_ID])
    await text_sensor.register_text_sensor(fantimer, fantimer_default_config)
    await cg.register_component(fantimer, fantimer_default_config)
    cg.add(fantimer.set_parent_fan(var))
    cg.add(var.set_fan_timer(fantimer))

    # Fan set timer button
    fansettimer_default_config = { CONF_ID: config[CONF_FANSETTIMER_ID],
                                CONF_NAME: "Set Timer",
                                CONF_DISABLED_BY_DEFAULT: False}
    fansettimer = cg.new_Pvariable(config[CONF_FANSETTIMER_ID])
    await button.register_button(fansettimer, fansettimer_default_config)
    await cg.register_component(fansettimer, fansettimer_default_config)
    cg.add(fansettimer.set_parent_fan(var))
    cg.add(var.set_fan_settimer(fansettimer))

    cg.add(var.set_interval_ms(config[CONF_INTERVAL_MS]))

    if remote_base.CONF_RECEIVER_ID in config:
        await remote_base.register_listener(var, config)
