#ifndef ROPE_HXX
#define ROPE_HXX

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

enum RopeResult { ROPE_OK = 0, ROPE_ERR = 1 };

struct RopeHead {
    std::vector<uint8_t> data;
};

inline RopeHead *rope_new() { return new RopeHead(); }

inline void rope_free(RopeHead *r) { delete r; }

inline RopeResult rope_insert(RopeHead *r, size_t byte_pos,
                               const uint8_t *bytes, size_t len) {
    if (!r || !bytes || len == 0) return ROPE_OK;
    size_t start = byte_pos < r->data.size() ? byte_pos : r->data.size();
    r->data.insert(r->data.begin() + (ptrdiff_t)start, bytes, bytes + len);
    return ROPE_OK;
}

inline void rope_del(RopeHead *r, size_t byte_pos, size_t len) {
    if (!r || len == 0) return;
    size_t start = byte_pos < r->data.size() ? byte_pos : r->data.size();
    size_t end   = (start + len) < r->data.size() ? (start + len) : r->data.size();
    if (start < end) {
        r->data.erase(r->data.begin() + (ptrdiff_t)start,
                      r->data.begin() + (ptrdiff_t)end);
    }
}

inline size_t rope_len(const RopeHead *r) { return r ? r->data.size() : 0; }

inline size_t rope_write(const RopeHead *r, uint8_t *dest) {
    if (!r || !dest || r->data.empty()) return 0;
    std::memcpy(dest, r->data.data(), r->data.size());
    return r->data.size();
}

inline void rope_foreach(const RopeHead *r,
                          const std::function<void(const uint8_t *, size_t)> &cb) {
    if (!r || !cb || r->data.empty()) return;
    // Chunk size matches the old Rust implementation's callback granularity.
    static constexpr size_t kChunk = 160;
    for (size_t off = 0; off < r->data.size(); off += kChunk) {
        size_t len = std::min(kChunk, r->data.size() - off);
        cb(r->data.data() + off, len);
    }
}

class RopeBuffer {
public:
    RopeBuffer() : r_(rope_new()) {}
    explicit RopeBuffer(const std::string &s) : r_(rope_new()) { assign(s); }
    ~RopeBuffer() { if (r_) rope_free(r_); }

    RopeBuffer(const RopeBuffer &) = delete;
    RopeBuffer &operator=(const RopeBuffer &) = delete;
    RopeBuffer(RopeBuffer &&o) noexcept : r_(o.r_) { o.r_ = nullptr; }
    RopeBuffer &operator=(RopeBuffer &&o) noexcept {
        if (this != &o) {
            if (r_) rope_free(r_);
            r_ = o.r_;
            o.r_ = nullptr;
        }
        return *this;
    }

    void assign(const std::string &s) {
        rope_free(r_);
        r_ = rope_new();
        if (!s.empty()) {
            rope_insert(r_, 0, reinterpret_cast<const uint8_t *>(s.data()), s.size());
        }
    }
    RopeBuffer &operator=(const std::string &s) { assign(s); return *this; }

    operator std::string() const { return to_string(); }

    size_t size() const { return r_ ? rope_len(r_) : 0; }
    bool empty() const { return size() == 0; }
    void clear() {
        rope_free(r_);
        r_ = rope_new();
    }

    char operator[](size_t pos) const;

    void insert(size_t pos, const char *text, size_t len) {
        if (!text || !len) return;
        rope_insert(r_, pos, reinterpret_cast<const uint8_t *>(text), len);
    }
    void insert(size_t pos, const std::string &s) {
        insert(pos, s.data(), s.size());
    }
    void insert(size_t pos, char ch) {
        insert(pos, &ch, 1);
    }

    void erase(size_t pos, size_t len = std::string::npos) {
        if (len == std::string::npos) len = size() - pos;
        rope_del(r_, pos, len);
    }

    std::string to_string() const {
        size_t nb = size();
        if (nb == 0) return {};
        std::string s(nb, '\0');
        rope_write(r_, reinterpret_cast<uint8_t *>(s.data()));
        return s;
    }

    std::string substr(size_t pos, size_t len = std::string::npos) const {
        std::string full = to_string();
        return full.substr(pos, len);
    }

    void for_each_chunk(const std::function<void(const char *, size_t)> &cb) const {
        if (!cb) return;
        rope_foreach(r_, [&](const uint8_t *d, size_t l) {
            cb(reinterpret_cast<const char *>(d), l);
        });
    }

private:
    RopeHead *r_ = nullptr;
};

inline char RopeBuffer::operator[](size_t target) const {
    if (!r_ || target >= size()) return '\0';
    return to_string()[target];
}

#endif
