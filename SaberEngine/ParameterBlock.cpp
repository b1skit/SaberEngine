#include "ParameterBlock.h"
#include "ParameterBlock_Platform.h"

using std::string;
using std::shared_ptr;
using std::make_shared;


namespace re
{
	void PermanentParameterBlock::Destroy()
	{
		platform::PermanentParameterBlock::Destroy(*this);
	}
}