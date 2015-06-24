#import <Cocoa/Cocoa.h>
#import <XCTest/XCTest.h>

#import "rx.h"
namespace rx = windberry::rx;

@interface rx_test : XCTestCase

@end

@implementation rx_test

- (void)setUp {
    [super setUp];
    //
}

- (void)tearDown {
    //
    [super tearDown];
}

- (void)testMap {
    // Using a bool here instead of an expectation will let the compiler optimize out the
    // asserts if it can tell that it's impossible for them to fail.
    bool ok = false;

    rx::pure_observable(3)
    .map([](int x){
        return x * 2;
    }).subscribe([self, &ok](int x){
        ok = true;
        XCTAssertEqual(x, 6);
    });

    XCTAssertTrue(ok);
}

- (void)testBind {
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
    }).subscribe([self, &ok](int x){
        ok = true;
        XCTAssertEqual(x, 13);
    }, [](rx::default_error_type){}, [&completed_count]{
        ++completed_count;
    });

    XCTAssertTrue(ok);
    XCTAssertEqual(completed_count, 1);
}

- (void)testBindError {
    bool ok = false;

    rx::pure_observable<const char *>(3)
    .bind([](int){
        return rx::error_observable<int>("boom");
    })
    .subscribe([self](int){
        XCTFail();
    }, [self, &ok](const char *e){
        XCTAssertEqual(strcmp(e, "boom"), 0);
        ok = true;
    }, [self]{
        //XCTFail(); // FIXME Disallow completion after error?
    });

    XCTAssertTrue(ok);
}

- (void)testCatchTo {
    bool ok = false;

    rx::error_observable<int>("boom")
    .catch_to(rx::error_observable<int>("splat"))
    .catch_to(rx::make_observable<int, const char *>([](auto s){
        s.send_next(7);
        s.send_completed();
    })).subscribe([self, &ok](int x){
        ok = true;
        XCTAssertEqual(x, 7);
    }, [=](rx::default_error_type){
        XCTFail();
    });

    XCTAssertTrue(ok);
}

- (void)testAsync {
    auto ex = [self expectationWithDescription:@"got result"];

    rx::pure_observable(8)
    .subscribe_on(dispatch_get_global_queue(0, 0))
    .deliver_on(dispatch_get_global_queue(0, 0))
    .subscribe([=](int x){
        XCTAssertEqual(x, 8);
        [ex fulfill];
    });

    [self waitForExpectationsWithTimeout:0.1 handler:nil];
}

- (void)testSubject {
    rx::replay_subject<int> s;
    s.send_next(2);
    s.send_next(4);

    int expected = 2;
    s.subscribe(rx::make_observer([&expected, self](int x){
        XCTAssertEqual(x, expected);
        expected *= 2;
    }, [self](rx::default_error_type){
        XCTFail();
    }, [self]{
        XCTFail();
    }));

    s.send_next(8);
}

@end
