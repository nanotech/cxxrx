#include "rx.h"

#include <stdio.h>
#include <string.h>

namespace rx = windberry::rx;

static void assert_eq(int a, int b) {
    fprintf(stderr, "%d %s %d\n", a, a == b ? "==" : "!=", b);
}

static void assert_true(bool p, const char *desc) {
    fprintf(stderr, "%s: %s\n", desc, p ? "true" : "false");
}

static void testMap(void) {
    // Using a bool here instead of an expectation will let the compiler optimize out the
    // asserts if it can tell that it's impossible for them to fail.
    bool ok = false;

    rx::pure_observable(3)
    .map([](int x){
        return x * 2;
    }).subscribe([&ok](int x){
        ok = true;
        assert_eq(x, 6);
    });

    assert_true(ok, "testMap");
}

static void testBind(void) {
    bool ok = false;
    int completed_count = 0;

    rx::make_observable<int>([](auto s){
        s.send_next(3);
        s.send_next(3);
        s.send_next(3);
        s.send_completed();
    })
    .bind([](int x){
        return rx::pure_observable(x + 10);
    }).subscribe([&ok](int x){
        ok = true;
        assert_eq(x, 13);
    }, [](rx::default_error_type){}, [&completed_count]{
        ++completed_count;
    });

    assert_true(ok, "testBind");
    assert_eq(completed_count, 1);
}

static void testBindError(void) {
    bool ok = false;

    rx::pure_observable<const char *>(3)
    .bind([](int){
        return rx::error_observable<int>("boom");
    })
    .subscribe([](int){
        assert_true(false, "testBindError subscribe");
    }, [&ok](const char *e){
        assert_eq(strcmp(e, "boom"), 0);
        ok = true;
    }, []{
        //assert_true(false, ""); // FIXME Disallow completion after error?
    });

    assert_true(ok, "testBindError");
}

static void testCatchTo(void) {
    bool ok = false;

    rx::error_observable<int>("boom")
    .catch_to(rx::error_observable<int>("splat"))
    .catch_to(rx::make_observable<int, const char *>([](auto s){
        s.send_next(7);
        s.send_completed();
    })).subscribe([&ok](int x){
        ok = true;
        assert_eq(x, 7);
    }, [=](rx::default_error_type){
        assert_true(false, "testCatchTo failed");
    });

    assert_true(ok, "testCatchTo");
}

int main(void) {
    testMap();
    testBind();
    testBindError();
    testCatchTo();
}
