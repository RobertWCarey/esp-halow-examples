# Copyright 2025 Robert Carey
# SPDX-License-Identifier: Apache-2.0

set(src "mm_app_common.c"
        "mm_app_loadconfig.c")
set(inc ".")

idf_component_register(INCLUDE_DIRS ${inc}
                       SRCS ${src}
                       REQUIRES mmutils morselib mm_shims mmipal)

# Workaround to allow us to use the link status callback in LWIP. The ESP-IDF does not current (as
# of v5.1.1) exposed this option from the lwip (esp-lwip) component.
add_compile_definitions(LWIP_NETIF_LINK_CALLBACK=1)

