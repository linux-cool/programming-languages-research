/*
 * DistFS 哈希算法模块
 * 提供多种哈希算法和一致性哈希实现
 */

#include "distfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* CRC32 查找表 */
static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

/* 一致性哈希环节点 */
typedef struct hash_ring_node {
    uint32_t hash;
    char node_id[64];
    void *data;
    struct hash_ring_node *next;
} hash_ring_node_t;

/* 一致性哈希环 */
struct distfs_hash_ring {
    hash_ring_node_t *nodes;
    int virtual_nodes;
    int node_count;
    pthread_mutex_t mutex;
};

/* 初始化CRC32查找表 */
static void init_crc32_table(void) {
    if (crc32_table_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = true;
}

/* CRC32 哈希算法 */
uint32_t distfs_hash_crc32(const void *data, size_t len) {
    if (!data || len == 0) return 0;
    
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    const unsigned char *bytes = (const unsigned char *)data;
    
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

/* FNV-1a 哈希算法 */
uint32_t distfs_hash_fnv1a(const void *data, size_t len) {
    if (!data || len == 0) return 0;
    
    const uint32_t FNV_PRIME = 0x01000193;
    const uint32_t FNV_OFFSET_BASIS = 0x811c9dc5;
    
    uint32_t hash = FNV_OFFSET_BASIS;
    const unsigned char *bytes = (const unsigned char *)data;
    
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    
    return hash;
}

/* MurmurHash3 32位版本 */
uint32_t distfs_hash_murmur3(const void *data, size_t len, uint32_t seed) {
    if (!data || len == 0) return seed;
    
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    const uint32_t r1 = 15;
    const uint32_t r2 = 13;
    const uint32_t m = 5;
    const uint32_t n = 0xe6546b64;
    
    uint32_t hash = seed;
    const unsigned char *bytes = (const unsigned char *)data;
    const int nblocks = len / 4;
    
    // 处理4字节块
    for (int i = 0; i < nblocks; i++) {
        uint32_t k = *(uint32_t *)(bytes + i * 4);
        
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;
        
        hash ^= k;
        hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
    }
    
    // 处理剩余字节
    const unsigned char *tail = bytes + nblocks * 4;
    uint32_t k1 = 0;
    
    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
                k1 *= c1;
                k1 = (k1 << r1) | (k1 >> (32 - r1));
                k1 *= c2;
                hash ^= k1;
    }
    
    // 最终混合
    hash ^= len;
    hash ^= hash >> 16;
    hash *= 0x85ebca6b;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35;
    hash ^= hash >> 16;
    
    return hash;
}

/* 字符串哈希 */
uint32_t distfs_hash_string(const char *str) {
    if (!str) return 0;
    return distfs_hash_fnv1a(str, strlen(str));
}

/* 64位哈希 */
uint64_t distfs_hash64(const void *data, size_t len) {
    if (!data || len == 0) return 0;
    
    // 使用两个不同的32位哈希组合成64位
    uint32_t h1 = distfs_hash_murmur3(data, len, 0x12345678);
    uint32_t h2 = distfs_hash_murmur3(data, len, 0x87654321);
    
    return ((uint64_t)h1 << 32) | h2;
}

/* 创建一致性哈希环 */
distfs_hash_ring_t* distfs_hash_ring_create(int virtual_nodes) {
    distfs_hash_ring_t *ring = malloc(sizeof(distfs_hash_ring_t));
    if (!ring) return NULL;
    
    memset(ring, 0, sizeof(distfs_hash_ring_t));
    ring->virtual_nodes = virtual_nodes > 0 ? virtual_nodes : 150;
    
    if (pthread_mutex_init(&ring->mutex, NULL) != 0) {
        free(ring);
        return NULL;
    }
    
    return ring;
}

/* 销毁一致性哈希环 */
void distfs_hash_ring_destroy(distfs_hash_ring_t *ring) {
    if (!ring) return;
    
    pthread_mutex_lock(&ring->mutex);
    
    hash_ring_node_t *node = ring->nodes;
    while (node) {
        hash_ring_node_t *next = node->next;
        free(node);
        node = next;
    }
    
    pthread_mutex_unlock(&ring->mutex);
    pthread_mutex_destroy(&ring->mutex);
    free(ring);
}

/* 在哈希环中插入节点 */
static void insert_ring_node(distfs_hash_ring_t *ring, hash_ring_node_t *new_node) {
    if (!ring->nodes) {
        ring->nodes = new_node;
        new_node->next = new_node; // 环形链表
        return;
    }
    
    hash_ring_node_t *current = ring->nodes;
    hash_ring_node_t *prev = NULL;
    
    // 找到插入位置（按哈希值排序）
    do {
        if (new_node->hash < current->hash) {
            break;
        }
        prev = current;
        current = current->next;
    } while (current != ring->nodes);
    
    // 插入节点
    new_node->next = current;
    if (prev) {
        prev->next = new_node;
    } else {
        // 插入到头部，需要更新环的最后一个节点
        hash_ring_node_t *last = ring->nodes;
        while (last->next != ring->nodes) {
            last = last->next;
        }
        last->next = new_node;
        ring->nodes = new_node;
    }
}

/* 添加节点到哈希环 */
int distfs_hash_ring_add_node(distfs_hash_ring_t *ring, const char *node_id, void *data) {
    if (!ring || !node_id) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&ring->mutex);
    
    // 为每个物理节点创建多个虚拟节点
    for (int i = 0; i < ring->virtual_nodes; i++) {
        hash_ring_node_t *node = malloc(sizeof(hash_ring_node_t));
        if (!node) {
            pthread_mutex_unlock(&ring->mutex);
            return DISTFS_ERROR_NO_MEMORY;
        }
        
        // 生成虚拟节点ID
        char virtual_id[128];
        snprintf(virtual_id, sizeof(virtual_id), "%s:%d", node_id, i);
        
        node->hash = distfs_hash_string(virtual_id);
        strncpy(node->node_id, node_id, sizeof(node->node_id) - 1);
        node->node_id[sizeof(node->node_id) - 1] = '\0';
        node->data = data;
        node->next = NULL;
        
        insert_ring_node(ring, node);
    }
    
    ring->node_count++;
    
    pthread_mutex_unlock(&ring->mutex);
    return DISTFS_SUCCESS;
}

/* 从哈希环中移除节点 */
int distfs_hash_ring_remove_node(distfs_hash_ring_t *ring, const char *node_id) {
    if (!ring || !node_id) {
        return DISTFS_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&ring->mutex);
    
    if (!ring->nodes) {
        pthread_mutex_unlock(&ring->mutex);
        return DISTFS_ERROR_NOT_FOUND;
    }
    
    hash_ring_node_t *current = ring->nodes;
    hash_ring_node_t *prev = NULL;
    int removed_count = 0;
    
    // 找到环的最后一个节点
    hash_ring_node_t *last = ring->nodes;
    while (last->next != ring->nodes) {
        last = last->next;
    }
    
    do {
        hash_ring_node_t *next = current->next;
        
        if (strcmp(current->node_id, node_id) == 0) {
            // 移除节点
            if (current == ring->nodes) {
                if (current->next == current) {
                    // 只有一个节点
                    ring->nodes = NULL;
                } else {
                    ring->nodes = current->next;
                    last->next = current->next;
                }
            } else {
                prev->next = current->next;
                if (current == last) {
                    last = prev;
                }
            }
            
            free(current);
            removed_count++;
        } else {
            prev = current;
        }
        
        current = next;
    } while (current != ring->nodes && ring->nodes);
    
    if (removed_count > 0) {
        ring->node_count--;
    }
    
    pthread_mutex_unlock(&ring->mutex);
    
    return removed_count > 0 ? DISTFS_SUCCESS : DISTFS_ERROR_NOT_FOUND;
}

/* 在哈希环中查找节点 */
const char* distfs_hash_ring_get_node(distfs_hash_ring_t *ring, const void *key, size_t key_len) {
    if (!ring || !key || key_len == 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&ring->mutex);
    
    if (!ring->nodes) {
        pthread_mutex_unlock(&ring->mutex);
        return NULL;
    }
    
    uint32_t hash = distfs_hash_murmur3(key, key_len, 0);
    
    hash_ring_node_t *current = ring->nodes;
    hash_ring_node_t *selected = ring->nodes;
    
    // 找到第一个哈希值大于等于key哈希值的节点
    do {
        if (current->hash >= hash) {
            selected = current;
            break;
        }
        current = current->next;
    } while (current != ring->nodes);
    
    const char *result = selected->node_id;
    
    pthread_mutex_unlock(&ring->mutex);
    
    return result;
}

/* 获取哈希环中的多个节点（用于副本） */
int distfs_hash_ring_get_nodes(distfs_hash_ring_t *ring, const void *key, size_t key_len,
                               const char **nodes, int max_nodes) {
    if (!ring || !key || key_len == 0 || !nodes || max_nodes <= 0) {
        return 0;
    }
    
    pthread_mutex_lock(&ring->mutex);
    
    if (!ring->nodes || ring->node_count == 0) {
        pthread_mutex_unlock(&ring->mutex);
        return 0;
    }
    
    uint32_t hash = distfs_hash_murmur3(key, key_len, 0);
    
    hash_ring_node_t *current = ring->nodes;
    
    // 找到起始节点
    do {
        if (current->hash >= hash) {
            break;
        }
        current = current->next;
    } while (current != ring->nodes);
    
    int count = 0;
    char added_nodes[max_nodes][64];
    
    // 收集不重复的物理节点
    hash_ring_node_t *start = current;
    do {
        bool already_added = false;
        for (int i = 0; i < count; i++) {
            if (strcmp(added_nodes[i], current->node_id) == 0) {
                already_added = true;
                break;
            }
        }
        
        if (!already_added) {
            nodes[count] = current->node_id;
            strncpy(added_nodes[count], current->node_id, sizeof(added_nodes[count]) - 1);
            added_nodes[count][sizeof(added_nodes[count]) - 1] = '\0';
            count++;
            
            if (count >= max_nodes || count >= ring->node_count) {
                break;
            }
        }
        
        current = current->next;
    } while (current != start);
    
    pthread_mutex_unlock(&ring->mutex);
    
    return count;
}
