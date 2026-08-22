/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * boards/shields/dongle_screen/src/brightness.h
 *
 * Public interface for the dongle-screen brightness/fade subsystem.
 * Include this wherever you need to trigger an animated brightness change.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Animate the display backlight to a target brightness.
 *
 * Queues a fade from the current backlight level to @p target_pct
 * using a cubic ease-in-out curve over CONFIG_DONGLE_SCREEN_ANIMATION_MS.
 * If a fade is already in progress it is superseded immediately.
 *
 * Safe to call from any thread or work-queue context.
 *
 * @param target_pct  Target brightness percent [0, 100].
 */
void dongle_screen_fade_to_brt(uint8_t target_pct);

/**
 * @brief Wake the screen when a peripheral reconnects
 * Called by battery widget when it detects a peripheral reconnection
 */
void brightness_wake_screen_on_reconnect(void);
