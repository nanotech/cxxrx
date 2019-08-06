#pragma once

#include "rx.h"

#include <memory>
#include <utility>

namespace windberry {
namespace rx {

template <typename Clock, typename Observable>
auto throttle_progress(Clock get_now, Observable o) {
    using Time = typename function_traits<Clock>::result_type;
    struct last_state {
        float progress = 0;
        Time update_time = 0;
    };
    std::shared_ptr<last_state> last = std::make_shared<last_state>();
    return o.bind([last = std::move(last), get_now](float p){
        if (p > last->progress + 0.002f) { // FIXME default frequency classes? (UI progress, frame rate, ...)
            Time now = get_now();
            if (now > last->update_time + 0.01f) {
                last->progress = p;
                last->update_time = now;
                return pure_observable(p).any();
            }
        }
        return empty_observable<float>().any();
    });
}

}
}
