#include <coroutine>
#include <type_traits>
#include <utility>
#include <exception>
#include <iterator>
#include <functional>
#include <memory>

template<typename T>
class generator;

namespace detail
{
	template<typename T>
	class generator_promise
	{
	public:
		using value_type = std::remove_reference_t<T>;
		using reference_type = std::conditional_t<std::is_reference_v<T>, T, T&>;
		using pointer_type = value_type*;

		pointer_type value_{};
		std::exception_ptr exception_{};

	public:
		generator_promise() = default;
		generator<T> get_return_object() noexcept;

		constexpr std::suspend_always initial_suspend() const noexcept { return {}; }
		constexpr std::suspend_always final_suspend() const noexcept { return {}; }

		template<
			typename U = T,
			std::enable_if_t<!std::is_rvalue_reference<U>::value, int> = 0>
		std::suspend_always yield_value(std::remove_reference_t<T>& value) noexcept
		{
			value_ = std::addressof(value);
			return {};
		}

		std::suspend_always yield_value(std::remove_reference_t<T>&& value) noexcept
		{
			value_ = std::addressof(value);
			return {};
		}

		void unhandled_exception()
		{
			exception_ = std::current_exception();
		}

		void return_void()
		{
		}

		reference_type value() const noexcept
		{
			return static_cast<reference_type>(*value_);
		}

		template<typename U>
		std::suspend_never await_transform(U&& value) = delete;

		void rethrow_if_exception()
		{
			if (exception_)
			{
				std::rethrow_exception(exception_);
			}
		}
	};

  struct generator_sentinel {};

	template<typename T>
	class generator_iterator
	{
		using coroutine_handle = std::coroutine_handle<generator_promise<T>>;

	public:

		using iterator_category = std::input_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = typename generator_promise<T>::value_type;
		using reference = typename generator_promise<T>::reference_type;
		using pointer = typename generator_promise<T>::pointer_type;

		generator_iterator() noexcept
		: coroutine_(nullptr)
		{}
		
		explicit generator_iterator(coroutine_handle coroutine) noexcept
		: coroutine_(coroutine)
		{}

		friend bool operator==(const generator_iterator& it, generator_sentinel) noexcept
		{
			return !it.coroutine_ || it.coroutine_.done();
		}

		friend bool operator!=(const generator_iterator& it, generator_sentinel s) noexcept
		{
			return !(it == s);
		}

		friend bool operator==(generator_sentinel s, const generator_iterator& it) noexcept
		{
			return (it == s);
		}

		friend bool operator!=(generator_sentinel s, const generator_iterator& it) noexcept
		{
			return it != s;
		}

		generator_iterator& operator++()
		{
			coroutine_.resume();
			if (coroutine_.done())
			{
				coroutine_.promise().rethrow_if_exception();
			}

			return *this;
		}

		// Need to provide post-increment operator to implement the 'Range' concept.
		void operator++(int)
		{
			(void)operator++();
		}

		reference operator*() const noexcept
		{
			return coroutine_.promise().value();
		}

		pointer operator->() const noexcept
		{
			return std::addressof(operator*());
		}

	private:

		coroutine_handle coroutine_;
	};
}

template<typename T>
class [[nodiscard]] generator
{
public:

	using promise_type = detail::generator_promise<T>;
	using iterator = detail::generator_iterator<T>;

	generator() noexcept
		: coroutine_{ nullptr }
	{}

	generator(generator&& other) noexcept
		: coroutine_{ other.coroutine_ }
	{
		other.coroutine_ = nullptr;
	}

	generator(const generator& other) = delete;

	~generator()
	{
		if (coroutine_)
		{
			coroutine_.destroy();
		}
	}

	generator& operator=(generator other) noexcept
	{
		swap(other);
		return *this;
	}

	iterator begin()
	{
		if (coroutine_)
		{
			coroutine_.resume();
			if (coroutine_.done())
			{
				coroutine_.promise().rethrow_if_exception();
			}
		}

		return iterator{ coroutine_ };
	}

	detail::generator_sentinel end() noexcept
	{
		return detail::generator_sentinel{};
	}

	void swap(generator& other) noexcept
	{
		std::swap(coroutine_, other.coroutine_);
	}

private:

	friend class detail::generator_promise<T>;

	explicit generator(std::coroutine_handle<promise_type> coroutine) noexcept
		: coroutine_(coroutine)
	{}

	std::coroutine_handle<promise_type> coroutine_;

};

template<typename T>
void swap(generator<T>& a, generator<T>& b)
{
	a.swap(b);
}

namespace detail
{
	template<typename T>
	generator<T> generator_promise<T>::get_return_object() noexcept
	{
		using coroutine_handle = std::coroutine_handle<generator_promise<T>>;
		return generator<T>{ coroutine_handle::from_promise(*this) };
	}
}

template<typename FUNC, typename T>
generator<std::invoke_result_t<FUNC&, typename generator<T>::iterator::reference>> fmap(FUNC func, generator<T> source)
{
	for (auto&& value : source)
	{
		co_yield std::invoke(func, static_cast<decltype(value)>(value));
	}
}
