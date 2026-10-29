#include <err.h>
#include <get_documents.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "clone_root.h"

#define SIZE_OF_STRING 512

// Auth thanks to get_subject.h, then make sure the root EPITA-TPs
// repository is available locally (cloning it if needed).
int main()
{
    int result = Auth();

    // Not signed in : nothing can be cloned, stop here
    if (result != EXIT_SUCCESS)
        return result;

    __CloneParentRepo();

    return result;
}