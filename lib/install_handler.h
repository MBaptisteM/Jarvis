#ifndef INSTALL_HANDLER_H
#define INSTALL_HANDLER_H

#define INSTALL_METHOD_KEY "install_method"

// Install a package by trying every install method (nix, apt). The method
// saved in the info file is tried first. If a method works while the one that
// was expected (saved or deduced) did not, the working one is saved in the
// info file so that it is used first from now on.
// nix_pkg / apt_pkg : package names, binary : command that must exist after.
// Returns 0 when the binary is available.
int InstallPackage(const char *nix_pkg, const char *apt_pkg,
                   const char *binary);

#endif /* INSTALL_HANDLER_H */
