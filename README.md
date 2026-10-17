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
* [Installation](#installation)
* [Commands](#commands)
  * [Authentication Command](#authentication-command)
  * [Clone Command](#clone-command)
  * [Pull Command](#pull-command)
  * [Push Command](#push-command)
  * [Find Command](#find-command)
  * [Go Command](#go-command)
  * [Custom Command](#custom-command)
  * [Erase Command](#erase-command)
  * [Help Command](#help-command)
  * [Uninstall Command](#uninstall-command)
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

* Secure login workflow.
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
* Existing repositories are moved to their new location when you leave the menu.
* The root repository can be pushed right away.

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

1. A browser window is opened.
2. You authenticate on EPITA Forge.
3. Jarvis stores the session.
4. Future executions reuse the saved session.
5. Your root repository is located (or cloned if it doesn't exist yet).



## Clone Command

Clone and organize a repository.

### Usage

```bash
jarvis clone [all | current | <remote-repository>]
```

* No argument, or `all`: finds your root repository, or clones it if it doesn't exist yet.
* `current`: lists your Forge projects so you can pick the one to clone.
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



## Pull Command

Pull and update repositories, submodules included.

### Usage

```bash
jarvis pull [all | current | <repository>]
```

* No argument, or `all`: pulls the root repository and every repository of the tree.
* `current`: pulls the repository currently marked as active.
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
jarvis push [root | current | <path>]
```

* No argument: pushes the repository of the current folder.
* `current`: pushes the repository currently marked as active.
* `root`: pushes only the root repository.
* `<path>`: pushes the repository located at this path.

Changes are added and committed with an automatic, numbered name unless you
give one. The root repository is updated and pushed at the same time.

### Example

```bash
jarvis push
jarvis push root
```



## Find Command

Locate the local path of a repository without touching it.

### Usage

```bash
jarvis find [root | current | <repository> | <keyword>]
```

* No argument, or `root`: prints the path of the root repository.
* `current`: prints the path of the currently active repository.
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
jarvis go [root | current | <repository> | <keyword>]
```

If several folders match, a menu lets you choose one. Your shell is opened in
the chosen folder: type `exit` to come back.

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

Everything is chosen with the arrows and enter:

* **Layers**: enable or disable the Year, Semester and Bimester folders (default: Semester and Bimester).
* **Prefixes**: choose the text written before each number (default: `Y`, `S`, `B`).
* **Reset**: go back to the default organization.
* **Save**: applies the changes to your existing repositories (they are moved) and pushes the root repository.
* **Quit**: applies the changes to your existing repositories, without pushing.

Repositories in the `Others` folder are never moved.

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
| `pull`      | `pl`     |
| `push`      | `ps`     |
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