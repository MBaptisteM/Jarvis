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
* [Installation](#installation)
* [Commands](#commands)

  * [Authentication Command](#authentication-command)
  * [Clone Command](#clone-command)
  * [Uninstall Command](#uninstall-command)
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
└── S2/
    └── B1/
        └── E1-Name-concept1-concept2/
```



## GitHub Integration

Jarvis can optionally create and manage a personal GitHub archive.

When enabled:

* A GitHub repository is automatically created.
* The repository becomes the root archive of all your practical work.
* Every cloned project can be synchronized automatically.
* Your entire academic history is backed up.

This repository allows you to have a Long-term archival and reuse your code whenever you want.

---

# Installation

> Documentation coming soon.



# Commands

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



## Clone Command

Clone and organize a repository.

### Usage

```bash
jarvis clone <remote-repository>
```

### Example

```bash
jarvis clone firstname.lastname@git.forge.epita.fr:p/example/repository.git
```

### What happens

(If it is an epita repository)
1. Repository is cloned.
2. Repository is placed in the correct workspace location.
3. Tree structure 
4. Given files and Suubject are downloaded.
5. Project structure is generated.
6. Repository is renamed.
7. GitHub archive is updated if enabled.



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

---


# License

Copyright (c) 2026 Baptiste Mahé. All rights reserved.

No copying, redistribution, resale, or commercial use permitted. Personal use only.
