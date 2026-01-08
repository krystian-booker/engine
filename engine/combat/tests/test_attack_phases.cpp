#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/combat/attack_phases.hpp>

using namespace engine::combat;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// AttackPhase Tests
// ============================================================================

TEST_CASE("AttackPhase enum", "[combat][attack]") {
    REQUIRE(static_cast<uint8_t>(AttackPhase::None) == 0);
    REQUIRE(static_cast<uint8_t>(AttackPhase::Startup) == 1);
    REQUIRE(static_cast<uint8_t>(AttackPhase::Active) == 2);
    REQUIRE(static_cast<uint8_t>(AttackPhase::Recovery) == 3);
    REQUIRE(static_cast<uint8_t>(AttackPhase::Canceled) == 4);
}

// ============================================================================
// AttackDefinition Tests
// ============================================================================

TEST_CASE("AttackDefinition defaults", "[combat][attack]") {
    AttackDefinition attack;

    REQUIRE(attack.name.empty());
    REQUIRE_THAT(attack.startup_duration, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(attack.active_duration, WithinAbs(0.2f, 0.001f));
    REQUIRE_THAT(attack.recovery_duration, WithinAbs(0.3f, 0.001f));
    REQUIRE(attack.can_cancel_startup == true);
    REQUIRE(attack.can_cancel_into_dodge == true);
    REQUIRE(attack.can_cancel_into_attack == false);
    REQUIRE_THAT(attack.cancel_window_start, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(attack.cancel_window_end, WithinAbs(0.9f, 0.001f));
    REQUIRE(attack.next_combo_attack.empty());
    REQUIRE(attack.combo_position == 0);
    REQUIRE(attack.max_combo_chain == 3);
    REQUIRE_THAT(attack.forward_movement, WithinAbs(0.0f, 0.001f));
    REQUIRE(attack.root_motion == false);
    REQUIRE(attack.can_rotate == false);
    REQUIRE(attack.hitbox_ids.empty());
    REQUIRE(attack.animation_name.empty());
    REQUIRE_THAT(attack.animation_speed, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("AttackDefinition custom values", "[combat][attack]") {
    AttackDefinition attack;
    attack.name = "heavy_slash";
    attack.startup_duration = 0.3f;
    attack.active_duration = 0.4f;
    attack.recovery_duration = 0.5f;
    attack.can_cancel_startup = false;
    attack.can_cancel_into_dodge = true;
    attack.can_cancel_into_attack = true;
    attack.cancel_window_start = 0.6f;
    attack.cancel_window_end = 0.95f;
    attack.next_combo_attack = "finishing_blow";
    attack.combo_position = 1;
    attack.max_combo_chain = 5;
    attack.forward_movement = 2.0f;
    attack.root_motion = true;
    attack.can_rotate = true;
    attack.hitbox_ids = {"sword_hitbox", "sword_tip_hitbox"};
    attack.animation_name = "anim_heavy_slash";
    attack.animation_speed = 1.2f;

    REQUIRE(attack.name == "heavy_slash");
    REQUIRE_THAT(attack.startup_duration, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(attack.active_duration, WithinAbs(0.4f, 0.001f));
    REQUIRE_THAT(attack.recovery_duration, WithinAbs(0.5f, 0.001f));
    REQUIRE_FALSE(attack.can_cancel_startup);
    REQUIRE(attack.can_cancel_into_dodge);
    REQUIRE(attack.can_cancel_into_attack);
    REQUIRE_THAT(attack.cancel_window_start, WithinAbs(0.6f, 0.001f));
    REQUIRE_THAT(attack.cancel_window_end, WithinAbs(0.95f, 0.001f));
    REQUIRE(attack.next_combo_attack == "finishing_blow");
    REQUIRE(attack.combo_position == 1);
    REQUIRE(attack.max_combo_chain == 5);
    REQUIRE_THAT(attack.forward_movement, WithinAbs(2.0f, 0.001f));
    REQUIRE(attack.root_motion);
    REQUIRE(attack.can_rotate);
    REQUIRE(attack.hitbox_ids.size() == 2);
    REQUIRE(attack.hitbox_ids[0] == "sword_hitbox");
    REQUIRE(attack.animation_name == "anim_heavy_slash");
    REQUIRE_THAT(attack.animation_speed, WithinAbs(1.2f, 0.001f));
}

// ============================================================================
// AttackPhaseComponent Tests
// ============================================================================

TEST_CASE("AttackPhaseComponent defaults", "[combat][attack]") {
    AttackPhaseComponent attack;

    REQUIRE(attack.current_phase == AttackPhase::None);
    REQUIRE_THAT(attack.phase_time, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(attack.phase_duration, WithinAbs(0.0f, 0.001f));
    REQUIRE(attack.current_attack.empty());
    REQUIRE(attack.combo_count == 0);
    REQUIRE_THAT(attack.combo_window_timer, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(attack.combo_window_duration, WithinAbs(0.5f, 0.001f));
    REQUIRE(attack.queued_attack.empty());
    REQUIRE_THAT(attack.hitstop_remaining, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("AttackPhaseComponent is_attacking", "[combat][attack]") {
    AttackPhaseComponent attack;

    SECTION("Not attacking when None") {
        attack.current_phase = AttackPhase::None;
        REQUIRE_FALSE(attack.is_attacking());
    }

    SECTION("Attacking during Startup") {
        attack.current_phase = AttackPhase::Startup;
        REQUIRE(attack.is_attacking());
    }

    SECTION("Attacking during Active") {
        attack.current_phase = AttackPhase::Active;
        REQUIRE(attack.is_attacking());
    }

    SECTION("Attacking during Recovery") {
        attack.current_phase = AttackPhase::Recovery;
        REQUIRE(attack.is_attacking());
    }

    SECTION("Not attacking when Canceled") {
        attack.current_phase = AttackPhase::Canceled;
        REQUIRE_FALSE(attack.is_attacking());
    }
}

TEST_CASE("AttackPhaseComponent phase queries", "[combat][attack]") {
    AttackPhaseComponent attack;

    SECTION("is_in_startup") {
        attack.current_phase = AttackPhase::Startup;
        REQUIRE(attack.is_in_startup());
        REQUIRE_FALSE(attack.is_in_active());
        REQUIRE_FALSE(attack.is_in_recovery());
    }

    SECTION("is_in_active") {
        attack.current_phase = AttackPhase::Active;
        REQUIRE_FALSE(attack.is_in_startup());
        REQUIRE(attack.is_in_active());
        REQUIRE_FALSE(attack.is_in_recovery());
    }

    SECTION("is_in_recovery") {
        attack.current_phase = AttackPhase::Recovery;
        REQUIRE_FALSE(attack.is_in_startup());
        REQUIRE_FALSE(attack.is_in_active());
        REQUIRE(attack.is_in_recovery());
    }
}

TEST_CASE("AttackPhaseComponent get_phase_progress", "[combat][attack]") {
    AttackPhaseComponent attack;
    attack.phase_duration = 0.5f;

    SECTION("Progress at start") {
        attack.phase_time = 0.0f;
        float progress = attack.get_phase_progress();
        REQUIRE_THAT(progress, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Progress at middle") {
        attack.phase_time = 0.25f;
        float progress = attack.get_phase_progress();
        REQUIRE_THAT(progress, WithinAbs(0.5f, 0.01f));
    }

    SECTION("Progress at end") {
        attack.phase_time = 0.5f;
        float progress = attack.get_phase_progress();
        REQUIRE_THAT(progress, WithinAbs(1.0f, 0.001f));
    }

    SECTION("Zero duration returns 1") {
        attack.phase_duration = 0.0f;
        float progress = attack.get_phase_progress();
        REQUIRE_THAT(progress, WithinAbs(1.0f, 0.001f));
    }
}

TEST_CASE("AttackPhaseComponent get_total_progress", "[combat][attack]") {
    AttackPhaseComponent attack;
    attack.attack_def.startup_duration = 0.1f;
    attack.attack_def.active_duration = 0.2f;
    attack.attack_def.recovery_duration = 0.3f;
    // Total = 0.6f

    SECTION("Not attacking returns 0") {
        attack.current_phase = AttackPhase::None;
        float progress = attack.get_total_progress();
        REQUIRE_THAT(progress, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Startup phase progress") {
        attack.current_phase = AttackPhase::Startup;
        attack.phase_time = 0.05f;  // Halfway through startup
        float progress = attack.get_total_progress();
        // 0.05 / 0.6 = ~0.083
        REQUIRE_THAT(progress, WithinAbs(0.083f, 0.01f));
    }

    SECTION("Active phase progress") {
        attack.current_phase = AttackPhase::Active;
        attack.phase_time = 0.1f;  // Halfway through active
        float progress = attack.get_total_progress();
        // (0.1 + 0.1) / 0.6 = ~0.333
        REQUIRE_THAT(progress, WithinAbs(0.333f, 0.01f));
    }

    SECTION("Recovery phase progress") {
        attack.current_phase = AttackPhase::Recovery;
        attack.phase_time = 0.15f;  // Halfway through recovery
        float progress = attack.get_total_progress();
        // (0.1 + 0.2 + 0.15) / 0.6 = 0.75
        REQUIRE_THAT(progress, WithinAbs(0.75f, 0.01f));
    }
}

TEST_CASE("AttackPhaseComponent can_cancel", "[combat][attack]") {
    AttackPhaseComponent attack;
    attack.attack_def.can_cancel_startup = true;
    attack.attack_def.cancel_window_start = 0.5f;
    attack.attack_def.cancel_window_end = 0.9f;

    SECTION("Can cancel during startup if allowed") {
        attack.current_phase = AttackPhase::Startup;
        REQUIRE(attack.can_cancel());
    }

    SECTION("Cannot cancel during startup if not allowed") {
        attack.attack_def.can_cancel_startup = false;
        attack.current_phase = AttackPhase::Startup;
        REQUIRE_FALSE(attack.can_cancel());
    }

    SECTION("Cannot cancel during active phase") {
        attack.current_phase = AttackPhase::Active;
        REQUIRE_FALSE(attack.can_cancel());
    }

    SECTION("Can cancel during recovery in window") {
        attack.current_phase = AttackPhase::Recovery;
        attack.phase_duration = 1.0f;
        attack.phase_time = 0.7f;  // 70% through recovery
        REQUIRE(attack.can_cancel());
    }

    SECTION("Cannot cancel during recovery outside window - too early") {
        attack.current_phase = AttackPhase::Recovery;
        attack.phase_duration = 1.0f;
        attack.phase_time = 0.3f;  // 30% through recovery
        REQUIRE_FALSE(attack.can_cancel());
    }

    SECTION("Cannot cancel during recovery outside window - too late") {
        attack.current_phase = AttackPhase::Recovery;
        attack.phase_duration = 1.0f;
        attack.phase_time = 0.95f;  // 95% through recovery
        REQUIRE_FALSE(attack.can_cancel());
    }
}

TEST_CASE("AttackPhaseComponent can_combo", "[combat][attack]") {
    AttackPhaseComponent attack;
    attack.attack_def.can_cancel_into_attack = true;
    attack.attack_def.max_combo_chain = 3;
    attack.attack_def.cancel_window_start = 0.5f;
    attack.attack_def.cancel_window_end = 0.9f;

    SECTION("Cannot combo if not allowed") {
        attack.attack_def.can_cancel_into_attack = false;
        REQUIRE_FALSE(attack.can_combo());
    }

    SECTION("Cannot combo at max chain") {
        attack.combo_count = 3;
        REQUIRE_FALSE(attack.can_combo());
    }

    SECTION("Can combo during cancel window") {
        attack.current_phase = AttackPhase::Recovery;
        attack.phase_duration = 1.0f;
        attack.phase_time = 0.7f;
        attack.combo_count = 1;
        REQUIRE(attack.can_combo());
    }
}

TEST_CASE("AttackPhaseComponent queue_attack", "[combat][attack]") {
    AttackPhaseComponent attack;
    REQUIRE(attack.queued_attack.empty());

    attack.queue_attack("next_slash");
    REQUIRE(attack.queued_attack == "next_slash");
}

TEST_CASE("AttackPhaseComponent clear", "[combat][attack]") {
    AttackPhaseComponent attack;
    attack.current_phase = AttackPhase::Active;
    attack.phase_time = 0.5f;
    attack.phase_duration = 1.0f;
    attack.current_attack = "slash";
    attack.queued_attack = "thrust";
    attack.attack_def.name = "slash";

    attack.clear();

    REQUIRE(attack.current_phase == AttackPhase::None);
    REQUIRE_THAT(attack.phase_time, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(attack.phase_duration, WithinAbs(0.0f, 0.001f));
    REQUIRE(attack.current_attack.empty());
    REQUIRE(attack.queued_attack.empty());
    REQUIRE(attack.attack_def.name.empty());
}

TEST_CASE("AttackPhaseComponent combo tracking", "[combat][attack]") {
    AttackPhaseComponent attack;
    attack.combo_count = 2;
    attack.combo_window_timer = 0.3f;
    attack.combo_window_duration = 0.5f;

    REQUIRE(attack.combo_count == 2);
    REQUIRE_THAT(attack.combo_window_timer, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(attack.combo_window_duration, WithinAbs(0.5f, 0.001f));
}

TEST_CASE("AttackPhaseComponent hitstop tracking", "[combat][attack]") {
    AttackPhaseComponent attack;
    attack.hitstop_remaining = 0.05f;

    REQUIRE_THAT(attack.hitstop_remaining, WithinAbs(0.05f, 0.001f));
}
