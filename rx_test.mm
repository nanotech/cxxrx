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

    rx::pure_observable(3)
    .bind([](int x){
        return rx::make_observable<int>([x](auto s){
            s.send_next(x + 10);
        });
    }).subscribe([self, &ok](int x){
        ok = true;
        XCTAssertEqual(x, 13);
    });

    XCTAssertTrue(ok);
}

- (void)testCatchTo {
    bool ok = false;

    rx::error_observable<int>()
    .catch_to(rx::error_observable<int>())
    .catch_to(rx::make_observable<int>([](auto s){
        s.send_next(7);
        s.send_completed();
    })).subscribe([self, &ok](int x){
        ok = true;
        XCTAssertEqual(x, 7);
    }, [=]{
        XCTAssert(0);
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

@end
