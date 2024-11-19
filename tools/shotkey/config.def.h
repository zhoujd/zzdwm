/**
 * config.def.h
 */

#include <X11/XF86keysym.h>

// Shell
char shell[] = "/bin/sh";

#define Super Mod4Mask

enum {
  VolumeControl,
  Execute,
  // Declare modes above this
  MODE_SIZE,
};

// Define mode key bindings here
// NOTE: "10" here is the maximum number of key bindings for each mode
Key modes[MODE_SIZE][10] = {
  [VolumeControl] = {
    { 0, XK_j, cmd("amixer sset Master '5%-'") },
    { 0, XK_k, cmd("amixer sset Master '5%+'") },
  },
  [Execute] = {
    { 0, XK_c,       cmd("urxvt") },
  },
};

// Define normal mode key bindings here
Key keys[] = {
  { Super,  XK_y,             mode(VolumeControl, True) },
  { Super,  XK_a,             mode(Execute, False) },
};

ModeProperties mode_properties[MODE_SIZE] = {
  [VolumeControl] = { "VolumeControl mode" },
  [Execute] = { "Execute mode" },
};

// Call this script on mode change
char* on_mode_change = "echo \"kadj [$SHOTKEY_MODE_ID] $SHOTKEY_MODE_LABEL\"";
