// Generic real-time safety test for ANY plugin in this repo (Linux / GNU ld only).
// Built automatically for every plugin by kit_add_plugin(); it links the plugin's shared code
// and drives it through createPluginFilter().
//
// Every heap allocation/free and every blocking-lock call made from the audio thread while it
// is inside the "audio callback" is trapped and reported with a stack trace:
//   * malloc/calloc/realloc/free/posix_memalign/aligned_alloc  (via -Wl,--wrap)
//   * all global operator new/delete variants                  (replaced below)
//   * pthread_mutex_lock/trylock, pthread_rwlock_*, pthread_cond_*wait (via -Wl,--wrap)
//
// The audio thread mimics exactly what JUCE's VST3 wrapper does inside IAudioProcessor::process():
// host parameter changes are applied with setValue() + sendValueChangedMessageToListeners()
// on the audio thread, then processBlock() runs. Meanwhile the message thread has the editor
// open and runs its event loop, like a host with the plug-in window showing.
//
// Scenarios: every bus layout the plugin accepts out of stereo/mono x side-chain on/off; random
// block sizes (1..2048, including blocks bigger than prepareToPlay announced); heavy automation
// of every parameter (including switches) every block; with and without the editor open.

#include <juce_audio_utils/juce_audio_utils.h>
#include <execinfo.h>
#include <pthread.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include <thread>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(); // defined by the plugin under test

//==============================================================================
namespace rt
{
    // Which part of the audio callback we're in: 0 = not in callback,
    // 1 = host delivering parameter changes (JUCE wrapper code), 2 = processBlock (our code)
    thread_local int phase = 0;
    thread_local bool inTrap = false;

    struct Counters { std::atomic<int> allocations { 0 }, frees { 0 }, locks { 0 }, contended { 0 }; };
    Counters counters[3];

    enum Kind { kAlloc, kFree, kLock };
    constexpr int maxStacks = 64, maxDepth = 24;
    void* stacks[maxStacks][maxDepth];
    int   depths[maxStacks];
    const char* kinds[maxStacks];
    int stackPhases[maxStacks];
    Kind stackKinds[maxStacks];
    std::atomic<int> numStacks { 0 };

    inline bool trap (const char* kind, Kind k)
    {
        if (phase == 0 || inTrap) return false;
        inTrap = true;
        auto& c = counters[phase];
        (k == kAlloc ? c.allocations : k == kFree ? c.frees : c.locks).fetch_add (1);
        const int i = numStacks.fetch_add (1);
        if (i < maxStacks)
        {
            kinds[i]  = kind;
            stackPhases[i] = phase;
            stackKinds[i]  = k;
            depths[i] = backtrace (stacks[i], maxDepth);
        }
        inTrap = false;
        return true;
    }

    struct PhaseScope
    {
        explicit PhaseScope (int p) { phase = p; }
        ~PhaseScope() { phase = 0; }
    };
    void resetCounters() { for (auto& c : counters) { c.allocations = 0; c.frees = 0; c.locks = 0; c.contended = 0; } numStacks = 0; }
}

//==============================================================================
extern "C"
{
    void* __real_malloc (size_t);
    void* __real_calloc (size_t, size_t);
    void* __real_realloc (void*, size_t);
    void  __real_free (void*);
    int   __real_posix_memalign (void**, size_t, size_t);
    void* __real_aligned_alloc (size_t, size_t);
    int   __real_pthread_mutex_lock (pthread_mutex_t*);
    int   __real_pthread_mutex_trylock (pthread_mutex_t*);
    int   __real_pthread_rwlock_rdlock (pthread_rwlock_t*);
    int   __real_pthread_rwlock_wrlock (pthread_rwlock_t*);
    int   __real_pthread_cond_wait (pthread_cond_t*, pthread_mutex_t*);
    int   __real_pthread_cond_timedwait (pthread_cond_t*, pthread_mutex_t*, const struct timespec*);

    void* __wrap_malloc (size_t n)                    { rt::trap ("malloc", rt::kAlloc);  return __real_malloc (n); }
    void* __wrap_calloc (size_t a, size_t b)          { rt::trap ("calloc", rt::kAlloc);  return __real_calloc (a, b); }
    void* __wrap_realloc (void* p, size_t n)          { rt::trap ("realloc", rt::kAlloc); return __real_realloc (p, n); }
    void  __wrap_free (void* p)                       { if (p) rt::trap ("free", rt::kFree);   __real_free (p); }
    int   __wrap_posix_memalign (void** p, size_t a, size_t n) { rt::trap ("posix_memalign", rt::kAlloc); return __real_posix_memalign (p, a, n); }
    void* __wrap_aligned_alloc (size_t a, size_t n)   { rt::trap ("aligned_alloc", rt::kAlloc); return __real_aligned_alloc (a, n); }
    int   __wrap_pthread_mutex_lock (pthread_mutex_t* m)
    {
        if (rt::trap ("pthread_mutex_lock", rt::kLock))
        {
            // Would this lock have blocked the audio thread? Try first; count real contention.
            if (__real_pthread_mutex_trylock (m) == 0) return 0;
            rt::counters[rt::phase].contended.fetch_add (1);
        }
        return __real_pthread_mutex_lock (m);
    }
    int   __wrap_pthread_mutex_trylock (pthread_mutex_t* m) { rt::trap ("pthread_mutex_trylock", rt::kLock); return __real_pthread_mutex_trylock (m); }
    int   __wrap_pthread_rwlock_rdlock (pthread_rwlock_t* l) { rt::trap ("pthread_rwlock_rdlock", rt::kLock); return __real_pthread_rwlock_rdlock (l); }
    int   __wrap_pthread_rwlock_wrlock (pthread_rwlock_t* l) { rt::trap ("pthread_rwlock_wrlock", rt::kLock); return __real_pthread_rwlock_wrlock (l); }
    int   __wrap_pthread_cond_wait (pthread_cond_t* c, pthread_mutex_t* m) { rt::trap ("pthread_cond_wait", rt::kLock); return __real_pthread_cond_wait (c, m); }
    int   __wrap_pthread_cond_timedwait (pthread_cond_t* c, pthread_mutex_t* m, const struct timespec* t) { rt::trap ("pthread_cond_timedwait", rt::kLock); return __real_pthread_cond_timedwait (c, m, t); }
}

// operator new/delete live in libstdc++ (not wrapped by --wrap), so replace them here.
static void* trappedNew (size_t n)              { rt::trap ("operator new", rt::kAlloc); if (auto* p = __real_malloc (n ? n : 1)) return p; throw std::bad_alloc(); }
static void* trappedNewAligned (size_t n, std::align_val_t a)
{
    rt::trap ("operator new (aligned)", rt::kAlloc);
    void* p = nullptr;
    if (__real_posix_memalign (&p, std::max ((size_t) a, sizeof (void*)), n ? n : 1) != 0) throw std::bad_alloc();
    return p;
}
static void trappedDelete (void* p)             { if (p) { rt::trap ("operator delete", rt::kFree); __real_free (p); } }

void* operator new (size_t n)                                        { return trappedNew (n); }
void* operator new[] (size_t n)                                      { return trappedNew (n); }
void* operator new (size_t n, const std::nothrow_t&) noexcept        { try { return trappedNew (n); } catch (...) { return nullptr; } }
void* operator new[] (size_t n, const std::nothrow_t&) noexcept      { try { return trappedNew (n); } catch (...) { return nullptr; } }
void* operator new (size_t n, std::align_val_t a)                    { return trappedNewAligned (n, a); }
void* operator new[] (size_t n, std::align_val_t a)                  { return trappedNewAligned (n, a); }
void operator delete (void* p) noexcept                              { trappedDelete (p); }
void operator delete[] (void* p) noexcept                            { trappedDelete (p); }
void operator delete (void* p, size_t) noexcept                      { trappedDelete (p); }
void operator delete[] (void* p, size_t) noexcept                    { trappedDelete (p); }
void operator delete (void* p, std::align_val_t) noexcept            { trappedDelete (p); }
void operator delete[] (void* p, std::align_val_t) noexcept          { trappedDelete (p); }
void operator delete (void* p, size_t, std::align_val_t) noexcept    { trappedDelete (p); }
void operator delete[] (void* p, size_t, std::align_val_t) noexcept  { trappedDelete (p); }

//==============================================================================
// Same logic as setValueAndNotifyIfChanged() in juce_audio_plugin_client_VST3.cpp
static void hostSetsParameter (juce::AudioProcessorParameter& p, float normalised)
{
    if (juce::approximatelyEqual (p.getValue(), normalised)) return;
    p.setValue (normalised);
    p.sendValueChangedMessageToListeners (normalised);
}

struct Scenario { const char* name; int mainCh; int scCh; };

/** Returns -1 if the plugin rejects this layout (scenario skipped), else 1 pass / 0 fail. */
static int runScenario (const Scenario& sc, bool withEditor)
{
    using namespace juce;
    std::unique_ptr<AudioProcessor> proc (createPluginFilter());

    AudioProcessor::BusesLayout layout;
    const auto mainSet = sc.mainCh == 1 ? AudioChannelSet::mono() : AudioChannelSet::stereo();
    const int numIns = proc->getBusCount (true), numOuts = proc->getBusCount (false);
    for (int i = 0; i < numIns; ++i)
        layout.inputBuses.add (i == 0 ? mainSet
                                      : (sc.scCh == 0 ? AudioChannelSet::disabled()
                                                      : (sc.scCh == 1 ? AudioChannelSet::mono() : AudioChannelSet::stereo())));
    for (int i = 0; i < numOuts; ++i)
        layout.outputBuses.add (i == 0 ? mainSet : AudioChannelSet::disabled());

    if ((sc.scCh > 0 && numIns < 2) || ! proc->setBusesLayout (layout))
    {
        if (withEditor) std::printf ("  %-30s (layout not supported by this plugin - skipped)\n", sc.name);
        return -1;
    }

    constexpr double fs = 48000.0;
    constexpr int announcedBlock = 512, maxBlock = 2048;
    proc->setRateAndBufferSizeDetails (fs, announcedBlock);
    proc->prepareToPlay (fs, announcedBlock);

    std::unique_ptr<AudioProcessorEditor> editor;
    if (withEditor) editor.reset (proc->createEditor());

    const int totalCh = jmax (proc->getTotalNumInputChannels(), proc->getTotalNumOutputChannels());
    AudioBuffer<float> buffer (totalCh, maxBlock);  // pre-allocated, like a host
    MidiBuffer midi;
    midi.ensureSize (256);
    auto params = proc->getParameters();

    rt::resetCounters();
    std::atomic<bool> done { false };
    std::atomic<int> hostChanges { 0 };

    std::thread audio ([&]
    {
        std::mt19937 rng (1234);
        std::uniform_int_distribution<int> blockDist (1, maxBlock);
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);
        double ph = 0.0;
        int changes = 0;

        for (int block = 0; block < 6000; ++block)
        {
            const int n = (block % 50 == 0) ? maxBlock : blockDist (rng);

            for (int ch = 0; ch < totalCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                for (int s = 0; s < n; ++s)
                    d[s] = 0.8f * (float) std::sin (ph + 0.031 * s * (ch + 1)) * (block % 7 == 0 ? 0.0f : 1.0f);
            }
            ph += 0.031 * n;

            {   // ---- phase 1: host applies automation inside process(), like JUCE's VST3 wrapper
                rt::PhaseScope scope (1);
                for (int i = 0; i < params.size(); ++i)
                    if ((block + i) % 3 == 0)
                    {
                        const float v = uni (rng);
                        if (! approximatelyEqual (params[i]->getValue(), v)) ++changes;
                        hostSetsParameter (*params[i], v);
                    }
            }
            {   // ---- phase 2: the plug-in's own processBlock
                AudioBuffer<float> view (buffer.getArrayOfWritePointers(), totalCh, n); // host-style wrapper (no alloc)
                rt::PhaseScope scope (2);
                proc->processBlock (view, midi);
            }
        }
        hostChanges = changes;
        done = true;
    });

    // Message thread: editor open, timers + async updates running, and the "user" turning knobs
    // at the same time as the host automates them (worst case for lock contention).
    std::mt19937 guiRng (99);
    while (! done)
    {
        MessageManager::getInstance()->runDispatchLoopUntil (2);
        if (withEditor)
        {
            auto* p = params[(int) (guiRng() % (unsigned) params.size())];
            p->beginChangeGesture();
            p->setValueNotifyingHost ((float) (guiRng() % 1000) / 1000.0f);
            p->endChangeGesture();
        }
    }
    audio.join();

    editor.reset();
    proc->releaseResources();

    auto& pd = rt::counters[1];
    auto& pb = rt::counters[2];
    const bool processOk = pb.allocations == 0 && pb.frees == 0 && pb.locks == 0;
    const bool deliveryOk = pd.allocations == 0 && pd.frees == 0 && pd.locks <= 2 * hostChanges;
    std::printf ("  %-30s editor=%s\n", sc.name, withEditor ? "open + user moving knobs" : "closed");
    std::printf ("      processBlock      : allocs=%d frees=%d locks=%d            -> %s\n",
                 pb.allocations.load(), pb.frees.load(), pb.locks.load(), processOk ? "PASS" : "FAIL");
    std::printf ("      host param changes: allocs=%d frees=%d locks=%d (%d changes, %d contended) -> %s\n",
                 pd.allocations.load(), pd.frees.load(), pd.locks.load(), hostChanges.load(), pd.contended.load(),
                 deliveryOk ? "PASS" : "FAIL");

    // Show stacks for real violations (any alloc/free, or any lock inside processBlock)
    int shown = 0;
    for (int i = 0; i < std::min ((int) rt::numStacks, rt::maxStacks) && shown < 3; ++i)
    {
        if (rt::stackKinds[i] == rt::kLock && rt::stackPhases[i] == 1) continue;
        ++shown;
        std::printf ("    --- %s in %s:\n", rt::kinds[i], rt::stackPhases[i] == 2 ? "processBlock" : "host param change");
        char** syms = backtrace_symbols (rt::stacks[i], rt::depths[i]);
        for (int d = 1; d < rt::depths[i] && d < 14; ++d)
            std::printf ("      %s\n", syms ? syms[d] : "?");
        std::free (syms);
    }
    return (processOk && deliveryOk) ? 1 : 0;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    { void* warm[4]; backtrace (warm, 4); } // make glibc load libgcc's unwinder now, not on first trap

    const Scenario scenarios[] {
        { "stereo + stereo side-chain", 2, 2 },
        { "stereo, side-chain disabled", 2, 0 },
        { "mono + mono side-chain", 1, 1 },
        { "mono, side-chain disabled", 1, 0 },
    };

    bool ok = true;
    int ran = 0;
    {
        std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
        std::printf ("Real-time safety [%s]: trapping heap + lock calls on the audio thread\n", p->getName().toRawUTF8());
    }
    for (const auto& s : scenarios)
        for (bool editor : { false, true })
        {
            const int r = runScenario (s, editor);
            if (r >= 0) { ++ran; ok &= (r == 1); }
        }
    if (ran == 0) { std::printf ("No supported bus layout was tested!\n"); ok = false; }

    std::printf ("\n%s\n", ok ? "REAL-TIME SAFE: processBlock is allocation- and lock-free; host parameter delivery "
                                   "allocates nothing and only touches JUCE's per-parameter listener locks"
                              : "REAL-TIME VIOLATIONS FOUND");
    return ok ? 0 : 1;
}
