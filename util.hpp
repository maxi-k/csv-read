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
   uintptr_t file_size;
   T* mapping;

   using iterator = T*;

   public:
   MMapping() : handle(-1), file_size(0), mapping(nullptr) {}
   MMapping(const std::string& filename, int flags = 0, uintptr_t size = 0) : MMapping() { open(filename.data(), flags, size); }
   ~MMapping() { close(); }

   inline void open(const char* file, int flags, std::size_t size) {
      close();
      int h = ::open(file, O_RDWR | flags, 0655);
      if (h < 0) {
         auto err = errno;
         throw std::logic_error("Coud not open file " + std::string(file) + ":" +
                                std::string(strerror(err)));
      }

      if (size == 0) {
         lseek(h, 0, SEEK_END);
         file_size = lseek(h, 0, SEEK_CUR);
      } else {
         auto res = ::ftruncate(h, size);
         if (res < 0) {
            auto err = errno;
            throw std::logic_error("Could not resize file: " + std::string(strerror(err)));
         }
         file_size = size;
      }

      auto m = mmap(nullptr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, h, 0);
      if (m == MAP_FAILED) {
         auto err = errno;
         ::close(h);
         throw std::logic_error("Cloud not mmap file: " +
                                std::string(strerror(err)));
      }

      handle = h;
      mapping = static_cast<T*>(m);
   }

   inline void close() {
      if (handle >= 0) {
         ::munmap(mapping, file_size);
         ::close(handle);
         handle = -1;
         mapping = nullptr;
      }
   }

   inline void flush() const {
      if (handle) { ::fdatasync(handle); }
   }

   inline T* data() const { return mapping; }
   inline const iterator begin() const { return data(); }
   inline const iterator end() const { return data() + (this->file_size / sizeof(T)); }
};

struct fixed_size {
   static constexpr bool IS_VARIABLE = false;
};

template <typename T>
struct DataColumn : MMapping<T> {
   using size_tag = fixed_size;
   using iterator = T*;
   using value_type = T;

   static constexpr uintptr_t GLOBAL_OVERHEAD = 0;
   static constexpr uintptr_t PER_ITEM_OVERHEAD = 0;

   uintptr_t count = 0ul;

   DataColumn(const char* filename, int flags = 0, uintptr_t size = 0)
       : MMapping<T>(filename, flags, size)
       , count(this->file_size / sizeof(T)) {
       assert(this->file_size % sizeof(T) == 0);

   }

   uintptr_t size() const { return count; }
   const T& operator[](std::size_t idx) const { return this->data()[idx]; }
};

struct variable_size {
   static constexpr bool IS_VARIABLE = true;
   struct StringIndexSlot {
      uint64_t size;
      uint64_t offset;
   };

   struct StringData {
      uint64_t count;
      StringIndexSlot slot[];
   };
};

template <>
struct DataColumn<std::string_view> : MMapping<variable_size::StringData> {
   using size_tag = variable_size;
   using Data = typename variable_size::StringData;
   using value_type = std::string_view;
   static constexpr uintptr_t PER_ITEM_OVERHEAD = sizeof(variable_size::StringIndexSlot);
   static constexpr uintptr_t GLOBAL_OVERHEAD = sizeof(uint64_t);

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

   DataColumn(const char* filename, int flags = 0, uintptr_t size = 0)
       : MMapping<Data>(filename, flags, size) {}

   Data* data() const { return this->mapping; }
   uintptr_t size() const { return data()->count; }

   inline const iterator begin() const { return iterator{0, data()}; }
   inline const iterator end() const { return iterator{data()->count, data()}; }

   inline variable_size::StringIndexSlot& slot_at(std::size_t idx) const {
     return data()->slot[idx];
   }

   inline std::string_view operator[](std::size_t idx) const {
      auto slot = data()->slot[idx];
      return std::string_view(reinterpret_cast<char*>(data()) + slot.offset,
                              slot.size);
   }
};

}  // namespace io
