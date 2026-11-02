#pragma once

#include <coroutine>
#include <type_traits>
#include <utility>
#include <exception>
#include <iterator>
#include <functional>

template <typename  T>
class TGenerator;

template <typename T>
class TGeneratorPromise {
public:
	using FValueType = std::remove_reference_t<T>;
	using FReferenceType = std::conditional_t<std::is_reference_v<T>, T, T&>; // That's cool I really like that.
	using FPointerType = FValueType*;
	
	TGeneratorPromise() = default;
	
	TGenerator<T> get_return_object() noexcept;
	
	constexpr std::suspend_always initial_suspend() const noexcept { return {}; }
	constexpr std::suspend_always final_suspend() const noexcept { return {}; }
	
	template <
		typename U = T,
		std::enable_if_t<!std::is_rvalue_reference_v<U>, int> = 0
	>
	std::suspend_always yield_value(std::remove_reference_t<T>& Value) noexcept {
		M_Value = std::addressof(Value);
		return {};
	}
	
	std::suspend_always yield_value(std::remove_reference_t<T>&& Value) noexcept {
		M_Value = std::addressof(Value);
		return {};
	}
	
	void unhandled_exception() {
		checkNoEntry(); // Exceptions are disabled
	}
	
	void return_void() {
		
	}
	
	FReferenceType Value() const noexcept {
		return static_cast<FReferenceType>(*M_Value);
	}
	
	// Don't allow co_await inside the generator coroutine
	template <typename U>
	std::suspend_never await_transform(U&& Value) = delete;
	
private:
	FPointerType M_Value;
};

struct FGeneratorSentinel {};

template <typename T>
class TGeneratorIterator {
	using FCoroutineHandle = std::coroutine_handle<TGeneratorPromise<T>>;
public:
	using FIteratorCategory = std::input_iterator_tag;
	
	using FDifferenceType = std::ptrdiff_t;
	using FValueType = typename TGeneratorPromise<T>::FValueType;
	using FReferenceType = typename TGeneratorPromise<T>::FReferenceType;
	using FPointerType = typename TGeneratorPromise<T>::FPointerType;
	
	TGeneratorIterator() noexcept 
	: M_Coroutine(nullptr) {}
	
	explicit TGeneratorIterator(FCoroutineHandle Coroutine) noexcept 
	: M_Coroutine(Coroutine) {}
	
	friend bool operator==(const TGeneratorIterator& It, FGeneratorSentinel) noexcept {
		return !It.M_Coroutine || It.M_Coroutine.done();
	}
	
	friend bool operator!=(const TGeneratorIterator& It, FGeneratorSentinel Sentinel) noexcept {
		return !(It == Sentinel);
	}
	
	friend bool operator==(FGeneratorSentinel Sentinel, const TGeneratorIterator& It) noexcept {
		return (It == Sentinel);
	}
	
	friend bool operator!=(FGeneratorSentinel Sentinel, const TGeneratorIterator& It) {
		return (It != Sentinel);
	}
	
	TGeneratorIterator& operator++() {
		M_Coroutine.resume();
		return *this;
	}
	
	// Post increment op for Range concept
	void operator++(int) {
		(void)operator++(); // lol nice
	}
	
	FReferenceType operator*() const noexcept {
		return M_Coroutine.promise().Value();
	}
	
	FPointerType operator->() const noexcept {
		return std::addressof(operator*());
	}
	
private:
	FCoroutineHandle M_Coroutine;
};

template <typename T>
class [[nodiscard]] TGenerator {
	using FCoroutineHandle = std::coroutine_handle<TGeneratorPromise<T>>;
public:
	using FPromiseType = TGeneratorPromise<T>;
	using promise_type = FPromiseType;
	using FIterator = TGeneratorIterator<T>; // hmm maybe rename? idk.
	
	TGenerator() noexcept
	: M_Coroutine(nullptr) {}
	
	TGenerator(TGenerator&& Other) noexcept
	: M_Coroutine(Other.M_Coroutine) {
		Other.M_Coroutine = nullptr;
	}
	
	TGenerator(const TGenerator& Other) = delete;
	
	~TGenerator() {
		if (M_Coroutine) {
			M_Coroutine.destroy();
		}
	}
	
	TGenerator& operator=(TGenerator Other) noexcept {
		Swap(Other); // may need optimisation
		return *this;
	}
	
	FIterator begin() {
		if (M_Coroutine) {
			M_Coroutine.resume();
		}
		return FIterator{ M_Coroutine };
	}
	
	FGeneratorSentinel end() noexcept {
		return FGeneratorSentinel{};
	}
	
	void Swap(TGenerator& Other) noexcept {
		std::swap(M_Coroutine, Other.M_Coroutine);
	}
	
private:
	
	friend class TGeneratorPromise<T>;
	
	explicit TGenerator(FCoroutineHandle Coroutine) noexcept
	: M_Coroutine(Coroutine) {}
	
	FCoroutineHandle M_Coroutine;
};

template <typename T>
void Swap(TGenerator<T>& A, TGenerator<T>& B) {
	A.Swap(B);
}

// Detail (add a namespace for it)
template <typename T>
TGenerator<T> TGeneratorPromise<T>::get_return_object() noexcept {
	using FCoroutineHandle = std::coroutine_handle<TGeneratorPromise<T>>;
	return TGenerator<T>{ FCoroutineHandle::from_promise(*this) };
}

// End detail

template <typename Function, typename  T>
TGenerator<std::invoke_result<Function&, typename TGenerator<T>::FIterator::FReferenceType>> FuncMap(
	Function Func, 
	TGenerator<T> Source) {
	for (auto&& Value : Source) {
		co_yield std::invoke(Func, static_cast<decltype(Value)>(Value)); // Forward reference shenanigans 
	}
}
