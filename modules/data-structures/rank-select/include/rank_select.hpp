#pragma once
#include <algorithm>
#include <bit>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

namespace algo {

class RankSelect {
 public:
  enum class Layout { Compact, Fast };

  // Constructor from packed words
  RankSelect(std::span<const std::uint64_t> words, std::uint64_t nbits,
             Layout layout = Layout::Compact);

  // Constructor from std::vector<bool>
  explicit RankSelect(const std::vector<bool>& bits, Layout layout = Layout::Compact);

  // Copyable and movable
  RankSelect(const RankSelect&) = default;
  RankSelect& operator=(const RankSelect&) = default;
  RankSelect(RankSelect&&) = default;
  RankSelect& operator=(RankSelect&&) = default;

  std::uint64_t size() const noexcept { return nbits_; }
  std::uint64_t ones() const noexcept { return ones_; }

  // Returns bit at position i; throws std::out_of_range if i >= n
  bool access(std::uint64_t i) const;

  // Returns number of 1s in [0, i); legal for 0 <= i <= n
  // Throws std::out_of_range if i > n
  std::uint64_t rank1(std::uint64_t i) const;

  // Returns i - rank1(i)
  std::uint64_t rank0(std::uint64_t i) const;

  // Returns position of k-th 1 (0-based); throws std::out_of_range if k >= ones()
  std::uint64_t select1(std::uint64_t k) const;

  // Returns position of k-th 0 (0-based); throws std::out_of_range if k >= (size() - ones())
  std::uint64_t select0(std::uint64_t k) const;

  // Bits of all auxiliary directories (not the raw words)
  std::uint64_t overhead_bits() const noexcept;

  // ceil(n/64)*64 + overhead_bits()
  std::uint64_t total_bits() const noexcept {
    return ((nbits_ + 63) / 64) * 64 + overhead_bits();
  }

  // 64-bit memory words read by most recent query
  std::uint64_t last_probes() const noexcept { return last_probes_; }

  // Verify directory consistency by recomputing from raw bits
  void check_invariants() const;

 private:
  std::vector<std::uint64_t> words_;  // packed bit vector
  std::uint64_t nbits_;               // number of bits
  std::uint64_t ones_;                // total number of 1 bits
  Layout layout_;
  mutable std::uint64_t last_probes_;

  // Compact layout structures
  struct CompactDir {
    std::vector<std::uint64_t> superblock_ranks;  // absolute rank at superblock starts
    std::vector<std::uint16_t> block_ranks;        // relative ranks within superblock
    std::vector<std::uint64_t> select1_samples;    // positions of 8192*k-th ones
    std::vector<std::uint64_t> select0_samples;    // positions of 8192*k-th zeros
  };

  // Fast layout structures (rank9-style)
  struct FastDir {
    std::vector<std::uint64_t> block_abs;         // absolute count per 512-bit block
    std::vector<std::uint64_t> block_rel;         // seven 9-bit relative counts per 512-bit block
    std::vector<std::uint64_t> select1_samples;   // positions of 8192*k-th ones
    std::vector<std::uint64_t> select0_samples;   // positions of 8192*k-th zeros
  };

  CompactDir compact_dir_;
  FastDir fast_dir_;

  // Build auxiliary structures
  void build_compact();
  void build_fast();

  // Helper to get a bit from the word array
  bool get_bit(std::uint64_t i) const noexcept {
    return (words_[i / 64] >> (i % 64)) & 1;
  }

  // Helper to count 1s in a word
  std::uint64_t popcount(std::uint64_t w) const noexcept { return std::popcount(w); }

  // Helper to count 1s in [0, i) within a block
  std::uint64_t count_in_partial(std::uint64_t block_idx, std::uint64_t i_in_block) const
      noexcept;

  // Select in a word using bit operations
  std::uint64_t select_in_word(std::uint64_t w, std::uint64_t k) const noexcept;
};

// Inline implementations

inline RankSelect::RankSelect(std::span<const std::uint64_t> words, std::uint64_t nbits,
                               Layout layout)
    : nbits_(nbits), layout_(layout), last_probes_(0) {
  // Copy words, masking unused bits in the last word
  const std::uint64_t num_words = (nbits + 63) / 64;
  words_.reserve(num_words);
  for (std::size_t i = 0; i < words.size(); ++i) {
    if (i < num_words) {
      words_.push_back(words[i]);
    }
  }
  if (words_.size() < num_words) {
    words_.resize(num_words, 0);
  }

  // Mask unused bits in the last word
  if (nbits > 0 && nbits % 64 != 0) {
    const std::uint64_t mask = (1ULL << (nbits % 64)) - 1;
    words_.back() &= mask;
  }

  // Count total 1s
  ones_ = 0;
  for (auto w : words_) ones_ += popcount(w);

  // Build the appropriate directory structure
  if (layout == Layout::Compact) {
    build_compact();
  } else {
    build_fast();
  }
}

inline RankSelect::RankSelect(const std::vector<bool>& bits, Layout layout)
    : RankSelect(std::span<const std::uint64_t>(), bits.size(), layout) {
  // Convert std::vector<bool> to word format
  nbits_ = bits.size();
  const std::uint64_t num_words = (nbits_ + 63) / 64;
  words_.clear();
  words_.resize(num_words, 0);

  for (std::size_t i = 0; i < bits.size(); ++i) {
    if (bits[i]) {
      words_[i / 64] |= 1ULL << (i % 64);
    }
  }

  // Count total 1s
  ones_ = 0;
  for (auto w : words_) ones_ += popcount(w);

  // Build the appropriate directory structure
  if (layout == Layout::Compact) {
    build_compact();
  } else {
    build_fast();
  }
}

inline bool RankSelect::access(std::uint64_t i) const {
  if (i >= nbits_) throw std::out_of_range("access: index out of range");
  last_probes_ = 1;
  return get_bit(i);
}

inline std::uint64_t RankSelect::rank1(std::uint64_t i) const {
  if (i > nbits_) throw std::out_of_range("rank1: index out of range");
  if (i == 0) {
    last_probes_ = 0;
    return 0;
  }
  if (i == nbits_) {
    last_probes_ = 1;
    return ones_;
  }

  last_probes_ = 0;
  if (layout_ == Layout::Compact) {
    const std::uint64_t superblock_idx = i / 32768;
    const std::uint64_t block_idx = i / 512;
    const std::uint64_t i_in_block = i % 512;
    std::uint64_t count = 0;
    if (superblock_idx < compact_dir_.superblock_ranks.size()) {
      count = compact_dir_.superblock_ranks[superblock_idx];
      ++last_probes_;
    }
    if (block_idx < compact_dir_.block_ranks.size()) {
      count += compact_dir_.block_ranks[block_idx];
      ++last_probes_;
    }
    const std::uint64_t bit_offset = i_in_block % 64;
    const std::uint64_t word_idx = block_idx * 8 + i_in_block / 64;
    for (std::uint64_t w = block_idx * 8; w < word_idx && w < words_.size(); ++w) {
      count += popcount(words_[w]);
      ++last_probes_;
    }
    if (bit_offset != 0 && word_idx < words_.size()) {
      count += popcount(words_[word_idx] & ((1ULL << bit_offset) - 1));
      ++last_probes_;
    }
    return count;
  } else {  // Fast layout
    const std::uint64_t block_idx = i / 512;
    const std::uint64_t i_in_block = i % 512;
    std::uint64_t count = 0;
    if (block_idx < fast_dir_.block_abs.size()) {
      count = fast_dir_.block_abs[block_idx];
      last_probes_++;
    }

    // Add the count in complete words before this word (rank9-style).
    if (block_idx < fast_dir_.block_rel.size()) {
      const std::uint64_t rel_word = fast_dir_.block_rel[block_idx];
      const std::uint64_t word_in_block = i_in_block / 64;
      if (word_in_block > 0) {
        count += (rel_word >> ((word_in_block - 1) * 9)) & 0x1FF;
      }
      last_probes_++;
    }

    const std::uint64_t bit_offset = i_in_block % 64;
    const std::uint64_t word_idx = block_idx * 8 + i_in_block / 64;
    if (bit_offset != 0 && word_idx < words_.size()) {
      count += popcount(words_[word_idx] & ((1ULL << bit_offset) - 1));
      last_probes_++;
    }
    return count;
  }
}

inline std::uint64_t RankSelect::rank0(std::uint64_t i) const {
  if (i > nbits_) throw std::out_of_range("rank0: index out of range");
  return i - rank1(i);
}

inline std::uint64_t RankSelect::select1(std::uint64_t k) const {
  if (k >= ones_) throw std::out_of_range("select1: index out of range");

  last_probes_ = 0;
  const std::uint64_t sample_interval = 8192;
  const std::uint64_t sample_idx = k / sample_interval;

  std::uint64_t left = 0, right = nbits_;
  if (layout_ == Layout::Compact && sample_idx < compact_dir_.select1_samples.size()) {
    left = compact_dir_.select1_samples[sample_idx];
    last_probes_++;
  }
  if (layout_ == Layout::Fast && sample_idx < fast_dir_.select1_samples.size()) {
    left = fast_dir_.select1_samples[sample_idx];
    last_probes_++;
  }
  if (sample_idx + 1 < (layout_ == Layout::Compact ? compact_dir_.select1_samples.size()
                                                     : fast_dir_.select1_samples.size())) {
    right = (layout_ == Layout::Compact ? compact_dir_.select1_samples[sample_idx + 1]
                                         : fast_dir_.select1_samples[sample_idx + 1]);
    last_probes_++;
  }

  // Binary search in the range
  while (left < right) {
    const std::uint64_t mid = left + (right - left) / 2;
    const std::uint64_t r = rank1(mid + 1);
    last_probes_++;
    if (r <= k) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }

  return left;
}

inline std::uint64_t RankSelect::select0(std::uint64_t k) const {
  if (k >= nbits_ - ones_) throw std::out_of_range("select0: index out of range");

  last_probes_ = 0;
  const std::uint64_t sample_interval = 8192;
  const std::uint64_t sample_idx = k / sample_interval;

  std::uint64_t left = 0, right = nbits_;
  if (layout_ == Layout::Compact && sample_idx < compact_dir_.select0_samples.size()) {
    left = compact_dir_.select0_samples[sample_idx];
    last_probes_++;
  }
  if (layout_ == Layout::Fast && sample_idx < fast_dir_.select0_samples.size()) {
    left = fast_dir_.select0_samples[sample_idx];
    last_probes_++;
  }
  if (sample_idx + 1 < (layout_ == Layout::Compact ? compact_dir_.select0_samples.size()
                                                     : fast_dir_.select0_samples.size())) {
    right = (layout_ == Layout::Compact ? compact_dir_.select0_samples[sample_idx + 1]
                                         : fast_dir_.select0_samples[sample_idx + 1]);
    last_probes_++;
  }

  // Binary search in the range
  while (left < right) {
    const std::uint64_t mid = left + (right - left) / 2;
    const std::uint64_t r = rank0(mid + 1);
    last_probes_++;
    if (r <= k) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }

  return left;
}

inline std::uint64_t RankSelect::overhead_bits() const noexcept {
  if (layout_ == Layout::Compact) {
    std::uint64_t bits = 0;
    bits += compact_dir_.superblock_ranks.size() * 64;
    bits += compact_dir_.block_ranks.size() * 16;
    bits += compact_dir_.select1_samples.size() * 64;
    bits += compact_dir_.select0_samples.size() * 64;
    return bits;
  } else {
    std::uint64_t bits = 0;
    bits += fast_dir_.block_abs.size() * 64;
    bits += fast_dir_.block_rel.size() * 64;
    bits += fast_dir_.select1_samples.size() * 64;
    bits += fast_dir_.select0_samples.size() * 64;
    return bits;
  }
}

inline void RankSelect::build_compact() {
  compact_dir_.superblock_ranks.clear();
  compact_dir_.block_ranks.clear();
  if (nbits_ == 0) return;

  const std::uint64_t num_words = (nbits_ + 63) / 64;
  const std::uint64_t superblock_size = 32768;  // bits
  const std::uint64_t block_size = 512;          // bits
  const std::uint64_t num_superblocks = (nbits_ + superblock_size - 1) / superblock_size;
  const std::uint64_t num_blocks = (nbits_ + block_size - 1) / block_size;

  // Build superblock ranks
  compact_dir_.superblock_ranks.reserve(num_superblocks);
  std::uint64_t rank = 0;
  for (std::uint64_t sb = 0; sb < num_superblocks; ++sb) {
    compact_dir_.superblock_ranks.push_back(rank);
    const std::uint64_t sb_end = std::min((sb + 1) * superblock_size, nbits_);
    for (std::uint64_t b = sb * superblock_size / block_size;
         b < (sb_end + block_size - 1) / block_size; ++b) {
      const std::uint64_t block_end = std::min((b + 1) * block_size, nbits_);
      for (std::uint64_t w = b * block_size / 64; w < (block_end + 63) / 64; ++w) {
        if (w < num_words) rank += popcount(words_[w]);
      }
    }
  }

  // Build block ranks (relative to superblock)
  compact_dir_.block_ranks.reserve(num_blocks);
  rank = 0;
  std::uint64_t current_sb = 0;
  for (std::uint64_t b = 0; b < num_blocks; ++b) {
    const std::uint64_t sb = b * block_size / superblock_size;
    if (sb != current_sb) {
      rank = 0;
      current_sb = sb;
    }
    compact_dir_.block_ranks.push_back(rank & 0xFFFF);  // 16-bit relative rank
    const std::uint64_t block_end = std::min((b + 1) * block_size, nbits_);
    for (std::uint64_t w = b * block_size / 64; w < (block_end + 63) / 64; ++w) {
      if (w < num_words) rank += popcount(words_[w]);
    }
  }

  // Build select1 samples (position of every 8192-th one)
  const std::uint64_t sample_interval = 8192;
  compact_dir_.select1_samples.clear();
  std::uint64_t current_ones = 0;
  std::uint64_t next_sample = 0;
  for (std::uint64_t i = 0; i < nbits_; ++i) {
    if (get_bit(i)) {
      if (current_ones == next_sample * sample_interval) {
        compact_dir_.select1_samples.push_back(i);
        next_sample++;
      }
      current_ones++;
    }
  }
  // Add sentinel
  compact_dir_.select1_samples.push_back(nbits_);

  // Build select0 samples (position of every 8192-th zero)
  compact_dir_.select0_samples.clear();
  std::uint64_t current_zeros = 0;
  next_sample = 0;
  for (std::uint64_t i = 0; i < nbits_; ++i) {
    if (!get_bit(i)) {
      if (current_zeros == next_sample * sample_interval) {
        compact_dir_.select0_samples.push_back(i);
        next_sample++;
      }
      current_zeros++;
    }
  }
  // Add sentinel
  compact_dir_.select0_samples.push_back(nbits_);
}

inline void RankSelect::build_fast() {
  fast_dir_.block_abs.clear();
  fast_dir_.block_rel.clear();
  if (nbits_ == 0) return;

  const std::uint64_t num_words = (nbits_ + 63) / 64;
  const std::uint64_t block_size = 512;  // bits
  const std::uint64_t num_blocks = (nbits_ + block_size - 1) / block_size;

  // Build block absolute ranks and relative ranks
  fast_dir_.block_abs.reserve(num_blocks);
  fast_dir_.block_rel.reserve(num_blocks);
  std::uint64_t rank = 0;

  for (std::uint64_t b = 0; b < num_blocks; ++b) {
    fast_dir_.block_abs.push_back(rank);

    // Compute 7 relative counts for the next 64-bit words
    std::uint64_t rel_word = 0;
    std::uint64_t block_start = b * block_size / 64;
    std::uint64_t block_end = std::min((b + 1) * block_size / 64, num_words);
    std::uint64_t rel_rank = 0;

    for (std::uint64_t i = 0; i < 8 && block_start + i < block_end; ++i) {
      if (i > 0) rel_word |= (rel_rank & 0x1FF) << ((i - 1) * 9);
      rel_rank += popcount(words_[block_start + i]);
    }
    fast_dir_.block_rel.push_back(rel_word);
    rank += rel_rank;
  }

  // Build select1 samples
  const std::uint64_t sample_interval = 8192;
  fast_dir_.select1_samples.clear();
  std::uint64_t current_ones = 0;
  std::uint64_t next_sample = 0;
  for (std::uint64_t i = 0; i < nbits_; ++i) {
    if (get_bit(i)) {
      if (current_ones == next_sample * sample_interval) {
        fast_dir_.select1_samples.push_back(i);
        next_sample++;
      }
      current_ones++;
    }
  }
  // Add sentinel
  fast_dir_.select1_samples.push_back(nbits_);

  // Build select0 samples
  fast_dir_.select0_samples.clear();
  std::uint64_t current_zeros = 0;
  next_sample = 0;
  for (std::uint64_t i = 0; i < nbits_; ++i) {
    if (!get_bit(i)) {
      if (current_zeros == next_sample * sample_interval) {
        fast_dir_.select0_samples.push_back(i);
        next_sample++;
      }
      current_zeros++;
    }
  }
  // Add sentinel
  fast_dir_.select0_samples.push_back(nbits_);
}

inline void RankSelect::check_invariants() const {
  // Verify ones count
  std::uint64_t expected_ones = 0;
  for (auto w : words_) expected_ones += popcount(w);
  if (expected_ones != ones_) throw std::logic_error("ones count mismatch");

  // Verify rank1 at all block boundaries and random points
  if (layout_ == Layout::Compact) {
    // Verify both levels of the rank directory.
    for (std::uint64_t sb = 0; sb < compact_dir_.superblock_ranks.size(); ++sb) {
      const std::uint64_t expected = rank1(sb * 32768);
      if (compact_dir_.superblock_ranks[sb] != expected) {
        throw std::logic_error("superblock rank mismatch");
      }
    }
    for (std::uint64_t b = 0; b < compact_dir_.block_ranks.size(); ++b) {
      const std::uint64_t sb = (b * 512) / 32768;
      const std::uint64_t expected = rank1(b * 512) - compact_dir_.superblock_ranks[sb];
      if (compact_dir_.block_ranks[b] != expected) {
        throw std::logic_error("block rank mismatch");
      }
    }
  } else {
    for (std::uint64_t b = 0; b < fast_dir_.block_abs.size(); ++b) {
      const std::uint64_t expected_abs = rank1(b * 512);
      if (fast_dir_.block_abs[b] != expected_abs) {
        throw std::logic_error("fast block rank mismatch");
      }
      std::uint64_t expected_rel = 0;
      const std::uint64_t word_start = b * 8;
      for (std::uint64_t i = 1; i < 8 && word_start + i < words_.size(); ++i) {
        expected_rel += popcount(words_[word_start + i - 1]);
        const std::uint64_t stored = (fast_dir_.block_rel[b] >> ((i - 1) * 9)) & 0x1FF;
        if (stored != expected_rel) throw std::logic_error("fast relative rank mismatch");
      }
    }
  }

  // Verify every select sample against the first occurrence of its rank.
  const std::uint64_t sample_interval = 8192;
  const auto& samples1 = layout_ == Layout::Compact ? compact_dir_.select1_samples
                                                     : fast_dir_.select1_samples;
  const auto& samples0 = layout_ == Layout::Compact ? compact_dir_.select0_samples
                                                     : fast_dir_.select0_samples;
  for (std::uint64_t i = 0; i + 1 < samples1.size(); ++i) {
    const std::uint64_t pos = samples1[i];
    if (pos >= nbits_ || !get_bit(pos) || rank1(pos) != i * sample_interval ||
        rank1(pos + 1) != i * sample_interval + 1) {
      throw std::logic_error("select1 sample mismatch");
    }
  }
  for (std::uint64_t i = 0; i + 1 < samples0.size(); ++i) {
    const std::uint64_t pos = samples0[i];
    if (pos >= nbits_ || get_bit(pos) || rank0(pos) != i * sample_interval ||
        rank0(pos + 1) != i * sample_interval + 1) {
      throw std::logic_error("select0 sample mismatch");
    }
  }
}

}  // namespace algo
