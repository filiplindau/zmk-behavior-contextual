/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_contextual

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/hid_usage_pages.h>

static uint32_t last_pressed_keycode = 0;

// GUARD: Only compile the listener on the Central half that processes the keymap
#if IS_ENABLED(CONFIG_ZMK_KEYMAP)

int contextual_tracker_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    
    if (ev && ev->state) { 
        if (ev->usage_page == HID_USAGE_KEY) {
            bool is_mod = (ev->keycode >= 0xE0 && ev->keycode <= 0xE7);
            if (!is_mod) {
                last_pressed_keycode = ZMK_HID_USAGE(ev->usage_page, ev->keycode);
            }
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(contextual_tracker, contextual_tracker_listener);
ZMK_SUBSCRIPTION(contextual_tracker, zmk_keycode_state_changed);

#endif // End of GUARD

struct behavior_contextual_config {
    uint32_t *pairs;
    int pairs_len;
};

static int behavior_contextual_init(const struct device *dev) { return 0; }

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_contextual_config *cfg = dev->config;

    uint32_t output_keycode = binding->param1;

    for (int i = 0; i < cfg->pairs_len; i += 2) {
        if (cfg->pairs[i] == last_pressed_keycode) {
            output_keycode = cfg->pairs[i+1];
            break;
        }
    }

    struct zmk_behavior_binding child = {
        .behavior_dev = "key_press",
        .param1 = output_keycode,
        .param2 = 0
    };
    
    zmk_behavior_invoke_binding(&child, event, true);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_contextual_config *cfg = dev->config;

    uint32_t output_keycode = binding->param1;

    for (int i = 0; i < cfg->pairs_len; i += 2) {
        if (cfg->pairs[i] == last_pressed_keycode) {
            output_keycode = cfg->pairs[i+1];
            break;
        }
    }

    struct zmk_behavior_binding child = {
        .behavior_dev = "key_press",
        .param1 = output_keycode,
        .param2 = 0
    };
    
    zmk_behavior_invoke_binding(&child, event, false);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_contextual_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define CONTEXTUAL_INST(n)                                                     \
    static uint32_t pairs_##n[] = DT_INST_PROP(n, key_pairs);                  \
    static const struct behavior_contextual_config behavior_contextual_config_##n = { \
        .pairs = pairs_##n,                                                    \
        .pairs_len = ARRAY_SIZE(pairs_##n),                                    \
    };                                                                         \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_contextual_init, NULL, NULL,           \
                            &behavior_contextual_config_##n, POST_KERNEL,      \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,               \
                            &behavior_contextual_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CONTEXTUAL_INST)