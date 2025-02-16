/*
 * Copyright 2022-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

/**
 * @ingroup MMWLAN_REGDB
 * @defgroup MMWLAN_REGDB_HEADER Template S1G regulatory database header
 *
 * \@{
 *
 * @section MMWLAN_REGDB_OVERRIDE Overriding Regulatory Database
 * The data in the database is defined in @ref mm_app_regdb.c, and can be overwritten as required.
 */

#pragma once

#include "mmwlan.h"

/**
 * Get a pointer to regulatory_db.
 * Stores channel information for each supported domain.
 * For more info, @ref see mm_app_regdb.c.
 *
 * @return Reference to the regulatory database
 */
const struct mmwlan_regulatory_db *get_regulatory_db(void);

/** \@} */
