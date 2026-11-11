#include "../tools/diagnostics/flycast_profile_policy.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>

int main() {
    using namespace flycast_profile;
    unsigned n = 99;
    assert(parse_unsigned("0", 0, 100000, n) && n == 0);
    assert(parse_unsigned("1800", 0, 100000, n) && n == 1800);
    for (const char* bad : {"", "-1", "+2", "2x", " 2", "100001", "999999999999999999999"})
        assert(!parse_unsigned(bad, 0, 100000, n));
    assert(!parse_unsigned("0", 1, 100000, n));
    InputScript script;
    assert(script.buttons(0.0).a);
    assert(!script.buttons(0.5).a);
    assert(!script.buttons(15.0).a);
    assert(!script.buttons(19.9).start);
    assert(script.buttons(20.0).start);
    assert(!script.buttons(20.3).start);
    assert(!script.buttons(20.0).start); // never issue a second Start edge
    assert(script.buttons(21.0).a);
    assert(!script.buttons(28.0).a);
    assert(!script.buttons(30.0).a && !script.buttons(30.0).start);
    InputScript delayed;
    assert(delayed.buttons(20.7).start); // audio batching must not skip Start
    assert(!delayed.buttons(21.0).start);
    FrameCounts counts;
    counts.observe(FrameKind::fresh);
    counts.observe(FrameKind::duplicate);
    counts.observe(FrameKind::fresh);
    counts.observe(FrameKind::unexpected);
    assert(counts.fresh == 2 && counts.duplicates == 1 && counts.unexpected == 1);
    assert(option_overrides().at("reicast_hle_bios") == "enabled");
    assert(option_overrides().at("reicast_enable_rttb") == "enabled");
    assert(option_overrides().at("reicast_enable_dsp") == "enabled");
    assert(option_overrides().at("reicast_sh4clock") == "200");
    assert(option_overrides().at("reicast_frame_skipping") == "disabled");
    assert(!valid_private_path("/"));
    assert(!valid_private_path("relative/profile"));
    assert(!valid_private_path("/data/local/tmp/../profile"));
    assert(valid_private_path("/data/local/tmp/vr4all-silent-scope-dc-run01"));
    assert(legacy_default("Choice; enabled|disabled") == "enabled");
    assert(legacy_default("bad definition").empty());
    assert(!audio_gate(0, 44100, 35));
    assert(!audio_gate(10000000, 0, 35));
    assert(!audio_gate(10000000, NAN, 35));
    assert(!audio_gate(35 * 44100 - 1, 44100, 35));
    assert(audio_gate(35 * 44100, 44100, 35));
    PendingValue<unsigned> pending;
    unsigned current = 7;
    assert(!pending.take(current) && current == 7);
    std::thread producer([&] { for (unsigned value = 1; value <= 1000; ++value) pending.publish(value); });
    producer.join();
    assert(pending.take(current) && current == 1000);
    assert(!pending.take(current) && current == 1000);
    std::cout << "flycast profile policy tests passed\n";
}
