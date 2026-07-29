#include "StringHash.h"

#ifdef GE_DEBUG
namespace Engine
{
    std::unordered_map<StringHash, const char*> HashCollisionRegistry::s_Registry;
}
#endif
