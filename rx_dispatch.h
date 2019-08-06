#include "rx.h"

#include <dispatch/dispatch.h>

#ifdef __OBJC__
@class NSOperationQueue;
#endif

namespace windberry {
namespace rx {

template <>
struct schedule_on<dispatch_queue_t> {
    inline auto operator()(dispatch_queue_t q) {
        assert(q);
        using namespace std::placeholders;
        return std::bind(dispatch_async, q, _1);
    }
};

template <>
struct schedule_on<dispatch_queue_main_t> {
    inline auto operator()(dispatch_queue_main_t q) {
        return schedule_on<dispatch_queue_t>()(q);
    }
};

#ifdef __OBJC__
template <>
struct schedule_on<NSOperationQueue *> {
    inline auto operator()(NSOperationQueue *q) {
        assert(q);
        return [q](auto f){
            [q addOperationWithBlock:f];
        };
    }
};
#endif

template <typename T, typename E, typename Observer>
struct throttle_observer : forwarding_observer<T, E, Observer> {
    std::unique_ptr<dispatch_source_t> timerp = std::make_unique<dispatch_source_t>();
    dispatch_queue_t q;
    dispatch_time_t interval, leeway;
    throttle_observer(Observer s_,
                      dispatch_queue_t q_,
                      dispatch_time_t i_,
                      dispatch_time_t l_)
        : forwarding_observer<T, E, Observer>(s_), q(q_), interval(i_), leeway(l_) {}

    void send_next(T x) const {
        if (*timerp) {
            return;
        }
        *timerp = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, q);
        dispatch_source_set_timer(*timerp, dispatch_time(DISPATCH_TIME_NOW, interval),
                                  DISPATCH_TIME_FOREVER, leeway);
        dispatch_source_set_event_handler(*timerp, ^{
          this->s.send_next(x);
          dispatch_source_cancel(*timerp);
#if !__has_feature(objc_arc)
          dispatch_release(*timerp);
#endif
          *timerp = (dispatch_source_t)nullptr;
        });
        dispatch_resume(*timerp);
    }
};

template <typename Observable>
auto throttle(const Observable &o,
              dispatch_queue_t q,
              dispatch_time_t interval,
              dispatch_time_t leeway) {
    using T = typename Observable::value_type;
    using E = typename Observable::error_type;
    return make_observable<T>([=](auto s){
        o.subscribe(throttle_observer<T, E, decltype(s)>{s, q, interval, leeway});
    });
}

}
}
