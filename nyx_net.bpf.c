#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <linux/pkt_cls.h>

// Shared Memory Map: The bridge between eBPF and the Wasm Engine
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u32);
} nyx_packet_map SEC(".maps");

SEC("tc")
int nyx_tc_ingress(struct __sk_buff *skb) {
    void *data_end = (void *)(long)skb->data_end;
    void *data = (void *)(long)skb->data;

    struct ethhdr *eth = data;
    if (data + sizeof(*eth) > data_end) return TC_ACT_OK;

    if (eth->h_proto != __constant_htons(ETH_P_IP)) return TC_ACT_OK;

    struct iphdr *ip = data + sizeof(*eth);
    if ((void *)(ip + 1) > data_end) return TC_ACT_OK;

    if (ip->protocol == IPPROTO_TCP) {
        struct tcphdr *tcp = (void *)ip + (ip->ihl * 4);
        if ((void *)(tcp + 1) > data_end) return TC_ACT_OK;

        // ONLY intercept HTTP traffic (Port 80)
        if (tcp->dest == __constant_htons(80) || tcp->source == __constant_htons(80)) {
            __u32 key = 0;
            // The value we pass is the total length of the packet
            __u32 val = skb->len; 
            
            // Write the packet size into the shared memory bridge
            bpf_map_update_elem(&nyx_packet_map, &key, &val, BPF_ANY);
            bpf_printk("[NYX-eBPF] Captured HTTP Packet! Size: %d bytes\n", val);
        }
    }

    return TC_ACT_OK;
}

char _license[] SEC("license") = "GPL";