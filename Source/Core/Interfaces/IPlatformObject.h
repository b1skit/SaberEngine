// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	class Context;
}
namespace core
{	
	struct IPlatObj
	{
		IPlatObj() = default;
		IPlatObj(IPlatObj&&) noexcept = default;
		IPlatObj& operator=(IPlatObj&&) noexcept = default;
		virtual ~IPlatObj() = default;

		template<typename T>
		T As();

		template<typename T>
		T const As() const;

		virtual void Destroy() {}

		re::Context* GetContext() const;


	private:
		friend class re::Context;
		static re::Context* s_context;


	private: // No copying allowed:
		IPlatObj(IPlatObj const&) = delete;
		IPlatObj const& operator=(IPlatObj const&) = delete;
	};


	template<typename T>
	inline T IPlatObj::As()
	{
		return dynamic_cast<T>(this);
	}


	template<typename T>
	inline T const IPlatObj::As() const
	{
		return dynamic_cast<T const>(this);
	}


	inline re::Context* IPlatObj::GetContext() const
	{
		return s_context;
	}
}