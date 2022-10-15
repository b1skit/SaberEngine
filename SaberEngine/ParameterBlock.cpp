#include "ParameterBlock.h"
#include "ParameterBlock_Platform.h"
#include "RenderManager.h"
#include "CoreEngine.h"

using std::string;
using std::shared_ptr;
using std::make_shared;


namespace re
{
	void ParameterBlock::Register(std::shared_ptr<re::ParameterBlock> newPB)
	{
		en::CoreEngine::GetRenderManager()->GetParameterBlockManager().RegisterParameterBlock(newPB);
	}


	void ParameterBlock::Destroy()
	{
		platform::ParameterBlock::Destroy(*this);
	}
}