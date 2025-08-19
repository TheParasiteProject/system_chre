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

"""A script to set up the CHRE development environment.

This script should only be incurred by a shell script and can be run in two modes:

1. Interactive environment setup:
   When run with a platform-target combination (e.g., `python env_setup.py -p <platform>-<target>`),
   it interactively prompts the user for necessary environment variables based on
   the env_config.json file. It then generates a series of 'export' commands
   that can be executed by the shell to configure the environment, along with necessary action
   function calls that can be incurred later (see below).

   The result is printed to stdout and piped into the shell to set the environment variables.

2. Standalone actions:
   When run with the --action flag (e.g., `python env_setup.py --action action_clone_repo ...`),
   it performs specific, one-off tasks based on the action name provided.
"""
import argparse
import json
import os
import re
import subprocess
import sys
from typing import Any

from shell_util import log_i, fatal_error


class _CustomArgumentParser(argparse.ArgumentParser):
  """A custom argument parser to override the default error handling."""

  def error(self, message):
    """Overrides the default error method to prevent printing errors to console"""
    fatal_error(
      f"an argument in the format of <platform_name-target_name> must be provided.\n{message}"
    )


def _get_input_from_shell(prompt: str) -> str:
  """Prompts the user for input from the shell and returns the response.

  Args:
    prompt: The prompt message to display to the user.

  Returns:
    The string entered by the user.
  """
  print(prompt, end="", file=sys.stderr, flush=True)
  return input()


def _action_clone_repo(url: str, branch: str, dest: str) -> None:
  """Clones a Git repository from 'url' with a specific 'branch' into 'dest'.

  Args:
      url (str): The URL of the Git repository.
      branch (str): The branch to clone.
      dest (str): The destination directory to clone the repository into.
  """
  if os.path.exists(dest):
    answer = _get_input_from_shell(
      f"{dest} already exists. Shall we override it? (y/N):")
    if not answer or answer.lower() == 'n':
      log_i(f"Skipping clone operation for {dest}")
      return
    log_i(f"Removing existing directory: {dest}")
    subprocess.run(['rm', '-rf', dest], check=True)

  try:
    # The --single-branch option fetches only the specified branch, saving time and space.
    subprocess.run(
      ['git', 'clone', '--branch', branch, '--single-branch', url, dest],
      check=True
    )
    log_i(f"Successfully cloned {url} (branch: {branch}) into {dest}")
  except subprocess.CalledProcessError:
    fatal_error('Error cloning repository')
  except FileNotFoundError:
    fatal_error(
      "Error: 'git' command not found. Please ensure Git is installed and in your PATH.")


def _assert_and_expand_env_variable(env_name, env_type: str, env_value: str):
  """Expands and validates an environment variable, then sets it.

  This function performs the following steps:
  1. Expands shell variables (like ~ and $HOME) in the provided value.
  2. Validates the expanded value based on its type.
  3. Updates os.environ with the validated environment variable.

  Args:
    env_name: The name of the environment variable.
    env_type: The type of the variable for validation.
    env_value: The value of the environment variable.

  Returns:
    The expanded value of the environment variable.
  """

  def _check_single_value(value, v_type):
    _expanded_value = os.path.expanduser(os.path.expandvars(value))
    if v_type == "path":
      if not os.path.isdir(_expanded_value):
        fatal_error(f"Path '{value}' does not exist.")
    elif v_type == "file":
      if not os.path.isfile(_expanded_value):
        fatal_error(f"File '{value}' does not exist.")
    elif v_type == "value":
      if not re.match(r"^[\w\-]+$", value, flags=re.ASCII):
        fatal_error(
          f"Invalid value '{value}'. Only dash and characters in [a-zA-Z0-9_] are allowed.")
    else:
      fatal_error(f"Unknown value type '{v_type}' for value '{value}'")
    return _expanded_value

  matched_list_type = re.match(r"^list\[(.*)]$", env_type)
  if matched_list_type:
    expanded_value = ":".join(
      _check_single_value(v, matched_list_type.group(1)) for v in env_value.split())
  else:
    expanded_value = _check_single_value(env_value, env_type)
  os.environ[env_name] = expanded_value
  return expanded_value


def _run_action(action_and_args):
  try:
    func = getattr(sys.modules[__name__], "_" + action_and_args[0])
    expanded_args = [os.path.expanduser(os.path.expandvars(arg)) for arg in action_and_args[1:]]
    func(*expanded_args)
  except AttributeError:
    fatal_error(f"Unknown action: '{action_and_args[0]}'")


def _load_config(config_file: str = None) -> Any:
  """Loads and parses the configuration file.

  Args:
    config_file: The file containing the config json file. When it's not provided the path to the
    directory containing this script must be set in the CHRE_DEV_SCRIPT_PATH environment variable
    so that the default config file can be retrieved.

  Returns:
    A dictionary containing the parsed JSON configuration data.
  """

  if config_file is None:
    if not os.getenv("CHRE_DEV_SCRIPT_PATH"):
      fatal_error("CHRE_DEV_SCRIPT_PATH must be set before calling this script")
    config_file = os.path.join(os.getenv("CHRE_DEV_SCRIPT_PATH"), "env_config.json")
  else:
    config_file = os.path.expanduser(os.path.expandvars(config_file))

  try:
    with open(config_file, "r") as f:
      return json.load(f)
  except FileNotFoundError:
    fatal_error(f"Error: Config file '{config_file}' not found")
  except json.JSONDecodeError as e:
    fatal_error(f"Error: Invalid JSON format in '{config_file}'\n{e}")


def _parse_platform_and_target_configs(
    config_data: Any, platform_and_target: str
):
  """Parses config data to find commands and env vars for a platform-target.

  Args:
    config_data: The parsed JSON configuration data.
    platform_and_target: A string in the format "<platform_name>-<target_name>".

  Returns:
    A tuple containing:
      - A dictionary of predefined environment variables.
      - A list of environment variable definitions to be processed further.
  """
  supported_combinations = [
    f"{entry.get('platform')}-{target.get('name')}"
    for entry in config_data
    for target in entry.get("targets", [])
  ]

  if not platform_and_target or not re.match(r"^\w+-\w+$", platform_and_target):
    fatal_error(
      "platform and target must be in the format of <platform_name-target_name>\n"
      f"Supported choices are: {supported_combinations}"
    )

  platform_name, target_name = platform_and_target.split("-")
  for platform in config_data:
    try:
      if platform["platform"] != platform_name:
        continue
      for target in platform["targets"]:
        if target["name"] != target_name:
          continue
        env_map = {"CHRE_PLATFORM": platform_name,
                   "CHRE_TARGET_TYPE": target_name,
                   "CHRE_BUILD_TARGET": target["build_target"]
                   }
        if platform.get("python_version"):
          env_map["CHRE_PYTHON_VERSION"] = platform.get("python_version")
        if target.get("install_location"):
          env_map["TARGET_INSTALL_LOCATION"] = target["install_location"]
        envs = platform.get("common_env_variables", []) + target.get(
          "env_variables", [])
        return env_map, envs
    except KeyError as e:
      fatal_error(
        f"Malformed config for {platform_name}-{target_name}: '{e.args[0]}' field is not defined")

  fatal_error(
    f"No platform-target combination found for '{platform_name}-{target_name}'\n"
    f"Supported choices are: {supported_combinations}"
  )


def _parse_env_variable_fields(env_vars, predefined_envs):
  """Interactively prompts for and processes environment variables.

  Iterates through a list of environment variable definitions, prompts the user
  for values, validates them, and generates the necessary 'export' commands. If a
  default value is provided and no user input is given, the default value will
  be used. Adds default action to the commands list too if it is specified.

  Args:
    predefined_envs: A dict of environment variables pre-defined for the platform
     and the target.
    env_vars: A list of dictionaries, where each dictionary defines an
      environment variable customizable by the user.

  Returns:
    A list of shell commands (strings) to set the environment variables.
  """
  all_env_names = set(predefined_envs)
  commands = [f"export {k}={v}" for k, v in predefined_envs.items()]
  for env_var in env_vars:
    try:
      if env_var["name"] in all_env_names:
        fatal_error(f"Duplicate env variable name: {env_var["name"]}")
      all_env_names.add(f"{env_var["name"]}")
      default_value = env_var.get("default")
      default_action = env_var.get("default_action", [])
      prompt = f"{env_var["name"]}"
      if default_value is not None:
        prompt = "{} ({})".format(env_var["name"], default_value if default_value else "EMPTY")

      print(f"\n{env_var["description"]}", file=sys.stderr)
      user_entered_value = _get_input_from_shell(f"{prompt}: ").strip()

      if user_entered_value:
        expanded_value = _assert_and_expand_env_variable(env_var["name"], env_var["type"],
                                                         user_entered_value)
        commands.append(f"export {env_var["name"]}={expanded_value}")
        # User entered a value, skip the default action
        continue

      # Either user has to enter a value or default_value must be provided
      if default_value is None:
        fatal_error(f"{env_var["name"]} must be provided. Please try it again")

      if default_action:
        _run_action(default_action)
      # Default action is supposed to have made the default value valid
      expanded_value = _assert_and_expand_env_variable(env_var["name"], env_var["type"],
                                                       default_value)
      commands.append(f"export {env_var["name"]}={expanded_value}")
    except KeyError as e:
      fatal_error(f"The environment variable doesn't have the field '{e.args[0]}'")

  # Create CHRE_ENVS to record all the env variables to be created
  commands.append("export CHRE_ENVS=(" + ' '.join(sorted(all_env_names)) + ")")
  return commands


def main():
  """Parses command-line arguments and orchestrates the script's execution."""
  arg_parser = _CustomArgumentParser(
    description="CHRE development environment setup",
  )
  arg_parser.add_argument(
    "platform_and_target", default=None,
    type=str
  )
  arg_parser.add_argument(
    "-c", "--config", type=str
  )

  args = arg_parser.parse_args()
  config_data = _load_config(args.config)
  fixed_env_map, target_envs_configs = _parse_platform_and_target_configs(config_data,
                                                                          args.platform_and_target)
  commands = _parse_env_variable_fields(target_envs_configs, fixed_env_map)

  print("\n".join(commands))
  return


if __name__ == "__main__":
  main()
