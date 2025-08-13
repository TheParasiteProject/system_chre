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

export CHRE_DEV_SCRIPT_PATH=`dirname "$(realpath "${BASH_SOURCE[0]}")"`

pushd $CHRE_DEV_SCRIPT_PATH > /dev/null
source ../common.sh
popd > /dev/null

chre_envs() {
  echo ""
  for env in "${CHRE_ENVS[@]}"; do
    printf "\033[32m%s\033[0m = %s\n" "$env" "${!env}"
  done
  echo ""
}

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
  done 3<<< "$commands"

  if [[ $? -ne 0 ]]; then
    onExit && return 1
  else
   echo ""
   echo "Environment variables set up for CHRE development:"
   chre_envs
   onSuccess
  fi
}


