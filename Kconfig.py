Import("env")  # pylint: disable=undefined-variable

# Install Kconfiglib if not available
try:
    import kconfiglib
except ImportError:
    env.Execute("$PYTHONEXE -m pip install kconfiglib")
    import kconfiglib

def generate_configs(*args, **kwargs):
    #
    # Generate lines for config.h from .config by using code as in the
    # Kconfig.write_autoconf() function.
    #
    kconf = kconfiglib.Kconfig("Kconfig")
    kconf.load_config()
    filename = "config.h"
    chunks = kconf._autoconf_contents(None).splitlines()

    #
    # Save the manipulated config header lines to config.h again replicating the
    # behavior of Kconfig.write_autoconf()
    #
    if kconf._write_if_changed(filename, "\n".join(chunks)):
        print("Kconfig header saved to '{}'".format(filename))
    else:
        print("No change to Kconfig header in '{}'".format(filename))

# Always update config files before build
generate_configs()

env.AddCustomTarget("alldefconfig", None, ['alldefconfig', generate_configs])
env.AddCustomTarget("defconfig", None, ['defconfig', generate_configs])
env.AddCustomTarget("genconfig", None, [generate_configs])
env.AddCustomTarget("guiconfig", None, ['guiconfig', generate_configs])
env.AddCustomTarget("menuconfig", None, ['menuconfig', generate_configs])
env.AddCustomTarget("mrproper", None, 'rm -f .config config.h')
env.AddCustomTarget("savedefconfig", None, ['savedefconfig'])
