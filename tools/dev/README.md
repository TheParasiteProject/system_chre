# CHRE Development Tools

This directory contains tools and configuration files for setting up a development
environment for CHRE.

## Usage

TODO(b/374392644) - Add explanations about how to use these tools.
TODO(b/374392644) - `chre_lunch` should support listing all supported platform-target combinations.

1. Run `source env_setup.sh` to initiate shell commands, which will enable commands like
   `chre_lunch`, etc.
2. Run `chre_lunch <platform-target>` to set up environment for a specific platform and target
   combination. For example, to start development of nanoapp on tinysys platform, run
   `chre_lunch tinysys-nanoapp`.

## Configuration

The configuration of the environment is done through the `env_config.json` file. It allows for
specifying different build targets, environment variables, and Python versions for various
hardware and software platforms.

### Top-Level Structure

The file consists of a JSON array, where each object in the array represents a single platform
configuration.

```json
[
  {
    "platform": "...",
    ...
  },
  {
    "platform": "...",
    ...
  }
]
```

### Platform Configuration Fields

Each platform configuration object has the following fields:

- `platform` (string, required): A unique name identifying the platform (e.g., "qsh",
  "tinysys").
- `python_version` (string, optional): The required Python version for development on this
  platform, if not specified, the default python version will be used.
- `common_env_variables` (array, optional): An array of objects where each defines a shell
  environment variables that are common to all targets within this platform.
- `targets` (array, required): An array of target-specific configuration objects.

### Target Configuration Fields

TODO(b/374392644) - Add explanations about the command to install the nanoapp for
`install_location`.

Each object within the `targets` array defines a specific build target and has the following
fields:

- `name` (string, required): A descriptive name for the target (e.g., "nanoapp", "shim").
- `build_target` (string, required): The identifier for the build system (e.g.,
  "aosp_hexagonv66_qsh").
- `install_location` (string, optional): The path where the output binary of the build will be
  installed on the connected physical device. If this is not specified, the output binary will
  not be installed.
- `env_variables` (array, optional): An array of objects where each defines a shell environment
  variables that are specific to this target.

### Environment Variable Fields

The environment variable objects, found in both `common_env_variables` and a target's
`env_variables`, are *exported* shell variables defined in the current shell and will exist in
spawned child shell sessions. Each environment variable object has the following structure:

- `name` (string, required): The name of the environment variable (e.g., "ANDROID_BUILD_TOP").
- `description` (string, required): A brief description of the variable's purpose and the expected
  value.
- `type` (string, required): The type of the variable, which can be:
    - `path`: Represents a file system path.
    - `value`: A simple string value. At this moment only `-` and characters from `[a-zA-Z0-9_]` are
      allowed.
    - `file`: Represents a path to a file.
- `default` (string, optional): The default value for the environment variable. This can be a
  static string or can reference other environment variables using the `$` syntax (e.g.,
  `$ANDROID_BUILD_TOP/external/nanopb-c`). Note that although using an environment variable to
  define another is supported, the variable on the right side must be defined prior to the variable
  on the left side. `common_env_variables` fields are parsed before target-specific `env_variables`
  fields.
- `default_action` (array, optional): An array of strings defining an action to be executed
  to obtain the default value if it's not already set. The first element is the action name,
  followed by its parameters.

### Actions

TODO(b/374392644) - add a link to the corresponding python script.

Actions are compound operations that are not easy to fulfil with simple shell commands. Predefined
actions listed below are supported. Behind the scene they are executed though dedicated python
functions.

- `action_clone_repo`: Clones a git repository. The parameters are:
    - Repository URL (string, required): The URL of the git repository to clone.
    - Branch (string, required): The branch to clone.
    - Destination Path (string, required): The path where the repository will be cloned. For
      example,
      ```json
      "default_action": [
        "action_clone_repo",
        "https://a-link.to.your.qsh.branch",
        "$QSH_BRANCH",
        "/tmp/qsh-$QSH_BRANCH"
      ]
      ```
      the above action will clone the qsh repository from the branch specified by the
      environment variable `QSH_BRANCH` into the directory `/tmp/qsh-$QSH_BRANCH`.

## Commands

TODO(b/374392644) - describe the shell commands available to use after the environment is set up.

- `chre_envs`: Prints all the environment variables set up for CHRE development.
- `chre_lunch <platform-target>`: Sets up the environment for specific platform and target
  combination.

## Python Scripts

TODO(b/374392644) - describe the purpose of the Python scripts in this directory and how to use them
separately.

`env_setup.py`: A helper script used to set up the CHRE development environment. Users should not
use this script directly. Instead, it is used internally by the shell script `env_setup.sh`.