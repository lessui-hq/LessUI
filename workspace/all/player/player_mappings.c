/**
 * player_mappings.c - Static button/label data for Player
 *
 * Contains all static mapping arrays and label strings.
 * Extracted from player.c for maintainability.
 */

#include "player_mappings.h"
#include "defines.h"
#include "libretro.h"

///////////////////////////////////////
// Label Arrays
///////////////////////////////////////

char* player_onoff_labels[] = {"Off", "On", NULL};

char* player_scaling_labels[] = {"Native", "Aspect", "Fullscreen", "Cropped", NULL};

char* player_effect_labels[] = {"None", "Lines", "Grid", "CRT", "Slot", NULL};

char* player_sharpness_labels[] = {"Sharp", "Crisp", "Soft", NULL};

char* player_max_ff_labels[] = {
    "None", "2x", "3x", "4x", "5x", "6x", "7x", "8x", NULL,
};

char* player_overclock_labels[] = {
    "Powersave", "Normal", "Performance", "Auto", NULL,
};

// Button labels for UI display
// NOTE: Must be in BTN_ID_ order, offset by 1 because of NONE (which is -1 in BTN_ID_ land)
char* player_button_labels[] = {
    "NONE", // displayed by default
    "UP",      "DOWN",    "LEFT",    "RIGHT",      "A",           "B",          "X",
    "Y",       "START",   "SELECT",  "L1",         "R1",          "L2",         "R2",
    "L3",      "R3",      "MENU+UP", "MENU+DOWN",  "MENU+LEFT",   "MENU+RIGHT", "MENU+A",
    "MENU+B",  "MENU+X",  "MENU+Y",  "MENU+START", "MENU+SELECT", "MENU+L1",    "MENU+R1",
    "MENU+L2", "MENU+R2", "MENU+L3", "MENU+R3",    NULL,
};

// TODO: This should be provided by the core
char* player_gamepad_labels[] = {
    "Standard",
    "DualShock",
    NULL,
};

char* player_gamepad_values[] = {
    "1",
    "517",
    NULL,
};

///////////////////////////////////////
// Button Mappings
///////////////////////////////////////

// Default button mapping - used if pak.cfg doesn't exist or doesn't have bindings
PlayerButtonMapping player_default_button_mapping[] = {
    {.name = "Up",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_UP,
     .local_id = BTN_ID_DPAD_UP,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "Down",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_DOWN,
     .local_id = BTN_ID_DPAD_DOWN,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "Left",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_LEFT,
     .local_id = BTN_ID_DPAD_LEFT,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "Right",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_RIGHT,
     .local_id = BTN_ID_DPAD_RIGHT,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "A Button",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_A,
     .local_id = BTN_ID_A,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "B Button",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_B,
     .local_id = BTN_ID_B,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "X Button",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_X,
     .local_id = BTN_ID_X,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "Y Button",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_Y,
     .local_id = BTN_ID_Y,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "Start",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_START,
     .local_id = BTN_ID_START,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "Select",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_SELECT,
     .local_id = BTN_ID_SELECT,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "L1 Button",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_L,
     .local_id = BTN_ID_L1,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "R1 Button",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_R,
     .local_id = BTN_ID_R1,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "L2 Button",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_L2,
     .local_id = BTN_ID_L2,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "R2 Button",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_R2,
     .local_id = BTN_ID_R2,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "L3 Button",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_L3,
     .local_id = BTN_ID_L3,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "R3 Button",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_R3,
     .local_id = BTN_ID_R3,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = NULL, .retro_id = 0, .local_id = 0, .modifier = 0, .default_id = 0, .ignore = 0}};

// Button label mapping - used to lookup retro_id and local_id from button name
PlayerButtonMapping player_button_label_mapping[] = {
    {.name = "NONE",
     .retro_id = -1,
     .local_id = BTN_ID_NONE,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "UP",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_UP,
     .local_id = BTN_ID_DPAD_UP,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "DOWN",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_DOWN,
     .local_id = BTN_ID_DPAD_DOWN,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "LEFT",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_LEFT,
     .local_id = BTN_ID_DPAD_LEFT,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "RIGHT",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_RIGHT,
     .local_id = BTN_ID_DPAD_RIGHT,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "A",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_A,
     .local_id = BTN_ID_A,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "B",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_B,
     .local_id = BTN_ID_B,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "X",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_X,
     .local_id = BTN_ID_X,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "Y",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_Y,
     .local_id = BTN_ID_Y,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "START",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_START,
     .local_id = BTN_ID_START,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "SELECT",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_SELECT,
     .local_id = BTN_ID_SELECT,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "L1",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_L,
     .local_id = BTN_ID_L1,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "R1",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_R,
     .local_id = BTN_ID_R1,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "L2",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_L2,
     .local_id = BTN_ID_L2,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "R2",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_R2,
     .local_id = BTN_ID_R2,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "L3",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_L3,
     .local_id = BTN_ID_L3,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = "R3",
     .retro_id = RETRO_DEVICE_ID_JOYPAD_R3,
     .local_id = BTN_ID_R3,
     .modifier = 0,
     .default_id = 0,
     .ignore = 0},
    {.name = NULL, .retro_id = 0, .local_id = 0, .modifier = 0, .default_id = 0, .ignore = 0}};

// Device button names indexed by BTN_ID_*
const char* player_device_button_names[LOCAL_BUTTON_COUNT] = {
    [BTN_ID_DPAD_UP] = "UP",
    [BTN_ID_DPAD_DOWN] = "DOWN",
    [BTN_ID_DPAD_LEFT] = "LEFT",
    [BTN_ID_DPAD_RIGHT] = "RIGHT",
    [BTN_ID_SELECT] = "SELECT",
    [BTN_ID_START] = "START",
    [BTN_ID_Y] = "Y",
    [BTN_ID_X] = "X",
    [BTN_ID_B] = "B",
    [BTN_ID_A] = "A",
    [BTN_ID_L1] = "L1",
    [BTN_ID_R1] = "R1",
    [BTN_ID_L2] = "L2",
    [BTN_ID_R2] = "R2",
    [BTN_ID_L3] = "L3",
    [BTN_ID_R3] = "R3",
};
