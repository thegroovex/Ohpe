// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TestHelpers.h"
#include "engines/AnalogEngine.h"
#include "engines/EngineFactory.h"
#include "engines/VoiceAllocator.h"

#include <doctest/doctest.h>

#include <cstring>

using namespace ohpe;
using namespace ohpe::test;

TEST_CASE ("voice allocator: free slots, same-note retrigger, steal oldest released first")
{
    VoiceAllocator<4> a;
    a.setPolyphony (3);
    bool stolen = false;
    CHECK (a.allocate (60, stolen) == 0);
    CHECK_FALSE (stolen);
    CHECK (a.allocate (62, stolen) == 1);
    CHECK (a.allocate (64, stolen) == 2);
    CHECK (a.allocate (62, stolen) == 1); // same note retriggers its voice
    CHECK (stolen);
    a.release (64, [] (int) {});
    CHECK (a.allocate (67, stolen) == 2); // released voice is stolen before held ones
    CHECK (stolen);
    CHECK (a.allocate (69, stolen) == 0); // oldest held
}

TEST_CASE ("engine respects polyphony and hard all-notes-off")
{
    AnalogEngine e;
    e.prepare (sampleRate, 512);
    e.setCommon (plainCommon());
    e.setPolyphony (4);
    for (int n = 0; n < 10; ++n) e.noteOn (48 + n, 1.0f);
    renderEngine (e, 512);
    CHECK (e.activeVoiceCount() == 4);
    e.allNotesOff (true);
    CHECK (e.activeVoiceCount() == 0);
    auto out = renderEngine (e, 512);
    CHECK (peak (out.l) == 0.0f);
}

TEST_CASE ("every implemented engine follows the ISynthEngine contract")
{
    for (int t = 0; t < numEngineTypes; ++t)
    {
        const auto type = (EngineType) t;
        auto e = createEngine (type);
        CHECK ((e != nullptr) == engineTypeInfo (type).implemented);
        if (e == nullptr) continue;
        CAPTURE (engineTypeInfo (type).id);
        CHECK (std::strcmp (e->info().id, engineTypeInfo (type).id) == 0);
        const auto params = e->getParamDescriptors();
        REQUIRE (params.size() >= (size_t) ISynthEngine::numMacros);
        e->prepare (sampleRate, 512);
        e->setCommon (plainCommon());
        for (size_t i = 0; i < params.size(); ++i) e->setParam ((int) i, params[i].defaultValue);
        CHECK (peak (renderEngine (*e, 1024).l) == 0.0f);
        e->noteOn (60, 1.0f);
        auto on = renderEngine (*e, 4800);
        CHECK (rms (on.l) > 0.001f);
        CHECK (allFinite (on.l));
        e->noteOff (60);
        renderEngine (*e, 48000 * 2);
        CHECK (e->activeVoiceCount() == 0);
    }
}
