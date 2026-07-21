# -*- coding: utf-8 -*-

"""PlatformIO script to generate a platforminfo.h file and adds its path to
   CPPPATH.

Exposes the PlatformIO environment name (e.g. "complete",
"lolin_d32_pro_sdmmc_pe") to the firmware at compile time, so the web
interface can match the running build against the per-platform folders of
https://github.com/biologist79/ESPuino-Firmware. Also records whether a
settings-override.h or platformio-override.ini was used for this build: such
a build no longer matches the official prebuilt firmware for its platform
(e.g. custom pin mappings), so the web interface must not offer it as a
drop-in replacement.

Note: see gitVersion.py for why this is a generated header instead of a
dynamic build flag (avoids forcing a full rebuild on every build).
"""

from pathlib import Path

Import("env")  # pylint: disable=undefined-variable


OUTPUT_PATH = (
    Path(env.subst("$BUILD_DIR")) / "generated" / "platforminfo.h"
)  # pylint: disable=undefined-variable

PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))  # pylint: disable=undefined-variable

TEMPLATE = """
#ifndef __PLATFORM_INFO_H__
    #define __PLATFORM_INFO_H__
    constexpr const char espuinoPlatform[] = "{platform}";
    constexpr bool espuinoCustomBuild = {custom_build};
#endif
"""


def is_custom_build():
    """Returns True if this build used settings-override.h or platformio-override.ini."""
    return (PROJECT_DIR / "src" / "settings-override.h").is_file() or (
        PROJECT_DIR / "platformio-override.ini"
    ).is_file()


def generate():
    """Generates header file."""
    print("GENERATING PLATFORM INFO HEADER FILE")
    platform = env.subst("$PIOENV")  # pylint: disable=undefined-variable
    custom_build = is_custom_build()
    print(f'  platform="{platform}" customBuild={custom_build} -> {OUTPUT_PATH}')
    OUTPUT_PATH.parent.mkdir(exist_ok=True, parents=True)
    with OUTPUT_PATH.open("w") as output_file:
        output_file.write(
            TEMPLATE.format(
                platform=platform,
                custom_build="true" if custom_build else "false",
            )
        )


generate()
env.Append(CPPPATH=[str(OUTPUT_PATH.parent)])  # pylint: disable=undefined-variable
