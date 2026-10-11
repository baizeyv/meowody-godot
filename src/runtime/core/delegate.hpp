//
// Created by baizeyv on 8/31/2026.
//

#ifndef GDMEOWODYFRAMEWORK_DELEGATE_H
#define GDMEOWODYFRAMEWORK_DELEGATE_H
#include <cassert>
#include <cstddef>
#include <functional>
#include <new>
#include <type_traits>
#include <utility>

namespace meowody
{
	template <typename Signature>
	class Delegate;

	/**
	 * Move-only type-erased callable.
	 *
	 * Features:
	 *   - Lambda
	 *   - Capturing Lambda
	 *   - Function pointer
	 *   - Member function
	 *   - SBO
	 *   - Custom VTable
	 *
	 * Example:
	 *
	 * Delegate<void(int)> fn = [](int value) {
	 *     ...
	 * };
	 *
	 * fn(10);
	 */
	template <typename R, typename... Args>
	class Delegate<R(Args...)>
	{
	  private:
		static constexpr size_t STORAGE_SIZE = 64;

		static constexpr size_t STORAGE_ALIGN = alignof(std::max_align_t);

		using Storage = std::aligned_storage_t<STORAGE_SIZE, STORAGE_ALIGN>;

		// * --------------------------------------------------------------------

		/**
		 * * 虚函数表
		 */
		struct VTable
		{
			R (*invoke)(void *object, Args &&...args);

			void (*destroy)(void *object);

			void (*move)(void *from, void *to);

			bool heap_allocated;
		};

		// * --------------------------------------------------------------------

	  public:
		Delegate() noexcept = default;

		Delegate(std::nullptr_t) noexcept {}

		~Delegate() { reset(); }

		Delegate(Delegate &&other) noexcept { move_from(std::move(other)); }

		Delegate &operator=(Delegate &&other)
		{
			if (this != &other)
			{
				reset();
				move_from(std::move(other));
			}
			return *this;
		}

		Delegate(const Delegate &) = delete;

		Delegate &operator=(const Delegate &) = delete;

		template <typename F, typename = std::enable_if_t<
								  !std::is_same_v<std::decay_t<F>, Delegate>>>
		Delegate(F &&callable)
		{
			assign(std::forward<F>(callable));
		}

		template <typename F>
		void assign(F &&callable)
		{
			reset();
			using FType = std::decay_t<F>;

			static_assert(
				std::is_invocable_r_v<R, FType &, Args...>,
				"Callable is not compatible with Delegate signature.");

			if constexpr (fits_inline<FType>())
			{
				new (&storage_) FType(std::forward<F>(callable));

				object_ = &storage_;

				vtable_ = &get_vtable<FType>();
			}
			else
			{
				FType *ptr = new FType(std::forward<F>(callable));
				object_	   = ptr;
				vtable_	   = &get_vtable<FType>();
			}
		}

		void reset() noexcept
		{
			if (!vtable_) return;

			vtable_->destroy(object_);

			object_ = nullptr;
			vtable_ = nullptr;
		}

		explicit operator bool() const noexcept { return vtable_ != nullptr; }

		bool valid() const noexcept { return vtable_ != nullptr; }

		R operator()(Args... args)
		{
			assert(vtable_ != nullptr);

			return vtable_->invoke(object_, std::forward<Args>(args)...);
		}

		R operator()(Args... args) const
		{
			assert(vtable_ != nullptr);

			return vtable_->invoke(object_, std::forward<Args>(args)...);
		}

	  private:
		template <typename F>
		static constexpr bool fits_inline()
		{
			return sizeof(F) <= STORAGE_SIZE && alignof(F) <= STORAGE_ALIGN &&
				   std::is_nothrow_move_constructible_v<F>;
		}

		template <typename F>
		static const VTable &get_vtable()
		{
			static const VTable table{

				// invoke
				[](void *object, Args &&...args) -> R {
					F *fn = static_cast<F *>(object);

					if constexpr (std::is_void_v<R>)
					{
						std::invoke(*fn, std::forward<Args>(args)...);

						return;
					}
					else
					{
						return std::invoke(*fn, std::forward<Args>(args)...);
					}
				},

				// destroy
				[](void *object) {
					F *fn = static_cast<F *>(object);

					if constexpr (fits_inline<F>())
					{
						fn->~F();
					}
					else
					{
						delete fn;
					}
				},

				// move
				[](void *from, void *to) {
					F *src = static_cast<F *>(from);

					new (to) F(std::move(*src));

					src->~F();
				},

				// heap allocated
				!fits_inline<F>()};

			return table;
		}

		void move_from(Delegate &&other) noexcept
		{
			if (!other.vtable_)
			{
				return;
			}

			vtable_ = other.vtable_;

			if (vtable_->heap_allocated)
			{
				object_ = other.object_;

				other.object_ = nullptr;
				other.vtable_ = nullptr;
			}
			else
			{
				vtable_->move(other.object_, &storage_);

				object_ = &storage_;

				other.object_ = nullptr;
				other.vtable_ = nullptr;
			}
		}

	  public:
		/**
		 * Bind normal member function.
		 */
		template <typename C, typename Method>
		static Delegate bind(C *instance, Method method)
		{
			return Delegate([instance, method](Args... args) -> R {
				if constexpr (std::is_void_v<R>)
				{
					std::invoke(method, instance, std::forward<Args>(args)...);

					return;
				}
				else
				{
					return std::invoke(method, instance,
									   std::forward<Args>(args)...);
				}
			});
		}

	  private:
		Storage storage_{};

		void *object_ = nullptr;

		const VTable *vtable_ = nullptr;
	};

	/**
	 * Helper.
	 */
	template <typename Signature, typename F>
	auto make_delegate(F &&fn) -> Delegate<Signature>
	{
		return Delegate<Signature>(std::forward<F>(fn));
	}
};	// namespace meowody

#endif	// GDMEOWODYFRAMEWORK_DELEGATE_H
