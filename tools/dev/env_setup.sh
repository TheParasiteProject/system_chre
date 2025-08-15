#!/bin/bash
#
# Copyright 2025, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# This script provides a set of shell functions to configure and manage the
# CHRE (Context Hub Runtime Environment) development environment. It should be
# sourced by the user's shell to make the functions available in their
# terminal session.
#
# The main entry point is the `chre_lunch` function, which interactively sets
# up environment variables by calling the companion `env_setup.py` script.
# It also provides helper functions for building (`chre_make`), flashing
# (`chre_flash`), and inspecting the environment (`chre_envs`).
#
export CHRE_DEV_SCRIPT_PATH=`dirname "$(realpath "${BASH_SOURCE[0]}")"`

pushd $CHRE_DEV_SCRIPT_PATH > /dev/null
source ../common.sh
popd > /dev/null

#######################################
# Sets up pyenv to use the required Python version.
# Globals:
#   CHRE_PYTHON_VERSION
# Arguments:
#   None
#######################################
_setup_pyenv() {
    # Check for pyenv
    if ! command -v pyenv &> /dev/null; then
        echo "pyenv is not installed. Please install pyenv and try again."
        return 1
    fi

    # Install Python version if not already present.
    # sudo is used because it's possible that the user doesn't have permission to modify
    # /usr/share/pyenv/versions. Once the python version is installed sudo permission is not needed.
    if ! sudo pyenv versions | grep -q "$CHRE_PYTHON_VERSION"; then
        echo "Python version $CHRE_PYTHON_VERSION not found. Installing..."
        sudo pyenv install "$CHRE_PYTHON_VERSION"
    fi

    # Make sure the correct python version is used to set up virtual environment
    sudo pyenv local $CHRE_PYTHON_VERSION
    export PATH=$HOME/.pyenv/shims:$PATH
}

#######################################
# Installs required Python packages from requirements files.
# Globals:
#   CHRE_DEV_SCRIPT_PATH
# Arguments:
#   None
#######################################
_install_py_pkgs() {
  local general_requirements="${CHRE_DEV_SCRIPT_PATH}/requirements.txt"
  local protobuf_requirements="${CHRE_DEV_SCRIPT_PATH}/requirements_protobuf.txt"
  echo -e "\nThe following python packages would be installed if they are not yet.\n"
  cat $general_requirements
  echo ""
  cat $protobuf_requirements
  echo -ne "\nInstalling python packages..."
  pip install -r "$general_requirements" >/dev/null 2>&1 && \
  pip install -r "$protobuf_requirements" >/dev/null 2>&1
  if [[ $? -ne 0 ]]; then
    echo -e "\nFailed to install python packages."
    return 1
  else
    echo -e "Done\n"
    pip list
  fi
}

#######################################
# Sets up a Python virtual environment for CHRE development.
#
# This function checks if a virtual environment is active. If not, it prompts
# the user to create one. It handles using `pyenv` to switch to the correct
# Python version if necessary before creating the virtual environment and
# installing packages.
# Globals:
#   CHRE_PYTHON_VERSION
#   CHRE_DEV_SCRIPT_PATH
#   VIRTUAL_ENV
# Arguments:
#   None
#######################################
_setup_python_virtual_env() {
  local current_py_version=$(python --version | awk '{print $2}')
  if [[ -n "$CHRE_PYTHON_VERSION" && "$current_py_version" != "$CHRE_PYTHON_VERSION" ]]; then
    local need_a_different_py_version=true
  fi

  echo "Preparing to install python packages..."
  if [[ -z "$VIRTUAL_ENV" ]]; then
    echo "The installation of python packages must be done in a virtual environment."
    echo -n "Shall we create one for you? [Y/n] "
    read user_response
    if [[ "$user_response" == "y" || "$user_response" == "Y" || "$user_response" == "" ]]; then
      if [[ "$need_a_different_py_version" == "true" ]]; then
        echo "setting up pyenv..."
        _setup_pyenv || return 1
      fi
      # Create and activate the virtual environment
      local chre_venv_path="/tmp/chre_venv"
      if [[ -d "$chre_venv_path" ]]; then
        echo "'${chre_venv_path}' exists. Removing it..."
        rm -rf "$chre_venv_path"
      fi
      echo "Creating a new virtual environment '${chre_venv_path}' with Python $CHRE_PYTHON_VERSION..."
      python -m venv "$chre_venv_path" || return 1
      echo "Activating virtual environment..."
      source ${chre_venv_path}/bin/activate
    else
      echo -e "\nPlease create a virtual environment and try again."
      return 1
    fi
  else
    echo "It looks like you are already in a virtual environment ${VIRTUAL_ENV}. Great!"
    if [[ "$need_a_different_py_version" == "true" ]]; then
      echo "The virtual environment has python version $current_py_version but $CHRE_PYTHON_VERSION is required."
      echo "Please deactivate the current virtual environment and try again."
      return 1
    fi
  fi

  _install_py_pkgs || return 1
}

#######################################
# Builds a CHRE target (nanoapp or shim).
#
# This function is a wrapper around `make`. After a successful build, it
# locates the output `.so` file, signs it according to the platform, and
# runs a symbol check for nanoapps.
# It also supports a special `-C` argument to generate `compile_commands.json`
# for IDE support without performing a full build.
# Globals:
#   CHRE_BUILD_TARGET, CHRE_DEV_SCRIPT_PATH, CHRE_PLATFORM,
#   CHRE_TARGET_TYPE, HEXAGON_SDK_PREFIX, TEST_SIGN_KEY
# Arguments:
#   -C: Cleans and generates compile_commands.json.
#######################################
chre_make() {
  # When '-C' is given the actual target won't be made because the dryrun output is used to generate
  # CMakeLists.txt and compile_commands.json.
  if [[ $1 == "-C" ]]; then
    shift
    make clean && \
    python3 $CHRE_DEV_SCRIPT_PATH/cml_gen.py -c "make -n $CHRE_BUILD_TARGET" -o out "$@" && \
    mkdir out/build && \
    pushd out/build > /dev/null
    if [[ $? -eq 0 ]]; then
      cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../ && \
      mv compile_commands.json ../ && \
      rm -rf out/build
    fi
    popd > /dev/null
    return
  fi

  make $CHRE_BUILD_TARGET || return 1

  local so_files=(./out/$CHRE_BUILD_TARGET/*.so)
  if [[ ${#so_files[@]} -eq 0 ]]; then
    echo "Error: No .so file found in ./out/$CHRE_BUILD_TARGET/" >&2
    return 1
  elif [[ ${#so_files[@]} -gt 1 ]]; then
    echo "Error: Multiple .so files found. Not sure which one to sign:" >&2
    printf " - %s\n" "${so_files[@]}" >&2
    return 1
  fi
  local so_file="${so_files[0]}"
  signed_path="./out/$CHRE_BUILD_TARGET/signed"

  if [[ ! -f "$so_file" ]]; then
    return 1
  fi

  if [[ ! -d $signed_path ]]; then
    mkdir -p $signed_path
  fi

  # Signing
  if [[ $CHRE_PLATFORM == "qsh" ]]; then
    python3 "$HEXAGON_SDK_PREFIX/tools/elfsigner/elfsigner.py" --no_disclaimer -i "$so_file" -o "$signed_path"
  elif [[ $CHRE_PLATFORM == "tinysys" ]]; then
    if [[ "$CHRE_TARGET_TYPE" == "nanoapp" ]]; then
      python3 "$CHRE_DEV_SCRIPT_PATH/tinysys_nanoapp_signer.py" "$TEST_SIGN_KEY" "$so_file" "$signed_path"
    fi
  else
    echo "Unsupported platform '$CHRE_PLATFORM' for signing"
  fi

  # Checking external symbols
  if [[ $CHRE_TARGET_TYPE == "nanoapp" ]]; then
    python3 "$CHRE_DEV_SCRIPT_PATH/check_nanoapp_symbols.py" --nanoapp "$so_file"
  fi
}

#######################################
# Builds and flashes a CHRE target to a device.
#
# This function first calls `chre_make` to build the target and then invokes
# the `flash.py` script to perform the flashing operation.
# Arguments:
#   All arguments are passed directly to the flash.py script.
#######################################
chre_flash() {
  chre_make || return 1
  python3 "$CHRE_DEV_SCRIPT_PATH/flash.py" "$@"
}

#######################################
# Prints all currently configured CHRE environment variables.
# Globals:
#   CHRE_ENVS (array of environment variable names)
# Arguments:
#   None
#######################################
chre_envs() {
  echo ""
  for env in "${CHRE_ENVS[@]}"; do
    printf "\033[32m%s\033[0m = %s\n" "$env" "${!env}"
  done
  echo ""
}

#######################################
# The main entry point for setting up the CHRE development environment.
#
# This function invokes `env_setup.py` to interactively prompt the user for
# configuration values. It then processes the output, which consists of
# `export` commands and `action_*` calls, to configure the shell session.
# Finally, it sets up the Python virtual environment.
# Arguments:
#   A platform-target string, e.g., "qsh-nanoapp".
#######################################
chre_lunch() {
  echo ""
  echo "============ Setting up environment for CHRE development ============"
  echo "NOTE: A default value is provided in the parenthesis when possible"
  echo ""

  commands=`python3 $CHRE_DEV_SCRIPT_PATH/env_setup.py -p "$@"` && \
  echo "exporting environment variables..." && \
  while read -r -u 3 command; do  # Read from fd 3 to save stdin for user input
    if [[ "$command" == "action_"* ]]; then
      python3 $CHRE_DEV_SCRIPT_PATH/env_setup.py --action $command
    elif [[ "$command" == "export "* ]]; then
      # Using eval to make spaces and parenthesis in the array definition work
      eval $command
    else
      echo "Unknown command '$command'"
      onExit && return 1
    fi

    if [[ $? -ne 0 ]]; then
      onExit && return 1
    fi
  done 3<<< "$commands" && \
  _setup_python_virtual_env

  if [[ $? -ne 0 ]]; then
    onExit && return 1
  else
   echo ""
   echo "Environment variables set up for CHRE development:"
   chre_envs
   onSuccess
  fi
}
