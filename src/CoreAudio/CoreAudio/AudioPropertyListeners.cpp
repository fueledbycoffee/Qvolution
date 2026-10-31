/* Own-source listener registry. No callbacks run while its mutex is held. */
#include "AudioPropertyListeners.h"
#include <Block.h>
#include <CarbonCore/MacErrors.h>
#include <atomic>
#include <new>

static bool sameAddress(const AudioObjectPropertyAddress& a, const AudioObjectPropertyAddress& b) {
    return a.mSelector == b.mSelector && a.mScope == b.mScope && a.mElement == b.mElement;
}
// Global concurrent queues reject queue-specific storage in libdispatch.
// Public priority/QoS aliases identify all public global queues without labels
// or private queue layout; synchronous work on these queues cannot self-deadlock.
static bool globalQueue(dispatch_queue_t queue) {
    const intptr_t priorities[] = { DISPATCH_QUEUE_PRIORITY_HIGH, DISPATCH_QUEUE_PRIORITY_DEFAULT,
        DISPATCH_QUEUE_PRIORITY_LOW, DISPATCH_QUEUE_PRIORITY_BACKGROUND,
        QOS_CLASS_USER_INTERACTIVE, QOS_CLASS_USER_INITIATED, QOS_CLASS_DEFAULT,
        QOS_CLASS_UTILITY, QOS_CLASS_BACKGROUND };
    for (auto priority : priorities)
        if (queue == dispatch_get_global_queue(priority, 0)) return true;
    return false;
}
struct AudioPropertyListeners::Registration {
    AudioObjectPropertyAddress address{};
    AudioObjectPropertyListenerProc proc = nullptr;
    void* client = nullptr;
    const void* originalBlock = nullptr;
    AudioObjectPropertyListenerBlock block = nullptr;
    dispatch_queue_t queue = nullptr;
    bool specific = false;
    std::atomic<bool> active{true};
    ~Registration() {
        if (specific) dispatch_queue_set_specific(queue, this, nullptr, nullptr);
        if (block) Block_release(block);
        if (queue) dispatch_release(queue);
    }
    bool matches(const Registration& other) const {
        return sameAddress(address, other.address) && proc == other.proc && client == other.client &&
               originalBlock == other.originalBlock && queue == other.queue;
    }
};
struct AudioPropertyListeners::Prepared {
    std::shared_ptr<Registration> record;
    AudioObjectID object;
    AudioObjectPropertyAddress address;
    bool io;
    dispatch_block_t work = nullptr;
    ~Prepared() { if (work) Block_release(work); }
};
AudioPropertyListeners::~AudioPropertyListeners() {
    std::vector<std::shared_ptr<Registration>> detached;
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        for (const auto& record : m_records) record->active.store(false);
        detached.swap(m_records);
    }
}
OSStatus AudioPropertyListeners::insert(std::shared_ptr<Registration> record) {
    try {
        std::lock_guard<std::mutex> guard(m_mutex);
        for (const auto& existing : m_records)
            if (existing->matches(*record)) return kAudioHardwareNoError;
        m_records.push_back(record);
        ++m_generation;
        return kAudioHardwareNoError;
    } catch (const std::bad_alloc&) { return kAudioHardwareUnspecifiedError; }
}
OSStatus AudioPropertyListeners::add(const AudioObjectPropertyAddress& address,
        AudioObjectPropertyListenerProc proc, void* client) {
    if (!proc) return paramErr;
    try {
        auto record = std::make_shared<Registration>();
        record->address = address; record->proc = proc; record->client = client;
        return insert(record);
    } catch (const std::bad_alloc&) { return kAudioHardwareUnspecifiedError; }
}
OSStatus AudioPropertyListeners::add(const AudioObjectPropertyAddress& address,
        dispatch_queue_t queue, AudioObjectPropertyListenerBlock block) {
    if (!block) return paramErr;
    try {
        auto record = std::make_shared<Registration>();
        record->address = address; record->originalBlock = (const void*)block;
        record->block = Block_copy(block);
        if (!record->block) return kAudioHardwareUnspecifiedError;
        if (queue) {
            dispatch_retain(queue); record->queue = queue;
            if (!globalQueue(queue)) {
                dispatch_queue_set_specific(queue, record.get(), record.get(), nullptr);
                record->specific = true;
            }
        }
        return insert(record);
    } catch (const std::bad_alloc&) { return kAudioHardwareUnspecifiedError; }
}
OSStatus AudioPropertyListeners::erase(const AudioObjectPropertyAddress& address,
        AudioObjectPropertyListenerProc proc, void* client, dispatch_queue_t queue, const void* block) {
    std::shared_ptr<Registration> removed;
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        for (auto it = m_records.begin(); it != m_records.end(); ++it) {
            const auto& record = *it;
            if (sameAddress(record->address, address) && record->proc == proc && record->client == client &&
                    record->queue == queue && record->originalBlock == block) {
                record->active.store(false); removed = record; m_records.erase(it); ++m_generation; break;
            }
        }
    }
    return kAudioHardwareNoError;
}
OSStatus AudioPropertyListeners::remove(const AudioObjectPropertyAddress& address,
        AudioObjectPropertyListenerProc proc, void* client) {
    return proc ? erase(address, proc, client, nullptr, nullptr) : paramErr;
}
OSStatus AudioPropertyListeners::remove(const AudioObjectPropertyAddress& address,
        dispatch_queue_t queue, AudioObjectPropertyListenerBlock block) {
    return block ? erase(address, nullptr, nullptr, queue, (const void*)block) : paramErr;
}
OSStatus AudioPropertyListeners::prepare(const AudioObjectPropertyAddress& address, Change& output, Context context, AddressMatch match) {
    try {
        Change result;
        std::vector<std::shared_ptr<Registration>> snapshot;
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            result.m_source = this;
            result.m_generation = m_generation;
            for (const auto& record : m_records)
                if ((match == AddressMatch::SelectorAliases ? record->address.mSelector == address.mSelector :
                        sameAddress(record->address, address)) && record->active.load()) snapshot.push_back(record);
        }
        // The public API cannot inspect a supplied queue's parent targets. If IO
        // delivery originates on an unknown queue, synchronizing to its descendant
        // could deadlock. Reject the whole change before publication in that case.
        if (context == Context::IO && !globalQueue(dispatch_get_current_queue())) {
            for (const auto& record : snapshot)
                if (record->queue && !globalQueue(record->queue) &&
                        dispatch_get_specific(record.get()) != record.get())
                    return kAudioHardwareUnsupportedOperationError;
        }
        // Allocation and block capture destruction happen outside the registry lock.
        // Capture values explicitly: neither the caller's address nor a loop reference
        // may outlive its scope when queued work eventually executes.
        for (const auto& record : snapshot) {
            auto ready = std::make_shared<Prepared>();
            ready->record = record; ready->object = m_object; ready->address = record->address;
            ready->io = context == Context::IO;
            if (record->block && record->queue) {
                auto owner = record;
                AudioObjectPropertyAddress payload = record->address;
                ready->work = Block_copy(^{
                    if (owner->active.load()) owner->block(1, &payload);
                });
                if (!ready->work) throw std::bad_alloc();
            }
            result.m_ready.push_back(ready);
        }
        output = std::move(result);
        return kAudioHardwareNoError;
    } catch (const std::bad_alloc&) { return kAudioHardwareUnspecifiedError; }
}
void AudioPropertyListeners::Change::deliver() const {
    for (const auto& ready : m_ready) {
        const auto& record = ready->record;
        if (!record->active.load()) continue;
        if (record->proc) record->proc(ready->object, 1, &ready->address, record->client);
        else if (!record->queue) record->block(1, &ready->address);
        else if (!ready->io) dispatch_async(record->queue, ready->work);
        else if (dispatch_get_specific(record.get()) == record.get()) ready->work();
        else dispatch_sync(record->queue, ready->work);
    }
}
