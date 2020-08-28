#include "MemoryAllocator.h"
using namespace MemoryAllocator;

/*--- Chunk Methods ---*/

void Chunk::initialize(size_t block_size, small_size_t number_of_blocks) {

  assert(block_size > 0 && number_of_blocks > 0);

  const size_t allocate_size = block_size * number_of_blocks;

  assert(allocate_size / block_size == number_of_blocks);

  assert(m_data_ptr == nullptr);
  m_data_ptr = ::operator new(allocate_size);

  reset(block_size, number_of_blocks);
}

void Chunk::reset(size_t block_size, small_size_t blocks) noexcept{

  assert(block_size > 0 && blocks > 0);

  m_first_available_block = 0;
  m_available_blocks = blocks;

  auto *p = reinterpret_cast<small_size_t*>(m_data_ptr);

  for (small_size_t i = 1; i != blocks; ++i) {
	*p = i;
	p += block_size;
  }
}

void Chunk::release() const noexcept {
  assert(m_data_ptr !=nullptr);
  ::operator delete(m_data_ptr);
}

pointer_t Chunk::allocate(size_t block_size) noexcept {
  assert(block_size > 0);

  if (m_available_blocks < 1) return nullptr;

  size_t offset = m_first_available_block * block_size;
  small_size_t *result_ptr = reinterpret_cast<small_size_t*>(m_data_ptr) + offset;

  m_first_available_block = *result_ptr;
  --m_available_blocks;

  return result_ptr;
}

void Chunk::deallocate(pointer_t ptr, size_t block_size) noexcept {
  assert(block_size > 0 && ptr >= m_data_ptr);

  auto* to_release = reinterpret_cast<small_size_t*>(ptr);

  assert((to_release - reinterpret_cast<small_size_t*>(m_data_ptr)) % block_size == 0);

  *to_release = m_first_available_block;
  small_size_t index =
	  (to_release - reinterpret_cast<small_size_t*>(m_data_ptr)) / block_size;

  m_first_available_block = index;
  ++m_available_blocks;
}

bool Chunk::has_block(pointer_t ptr, size_t chunk_length) const noexcept {
  return (m_data_ptr <= ptr) && (ptr < reinterpret_cast<small_size_t*>(m_data_ptr) + chunk_length);
}



/*--- Fixed Allocator Methods ---*/

FixedAllocator::FixedAllocator(size_t block_size, size_t chunk_size) {
  initialize(block_size, chunk_size);
}

FixedAllocator::~FixedAllocator() {
  for(Chunk &chunk : m_chunks) {
	chunk.release();
  }
}

void FixedAllocator::initialize(size_t block_size, size_t chunk_size) noexcept {

  assert(block_size > 0 && chunk_size >= block_size);
  m_block_size = block_size;

  size_t number_of_blocks = chunk_size / block_size;

  assert(number_of_blocks > 0);
  if(number_of_blocks > MAX_BLOCKS_PER_CHUNK)
	number_of_blocks = MAX_BLOCKS_PER_CHUNK;
  else if(number_of_blocks < MIN_BLOCKS_PER_CHUNK)
	number_of_blocks = MIN_BLOCKS_PER_CHUNK;

  m_number_of_blocks = number_of_blocks;
}

Chunk * FixedAllocator::find_in_vicinity(pointer_t ptr) {

  if(m_chunks.empty()) return nullptr;

  const size_t chunk_length = m_number_of_blocks * m_block_size;

  Chunk *lower = m_last_deallocation, *upper = m_last_deallocation + 1;
  Chunk *lower_bound = &m_chunks.front(), *upper_bound = &m_chunks.back();

  if(upper == upper_bound) upper = nullptr;

  while(true) {
	if(lower) {
	  if(ptr >= lower->m_data_ptr && ptr < (reinterpret_cast<small_size_t*>(lower->m_data_ptr) + chunk_length)) {
		return lower;
	  }
	  if(lower == lower_bound) lower = nullptr;
	  else --lower;
	}
	if(upper) {
	  if(ptr >= upper->m_data_ptr && ptr < (reinterpret_cast<small_size_t*>(upper->m_data_ptr) + chunk_length)) {
		return upper;
	  }
	}
  }
}

pointer_t FixedAllocator::allocate() {

  if (!m_last_allocation || m_last_allocation->m_available_blocks == 0) {

	for(auto &chunk : m_chunks) {
	  if (chunk.m_available_blocks > 0) {
		m_last_allocation = &chunk;
		break;
	  }
	}

	if(m_last_allocation == nullptr) {
	  m_chunks.reserve(m_chunks.size() + 1);
	  Chunk new_chunk;
	  new_chunk.initialize(m_block_size, m_number_of_blocks);
	  m_chunks.push_back(new_chunk);
	  m_last_allocation = &m_chunks.back();
	  m_last_deallocation = &m_chunks.front();
	}
  }

  assert(m_last_allocation != nullptr);
  assert(m_last_allocation->m_available_blocks > 0);

  return m_last_allocation->allocate(m_block_size);
}

void FixedAllocator::deallocate(pointer_t ptr) noexcept {

  assert(!m_chunks.empty());

  m_last_deallocation = find_in_vicinity(ptr);

  assert(m_last_deallocation != nullptr);

  m_last_deallocation->deallocate(ptr, m_block_size);

  if(m_last_deallocation->m_available_blocks > m_number_of_blocks) {

	Chunk *last_chunk = &m_chunks.back();

	if(last_chunk == m_last_deallocation) {
	  if(m_chunks.size() > 1 && m_last_deallocation[-1].m_available_blocks == m_number_of_blocks) {
		last_chunk->release();
		m_chunks.pop_back();
		m_last_allocation = m_last_deallocation = &m_chunks.front();
	  }
	  return;
	}

	if(last_chunk->m_available_blocks == m_number_of_blocks) {
	  last_chunk->release();
	  m_chunks.pop_back();
	  m_last_allocation = m_last_deallocation;
	  if(last_chunk == m_last_deallocation || last_chunk->m_available_blocks == m_number_of_blocks) {
		return;
	  }
	}
	else {
	  std::swap(*m_last_deallocation, *last_chunk);
	  m_last_allocation = &m_chunks.back();
	}
  }
}

Chunk * FixedAllocator::has_block(pointer_t ptr) noexcept {
  const size_t chunk_length = m_number_of_blocks * m_block_size;
  for(Chunk &chunk : m_chunks) {
	if(chunk.has_block(ptr, chunk_length)) {
	  return &chunk;
	}
  }
  return nullptr;
}



/*--- Small Object Allocator Methods ---*/

SmallObjectAllocator::SmallObjectAllocator(size_t chunk_size, size_t max_object_size, size_t object_alignment) {

  m_max_object_size = max_object_size;
  m_object_alignment = object_alignment;

  const size_t count = get_offset(max_object_size, object_alignment);
  m_pool.reserve(count);
  for(size_t i = 0; i < count; i++) {
	m_pool[i].initialize((i+1) * object_alignment, chunk_size);
  }

}

pointer_t SmallObjectAllocator::allocate(size_t bytes) {

  if(bytes > m_max_object_size) {
	return ::operator new(bytes);
  }

  if(bytes == 0) return nullptr;

  const size_t index = get_offset(bytes, m_object_alignment) - 1;

  const size_t max_index = get_offset(m_max_object_size, m_object_alignment);
  assert(index < max_index);

  FixedAllocator & allocator = m_pool[index];

  pointer_t place = m_pool[index].allocate();

  return place;
}

void SmallObjectAllocator::deallocate(pointer_t ptr, size_t bytes) {
  if(!ptr || bytes == 0) return;
  if(bytes > m_max_object_size) {
	::operator delete(ptr);
  }

  const size_t index = get_offset(bytes, m_object_alignment) - 1;

  const size_t max_index = get_offset(m_max_object_size, m_object_alignment);
  assert(index < max_index);

  FixedAllocator & allocator = m_pool[index];
  assert(allocator.block_size() >= bytes);
  assert(allocator.block_size() < bytes + m_object_alignment);

  allocator.deallocate(ptr);
}

void SmallObjectAllocator::deallocate(pointer_t ptr) {

  if(ptr == nullptr) return;

  FixedAllocator *allocator_ptr = nullptr;
  const size_t max_index = get_offset(m_max_object_size, m_object_alignment);

  for(size_t i = 0; i < max_index; i++) {
	Chunk *chunk = m_pool[i].has_block(ptr);
	if(chunk) {
	  allocator_ptr = &m_pool[i];
	  break;
	}
  }
  if(allocator_ptr == nullptr) {
	return;
  }
}



/*--- Small Object Singleton ---*/

SmallObjectAllocator * SmallObject::get_instance(std::size_t chunk_size, std::size_t max_object_size, std::size_t object_alignment) {

  //double-check locking pattern
  if(m_small_object_allocator == nullptr) {

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_small_object_allocator == nullptr) {
	  	m_small_object_allocator = new SmallObjectAllocator(chunk_size, max_object_size, object_alignment);
	}
  }

  return m_small_object_allocator;
}

pointer_t SmallObject::operator new(std::size_t size)  {
  std::cout << "operator new" << std::endl;
  return SmallObject::get_instance()->allocate(size);
}

pointer_t SmallObject::operator new(std::size_t size, pointer_t place)  {
  std::cout << "global placement operator new" << std::endl;
  return ::operator new(size, place);
}


void SmallObject::operator delete(pointer_t ptr) {
  std::cout << "operator delete" << std::endl;
  SmallObject::get_instance()->deallocate(ptr);
}

void SmallObject::operator delete(pointer_t ptr, std::size_t size) {
  std::cout << "operator delete with size" << std::endl;
  SmallObject::get_instance()->deallocate(ptr, size);
}

void SmallObject::operator delete(pointer_t ptr, pointer_t place) {
  std::cout << "global placement operator delete" << std::endl;
  ::operator delete(ptr);
}