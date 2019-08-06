#pragma once

#include <functional>
#include <memory>
#include <utility>

namespace windberry {
namespace rx {

template <typename A, typename B>
using disable_if_same_or_derived = typename std::
    enable_if<!std::is_base_of<A, typename std::remove_reference<B>::type>::value>::type;

struct empty_callable {
    inline void operator()() const {}
};

template <bool>
struct callable_or_empty_;

template <>
struct callable_or_empty_<true> {
    template <size_t I, typename T>
    inline constexpr auto operator()(const T &t) const {
        return std::get<I>(t);
    }
};

template <>
struct callable_or_empty_<false> {
    template <size_t I, typename T>
    inline constexpr auto operator()(const T &) const {
        return empty_callable{};
    }
};

template <size_t I, typename... Fs>
inline constexpr auto callable_or_empty(const std::tuple<Fs...> &t) {
    using Tuple = std::tuple<Fs...>;
    constexpr bool ok = I < std::tuple_size<Tuple>();
    return callable_or_empty_<ok>{}.template operator()<I, Tuple>(t);
}

template <typename T, typename... Fs>
struct tuple_observer {
    using value_type = T;
    std::tuple<Fs...> fs;

    tuple_observer(std::tuple<Fs...> &&fs_) : fs(std::move(fs_)) {}

    inline void send_next(T x) const { std::get<0>(fs)(x); }
    inline void send_error() const { callable_or_empty<1, Fs...>(fs)(); }
    inline void send_completed() const { callable_or_empty<2, Fs...>(fs)(); }
};

template <typename T>
struct any_observer {
    struct base;

    using value_type = T;

    std::shared_ptr<base> p;
    template <typename O,
              typename = disable_if_same_or_derived<std::decay_t<O>, any_observer<T>>>
    any_observer(O &&o)
        : p(new model<std::decay_t<O>>(std::forward<O>(o))) {}

    inline void send_next(T x) const { p->send_next(x); }
    inline void send_error() const { p->send_error(); }
    inline void send_completed() const { p->send_completed(); }

    struct base {
        using value_type = T;
        virtual ~base() {}
        virtual void send_next(T) const = 0;
        virtual void send_error() const = 0;
        virtual void send_completed() const = 0;
    };

    template <typename O>
    struct model : base {
        O o;
        template <typename O_>
        model(O_ &&o_) : o(std::forward<O_>(o_)) {}
        void send_next(T x) const override { o.send_next(x); }
        void send_error() const override { o.send_error(); }
        void send_completed() const override { o.send_completed(); }
    };
};

template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())> {};

template <typename Class, typename R, typename... Args>
struct function_traits<R (Class::*)(Args...) const> {
    static const size_t nargs = sizeof...(Args);

    using result_type = R;

    template <size_t i>
    struct arg {
        using type = typename std::tuple_element<i, std::tuple<Args...>>::type;
    };
};

template <typename F>
using result_type = typename function_traits<F>::result_type;

template <typename F>
using first_argument_type = typename function_traits<F>::template arg<0>::type;

template <typename F>
inline auto make_observer(F &&f) -> tuple_observer<first_argument_type<F>, F> {
    return std::make_tuple(std::forward<F>(f));
}
template <typename F, typename G>
inline auto make_observer(F &&f, G &&g) -> tuple_observer<first_argument_type<F>, F, G> {
    return std::make_tuple(std::forward<F>(f), std::forward<G>(g));
}
template <typename F, typename G, typename H>
inline auto make_observer(F &&f,
                          G &&g,
                          H &&h) -> tuple_observer<first_argument_type<F>, F, G, H> {
    return std::make_tuple(std::forward<F>(f), std::forward<G>(g), std::forward<H>(h));
}

template <typename T, typename F>
class observable;

template <typename T, typename F>
inline auto make_observable(F &&f) -> observable<T, F> {
    return {std::forward<F>(f)};
}

template <typename T>
inline auto pure_observable(T x) {
    return make_observable<T>([x](auto s) {
        s.send_next(x);
        s.send_completed();
    });
}

template <typename T>
inline auto empty_observable() {
    return make_observable<T>([](auto s) { s.send_completed(); });
}

template <typename T>
static auto error_observable() {
    return rx::make_observable<T>([](auto s) { s.send_error(); });
}

template <typename T>
using any_observable = observable<T, std::function<void(any_observer<T>)>>;

// Specialize to implement
template <typename> struct schedule_on;

template <typename T, typename DoSub>
struct observable {
    using value_type = T;

    DoSub do_subscribe;

    observable() : do_subscribe([](auto){}) {}

    template <typename DoSubscribe,
              typename = disable_if_same_or_derived<observable, DoSubscribe>>
    observable(DoSubscribe &&o)
        : do_subscribe(std::forward<DoSubscribe>(o)) {}

    template <typename Observer, typename U = typename std::decay_t<Observer>::value_type>
    void subscribe(Observer &&o) const {
        //printf("observer size: %zu [%s]\n", sizeof(typename std::decay_t<Observer>), typeid(o).name());
        do_subscribe(std::forward<Observer>(o));
    }

    template <typename... Args>
    void subscribe(Args &&... args) const {
        subscribe(make_observer(std::forward<Args>(args)...));
    }

    template <typename F>
    auto map(F &&f) {
        return bind([f](T x){
            return pure_observable(f(x));
        });
    }

    template <typename Observer>
    struct forwarding_observer {
        using value_type = T;
        Observer s;
        forwarding_observer(Observer s_) : s(s_) {}

        inline void send_next(T x) const { s.send_next(x); }
        inline void send_error() const { s.send_error(); }
        inline void send_completed() const { s.send_completed(); }
    };

    template <typename Observer, typename F>
    struct bind_observer : forwarding_observer<Observer> {
        F f;
        bind_observer(Observer s_, F f_) : forwarding_observer<Observer>(s_), f(f_) {}

        inline void send_next(T x) const { f(x).subscribe(this->s); }
    };

    template <typename F>
    auto bind(F &&f) {
        using U = typename result_type<F>::value_type;
        return make_observable<U>([f, me = *this](auto s){
            me.subscribe(bind_observer<decltype(s), F>{s,f});
        });
    }

    template <typename Observer, typename Observable>
    struct catch_to_observer : forwarding_observer<Observer> {
        Observable o;
        catch_to_observer(Observer s_, Observable o_)
            : forwarding_observer<Observer>(s_), o(o_) {}

        inline void send_error() const { o.subscribe(this->s); }
    };

    template <typename Observable>
    auto catch_to(Observable o) {
        using U = typename Observable::value_type;
        static_assert(std::is_same<T, U>(), "");
        return make_observable<U>([o, me = *this](auto s) {
            me.subscribe(catch_to_observer<decltype(s), Observable>{s, o});
        });
    }

    template <typename F>
    auto deliver_with(F f) {
        struct deliver_observer {
            using value_type = T;
            F f;
            any_observer<T> s;
            deliver_observer(F fn_, any_observer<T> s_) : f(fn_), s(s_) {}

            inline void send_next(T x)   const { f([s = s, x]{ s.send_next(x); }); }
            inline void send_error()     const { f([s = s]{ s.send_error(); }); }
            inline void send_completed() const { f([s = s]{ s.send_completed(); }); }
        };
        return make_observable<T>([f, me = *this](auto s){
            me.subscribe(deliver_observer{f, s});
        });
    }

    template <typename F>
    auto subscribe_with(F f) {
        auto g = do_subscribe;
        return make_observable<T>([f, g](auto s){
            f([s, g]{
                g(s);
            });
        });
    }

    template <typename Q>
    auto deliver_on(Q q) {
        return deliver_with(schedule_on<Q>{}(q));
    }

    template <typename Q>
    auto subscribe_on(Q q) {
        return subscribe_with(schedule_on<Q>{}(q));
    }

    auto any() const& -> any_observable<T> {
        return any_observable<T>(do_subscribe);
    }

    auto any() && -> any_observable<T> {
        return any_observable<T>(std::move(do_subscribe));
    }
};

}
}

#include <dispatch/dispatch.h>

#ifdef __OBJC__
@class NSOperationQueue;
#endif

namespace windberry {
namespace rx {

template <>
struct schedule_on<dispatch_queue_t> {
    inline auto operator()(dispatch_queue_t q) {
        using namespace std::placeholders;
        return std::bind(dispatch_async, q, _1);
    }
};

#ifdef __OBJC__
template <>
struct schedule_on<NSOperationQueue *> {
    inline auto operator()(NSOperationQueue *q) {
        return [q](auto f){
            [q addOperationWithBlock:f];
        };
    }
};
#endif


}
}
