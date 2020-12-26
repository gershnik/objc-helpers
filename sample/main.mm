#import <Foundation/Foundation.h>

#include "../include/BlockUtil.h"
#include "../include/NSStringUtil.h"
#include "../include/NSObjectUtil.h"
#include "../include/NSNumberUtil.h"

#include <iostream>
#include <unordered_map>
#include <map>
#include <codecvt>


@interface BlockDemo : NSObject

@end

@implementation BlockDemo

- (void) callBlock {
    dispatch_sync(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), makeBlock([weakSelf = makeWeak(self)]() {
        
        auto self = makeStrong(weakSelf);
        
        [self aMethod];
    }));
}

- (void) aMethod {
    std::cout << "aMethod called\n";
}

@end

void IOStreamDemo() {
    
    std::cout << @"abc" << '\n';
    std::cout << @1 << '\n';
    std::cout << @[@1, @2, @3] << '\n';
    std::cout << [NSError errorWithDomain:NSURLErrorDomain code:1 userInfo:nil] << '\n';
}

void MapByStringDemo() {
    
    auto another_abc = [NSString stringWithFormat:@"%s", "abc"];
    assert((NSObject*)another_abc != (NSObject*)@"abc");
    
    std::map<NSString *, std::string, NSStringLess> map;
    std::unordered_map<NSString *, std::string, NSObjectHash, NSStringEqual> umap;
    
    map[@"abc"] = "xyz";
    std::cout << map[another_abc] << '\n';
    
    
    umap[@"abc"] = "xyz";
    std::cout << umap[another_abc] << '\n';
}

void MapByNumberDemo() {
    
    auto another_25 = [NSNumber numberWithLong:25];
    assert((NSObject*)another_25 != (NSObject*)@25);
    
    std::map<NSNumber *, std::string, NSNumberLess> map;
    std::unordered_map<NSNumber *, std::string, NSObjectHash, NSNumberEqual> umap;
    
    map[@25] = "abc";
    std::cout << map[another_25] << '\n';
    
    
    umap[@25] = "abc";
    std::cout << umap[another_25] << '\n';
}

void MapByObjectDemo() {
    
    std::unordered_map<NSURL *, int, NSObjectHash, NSObjectEqual> umap;
    
    auto url1 = [NSURL URLWithString:@"http://www.google.com"];
    auto url2 = [NSURL URLWithString:@"http://www.google.com"];
    assert((NSObject*)url1 != (NSObject*)url2);
    
    umap[url1] = 5;
    std::cout << umap[url2] << '\n';
}

void NSStringCharAccessDemo() {
    
    auto str = @"Hello World";
    auto access = NSStringCharAccess(str);
    
    std::u16string reversed(access.rbegin(), access.rend());
    
    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> cv;
    std::cout << cv.to_bytes(reversed) << '\n';
}

int main(int argc, const char * argv[]) {
    
    [[BlockDemo new] callBlock];
    
    IOStreamDemo();
    
    MapByStringDemo();
    
    MapByNumberDemo();
    
    MapByObjectDemo();
    
    NSStringCharAccessDemo();
    
    return 0;
}
