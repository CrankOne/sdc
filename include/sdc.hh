/* SDC - A self-descriptive calibration data format library.
 * Copyright (C) 2022  Renat R. Dusaev  <renat.dusaev@cern.ch>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#pragma once

/**\file
 * \brief Wrapper include file for SDC as a library
 *
 * This file is included for SDC used as library. Alternative usage is to use
 * SDC as a header-only source (then one shall use `sdc-base.hh` directly).
 * This header turns on `SDC_NO_IMPLEM` macro disabling header's implementation
 * codes.
 * */

//#include "sdc-config.h"

#ifndef SDC_NO_IMPLEM
#   define SDC_NO_IMPLEM 1
#endif

#include "sdc-base.hh"

#undef SDC_NO_IMPLEM

