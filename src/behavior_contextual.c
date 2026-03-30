/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_contextual

#include <zephyr/device.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>

static uint32_t last_pressed_keycode = 0;

// Listen to the global keycode stream to track the previous key
int contextual_tracker_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    
    if (ev && ev->state) { 
        // Optional: Ignore modifiers (0xE0 to 0xE7) so "Shift + Q" still registers 'Q' as the last key
        bool is_mod = (ev->usage_page == 0x07 && ev->keycode >= 0xE0 && ev->keycode <= 0xE7);
        
        if (!is_mod) {
            // Re-encode to match the 32-bit values stored in the device tree
            last_pressed_keycode = ZMK_HID_USAGE(ev->usage_page, ev->keycode);
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(contextual_tracker, contextual_tracker_listener);
ZMK_SUBSCRIPTION(contextual_tracker, zmk_keycode_state_changed);

// Behavior implementation
struct behavior_contextual_config {
    uint32_t *pairs;
    int pairs_len;
};

static int behavior_contextual_init(const struct device *dev) { return 0; }

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_contextual_config *cfg = dev->config;

    uint32_t output_keycode = binding->param1; // This is your fallback key

    // Scan the config map for a match: pairs of [trigger, output]
    for (int i = 0; i < cfg->pairs_len; i += 2) {
        if (cfg->pairs[i] == last_pressed_keycode) {
            output_keycode = cfg->pairs[i+1];
            break;
        }
    }

    // Emit the resulting keypress to the OS
    raise_zmk_keycode_state_changed((struct zmk_keycode_state_changed){
        .usage_page = ZMK_HID_USAGE_PAGE(output_keycode),
        .keycode = ZMK_HID_USAGE_ID(output_keycode),
        .implicit_modifiers = ZMK_HID_MODS(output_keycode),
        .state = true,
        .timestamp = k_uptime_get()
    });

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_contextual_config *cfg = dev->config;

    uint32_t output_keycode = binding->param1; // Fallback

    for (int i = 0; i < cfg->pairs_len; i += 2) {
        if (cfg->pairs[i] == last_pressed_keycode) {
            output_keycode = cfg->pairs[i+1];
            break;
        }
    }

    // Release the key
    raise_zmk_keycode_state_changed((struct zmk_keycode_state_changed){
        .usage_page = ZMK_HID_USAGE_PAGE(output_keycode),
        .keycode = ZMK_HID_USAGE_ID(output_keycode),
        .implicit_modifiers = ZMK_HID_MODS(output_keycode),
        .state = false,
        .timestamp = k_uptime_get()
    });

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