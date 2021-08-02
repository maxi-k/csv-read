#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

namespace io {

template<typename T = char>
struct MMapping {
   int handle;
   uintptr_t size;
   T* mapping;

   using iterator = T*;

   public:
   MMapping() : handle(-1), size(0), mapping(nullptr) {}
   MMapping(const std::string& filename, int flags = 0) : MMapping() { open(filename.data(), flags); }
   ~MMapping() { close(); }

   inline void open(const char* file, int flags) {
      close();
      int h = ::open(file, O_RDONLY | flags);
      if (h < 0) {
         auto err = errno;
         throw std::logic_error("Cloud not open file " +
                                std::string(strerror(err)));
      }

      lseek(h, 0, SEEK_END);
      size = lseek(h, 0, SEEK_CUR);

      auto m = mmap(nullptr, size, PROT_READ, MAP_SHARED, h, 0);
      if (m == MAP_FAILED) {
         auto err = errno;
         ::close(h);
         throw std::logic_error("Cloud not mmap file " +
                                std::string(strerror(err)));
      }

      handle = h;
      mapping = static_cast<T*>(m);
   }

   inline void close() {
      if (handle >= 0) {
         ::munmap(mapping, size);
         ::close(handle);
         handle = -1;
         mapping = nullptr;
      }
   }

   inline T* data() const { return mapping; }
   inline const iterator begin() const { return data(); }
   inline const iterator end() const { return static_cast<char*>(data()) + this->size; }
};

template <typename T>
struct DataColumn : MMapping<T> {
   using iterator = T*;
   using value_type = T;

   uintptr_t count = 0ul;

   DataColumn(const char* filename, int flags = 0)
       : MMapping<T>(filename, flags)
       , count(this->size / sizeof(T)) {
       assert(this->size % sizeof(T) == 0);

   }

   uintptr_t size() const { return count; }
   const T& operator[](std::size_t idx) const { return this->data()[idx]; }
};

namespace variable_size {
   struct StringIndexSlot {
      uint64_t size;
      uint64_t offset;
   };

   struct StringData {
      uint64_t count;
      StringIndexSlot slot[];
   };
}

template <>
struct DataColumn<std::string_view> : MMapping<variable_size::StringData> {
   using Data = variable_size::StringData;
   using value_type = std::string_view;

   struct iterator {
      uint64_t idx;
      Data* data;

      inline iterator& operator++() {
         ++idx;
         return *this;
      }

      inline iterator operator++(int) {
         auto prev = *this;
         ++idx;
         return prev;
      }

      std::string_view operator*() {
         auto slot = data->slot[idx];
         return std::string_view(reinterpret_cast<char*>(data) + slot.offset,
                                 slot.size);
      }

      bool operator==(const iterator& rhs) {
         return this->idx == rhs.idx && this->data == rhs.data;
      }
   };

   DataColumn(const char* filename, int flags = 0)
       : MMapping<Data>(filename, flags) {}

   Data* data() const { return this->mapping; }
   uintptr_t size() const { return data()->count; }

   inline const iterator begin() const { return iterator{0, data()}; }
   inline const iterator end() const { return iterator{data()->count, data()}; }

   inline std::string_view operator[](std::size_t idx) const {
      auto slot = data()->slot[idx];
      return std::string_view(reinterpret_cast<char*>(data()) + slot.offset,
                              slot.size);
   }
};

}  // namespace io
