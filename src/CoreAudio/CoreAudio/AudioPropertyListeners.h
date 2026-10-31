/* Genuine property listener ownership/dispatch for the own CoreAudio model. */
#ifndef AUDIO_PROPERTY_LISTENERS_H
#define AUDIO_PROPERTY_LISTENERS_H
#include <CoreAudio/AudioHardware.h>
#include <memory>
#include <cstdint>
#include <mutex>
#include <vector>

class AudioPropertyListeners {
    struct Registration;
    struct Prepared;
public:
    enum class Context { Normal, IO };
    enum class AddressMatch { Exact, SelectorAliases };
    class Change {
        friend class AudioPropertyListeners;
        std::vector<std::shared_ptr<Prepared>> m_ready;
        const AudioPropertyListeners* m_source = nullptr;
        std::uint64_t m_generation = 0;
    public:
        void deliver() const;
    };
    explicit AudioPropertyListeners(AudioObjectID object) : m_object(object) {}
    ~AudioPropertyListeners();
    OSStatus add(const AudioObjectPropertyAddress&, AudioObjectPropertyListenerProc, void*);
    OSStatus remove(const AudioObjectPropertyAddress&, AudioObjectPropertyListenerProc, void*);
    OSStatus add(const AudioObjectPropertyAddress&, dispatch_queue_t, AudioObjectPropertyListenerBlock);
    OSStatus remove(const AudioObjectPropertyAddress&, dispatch_queue_t, AudioObjectPropertyListenerBlock);
    // Prepare all owned payloads before committing state or delivering callbacks.
    OSStatus prepare(const AudioObjectPropertyAddress&, Change&, Context = Context::Normal,
                     AddressMatch = AddressMatch::Exact);
    // commitState may only publish own state, with no allocation or client work.
    template <class CommitState>
    bool commitIfCurrent(const Change& change, CommitState commitState) {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (change.m_source != this || change.m_generation != m_generation) return false;
        commitState();
        return true;
    }
private:
    OSStatus insert(std::shared_ptr<Registration>);
    OSStatus erase(const AudioObjectPropertyAddress&, AudioObjectPropertyListenerProc, void*,
                   dispatch_queue_t, const void*);
    AudioObjectID m_object;
    std::mutex m_mutex;
    std::uint64_t m_generation = 0;
    std::vector<std::shared_ptr<Registration>> m_records;
};
#endif
