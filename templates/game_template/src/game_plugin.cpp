// Game Plugin Entry Point
// This file instantiates the plugin exports defined by IMPLEMENT_GAME_PLUGIN

#include "game.hpp"

// The IMPLEMENT_GAME_PLUGIN macro in game.hpp generates all exported functions:
// - game_get_info()
// - game_init()
// - game_register_systems()
// - game_register_components()
// - game_pre_reload()
// - game_post_reload()
// - game_shutdown()
//
// These are the entry points the engine uses to interact with the game DLL.
