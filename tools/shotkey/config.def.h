/**
 * config.def.h
 */

#include <X11/XF86keysym.h>

// Shell
char shell[] = "/bin/sh";

#define Super Mod4Mask

enum {
  Control,
  Execute,
  // Declare modes above this
  MODE_SIZE,
};

// Define mode key bindings here
// NOTE: "10" here is the maximum number of key bindings for each mode
// NOTE: Push cmd to backgroud with `&` to prevent zombie process creation
Key modes[MODE_SIZE][10] = {
  [Control] = {
    { 0, XK_j, cmd("amixer sset Master 5%- &") },
    { 0, XK_k, cmd("amixer sset Master 5%+ &") },
    { 0, XK_m, cmd("amixer set Master toggle &") },
  },
  [Execute] = {
    { 0, XK_c, cmd("st &") },
    { 0, XK_e, cmd("emacs &") },
    { 0, XK_f, cmd("pcmanfm &") },
    { 0, XK_g, cmd("google-chrome &") },
    { 0, XK_t, cmd("urxvt &") },
    { 0, XK_u, cmd("code &") },
  },
};

// Define normal mode key bindings here
Key keys[] = {
  { Super, XK_y, mode(Control, True) },
  { Super, XK_a, mode(Execute, False) },
};

ModeProperties mode_properties[MODE_SIZE] = {
  [Control] = { "Control mode" },
  [Execute] = { "Execute mode" },
};

// Call this script on mode change
char* on_mode_change = "echo \"kadj [$SHOTKEY_MODE_ID] $SHOTKEY_MODE_LABEL\"";
