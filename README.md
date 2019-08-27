# cxxrx

![Standard](https://img.shields.io/badge/c%2B%2B-14-blue.svg)
![License](https://img.shields.io/badge/license-MIT-blue.svg)
[![Build Status](https://nanotech-oss.visualstudio.com/nanotech/_apis/build/status/nanotech.cxxrx?branchName=master)](https://nanotech-oss.visualstudio.com/nanotech/_build/latest?definitionId=2&branchName=master)

cxxrx is a small observable stream library for C++, inspired by [Reactive Extensions](reactivex).

[reactivex]: http://reactivex.io/

## Example

```c++
#include "rx.h"
namespace rx = windberry::rx;

#include <stdio.h>

static rx::any_observable<int> number_sequence(int upto) {
    return rx::make_observable<int>([=](auto subscriber) {
        for (int i = 1; i <= upto; ++i) {
            subscriber.send_next(i);
        }
        subscriber.send_completed();
    }).any();
}

int main() {
    auto seq = number_sequence(10);
    seq.subscribe([](int x) {
        printf("%d ", x);
    });
    printf("\n");
    seq.map([](int x) {
        return x * 10;
    }).subscribe([](int x) {
        printf("%d ", x);
    });
    printf("\n");
}
```

```
$ c++ -std=c++14 example.cc -o example && ./example
1 2 3 4 5 6 7 8 9 10
10 20 30 40 50 60 70 80 90 100
```

Observables created with `rx::make_observable` are [cold][cold-vs-hot] and only begin running once they are subscribed to. Additionally, each subscription will invoke the observable's generating function separately.

[cold-vs-hot]: https://blog.thoughtram.io/angular/2016/06/16/cold-vs-hot-observables.html

## Documentation

### Contents

- [Definitions](#definitions)
- [Functions](#functions)
- [Subscriber methods](#subscribert-e-methods)
- [Observable methods](#observablet-e-methods)
- [Subjects](#subjects)
- [Specializations](#specializations)

### Definitions

- Observable: An object that sends events to its Subscribers.
- Event: A value, error, or completion from an Observable.
- Subscriber: A function or object that receives values, errors, and/or a completion event. Referred to as "observers" in the source.
- Generator: A function that produces the events of an Observable.
- Queue: An asynchronous context to run functions in, such as a `libdispatch` `dispatch_queue_t`.

All of cxxrx is in the `windberry::rx` namespace. Names below will be referred to without the namespace.

Common type variables used are:

- `T`, the type of value the observable produces.
- `U`, another type of value some observable produces.
- `E`, the type of error the observable produces.
- `Subscriber`, some type that is a subscriber, but has an unspecified or unnameable concrete type.
- `Observable`, some type that is an observable, but has an unspecified or unnameable concrete type.

A pseudo syntax of `function_argument(Arg1Type, Arg2Type, ...) -> ReturnType` will be used for function types in the style of `auto f(T) -> U`, but without the `auto` prefix. `-> void` return types are omitted.

### Functions

##### `make_observable<T, E>(generator(Subscriber<T, E>)) -> Observable<T, E>`

Creates a [cold][cold-vs-hot] observable that runs `generator` for each subscription.

`generator` is run synchronously from the observable's `subscribe` method. Subscription callbacks are run synchronously from `send_*` methods called inside `generator`.

To run `generator` or subscription callbacks asynchronously, see the `subscribe_on` and `deliver_on` methods of Observable.

##### `pure_observable<T>(T) -> Observable<T, E>`

Creates an observable that immediately sends exactly one value and then completes.

##### `error_observable<E>(E) -> Observable<T, E>`

Creates an observable that immediately sends exactly one error.

##### `empty_observable() -> Observable<T, E>`

Creates an observable that immediately sends that it is completed.

##### `make_observer<T, E>(on_next(T)) -> Subscriber<T, E>`
##### `make_observer<T, E>(on_next(T), on_error(E)) -> Subscriber<T, E>`
##### `make_observer<T, E>(on_next(T), on_error(E), on_complete()) -> Subscriber<T, E>`

Creates a subscriber object from the given callbacks. Currently necessary for subscribing to `replay_subject`s.

### `Subscriber<T, E>` methods

##### `.send_next(T)`

Runs the subscriber's `on_next` callback with the given value.

##### `.send_error(E)`

Runs the subscriber's `on_error` callback with the given error.

##### `.send_completed()`

Runs the subscriber's `on_complete` callback.

### `Observable<T, E>` methods

##### `.subscribe(on_next(T))`
##### `.subscribe(on_next(T), on_error(E))`
##### `.subscribe(on_next(T), on_error(E), on_complete())`

Runs the observable's generator with the given callback functions. Each `send_*` method on the subscriber passed to the generator will invoke the respective `on_*` callback.

Omitted callbacks default to empty functions.

##### `.map(fn(T) -> U) -> Observable<U, E>`

Applies `f` to values from the observable. `f` may return values of a different type `U`.

##### `.bind(fn(T) -> Observable<U, E>) -> Observable<U, E>`

Applies `f` to values from the observable, subscribing to each returned observable. `f` may return observables with a different type `U`.

##### `.any() -> any_observable<T, E>`

Erases the function types of the observable, which may be unnameable. The returned `any_observable` contains the observable in a new heap allocation.

`any_observable` is the observable equivalent of `std::function`.

##### `.subscribe_on(Queue) -> Observable<T, E>`

Creates an observable that runs its generator on the given `Queue`.

For example, to run a long-running generator on a background queue.

Requires a specialization of `struct schedule_on<Queue>` to be implemented, such as from `rx_dispatch.h` for `dispatch_queue_t`.

##### `.deliver_on(Queue) -> Observable<T, E>`

Creates an observable that runs subscriber callbacks on the given `Queue`.

For example, to allow callbacks to update UI elements from a UI thread.

Requires a specialization of `struct schedule_on<Queue>` to be implemented, such as from `rx_dispatch.h` for `dispatch_queue_t`.

### Subjects

Subjects are special observables that allow the submission of events from outside of a generator. Currently there is only `replay_subject`.

##### `replay_subject<T, E>`

A subject that records and replays any events it receives to new subscribers.

Currently requires subscribing with a subscriber object created using `make_observer` due to overload issues in the implementation.

Example:

```c++
int i = 1;
rx::replay_subject<int> subject;
subject.send_next(i++);
subject.send_next(i++);

for (int j = 0; j < 2; ++j) {
    subject.subscribe(rx::make_observer([](int x) {
        printf("%d ", x);
    }));
    printf("\n");
}

subject.send_next(i++);
printf("\n");
```

```
1 2
1 2
3 3
```

### Specializations

##### `struct schedule_on<Queue>`

Specialize `schedule_on` and implement an `operator()` that
returns a callable object that runs its argument on the given queue.

Example:

```c++
template <>
struct schedule_on<dispatch_queue_t> {
    inline auto operator()(dispatch_queue_t q) {
        return std::bind(dispatch_async, q, std::placeholders::_1);
    }
};
```
