#include <cstdio.hpp>
#include <instant/syscall.hpp>
#include <cstdint>
#include <cstddef>

namespace __cxxabiv1 {
    struct __cxa_exception {
        void* reserve;
        void* exceptionType;
        void (*exceptionDestructor)(void*);
        void (*unexpectedHandler)();
        void (*terminateHandler)();
        __cxa_exception* nextException;
        int handlerCount;
        int handlerSwitchValue;
        const char* actionRecord;
        const char* languageSpecificData;
        void* catchTemp;
        void* adjustedPtr;
        void* thrownObject;
    };
    
    struct __cxa_eh_globals {
        __cxa_exception* caughtExceptions;
        unsigned int uncaughtExceptions;
    };
    
    static __cxa_eh_globals eh_globals;
    
    extern "C" {
        __cxa_eh_globals* __cxa_get_globals() {
            return &eh_globals;
        }
        
        __cxa_eh_globals* __cxa_get_globals_fast() {
            return &eh_globals;
        }
        
        void* __cxa_allocate_exception(size_t thrown_size) {
            size_t size = thrown_size + sizeof(__cxa_exception);
            void* mem = operator new(size);
            
            __cxa_exception* header = reinterpret_cast<__cxa_exception*>(mem);
            header->reserve = nullptr;
            header->exceptionType = nullptr;
            header->exceptionDestructor = nullptr;
            header->unexpectedHandler = nullptr;
            header->terminateHandler = nullptr;
            header->nextException = nullptr;
            header->handlerCount = 0;
            
            return reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(mem) + sizeof(__cxa_exception));
        }
        
        void __cxa_free_exception(void* thrown_exception) {
            if (!thrown_exception) return;
            
            void* mem = reinterpret_cast<void*>(
                reinterpret_cast<uint8_t*>(thrown_exception) - sizeof(__cxa_exception)
            );
            operator delete(mem);
        }
        
        void __cxa_throw(void* thrown_exception, void* tinfo, void (*dest)(void*)) {
            __cxa_exception* header = reinterpret_cast<__cxa_exception*>(
                reinterpret_cast<uint8_t*>(thrown_exception) - sizeof(__cxa_exception)
            );
            
            header->exceptionType = tinfo;
            header->exceptionDestructor = dest;
            header->thrownObject = thrown_exception;
            
            eh_globals.uncaughtExceptions++;
            
            printf("Exception thrown but no handler found\n");
            syscall0(0);
        }
        
        void* __cxa_begin_catch(void* exc_obj_in) {
            __cxa_exception* header = reinterpret_cast<__cxa_exception*>(
                reinterpret_cast<uint8_t*>(exc_obj_in) - sizeof(__cxa_exception)
            );
            
            header->handlerCount++;
            eh_globals.caughtExceptions = header;
            
            if (eh_globals.uncaughtExceptions > 0) {
                eh_globals.uncaughtExceptions--;
            }
            
            return header->adjustedPtr ? header->adjustedPtr : exc_obj_in;
        }
        
        void __cxa_end_catch() {
            __cxa_exception* header = eh_globals.caughtExceptions;
            if (!header) return;
            
            header->handlerCount--;
            
            if (header->handlerCount == 0) {
                if (header->exceptionDestructor) {
                    header->exceptionDestructor(header->thrownObject);
                }
                __cxa_free_exception(header->thrownObject);
                eh_globals.caughtExceptions = header->nextException;
            }
        }
        
        void __cxa_rethrow() {
            __cxa_exception* header = eh_globals.caughtExceptions;
            if (!header) {
                printf("Rethrow with no active exception\n");
                syscall0(0);
            }
            
            header->handlerCount--;
            eh_globals.uncaughtExceptions++;
            
            printf("Exception rethrown but no handler found\n");
            syscall0(0);
        }
        
        void* __cxa_current_exception_type() {
            __cxa_exception* header = eh_globals.caughtExceptions;
            return header ? header->exceptionType : nullptr;
        }
        
        void __cxa_call_unexpected(void* exc_obj_in) {
            printf("Unexpected exception\n");
            syscall0(0);
        }
    }
}

namespace std {
    void terminate() noexcept {
        printf("std::terminate called\n");
        syscall0(0);
        while(1);
    }
    
    void unexpected() {
        printf("std::unexpected called\n");
        syscall0(0);
        while(1);
    }
}

extern "C" {
    void __cxa_call_terminate(void* exc_obj) {
        std::terminate();
    }
    
    void _Unwind_Resume(void*) {
        printf("_Unwind_Resume called\n");
        syscall0(0);
    }
    
    typedef int _Unwind_Reason_Code;
    typedef int _Unwind_Action;
    
    struct _Unwind_Context;
    struct _Unwind_Exception;
    
    _Unwind_Reason_Code _Unwind_RaiseException(_Unwind_Exception*) {
        printf("_Unwind_RaiseException called\n");
        syscall0(0);
        return 0;
    }
    
    void _Unwind_DeleteException(_Unwind_Exception*) {
    }
    
    unsigned long _Unwind_GetGR(_Unwind_Context*, int) {
        return 0;
    }
    
    void _Unwind_SetGR(_Unwind_Context*, int, unsigned long) {
    }
    
    unsigned long _Unwind_GetIP(_Unwind_Context*) {
        return 0;
    }
    
    void _Unwind_SetIP(_Unwind_Context*, unsigned long) {
    }
    
    unsigned long _Unwind_GetLanguageSpecificData(_Unwind_Context*) {
        return 0;
    }
    
    unsigned long _Unwind_GetRegionStart(_Unwind_Context*) {
        return 0;
    }
}
