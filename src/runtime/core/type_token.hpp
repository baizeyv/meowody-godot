//
// Created by baizeyv on 8/31/2026.
//

#ifndef GDMEOWODYFRAMEWORK_TYPE_TOKEN_HPP
#define GDMEOWODYFRAMEWORK_TYPE_TOKEN_HPP

#include <cstddef>
#include <type_traits>

namespace meowody
{
	namespace detail
	{
		template <typename T>
		inline constexpr int type_token_anchor = 0;
	}

	class TypeToken
	{
	  public:
		constexpr TypeToken() noexcept = default;

		constexpr explicit TypeToken(const void *value) noexcept : value_(value)
		{
		}

		template <typename T>
		static TypeToken of() noexcept
		{
			using Type = std::remove_cvref_t<T>;

			return TypeToken(&detail::type_token_anchor<Type>);
		}

		constexpr const void *value() const noexcept { return value_; }

		friend constexpr bool operator==(TypeToken lhs, TypeToken rhs) noexcept
		{
			return lhs.value_ == rhs.value_;
		}

	  private:
		const void *value_ = nullptr;
	};

	struct TypeTokenHash
	{
		std::size_t operator()(TypeToken token) const noexcept
		{
			return reinterpret_cast<std::size_t>(token.value());
		}
	};

	template <typename T>
	TypeToken type_token_of() noexcept
	{
		return TypeToken::of<T>();
	}
}  // namespace meowody

#endif	// GDMEOWODYFRAMEWORK_TYPE_TOKEN_HPP
