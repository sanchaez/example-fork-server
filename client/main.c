#include <unistd.h>
#include <stdlib.h>
#include <argp.h>

const char *argp_program_version = "latest";
const char *argp_program_bug_address = "<o.shaposhnikov@globallogic.com>";

/* Program documentation. */
static char doc[] =
    "A basic echo TCP client";

static struct argp argp = { 0, 0, 0, doc };

int main(int argc, char **argv)
{
    argp_parse (&argp, argc, argv, 0, 0, 0);
    return 0;
}
