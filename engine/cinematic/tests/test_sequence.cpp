#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/cinematic/sequence.hpp>

using namespace engine::cinematic;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// SequenceInfo Tests
// ============================================================================

TEST_CASE("SequenceInfo defaults", "[cinematic][sequence]") {
    SequenceInfo info;

    REQUIRE(info.name.empty());
    REQUIRE(info.description.empty());
    REQUIRE(info.author.empty());
    REQUIRE_THAT(info.frame_rate, WithinAbs(30.0f, 0.001f));
    REQUIRE_FALSE(info.is_looping);
}

TEST_CASE("SequenceInfo configuration", "[cinematic][sequence]") {
    SequenceInfo info;
    info.name = "intro_cutscene";
    info.description = "Opening cinematic sequence";
    info.author = "Game Studio";
    info.frame_rate = 60.0f;
    info.is_looping = false;

    REQUIRE(info.name == "intro_cutscene");
    REQUIRE(info.description == "Opening cinematic sequence");
    REQUIRE(info.author == "Game Studio");
    REQUIRE_THAT(info.frame_rate, WithinAbs(60.0f, 0.001f));
}

// ============================================================================
// TrackGroup Tests
// ============================================================================

TEST_CASE("TrackGroup defaults", "[cinematic][sequence]") {
    TrackGroup group;

    REQUIRE(group.name.empty());
    REQUIRE(group.tracks.empty());
    REQUIRE_FALSE(group.collapsed);
    REQUIRE_FALSE(group.muted);
}

TEST_CASE("TrackGroup configuration", "[cinematic][sequence]") {
    TrackGroup group;
    group.name = "Camera Tracks";
    group.collapsed = true;
    group.muted = false;

    REQUIRE(group.name == "Camera Tracks");
    REQUIRE(group.collapsed);
    REQUIRE_FALSE(group.muted);
}

// ============================================================================
// Sequence::Section Tests
// ============================================================================

TEST_CASE("Sequence::Section configuration", "[cinematic][sequence]") {
    Sequence::Section section;
    section.name = "Act 1";
    section.start_time = 0.0f;
    section.end_time = 30.0f;
    section.color = 0xFF0000FF;  // Red

    REQUIRE(section.name == "Act 1");
    REQUIRE_THAT(section.start_time, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(section.end_time, WithinAbs(30.0f, 0.001f));
    REQUIRE(section.color == 0xFF0000FF);
}

// ============================================================================
// Sequence Tests
// ============================================================================

TEST_CASE("Sequence default constructor", "[cinematic][sequence]") {
    Sequence seq;

    REQUIRE(seq.get_name().empty());
    REQUIRE(seq.track_count() == 0);
    REQUIRE(seq.get_tracks().empty());
    REQUIRE(seq.get_markers().empty());
    REQUIRE(seq.get_sections().empty());
}

TEST_CASE("Sequence named constructor", "[cinematic][sequence]") {
    Sequence seq("boss_fight_intro");

    REQUIRE(seq.get_name() == "boss_fight_intro");
    REQUIRE(seq.track_count() == 0);
}

TEST_CASE("Sequence set_name", "[cinematic][sequence]") {
    Sequence seq;
    seq.set_name("my_sequence");

    REQUIRE(seq.get_name() == "my_sequence");
}

TEST_CASE("Sequence info access", "[cinematic][sequence]") {
    Sequence seq("test");

    seq.get_info().description = "Test sequence";
    seq.get_info().frame_rate = 24.0f;
    seq.get_info().is_looping = true;

    REQUIRE(seq.get_info().description == "Test sequence");
    REQUIRE_THAT(seq.get_info().frame_rate, WithinAbs(24.0f, 0.001f));
    REQUIRE(seq.get_info().is_looping);
}

TEST_CASE("Sequence add_camera_track", "[cinematic][sequence]") {
    Sequence seq("test");

    CameraTrack* track = seq.add_camera_track("main_camera");

    REQUIRE(track != nullptr);
    REQUIRE(seq.track_count() == 1);
    REQUIRE(track->get_name() == "main_camera");
    REQUIRE(track->get_type() == TrackType::Camera);
}

TEST_CASE("Sequence add_event_track", "[cinematic][sequence]") {
    Sequence seq("test");

    EventTrack* track = seq.add_event_track("game_events");

    REQUIRE(track != nullptr);
    REQUIRE(seq.track_count() == 1);
    REQUIRE(track->get_name() == "game_events");
    REQUIRE(track->get_type() == TrackType::Event);
}

TEST_CASE("Sequence add_audio_track", "[cinematic][sequence]") {
    Sequence seq("test");

    AudioTrack* track = seq.add_audio_track("sfx");

    REQUIRE(track != nullptr);
    REQUIRE(seq.track_count() == 1);
    REQUIRE(track->get_name() == "sfx");
}

TEST_CASE("Sequence get_track", "[cinematic][sequence]") {
    Sequence seq("test");
    seq.add_camera_track("cam1");
    seq.add_event_track("events");

    Track* cam = seq.get_track("cam1");
    Track* events = seq.get_track("events");
    Track* missing = seq.get_track("nonexistent");

    REQUIRE(cam != nullptr);
    REQUIRE(events != nullptr);
    REQUIRE(missing == nullptr);
}

TEST_CASE("Sequence get_track_as", "[cinematic][sequence]") {
    Sequence seq("test");
    seq.add_camera_track("cam1");

    CameraTrack* cam = seq.get_track_as<CameraTrack>("cam1");
    EventTrack* wrong = seq.get_track_as<EventTrack>("cam1");

    REQUIRE(cam != nullptr);
    REQUIRE(wrong == nullptr);  // Wrong type
}

TEST_CASE("Sequence remove_track", "[cinematic][sequence]") {
    Sequence seq("test");
    seq.add_camera_track("cam1");
    seq.add_event_track("events");

    REQUIRE(seq.track_count() == 2);

    seq.remove_track("cam1");

    REQUIRE(seq.track_count() == 1);
    REQUIRE(seq.get_track("cam1") == nullptr);
    REQUIRE(seq.get_track("events") != nullptr);
}

TEST_CASE("Sequence clear_tracks", "[cinematic][sequence]") {
    Sequence seq("test");
    seq.add_camera_track("cam1");
    seq.add_event_track("events");
    seq.add_audio_track("audio");

    REQUIRE(seq.track_count() == 3);

    seq.clear_tracks();

    REQUIRE(seq.track_count() == 0);
    REQUIRE(seq.get_tracks().empty());
}

TEST_CASE("Sequence markers", "[cinematic][sequence]") {
    Sequence seq("test");

    seq.add_marker("start", 0.0f);
    seq.add_marker("boss_enters", 5.0f);
    seq.add_marker("boss_defeated", 30.0f);

    REQUIRE(seq.get_markers().size() == 3);
    REQUIRE_THAT(seq.get_marker_time("start"), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(seq.get_marker_time("boss_enters"), WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(seq.get_marker_time("boss_defeated"), WithinAbs(30.0f, 0.001f));
}

TEST_CASE("Sequence remove_marker", "[cinematic][sequence]") {
    Sequence seq("test");

    seq.add_marker("marker1", 1.0f);
    seq.add_marker("marker2", 2.0f);

    seq.remove_marker("marker1");

    REQUIRE(seq.get_markers().size() == 1);
    REQUIRE(seq.get_markers().find("marker1") == seq.get_markers().end());
}

TEST_CASE("Sequence sections", "[cinematic][sequence]") {
    Sequence seq("test");

    Sequence::Section act1;
    act1.name = "Act 1";
    act1.start_time = 0.0f;
    act1.end_time = 60.0f;

    Sequence::Section act2;
    act2.name = "Act 2";
    act2.start_time = 60.0f;
    act2.end_time = 120.0f;

    seq.add_section(act1);
    seq.add_section(act2);

    REQUIRE(seq.get_sections().size() == 2);

    const Sequence::Section* found = seq.get_section("Act 1");
    REQUIRE(found != nullptr);
    REQUIRE_THAT(found->start_time, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(found->end_time, WithinAbs(60.0f, 0.001f));
}

TEST_CASE("Sequence get_section not found", "[cinematic][sequence]") {
    Sequence seq("test");

    const Sequence::Section* found = seq.get_section("nonexistent");
    REQUIRE(found == nullptr);
}
