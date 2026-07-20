# See https://docs.platformio.org/en/latest/manifests/library-json/fields/build/extrascript.html
Import("env")
from os.path import join, realpath

# Base srcFilter. Cannot be set in library.json.
src_filter = [
    "+<resources>",
    "+<locale>",
    "+<source>",
]


def define_name(item):
    """PlatformIO CPPDEFINES entries are either 'NAME' or ('NAME', value)."""
    if isinstance(item, (list, tuple)):
        return item[0] if item else None
    return item


for item in env.get("CPPDEFINES", []):
    name = define_name(item)
    if not isinstance(name, str):
        continue

    # Add generated view directory to include path depending on VIEW_* macro
    if name.startswith("VIEW_"):
        view = f"ui_{name[5:]}".lower()  # Ex value: "ui_320x240"
        env.Append(CPPPATH=[realpath(join("generated", view))])
        src_filter.append(f"+<generated/{view}>")
    # Add portduino directory to include path depending on ARCH_PORTDUINO macro
    elif name == "ARCH_PORTDUINO":
        env.Append(CPPPATH=[realpath("portduino")])
        src_filter.append("+<portduino>")

# Only `Replace` is supported for SRC_FILTER, not `Append` or `Prepend`
env.Replace(SRC_FILTER=src_filter)

# Dump construction environment (for debug purposes)
# print("meshtastic-device-ui Library ENV:")
# print(env.Dump())
