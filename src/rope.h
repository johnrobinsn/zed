// Rope data structure - AVL balanced tree of strings
// Provides O(log n) insert, delete, and lookup operations

#ifndef ZED_ROPE_H
#define ZED_ROPE_H

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>

// Rope node size: 256-512 bytes for small nodes (cache efficient)
constexpr size_t ROPE_NODE_CAPACITY = 512;

// Rope node
struct RopeNode {
    // Tree structure
    RopeNode* left;
    RopeNode* right;
    int height;          // For AVL balancing
    size_t weight;       // Number of characters in left subtree + this node if leaf

    // Leaf data
    bool is_leaf;
    char data[ROPE_NODE_CAPACITY];
    size_t length;       // Actual length of data (only for leaves)

    RopeNode() : left(nullptr), right(nullptr), height(1), weight(0),
                 is_leaf(true), length(0) {
        data[0] = '\0';
    }
};

// Rope structure
struct Rope {
    RopeNode* root;
    size_t total_length;
};

// Forward declarations
inline RopeNode* rope_node_create_leaf(const char* str, size_t len);
inline RopeNode* rope_node_create_internal(RopeNode* left, RopeNode* right);
inline void rope_node_free(RopeNode* node);
inline int rope_node_get_height(RopeNode* node);
inline size_t rope_node_get_weight(RopeNode* node);
inline void rope_node_update(RopeNode* node);
inline int rope_node_balance_factor(RopeNode* node);
inline RopeNode* rope_node_rotate_right(RopeNode* node);
inline RopeNode* rope_node_rotate_left(RopeNode* node);
inline RopeNode* rope_node_balance(RopeNode* node);
inline RopeNode* rope_node_insert(RopeNode* node, size_t pos, const char* str, size_t len);
inline RopeNode* rope_node_delete(RopeNode* node, size_t pos, size_t len);
inline size_t rope_node_copy(RopeNode* node, size_t pos, char* buffer, size_t len);

// Create a leaf node
inline RopeNode* rope_node_create_leaf(const char* str, size_t len) {
    RopeNode* node = new RopeNode();
    node->is_leaf = true;
    node->length = std::min(len, ROPE_NODE_CAPACITY);
    memcpy(node->data, str, node->length);
    node->data[node->length] = '\0';
    node->weight = node->length;
    return node;
}

// Create an internal node
inline RopeNode* rope_node_create_internal(RopeNode* left, RopeNode* right) {
    RopeNode* node = new RopeNode();
    node->is_leaf = false;
    node->left = left;
    node->right = right;
    rope_node_update(node);
    return node;
}

// Free a rope node and its children
inline void rope_node_free(RopeNode* node) {
    if (!node) return;

    if (!node->is_leaf) {
        rope_node_free(node->left);
        rope_node_free(node->right);
    }

    delete node;
}

// Get height of node (0 for null)
inline int rope_node_get_height(RopeNode* node) {
    return node ? node->height : 0;
}

// Get weight of node
inline size_t rope_node_get_weight(RopeNode* node) {
    if (!node) return 0;

    if (node->is_leaf) {
        return node->length;
    } else {
        return rope_node_get_weight(node->left) + rope_node_get_weight(node->right);
    }
}

// Update node metadata (height and weight)
inline void rope_node_update(RopeNode* node) {
    if (!node) return;

    node->height = 1 + std::max(rope_node_get_height(node->left),
                                 rope_node_get_height(node->right));

    if (node->is_leaf) {
        node->weight = node->length;
    } else {
        node->weight = rope_node_get_weight(node->left);
    }
}

// Get balance factor for AVL balancing
inline int rope_node_balance_factor(RopeNode* node) {
    return node ? rope_node_get_height(node->left) - rope_node_get_height(node->right) : 0;
}

// AVL rotation: right
inline RopeNode* rope_node_rotate_right(RopeNode* y) {
    RopeNode* x = y->left;
    RopeNode* T2 = x->right;

    x->right = y;
    y->left = T2;

    rope_node_update(y);
    rope_node_update(x);

    return x;
}

// AVL rotation: left
inline RopeNode* rope_node_rotate_left(RopeNode* x) {
    RopeNode* y = x->right;
    RopeNode* T2 = y->left;

    y->left = x;
    x->right = T2;

    rope_node_update(x);
    rope_node_update(y);

    return y;
}

// Balance node using AVL rotations
inline RopeNode* rope_node_balance(RopeNode* node) {
    if (!node) return nullptr;

    rope_node_update(node);

    int balance = rope_node_balance_factor(node);

    // Left-heavy
    if (balance > 1) {
        if (rope_node_balance_factor(node->left) < 0) {
            node->left = rope_node_rotate_left(node->left);
        }
        return rope_node_rotate_right(node);
    }

    // Right-heavy
    if (balance < -1) {
        if (rope_node_balance_factor(node->right) > 0) {
            node->right = rope_node_rotate_right(node->right);
        }
        return rope_node_rotate_left(node);
    }

    return node;
}

// Insert text at position
inline RopeNode* rope_node_insert(RopeNode* node, size_t pos, const char* str, size_t len) {
    if (!node) {
        // Inserting into empty rope - handle large strings by splitting into chunks
        if (len <= ROPE_NODE_CAPACITY) {
            return rope_node_create_leaf(str, len);
        } else {
            // Split large string into balanced tree of leaves
            size_t mid = len / 2;
            // Round to nearest chunk boundary for better balance
            mid = (mid / ROPE_NODE_CAPACITY) * ROPE_NODE_CAPACITY;
            if (mid == 0) mid = ROPE_NODE_CAPACITY;

            RopeNode* left = rope_node_insert(nullptr, 0, str, mid);
            RopeNode* right = rope_node_insert(nullptr, 0, str + mid, len - mid);
            return rope_node_create_internal(left, right);
        }
    }

    if (node->is_leaf) {
        // Leaf node: split if necessary
        if (node->length + len <= ROPE_NODE_CAPACITY) {
            // Fits in current node
            memmove(node->data + pos + len, node->data + pos, node->length - pos);
            memcpy(node->data + pos, str, len);
            node->length += len;
            node->data[node->length] = '\0';
            rope_node_update(node);
            return node;
        } else {
            // Need to split
            RopeNode* left_node = rope_node_create_leaf(node->data, pos);
            RopeNode* right_node = rope_node_create_leaf(node->data + pos, node->length - pos);

            RopeNode* new_text_node = rope_node_create_leaf(str, len);

            // Build tree: (left, new_text) + right
            RopeNode* left_tree = rope_node_create_internal(left_node, new_text_node);
            RopeNode* result = rope_node_create_internal(left_tree, right_node);

            delete node;
            return rope_node_balance(result);
        }
    }

    // Internal node: recurse
    if (pos <= node->weight) {
        node->left = rope_node_insert(node->left, pos, str, len);
    } else {
        node->right = rope_node_insert(node->right, pos - node->weight, str, len);
    }

    return rope_node_balance(node);
}

// Delete text at position
inline RopeNode* rope_node_delete(RopeNode* node, size_t pos, size_t len) {
    if (!node || len == 0) return node;

    if (node->is_leaf) {
        // Leaf node: delete from data
        if (pos >= node->length) return node;

        size_t actual_len = std::min(len, node->length - pos);
        memmove(node->data + pos, node->data + pos + actual_len,
                node->length - pos - actual_len);
        node->length -= actual_len;
        node->data[node->length] = '\0';

        rope_node_update(node);

        // If empty, delete node
        if (node->length == 0) {
            delete node;
            return nullptr;
        }

        return node;
    }

    // Internal node: recurse
    size_t left_weight = rope_node_get_weight(node->left);

    if (pos < left_weight) {
        // Delete starts in left subtree
        size_t left_delete_len = std::min(len, left_weight - pos);
        node->left = rope_node_delete(node->left, pos, left_delete_len);

        if (len > left_delete_len) {
            // Continue deleting in right subtree
            node->right = rope_node_delete(node->right, 0, len - left_delete_len);
        }
    } else {
        // Delete starts in right subtree
        node->right = rope_node_delete(node->right, pos - left_weight, len);
    }

    // If one child is null, collapse
    if (!node->left && !node->right) {
        delete node;
        return nullptr;
    } else if (!node->left) {
        RopeNode* temp = node->right;
        delete node;
        return temp;
    } else if (!node->right) {
        RopeNode* temp = node->left;
        delete node;
        return temp;
    }

    return rope_node_balance(node);
}

// Copy text from rope to buffer
inline size_t rope_node_copy(RopeNode* node, size_t pos, char* buffer, size_t len) {
    if (!node || len == 0) return 0;

    if (node->is_leaf) {
        if (pos >= node->length) return 0;

        size_t copy_len = std::min(len, node->length - pos);
        memcpy(buffer, node->data + pos, copy_len);
        return copy_len;
    }

    // Internal node
    size_t left_weight = rope_node_get_weight(node->left);

    if (pos < left_weight) {
        size_t copied = rope_node_copy(node->left, pos, buffer, len);
        if (copied < len) {
            copied += rope_node_copy(node->right, 0, buffer + copied, len - copied);
        }
        return copied;
    } else {
        return rope_node_copy(node->right, pos - left_weight, buffer, len);
    }
}

// Initialize rope
inline void rope_init(Rope* rope) {
    rope->root = nullptr;
    rope->total_length = 0;
}

// Get total length
inline size_t rope_length(Rope* rope) {
    return rope->total_length;
}

// Insert text at position
inline void rope_insert(Rope* rope, size_t pos, const char* str, size_t len) {
    if (len == 0) return;

    rope->root = rope_node_insert(rope->root, pos, str, len);
    rope->total_length += len;
}

// Create rope from string (must come after rope_insert)
inline void rope_from_string(Rope* rope, const char* str) {
    rope->root = nullptr;
    rope->total_length = 0;

    size_t len = strlen(str);
    if (len == 0) return;

    // For large strings, insert in chunks to build a balanced tree
    // This avoids the ROPE_NODE_CAPACITY limit of 512 bytes
    if (len <= ROPE_NODE_CAPACITY) {
        // Small string - create single leaf
        rope->root = rope_node_create_leaf(str, len);
        rope->total_length = len;
    } else {
        // Large string - use rope_insert to build balanced tree
        rope_insert(rope, 0, str, len);
    }
}

// Delete text at position
inline void rope_delete(Rope* rope, size_t pos, size_t len) {
    if (len == 0) return;

    rope->root = rope_node_delete(rope->root, pos, len);
    rope->total_length -= std::min(len, rope->total_length - std::min(pos, rope->total_length));
}

// Copy substring to buffer
inline size_t rope_copy(Rope* rope, size_t pos, char* buffer, size_t len) {
    if (!rope->root) return 0;
    return rope_node_copy(rope->root, pos, buffer, len);
}

// Get character at position
inline char rope_char_at(Rope* rope, size_t pos) {
    char c = '\0';
    rope_copy(rope, pos, &c, 1);
    return c;
}

// Convert rope to string (allocates)
inline char* rope_to_string(Rope* rope) {
    char* str = new char[rope->total_length + 1];
    rope_copy(rope, 0, str, rope->total_length);
    str[rope->total_length] = '\0';
    return str;
}

// Free rope
inline void rope_free(Rope* rope) {
    rope_node_free(rope->root);
    rope->root = nullptr;
    rope->total_length = 0;
}

// Get substring from rope
inline void rope_substr(Rope* rope, size_t pos, size_t length, char* buffer) {
    if (pos >= rope_length(rope)) {
        buffer[0] = '\0';
        return;
    }

    // Adjust length if it goes past the end
    size_t rope_len = rope_length(rope);
    if (pos + length > rope_len) {
        length = rope_len - pos;
    }

    // Copy characters one by one
    for (size_t i = 0; i < length; i++) {
        buffer[i] = rope_char_at(rope, pos + i);
    }
    buffer[length] = '\0';
}

#endif // ZED_ROPE_H
