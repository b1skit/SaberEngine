// © 2022 Adam Badke. All rights reserved.
#pragma once

namespace en
{
	class Command
	{
	public:
		virtual void Execute() = 0;


	public:
		Command() = default;
		Command(Command const&) = default;
		Command(Command&&) = default;
		Command& operator=(Command const&) = default;
		Command& operator=(Command&&) = default;
		
		virtual ~Command() = default;
	};
}