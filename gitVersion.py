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
    constexpr const char gitBranch[] = "{git_branch}";
    constexpr const char softwareRevisionShort[] = "{software_revision}";
    constexpr const char softwareRevision[] = "Software-revision: {software_revision}";
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


def git_branch():
    """Returns the current Git branch name or unknown (e.g. detached HEAD in CI checkouts)."""
    try:
        branch = subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            text=True,
            stderr=subprocess.PIPE,
        ).strip()
        return branch if branch != "HEAD" else "unknown"
    except (subprocess.CalledProcessError, OSError):
        return "unknown"


def git_commit_date():
    """Returns the commit date (YYYYMMDD) of HEAD, or None if unavailable
       (e.g. no .git directory, such as in a GitHub zip download)."""
    try:
        return subprocess.check_output(
            ["git", "log", "-1", "--format=%cd", "--date=format:%Y%m%d"],
            text=True,
            stderr=subprocess.PIPE,
        ).strip()
    except (subprocess.CalledProcessError, OSError):
        return None


def generate():
    """Generates header file."""
    print("GENERATING GIT REVISION HEADER FILE")
    gitrev = git_revision()
    gitbranch = git_branch()
    commit_date = git_commit_date()
    swrev = f"{commit_date}-DEV" if commit_date else "unknown"
    print(f'  "{gitrev}" (branch "{gitbranch}", revision "{swrev}") -> {OUTPUT_PATH}')
    OUTPUT_PATH.parent.mkdir(exist_ok=True, parents=True)
    with OUTPUT_PATH.open("w") as output_file:
        output_file.write(
            TEMPLATE.format(git_revision=gitrev, git_branch=gitbranch, software_revision=swrev)
        )


generate()
env.Append(CPPPATH=OUTPUT_PATH.parent)  # pylint: disable=undefined-variable
