#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/in.h>

// Shared Memory Map: The bridge between XDP and the Wasm Engine
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u32);
} nyx_packet_map SEC(".maps");

// NEW: Dropping down to XDP SEC
SEC("xdp")
int nyx_xdp_ingress(struct xdp_md *ctx) {
    // We are now dealing with raw silicon memory buffers
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    struct ethhdr *eth = data;
    if (data + sizeof(*eth) > data_end) return XDP_PASS;

    if (eth->h_proto != __constant_htons(ETH_P_IP)) return XDP_PASS;

    struct iphdr *ip = data + sizeof(*eth);
    if ((void *)(ip + 1) > data_end) return XDP_PASS;

    if (ip->protocol == IPPROTO_TCP) {
        struct tcphdr *tcp = (void *)ip + (ip->ihl * 4);
        if ((void *)(tcp + 1) > data_end) return XDP_PASS;

        // ONLY intercept HTTP traffic (Port 80)
        if (tcp->dest == __constant_htons(80) || tcp->source == __constant_htons(80)) {
            __u32 key = 0;
            
            // NEW: Calculate payload size directly from physical memory bounds
            __u32 val = (__u32)(data_end - data); 
            
            // Write the packet size into the shared memory bridge
            bpf_map_update_elem(&nyx_packet_map, &key, &val, BPF_ANY);
            bpf_printk("[NYX-XDP] Silicon Drop! HTTP Packet Size: %d bytes\n", val);
        }
    }

    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";