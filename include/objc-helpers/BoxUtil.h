/*
 Copyright 2023 Eugene Gershnik

 Use of this source code is governed by a BSD-style
 license that can be found in the LICENSE file or at
 https://github.com/gershnik/objc-helpers/blob/main/LICENSE
*/

#ifndef HEADER_BOX_UTIL_INCLUDED
#define HEADER_BOX_UTIL_INCLUDED

#if !__cpp_concepts
    #error This header requires C++20 mode or above with concepts support
#endif

#include <objc/runtime.h>
#import <Foundation/Foundation.h>

#include <dlfcn.h>

#include <cxxabi.h>

#include <concepts>
#include <string>
#include <ostream>
#include <sstream>
#include <filesystem>


/**
 Marks that NSObject conforming to it is a boxed C++ object
 */
@protocol BoxedValue<NSObject>
@end

/**
 Marks that NSObject conforming to it can be comapred to others of the same type
 */
@protocol BoxedComparable<NSObject>
- (NSComparisonResult) compare:(id<BoxedComparable> __nonnull)other;
@end



namespace BoxMakerDetail __attribute__((visibility("hidden"))) {
    using std::to_string;
    
    
    template<class T>
    concept ToStringDescriptable = requires(T obj) {
        { to_string(obj) } -> std::same_as<std::string>;
    };
    
    
    template<class T>
    concept OStreamDescriptable = requires(T obj, std::ostream & str) {
        { str << obj };
    };
    
    template<class T>
    concept Hashable = std::is_default_constructible_v<std::hash<T>>;
    
    
    inline decltype(auto) getObjcData() {
        
        struct ObjcData {
            SEL newSel = @selector(init);
            SEL initSel = @selector(init);
            SEL deallocSel = sel_registerName("dealloc");
            SEL descriptionSel = @selector(description);
            SEL copyWithZoneSel = @selector(copyWithZone:);
            SEL isEqualSel = @selector(isEqual:);
            SEL hashSel = @selector(hash);
            SEL compareSel = @selector(compare:);
            
            Class NSObjectClass = NSObject.class;
            id (*NSObject_initIMP)(id, SEL);
            std::string modulePrefix;
            
            ObjcData(const void * sym):
                NSObject_initIMP((decltype(NSObject_initIMP))class_getMethodImplementation(NSObjectClass, initSel)) {
                    
                Dl_info info;
                if (!dladdr(sym, &info))
                    @throw [NSException exceptionWithName:NSGenericException reason:@"dladdr failed" userInfo:nullptr];
                std::filesystem::path mypath(info.dli_fname);
                modulePrefix = mypath.stem().string() + '!';
            }
        };
        
        static const ObjcData data(&data);
        return static_cast<const ObjcData &>(data);
    };
    
    struct ClassData {
        Class __nullable cls = nullptr;
        ptrdiff_t _valueOffset = 0;
        std::string tName;
        
        ClassData() noexcept = default;
        
        ClassData(ClassData && src) noexcept :
            cls(std::exchange(src.cls, nullptr)),
            _valueOffset(std::exchange(src._valueOffset, 0)),
            tName(std::move(src.tName))
        {}
        
        ~ClassData() noexcept {
            if (cls)
                objc_disposeClassPair(cls);
        }
        
        auto addrOfValue(id __nonnull obj) const -> void * __nonnull
            { return (void*)((std::byte *)(__bridge void *)obj + this->_valueOffset); }
    };

}


template<class T>
class __attribute__((visibility("hidden"))) BoxMaker {
private:
    static auto detectBoxedType() {
        if constexpr (std::totally_ordered<T>) {
            if constexpr (std::is_copy_constructible_v<T>) {
                return (NSObject<BoxedValue, BoxedComparable, NSCopying> *)nullptr;
            } else {
                return (NSObject<BoxedValue, BoxedComparable> *)nullptr;
            }
        } else {
            if constexpr (std::is_copy_constructible_v<T>) {
                return (NSObject<BoxedValue, NSCopying> *)nullptr;
            } else {
                return (NSObject<BoxedValue> *)nullptr;
            }
        }
    }
    
public:
    using BoxedType = decltype(detectBoxedType());
    
private:
    
    static __attribute__((visibility("hidden"))) auto getClassData() -> const BoxMakerDetail::ClassData & {
        
        static BoxMakerDetail::ClassData data = [] {
            using namespace std::literals;
            
            const auto & objcData = BoxMakerDetail::getObjcData();
            
            BoxMakerDetail::ClassData classData;
            
            auto & tid = typeid(T);
            classData.tName = demangle(tid.name());
            
            std::string className = objcData.modulePrefix + "Boxed["s + tid.name() + ']';
            Class cls = objc_allocateClassPair(objcData.NSObjectClass, className.c_str(), 0);
            classData.cls = cls;
            
            if (!class_addIvar(cls, "_value", sizeof(T), alignof(T), @encode(T)))
                @throw [NSException exceptionWithName:NSGenericException reason:@"class_addIvar(_value) failed" userInfo:nullptr];
            auto valueIvar = class_getInstanceVariable(cls, "_value");
            classData._valueOffset = ivar_getOffset(valueIvar);
                        
            if (!class_addMethod(cls, objcData.initSel, IMP(init), "@@:"))
                @throw [NSException exceptionWithName:NSGenericException reason:@"class_addMethod(init) failed" userInfo:nullptr];
            
            if (!class_addMethod(cls, objcData.deallocSel, IMP(dealloc), "v@:"))
                @throw [NSException exceptionWithName:NSGenericException reason:@"class_addMethod(dealloc) failed" userInfo:nullptr];
            
            if (!class_addMethod(cls, objcData.descriptionSel, IMP(description), "@@:"))
                @throw [NSException exceptionWithName:NSGenericException reason:@"class_addMethod(description) failed" userInfo:nullptr];
            
            if constexpr (std::is_copy_constructible_v<T>) {
                if (!class_addMethod(cls, objcData.copyWithZoneSel, IMP(copyWithZone), "@@:@"))
                    @throw [NSException exceptionWithName:NSGenericException reason:@"class_addMethod(copyWithZone) failed" userInfo:nullptr];
            }
            
            if (!class_addMethod(cls, objcData.isEqualSel, IMP(isEqual), "@@:@"))
                @throw [NSException exceptionWithName:NSGenericException reason:@"class_addMethod(isEqualTo) failed" userInfo:nullptr];
            
            if (!class_addMethod(cls, objcData.hashSel, IMP(hash), (@encode(NSUInteger) + "@:"s).c_str()))
                @throw [NSException exceptionWithName:NSGenericException reason:@"class_addMethod(hash) failed" userInfo:nullptr];
            
            if constexpr (std::totally_ordered<T>) {
                if (!class_addMethod(cls, objcData.compareSel, IMP(compare), (@encode(NSComparisonResult) + "@:@"s).c_str()))
                    @throw [NSException exceptionWithName:NSGenericException reason:@"class_addMethod(compare) failed" userInfo:nullptr];
            }
            
            objc_registerClassPair(cls);
            
            return classData;
        }();
        
        return data;
    }
    
    static auto demangle(const char * __nonnull name) -> std::string {

        int status = 0;
        std::unique_ptr<char, void(*)(void*)> res {
            abi::__cxa_demangle(name, nullptr, nullptr, &status),
            std::free
        };
        return (status==0) ? res.get() : name ;
    }
    
    static auto init(id __nonnull, SEL __nonnull) -> id __nullable {
        @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"calling init on this object is not allowed" userInfo:nullptr];
    }
    
    static void dealloc(id __nonnull self, SEL __nonnull) {
        auto & classData = getClassData();
        auto * val = (T *)classData.addrOfValue(self);
        
        val->~T();
    }
    
    static auto description(id __nonnull self, SEL __nonnull) -> NSString * __nonnull {
        auto & classData = getClassData();
        auto * val = (T *)classData.addrOfValue(self);
        
        if constexpr (BoxMakerDetail::ToStringDescriptable<T>) {
            using std::to_string;
            auto str = to_string(*val);
            return @(str.c_str());
        } else if constexpr (BoxMakerDetail::OStreamDescriptable<T>) {
            std::ostringstream str;
            str << *val;
            return @(str.str().c_str());
        } else {
            return [NSString stringWithFormat:@"Boxed object of type \"%s\"", classData.tName.c_str()];
        }
    }
    
    static auto copyWithZone(id __nonnull self, SEL __nonnull, NSZone * __nullable) {
        
        auto & classData = getClassData();
        auto * val = (T *)classData.addrOfValue(self);
        return box(static_cast<const T &>(*val));
    }
    
    static auto isEqual(NSObject<BoxedValue> * __nonnull self, SEL __nonnull, id __nullable other) -> BOOL {
        
        if (other == self)
            return YES;
        if (!other)
            return NO;
        auto & classData = getClassData();
        if (object_getClass(other) != classData.cls)
            return NO;
        if constexpr (std::equality_comparable<T>) {
            auto * val = (T *)classData.addrOfValue(self);
            auto * otherVal = (T *)classData.addrOfValue(other);
            return *val == *otherVal;
        } else {
            return NO;
        }
    }
    
    static auto hash(id __nonnull self, SEL __nonnull) -> NSUInteger {
        auto & classData = getClassData();
        auto * val = (T *)classData.addrOfValue(self);
        if constexpr (BoxMakerDetail::Hashable<T>) {
            return std::hash<T>()(*val);
        } else if constexpr (std::equality_comparable<T>) {
            auto reason = [NSString stringWithFormat:@"hash is called on boxed type %s, which defines operator== but "
                                                      "does not have std::hash<%s> specialization. Provide such "
                                                      "specialization to ensure behavior consistent with operator==",
                           classData.tName.c_str(), classData.tName.c_str()];
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:reason userInfo:nullptr];
        } else {
            return std::hash<void *>()((__bridge void *)self);
        }
    }
    
    static auto compare(id __nonnull self, SEL __nonnull, id __nullable other) -> NSComparisonResult {
        if (other == self)
            return NSOrderedSame;
        if (!other)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"comparison operand is nil" userInfo:nullptr];
        auto & classData = getClassData();
        if (object_getClass(other) != classData.cls)
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"comparison operand is of invalid type" userInfo:nullptr];
        auto * val = (T *)classData.addrOfValue(self);
        auto * otherVal = (T *)classData.addrOfValue(other);
        const auto res = std::compare_strong_order_fallback(*val, *otherVal);
        return NSComparisonResult(-(res < 0) + (res > 0));
    }
public:
    template<class... Args>
    requires(std::is_constructible_v<T, Args...>)
    static auto box(Args &&... args) -> BoxedType {
        auto & objcData = BoxMakerDetail::getObjcData();
        auto & classData = getClassData();
        
        id obj = class_createInstance(classData.cls, 0);
        obj = objcData.NSObject_initIMP(obj, objcData.initSel);
        if (!obj)
            return nullptr;
        auto * dest = classData.addrOfValue(obj);
        new (dest) T(std::forward<Args>(args)...);
        return obj;
    }
    
    static auto boxedValue(BoxedType obj) -> T & {
        auto & classData = getClassData();
        
        if (obj.class != classData.cls) {
            auto reason = [NSString stringWithFormat:@"this object is not a boxed value of type %s", classData.tName.c_str()];
            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:reason userInfo:nullptr];
        }
        
        auto * val = (T *)classData.addrOfValue(obj);
        return *val;
    }
};

/**
 Box a value emplacing it.
 
 Call it like this:
 @code
 //this constructs a boxed vector of 7 chars 'a'
 auto obj = box<std::vector<char>>(7, 'a');
 @endcode
 */
template<class T, class... Args>
requires(std::is_constructible_v<T, Args...>)
inline auto box(Args &&... args) -> BoxMaker<T>::BoxedType
    { return BoxMaker<T>::box(std::forward<Args>(args)...); }


/**
 Box a value via copy or move
 
 @return NSObject\<BoxedValue, other protocols\> \* object where other protocols are as follows
 
 - BoxedComparable if the boxed value can be `<=>` compared producing strong ordering
 - NSCopying if the boxed value has copy constructor
 
 Call it like this:
 @code
 std::string str("abc")
 //this constructs a boxed copy of the string
 auto obj = box(str);
 //this constructs a boxed moved copy of the string
 auto obj1 = box(std::move(str));
 @endcode
 */
template<class T>
requires(std::is_constructible_v<std::remove_cvref_t<T>, T &&>)
inline auto box(T && src) -> BoxMaker<T>::BoxedType
    { return BoxMaker<std::remove_cvref_t<T>>::box(std::forward<T>(src)); }

/**
 Retrieve a reference to the boxed value
 */
template<class T>
inline auto boxedValue(typename BoxMaker<T>::BoxedType obj) -> T &
    { return BoxMaker<T>::boxedValue(obj); }

#endif
