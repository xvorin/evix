#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace xvorin::evix {

namespace detail {

template <typename Callback>
struct SlotImpl;

template <typename Callback>
struct SignalImpl {
    using SlotList = std::vector<std::weak_ptr<SlotImpl<Callback>>>;

    SignalImpl()
        : slots_(std::make_shared<SlotList>())
    {
    }

    void copy_on_write()
    {
        if (!slots_.unique()) {
            slots_ = std::make_shared<SlotList>(*slots_);
        }
        assert(slots_.unique());
    }

    void cleanup()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        copy_on_write();
        SlotList& list(*slots_);
        auto iter = list.begin();

        while (iter != list.end()) {
            if (iter->expired()) {
                iter = list.erase(iter);
            } else {
                ++iter;
            }
        }
    }

    mutable std::mutex mutex_;
    std::shared_ptr<SlotList> slots_;
};

template <typename Callback>
struct SlotImpl {
    using Owner = SignalImpl<Callback>;

    SlotImpl(const std::shared_ptr<Owner>& owner, Callback&& cb)
        : owner_(owner)
        , cb_(cb)
        , tie_()
        , tied_(false)
    {
    }

    SlotImpl(const std::shared_ptr<Owner>& owner, Callback&& cb, const std::shared_ptr<void>& tie)
        : owner_(owner)
        , cb_(cb)
        , tie_(tie)
        , tied_(true)
    {
    }

    ~SlotImpl()
    {
        std::shared_ptr<Owner> owner(owner_.lock());
        if (owner) {
            owner->cleanup();
        }
    }

    std::weak_ptr<Owner> owner_;
    Callback cb_;
    std::weak_ptr<void> tie_;
    bool tied_;
};

} // namespace xvorin::evix::detail
using Slot = std::shared_ptr<void>;

template <typename Signature>
class Signal;

template <typename RET, typename... ARGS>
class Signal<RET(ARGS...)> {
    using Callback = std::function<void(ARGS...)>;
    using SignalImpl = detail::SignalImpl<Callback>;
    using SlotImpl = detail::SlotImpl<Callback>;

public:
    Signal()
        : impl_(std::make_shared<SignalImpl>())
    {
    }

    Signal(const Signal&) = delete;
    void operator=(const Signal&) = delete;

    ~Signal() = default;

    Slot connect(Callback&& func)
    {
        auto slotImpl = std::make_shared<SlotImpl>(impl_, std::forward<Callback>(func));
        add(slotImpl);
        return slotImpl;
    }

    Slot connect(Callback&& func, const std::shared_ptr<void>& tie)
    {
        auto slotImpl = std::make_shared<SlotImpl>(impl_, std::forward<Callback>(func), tie);
        add(slotImpl);
        return slotImpl;
    }

    size_t count()
    {
        size_t retval = 0;
        SignalImpl& impl(*impl_);
        {
            std::lock_guard<std::mutex> lock(impl.mutex_);
            retval = impl.slots_->size();
        }
        return retval;
    }

    void call(ARGS&&... args)
    {
        SignalImpl& impl(*impl_);
        std::shared_ptr<typename SignalImpl::SlotList> slots;
        {
            std::lock_guard<std::mutex> lock(impl.mutex_);
            slots = impl.slots_;
        }
        auto& s(*slots);
        for (auto iter = s.begin(); iter != s.end(); ++iter) {
            std::shared_ptr<SlotImpl> slotImpl = iter->lock();
            if (slotImpl) {
                std::shared_ptr<void> guard;
                if (slotImpl->tied_) {
                    guard = slotImpl->tie_.lock();
                    if (guard) {
                        slotImpl->cb_(args...);
                    }
                } else {
                    slotImpl->cb_(args...);
                }
            }
        }
    }

private:
    void add(const std::shared_ptr<SlotImpl>& slot)
    {
        SignalImpl& impl(*impl_);
        {
            std::lock_guard<std::mutex> lock(impl.mutex_);
            impl.copy_on_write();
            impl.slots_->push_back(slot);
        }
    }

    const std::shared_ptr<SignalImpl> impl_;
};

} // namespace xvorin::evix
