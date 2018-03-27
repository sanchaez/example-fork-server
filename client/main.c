#include <unistd.h>
#include <stdlib.h>
#include <argp.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

const char *argp_program_version = "latest";
const char *argp_program_bug_address = "<o.shaposhnikov@globallogic.com>";

/* Program documentation. */
static char doc[] =
    "A basic echo TCP client";

/* Arguments parser */
struct settings
{
    int   verbose;
    char *port;
    char *host;
    // TODO: extend this
};

#define DEFAULT_HOST "localhost"
#define DEFAULT_PORT "7000"
#define DATA_CAPACITY 100

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
    /* Get the input argument from argp_parse, which we
       know is a pointer to our arguments structure. */
    struct settings *settings = state->input;

    switch (key)
    {
    case 'v':
        settings->verbose = 1;
        break;

    case 'p':
        settings->port = arg;
        break;

    case 'h':
        settings->host = arg;
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp_option options[] = {
    {"verbose",  'v', 0,      0, "Produce verbose output" },
    {"port",     'p', "PORT", 0, "Override default port"  },
    {"host",     'h', "HOST", 0, "Specify host"           },
    { 0 },
};

static struct argp argp = { options, parse_opt, 0, doc };

/* Print helpers */

// define the VERBOSE_FLAG constant/variable/macro before using `trace()`
#define trace(...) do { if(VERBOSE_FLAG) printf(__VA_ARGS__); } while (0)
#define err(...) fprintf(stderr, __VA_ARGS__)

// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*) sa)->sin_addr);

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}


int main(int argc, char **argv)
{
    struct settings settings = { .verbose = 0,
                                 .port = DEFAULT_PORT,
                                 .host = DEFAULT_HOST, };
    struct addrinfo hints = { 0 };
    struct addrinfo *servinfo;
    int sockfd, numbytes;
    int rv;
    char s[INET6_ADDRSTRLEN];
    char buf[DATA_CAPACITY];
    //int VERBOSE_FLAG;

    argp_parse (&argp, argc, argv, 0, 0, &settings);

    //VERBOSE_FLAG = settings.verbose;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rv = getaddrinfo(settings.host, settings.port, &hints, &servinfo);
    if (rv != 0)
    {
        err("getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    struct addrinfo *p;
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("connect");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        err("Failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family,
              get_in_addr((struct sockaddr *)p->ai_addr),
              s, sizeof(s));

    printf("Connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    if ((numbytes = recv(sockfd, buf, DATA_CAPACITY-1, 0)) == -1)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    buf[numbytes] = '\0';

    printf("Received '%s'\n",buf);

    close(sockfd);

    return EXIT_SUCCESS;
}
