/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Morse Micro load configuration helper
 *
 * This file contains helper routines to load commonly used configuration settings
 * such as SSID, password, IP address settings and country code from the config store.
 * If a particular setting is not found, defaults are used.  It is safe to call these
 * functions if none of the settings are available in config store.
 */

#include "mm_app_loadconfig.h"
#include "mmipal.h"
#include "mmosal.h"
#include "mmwlan.h"
#include "mmwlan_regdb.def"

#ifndef COUNTRY_CODE
#define COUNTRY_CODE CONFIG_HALOW_COUNTRY_CODE
#endif

/* Default SSID  */
#ifndef SSID
/** SSID of the AP to connect to. (Do not quote; it will be stringified.) */
#define SSID CONFIG_HALOW_SSID
#endif

/* Default passphrase  */
#ifndef SAE_PASSPHRASE
/** Passphrase of the AP (ignored if security type is not SAE).
 *  (Do not quote; it will be stringified.) */
#define SAE_PASSPHRASE CONFIG_HALOW_PASSWORD
#endif

/* Default security type  */
#ifndef SECURITY_TYPE
/** Security type (@see mmwlan_security_type). */
#define SECURITY_TYPE MMWLAN_SAE
#endif

/* Configure the STA to use DHCP, this overrides any static configuration.
 * If the @c ip.dhcp_enabled is set in the config store that will take priority */
#define ENABLE_DHCP CONFIG_ENABLE_DHCP

/* Static Network configuration */
#ifndef STATIC_LOCAL_IP
/** Statically configured IP address (if ENABLE_DHCP is not set). */
#define STATIC_LOCAL_IP CONFIG_STATIC_LOCAL_IP
#endif
#ifndef STATIC_GATEWAY
/** Statically configured gateway address (if ENABLE_DHCP is not set). */
#define STATIC_GATEWAY CONFIG_STATIC_GATEWAY
#endif
#ifndef STATIC_NETMASK
/** Statically configured netmask (if ENABLE_DHCP is not set). */
#define STATIC_NETMASK CONFIG_STATIC_NETMASK
#endif

#define ENABLE_AUTOCONFIG CONFIG_ENABLE_AUTOCONFIG
/* Static Network configuration */
#ifndef STATIC_LOCAL_IP6
/** Statically configured IP address (if ENABLE_AUTOCONFIG is not set). */
#define STATIC_LOCAL_IP6 CONFIG_STATIC_LOCAL_IP6
#endif

/** Stringify macro. Do not use directly; use @ref STRINGIFY(). */
#define _STRINGIFY(x) #x
/** Convert the content of the given macro to a string. */
#define STRINGIFY(x) _STRINGIFY(x)

void load_mmipal_init_args(struct mmipal_init_args *args)
{
#if ENABLE_DHCP
    args->mode = MMIPAL_DHCP;
#else
    args->mode = MMIPAL_STATIC;
    (void)mmosal_safer_strcpy(args->ip_addr, STATIC_LOCAL_IP, sizeof(args->ip_addr));
    (void)mmosal_safer_strcpy(args->netmask, STATIC_NETMASK, sizeof(args->netmask));
    (void)mmosal_safer_strcpy(args->gateway_addr, STATIC_GATEWAY, sizeof(args->gateway_addr));
#endif

    if (args->mode == MMIPAL_DHCP)
    {
        printf("Initialize IPv4 using DHCP...\n");
    }
    else if (args->mode == MMIPAL_DHCP_OFFLOAD)
    {
        printf("Initialize IPv4 using DHCP offload...\n");
    }
    else
    {
        printf("Initialize IPv4 with static IP: %s...\n", args->ip_addr);
    }

#if ENABLE_AUTOCONFIG
    args->ip6_mode = MMIPAL_IP6_AUTOCONFIG;
#else
    args->ip6_mode = MMIPAL_IP6_STATIC;
    (void)mmosal_safer_strcpy(args->ip6_addr, STATIC_LOCAL_IP6, sizeof(args->ip6_addr));
#endif

    if (args->ip6_mode == MMIPAL_IP6_AUTOCONFIG)
    {
        printf("Initialize IPv6 using Autoconfig...\n");
    }
    else
    {
        printf("Initialize IPv6 with static IP %s\n", args->ip6_addr);
    }
}

const struct mmwlan_s1g_channel_list *load_channel_list(void)
{
    char strval[16];
    const struct mmwlan_s1g_channel_list *channel_list;

    (void)mmosal_safer_strcpy(strval, COUNTRY_CODE, sizeof(strval));
    channel_list = mmwlan_lookup_regulatory_domain(get_regulatory_db(), strval);
    if (channel_list == NULL)
    {
        printf("Could not find specified regulatory domain matching country code %s\n", strval);
        printf("Please set the configuration key wlan.country_code to the correct country code.\n");
        MMOSAL_ASSERT(false);
    }
    return channel_list;
}

void load_mmwlan_sta_args(struct mmwlan_sta_args *sta_config)
{
    (void)mmosal_safer_strcpy((char *)sta_config->ssid, SSID, sizeof(sta_config->ssid));
    sta_config->ssid_len = strlen((char *)sta_config->ssid);

    (void)mmosal_safer_strcpy(sta_config->passphrase, SAE_PASSPHRASE,
                              sizeof(sta_config->passphrase));
    sta_config->passphrase_len = strlen(sta_config->passphrase);

    sta_config->security_type = SECURITY_TYPE;
}

void load_mmwlan_settings(void)
{
}
