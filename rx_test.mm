#import <Cocoa/Cocoa.h>
#import <XCTest/XCTest.h>

#import "rx_dispatch.h"
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
