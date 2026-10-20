# Jarvis

<p align="center">
  <img src="JarvisLogo.png" alt="Jarvis Logo" width="200">
</p>

<p align="center">
  <strong>Your personal EPITA repository assistant.</strong>
</p>

<p align="center">
  Open source tool that automate repository cloning, organization, synchronization and archival.
</p>

---

# Table of Contents

* [Overview](#overview)
* [Features](#features)
  * [Authentication](#authentication)
  * [Enhanced Clone](#enhanced-clone)
  * [Automatic Tree Structure Organization](#automatic-tree-structure-organization)
  * [GitHub Integration](#github-integration)
  * [Sync & Locate](#sync--locate)
  * [Customizable Organization](#customizable-organization)
  * [Code Formatting](#code-formatting)
* [Installation](#installation)
* [Commands](#commands)
  * [Authentication](#authentication-command)
  * [Clone](#clone-command)
  * [Fill](#fill-command)
  * [Pull](#pull-command)
  * [Push](#push-command)
  * [Format](#format-command)
  * [Find](#find-command)
  * [Go](#go-command)
  * [Custom](#custom-command)
  * [Erase](#erase-command)
  * [Help](#help-command)
  * [Uninstall](#uninstall-command)
  * [Shortcuts](#shortcuts)
* [License](#license)

---

# Overview

Jarvis is an open source command-line assistant written in C that automates the management of EPITA TPs repositories.

Instead of manually cloning repositories, organizing projects, recreating assignment structures and maintaining backups, Jarvis performs these tasks automatically.

It is an open source tool that helps EPITA students to :

* Find a specific TP.
* Remember what concepts were covered.
* Keep repositories organized.
* Maintain backups.
* Reconstruct assignment structures.

---

# Features

## Authentication

Jarvis can authenticate directly with your EPITA Forge account.

Features:

* Secure login workflow, in a browser window (default) or directly in the console.
* Session persistence.
* Cookie storage for future executions.
* Automatic reuse of saved sessions.
* Automatically locates or clones your root repository if needed.



## Enhanced Clone

Unlike a traditional `git clone`, Jarvis performs a complete repository analysis.

When cloning an EPITA repository, Jarvis can:

* Clone the repository.
* Detect the associated subject.
* Detect the semester.
* Detect the bimester.
* Download provided files.
* Analyze assignment specifications.
* Reconstruct the expected project structure.
* Organize the repository automatically.
* Synchronize changes when enabled.



## Automatic Tree Structure Organization

Jarvis maintains a structured workspace.

Repositories are automatically classified according to:

* Year (optional, see [Customizable Organization](#customizable-organization))
* Semester
* Bimester
* Main concepts covered

Example:

```text
EPITA-TPs/
├── S1/
│   ├── B1/
│   │   ├── E1-Name-concept1-concept2/
│   │   ├── P1-Name-concept1-concept2/
│   │   ├── P2-Name-concept1-concept2/
│   │   └── P3-Name-concept1-concept2/
│   │
│   └── B2/
│       ├── E1-Name-concept1-concept2/
│       └── P1-Name-concept1-concept2/
│
├── S2/
│   └── B1/
│       └── E1-Name-concept1-concept2/
│
└── Others/
    └── repository-whose-course-cannot-be-deduced/
```

With the Year layer enabled, the same repositories go one level deeper
(`EPITA-TPs/Y1/S1/B2/...`).



## GitHub Integration

Jarvis can optionally create and manage a personal GitHub archive.

When enabled:

* A GitHub repository is automatically created.
* The repository becomes the root archive of all your practical work.
* Every cloned project can be synchronized automatically.
* Your entire academic history is backed up.

This repository allows you to have a Long-term archival and reuse your code whenever you want.



## Sync & Locate

Keep every repository up to date and find any of them instantly.

* Pull one repository, or everything at once.
* Submodules are initialized and pulled recursively.
* A submodule whose pinned commit is no longer reachable is automatically resynced with its remote.
* Locate a repository by its title, its git remote, or by keyword.
* Jump straight into a repository with `jarvis go`.



## Customizable Organization

Choose how your workspace is laid out with `jarvis custom`.

* Enable or disable the Year, Semester and Bimester layers.
* Choose the prefix written before each number (`S`, `Semester`, ...).
* Rename the `Others` folder.
* Enable the automatic code formatting before each push and choose which file
  extensions are formatted (see [Code Formatting](#code-formatting)).
* Existing repositories are moved to their new location when you leave the menu.
* The root repository can be pushed right away.



## Code Formatting

Format your code with `clang-format` in one command, or automatically before every push.

* `jarvis format [<path>]` formats every supported file of the current folder (or of the given folder) and its sub-folders.
* The automatic format before `jarvis push` is disabled by default, enable it in `jarvis custom`.
* The formatted extensions can be edited (add / remove / restore the default list).
* A formatting problem is reported but never blocks a push.

---

# Installation

Install Jarvis directly from your terminal using the automated setup script.

### Usage

```bash
curl -fsSL https://raw.githubusercontent.com/MBaptisteM/Jarvis/main/setup.sh | bash
```

### What happens

1. A temporary workspace is created on your system.
2. The latest version of Jarvis is downloaded.
3. The setup script configures and installs the tool.
4. Temporary files are safely cleaned up.

For browser authentication, Jarvis uses the system Chromium package installed
with `nix profile add nixpkgs#chromium` on NixOS, because the generic Chromium
downloaded by Playwright is not directly executable there. On other Unix
systems, Jarvis continues to install Chromium through Playwright.

---

# Commands

Type `jarvis help` to list the commands, or `jarvis help <command>` for the
usage and examples of one of them.

## Authentication Command

Authenticate with your EPITA account.

### Usage

```bash
jarvis auth
```

### What happens

1. You choose how to authenticate:
   * **Browser** (default): a window is opened, you authenticate on EPITA Forge
     and close it.
   * **Console**: you type your username and password, and Jarvis signs in and
     gets the cookies by itself. The password is hidden, sent to the login
     script through its standard input, and never saved.
2. Jarvis stores the session.
3. Future executions reuse the saved session.
4. Your root repository is located (or cloned if it doesn't exist yet).

Without a terminal (script, pipe), the browser is used without asking.



## Clone Command

Clone and organize a repository.

### Usage

```bash
jarvis clone [all | current/c/cu/cur/curr | <remote-repository>]
```

* No argument, or `all`: finds your root repository, or clones it if it doesn't exist yet.
* `current` (or `c`, `cu`, `cur`, `curr`): lists your Forge projects so you
  can pick the one to clone.
* `<remote-repository>`: clones the specified repository.

### Example

```bash
jarvis clone firstname.lastname@git.forge.epita.fr:p/example/repository.git
```

### What happens

(If it is an epita repository)
1. Repository is cloned.
2. Repository is placed in the correct workspace location.
3. Tree structure is generated.
4. Given files and Subject are downloaded.
5. Project structure is generated.
6. Repository is renamed.
7. GitHub archive is updated if enabled.



## Fill Command

Clone many repositories of the forge at once.

### Usage

```bash
jarvis fill [old | all | current/now | <project>]
```

* No argument, `old`, `anc`, `ancient` or `past`: every finished repository.
* `all`: every repository, finished or not.
* `current` or `now`: only the repositories that are not finished.
* `<project>` (ex: `epita-prepa-computer-science`): every repository listed on
  the page of that project.

Each repository is cloned like `jarvis clone` does, without any question:
repositories already cloned are skipped, the content of a repository that is not
empty is never changed, and the root repository is pushed once at the end.

A repository whose course can't be deduced goes in `Others/<project>/`
(ex: `Others/epita-prepa-computer-science/`). A repository whose course is known
is placed by its semester/bimester, whatever its project.

### Example

```bash
jarvis fill
jarvis fill now
jarvis fill epita-prepa-computer-science
```



## Pull Command

Pull and update repositories, submodules included.

### Usage

```bash
jarvis pull [all | current/c/cu/cur/curr | <repository>]
```

* No argument, or `all`: pulls the root repository and every repository of the tree.
* `current` (or `c`, `cu`, `cur`, `curr`): pulls the repository currently
  marked as active.
* `<repository>`: pulls this repository directly if it has already been cloned.

Submodules are initialized, checked out and pulled recursively. A submodule
whose pinned commit is no longer reachable is automatically resynced with its
remote's default branch.

### Example

```bash
jarvis pull
jarvis pull current
```



## Push Command

Commit and push a repository together with the root repository.

### Usage

```bash
jarvis push [root | current/c/cu/cur/curr | <path>] [-n <name>] [-t <tag>] [-p <path>]
```

* No argument: pushes the repository of the current folder.
* `current` (or `c`, `cu`, `cur`, `curr`): pushes the repository currently
  marked as active.
* `root`: pushes only the root repository.
* `<path>`: pushes the repository located at this path.

Options:

| Option                 | Description                                                              |
|------------------------|--------------------------------------------------------------------------|
| `-n`, `--name <name>` | Name of the commit (an automatic, numbered name is generated otherwise, or the name of the tag if there is one). |
| `-t`, `--tag <name>`    | Creates and pushes a tag named `<name>-<number>` (ex: `v1` gives `v1-0`, then `v1-1`...). |
| `-p`, `--path <path>`   | Same as `<path>`; `root` and `current` also work as values.              |

`--name=<name>`, `--tag=<name>` and `--path=<path>` work too, and `--name` is
an alias of `--name`. An unknown option stops the command with an error. The older
`name=<name>` and `tag=<name>` arguments are still accepted.

The repository is pushed first, then the root repository, which records the new
state of the repository. Running `jarvis push` from the root repository pushes
only the root repository.

If the automatic format is enabled (see [Custom](#custom-command)), the
repository is formatted before it is committed. The root repository alone
(`jarvis push root`, or a push from the root repository) is never formatted,
since it would reformat every repository of the tree.

### Reliable reporting

* A problem (a failed `git add`, a rejected push, an unreachable remote...) is
  reported but does not stop the other steps: the root repository is pushed even
  if the repository push failed.
* "Pushed" is displayed only when commits were really sent to the remote.
  Otherwise Jarvis tells you that there is nothing to push, or why the push
  failed (and suggests `jarvis pull` when the remote has new commits).
* A nested repository without any commit is temporarily hidden while staging
  (git would otherwise refuse the whole `git add`) and restored right after, so
  it is not recorded in the root repository until it has a commit.
* When there is nothing to push, Jarvis lists what git does not see if you
  added something by hand: empty folders (git does not track them, add a file
  such as `.gitkeep`) and files ignored by `.gitignore`.
* The exit code is not zero when something failed, and a summary of what was
  pushed and what was not is printed.
* The tag is created after the commit, so it points to the pushed commit.

### Example

```bash
jarvis push
jarvis push root
jarvis push -n "Fix bug" -t v1 -p ~/EPITA-TPs/S1/B2/my_repository
jarvis push --name="Fix bug" --path current
```



## Format Command

Format the code of the current folder with `clang-format`.

### Usage

```bash
jarvis format [<path> | -p <path>]
```

* No argument: formats the current folder.
* `<path>` (or `-p`, `--path <path>`): formats this folder instead. It must be an
  existing folder.

Runs `clang-format -i` on every file of the folder and all its
sub-folders having one of the configured extensions. `clang-format` must be
installed (otherwise an error says so). The `.clang-format` file closest to each
file is used, and the extensions are compared without taking the case into
account.

Default extensions: `.cpp`, `.cc`, `.cxx`, `.c++`, `.cp`, `.hpp`, `.hh`, `.hxx`,
`.h++`, `.inline.hpp`, `.inl`, `.tpp`, `.txx`, `.c`, `.h`, `.cu`, `.cuh`, `.ixx`,
`.cppm`, `.mxx`, `.ccm`, `.js`, `.mjs`, `.cjs`, `.ts`, `.tsx`, `.cs`, `.java`,
`.json`, `.ipynb`, `.m`, `.mm`, `.proto`, `.protodevel`, `.textproto`, `.textpb`,
`.txtpb`, `.pb.txt`, `.asciipb`, `.td`, `.v`, `.vh`, `.sv`, `.svh`.

`.git` and `node_modules` folders are skipped (and the temporary
`.git.jarvis-tmp` ones). A file that can't be formatted is
reported and does not stop the others.

The extensions, and the automatic run before each `jarvis push`, are set in
`jarvis custom` > **Format**.

### Example

```bash
jarvis format
jarvis format ../EPITA-TPs
```



## Find Command

Locate the local path of a repository without touching it.

### Usage

```bash
jarvis find [root | current/c/cu/cur/curr | <repository> | <keyword>]
```

* No argument, or `root`: prints the path of the root repository.
* `current` (or `c`, `cu`, `cur`, `curr`): prints the path of the currently
  active repository.
* A known git remote: prints its local path, re-locating it automatically if it moved.
* Any other word: searches every repository title under the root repository and prints the matching paths.

### Example

```bash
jarvis find current
jarvis find "trees"
```



## Go Command

Move into a repository. It works like `find`, but takes you there.

### Usage

```bash
jarvis go [root | current/c/cu/cur/curr | <repository> | <keyword>]
```

Use `current`, `c`, `cu`, `cur` or `curr` to go to the currently active
repository.

If several folders match, a menu lets you choose one. `jarvis go` changes the
current shell's directory, so `cd -` returns to the previous directory. This
shell integration is enabled for Bash, Zsh, and Fish by the installer and takes
effect in new shells.

### Example

```bash
jarvis go current
jarvis go "trees"
```



## Custom Command

Customize the tree structure of your repositories.

### Usage

```bash
jarvis custom
```

Menus are chosen with the arrows and enter (prefixes, names and extensions are
typed):

* **Layers**: enable or disable the Year, Semester and Bimester folders (default: Semester and Bimester).
* **Prefixes**: choose the text written before each number (default: `Y`, `S`, `B`).
* **Others**: rename the folder used when the course can't be deduced (default: `Others`).
* **Format**: enable or disable the automatic `jarvis format` before each push
  (default: disabled), and edit the formatted extensions:
  * **add**: type one or several extensions separated by commas. Case and spaces
    are ignored and the leading dot is optional (`C++, .Json,  pb.txt` becomes
    `.c++`, `.json`, `.pb.txt`). Invalid entries are reported and ignored.
  * **remove**: type the extensions to remove, with the same rules.
  * **show the list** and **restore the default ones**.
* **Reset**: go back to the default organization.
* **Save**: applies the changes to your existing repositories (they are moved) and pushes the root repository.
* **Quit**: applies the changes to your existing repositories, without pushing.

The settings are stored in the `.jarvis_config` file of the root repository, so
once pushed they are shared by every Jarvis using it. Format settings are saved
immediately; use **Save** to push them.

Repositories in the `Others` folder are never moved (unless the folder is renamed).

### Example

With the Year layer enabled, `EPITA-TPs/S1/B2/<repository>` becomes
`EPITA-TPs/Y1/S1/B2/<repository>`.



## Erase Command

Back up the root repository, delete its contents except `.git`, and push the
cleaned repository.

### Usage

```bash
jarvis erase
```

The command asks for confirmation before deleting anything. The backup push
replaces remote-only commits. Ignored files are not included in the backup.
If needed, the command prints the Git command to restore the backed-up files.



## Help Command

List the commands, or show the usage of one of them.

### Usage

```bash
jarvis help [command]
```

`jarvis <command> --help` works too.



## Uninstall Command

Remove Jarvis from the system.

### Usage

```bash
jarvis uninstall
```

### Confirmation

```text
Are you sure you want to uninstall Jarvis? [y/N]
```



## Shortcuts

| Command     | Shortcut |
|-------------|----------|
| `auth`      | `a`      |
| `clone`     | `c`      |
| `fill`      | `fl`     |
| `pull`      | `pl`     |
| `push`      | `ps`     |
| `format`    | `fm`     |
| `find`      | `f`      |
| `go`        | `g`      |
| `help`      | `h`      |

`erase`, `custom` and `uninstall` have no shortcut (`erase` and `uninstall` are
destructive on purpose).

`j` also works instead of `jarvis` (ex: `j c`, `j g`, `j pl`).

---

# License

Copyright (c) 2026 Baptiste Mahé. All rights reserved.

No copying, redistribution, resale, or commercial use permitted. Personal use only.