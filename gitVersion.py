# -*- coding: utf-8 -*-

"""PlatformIO script to generate a gitversion.h file and adds its path to
   CPPPATH.

Note: The PlatformIO documentation suggests to simply add the Git version as
dynamic build flag (=define). This however leads to constant rebuilds of the
complete project.
"""

import subprocess
from pathlib import Path

Import("env")  # pylint: disable=undefined-variable


OUTPUT_PATH = (
    Path(env.subst("$BUILD_DIR")) / "generated" / "gitrevision.h"
)  # pylint: disable=undefined-variable

TEMPLATE = """
#ifndef __GIT_REVISION_H__
    #define __GIT_REVISION_H__
    constexpr const char gitRevision[] = "Git-revision: {git_revision}";
    constexpr const char gitRevShort[] = "\\"{git_revision}\\"";
#endif
"""


def git_revision():
    """Returns Git revision or unknown."""
    try:
        return subprocess.check_output(
            ["git", "describe", "--always", "--dirty"],
            text=True,
            stderr=subprocess.PIPE,
        ).strip()
    except (subprocess.CalledProcessError, OSError) as err:
        print(
            f"  Warning: Setting Git revision to 'unknown': {err.stderr.split(':', 1)[1].strip()}"
        )
        return "unknown"


def generate():
    """Generates header file."""
    print("GENERATING GIT REVISION HEADER FILE")
    gitrev = git_revision()
    print(f'  "{gitrev}" -> {OUTPUT_PATH}')
    OUTPUT_PATH.parent.mkdir(exist_ok=True, parents=True)
    with OUTPUT_PATH.open("w") as output_file:
        output_file.write(TEMPLATE.format(git_revision=gitrev))


generate()
env.Append(CPPPATH=OUTPUT_PATH.parent)  # pylint: disable=undefined-variable
