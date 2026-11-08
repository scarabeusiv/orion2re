#pragma once

namespace SETTINGS_SCREEN {
    // Runs the in-game settings screen: every interface and
    // synchronized_interface setting from the spec, with an editor picked
    // from the leaf type (booleans toggle, small enums open a selection
    // list, integers and strings take an edit field). A change is written
    // to the user orion2re.cfg and reloaded live; settings the spec marks
    // as taking effect on the next start are flagged with "*". Settings
    // with no editor render as info rows. Returns to the main menu when
    // closed.
    void __cdecl Settings_Screen();

} // namespace SETTINGS_SCREEN
