//-----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2025, Thierry Lelegard
// BSD-2-Clause license, see LICENSE.txt file or https://tsduck.io/license
//
//-----------------------------------------------------------------------------

#include "tsPlatform.h"

#define TS_COMIDS_DEFINE 1
#include <initguid.h>
#include <tuner.h>
TS_PUSH_WARNING()
// disable warning on untested API on older mingw-w64
#if defined(__clang__)
    #pragma clang diagnostic ignored "-W#warnings"
#endif
TS_GCC_NOWARNING(cpp)
#include <dvbsiparser.h>
TS_POP_WARNING()
#include <uuids.h>
#include "tsComIds.h"
