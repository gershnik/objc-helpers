# ObjC-Helpers #

A collection of helpers to make coding in ObjectiveC more pleasant

## What's included? ##

The library is a collection of mostly independent header files. There is no library to link with. Simply add these headers to your include path and include them as needed.

`sample` directory contains a short sample that demonstrates the usage.

The headers are as follows:

### `BlockUtil.h` ###

Allows nice and safe usage of C++ lambdas instead of ObjectiveC blocks.
 
Why not use blocks? Blocks capture any variable mentioned in them automatically which
causes no end of trouble with inadvertent capture and circular references. The most 
common one is to accidentally capture `self` in a block. There is no protection in 
ObjectiveC - you have to manually inspect code to ensure this doesn't happen. 
Add in code maintainance, copy/paste and you are almost guaranteed to have bugs.
Even if you have no bugs your block code is likley littered with strongSelf
nonsense making it harder to understand.
 
C++ lambdas force you to explicitly specify what you capture, removing this whole problem.
Unfortunately lambdas cannot be used as blocks. This header provides utility functions
to fix this.
The intended usage is

```objc 
[foo doBar:makeBlock([weakSelf = makeWeak(self)] () {
	auto self = makeStrong(self);
	if (!self)
    	return;
    [self doSomething];
}];
```
 
 Note that since in C++ code `self` isn't anything special we can use it as variable name
 for, well, actual self.


### `NSObjectUtil.h` ###

`NSObjectEqual` and `NSObjectHash` - functors that provide equality and hash code for any NSObject and allow them to be used as keys in `std::unordered_map` and `std::unordered_set` for example. These are implemented in terms of `isEqual` and `hash` methods of `NSObject`. 

`operator<<` for any NSObject to print it to an `std::ostream`. This behaves exactly like `%@` formatting flag by delegating either to `descriptionWithLocale:` or to `description`.

### `NSStringUtil.h` ###

`NSStringLess` and `NSStringLocaleLess` comparators. These allow `NSString` objects to be used as keys in `std::map` or `std::set `as well as used in STL sorting and searching algorithms..  

`NSStringEqual` comparator. This is more efficient than `NSObjectEqual` and is implemented in terms of `isEqualToString`. 

`operator<<` to print to an `std::ostream`. This outputs `UTF8String`.

### `NSNumberUtil.h` ###

`NSNumberLess` comparator. This allows `NSNumber` objects to be used as keys in `std::map` or `std::set` as well as used in STL sorting and searching algorithms.  

`NSNumberEqual` comparator. This is more efficient than `NSObjectEqual` and is implemented in terms of `isEqualToNumber`. 

## General notes ##

For all comparators `nil`s are handled properly. A `nil` is equal to `nil` and is less than any non-`nil` object.

Why would you want to have `NSObject`s as keys in `std::map` and such instead of using `NSDictionary`? Usually becuase you want the values to be a non ObjectiveC type, require efficiency or STL compatibility.


