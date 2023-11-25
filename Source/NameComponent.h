#pragma once


namespace fr
{
	class NameComponent
	{
	public:
		NameComponent(char const* name) { strcpy(m_name, name); }
		NameComponent(std::string const& name) { strcpy(m_name, name.c_str()); }

		char const* GetName() { return m_name; }


	private:
		static constexpr uint32_t k_maxNameLength = 128;
		char m_name[1024];
	};
}