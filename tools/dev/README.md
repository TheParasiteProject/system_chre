# CHRE Development Tools

This directory contains tools and configuration files for setting up a development
environment for CHRE.

## Usage

TODO(b/374392644) - Add explanations about how to use these tools.\
TODO(b/374392644) - `chre_lunch` should support listing all supported platform-target combinations.

### Set up the environment

To initiate the CHRE development environment, follow these steps:

1. Run `source env_setup.sh` to initiate shell commands, which will enable commands like
   `chre_lunch`, etc.
2. Run `chre_lunch <platform-target>` to set up environment for a specific platform and target
   combination. For example, to start development of nanoapp on tinysys platform, run
   `chre_lunch tinysys-nanoapp`.

### Build a target

After setting up the environment for a specific platform and target combination, a CHRE target
(e.g., `nanoapp` or `libchre`) can be built by running `chre_make` from the folder where the target
Makefile is defined.

The command will also sign the binary and check if there are any external symbols that are
unresolvable.

#### Use `-C` option to generate `CMakeLists.txt` and `compile_commands.json`

When `-C` option is provided, `chre_make`can generate `CMakeLists.txt` and `compile_commands.json`.
An additional source path can be provided using `-s`, which fits for the project that the build
folder is separated from the src folder. For example:

```
chre_make -C -s ../../src/general_test
```

Note that `chre_make` will only generate above two files when `-C` is provided. This is because the
dryrun output of making the target is used extract source files, include directories, compiler
flags, and macro definitions. Therefore to generate the target binary please do not add `-C`.

### Commands available

TODO(b/374392644) - describe the shell commands available to use after the environment is set up.

- `chre_envs`: Prints all the environment variables set up for CHRE development.
- `chre_lunch <platform-target>`: Sets up the environment for specific platform and target
  combination.
- `chre_make [-C] [-s <src_path>]`: Builds the CHRE target.

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

## Python Scripts

TODO(b/374392644) - describe the purpose of the Python scripts in this directory and how to use them
separately.

### Internal

Scripts listed here are used internally by the shell commands. Using them directly is not supported.

- `env_setup.py`: A helper script used to set up the CHRE development environment. It is used
  internally by the shell script `env_setup.sh`.

### public

Scripts listed here are can also be used as a stand-alone tool to facilitate special needs.

- `cml_gen.py`: A script to generate `CMakeLists.txt` and `compile commands.json` files for the
  corresponding target.