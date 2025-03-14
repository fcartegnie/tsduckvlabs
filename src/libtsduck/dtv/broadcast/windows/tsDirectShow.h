//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2025, Thierry Lelegard
// BSD-2-Clause license, see LICENSE.txt file or https://tsduck.io/license
//
//----------------------------------------------------------------------------
//!
//!  @file
//!  @ingroup libtsduck windows
//!  Include the definitions for DirectShow (Windows media framework).
//!
//----------------------------------------------------------------------------

#pragma once
#include "tsPlatform.h"

#include "tsBeforeStandardHeaders.h"
#include <dshow.h>     // DirectShow (aka ActiveMovie)
#if !defined(__MINGW64_VERSION_MAJOR)
// these headers are not available in mingw-w64
#include <dshowasf.h>
#include <videoacc.h>
#include <bdatif.h>
#include <dsattrib.h>
#else
// available in mingw-w64 and included by the unavailable headers
#include <tuner.h>
#endif
#include <amstream.h>
#include <ks.h>
#include <ksproxy.h>
#include <ksmedia.h>
#include <bdatypes.h>  // BDA (Broadcast Device Architecture)
#include <bdamedia.h>
#include <bdaiface.h>
#include <dvbsiparser.h>
#include <mpeg2data.h>
#include <vidcap.h>
#include "tsAfterStandardHeaders.h"

// Required link libraries.
#if defined(TS_MSC)
    #pragma comment(lib, "quartz.lib")
#endif
