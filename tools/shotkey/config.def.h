/**
 * config.def.h
 */

#include <X11/XF86keysym.h>

// Shell
char shell[] = "/bin/sh";

#define MODKEY Mod4Mask

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
    { 0, XK_g, cmd("dmenu_drun google-chrome &") },
    { 0, XK_u, cmd("dmenu_drun code &") },
  },
};

// Define normal mode key bindings here
Key keys[] = {
  { MODKEY,                       XK_Escape,     mode(Control, True) },
  { MODKEY,                       XK_a,          mode(Execute, False) },
  { MODKEY|Mod1Mask,              XK_c,          cmd("dmenu_app st &") },
  { MODKEY|ControlMask,           XK_c,          cmd("focuswin st &") },
  { MODKEY|ShiftMask,             XK_c,          cmd("movewin &") },
  { MODKEY|ControlMask|ShiftMask, XK_c,          cmd("deckwin &") },
  { MODKEY|Mod1Mask,              XK_e,          cmd("dmenu_app emacs ec &") },
  { MODKEY|ControlMask,           XK_e,          cmd("focuswin emacs ec &") },
  { MODKEY|ShiftMask,             XK_e,          cmd("emacs &") },
  { MODKEY|ShiftMask,             XK_t,          cmd("urxvt &") },
  { MODKEY|Mod1Mask,              XK_t,          cmd("dmenu_app urxvt &") },
  { MODKEY|ControlMask,           XK_t,          cmd("focuswin urxvt &") },
  { MODKEY|ShiftMask,             XK_g,          cmd("dmenu_drun google-chrome &") },
  { MODKEY|Mod1Mask,              XK_g,          cmd("dmenu_app google-chrome &") },
  { MODKEY|ShiftMask,             XK_u,          cmd("dmenu_drun code &") },
  { MODKEY|Mod1Mask,              XK_u,          cmd("dmenu_app code &") },
  { MODKEY,                       XK_r,          cmd("dmenu_run &") },
  { MODKEY|Mod1Mask,              XK_r,          cmd("dmenu_run &") },
  { MODKEY,                       XK_s,          cmd("dmenu_ssh &") },
  { MODKEY|Mod1Mask,              XK_s,          cmd("dmenu_ssh &") },
  { MODKEY,                       XK_w,          cmd("dmenu_win &") },
  { MODKEY|Mod1Mask,              XK_w,          cmd("dmenu_win &") },
  { MODKEY,                       XK_p,          cmd("dmenu_drun &") },
  { MODKEY|Mod1Mask,              XK_p,          cmd("dmenu_drun &") },
  { MODKEY,                       XK_q,          cmd("dmenu_exit &") },
  { MODKEY|Mod1Mask,              XK_q,          cmd("dmenu_exit &") },
  { MODKEY|Mod1Mask,              XK_i,          cmd("dmenu_info &") },
  { MODKEY,                       XK_n,          cmd("nextwin &") },
  { MODKEY|ShiftMask,             XK_n,          cmd("prevwin &") },
  { MODKEY|ShiftMask,             XK_z,          cmd("slock &") },
  { MODKEY|ShiftMask,             XK_Return,     cmd("dupterm &") },
  { MODKEY,                       XK_slash,      cmd("dmenu_man &") },
  { MODKEY,                       XK_apostrophe, cmd("dmenu_desk &") },
  { MODKEY,                       XK_grave,      cmd("dmenu_menu &") },
  { MODKEY,                       XK_semicolon,  cmd("dmenu_wincd &") },
};

ModeProperties mode_properties[MODE_SIZE] = {
  [Control] = { "Control mode" },
  [Execute] = { "Execute mode" },
};

// Call this script on mode change
char* on_mode_change = "echo \"kadj [$SHOTKEY_MODE_ID] $SHOTKEY_MODE_LABEL\"";
