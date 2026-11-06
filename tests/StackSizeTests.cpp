// What gets copied onto the stack stays small. Windows gives a thread 1 MB of stack (Linux 8 MB), and a
// preset, the mod matrix and the synth's settings are copied there for undo, loading and each audio block:
// 0.15.0's arp patterns (40 KB, inline in ModMatrix) made every test that built the plugin crash on Windows
// while Linux passed. (On Linux, `ulimit -s 1024` before the tests runs them with Windows' stack.)
#include "PluginProcessor.h"
#include "rigs/Rig.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE ("Stack: presets, the mod matrix, the settings and the engine stay small enough to copy on Windows' stack", "[stack]")
{
    CHECK (sizeof (ModMatrix) < 16 * 1024);
    CHECK (sizeof (Rig) < 16 * 1024);
    CHECK (sizeof (synth::Settings) < 16 * 1024);
    CHECK (sizeof (synth::Engine) < 64 * 1024);
    CHECK (sizeof (ParagonX1AudioProcessor) < 128 * 1024);
}

// 0.78.0: the factory presets' function held 59 preset builders at once, each a whole Rig on the stack (418 KB on
// Linux, more with Microsoft's compiler); one test crashed on Windows. A builder now keeps its preset on the heap.
#include "rigs/RigBuilder.h"
TEST_CASE ("Stack: a preset builder keeps its preset on the heap", "[stack]")
{
    CHECK (sizeof (rigbuilder::Builder) <= 64);
    rigbuilder::Builder a ("A", 1, { "Test" }, "notes");
    a.set ("patchLevel", 2.0f);
    rigbuilder::Builder b = a;   // a copy is its own preset
    b.rig.name = "B";
    CHECK (a.rig.name == "A");
    CHECK (b.rig.parameters.at ("patchLevel") == 2.0f);
}
