#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

// Map to store the port number to filter
struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u16);
} port_map SEC(".maps");

// Map to store packet statistics
struct packet_stats
{
    __u64 total_packets;
    __u64 tcp_packets;
    __u64 dropped_packets;
    __u64 passed_packets;
};

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, struct packet_stats);
} stats_map SEC(".maps");

SEC("xdp")
int xdp_packet_filter(struct xdp_md *ctx)
{
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    // Initialize statistics
    __u32 stats_key = 0;
    struct packet_stats *stats = bpf_map_lookup_elem(&stats_map, &stats_key);
    if (!stats)
    {
        return XDP_ABORTED;
    }

    // Increment total packet count
    __sync_fetch_and_add(&stats->total_packets, 1);

    // Parse Ethernet header
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
    {
        return XDP_PASS;
    }

    // Check if it's an IPv4 packet
    if (eth->h_proto != bpf_htons(ETH_P_IP))
    {
        __sync_fetch_and_add(&stats->passed_packets, 1);
        return XDP_PASS;
    }

    // Parse IP header
    struct iphdr *ip = (struct iphdr *)(eth + 1);
    if ((void *)(ip + 1) > data_end)
    {
        return XDP_PASS;
    }

    // Check if it's a TCP packet
    if (ip->protocol != IPPROTO_TCP)
    {
        __sync_fetch_and_add(&stats->passed_packets, 1);
        return XDP_PASS;
    }

    // Increment TCP packet count
    __sync_fetch_and_add(&stats->tcp_packets, 1);

    // Parse TCP header
    struct tcphdr *tcp = (struct tcphdr *)((unsigned char *)ip + (ip->ihl * 4));
    if ((void *)(tcp + 1) > data_end)
    {
        return XDP_PASS;
    }

    // Get the configured port from the map
    __u32 port_key = 0;
    __u16 *target_port = bpf_map_lookup_elem(&port_map, &port_key);
    if (!target_port)
    {
        // If no port is configured, pass all packets
        __sync_fetch_and_add(&stats->passed_packets, 1);
        return XDP_PASS;
    }

    // Check if the destination port matches our target port
    __u16 dest_port = bpf_ntohs(tcp->dest);
    if (dest_port == *target_port)
    {
        // Drop the packet
        __sync_fetch_and_add(&stats->dropped_packets, 1);
        bpf_printk("Dropping TCP packet to port %u", dest_port);
        return XDP_DROP;
    }

    // Check if the source port matches our target port
    __u16 src_port = bpf_ntohs(tcp->source);
    if (src_port == *target_port)
    {
        // Drop the packet
        __sync_fetch_and_add(&stats->dropped_packets, 1);
        bpf_printk("Dropping TCP packet from port %u", src_port);
        return XDP_DROP;
    }

    // Pass the packet if it doesn't match our filter
    __sync_fetch_and_add(&stats->passed_packets, 1);
    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";