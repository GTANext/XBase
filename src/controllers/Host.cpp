#include <XBase/Host.h>

#include "CHud.h"
#include "plugin.h"

#include <windows.h>

#include <condition_variable>
#include <mutex>

namespace XBase::Host {
namespace {

struct HostState {
    Callbacks callbacks;
    unsigned int activeCallbacks = 0;
    bool installed = false;
    bool acceptingCallbacks = false;
    bool gameInitSubscribed = false;
    bool processSubscribed = false;
};

HostState s_state;
std::mutex s_stateMutex;
std::condition_variable s_callbacksIdle;
std::mutex s_eventMutex;
std::mutex s_lifecycleMutex;

void DispatchProcess();

class CallbackLease {
public:
    explicit CallbackLease(Callback callback) : callback_(callback) {}

    CallbackLease(const CallbackLease&) = delete;
    CallbackLease& operator=(const CallbackLease&) = delete;

    ~CallbackLease() {
        if (!callback_) return;
        {
            std::lock_guard<std::mutex> lock(s_stateMutex);
            --s_state.activeCallbacks;
        }
        s_callbacksIdle.notify_all();
    }

    explicit operator bool() const { return callback_ != nullptr; }
    void Invoke() const { callback_(); }

private:
    Callback callback_ = nullptr;
};

CallbackLease AcquireCallback(Callback Callbacks::* member) {
    std::lock_guard<std::mutex> lock(s_stateMutex);
    if (!s_state.acceptingCallbacks) return CallbackLease(nullptr);

    const Callback callback = s_state.callbacks.*member;
    if (!callback) return CallbackLease(nullptr);
    ++s_state.activeCallbacks;
    return CallbackLease(callback);
}

void Invoke(Callback Callbacks::* member) {
    CallbackLease lease = AcquireCallback(member);
    if (lease) lease.Invoke();
}

void SubscribeProcess() {
    std::lock_guard<std::mutex> eventLock(s_eventMutex);
    {
        std::lock_guard<std::mutex> stateLock(s_stateMutex);
        if (!s_state.acceptingCallbacks || s_state.processSubscribed) return;
        s_state.processSubscribed = true;
    }

#if defined(XBASE_BACKEND_III)
    plugin::Events::gameProcessEvent += DispatchProcess;
#else
    plugin::Events::processScriptsEvent += DispatchProcess;
#endif
}

void DispatchGameInit() {
    Invoke(&Callbacks::onGameInit);
    SubscribeProcess();
}

void DispatchProcess() {
    Invoke(&Callbacks::onProcess);
}

void SubscribeGameInit() {
    std::lock_guard<std::mutex> eventLock(s_eventMutex);
    {
        std::lock_guard<std::mutex> stateLock(s_stateMutex);
        if (!s_state.acceptingCallbacks || s_state.gameInitSubscribed) return;
        s_state.gameInitSubscribed = true;
    }
    plugin::Events::initGameEvent.after += DispatchGameInit;
}

void UnsubscribeGameInit() {
    std::lock_guard<std::mutex> eventLock(s_eventMutex);
    bool subscribed = false;
    {
        std::lock_guard<std::mutex> stateLock(s_stateMutex);
        subscribed = s_state.gameInitSubscribed;
        s_state.gameInitSubscribed = false;
    }
    if (subscribed) plugin::Events::initGameEvent.after -= DispatchGameInit;
}

void UnsubscribeProcess() {
    std::lock_guard<std::mutex> eventLock(s_eventMutex);
    bool subscribed = false;
    {
        std::lock_guard<std::mutex> stateLock(s_stateMutex);
        subscribed = s_state.processSubscribed;
        s_state.processSubscribed = false;
    }
    if (!subscribed) return;

#if defined(XBASE_BACKEND_III)
    plugin::Events::gameProcessEvent -= DispatchProcess;
#else
    plugin::Events::processScriptsEvent -= DispatchProcess;
#endif
}

} // namespace

bool Install(const Callbacks& callbacks) {
    if (!callbacks.onGameInit || !callbacks.onProcess) return false;
    std::lock_guard<std::mutex> lifecycleLock(s_lifecycleMutex);

    bool subscribe = false;
    {
        std::lock_guard<std::mutex> lock(s_stateMutex);
        s_state.callbacks = callbacks;
        s_state.acceptingCallbacks = true;
        if (!s_state.installed) {
            s_state.installed = true;
            subscribe = true;
        }
    }

    if (subscribe) SubscribeGameInit();
    return true;
}

void Shutdown() {
    std::lock_guard<std::mutex> lifecycleLock(s_lifecycleMutex);
    bool unsubscribeGameInit = false;
    {
        std::lock_guard<std::mutex> lock(s_stateMutex);
        s_state.acceptingCallbacks = false;
        unsubscribeGameInit = s_state.installed;
        s_state.installed = false;
    }

    if (unsubscribeGameInit) UnsubscribeGameInit();
    UnsubscribeProcess();

    std::unique_lock<std::mutex> lock(s_stateMutex);
    s_callbacksIdle.wait(lock, [] {
        return s_state.activeCallbacks == 0;
    });
    s_state.callbacks = {};
}

bool IsInstalled() {
    std::lock_guard<std::mutex> lock(s_stateMutex);
    return s_state.installed;
}

bool ShowMessage(const char* message) {
    if (!message || !message[0]) return false;
#if defined(XBASE_BACKEND_SA)
    CHud::SetHelpMessage(message, true, false, false);
#else
    wchar_t wide[512]{};
    if (MultiByteToWideChar(CP_UTF8, 0, message, -1, wide, 512) == 0) return false;
#if defined(XBASE_BACKEND_VC)
    CHud::SetHelpMessage(wide, true, false);
#else
    CHud::SetHelpMessage(wide, true);
#endif
#endif
    return true;
}

} // namespace XBase::Host