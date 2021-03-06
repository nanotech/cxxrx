#pragma once

#include <exception>
#include <functional>
#include <memory>
#include <utility>

#include <assert.h>
#include <vector>
#include "Maybe.h"

namespace windberry {
namespace rx {

template <typename A, typename B>
using disable_if_same_or_derived = typename std::
    enable_if<!std::is_base_of<A, typename std::remove_reference<B>::type>::value>::type;

struct empty_callable {
    template <typename... Args>
    inline void operator()(Args...) const {}
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

using default_error_type = std::exception_ptr;

template <typename T, typename E, typename... Fs>
struct tuple_observer {
    using value_type = T;
    using error_type = E;
    std::tuple<Fs...> fs;

    tuple_observer(std::tuple<Fs...> &&fs_) : fs(std::move(fs_)) {}

    inline void send_next(T x) const { std::get<0>(fs)(x); }
    inline void send_error(E e) const { callable_or_empty<1, Fs...>(fs)(e); }
    inline void send_completed() const { callable_or_empty<2, Fs...>(fs)(); }
};

template <typename T, typename E = default_error_type>
struct any_observer {
    struct base;

    using value_type = T;

    std::shared_ptr<base> p;
    template <typename O,
              typename = disable_if_same_or_derived<std::decay_t<O>, any_observer<T>>>
    any_observer(O &&o)
        : p(new model<std::decay_t<O>>(std::forward<O>(o))) {}

    inline void send_next(T x) const { p->send_next(x); }
    inline void send_error(E e) const { p->send_error(e); }
    inline void send_completed() const { p->send_completed(); }

    struct base {
        using value_type = T;
        virtual ~base() {}
        virtual void send_next(T) const = 0;
        virtual void send_error(E) const = 0;
        virtual void send_completed() const = 0;
    };

    template <typename O>
    struct model : base {
        O o;
        template <typename O_>
        model(O_ &&o_) : o(std::forward<O_>(o_)) {}
        void send_next(T x) const override { o.send_next(x); }
        void send_error(E e) const override { o.send_error(e); }
        void send_completed() const override { o.send_completed(); }
    };
};

template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())> {};

template <typename R, typename... Args>
struct function_traits<R (*)(Args...)> {
    static const size_t nargs = sizeof...(Args);

    using result_type = R;

    template <size_t i>
    struct arg {
        using type = typename std::tuple_element<i, std::tuple<Args...>>::type;
    };
};

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

template <typename E = default_error_type, typename F>
inline auto make_observer(F &&f) -> tuple_observer<first_argument_type<F>, E, F> {
    return std::make_tuple(std::forward<F>(f));
}
template <typename F, typename G>
inline auto make_observer(F &&f, G &&g)
    -> tuple_observer<first_argument_type<F>, first_argument_type<G>, F, G> {
    return std::make_tuple(std::forward<F>(f), std::forward<G>(g));
}
template <typename F, typename G, typename H>
inline auto make_observer(F &&f, G &&g, H &&h)
    -> tuple_observer<first_argument_type<F>, first_argument_type<G>, F, G, H> {
    return std::make_tuple(std::forward<F>(f), std::forward<G>(g), std::forward<H>(h));
}

template <typename T, typename E, typename F>
struct observable;

template <typename T, typename E = default_error_type, typename F>
inline auto make_observable(F &&f) -> observable<T, E, F> {
    return observable<T, E, F>{std::forward<F>(f)};
}

template <typename E = default_error_type, typename T>
inline auto pure_observable(T x) {
    return make_observable<T, E>([x](auto s) {
        s.send_next(x);
        s.send_completed();
    });
}

template <typename T, typename E = default_error_type>
inline auto empty_observable() {
    return make_observable<T, E>([](auto s) { s.send_completed(); });
}

template <typename T, typename E>
static auto error_observable(E e) {
    return rx::make_observable<T, E>([e](auto s) { s.send_error(e); });
}

template <typename T, typename E = default_error_type>
using any_observable = observable<T, E, std::function<void(any_observer<T>)>>;

// Specialize to implement
template <typename> struct schedule_on;

template <typename T, typename E, typename Observer>
struct forwarding_observer {
    using value_type = T;
    Observer s;
    forwarding_observer(Observer s_) : s(s_) {}

    inline void send_next(T x) const { s.send_next(x); }
    inline void send_error(E e) const { s.send_error(e); }
    inline void send_completed() const { s.send_completed(); }
};

template <typename T, typename E, typename Observer>
struct uncompletable_observer : forwarding_observer<T, E, Observer> {
    uncompletable_observer(Observer s_) : forwarding_observer<T, E, Observer>(s_) {}
    inline void send_completed() const {}
};

template <typename T, typename E, typename Derived>
struct observable_methods {
    using value_type = T;
    using error_type = E;

    template <typename F>
    auto map(F &&f) {
        return bind([f](T x){
            return pure_observable(f(x));
        });
    }

    template <typename Observer, typename F>
    struct bind_observer : forwarding_observer<T, E, Observer> {
        F f;
        bind_observer(Observer s_, F f_)
            : forwarding_observer<T, E, Observer>(s_), f(f_) {}

        inline void send_next(T x) const {
            using U = typename decltype(f(x))::value_type;
            f(x).subscribe(uncompletable_observer<U, E, Observer>{this->s});
        }
    };

    template <typename F>
    auto bind(F &&f) {
        using T2 = typename result_type<F>::value_type;
        using E2 = typename result_type<F>::error_type;
        return make_observable<T2, E2>([f, me = *This()](auto s){
            me.subscribe(bind_observer<decltype(s), F>{s,f});
        });
    }

    template <typename Observer, typename Observable>
    struct catch_to_observer : forwarding_observer<T, E, Observer> {
        Observable o;
        catch_to_observer(Observer s_, Observable o_)
            : forwarding_observer<T, E, Observer>(s_), o(o_) {}

        inline void send_error(E) const { o.subscribe(this->s); }
    };

    template <typename Observable>
    auto catch_to(Observable o) {
        using T2 = typename Observable::value_type;
        using E2 = typename Observable::error_type;
        static_assert(std::is_same<T, T2>(), "Value types must match");
        static_assert(std::is_same<E, E2>(), "Error types must match");
        return make_observable<T2, E2>([o, me = *This()](auto s) {
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
            inline void send_error(E e)  const { f([s = s, e]{ s.send_error(e); }); }
            inline void send_completed() const { f([s = s]{ s.send_completed(); }); }
        };
        return make_observable<T, E>([f, me = *This()](auto s){
            me.subscribe(deliver_observer{f, s});
        });
    }

    template <typename F>
    auto subscribe_with(F f) {
        auto me = *This();
        return make_observable<T, E>([f, me](auto s){
            f([s, me]{
                me.subscribe(s);
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

    auto any() -> any_observable<T, E> {
        return any_observable<T, E>([me = *This()](auto s){
            me.subscribe(s);
        });
    }

private:
    // Use the correct pointer type when copying to prevent slicing.
    inline Derived* This() { return static_cast<Derived *>(this); }

    template <typename... Args>
    inline void subscribe(Args &&... args) const {
        static_cast<const Derived *>(this)->subscribe(std::forward<Args>(args)...);
    }
};

template <typename T, typename E, typename DoSub>
struct observable : observable_methods<T, E, observable<T, E, DoSub>> {
    DoSub do_subscribe;

    observable() : do_subscribe([](auto){}) {}

    template <typename DoSubscribe,
              typename = disable_if_same_or_derived<observable, DoSubscribe>>
    explicit observable(DoSubscribe &&o)
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
};

template <typename T, typename E = default_error_type>
struct replay_subject : observable_methods<T, E, replay_subject<T, E>> {
    replay_subject() : st(std::make_shared<state>()) {}

    void subscribe(const any_observer<T> &original) const {
        st->observers.push_back(original);
        auto &o = st->observers.back();
        for (auto &e : st->events) {
            e.send(o);
        }
    }

    void send_next(T x)   const { st->add_event(event{WT{x}}); }
    void send_error(E e)     const { st->add_event(event{WE{e}}); }
    void send_completed() const { st->add_event(event{}); }

  private:
    // Wrap Objective-C objects to avoid hitting ARC restrictions on putting them
    // directly in the Maybe union.
    struct WT { T unwrap; };
    struct WE { E unwrap; };

    enum class event_type : char { next, error, completed };
    struct event {
        Maybe<WT> value;
        Maybe<WE> error;
        event_type type;

        explicit event(WT x) : value(Just(std::move(x))), type(event_type::next) {}
        explicit event(WE e) : error(Just(std::move(e))), type(event_type::error) {}
        explicit event() : type(event_type::completed) {}

        void send(any_observer<T> &o) {
            switch (type) {
                case event_type::next:      o.send_next((*value.orNull()).unwrap); break;
                case event_type::error:     o.send_error((*error.orNull()).unwrap); break;
                case event_type::completed: o.send_completed(); break;
            }
        }
    };

    struct state {
        std::vector<event> events;
        std::vector<any_observer<T>> observers;

        // final when error or complete; next is not final
        event_type final_event_type = event_type::next;

        void add_event(event &&e) {
            assert(final_event_type == event_type::next);
            final_event_type = e.type;
            events.emplace_back(std::move(e));
            auto &ev = events.back();
            for (auto &o : observers) {
                ev.send(o);
            }
        }
    };
    std::shared_ptr<state> st;
};

struct unit {};

}
}
