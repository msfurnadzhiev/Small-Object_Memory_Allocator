#pragma once
#include <iostream>
#include <vector>
#include <climits>
#include <cstddef>
#include <cassert>
#include <exception>
#include <mutex>

//#define NDEBUG

#define DEFAULT_CHUNK_SIZE        4096
#define MIN_BLOCKS_PER_CHUNK      4
#define MAX_BLOCKS_PER_CHUNK      UCHAR_MAX
#define MAX_OBJECT_SIZE           256
#define DEFAULT_OBJECT_ALIGNMENT  4

namespace MemoryAllocator {

using small_size_t = unsigned char;
using pointer_t = void *;

inline auto get_offset = [](std::size_t bytes, std::size_t alignment) {return (bytes + alignment - 1)/alignment;};


class Chunk {

  friend class FixedAllocator;

 private:

  pointer_t m_data_ptr = nullptr;
  small_size_t m_first_available_block = 0;
  small_size_t m_available_blocks = 0;


  /** Initializes a chunk object.
   *  @param block_size Number of bytes ber block
   *  @param blocks Number of blocks per Chunk
   */
  void initialize(std::size_t block_size, small_size_t number_of_blocks);

  /// Clears an already allocated Chunk by reset its values by default.
  void reset(std::size_t block_size, small_size_t blocks) noexcept;

  /// Releases the allocated block of memory.
  void release() const noexcept;

  /** Allocate a block in the Chunk with constant time complexity.
   *  @return Pointer to block in the Chunk
   */
  pointer_t allocate(std::size_t block_size) noexcept;

  /// Deallocate a block in the Chunk with constant time complexity.
  void deallocate(pointer_t ptr, std::size_t block_size) noexcept;

  /// @return True if block at address ptr is inside this Chunk.
  bool has_block(pointer_t ptr, std::size_t chunk_length) const noexcept;
};

class FixedAllocator {

  using Chunks = std::vector<Chunk>;

 private:

  /// Number of bytes in a single block in a Chunk.
  std::size_t m_block_size = 0;
  /// Number of blocks managed by each Chunk.
  small_size_t m_number_of_blocks = 0;

  Chunks m_chunks{};

  /// Pointer to Chunk used for last allocation.
  Chunk *m_last_allocation = nullptr;
  /// Pointer to Chunk used for last deallocation.
  Chunk *m_last_deallocation = nullptr;

  /** Finds the Chunk which owns the block at address ptr
   * Worst-case time complexity of the function is O(n), where n is
   * number of Chunks
   * @return Pointer to Chunk that owns ptr, or nullptr if no owner found.
   */
  Chunk * find_in_vicinity(pointer_t ptr);

 public:

  FixedAllocator() = default;
  FixedAllocator(std::size_t block_size, std::size_t page_size);
  ~FixedAllocator();

  void initialize(std::size_t block_size, std::size_t page_size) noexcept;

  /**
   * @return Pointer to allocated memory block of fixed size or nullptr
   * if it failed to allocation.
   */
  pointer_t allocate();

  /// Deallocate a memory block at address ptr.
  void deallocate(pointer_t ptr) noexcept;

  /**
   * @return True if the block at address p is within a Chunk
   * owned by this FixedAllocator.
   */
  Chunk * has_block(pointer_t ptr) noexcept;

  size_t block_size() const noexcept {
	return m_block_size;
  }
};

class SmallObjectAllocator {

 private:

  std::size_t m_object_alignment;
  std::size_t m_max_object_size;

  std::vector<FixedAllocator> m_pool{};

 public:

  explicit SmallObjectAllocator(
	  std::size_t chunk_size,
	  std::size_t max_object_size,
	  std::size_t object_alignment);

  ~SmallObjectAllocator() = default;

  pointer_t allocate(std::size_t bytes);
  void deallocate(pointer_t ptr, std::size_t size);
  void deallocate(pointer_t ptr);
};

class SmallObject {

 private:

  inline static SmallObjectAllocator *m_small_object_allocator;
  inline static std::mutex m_mutex;

 public:

  static SmallObjectAllocator *get_instance(
	  std::size_t chunk_size = DEFAULT_CHUNK_SIZE,
	  std::size_t max_object_size = MAX_OBJECT_SIZE,
	  std::size_t object_alignment = DEFAULT_OBJECT_ALIGNMENT
  );

  virtual ~SmallObject() = default;

  static pointer_t operator new(std::size_t size);
  static pointer_t operator new(std::size_t size, pointer_t place);

  static void operator delete(pointer_t ptr);
  static void operator delete(pointer_t ptr, std::size_t size);
  static void operator delete(pointer_t ptr, pointer_t place);

};

}