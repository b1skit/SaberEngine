#include "ParameterBlock.h"
#include "ParameterBlock_Platform.h"
#include "RenderManager.h"

using std::string;
using std::shared_ptr;
using std::make_shared;


namespace re
{
	ParameterBlock::Handle ParameterBlock::Register(std::shared_ptr<re::ParameterBlock> newPB)
	{
		return RenderManager::Get()->GetParameterBlockManager().RegisterParameterBlock(newPB);
	}


	void ParameterBlock::Destroy()
	{
		platform::ParameterBlock::Destroy(*this);
	}
}