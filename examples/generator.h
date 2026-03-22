#include <coroutine>
#include <iostream>
#include <optional>
#include <map>
#include <string>
#include <memory>
#include <stdexcept>
#include <random>
#include <thread>
#include <concepts>

#include <openssl/sha.h>

namespace hsh {

  /**
   * Utility to generate random string with a given length
   **/
  std::string randomString(std::size_t length)
  {
    const std::string CHARACTERS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<> distribution(0, CHARACTERS.size() - 1);

    std::string random_string;

    for (std::size_t i = 0; i < length; ++i)
    {
      random_string += CHARACTERS[distribution(generator)];
    }

    return random_string;
  }

  /**
   * Convert N bytes to their respective Hex representation
   **/
  template <size_t N>
    std::string displayableDigest(const std::array<unsigned char, N>& buf)
    {
      const char letter[] { "0123456789abcdef"};

      std::string ret(N*2, '-');
      for (size_t i=0, iEnd = N ; i < iEnd; ++i)
      {
        ret[i*2] = letter[ buf[i] >> 4 ];
        ret[i*2+1] = letter[ buf[i] & 0xf ];
      }

      return ret;
    }

  /**
   * create a sha1 for string content
   **/
  std::string sha1(const std::string& content)
  {
    std::array<unsigned char, 20> buf;
    SHA1(reinterpret_cast<const unsigned char*>(content.data()), content.size(), buf.data());

    return displayableDigest(buf);
  }

	template<std::movable T>
  class generator {
		public:

      // ------------------------- Promise type -----------------------
			struct promise_type {
				generator<T> get_return_object()                      { return generator{Handle::from_promise(*this)}; }
				static std::suspend_always initial_suspend() noexcept { return {}; }
				static std::suspend_always final_suspend() noexcept   { return {}; }
				std::suspend_always yield_value(T value) noexcept     { current_value = std::move(value); return {}; }

				void await_transform() = delete;  	// Disallow co_await in generator coroutines.
				[[noreturn]] static void unhandled_exception() { throw; }

				T current_value;
			};

      // ------------------------ Impl --------------------------
			using Handle = std::coroutine_handle<promise_type>;

			generator() = default;
			explicit generator(const Handle coroutine) : cor_{coroutine} {}

			~generator() {
				if (cor_) {
					cor_.destroy();
				}
			}

			generator(const generator&) = delete;
			generator& operator=(const generator&) = delete;

			generator(generator&& other)            noexcept : cor_{other.cor_} { other.cor_ = {}; }
			generator& operator=(generator&& other) noexcept {
				if (this != &other) {
					if (cor_) { cor_.destroy(); }

					cor_ = other.cor_;
					other.cor_ = {};
				}
				return *this;
			}

			//  --------------------------- Range support ----------------------------
			class iterator {
				public:
					void operator++()                              { cor_.resume(); }
					const T& operator*() const                     { return cor_.promise().current_value; }
					bool operator==(std::default_sentinel_t) const { return !cor_ || cor_.done(); }
					explicit iterator(const Handle coroutine): cor_{coroutine} {}

				private:
					Handle cor_;
			};

			iterator begin() {
				if (cor_) { cor_.resume(); }
				return iterator{cor_};
			}
			std::default_sentinel_t end() { return {}; }

		private:
			Handle cor_;
	};


  using Hash = std::string;
  using Content = std::string;
  generator<std::pair<Hash, Content>>
  elements(size_t maxFiles = 10, size_t maxFileSize = 100)
  {
     for (size_t i = 0; i < maxFiles; ++i)
     {
       std::string content = randomString(maxFileSize);
       co_yield { sha1(content), content };
     }
  }
}
