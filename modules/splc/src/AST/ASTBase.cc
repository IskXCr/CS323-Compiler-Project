#include "AST/ASTBase.hh"

namespace splc {

Ptr<AST> AST::findFirstChild(ASTSymbolType type) const noexcept
{
    // TODO: find the first available child
    return createPtr<AST>();
}

} // namespace splc