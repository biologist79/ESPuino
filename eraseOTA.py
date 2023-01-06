# -*- coding: utf-8 -*-

"""
Erases the OTA partition before flashing, this ensures that the software flashed
via USB will be booted and not an older OTA version.
"""

import os
import sys
from pathlib import Path

Import("env")  # pylint: disable=undefined-variable


def before_upload(source, target, env):
    """Remove the OTA partition before uploading a new firmware"""

    print("Performing pre-upload erase of OTA partition...")

    idf_path = Path(env.PioPlatform().get_package_dir("framework-espidf"))
    os.environ["IDF_PATH"] = str(idf_path)
    otatool_path = idf_path / "components" / "app_update"
    parttool_path = idf_path / "components" / "partition_table"
    sys.path.extend([str(otatool_path), str(parttool_path)])

    from otatool import (  # pylint: disable=import-error,import-outside-toplevel
        OtatoolTarget,
    )

    ota_target = OtatoolTarget(env.subst("$UPLOAD_PORT"))
    ota_target.erase_otadata()


env.AddPreAction("upload", before_upload)  # pylint: disable=undefined-variable
