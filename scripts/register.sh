#! /bin/sh
# usage: register.sh <runtime-path>

if [ "$#" -lt 1 ]; then
    echo "$0 <runtime-path>"
    exit 1
fi

if [ ! -f "$1" ]; then
    echo "--> error: Could not find runtime '$1'."
    exit 1
fi

RUNTIME_FILENAME=$(realpath "$1")

if [ "$(uname)" = "Darwin" ]; then
    MANIFEST_DIRECTORY="/usr/local/share/openxr/1"
elif [ "$(uname)" = "Linux" ]; then
    if [ -n "${XDG_CONFIG_HOME}" ]; then
        MANIFEST_DIRECTORY="${XDG_CONFIG_HOME}/openxr/1"
    elif [ -n "${HOME}" ]; then
        MANIFEST_DIRECTORY="${HOME}/.config/openxr/1"
    else
        echo "--> error: Neither XDG_CONFIG_HOME nor HOME are set. Could not figure out a place to put the manifest file."
        exit 1
    fi
fi

MANIFEST_FILENAME="${MANIFEST_DIRECTORY}/active_runtime.json"

echo " runtime to register: ${RUNTIME_FILENAME}"
echo "     openxr manifest: ${MANIFEST_FILENAME}"
echo

if [ ! -d "${MANIFEST_DIRECTORY}" ]; then
    echo "--> Create manifest directory '${MANIFEST_DIRECTORY}'."
    mkdir -p "${MANIFEST_DIRECTORY}"
fi

if [ -f "${MANIFEST_FILENAME}" ]; then
    echo "--> Backup manifest file '${MANIFEST_FILENAME}' to '${MANIFEST_FILENAME}.saved'."
    if ! cp "${MANIFEST_FILENAME}" "${MANIFEST_FILENAME}.saved"; then
        echo "--> error: Could not backup the manifest file. Try running the script with root privileges."
        exit 1
    fi
fi

cat << END_OF_FILE > "${MANIFEST_FILENAME}"
{
  "file_format_version": "1.0.0",
  "runtime": {
    "library_path": "${RUNTIME_FILENAME}"
  }
}
END_OF_FILE

echo "--> Successfully registered runtime '${RUNTIME_FILENAME}'."
