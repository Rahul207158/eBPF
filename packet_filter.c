#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <linux/bpf.h>
#include <linux/if_link.h>

// Structure to match the kernel-side stats
struct packet_stats
{
    __u64 total_packets;
    __u64 tcp_packets;
    __u64 dropped_packets;
    __u64 passed_packets;
};

// Global variables for cleanup
static int prog_fd = -1;
static int ifindex = -1;
static int port_map_fd = -1;
static int stats_map_fd = -1;

// Signal handler for graceful shutdown
static volatile sig_atomic_t keep_running = 1;

void signal_handler(int sig)
{
    keep_running = 0;
}

// Function to detach XDP program
void cleanup()
{
    if (prog_fd >= 0 && ifindex >= 0)
    {
        bpf_xdp_detach(ifindex, XDP_FLAGS_UPDATE_IF_NOEXIST, NULL);
        printf("XDP program detached from interface\n");
    }
    if (prog_fd >= 0)
    {
        close(prog_fd);
    }
    if (port_map_fd >= 0)
    {
        close(port_map_fd);
    }
    if (stats_map_fd >= 0)
    {
        close(stats_map_fd);
    }
}

// Function to print usage
void print_usage(const char *prog_name)
{
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("Options:\n");
    printf("  -i, --interface <name>    Network interface to attach to (required)\n");
    printf("  -p, --port <port>         Port to filter (default: 4040)\n");
    printf("  -s, --stats               Show packet statistics every 5 seconds\n");
    printf("  -h, --help                Show this help message\n");
    printf("\nExample:\n");
    printf("  %s -i eth0 -p 8080 -s\n", prog_name);
}

// Function to print packet statistics
void print_stats(int stats_fd)
{
    __u32 key = 0;
    struct packet_stats stats;

    if (bpf_map_lookup_elem(stats_fd, &key, &stats) != 0)
    {
        fprintf(stderr, "Failed to read statistics: %s\n", strerror(errno));
        return;
    }

    printf("\n=== Packet Statistics ===\n");
    printf("Total packets:   %llu\n", stats.total_packets);
    printf("TCP packets:     %llu\n", stats.tcp_packets);
    printf("Dropped packets: %llu\n", stats.dropped_packets);
    printf("Passed packets:  %llu\n", stats.passed_packets);

    if (stats.total_packets > 0)
    {
        double drop_rate = (double)stats.dropped_packets / stats.total_packets * 100;
        printf("Drop rate:       %.2f%%\n", drop_rate);
    }
    printf("========================\n");
}

int main(int argc, char **argv)
{
    char *interface = NULL;
    int port = 4040; // Default port
    int show_stats = 0;
    int opt;

    // Parse command line arguments
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interface") == 0)
        {
            if (i + 1 < argc)
            {
                interface = argv[++i];
            }
            else
            {
                fprintf(stderr, "Option %s requires an argument\n", argv[i]);
                print_usage(argv[0]);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0)
        {
            if (i + 1 < argc)
            {
                port = atoi(argv[++i]);
                if (port <= 0 || port > 65535)
                {
                    fprintf(stderr, "Invalid port number: %d\n", port);
                    return 1;
                }
            }
            else
            {
                fprintf(stderr, "Option %s requires an argument\n", argv[i]);
                print_usage(argv[0]);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--stats") == 0)
        {
            show_stats = 1;
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else
        {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!interface)
    {
        fprintf(stderr, "Network interface is required\n");
        print_usage(argv[0]);
        return 1;
    }

    // Get interface index
    ifindex = if_nametoindex(interface);
    if (ifindex == 0)
    {
        fprintf(stderr, "Invalid interface name: %s\n", interface);
        return 1;
    }

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Load BPF program
    struct bpf_object *obj;
    struct bpf_program *prog;

    obj = bpf_object__open("packet_filter.bpf.o");
    if (libbpf_get_error(obj))
    {
        fprintf(stderr, "Failed to open BPF object file: %s\n", strerror(errno));
        return 1;
    }

    if (bpf_object__load(obj))
    {
        fprintf(stderr, "Failed to load BPF object: %s\n", strerror(errno));
        bpf_object__close(obj);
        return 1;
    }

    // Find the program
    prog = bpf_object__find_program_by_name(obj, "xdp_packet_filter");
    if (!prog)
    {
        fprintf(stderr, "Failed to find XDP program\n");
        bpf_object__close(obj);
        return 1;
    }

    prog_fd = bpf_program__fd(prog);
    if (prog_fd < 0)
    {
        fprintf(stderr, "Failed to get program file descriptor\n");
        bpf_object__close(obj);
        return 1;
    }

    // Get map file descriptors
    struct bpf_map *port_map = bpf_object__find_map_by_name(obj, "port_map");
    struct bpf_map *stats_map = bpf_object__find_map_by_name(obj, "stats_map");

    if (!port_map || !stats_map)
    {
        fprintf(stderr, "Failed to find BPF maps\n");
        bpf_object__close(obj);
        return 1;
    }

    port_map_fd = bpf_map__fd(port_map);
    stats_map_fd = bpf_map__fd(stats_map);

    // Initialize statistics map
    __u32 stats_key = 0;
    struct packet_stats initial_stats = {0};
    if (bpf_map_update_elem(stats_map_fd, &stats_key, &initial_stats, BPF_ANY) != 0)
    {
        fprintf(stderr, "Failed to initialize statistics map: %s\n", strerror(errno));
        bpf_object__close(obj);
        return 1;
    }

    // Set the port to filter
    __u32 port_key = 0;
    __u16 target_port = (__u16)port;
    if (bpf_map_update_elem(port_map_fd, &port_key, &target_port, BPF_ANY) != 0)
    {
        fprintf(stderr, "Failed to set target port: %s\n", strerror(errno));
        bpf_object__close(obj);
        return 1;
    }

    // Attach XDP program to interface
    if (bpf_xdp_attach(ifindex, prog_fd, XDP_FLAGS_UPDATE_IF_NOEXIST, NULL) != 0)
    {
        fprintf(stderr, "Failed to attach XDP program to interface %s: %s\n",
                interface, strerror(errno));
        bpf_object__close(obj);
        return 1;
    }

    printf("XDP packet filter loaded successfully!\n");
    printf("Interface: %s\n", interface);
    printf("Filtering TCP packets on port: %d\n", port);
    printf("Press Ctrl+C to stop...\n");

    if (show_stats)
    {
        printf("Statistics will be shown every 5 seconds\n");
    }

    // Main loop
    int stats_counter = 0;
    while (keep_running)
    {
        sleep(1);

        if (show_stats)
        {
            stats_counter++;
            if (stats_counter >= 5)
            {
                print_stats(stats_map_fd);
                stats_counter = 0;
            }
        }
    }

    // Show final statistics
    if (show_stats)
    {
        printf("\nFinal statistics:\n");
        print_stats(stats_map_fd);
    }

    // Cleanup
    cleanup();
    bpf_object__close(obj);

    printf("Program terminated gracefully\n");
    return 0;
}