# Arduino's built-in LittleFS/SD libraries include FS.h without dependency metadata.
# Make that core header directory visible to their library compilation as well.
Import('env')
from pathlib import Path
framework=env.PioPlatform().get_package_dir('framework-arduinoespressif32')
env.Append(CPPPATH=[str(Path(framework)/'libraries/FS/src')])
