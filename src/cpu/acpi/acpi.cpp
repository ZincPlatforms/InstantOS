#include "acpi.hpp"
#include <limine.h>
#include <string.h>

extern "C" {
    #include <uacpi/uacpi.h>
    #include <uacpi/namespace.h>
    #include <uacpi/tables.h>
    #include <uacpi/event.h>
    #include <uacpi/sleep.h>
    #include <uacpi/utilities.h>
    #include <uacpi/resources.h>
}

ACPI& ACPI::get() {
    static ACPI instance;
    return instance;
}

bool ACPI::initialize() {
    if (initialized) {
        return true;
    }
    
    uacpi_status ret = uacpi_initialize(0);
    if (uacpi_unlikely_error(ret)) {
        return false;
    }

    ret = uacpi_namespace_load();
    if (uacpi_unlikely_error(ret)) {
        return false;
    }

    ret = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(ret)) {
        return false;
    }

    ret = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(ret)) {
        return false;
    }

    initialized = true;
    
    return true;
}

void ACPI::shutdown() {
    if (initialized) {
        initialized = false;
    }
}

void ACPI::reboot() {
    if (!initialized) {
        return;
    }
    
    uacpi_reboot();
    return;
}

void ACPI::sysShutdown() {
    if (!initialized) {
        return;
    }
    
    uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(ret)) {
        return;
    }
    
    asm volatile("cli");
    
    ret = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(ret)) {
        return;
    }
}