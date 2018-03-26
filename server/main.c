#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <argp.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

const char *argp_program_version = "latest";
const char *argp_program_bug_address = "<o.shaposhnikov@globallogic.com>";

/* Program documentation. */
static char doc[] =
    "A basic TCP server that uses fork() to manage multiple clients.";

/* Arguments parser */
struct settings
{
    int   verbose;
    char *port;
    // TODO: extend this
};

#define DEFAULT_PORT "7000"
#define BACKLOG 5

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

    case ARGP_KEY_ARG:
        /* TODO: accept port as an argument */
        // fallthrough
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp_option options[] = {
    {"verbose",  'v', 0,      0,  "Produce verbose output" },
    {"port",     'p', "PORT", 0,  "Override default port"  },
    { 0 },
};

static struct argp argp = { options, parse_opt, 0, doc };

/* Print helpers */

// define the VERBOSE_FLAG before using `trace()`
// #define VERBOSE_FLAG *value*
#define trace(...) do { if(VERBOSE_FLAG) printf(__VA_ARGS__); } while (0)
#define err(...) fprintf(stderr, __VA_ARGS__)

// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// get port, IPv4 or IPv6:
in_port_t get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return (((struct sockaddr_in*)sa)->sin_port);
    }

    return (((struct sockaddr_in6*)sa)->sin6_port);
}

static volatile sig_atomic_t done = 0;

/* SIGTERM handler */
void term(int signum)
{
    (void) signum;
    done = 1;
}

//TODO: refactor
int listen_connections(struct settings *settings)
{
#define VERBOSE_FLAG settings->verbose
    int status;
    int listener_fd;
    struct addrinfo hints = { 0 };
    struct addrinfo *servinfo;

    printf("Set up a connection\n");
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    trace("Get a list of possible adderesses on port %s\n", settings->port);
    status = getaddrinfo(NULL, settings->port, &hints, &servinfo);
    if (status)
    {
        err("getaddrinfo: %s\n", gai_strerror(status));
        trace("E: getaddrinfo failed!\n");
        return EXIT_FAILURE;
    }

    trace("Loop through the addresses and bind to the first valid one\n");
    struct addrinfo *p;
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        char ip[INET6_ADDRSTRLEN];
        in_port_t port;

        // convert client address and port for printing
        inet_ntop(p->ai_family,
                  get_in_addr(p->ai_addr),
                  ip, sizeof(ip));

        port = get_in_port(p->ai_addr);

        trace("Create socket: addr:%s, port:%d\n  protocol:0x%.4x, type:0x%.4x, flags:0x%.4x\n",
              ip,
              ntohs(port),
              p->ai_family,
              p->ai_socktype,
              p->ai_protocol);

        listener_fd = socket(p->ai_family,
                             p->ai_socktype,
                             p->ai_protocol);
        if (listener_fd == -1)
        {
            perror("socket");
            trace("E: failed to create socket!\n");
            continue;
        }

        trace("Try to bind a port to the socket\n");
        if (bind(listener_fd, p->ai_addr, p->ai_addrlen) != -1)
        {
            trace("Listener port bound\n");
            break;
        }

        perror("bind");
        trace("E: failed to bind!\n");
        close(listener_fd);
    }

    // cleanup
    freeaddrinfo(servinfo);

    if (p == NULL)
    {
        err("Could not bind\n");
        trace("C: failed to find suitable socket!\n");
        return EXIT_FAILURE;
    }

    trace("Set up listening on a socket\n");
    if(listen(listener_fd, BACKLOG) == -1)
    {
        perror("listen");
        trace("C: failed to listen on a socket!\n");
        return EXIT_FAILURE;
    }

    trace("Set up SIGCHLD handler\n");
    if(signal(SIGCHLD, SIG_IGN) == SIG_ERR)
    {
        perror("signal (SIGCHLD)");
        trace("C: failed to set a signal handler!\n");
        return EXIT_FAILURE;
    }

    printf("Waiting for connections...\n");

    while(!done) //... until SIGTERM
    {
        int new_connection_fd;
        char s[INET6_ADDRSTRLEN]; // IPv4 address will fit in this buffer too
        struct sockaddr_storage their_addr;
        socklen_t sin_size;

        trace("Wait and accept connection...\n");
        sin_size = sizeof their_addr;
        new_connection_fd = accept(listener_fd,
                                   (struct sockaddr *)&their_addr,
                                   &sin_size);
        if (new_connection_fd == -1)
        {
            perror("accept");
            trace("E: Failed to accept connection!");
            continue;
        }

        // convert client address for printing
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);

        printf("Got connection from %s\n", s);

        if (!fork())
        {
            /* child process */
            close(listener_fd);

            //TODO: actual communication
            if (send(new_connection_fd, "Hello, world!", 13, 0) == -1)
            {
                perror("send");
            }

            close(new_connection_fd);
            exit(EXIT_SUCCESS);
        }

        close(new_connection_fd);
    }

#undef VERBOSE_FLAG
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    struct settings settings = { .verbose = 0,
                                 .port = DEFAULT_PORT };

    // parse arguments
    argp_parse(&argp, argc, argv, 0, 0, &settings);

    // set sigterm handler
    if(signal(SIGTERM, &term) == SIG_ERR)
    {
        perror("signal (SIGTERM)");
        return EXIT_FAILURE;
    }

    // run main loop
    return listen_connections(&settings);
}
