#pragma once
#include "stdafx.h"
#include <foobar2000/helpers/foobar2000+atl.h>
#include <foobar2000/helpers/readers.h>
#include <libPPUI/CEditWithButtons.h>

#include <helpers/advconfig_impl.h>
#include <SDK/cfg_var.h>

namespace foo_window_swapper {

    enum Window {
        DEFAULT,
        ALT,
    };

    // The default values for configs. Resetting preferences page sets these values.
    struct Defaults {
        inline static const bool enabled = true;
        inline static pfc::string8 default_pattern = "ESLyric";
        inline static pfc::string8 alt_pattern = "%title% - %artist%[' ('%album%')']";
        inline static pfc::string8 alt_files = "mp4";
    };

    // Configs for window swapper. Change UUIDs if reusing code!
    struct Configs {
        // Set configs: UUID and default value needed
        inline static cfg_bool enabled = cfg_bool({ 0x6b70f7d5, 0xd379, 0x450a, { 0x86, 0xf3, 0x59, 0x8a, 0xa, 0xf0, 0xde, 0xbd } }, Defaults::enabled);
        inline static cfg_string default_pattern = cfg_string({ 0x2dee0f77, 0x32f1, 0x4218, { 0xa9, 0xaf, 0x2b, 0x4a, 0xad, 0xc4, 0xd3, 0x3e } }, Defaults::default_pattern);
        inline static cfg_string alt_pattern = cfg_string({ 0x835568bd, 0xe7cb, 0x47fb, { 0xbb, 0x7, 0x40, 0x91, 0x2f, 0xb0, 0x64, 0x2d } }, Defaults::alt_pattern);
        inline static cfg_string alt_files = cfg_string({ 0xc451763d, 0xe7b, 0x4555, { 0x86, 0x9b, 0xd2, 0xbd, 0x60, 0x5f, 0x1c, 0xe } }, Defaults::alt_files);
    };

    const GUID window_swapper_menu_group_guid = { 0x80934b3b, 0x9bd, 0x40cb, { 0x98, 0x73, 0x62, 0x81, 0x55, 0x1f, 0x2c, 0xbb } };
}
