/*
 * blabla
 *
 * (C) 2023 Daniel Gibson
 */
 
#ifndef _DG_SCOPEDALLOCATOR_HPP_
#define _DG_SCOPEDALLOCATOR_HPP_

#include <string.h> // memset
#include <stdint.h>
#include <assert.h>

namespace dg {
	
	typedef unsigned char byte;
	
namespace impl {

/*
 * General Design of the Implementation
 *
 * Each thread has a PerThreadAllocator in TLS, creating a ScopedAllocator
 * automatically references it.
 * Each PerThreadAllocator has one or more MemBlocks of a fixed size (MEMBLOCK_SIZE, maybe 16MB),
 * if one is full, a second (third, ...) is created as needed.
 * The memory of a MemBlock contains Chunks of memory, each representing an allocation
 * and aligned to 16 bytes. Each Chunk starts with a ChunkHeader followed by its actual memory
 * (as returned to the user), unless the user requested too much memory (> MAX_CHUNK_SIZE),
 * then the memory returned to the user is allocated through a normal heap allocator,
 * then the ChunkHeader is just followed by a pointer to that memory and its size (struct ExtMem).
 *
 * All memory allocated through a ScopedAllocator is freed by the ScopedAllocator's destructor,
 * so it's kinda like stack memory
 */

enum {
	MEMBLOCK_SIZE = 16*1024*1024, // 16MB
	MAX_CHUNK_SIZE = 4*1024*1024, // 4MB
};

byte* HeapAlloc(size_t size, size_t alignment = 16);
void HeapFree(void* mem);

struct alignas(16) ChunkHeader {
	
	enum Flags : uint32_t {
		IS_EXTERNALLY_ALLOCATED = 1 << 24, // in that case, data is a struct ExtMem
		
		FLAGS_MASK = uint32_t(255) << 24,
		SIZE_MASK = ~FLAGS_MASK
	};
	
	// TODO: make size int32_t and use -1 for "externally allocated", if we don't need any other flags?
	uint32_t flags_size;
	uint32_t offsetToPrev; // = 0
	
	// TODO: allow alignments > 16 bytes? then we need to store an offset within data/em->dataPtr, for destructFun() and free()

	// this can be used to properly call the destructor(s) of the element(s) held by this block
	typedef void (DestructFunc)(void* data, size_t dataSize);
	DestructFunc* destructFunc; // = nullptr

	// used for data allocated externally (with malloc() or similar)
	// because the requested size was too big. stored in data[], if flags_size & IS_EXTERNALLY_ALLOCATED
	struct ExtMem {
		void* dataPtr;
		size_t dataSize;
	};

	// flexible array member to hold the data (it's right after this object)
	alignas(16) byte data[];
	

	void* GetData() {
		if((flags_size & IS_EXTERNALLY_ALLOCATED) == 0)
			return data;
		ExtMem* em = reinterpret_cast<ExtMem*>(data);
		return em->dataPtr;
	}
	
	// size of this block within the stack memory
	// if IS_EXTERNALLY_ALLOCATED, this will be sizeof(AllocatedMemBlock) + sizeof(void*),
	// rounded up to the next multiple of 16
	uint32_t GetBlockSize() {
		uint32_t dataSize = ((flags_size & IS_EXTERNALLY_ALLOCATED) == 0)
			? (flags_size & SIZE_MASK) : sizeof(ExtMem);
		
		dataSize = (dataSize + 15) & ~uint32_t(15); // round to multiple of 16
		
		return sizeof(ChunkHeader) + dataSize;
	}
	
	size_t GetDataSize() {
		if((flags_size & IS_EXTERNALLY_ALLOCATED) == 0)
			return (flags_size & SIZE_MASK);

		ExtMem* em = reinterpret_cast<ExtMem*>(data);
		return em->dataSize;
	}

	void Free() {
		void* mem = GetData();
		if(destructFunc != nullptr) {
			destructFunc(mem, GetDataSize());
		}
		if(flags_size & IS_EXTERNALLY_ALLOCATED) {
			HeapFree(mem);
		}
		flags_size = 0;
		destructFunc = nullptr;
	}
};

static_assert((sizeof(ChunkHeader) & 15) == 0, "ChunkHeader's size should be (a multiple of) 16 bytes!");

struct MemBlock {
	byte* memory = nullptr;

	uint32_t lastOffset = 0; // offset of last allocated chunk
	uint32_t curOffset = 0;  // offset of next chunk to allocate

	void Init() {
		memory = HeapAlloc(MEMBLOCK_SIZE);
		lastOffset = curOffset = 0;
	}

	void Destroy() {
		FreeUntilOffset(0);
		HeapFree(memory);
		memory = nullptr;
	}

	// frees all objects in this memblock until (including) offset, starting at the end
	void FreeUntilOffset(uint32_t offset) {
		uint32_t curOS = curOffset;
		if(curOS == 0)
			return;
		uint32_t lastOS = lastOffset;
		byte* mem = memory;
		ChunkHeader* chunk;
		do {
			byte* chunkMem = mem + lastOS;
			chunk = (ChunkHeader*)chunkMem;
			chunk->Free();
			curOS = lastOS;
			assert(chunk->offsetToPrev <= lastOS);
			lastOS -= chunk->offsetToPrev;
			assert(chunkMem >= mem);
		} while(lastOS >= offset);

		curOffset = curOS;
		lastOffset = lastOS;
		assert(offset != 0 || (lastOffset == 0 && curOffset == 0));
	}

	byte* Allocate(size_t size, uint32_t* out_offset, ChunkHeader::DestructFunc* destructFunc = nullptr) {
		uint32_t chunkSize = (size <= MAX_CHUNK_SIZE) ? size : sizeof(ChunkHeader::ExtMem);
		chunkSize = (chunkSize + 15) & ~uint32_t(15); // make sure chunkSize is multiple of 16
		chunkSize += sizeof(ChunkHeader);
		if(lastOffset + chunkSize > MEMBLOCK_SIZE)
			return nullptr;

		uint32_t offsetToPrev = curOffset - lastOffset;

		lastOffset = curOffset;
		curOffset += chunkSize;
		*out_offset = lastOffset; // FIXME: what was out_offset for?

		byte* ret = nullptr;

		ChunkHeader* chunk = (ChunkHeader*)(memory+lastOffset);
		chunk->offsetToPrev = offsetToPrev;
		chunk->destructFunc = destructFunc;
		if(size <= MAX_CHUNK_SIZE) {
			chunk->flags_size = size;
			ret = chunk->data;
		} else {
			chunk->flags_size = ChunkHeader::IS_EXTERNALLY_ALLOCATED;
			ChunkHeader::ExtMem* em = (ChunkHeader::ExtMem*)chunk->data;
			ret = HeapAlloc(size);
			em->dataPtr = ret;
			em->dataSize = size;
		}

		return ret;
	}

};

#if 0 // FIXME

// what if
void fun() {
	ScopedAllocator sa1;
	void* bla = sa1.Alloc(42);
	void* ding = nullptr;
	if(whatever)
	{
		ScopedAllocator sa2;
		void* blub = sa2.Alloc(23);
		ding = sa1.Alloc(50); // sa1 !!
	}
	if(ding)
		doSomething(ding);
}
// ???
// => ding will be allocated behind blub (of sa2)
// and thus it will be free'd when sa2 goes out of scope
// even though it "belongs to" sa1
// I probably can't prevent sa1 from being used in sa2's scope, or can I? with an assertion maybe?
// *should* it be disallowed? should I instead delay freeing sa2? or only destroy its elements but not decrease the "stack pointer"?

#endif


struct PerThreadAllocator
{
	int curMemBlockIdx = 0;
	MemBlock memBlocks[8]; // TODO: dynamic?

	PerThreadAllocator() {
		memBlocks[0].Init();
	}
};
	
} //namespace impl

class ScopedAllocator
{
	static thread_local impl::PerThreadAllocator tlsPerThreadAlloc;
	
	// reference to tlsPerThreadAlloc so TLS only needs to be looked up
	// once, in constructor
	impl::PerThreadAllocator& perThreadAlloc;
	
	int startMemBlockIndex = 0;
	uint32_t startMemBlockOffset = 0;

public:
	ScopedAllocator() : perThreadAlloc( tlsPerThreadAlloc )
	{
		startMemBlockIndex = perThreadAlloc.curMemBlockIdx;
		startMemBlockOffset = perThreadAlloc.memBlocks[startMemBlockIndex].curOffset;
	}
	
	~ScopedAllocator() {
		// TODO
	}
	
	void* AllocRaw(size_t size) {
		// TODO
		return nullptr;
	}
	
	void* AllocRawZeroed(size_t size) {
		void* ret = AllocRaw(size);
		if(size > 0)
			memset(ret, 0, size);
		return ret;
	}
	
	
};


} //namespace dg
#endif // _DG_SCOPEDALLOCATOR_HPP_

// ########## non-inline Implementation ##########

#ifdef DG_SCOPEDALLOCATOR_IMPL

#include <stdlib.h> // malloc(), free()
#ifdef _WIN32
#include <malloc.h> // _aligned_malloc
#endif

namespace dg {
	
thread_local impl::PerThreadAllocator ScopedAllocator::tlsPerThreadAlloc;

byte* impl::HeapAlloc(size_t size, size_t alignment)
{
#ifdef _WIN32
	// Windows: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/aligned-malloc?view=msvc-140
	return (byte*)_aligned_malloc(size, alignment);
#else // C11, C++17 on sane platforms
	return (byte*)::aligned_alloc(alignment, size);

	// TODO: check for aligned_alloc() and maybe posix_memalign() in cmake or something?

	// FIXME: macos only supports this in macOS 10.15 (catalina, 2019) and newer,
	//        posix_memalign() wasn't always available either.. (10.6+)
	//        but it guarantees 16byte alignment at least
	// TODO: posix_memalign() for older linux distros? (aligned_alloc() is glibc 2.16+, 2012)

#endif
}

void impl::HeapFree(void* mem)
{
#ifdef _WIN32
	// https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/aligned-free?view=msvc-140
	_aligned_free(mem);
#else // C11, C++17 on sane platforms
	::free(mem);
#endif
}


} //namespace dg
#endif // DG_SCOPEDALLOCATOR_IMPL
